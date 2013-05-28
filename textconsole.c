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
	{ "0123456789-=" },
	{ "~!@#$%^&*\\|_" },
	{ "+`[]{}()<>/?" },
*/
};


static void
draw_text(void)
{
	const uint8_t height = 24;

	uint8_t y = 256-height;
	for (uint8_t row = 0 ; row < MAX_ROWS ; row++)
	{
		uint8_t x = 0;
		for (uint8_t col = 0 ; col < MAX_COLS ; col++)
		{
			x += draw_char_small(x, y, text[row][col]) + 3;
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

	clock_init();

	uint8_t col = 0;

	while (1)
	{
		draw_text();
		int c = usb_serial_getchar();
		if (c == -1)
			continue;

		if (c == '\f')
		{
			col = 0;
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
