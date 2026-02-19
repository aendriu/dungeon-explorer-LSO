#ifndef SERVER_DUNGEON_H
#define SERVER_DUNGEON_H

#include <stdbool.h>
#include <pthread.h>
#include "../../utils/enums.h"

typedef enum {
    ITEM_NONE,
    ITEM_NORMAL,
    ITEM_QUEST,
    ITEM_TRAP,
    ITEM_BOOBYTRAP,
    ITEM_APPLE
} ItemType;

typedef struct {
    int x, y;
    ItemType type;
    bool collected;
} Item;

typedef struct {
    int x, y;
    MonsterState state;
    int target_player_id;
    int failed_moves;
} Monster;

// Room è una stanza del dungeon
typedef struct {
    int id;
    int width;
    int height;
    Item **items;
    Monster ***monsters;
} Room;

// Dungeon contiene più stanze
typedef struct {
    int num_rooms;
    Room **rooms;  // array di stanze
} Dungeon;

extern int quest_items_collected;
extern pthread_mutex_t quest_items_mutex;

Item* room_get_item(Room *r, int x, int y);
Monster* room_get_monster(Room *r, int x, int y);
Dungeon* dungeon_create(int num_rooms);
void dungeon_destroy(Dungeon *d);
Room* dungeon_get_room(Dungeon *d, int room_id);

#endif

