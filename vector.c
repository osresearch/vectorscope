/**
 * \file
 * Vector line drawing and fonts
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
#include "hershey.h"
#include "sin_table.h"


/** Slow scopes require time at each move; give them the chance */
#define CONFIG_SLOW_SCOPE

static void
moveto(
	uint8_t x,
	uint8_t y
)
{
#ifdef CONFIG_SLOW_SCOPE
	int first_dx = PORTB - x;
	int first_dy = PORTD - y;
	if (first_dx < 0)
		first_dx = -first_dx;
	if (first_dy < 0)
		first_dy = -first_dy;
#endif

	PORTB = x;
	PORTD = y;

#ifdef CONFIG_SLOW_SCOPE
	// Allow the scope to reach this point
	_delay_us((first_dx + first_dy) / 2);
#endif
}


static inline void
pixel_delay(void)
{
#ifdef CONFIG_SLOW_SCOPE
	_delay_us(1);
#endif
}



void
line_vert(
	uint8_t x0,
	uint8_t y0,
	uint8_t w
)
{
	moveto(x0, y0);
	for (uint8_t i = 0 ; i < w ; i++)
	{
		PORTD = y0++;
		pixel_delay();
	}
}

void
line_horiz(
	uint8_t x0,
	uint8_t y0,
	uint8_t h
)
{
	moveto(x0, y0);
	for (uint8_t i = 0 ; i < h ; i++)
	{
		PORTB = x0++;
		pixel_delay();
	}
}


void
line(
	uint8_t x0,
	uint8_t y0,
	uint8_t x1,
	uint8_t y1
)
{
#if 1
	int dx;
	int dy;
	int sx;
	int sy;

	if (x0 == x1)
	{
		if (y0 < y1)
			line_vert(x0, y0, y1 - y0);
		else
			line_vert(x0, y1, y0 - y1);
		return;
	}

	if (y0 == y1)
	{
		if (x0 < x1)
			line_horiz(x0, y0, x1 - x0);
		else
			line_horiz(x1, y0, x0 - x1);
		return;
	}

	if (x0 <= x1)
	{
		dx = x1 - x0;
		sx = 1;
	} else {
		dx = x0 - x1;
		sx = -1;
	}

	if (y0 <= y1)
	{
		dy = y1 - y0;
		sy = 1;
	} else {
		dy = y0 - y1;
		sy = -1;
	}

	int err = dx - dy;

	moveto(x0, y0);

	while (1)
	{
		if (x0 == x1 && y0 == y1)
			break;

		int e2 = 2 * err;
		if (e2 > -dy)
		{
			err = err - dy;
			PORTB = (x0 += sx);
		}
		if (e2 < dx)
		{
			err = err + dx;
			PORTD = (y0 += sy);
		}

		pixel_delay();
	}
#else
	uint8_t dx;
	uint8_t dy;
	int8_t sx;
	int8_t sy;

	if (x0 < x1)
	{
		dx = x1 - x0;
		sx = 1;
	} else {
		dx = x0 - x1;
		sx = -1;
	}

	if (y0 < y1)
	{
		dy = y1 - y0;
		sy = 1;
	} else {
		dy = y0 - y1;
		sy = -1;
	}

	if (dx > dy)
	{
		while (x0 != x1)
		{
			PORTB = x0;
			PORTD = y0;
			x0 += sx;
			y0 += sy;
		}
	} else {
		while (y0 != y1)
		{
			PORTB = x0;
			PORTD = y0;
			x0 += sx;
			y0 += sy;
		}
	}
#endif
}


static inline int8_t
scaling(
	int8_t d,
	uint8_t scale
)
{
	if (scale == 0)
		return d / 2;
	if (scale == 1)
		return d;
	if (scale == 2)
		return (d * 3) / 2;
	if (scale == 3)
		return d * 2;
	return d;
}


static inline uint8_t
_draw_char(
	const uint8_t x,
	const uint8_t y,
	const uint8_t c,
	const uint8_t scale
)
{
	const hershey_char_t * p = &hershey_simplex[c - 0x20];
	const uint8_t count = pgm_read_byte(&p->count);

	uint8_t ox = x;
	uint8_t oy = y;
	uint8_t pen_down = 0;

	for (uint8_t i = 0 ; i < count ; i++)
	{
		const int8_t px = pgm_read_byte(&p->points[i*2+0]);
		const int8_t py = pgm_read_byte(&p->points[i*2+1]);
		if (px == -1 && py == -1)
		{
			pen_down = 0;
			continue;
		}

		const uint8_t nx = x + scaling(px, scale);
		const uint8_t ny = y + scaling(py, scale);

		if (pen_down)
			line(ox, oy, nx, ny);

		pen_down = 1;
		ox = nx;
		oy = ny;
	}

	const uint8_t width = pgm_read_byte(&p->width);
	return scaling(width, scale);
}


uint8_t
draw_char_big(
	uint8_t x,
	uint8_t y,
	uint8_t c
)
{
	return _draw_char(x, y, c, 2);
}


uint8_t
draw_char_med(
	uint8_t x,
	uint8_t y,
	uint8_t c
)
{
	return _draw_char(x, y, c, 1);
}

uint8_t
draw_char_small(
	uint8_t x,
	uint8_t y,
	uint8_t c
)
{
	return _draw_char(x, y, c, 0);
}
