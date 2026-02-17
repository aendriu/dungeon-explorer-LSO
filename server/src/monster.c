#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../header/dungeon.h"
#include "../header/monster.h"
#include "../header/game_state.h"

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

            int monster_rand = rand_r(seed) % 25;
            if (monster_rand == 0) {
                r->monsters[y][x].alive = true;
                r->monsters[y][x].symbol = MONSTER_SYMBOL;
            } else {
                r->monsters[y][x].alive = false;
                r->monsters[y][x].symbol = ' ';
            }
        }
    }

    printf("- Room %d monsters generated (%dx%d)\n", r->id, r->width, r->height);
}

Monster* room_get_monster(Room *r, int x, int y) {
    if (!r || x < 0 || x >= r->width || y < 0 || y >= r->height) {
        return NULL;
    }

    Monster *monster = &r->monsters[y][x];
    if (!monster->alive) {
        return NULL;
    }

    return monster;
}
