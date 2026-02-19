#ifndef SERVER_MONSTER_H
#define SERVER_MONSTER_H

#include <stdbool.h>
#include "dungeon.h"

void dungeon_kill_all_monsters(Dungeon *d);
void room_assign_monsters_to_players(Room *r, int *player_ids, int num_players);
int  room_move_monsters_toward_player(Room *r, int player_id, int target_y, int target_x);

#endif
