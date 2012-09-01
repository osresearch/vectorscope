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
	path_t * p = digits[val];
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

	uint16_t t = 0xDEAD;

	uint8_t px = 0;
	uint8_t py = 0;
	uint16_t count = 0;
	

	while (1)
	{
		uint16_t x = t / 64;
		uint8_t s = x % 60; x /= 60;
		uint8_t m = x % 60; x /= 60;
		uint8_t h = x % 24;

		if (count++ == 1000)
		{
			count = 0;
			if (px < 64)
				px++;
			else
				px = 0;
	
			if (py < 160)
				py += 3;
			else
				py = 0;
		}

		draw_digit( 0+px, 64+py, h / 10);
		draw_digit(32+px, 64+py, h % 10);

		draw_digit(80+px, 64+py, m / 10);
		draw_digit(80+32+px, 64+py, m % 10);

		draw_digit(160+px, 64+py, s / 10);
		draw_digit(160+32+px, 64+py, s % 10);

		draw_digit(0*32+px, py, (t >> 15) & 7);
		draw_digit(1*32+px, py, (t >> 12) & 7);
		draw_digit(2*32+px, py, (t >>  9) & 7);
		draw_digit(3*32+px, py, (t >>  6) & 7);
		draw_digit(4*32+px, py, (t >>  3) & 7);
		draw_digit(5*32+px, py, (t >>  0) & 7);

		//PORTB = PORTD = 0;

		t++;

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
