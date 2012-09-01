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

	uint8_t dx = 1;
	uint8_t dy = 1;

	while (1)
	{
		cli();
		line_vert(64,64,128);
		line_vert(128+64,64,128);
		line_horiz(64,64,128);
		line_horiz(64,128+64,128);

		line_vert(64+30,64+30,128);
		line_vert(128+64+30,64+30,128);
		line_horiz(64+30,64+30,128);
		line_horiz(64+30,128+64+30,128);

		PORTD = PORTB = 0;
		sei();
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
