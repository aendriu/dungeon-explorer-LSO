#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../header/dungeon.h"
#include "../header/game_state.h"

int quest_items_collected = 0;
pthread_mutex_t quest_items_mutex = PTHREAD_MUTEX_INITIALIZER;

/* ---------- Allocazione griglie per Room ---------- */

/* Alloca e inizializza la griglia 2D degli item di una stanza. */
static Item **alloc_item_grid(int width, int height) {
    Item **grid = malloc(height * sizeof(Item *));
    if (!grid) return NULL;

    for (int y = 0; y < height; y++) {
        grid[y] = malloc(width * sizeof(Item));
        if (!grid[y]) {
            for (int j = 0; j < y; j++) free(grid[j]);
            free(grid);
            return NULL;
        }
        for (int x = 0; x < width; x++) {
            grid[y][x] = (Item){ .x = x, .y = y, .type = ITEM_NONE,
                                 .collected = true };
        }
    }
    return grid;
}

/* Alloca la griglia 2D di puntatori a mostri, inizialmente tutti NULL. */
static Monster ***alloc_monster_grid(int width, int height) {
    Monster ***grid = malloc(height * sizeof(Monster **));
    if (!grid) return NULL;

    for (int y = 0; y < height; y++) {
        grid[y] = calloc(width, sizeof(Monster *));
        if (!grid[y]) {
            for (int j = 0; j < y; j++) free(grid[j]);
            free(grid);
            return NULL;
        }
    }
    return grid;
}

/* Libera la griglia item allocata con alloc_item_grid. */
static void free_item_grid(Item **grid, int height) {
    if (!grid) return;
    for (int i = 0; i < height; i++) free(grid[i]);
    free(grid);
}

/* Libera mostri e struttura della griglia allocata con alloc_monster_grid. */
static void free_monster_grid(Monster ***grid, int width, int height) {
    if (!grid) return;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) free(grid[y][x]);
        free(grid[y]);
    }
    free(grid);
}

/* ---------- Room lifecycle ---------- */

/* Crea una stanza con dimensioni date e relative griglie item/mostri. */
Room* room_create(int id, int width, int height) {
    Room *r = malloc(sizeof(Room));
    if (!r) return NULL;

    r->id = id;
    r->width = width;
    r->height = height;

    r->items = alloc_item_grid(width, height);
    if (!r->items) { free(r); return NULL; }

    r->monsters = alloc_monster_grid(width, height);
    if (!r->monsters) { free_item_grid(r->items, height); free(r); return NULL; }

    return r;
}

/* Distrugge una stanza e tutte le sue risorse dinamiche interne. */
void room_destroy(Room *r) {
    if (!r) return;
    free_item_grid(r->items, r->height);
    free_monster_grid(r->monsters, r->width, r->height);
    free(r);
}

// Genera un tipo di item casuale con probabilita' diverse
static ItemType roll_item_type(unsigned int *seed) {
    if (rand_r(seed) % 100 == 0) return ITEM_QUEST;
    if (rand_r(seed) % 75  == 0) return ITEM_APPLE;
    if (rand_r(seed) % 50  == 0) return ITEM_TRAP;
    if (rand_r(seed) % 25  == 0) return ITEM_NORMAL;
    return ITEM_NONE;
}

/* Applica tipo e stato di raccolta all'item di una specifica cella. */
static void place_item(Item *item, ItemType type) {
    switch (type) {
    case ITEM_QUEST:
    case ITEM_APPLE:
    case ITEM_TRAP:
    case ITEM_NORMAL:
        item->type      = type;
        item->collected  = false;
        break;
    default:
        item->type      = ITEM_NONE;
        item->collected  = true;
        break;
    }
}

/* Popola tutta la stanza con item casuali. */
static void room_generate_items(Room *r) {
    if (!r) return;
    unsigned int *seed = get_rand_seed();

    for (int y = 0; y < r->height; y++)
        for (int x = 0; x < r->width; x++)
            place_item(&r->items[y][x], roll_item_type(seed));

    printf("- Room %d items generated (%dx%d)\n", r->id, r->width, r->height);
}

