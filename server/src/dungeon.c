#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../header/dungeon.h"
#include "../header/game_state.h"

int quest_items_collected = 0;
pthread_mutex_t quest_items_mutex = PTHREAD_MUTEX_INITIALIZER;

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

    r->items = malloc(height * sizeof(Item*));
    if (!r->items) {
        free(r);
        return NULL;
    }

    for (int i = 0; i < height; i++) {
        r->items[i] = malloc(width * sizeof(Item));
        if (!r->items[i]) {
            for (int j = 0; j < i; j++) {
                free(r->items[j]);
            }
            free(r->items);
            free(r);
            return NULL;
        }
    }

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            r->items[y][x].x = x;
            r->items[y][x].y = y;
            r->items[y][x].type = ITEM_NONE;
            r->items[y][x].collected = true;
            r->items[y][x].symbol = ' ';
        }
    }

    r->monsters = malloc(height * sizeof(Monster*));
    if (!r->monsters) {
        for (int i = 0; i < height; i++) {
            free(r->items[i]);
        }
        free(r->items);
        free(r);
        return NULL;
    }

    for (int i = 0; i < height; i++) {
        r->monsters[i] = malloc(width * sizeof(Monster));
        if (!r->monsters[i]) {
            for (int j = 0; j < i; j++) {
                free(r->monsters[j]);
            }
            free(r->monsters);
            for (int j = 0; j < height; j++) {
                free(r->items[j]);
            }
            free(r->items);
            free(r);
            return NULL;
        }
    }

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            r->monsters[y][x].x = x;
            r->monsters[y][x].y = y;
            r->monsters[y][x].alive = false;
            r->monsters[y][x].symbol = ' ';
        }
    }

    return r;
}

void room_destroy(Room *r) {
    if (!r) return;

    for (int i = 0; i < r->height; i++) {
        free(r->items[i]);
    }
    free(r->items);

    for (int i = 0; i < r->height; i++) {
        free(r->monsters[i]);
    }
    free(r->monsters);

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
