/** \file
 * "Space Rocks" game.
 *
 * Might be similar to another game you've played.
 */
#include <stdio.h>
#include <inttypes.h>

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


#define NUM_ROCKS	32
#define NUM_BULLETS	4

typedef struct
{
	ship_t s;
	bullet_t b[NUM_BULLETS];
	rock_t r[NUM_ROCKS];
} game_t;


static void
point_update(
	point_t * const p
)
{
	p->x += p->vx;
	p->y += p->vy;
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
	s->vx += (s->ax * thrust) / 128;
	s->vy += (s->ay * thrust) / 128;
}


static void
ship_update_ammo(
	ship_t * const s,
	uint8_t fire
)
{
	if (fire == 0 || s->shots == 0)
		return;

	// find the first free slot for bullet tracking
	for (uint8_t i = 0 ; i < NUM_BULLETS ; i++)
	{
		bullet_t * const b = &bullets[i];
		if (b->age != 0)
			continue;

		b->age = 255;
		b->p.x = s->p.x;
		b->p.y = s->p.y;
		b->p.vx = s->ax; // in the direction of the ship
		b->p.vy = s->ay; // in the direction of the ship

		s->shots--;

		return;
	}

	// no bullets available.  don't fire.
}


static void
ship_update(
	ship_t * const s,
	int8_t rot,
	uint8_t thrust,
	uint8_t fire
)
{
	ship_update_angle(s, rot);
	ship_udpate_thrust(s, thrust);

	// Update our position before we fire the gun
	point_update(&s->p);

	ship_update_bullets(s, fire);
}


static void
rocks_update(
	ship_t * const s,
	rock_t * const rocks,
	bullet_t * const bullets
)
{
	for (uint8_t i = 0 ; i < NUM_ROCKS ; i++)
	{
		rock_t * const r = &rocks[i];
		if (r->size == 0)
			break;

		point_update(&r->p);
		uint8_t rock_dead = 0;

		// check for bullet collision
		for (uint8_t j = 0 ; j < NUM_BULLETS ; j++)
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
	bullet_t * const bullets
)
{
	for (uint8_t i = 0 ; i < NUM_BULLETS ; i++)
	{
		bullet_t * const b = &bullets[i];
		if (b->age == 0)
			continue;

		point_update(&b->p);
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
	s->ax = sin_lookup(s->angle);
	s->ay = cos_lookup(s->angle);
}


static void
bullets_init(
	bullet_t * const bullets
)
{
	for (uint8_t i = 0 ; i < NUM_BULLETS ; i++)
	{
		bullet_t * const b = &bullets[i];
		b->age = 0;
	}
}


static void
rocks_init(
	rocks_t * const rocks,
	uint8_t num
)
{
	for (uint8_t i = 0 ; i < NUM_ROCKS ; i++)
	{
		rock_t * const r = &rocks[i];
		if (i > num)
		{
			// empty slot
			r->size = 0;
			continue;
		}

		r->size = 64;

		// Make sure that there is space around the center
		uint8_t x = rand();
		uint8_t y = rand();
		if (0 <= x)
			x += min_radius;
		else
			x -= min_radius;
		if (0 <= y)
			y += min_radius;
		else
			y -= min_radius;
			
		r->p.x = x;
		r->p.y = y;
		r->p.vx = rand() % 64;
		r->p.vy = rand() % 64;
	}
}


static void
game_init(
	game_t * const g,
)
{
	ship_init(&g->s);
	bullets_init(&g->b);
	rocks_init(&g->r, 8);
}


static void
game_update(
	game_t * const g,
	int8_t rot,
	uint8_t thrust,
	uint8_t fire
)
{
	ship_update(&g->s, rot, thrust, fire);
	bullet_update(&g->b);
	rocks_update(&g->s, &g->b, &g->r);
	if (&g->s.dead)
		game_init(g);
}


int main(void)
{
	game_t g;

	game_init(&g);

	while (1)
	{
		printf("%+6d,%+6d %+6d,%+6d\n", g.s.p.x, g.s.p.y, g.s.p.vx, g.s.p.vy);

		for (uint8_t i = 0 ; i < NUM_ROCKS ; i++)
		{
			const rock_t * const r = &g.r[i];
			if (r->size == 0)
				continue;
			printf("%d: %+6d,%+6d %+6d,%+6d\n", i, g.s.p.x, g.s.p.y, g.s.p.vx, g.s.p.vy);
		}

		game_update(g, 0, 0, 0);
	}
