#include <avr/io.h>
#include <avr/interrupt.h>
#include "bits.h"
#include "clock.h"

volatile uint8_t now_hour = 15;
volatile uint8_t now_min = 0;
volatile uint8_t now_sec = 0;
volatile uint16_t now_ms;
volatile uint16_t now; // ms since boot

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


void
clock_init(void)
{
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


	// Clk/256 @ 16 MHz => 250 ticks == 1 ms
	OCR0A = 250;

#ifdef CONFIG_HZ_IRQ
	sbi(TIMSK0, OCIE0A);
	sei();
#endif
}
