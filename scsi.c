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


/** Wait 250 ns */
static inline void
deskew_delay(void)
{
	asm("nop"); asm("nop"); asm("nop"); asm("nop");
}

/** Wait 400 ns */
static inline void
bus_settle_delay(void)
{
	asm("nop"); asm("nop"); asm("nop"); asm("nop");
	asm("nop"); asm("nop"); asm("nop");
}


#define SCSI_BSY	0xB7
#define SCSI_SEL	0xB4
#define SCSI_RST	???
#define SCSI_CD		0xC6
#define SCSI_IO		0xB2
#define SCSI_MSG	0xB1
#define SCSI_REQ	0xB0
#define SCSI_ACK	0xE6
#define SCSI_AST	0xB3
#define SCSI_ATN	0xC7
#define SCSI_DATA_IN	PIND
#define SCSI_DATA_OUT	PORTD
#define SCSI_DATA_DDR	DDRD


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
	deskew_delay();

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


static inline void
scsi_drive(
	const uint8_t port
)
{
	ddr(port, 1);
	out(port, 0);
}

static inline void
scsi_release(
	const uint8_t port
)
{
	out(port, 1);
	ddr(port, 0);
}

typedef struct
{
	uint8_t cmd;
	uint8_t lun;
	uint8_t res1;
	uint8_t res2;
	uint8_t len;
	uint8_t control;
} __attribute__((__packed__))
scsi_cdb_t;


typedef struct
{
	uint8_t cmd;
	uint8_t lun_lba16;
	uint8_t lba8;
	uint8_t lba0;
	uint8_t len;
	uint8_t control;
} __attribute__((__packed__))
scsi_cdb_read6_t;
#define SCSI_CMD_READ6 0x08



static uint8_t
__attribute__((noinline))
scsi_read(void)
{
	scsi_drive(SCSI_REQ);

	while (in(SCSI_ACK) != 0)
		;

	deskew_delay();
	const uint8_t x = ~SCSI_DATA_IN;

	// Signal that we have read this byte
	scsi_release(SCSI_REQ);

	return x;
}


static void
scsi_write(
	uint8_t x
)
{
	SCSI_DATA_DDR = 0xFF;
	SCSI_DATA_OUT = x;
	deskew_delay();

	scsi_drive(SCSI_REQ);

	// Wait for ACK to be set
	while (in(SCSI_ACK) != 0)
		;

	scsi_release(SCSI_REQ);
	SCSI_DATA_OUT = 0xFF;
	SCSI_DATA_DDR = 0;
}



/** Read a 6-byte command */
static int
scsi_read_cdb(
	scsi_cdb_t * cdb
)
{
	scsi_drive(SCSI_CD);
	bus_settle_delay();

	uint8_t * buf = (void*) cdb;

	for (int i = 0 ; i < sizeof(*cdb) ; i++)
		buf[i] = scsi_read();

	scsi_release(SCSI_CD);

	return 0;
}


/** Respond to a 6-byte read command */
static int
scsi_cmd_read6(
	const scsi_cdb_t * const cdb
)
{
	const scsi_cdb_read6_t * const cmd = (const void *) cdb;

	uint32_t lba = 0
		| (uint32_t)(cmd->lun_lba16 & 0x1F) << 16
		| (uint32_t)cmd->lba8 << 8
		| (uint32_t)cmd->lba0 << 0
		;

	// Respond with all zeros
	scsi_drive(SCSI_IO);

	scsi_write(0x00);

	scsi_release(SCSI_IO);

	return 0;
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


	const uint8_t scsi_id = (1 << 6);
	uint8_t buf[256];
	uint8_t off = 0;

	while (1)
	{
		send_str(PSTR("sel: "));
		const uint8_t sel_id = scsi_wait_for_selection();
		buf[off++] = hexdigit(sel_id >> 4);
		buf[off++] = hexdigit(sel_id >> 0);
		buf[off++] = ' ';

		if (sel_id == scsi_id)
		{
			// That's me!  Wait for the BSY to go low, then
			// assert it ourselves.
			// should check for !SEL as well?
			while (in(SCSI_BSY) == 0)
				;

			deskew_delay();

			// Now assert it ourselves
			scsi_drive(SCSI_BSY);

			// And wait for SEL to go high
			while (in(SCSI_SEL) == 0)
				;

			deskew_delay();

			// Now signal that we are ready to receive
			// the command from the initiator.
			scsi_cdb_t cdb;

			scsi_read_cdb(&cdb);

			buf[off++] = hexdigit(cdb.cmd >> 4);
			buf[off++] = hexdigit(cdb.cmd >> 0);
			buf[off++] = hexdigit(cdb.lun >> 5);
			buf[off++] = ' ';
			buf[off++] = hexdigit(cdb.len >> 4);
			buf[off++] = hexdigit(cdb.len >> 0);
			buf[off++] = ' ';
			buf[off++] = hexdigit(cdb.control >> 4);
			buf[off++] = hexdigit(cdb.control >> 0);

			if (cdb.cmd == SCSI_CMD_READ6)
				scsi_cmd_read6(&cdb);

			// turn off busy for now
			scsi_release(SCSI_REQ);
			scsi_release(SCSI_CD);
		}


		buf[off++] = '\r';
		buf[off++] = '\n';

		usb_serial_write(buf, off);
		off = 0;

		// just in case
		scsi_release(SCSI_BSY);
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
