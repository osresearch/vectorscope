/** \file
 * "Space Rocks" game.
 *
 * Might be similar to another game you've played.
 */
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include "sin_table.h"

#define rand() lrand48()

#define STARTING_FUEL 65535
#define MAX_ROCKS	32
#define MAX_BULLETS	4



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
	uint8_t shots;
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
	uint8_t size;
} rock_t;


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
	const uint8_t radius
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
	if (s->shots == 0)
		return;

#define BULLET_RANGE 255
#define BULLET_VEL 16

	b->age = BULLET_RANGE;
	b->p.x = s->p.x;
	b->p.y = s->p.y;
	b->p.vx = s->ax * BULLET_VEL + s->p.vx; // in the direction of the ship
	b->p.vy = s->ay * BULLET_VEL + s->p.vy; // in the direction of the ship

	s->shots--;
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
			bullet_t * const b = &bullets[i];
			if (b->age == 0)
				continue;
			if (collide(&r->p, &b->p, r->size))
			{
				r->size = 0;
				rock_dead = 1;
				// \todo split into mulitple rocks
				break;
			}
		}

		// check for ship collision if this rock wasn't destroyed
		if (!rock_dead && collide(&r->p, &s->p, r->size))
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
			point_update(&b->p);
		else
		if (fire)
		{
			// We can try to fire this one
			ship_fire(s, b);
			fire = 0;
		}
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
	s->angle = 0;
	s->dead = 0;
	s->fuel = STARTING_FUEL;
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
		if (i > num)
		{
			// empty slot
			r->size = 0;
			continue;
		}

		r->size = 64;

#define MIN_RADIUS 1024

		// Make sure that there is space around the center
		int16_t x = rand();
		int16_t y = rand();
printf("x,y=%d,%d\n", x, y);
		if (0 <= x)
			x += MIN_RADIUS;
		else
			x -= MIN_RADIUS;
		if (0 <= y)
			y += MIN_RADIUS;
		else
			y -= MIN_RADIUS;
			
		r->p.x = x;
		r->p.y = y;
		r->p.vx = rand() % 64;
		r->p.vy = rand() % 64;
	}
}


static void
game_init(
	game_t * const g
)
{
	ship_init(&g->s);
	bullets_init(g->b);
	rocks_init(g->r, 8);
}


static void
game_update(
	game_t * const g,
	int8_t rot,
	uint8_t thrust,
	uint8_t fire
)
{
	// Update our position before we fire the gun
	ship_update(&g->s, rot, thrust);

	// Update our bullets before the rocks move
	bullets_update(&g->s, g->b, fire);

	// Update the rocks, checking for collisions
	rocks_update(&g->s, g->b, g->r);

	// If we hit something, start over
	if (g->s.dead)
	{
		printf("game over\n");
		game_init(g);
	}
}


int main(void)
{
	game_t g;

	game_init(&g);

	while (1)
	{
		printf("---\nS: %+6d,%+6d %+6d,%+6d\n", g.s.p.x, g.s.p.y, g.s.p.vx, g.s.p.vy);

		for (uint8_t i = 0 ; i < MAX_ROCKS ; i++)
		{
			const rock_t * const r = &g.r[i];
			if (r->size == 0)
				continue;
			printf("%d: %+6d,%+6d %+6d,%+6d\n", i, r->p.x, r->p.y, r->p.vx, r->p.vy);
		}

		game_update(&g, 0, 0, 0);
	}
}
