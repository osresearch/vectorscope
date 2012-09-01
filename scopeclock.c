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
#include "hershey.h"


/** Track the number of miliseconds, sec, min and hour since midnight */
static volatile uint16_t now_ms;
static volatile uint8_t now_sec;
static volatile uint8_t now_min;
static volatile uint8_t now_hour;


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


static void
line_vert(
	uint8_t x0,
	uint8_t y0,
	uint8_t w
)
{
	PORTB = x0;
	PORTD = y0;

	for (uint8_t i = 0 ; i < w ; i++)
	{
		PORTD++;
	}
}

static void
line_horiz(
	uint8_t x0,
	uint8_t y0,
	uint8_t h
)
{
	PORTB = x0;
	PORTD = y0;

	for (uint8_t i = 0 ; i < h ; i++)
	{
		PORTB++;
	}
}


static void
line(
	uint8_t x0,
	uint8_t y0,
	uint8_t x1,
	uint8_t y1
)
{
	int dx;
	int dy;
	int sx;
	int sy;

	if (x0 < x1)
	{
		dx = x1 - x0;
		sx = 1;
	} else {
		dx = x0 - x1;
		sx = -1;
	}

	if (y0 < y1)
	{
		dy = y1 - y0;
		sy = 1;
	} else {
		dy = y0 - y1;
		sy = -1;
	}

	int err = dx - dy;

	while (1)
	{
		PORTB = x0;
		PORTD = y0;

		if (x0 == x1 && y0 == y1)
			break;

		int e2 = 2 * err;

		if (e2 > -dy)
		{
			err = err - dy;
			x0 += sx;
		}
		if (e2 < dx)
		{
			err = err + dx;
			y0 += sy;
		}
	}
}


static void
draw_digit(
	uint8_t x,
	uint8_t y,
	uint8_t val
)
{
	const path_t * p = digits[val];
	const int8_t scale = 2;

	while (1)
	{
		uint8_t ox = x + p->x * scale;
		uint8_t oy = y + p->y * scale;

		while (1)
		{
			p++;
			int8_t px = p->x;
			int8_t py = p->y;

			if (px == 0 && py == 0)
				return;

			if (px == -1 && py == -1)
			{
				p++;
				break;
			}

			uint8_t nx = x + px * scale;
			uint8_t ny = y + py * scale;

			line(ox, oy, nx, ny);
			ox = nx;
			oy = ny;
		}
	}
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

		uint8_t h = now_hour;
		draw_digit( 0+px, 64+py, h / 10);
		draw_digit(32+px, 64+py, h % 10);

		uint8_t m = now_min;
		draw_digit(80+px, 64+py, m / 10);
		draw_digit(80+32+px, 64+py, m % 10);

		uint8_t s = now_sec;
		draw_digit(160+px, 64+py, s / 10);
		draw_digit(160+32+px, 64+py, s % 10);

/*
		draw_digit(0*32+px, py, (now_ms / 100) % 10);
		draw_digit(1*32+px, py, (now_ms / 10) % 10);
		draw_digit(2*32+px, py, (now_ms / 1) % 10);

		draw_digit(4*32+px, py, (TCNT0 / 100) % 10);
		draw_digit(5*32+px, py, (TCNT0 / 10) % 10);
		draw_digit(6*32+px, py, (TCNT0 / 1) % 10);
*/

		//line(128, 128, (x & 255), x >> 8);

		//line_horiz(0,0,255);
		//line_vert(0,0,255);
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
