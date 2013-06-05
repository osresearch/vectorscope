/** \file
 * "Space Rocks" game.
 *
 * Might be similar to another game you've played.
 */
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include "sin_table.h"
#include "memspaces.h"

#ifdef __i386__
#define fastrand() lrand48()
#else
#include <avr/io.h>
#include "vector.h"
#include "bits.h"
#include "usb_serial.h"

static inline uint16_t
fastrand(void)
{
	uint16_t r = rand();
	r ^= rand() << 1;
	return r;
}
#endif

#define STARTING_FUEL 65535
#define STARTING_AMMO 200
#define MAX_ROCKS	16
#define MAX_BULLETS	4
#define ROCK_VEL	128
#define MIN_RADIUS 	10000


typedef struct
{
	int16_t x;
	int16_t y;
	int16_t vx;
	int16_t vy;
} point_t;

typedef struct
{
	point_t p;
	int16_t ax;
	int16_t ay;
	uint16_t fuel;
	uint16_t ammo;
	uint8_t angle;
	uint8_t dead;
} ship_t;

typedef struct
{
	point_t p;
	uint8_t age;
} bullet_t;

typedef struct
{
	point_t p;
	uint8_t type;
	uint16_t size;
} rock_t;


/** There are various types of rocks */
static const int8_t PROGMEM rock_paths[][8*2] = {
	{
		-4, -2,
		-4, +2,
		-2, +4,
		 0, +2,
		+2, +4,
		+4, -2,
		 0, -4,
		-4, -2,
	},
	{
		-4, -2,
		-3,  0,
		-4, +2,
		-2, +4,
		+4, +2,
		+2, +1,
		+4, -3,
		-4, -2,
	},
	{
		-2, -4,
		-4, -1,
		-3, +4,
		+2, +4,
		+4, +1,
		+3, -4,
		 0, -1,
		-2, -4,
	},
	{
		-4, -2,
		-4, +2,
		-2, +4,
		+2, +4,
		+4, +2,
		+4, -2,
		+1, -4,
		-4, -2,
	},
};


#define NUM_ROCK_TYPES (sizeof(rock_paths) / sizeof(*rock_paths))


typedef struct
{
	ship_t s;
	bullet_t b[MAX_BULLETS];
	rock_t r[MAX_ROCKS];
} game_t;


static void
point_update(
	point_t * const p
)
{
	p->x += p->vx;
	p->y += p->vy;
}


static int
collide(
	const point_t * const p,
	const point_t * const q,
	const int16_t radius
)
{
	int16_t dx = p->x - q->x;
	int16_t dy = p->y - q->y;

	if (-radius < dx && dx < radius
	&&  -radius < dy && dy < radius)
		return 1;

	// Not in the bounding box.
	return 0;
}


static void
ship_update_angle(
	ship_t * const s,
	int8_t rot
)
{
	if (rot == 0)
		return;

	s->angle += rot;
	s->ax = sin_lookup(s->angle);
	s->ay = cos_lookup(s->angle);
}


static void
ship_update_thrust(
	ship_t * const s,
	uint8_t thrust
)
{
	if (thrust == 0 || s->fuel == 0)
		return;

	if (s->fuel < thrust)
		thrust = s->fuel;

	s->fuel -= thrust;
	s->p.vx += (s->ax * thrust) / 128;
	s->p.vy += (s->ay * thrust) / 128;
}


static void
ship_fire(
	ship_t * const s,
	bullet_t * const b
)
{
	if (s->ammo == 0)
		return;

#define BULLET_RANGE 32
#define BULLET_VEL 8

	b->age = BULLET_RANGE;
	b->p.x = s->p.x;
	b->p.y = s->p.y;
	b->p.vx = s->ax * BULLET_VEL + s->p.vx; // in the direction of the ship
	b->p.vy = s->ay * BULLET_VEL + s->p.vy; // in the direction of the ship

#ifdef __i386__
	fprintf(stderr, "fire: vx=%d vy=%d\n", b->p.vx, b->p.vy);
#endif

	s->ammo--;
}


static void
ship_update(
	ship_t * const s,
	int8_t rot,
	uint8_t thrust
)
{
	ship_update_angle(s, rot);
	ship_update_thrust(s, thrust);
	point_update(&s->p);
}

