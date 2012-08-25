/**
 * \file SCSI emulator
 *
 * Emulate a SCSI1 bus.
 * http://en.wikipedia.org/wiki/Parallel_SCSI
 * http://www.connectworld.net/scsi.html
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

#define SCSI_BSY	0xB7
#define SCSI_SEL	0xB4
#define SCSI_RST	???
#define SCSI_CD		0xC6
#define SCSI_IO		0xB2
#define SCSI_MSG	0xB1
#define SCSI_REQ	0xB0
#define SCSI_ACK	0xB3
#define SCSI_ATN	0xE6
#define SCSI_DATA_IN	PIND
#define SCSI_DATA_OUT	PORTD


static uint8_t
scsi_wait_for_selection(void)
{
	while (1)
	{
		if (in(SCSI_BSY) != 0)
			continue;
		if (in(SCSI_SEL) != 0)
			continue;
		break;
	}

	// Master has asserted BSY and SEL; read their address.
	const uint8_t master_id = ~SCSI_DATA_IN;

	// Wait for a target id to be selected
	while (1)
	{
		// Abort our loop if SCSI_BSY goes low
		if (in(SCSI_BSY) != 0)
			return 0xFF;

		const uint8_t id = ~SCSI_DATA_IN;
		if (id == master_id)
			continue;

		return id ^ master_id;
	}
}



int main(void)
{
	// set for 16 MHz clock
#define CPU_PRESCALE(n) (CLKPR = 0x80, CLKPR = (n))
	CPU_PRESCALE(0);

	// Disable the ADC
	ADMUX = 0;

	// Configure everything as input
	DDRB = 0;
	DDRC = 0;
	DDRD = 0;
	DDRE = 0;
	DDRF = 0;

	// Pullups
	PORTB = 0xFF;
	PORTC = 0xFF;
	PORTD = 0xFF;
	PORTE = 0xFF;
	PORTF = 0xFF;

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


	const uint8_t scsi_id = (1 << 4);
	uint8_t buf[256];
	uint8_t off = 0;

	while (1)
	{
		const uint8_t sel_id = scsi_wait_for_selection();
		buf[off++] = hexdigit(sel_id >> 4);
		buf[off++] = hexdigit(sel_id >> 0);
		buf[off++] = ' ';

/*
		uint8_t off = 0;
		for (int i = 0 ; i < 64 ; i++)
		{
			uint8_t x = ~SCSI_DATA_IN;
			if (x == 0x81)
			{
				break;
			}
				ddr(SCSI_BSY, 1);

			buf[off++] = hexdigit(x >> 4);
			buf[off++] = hexdigit(x >> 0);
			buf[off++] = ' ';
			if (in(SCSI_BSY) != 0)
				break;
			_delay_us(1);
		}

		ddr(SCSI_BSY, 0);
		uint8_t x;

		x = SCSI_DATA_IN;
		buf[off++] = hexdigit(x >> 4);
		buf[off++] = hexdigit(x >> 0);
		buf[off++] = ' ';

		buf[off++] = 'B';
		buf[off++] = in(SCSI_BSY) ? '1' : '0';
		buf[off++] = ' ';

		buf[off++] = 'S';
		buf[off++] = in(SCSI_SEL) ? '1' : '0';
		buf[off++] = ' ';

		buf[off++] = 'C';
		buf[off++] = in(SCSI_CD) ? '1' : '0';
		buf[off++] = ' ';

		buf[off++] = 'M';
		buf[off++] = in(SCSI_MSG) ? '1' : '0';
		buf[off++] = ' ';

		buf[off++] = 'R';
		buf[off++] = in(SCSI_REQ) ? '1' : '0';
		buf[off++] = ' ';

		buf[off++] = 'A';
		buf[off++] = in(SCSI_ACK) ? '1' : '0';
		buf[off++] = ' ';

		buf[off++] = 'a';
		buf[off++] = in(SCSI_ATN) ? '1' : '0';
		buf[off++] = ' ';
*/

		buf[off++] = '\r';
		buf[off++] = '\n';

		usb_serial_write(buf, off);
		off = 0;
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
