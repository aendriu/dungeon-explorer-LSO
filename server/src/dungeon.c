#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../header/dungeon.h"
#include "../header/game_state.h"

int quest_items_collected = 0;
pthread_mutex_t quest_items_mutex = PTHREAD_MUTEX_INITIALIZER;

/* ---------- Allocazione griglie per Room ---------- */

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
                                 .collected = true, .symbol = ' ' };
        }
    }
    return grid;
}

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

static void free_item_grid(Item **grid, int height) {
    if (!grid) return;
    for (int i = 0; i < height; i++) free(grid[i]);
    free(grid);
}

static void free_monster_grid(Monster ***grid, int width, int height) {
    if (!grid) return;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) free(grid[y][x]);
        free(grid[y]);
    }
    free(grid);
}

/* ---------- Room lifecycle ---------- */

Room* room_create(int id, int width, int height) {
    Room *r = malloc(sizeof(Room));
    if (!r) return NULL;

    r->id = id;
    r->width = width;
    r->height = height;
    r->entrance_x = 0;
    r->entrance_y = 0;
    r->exit_x = width - 1;
    r->exit_y = height - 1;

    r->items = alloc_item_grid(width, height);
    if (!r->items) { free(r); return NULL; }

    r->monsters = alloc_monster_grid(width, height);
    if (!r->monsters) { free_item_grid(r->items, height); free(r); return NULL; }

    return r;
}

void room_destroy(Room *r) {
    if (!r) return;
    free_item_grid(r->items, r->height);
    free_monster_grid(r->monsters, r->width, r->height);
    free(r);
}

void room_generate_items(Room *r) {
    if (!r) return;
    unsigned int *seed = get_rand_seed();

    for (int y = 0; y < r->height; y++) {
        for (int x = 0; x < r->width; x++) {
            int quest_rand = rand_r(seed) % 100;
            
            if (quest_rand == 0) {
                r->items[y][x].type = ITEM_QUEST;
                r->items[y][x].collected = false;
                r->items[y][x].symbol = ITEM_SYMBOL;
            } else {
                int trap_rand = rand_r(seed) % 50;
                if (trap_rand == 0) {
                    r->items[y][x].type = ITEM_TRAP;
                    r->items[y][x].collected = false;
                    r->items[y][x].symbol = TRAP_SYMBOL;
                } else {
                    int normal_rand = rand_r(seed) % 25;
                    if (normal_rand == 0) {
                        r->items[y][x].type = ITEM_NORMAL;
                        r->items[y][x].collected = false;
                        r->items[y][x].symbol = ITEM_SYMBOL;
                    } else {
                        r->items[y][x].type = ITEM_NONE;
                        r->items[y][x].collected = true;
                        r->items[y][x].symbol = ' ';
                    }
                }
            }
        }
    }

    printf("- Room %d items generated (%dx%d)\n", r->id, r->width, r->height);
}

void room_generate_monsters(Room *r) {
    if (!r) return;
    unsigned int *seed = get_rand_seed();

    int spawn_x = r->width / 2;
    int spawn_y = r->height / 2;

    for (int y = 0; y < r->height; y++) {
        for (int x = 0; x < r->width; x++) {
            if (x == spawn_x && y == spawn_y) {
                continue;
            }

            if (r->items[y][x].type != ITEM_NONE && !r->items[y][x].collected) {
                continue;
            }

            int monster_rand = rand_r(seed) % 50;
            if (r->monsters[y][x] != NULL) {
                free(r->monsters[y][x]);
                r->monsters[y][x] = NULL;
            }
            if (monster_rand == 0) {
                Monster *m = malloc(sizeof(Monster));
                if (!m) {
                    continue;
                }
                m->x = x;
                m->y = y;
                m->state = MONSTER_ALIVE;
                m->target_player_id = -1;
                m->failed_moves = 0;
                m->symbol = MONSTER_SYMBOL;
                r->monsters[y][x] = m;
            }
        }
    }

    printf("- Room %d monsters generated (%dx%d)\n", r->id, r->width, r->height);
}

