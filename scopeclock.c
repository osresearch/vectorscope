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
#include "clock.h"




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
analog_clock(void)
{
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

	clock_init();

	while (1)
	{
		if (0)
			planet_loop();
		else
			analog_clock();
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
