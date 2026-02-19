#ifndef SERVER_PLAYER_STATE_H
#define SERVER_PLAYER_STATE_H

#include "dungeon.h"

#define MAX_ITEMS_PER_PLAYER 25

typedef struct {
    int player_id;
    int items_collected;
    int room_id;
    int x, y;
    char symbol;
    Item items[MAX_ITEMS_PER_PLAYER];
    int normal_items_count;
    int quest_items_count;
    int traps_triggered;
    int monsters_killed;
} Player;

#endif
