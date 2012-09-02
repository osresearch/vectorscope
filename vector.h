/**
 * \file
 * Vector line drawing and fonts
 *
 */

#ifndef _vector_h_
#define _vector_h_

#include <stdint.h>

void
line_vert(
	uint8_t x0,
	uint8_t y0,
	uint8_t w
);

void
line_horiz(
	uint8_t x0,
	uint8_t y0,
	uint8_t h
);


void
line(
	uint8_t x0,
	uint8_t y0,
	uint8_t x1,
	uint8_t y1
);


void
draw_digit_big(
	uint8_t x,
	uint8_t y,
	uint8_t val
);


void
draw_digit(
	uint8_t x,
	uint8_t y,
	uint8_t val
);


#endif