// 2% di probabilita' di spawnare un mostro nella cella
static void place_monster(Monster **cell, int x, int y, unsigned int *seed) {
    if (*cell != NULL) {
        free(*cell);
        *cell = NULL;
    }
    if (rand_r(seed) % 50 != 0) return;

    Monster *m = malloc(sizeof(Monster));
    if (!m) return;
    *m = (Monster){ .x = x, .y = y, .state = MONSTER_ALIVE,
                    .target_player_id = -1, .failed_moves = 0 };
    *cell = m;
}

// Genera mostri nella stanza, evitando lo spawn e gli item
static void room_generate_monsters(Room *r) {
    unsigned int *seed = get_rand_seed();
    int spawn_x = r->width / 2;
    int spawn_y = r->height / 2;

    for (int y = 0; y < r->height; y++)
        for (int x = 0; x < r->width; x++) {
            if (x == spawn_x && y == spawn_y) continue;
            if (r->items[y][x].type != ITEM_NONE && !r->items[y][x].collected) continue;
            place_monster(&r->monsters[y][x], x, y, seed);
        }

    printf("- Room %d monsters generated (%dx%d)\n", r->id, r->width, r->height);
}

/* Esegue la popolazione completa della stanza (item + mostri). */
static void room_populate(Room *r) {
    if (!r) return;
    room_generate_items(r);
    room_generate_monsters(r);
}

// Ritorna il mostro vivo nella cella, o NULL
Monster* room_get_monster(Room *r, int x, int y) {
    if (!r || x < 0 || x >= r->width || y < 0 || y >= r->height)
        return NULL;

    Monster *monster = r->monsters[y][x];
    if (monster == NULL || monster->state == MONSTER_DEAD)
        return NULL;

    return monster;
}

// Ritorna l'item non raccolto nella cella, o NULL
Item* room_get_item(Room *r, int x, int y) {
    if (!r || x < 0 || x >= r->width || y < 0 || y >= r->height)
        return NULL;

    Item *item = &r->items[y][x];
    if (item->type == ITEM_NONE || item->collected)
        return NULL;

    return item;
}

// Crea il dungeon con stanze di dimensioni casuali
Dungeon* dungeon_create(int num_rooms) {
    Dungeon *d = malloc(sizeof(Dungeon));
    if (!d) return NULL;

    d->num_rooms = num_rooms;
    d->rooms = malloc(num_rooms * sizeof(Room*));
    if (!d->rooms) {
        free(d);
        return NULL;
    }

    unsigned int *seed = get_rand_seed();

    for (int i = 0; i < num_rooms; i++) {
        int rh = ROOM_H_MIN + rand_r(seed) % (ROOM_H_MAX - ROOM_H_MIN + 1);
        int rw = ROOM_W_MIN + rand_r(seed) % (ROOM_W_MAX - ROOM_W_MIN + 1);
        d->rooms[i] = room_create(i, rw, rh);
        if (!d->rooms[i]) {
            for (int j = 0; j < i; j++) {
                room_destroy(d->rooms[j]);
            }
            free(d->rooms);
            free(d);
            return NULL;
        }
        room_populate(d->rooms[i]);
        printf("- Room %d: %dx%d\n", i, rw, rh);
    }

    printf("- Dungeon created: %d rooms\n", num_rooms);
    return d;
}

/* Libera tutte le stanze del dungeon e la struttura contenitore. */
void dungeon_destroy(Dungeon *d) {
    if (!d) return;
    for (int i = 0; i < d->num_rooms; i++)
        room_destroy(d->rooms[i]);
    free(d->rooms);
    free(d);  
}

/* Ritorna il puntatore alla stanza richiesta, oppure NULL se invalida. */
Room* dungeon_get_room(Dungeon *d, int room_id) {
    if (!d || room_id < 0 || room_id >= d->num_rooms)
        return NULL;
    return d->rooms[room_id];
}