Monster* room_get_monster(Room *r, int x, int y) {
    if (!r || x < 0 || x >= r->width || y < 0 || y >= r->height) {
        return NULL;
    }

    Monster *monster = r->monsters[y][x];
    if (monster == NULL || monster->state == MONSTER_DEAD) {
        return NULL;
    }

    return monster;
}

Item* room_get_item(Room *r, int x, int y) {
    if (!r || x < 0 || x >= r->width || y < 0 || y >= r->height) {
        return NULL;
    }

    Item *item = &r->items[y][x];
    if (item->type == ITEM_NONE || item->collected) {
        return NULL;
    }

    return item;
}

Dungeon* dungeon_create(int num_rooms, int room_width, int room_height) {
    Dungeon *d = malloc(sizeof(Dungeon));
    if (!d) return NULL;

    d->num_rooms = num_rooms;
    d->rooms = malloc(num_rooms * sizeof(Room*));
    if (!d->rooms) {
        free(d);
        return NULL;
    }

    for (int i = 0; i < num_rooms; i++) {
        d->rooms[i] = room_create(i, room_width, room_height);
        if (!d->rooms[i]) {
            for (int j = 0; j < i; j++) {
                room_destroy(d->rooms[j]);
            }
            free(d->rooms);
            free(d);
            return NULL;
        }
    }

    printf("- Dungeon created: %d rooms of %dx%d\n", num_rooms, room_width, room_height);
    return d;
}

void dungeon_destroy(Dungeon *d) {
    if (!d) return;

    for (int i = 0; i < d->num_rooms; i++) {
        room_destroy(d->rooms[i]);
    }
    free(d->rooms);
    free(d);
}

Room* dungeon_get_room(Dungeon *d, int room_id) {
    if (!d || room_id < 0 || room_id >= d->num_rooms) {
        return NULL;
    }

    return d->rooms[room_id];
}

char room_get_display_char(Room *r, int x, int y) {
    if (!r || x < 0 || x >= r->width || y < 0 || y >= r->height) {
        return '?';
    }

    if (x == r->entrance_x && y == r->entrance_y) {
        return DOOR_ENTRANCE;
    }
    if (x == r->exit_x && y == r->exit_y) {
        return DOOR_EXIT;
    }

    Item *item = &r->items[y][x];
    if (item->type == ITEM_NONE || item->collected) {
        return '.';
    }

    return ITEM_SYMBOL;
}

void room_print_map(Room *r) {
    if (!r) return;

    printf("\n=== ROOM %d MAP (%dx%d) ===\n", r->id, r->width, r->height);
    
    for (int y = 0; y < r->height; y++) {
        printf("|");
        for (int x = 0; x < r->width; x++) {
            printf("%c", room_get_display_char(r, x, y));
        }
        printf("|\n");
    }
    printf("\n");
}

bool item_collect(Dungeon *d, int player_id, int room_id, int x, int y) {
    Room *r = dungeon_get_room(d, room_id);
    if (!r) {
        return false;
    }

    Item *item = room_get_item(r, x, y);
    if (!item) {
        return false;
    }

    item->collected = true;
    item->symbol = '.';

    if (item->type == ITEM_NORMAL) {
        printf("[PLAYER %d] Collected normal item at room %d (%d, %d)\n", player_id, room_id, x, y);
        return false;
    } else if (item->type == ITEM_QUEST) {
        pthread_mutex_lock(&quest_items_mutex);
        quest_items_collected++;
        int total = quest_items_collected;
        pthread_mutex_unlock(&quest_items_mutex);

        printf("[PLAYER %d] Collected QUEST item at room %d (%d, %d)! Total: %d/3\n", player_id, room_id, x, y, total);

        if (total >= 3) {
            printf("[GAME] TEAM WON! All quest items collected!\n");
            return true;
        }
    } else if (item->type == ITEM_TRAP) {
        printf("[PLAYER %d] Triggered a TRAP at room %d (%d, %d)!\n", player_id, room_id, x, y);
        return false;
    }

    return false;
}
