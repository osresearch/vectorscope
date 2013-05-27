/** \file
 * Super simple font from Asteroids.
 *
 * http://www.edge-online.com/wp-content/uploads/edgeonline/oldfiles/images/feature_article/2009/05/asteroids2.jpg
 */

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <stdint.h>
#include "asteroids-font.h"

#define P(x,y)	((((x) & 0xF) << 4) | (((y) & 0xF) << 0))
#define UP 0xFE
#define LAST 0xFF
 
const PROGMEM asteroids_char_t asteroids_font[] = {
	['0' - 0x20] = { P(0,0), P(8,0), P(8,12), P(0,12), P(0,0), P(8,12), LAST },
	['1' - 0x20] = { P(4,0), P(4,12), LAST },
	['2' - 0x20] = { P(0,12), P(8,12), P(8,7), P(0,5), P(0,0), P(8,0), LAST },
	['3' - 0x20] = { P(0,12), P(8,12), P(8,0), P(0,0), UP, P(0,6), P(8,6), LAST },
	['4' - 0x20] = { P(0,12), P(0,6), P(8,6), UP, P(8,12), P(8,0), LAST },
	['5' - 0x20] = { P(0,0), P(8,0), P(8,5), P(0,6), P(0,12), P(8,12), LAST },
	['6' - 0x20] = { P(0,12), P(0,0), P(8,0), P(8,5), P(0,7), LAST },
	['7' - 0x20] = { P(0,12), P(8,12), P(8,6), P(4,0), LAST },
	['8' - 0x20] = { P(0,0), P(8,0), P(8,12), P(0,12), P(0,0), UP, P(0,6), P(8,6), LAST },
	['9' - 0x20] = { P(8,0), P(8,12), P(0,12), P(0,7), P(8,5), LAST },
};

