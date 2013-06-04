/**
 * \file Display "text" on the vector scope
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


#define MAX_ROWS 10
#define MAX_COLS 15

static char text[MAX_ROWS][MAX_COLS] = {
	{ "" },
	{ "Future crew" },
	{ "Version 1.0" },
	{ "Incept date" },
	{ "1945-05-27" },
/*
	{ "012345678901234" },
	{ "0123456789-=" },
	{ "~!@#$%^&*\\|_" },
	{ "+`[]{}()<>/?" },
*/
};

static vector_rot_t rot = {
	.scale = 64,
	.cx = 128,
	.cy = 128,
};


static void
draw_text(void)
{
	const uint8_t height = 24;

	uint8_t y = 256 - height;
	for (uint8_t row = 0 ; row < MAX_ROWS ; row++)
	{
		uint8_t x = 0;
		for (uint8_t col = 0 ; col < MAX_COLS ; col++)
		{
			draw_char_small(x, y, text[row][col]);
			x += 16;
		}

		y -= height;
	}
}


int main(void)
{
	// set for 16 MHz clock
#define CPU_PRESCALE(n) (CLKPR = 0x80, CLKPR = (n))
	CPU_PRESCALE(0);

	// Disable the ADC
	ADMUX = 0;

	// initialize the USB
	usb_init();
	DDRB = 0xFF;
	DDRD = 0xFF;
	PORTB = 128;
	PORTD = 0;

	clock_init();

	uint8_t col = 0;
	uint16_t theta = 0;
	uint8_t size = 0;


	// wait for the host to setup the USB port and for the user
	// to run their terminal emulator program which sets DTR to
	// indicate it is ready to receive.  Otherwise display the
	// attract screen
	while (1)
	{
		if (usb_configured()
		&& usb_serial_get_control() & USB_SERIAL_DTR)
			break;

		vector_rot_init(&rot, (theta++) / 4);

		if (size >= 128)
			rot.scale = (32+64) - (size - 128)/2;
		else
			rot.scale = 32 + size/2;

		size = (size + 3);

		draw_char_rot(&rot, -50, -10, 'A');
		draw_char_rot(&rot, -30, -10, 'L');
		draw_char_rot(&rot, -10, -10, 'E');
		draw_char_rot(&rot, +20, -10, 'R');
		draw_char_rot(&rot, +40, -10, 'T');
		draw_char_rot(&rot, +60, -10, '!');

		draw_char_med(30, 60, '#');
		draw_char_med(50, 60, 'F');
		draw_char_med(70, 60, 'u');
		draw_char_med(90, 60, 't');
		draw_char_med(110, 60, 'u');
		draw_char_med(132, 60, 'r');
		draw_char_med(152, 60, 'e');

		draw_char_med(100, 30, 'C');
		draw_char_med(120, 30, 'r');
		draw_char_med(140, 30, 'e');
		draw_char_med(160, 30, 'w');
		draw_char_med(180, 30, '*');

		line(0, 0, 254, 0);
		line(254, 0, 254, 254);
		line(254, 254, 0, 254);
		line(0, 254, 0, 0);
	}


	// discard anything that was received prior.  Sometimes the
	// operating system or other software will send a modem
	// "AT command", which can still be buffered.
	usb_serial_flush_input();

	// No rotation for the text
	vector_rot_init(&rot, 0);
	rot.scale = 48;

	while (1)
	{
		if (rot.scale < 48)
			rot.scale = (size++) / 2;

		draw_text();
		int c = usb_serial_getchar();
		if (c == -1)
			continue;

		if (c == '\f')
		{
			col = 0;
			rot.scale = 0;
			size = 0;
			memset(text, '\0', sizeof(text));
			continue;
		}

		if (col >= MAX_COLS || c == '\n')
		{
			memmove(&text[0], &text[1], (MAX_ROWS-1)*sizeof(text[0]));
			memset(text[MAX_ROWS-1], '\0', sizeof(text[0]));
			col = 0;
		}

		if (c < ' ')
			continue;

		text[MAX_ROWS-1][col++] = c;
	}
}