static void
rock_create(
	rock_t * const rocks,
	int16_t x,
	int16_t y,
	int16_t size
)
{
	// find a free rock
	for (uint8_t i = 0 ; i < MAX_ROCKS ; i++)
	{
		rock_t * const r = &rocks[i];
		if (r->size != 0)
			continue;

		r->size = size;
		r->p.x = x;
		r->p.y = y;
		r->p.vx = fastrand() % ROCK_VEL;
		r->p.vy = fastrand() % ROCK_VEL;
		r->type = fastrand() % (NUM_ROCK_TYPES * 8);
		return;
	}
}


static void
rocks_update(
	ship_t * const s,
	bullet_t * const bullets,
	rock_t * const rocks
)
{
	for (uint8_t i = 0 ; i < MAX_ROCKS ; i++)
	{
		rock_t * const r = &rocks[i];
		if (r->size == 0)
			break;

		point_update(&r->p);
		uint8_t rock_dead = 0;

		// check for bullet collision
		for (uint8_t j = 0 ; j < MAX_BULLETS ; j++)
		{
			bullet_t * const b = &bullets[j];
			if (b->age == 0)
				continue;
			if (collide(&r->p, &b->p, r->size))
			{
				uint16_t new_size = r->size / 2;
				if (new_size > 256)
				{
					rock_create(rocks, r->p.x, r->p.y, new_size);
					rock_create(rocks, r->p.x, r->p.y, new_size);
					rock_create(rocks, r->p.x, r->p.y, new_size);
				}

				r->size = 0;
				b->age = 0;
				rock_dead = 1;

				break;
			}
		}

		if (rock_dead)
			continue;

		 if (collide(&r->p, &s->p, r->size))
			s->dead = 1;
	}
}


static void
bullets_update(
	ship_t * const s,
	bullet_t * const bullets,
	uint8_t fire
)
{
	for (uint8_t i = 0 ; i < MAX_BULLETS ; i++)
	{
		bullet_t * const b = &bullets[i];
		if (b->age != 0)
		{
			b->age--;
			point_update(&b->p);
		} else
		if (fire)
		{
			// We can try to fire this one
			ship_fire(s, b);
			fire = 0;
		}
	}

	if (fire)
	{
#ifdef __i386__
		fprintf(stderr, "no bullets\n");
#endif
	}
}



static void
ship_init(
	ship_t * const s
)
{
	s->p.x = 0;
	s->p.y = 0;
	s->p.vx = 0;
	s->p.vy = 0;
	s->angle = fastrand();
	s->dead = 0;
	s->fuel = STARTING_FUEL;
	s->ammo = STARTING_AMMO;
	s->ax = sin_lookup(s->angle);
	s->ay = cos_lookup(s->angle);
}


static void
bullets_init(
	bullet_t * const bullets
)
{
	for (uint8_t i = 0 ; i < MAX_BULLETS ; i++)
	{
		bullet_t * const b = &bullets[i];
		b->age = 0;
	}
}



static void
rocks_init(
	rock_t * const rocks,
	uint8_t num
)
{
	for (uint8_t i = 0 ; i < MAX_ROCKS ; i++)
	{
		rock_t * const r = &rocks[i];
		r->size = 0;
	}

	for (uint8_t i = 0 ; i < num ; i++)
	{
		// Make sure that there is space around the center
		int16_t x = fastrand();
		int16_t y = fastrand();
		uint16_t size = (fastrand() % 32) * 256 + 512;
		if (0 <= x)
			x += MIN_RADIUS;
		else
			x -= MIN_RADIUS;
		if (0 <= y)
			y += MIN_RADIUS;
		else
			y -= MIN_RADIUS;

		rock_create(rocks, x, y, size);

	}
}


static void
game_init(
	game_t * const g
)
{
	ship_init(&g->s);
	bullets_init(g->b);
	rocks_init(g->r, 5);
}


static void
game_update(
	game_t * const g,
	int8_t rot,
	uint8_t thrust,
	uint8_t fire
)
{
	//printf("rot=%d thrust=%d fire=%d\n", rot, thrust, fire);

	// Update our position before we fire the gun
	ship_update(&g->s, rot, thrust);

	// Update our bullets before the rocks move
	bullets_update(&g->s, g->b, fire);

	// Update the rocks, checking for collisions
	rocks_update(&g->s, g->b, g->r);

	// If we hit something, start over
	if (g->s.dead)
	{
#ifdef __i386__
		fprintf(stderr, "game over\n");
#endif
		game_init(g);
	}
}


