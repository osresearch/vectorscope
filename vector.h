/**
 * \file
 * Vector line drawing and fonts
 *
 */

#ifndef _vector_h_
#define _vector_h_

#include <stdint.h>


static inline void
vector_x(
	uint8_t x
)
{
	PORTB = x;
}

static  inline void
vector_y(
	uint8_t y
)
{
	PORTD = y;
}


static inline void
vector_init(void)
{
	DDRD = 0xFF;
	DDRB = 0xFF;
	vector_x(128);
	vector_y(128);
}


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


uint8_t
draw_char_big(
	uint8_t x,
	uint8_t y,
	uint8_t val
);

uint8_t
draw_char_med(
	uint8_t x,
	uint8_t y,
	uint8_t val
);

uint8_t
draw_char_small(
	uint8_t x,
	uint8_t y,
	uint8_t val
);


#endif
