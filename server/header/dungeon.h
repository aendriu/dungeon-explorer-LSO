#ifndef SERVER_DUNGEON_H
#define SERVER_DUNGEON_H

#include <stdbool.h>
#include <pthread.h>
#include "../../utils/enums.h"

#define ITEM_SYMBOL '?'
#define TRAP_SYMBOL '?'
#define MONSTER_SYMBOL 'M'
#define DOOR_ENTRANCE 'E'
#define DOOR_EXIT 'U'

typedef enum {
    ITEM_NONE,
    ITEM_NORMAL,
    ITEM_QUEST,
    ITEM_TRAP
} ItemType;

typedef struct {
    int x, y;
    ItemType type;
    bool collected;
    char symbol;
} Item;

typedef struct {
    int x, y;
    MonsterState state;
    char symbol;
} Monster;

// Room è una stanza del dungeon
typedef struct {
    int id;
    int width;
    int height;
    Item **items;      // matrice 2D di items nella stanza
    Monster ***monsters; // matrice 2D di puntatori ai mostri nella stanza
    int entrance_x, entrance_y;  // Coordinata porta di entrata
    int exit_x, exit_y;           // Coordinata porta di uscita
} Room;

// Dungeon contiene più stanze
typedef struct {
    int num_rooms;
    Room **rooms;  // array di stanze
} Dungeon;

extern int quest_items_collected;
extern pthread_mutex_t quest_items_mutex;

Item* room_get_item(Room *r, int x, int y);
void room_generate_items(Room *r);
char room_get_display_char(Room *r, int x, int y);

Dungeon* dungeon_create(int num_rooms, int room_width, int room_height);
void dungeon_destroy(Dungeon *d);
Room* dungeon_get_room(Dungeon *d, int room_id);
bool item_collect(Dungeon *d, int player_id, int room_id, int x, int y);

void room_print_map(Room *r);

#endif