static inline uint8_t
same_quad(
	int16_t p1,
	int16_t p2
)
{
	if (p1 < 0)
		return p2 < 0;
	if (p1 > 255)
		return p2 > 255;
	if (p2 < 0)
		return 0;
	if (p2 > 255)
		return 0;
	return 1;
}


static void
draw_path(
	uint8_t x,
	uint8_t y,
	const int8_t * const p,
	uint8_t n
)
{
	int16_t ox = x + p[0];
	int16_t oy = y + p[1];
	for (uint8_t i = 1 ; i < n ; i++)
	{
		int16_t px = x + p[2*i+0];
		int16_t py = y + p[2*i+1];

		uint8_t do_line = same_quad(px, ox) && same_quad(py, oy);

		uint8_t ox8 = ox;
		uint8_t oy8 = oy;
		uint8_t px8 = px;
		uint8_t py8 = py;

		if (do_line)
		{
#ifdef __i386__
			printf("%d %d\n%d %d\n\n", ox8, oy8, px8, py8);
#else
			line(ox8, oy8, px8, py8);
#endif
		}
		ox = px;
		oy = py;
	}
}


#define ROTATE(x,y) \
	((x) * s->ay + (y) * s->ax) / 128, \
	((y) * s->ay - (x) * s->ax) / 128 \

/** Generate the ship vectors, rotated by the current angle.
 * The ship_update_rot() has cached the sin/cos value for us.
 */
static void
draw_ship(
	const ship_t * const s
)
{
	int8_t path[] = {
		ROTATE(0,0),
		ROTATE(-6, -6),
		ROTATE(0,12),
		ROTATE(+6,-6),
		ROTATE(0,0),
	};

	draw_path(s->p.x / 256 + 128, s->p.y / 256 + 128, path, 5);
}


/** Draw the bullets lined in the direction of travel */
static void
draw_bullet(
	const bullet_t * const b
)
{
	int8_t path[] = {
		-1, -1,
		-1, +1,
		+1, +1,
		+1, -1,
		-1, -1,
		//b->p.vx/64, b->p.vy/64,
	};

	draw_path(b->p.x / 256 + 128, b->p.y / 256 + 128, path, 5);
}



static void
draw_rock(
	const rock_t * const r
)
{
	//printf("rock %d,%d size %d\n", r->p.x/256+128, r->p.y/256+128, r->size/256);

	const int8_t s = r->size / 256;
	const uint8_t swap_xy = r->type & 1;
	const uint8_t flip_x = r->type & 2;
	const uint8_t flip_y = r->type & 4;
	const int8_t * const rp = rock_paths[r->type >> 3];

	int8_t path[8*2];
	for (uint8_t i = 0 ; i < 8 ; i++)
	{
		int8_t rx = pgm_read_byte(&rp[2*i +  swap_xy]);
		int8_t ry = pgm_read_byte(&rp[2*i + !swap_xy]);
		int8_t x = (rx * s) / 4;
		int8_t y = (ry * s) / 4;
		path[2*i+0] = flip_x ? -x : x;
		path[2*i+1] = flip_y ? -y : y;
	}

	draw_path(r->p.x / 256 + 128, r->p.y / 256 + 128, path, 8);
}


static void
game_vectors(
	const game_t * const g
)
{
	draw_ship(&g->s);

	for (uint8_t i = 0 ; i < MAX_ROCKS ; i++)
	{
		const rock_t * const r = &g->r[i];
		if (r->size == 0)
			continue;
		draw_rock(r);
	}

	for (uint8_t i = 0 ; i < MAX_BULLETS ; i++)
	{
		const bullet_t * const b = &g->b[i];
		if (b->age == 0)
			continue;
		draw_bullet(b);
	}
}


