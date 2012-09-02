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
#include "sin_table.h"


/** Track the number of miliseconds, sec, min and hour since midnight */
static volatile uint16_t now_ms;
static volatile uint8_t now_sec = 30;
static volatile uint8_t now_min = 19;
static volatile uint8_t now_hour = 20;


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


static inline void
_draw_digit(
	uint8_t x,
	uint8_t y,
	uint8_t val,
	const uint8_t scale
)
{
	const path_t * p = digits[val];

	while (1)
	{
		uint8_t ox = x + p->x / scale;
		uint8_t oy = y + p->y / scale;

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

			uint8_t nx = x + px / scale;
			uint8_t ny = y + py / scale;

			line(ox, oy, nx, ny);
			ox = nx;
			oy = ny;
		}
	}
}


static void
draw_digit_big(
	uint8_t x,
	uint8_t y,
	uint8_t val
)
{
	_draw_digit(x, y, val, 1);
}


static void
draw_digit(
	uint8_t x,
	uint8_t y,
	uint8_t val
)
{
	_draw_digit(x, y, val, 2);
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
		uint8_t h = now_hour;
		uint8_t m = now_min;
		uint8_t s = now_sec;

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
			s2 = (s2 * 1092 + now_ms) / 256;
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
