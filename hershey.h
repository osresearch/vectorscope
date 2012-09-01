#ifndef _hershey_h_
#define _hershey_h_

#include <stdint.h>

typedef struct
{
	int8_t x;
	int8_t y;
} __attribute__((__packed__))
path_t;

extern const path_t digits[][32];

#endif