#ifdef __i386__
int main(void)
{
	srand48(getpid());

	game_t g;

	game_init(&g);

	while (1)
	{
#if 0
		printf("---\nS: %+6d,%+6d %+6d,%+6d %d\n", g.s.p.x/256, g.s.p.y/256, g.s.p.vx, g.s.p.vy, g.s.angle);

		for (uint8_t i = 0 ; i < MAX_ROCKS ; i++)
		{
			const rock_t * const r = &g.r[i];
			if (r->size == 0)
				continue;
			printf("%d: %+6d,%+6d %+6d,%+6d\n", i, r->p.x/256, r->p.y/256, r->p.vx, r->p.vy);
		}
#else
		game_vectors(&g);
#endif

		int c;

		while (1)
		{
			c = getchar();
			if (c == '\n')
				continue;
			if (c == -1)
				return 0;
			break;
		}

		game_update(
			&g,
			c == 'l' ? -17 : c == 'r' ? +17 : 0,
			c == 't' ? 16 : 0,
			c == 'f' ? 1 : 0
		);
	}
}

#else

static inline uint8_t
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


/**
 * Enable ADC and select input ADC0 / F0
 * Select high-speed mode, left aligned
 * Use clock divisor of 2
 * System clock is 16 MHz on teensy, 8 MHz on tiny,
 * conversions take 13 ticks, so divisor == 128 (1,1,1) should
 * give 9.6 KHz of samples.
 */
static uint8_t adc_input;
static uint16_t adc_values[4];

static void
joy_init(void)
{
	ADMUX = adc_input
		| (0 << REFS1)
		| (1 << REFS0)
		;

	ADCSRA = 0
		| (1 << ADEN) // enable ADC
		| (0 << ADSC) // don't start yet
		| (0 << ADIE) // don't enable the interrupts
		| (0 << ADPS2)
		| (1 << ADPS1)
		| (1 << ADPS0)
		;

	ADCSRB = 0
		| (1 << ADHSM) // enable highspeed mode
		;

	DDRF = 0;
	PORTF = 0x00;
	sbi(PORTF, 4);
	sbi(PORTF, 5);
	sbi(DIDR0, ADC0D);
	sbi(DIDR0, ADC1D);

	// Start the first conversion!
	sbi(ADCSRA, ADSC);
}


static void
adc_read(void)
{
	if (bit_is_set(ADCSRA, ADSC))
		return;

	adc_values[adc_input] = ADC;
	adc_input = (adc_input + 1) % 2;

	// Configure for the next input
	ADMUX = (ADMUX & ~0x1F) | adc_input;

	// Start the next conversion
	sbi(ADCSRA, ADSC);
}


static void
draw_hex(
	uint8_t x,
	uint8_t y,
	uint16_t v
)
{
	draw_char_small(x+0, y, hexdigit(v >> 8));
	draw_char_small(x+20, y, hexdigit(v >> 4));
	draw_char_small(x+40, y, hexdigit(v >> 0));
}


static game_t g;

int main(void)
{
	// set for 16 MHz clock
#define CPU_PRESCALE(n) (CLKPR = 0x80, CLKPR = (n))
	CPU_PRESCALE(0);

	usb_init();
	joy_init();
	game_init(&g);

	DDRB = 0xFF;
	DDRD = 0xFF;

	uint8_t last_fire = 0;

	while (1)
	{
		adc_read();
		adc_values[2] = in(0xF4);
		adc_values[3] = in(0xF5);

		//line_horiz(0,0, 250);
		//line_vert(0,0, 250);
		game_vectors(&g);

		draw_char_small( 0, 230, 'F');
		draw_char_small(20, 230, '=');
		draw_hex(40, 230, g.s.fuel >> 4);

		draw_char_small( 0, 200, 'A');
		draw_char_small(20, 200, '=');
		draw_hex(40, 200, g.s.ammo);

		draw_hex(255-60, 30, adc_values[0]);
		draw_hex(255-60, 10, adc_values[1]);
		//draw_hex(0, 80, adc_values[2]);
		//draw_hex(80, 80, adc_values[3]);

/*
		if (in(BUTTON_L) && in(button_R))
			ship_hyperspace(&g.s);
*/

		int c = -1;

		int8_t rot = (adc_values[0] >> 6) - (512 >> 6);
		int8_t thrust = (adc_values[1] >> 2) - (512 >> 2);
		if (thrust == -128)
			g.s.p.vx = g.s.p.vy = 0;
		if (thrust < 0)
			thrust = 0;
	
		int8_t fire = adc_values[2] == 0;

		game_update(
			&g,
			-rot,
			thrust,
			last_fire ? 0 : fire
		);

		last_fire = fire;
	}
}
#endif
