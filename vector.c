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
	_delay_us((first_dx + first_dy));
#endif
}


static inline void
pixel_delay(void)
{
#ifdef CONFIG_SLOW_SCOPE
	_delay_us(10);
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


static inline void
_draw_digit(
	uint8_t x,
	uint8_t y,
	uint8_t val,
	const uint8_t scale
)
{
	const path_t * p = digits[val];

	while (1)
	{
		uint8_t ox = x + p->x / scale;
		uint8_t oy = y + p->y / scale;

		while (1)
		{
			p++;
			int8_t px = p->x;
			int8_t py = p->y;

			if (px == 0 && py == 0)
				return;

			if (px == -1 && py == -1)
			{
				p++;
				break;
			}

			uint8_t nx = x + px / scale;
			uint8_t ny = y + py / scale;

			line(ox, oy, nx, ny);
			ox = nx;
			oy = ny;
		}
	}
}


void
draw_digit_big(
	uint8_t x,
	uint8_t y,
	uint8_t val
)
{
	_draw_digit(x, y, val, 1);
}


void
draw_digit(
	uint8_t x,
	uint8_t y,
	uint8_t val
)
{
	_draw_digit(x, y, val, 2);
}
