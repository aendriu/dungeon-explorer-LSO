#ifndef DUNGEON
#define DUNGEON

#include <stdbool.h>
#include <pthread.h>

#define ITEM_SYMBOL '?'
#define DOOR_ENTRANCE 'E'
#define DOOR_EXIT 'U'
#define PLAYER_1_SYMBOL '@'
#define PLAYER_2_SYMBOL '#'
#define PLAYER_3_SYMBOL '$'
#define PLAYER_4_SYMBOL '%'

typedef enum {
    ITEM_NONE,
    ITEM_NORMAL,
    ITEM_QUEST
} ItemType;

typedef struct {
    int x, y;
    ItemType type;
    bool collected;
    char symbol;  // Display symbol (? per gli items)
} Item;

// Room è una stanza del dungeon
typedef struct {
    int id;
    int width;
    int height;
    Item **items;  // matrice 2D di items nella stanza
    int entrance_x, entrance_y;  // Coordinata porta di entrata
    int exit_x, exit_y;           // Coordinata porta di uscita
} Room;

// Dungeon contiene più stanze
typedef struct {
    int num_rooms;
    Room **rooms;  // array di stanze
} Dungeon;

// Global quest items collected (shared among all players)
extern int quest_items_collected;
extern pthread_mutex_t quest_items_mutex;

// Room functions
Item* room_get_item(Room *r, int x, int y);
void room_generate_items(Room *r);
char room_get_display_char(Room *r, int x, int y);

// Dungeon functions
Dungeon* dungeon_create(int num_rooms, int room_width, int room_height);
void dungeon_destroy(Dungeon *d);
Room* dungeon_get_room(Dungeon *d, int room_id);
bool item_collect(Dungeon *d, int player_id, int room_id, int x, int y);

// Map display
void room_print_map(Room *r);

#endif


