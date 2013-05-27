/**
 * \file
 * Planet orbit simulator.
 */

#include <avr/io.h>
#include <stdint.h>
#include <math.h>
#include "vector.h"
#include "clock.h"


// To save cpu time, the sun is at (0,0)
#define SUN_X 0
#define SUN_Y 0
#define SUN_MASS 1.989e30 // kg

#define GRAVITY 6.67384e-11 // N m / kg^2
#define ONE_AU 149e9 // m

typedef struct
{
	float x;
	float y;
	float vx;
	float vy;
	float mass;
} planet_t;


#define PLANET_COUNT 4
static planet_t planets[] =
{
	{
		// Mercury
		.mass = 328.5e21, // kg
		.x = 0,
		.y = 46.001e9, // m
		.vx = 47870, // m/s
		.vy = 0,
	},
	{
		// Venus
		.mass = 4.868e24, // kg
		.x = 0,
		.y = 107.477e9, // m
		.vx = 35020, // m/s
		.vy = 0,
	},
	{
		// Earth
		.mass = 5.974e24,
		.x = 0,
		.y = 147.098e9, // m
		.vx = 29780, // m/s
		.vy = 0,
	},
	{
		// Mars
		.mass = 6.419e23,
		.x = 0,
		.y = 206.669e9, // m
		.vx = 24007, // m/s
		.vy = 0,
	},
};


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


/** Compute G m1*m2 / r^2 for a planet.
 *
 * Distances are in m, mass in kg.
 * Return is in m/s^2
 */
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

	// dx,dy are in m
	// mass is in kg
	float r_squared = (dx * dx + dy * dy);
	return GRAVITY * SUN_MASS / r_squared;
#endif
}


void
planet_update(
	planet_t * s
)
{
	float dx = SUN_X - s->x;
	float dy = SUN_X - s->y;
	float g = gravity(dx, dy);

	// Project the gravity in the direction of the sun.
	const float dt = 3600.0; // one day
	float theta = atan2(dy, dx);
	s->vx += g * cos(theta) * dt;
	s->vy += g * sin(theta) * dt;

	s->x += s->vx * dt;
	s->y += s->vy * dt;

/*
	if (s->x > 32768)
		s->x = s->x - 65536.0;
	if (s->x < -32768)
		s->x = 65536.0 - s->x;
	if (s->y > 32768)
		s->y = s->y - 65536.0;
	if (s->y < -32768)
		s->y = 65536.0 - s->y;
*/
}


void
planet_draw(
	planet_t * s,
	int i
)
{
	// Scale x and y so that 1.6 au == 128
	float x = (s->x / (ONE_AU * 1.6)) * 128 + 128;
	float y = (s->y / (ONE_AU * 1.6)) * 128 + 128;
	if (x < 0 || x > 250 || y < 0 || y > 250)
		return;

	//line(x, y, x+s->vx*100, y+s->vy*100);
	draw_char_small(x, y, i + '0');

	//line_horiz(x-5, y, 10);
	//line_vert(x, y-5, 10);
}


void
planet_loop(void)
{
	for (int i = 0 ; i < PLANET_COUNT ; i++)
	{
		planet_draw(&planets[i], i+1);
		planet_update(&planets[i]);
	}

	PORTB = PORTD = 128;
}
