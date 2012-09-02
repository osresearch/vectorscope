/**
 * \file Dual DAC outputs for driving a vector scope.
 *
 */

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include <string.h>
#include <util/delay.h>
#include "usb_serial.h"
#include "bits.h"
#include "sin_table.h"
#include "vector.h"


/** Track the number of miliseconds, sec, min and hour since midnight */
static volatile uint8_t now_hour = 21;
static volatile uint8_t now_min = 15;
static volatile uint8_t now_sec = 30;
static volatile uint16_t now_ms;
static volatile uint16_t fps;
static volatile uint16_t last_fps;


// Define CONFIG_HZ_IRQ to enable a timer interrupt rather than
// polling the interrupt flag.
#define CONFIG_HZ_IRQ

/** 1000-hz overflow */
#ifdef CONFIG_HZ_IRQ
ISR(TIMER0_COMPA_vect)
#else
static void
now_update(void)
#endif
{
	if (now_ms < 999)
	{
		now_ms += 1;
		return;
	}

	now_ms = 0;
	last_fps = fps;
	fps = 0;

	if (now_sec < 59)
	{
		now_sec++;
		return;
	}

	now_sec = 0;

	if (now_min < 59)
	{
		now_min++;
		return;
	}

	now_min = 0;

	if (now_hour < 23)
	{
		now_hour++;
		return;
	}

	now_hour = 0;
}



void send_str(const char *s);
uint8_t recv_str(char *buf, uint8_t size);
void parse_and_execute_command(const char *buf, uint8_t num);

static uint8_t
hexdigit(
	uint8_t x
)
{
	x &= 0xF;
	if (x < 0xA)
		return x + '0' - 0x0;
	else
		return x + 'A' - 0xA;
}


static uint8_t
printable(
	uint8_t x
)
{
	if ('A' <= x && x <= 'Z')
		return 1;
	if ('a' <= x && x <= 'z')
		return 1;
	if ('0' <= x && x <= '9')
		return 1;
	if (x == ' ')
		return 1;

	return 0;
}


int main(void)
{
	// set for 16 MHz clock
#define CPU_PRESCALE(n) (CLKPR = 0x80, CLKPR = (n))
	CPU_PRESCALE(0);

	// Disable the ADC
	ADMUX = 0;

	// initialize the USB, and then wait for the host
	// to set configuration.  If the Teensy is powered
	// without a PC connected to the USB port, this 
	// will wait forever.
	usb_init();
	while (!usb_configured()) /* wait */ ;
	_delay_ms(1000);

	// wait for the user to run their terminal emulator program
	// which sets DTR to indicate it is ready to receive.
	while (!(usb_serial_get_control() & USB_SERIAL_DTR))
		continue;

	// discard anything that was received prior.  Sometimes the
	// operating system or other software will send a modem
	// "AT command", which can still be buffered.
	usb_serial_flush_input();

	DDRB = 0xFF;
	DDRD = 0xFF;
	PORTB = 128;
	PORTD = 0;

	uint8_t off = 0;
	char buf[32];

	uint8_t px = 0;
	uint8_t py = 0;
	uint8_t count = 0;

	// Configure timer0 to overflow every 1 ms
	// CTC mode (clear counter at OCR0A, signal interrupt)
	TCCR0A = 0
		| (1 << WGM01)
		| (0 << WGM00)
		;

	TCCR0B = 0
		| (0 << WGM02)
		| (0 << CS02)
		| (1 << CS01)
		| (1 << CS00)
		;


	// Clk/256 @ 16 MHz => 250 ticks == 10 ms
	OCR0A = 250;

#ifdef CONFIG_HZ_IRQ
	sbi(TIMSK0, OCIE0A);
	sei();
#endif

	

	while (1)
	{
		cli();
		fps++;
		uint16_t old_fps = last_fps;
		sei();
		draw_digit(0, 0, (old_fps / 100) % 10);
		draw_digit(8, 0, (old_fps / 10) % 10);
		draw_digit(16, 0, (old_fps / 1) % 10);

#ifndef CONFIG_HZ_IRQ
		// If the interrupt is not in use, check the overflow bit
		if (bit_is_set(TIFR0, OCF0A))
		{
			sbi(TIFR0, OCF0A);
			now_update();
		}
#endif

		if (count++ == 100)
		{
			count = 0;
			if (px < 30)
				px++;
			else
				px = 0;
	
			if (py < 120)
				py += 3;
			else
				py = 0;
		}


		// Draw all the digits around the outside
		for (uint8_t h = 0 ; h < 24 ; h++)
		{
			uint16_t h2 = h;
			h2 = (h2 * 682) / 64;
			uint8_t x = sin_lookup(h2) * 7 / 8 + 128;
			uint8_t y = cos_lookup(h2) * 7 / 8 + 128;
			draw_digit(x-8, y-4, h / 10);
			draw_digit(x+2, y-4, h % 10);
		}

		// Draw the hour hand
		cli();
		uint16_t ms = now_ms;
		uint8_t h = now_hour;
		uint8_t m = now_min;
		uint8_t s = now_sec;
		sei();

		const uint8_t cx = 72;
		const uint8_t cy = 64;

		draw_digit_big( 0+cx, cy, h / 10);
		draw_digit_big(16+cx, cy, h % 10);
		draw_digit_big(40+cx, cy, m / 10);
		draw_digit_big(56+cx, cy, m % 10);
		draw_digit_big(80+cx, cy, s / 10);
		draw_digit_big(96+cx, cy, s % 10);

		{
			uint16_t h2 = h;
			h2 = (h2 * 682 + m*11) / 64;
			uint8_t hx = sin_lookup(h2) * 3 / 8 + 128;
			uint8_t hy = cos_lookup(h2) * 3 / 8 + 128;
			line(128, 128, hx, hy);
		}


		{
			uint16_t m2 = m;
			m2 = (m2 * 273 + s*4) / 64;
			uint8_t mx = sin_lookup(m2) * 5 / 8 + 128;
			uint8_t my = cos_lookup(m2) * 5 / 8 + 128;
			line(128, 128, mx, my);
			line_horiz(mx - 5, my, 10);
			line_vert(mx, my - 5, 10);
		}


		// seconds to "degrees" = 
		{
			uint16_t s2 = s;
			s2 = (s2 * 1092 + ms) / 256;
			uint8_t sx = sin_lookup(s2) * 6 / 8 + 128;
			uint8_t sy = cos_lookup(s2) * 6 / 8 + 128;
			line(128, 128, sx, sy);
		}

/*
		line_horiz(0,0,255);
		line_horiz(0,255,255);
		line_vert(0,0,255);
		line_vert(255,0,255);
*/
	}
}


// Send a string to the USB serial port.  The string must be in
// flash memory, using PSTR
//
void send_str(const char *s)
{
	char c;
	while (1) {
		c = pgm_read_byte(s++);
		if (!c) break;
		usb_serial_putchar(c);
	}
}
