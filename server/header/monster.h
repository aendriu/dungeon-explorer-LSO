#ifndef MONSTER
#define MONSTER

#include <stdbool.h>
#include "dungeon.h"

#define MONSTER_SYMBOL 'M'

// Funzioni per la gestione dei mostri
void room_generate_monsters(Room *r);
Monster* room_get_monster(Room *r, int x, int y);

#endif
