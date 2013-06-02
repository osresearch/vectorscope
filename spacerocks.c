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
#define STARTING_AMMO 65535
#define MAX_ROCKS	32
#define MAX_BULLETS	4
#define ROCK_VEL	64

static int frame_num = 0;


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
	uint16_t shots;
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
static const int8_t rock_paths[][8*2] = {
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
		+2, -4,
		-2, -4,
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

	if (0) fprintf(stderr, "%d,%d -> %d,%d => %d,%d\n",
		p->x, p->y,
		q->x, q->y,
		dx, dy
	);

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
#define BULLET_VEL 8

	b->age = BULLET_RANGE;
	b->p.x = s->p.x;
	b->p.y = s->p.y;
	b->p.vx = s->ax * BULLET_VEL + s->p.vx; // in the direction of the ship
	b->p.vy = s->ay * BULLET_VEL + s->p.vy; // in the direction of the ship

	fprintf(stderr, "fire: vx=%d vy=%d\n", b->p.vx, b->p.vy);

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

fprintf(stderr, "%d: %d,%d\n", i, x, y);
		r->size = size;
		r->p.x = x;
		r->p.y = y;
		r->p.vx = rand() % ROCK_VEL;
		r->p.vy = rand() % ROCK_VEL;
		r->type = rand() % NUM_ROCK_TYPES;
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
//fprintf(stderr, "Bullet check\n");
		for (uint8_t j = 0 ; j < MAX_BULLETS ; j++)
		{
			bullet_t * const b = &bullets[j];
			if (b->age == 0)
				continue;
			if (collide(&r->p, &b->p, r->size))
			{
				fprintf(stderr, "rock %d is dead\n", i);

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

//fprintf(stderr, "Ship check\n");
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
			point_update(&b->p);
			fprintf(stderr, "%d,%d\n", b->p.x/256, b->p.y/256);
		} else
		if (fire)
		{
			// We can try to fire this one
			ship_fire(s, b);
			fire = 0;
		}
	}

	if (fire)
		fprintf(stderr, "no bullets\n");
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
	s->angle = rand();
	s->dead = 0;
	s->fuel = STARTING_FUEL;
	s->shots = STARTING_AMMO;
	s->ax = cos_lookup(s->angle);
	s->ay = sin_lookup(s->angle);
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
#define MIN_RADIUS 1024
		int16_t x = rand();
		int16_t y = rand();
		uint16_t size = (rand() % 32) * 256 + 512;
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
		fprintf(stderr, "game over\n");
		game_init(g);
	}
}


static void
draw_path(
	uint8_t x,
	uint8_t y,
	const int8_t * const p,
	uint8_t n
)
{
	for (uint8_t i = 0 ; i < n ; i++)
		printf("%d %d %d\n", x + p[2*i+0], y + p[2*i+1], frame_num);
	printf("\n");
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
		ROTATE(-4, -4),
		ROTATE(0,8),
		ROTATE(+4,-4),
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
	const int8_t * const rp = rock_paths[r->type];
	int8_t path[8*2];
	for (uint8_t i = 0 ; i < 16 ; i++)
		path[i] = (rp[i] * s) / 4;

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


int main(void)
{
	srand(getpid());

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
		frame_num++;
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
