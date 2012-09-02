/** \file
 * Realtime clock tracking.
 *
 * Uses Timer0 overflow to generate ms level clock ticks.
 */

#ifndef _clock_h_
#define _clock_h_

#include <avr/io.h>
#include <stdint.h>

/** Track the number of miliseconds, sec, min and hour since midnight */
volatile uint8_t now_hour;
volatile uint8_t now_min;
volatile uint8_t now_sec;
volatile uint16_t now_ms;
volatile uint16_t now; // ms since boot


void
clock_init(void);


#endif
