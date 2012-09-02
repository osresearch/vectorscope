/**
 * \file
 * Spacewar like gravity game for an oscilloscope
 */

#include <avr/io.h>
#include <stdint.h>
#include <math.h>
#include "vector.h"
#include "clock.h"


// To save cpu time, the sun is at (0,0)
#define SUN_X 0
#define SUN_Y 0

typedef struct
{
	float x;
	float y;
	float vx;
	float vy;
	float heading;
} ship_t;

#define SHIP_COUNT 3
static ship_t ships[SHIP_COUNT];


/** Precomputed inverse square law gravity forces:
 * perl -e 'printf "%d,\n", 8192 / sqrt($_ * 1024) for 1..16'
 */
static const uint8_t gravity_table[] = {
0, // means dead
255, // way fast!
181,
147,
128,
114,
104,
96,
90,
85,
80,
77,
73,
71,
68,
66,
64,
62,
60,
58,
57,
55,
54,
53,
52,
51,
50,
49,
48,
47,
46,
45,
};

static float
gravity(
	float dx,
	float dy
)
{
#if 0
	dx /= 256;
	dy /= 256;

	// maximum dx == 32768 => 128
	// maximum delta = 128*128*2 == 32768
	// gravity table has up to 32 entries
	uint16_t delta = dx*dx + dx*dx;
	return gravity_table[delta / 1024];
#else
	if (dx == 0 && dy == 0)
		return 0;

	float delta = dx * dx + dy * dy;
	return 1000 / sqrt(delta);
#endif
}


void
ship_update(
	ship_t * s
)
{
	float dx = SUN_X - s->x;
	float dy = SUN_X - s->y;
	float g = gravity(dx, dy);
	if (g == 0)
	{
		// ship is dead; reset it somewhere random
		s->vx = s->vy = 0;
		s->x = TCNT1;
		s->y = ~TCNT1;
		return;
	}


	// Project the gravity in the direction of the sun.
	const float dt = 1;
	float theta = atan2(dy, dx);
	s->vx += g * cos(theta) * dt;
	s->vy += g * sin(theta) * dt;

	s->x += s->vx * dt;
	s->y += s->vy * dt;

	if (s->x > 32768)
		s->x = s->x - 65536.0;
	if (s->x < -32768)
		s->x = 65536.0 - s->x;
	if (s->y > 32768)
		s->y = s->y - 65536.0;
	if (s->y < -32768)
		s->y = 65536.0 - s->y;
}


void
ship_draw(
	ship_t * s,
	int i
)
{
	uint8_t x = ((uint16_t) s->x + 0x8000) / 256;
	uint8_t y = ((uint16_t) s->y + 0x8000) / 256;

	//line(x, y, x+s->vx*100, y+s->vy*100);
	draw_digit(x, y, i);

	//line_horiz(x-5, y, 10);
	//line_vert(x, y-5, 10);
}


void
ship_loop(void)
{
	for (int i = 0 ; i < SHIP_COUNT ; i++)
	{
		ship_t * const ship = &ships[i];
		ship->x = 0;
		ship->y = i * 4000 + 12000;
		ship->vx = i * 20 + 0;
		ship->vy = 0;
	}

	while (1)
	{
		for (int i = 0 ; i < SHIP_COUNT ; i++)
		{
			ship_draw(&ships[i], i);
			ship_update(&ships[i]);
		}

		PORTB = PORTD = 128;
	}
}
