#ifndef SERVER_MONSTER_H
#define SERVER_MONSTER_H

#include <stdbool.h>
#include "dungeon.h"

void room_generate_monsters(Room *r);
Monster* room_get_monster(Room *r, int x, int y);
void dungeon_kill_all_monsters(Dungeon *d);

#endif
