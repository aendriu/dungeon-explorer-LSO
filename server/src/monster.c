#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../header/dungeon.h"
#include "../header/monster.h"

void room_generate_monsters(Room *r) {
    if (!r) return;

    // Posizione di spawn iniziale dei player (quella che deve rimanere libera)
    int spawn_x = r->width / 2;
    int spawn_y = r->height / 2;

    // Per ogni cella:
    // 1. Se c'è un item (probabilità combinata di item/quest/trap), salta
    // 2. Altrimenti tenta mostro (1/25)
    for (int y = 0; y < r->height; y++) {
        for (int x = 0; x < r->width; x++) {
            // Proteggi la posizione di spawn iniziale dai mostri
            if (x == spawn_x && y == spawn_y) {
                continue;
            }

            // Se la cella contiene un item, non mettere mostro
            if (r->items[y][x].type != ITEM_NONE && !r->items[y][x].collected) {
                continue;
            }

            // Tenta di spawnare un mostro (probabilità 1/25)
            int monster_rand = rand() % 25;
            if (monster_rand == 0) {
                // Mostro!
                r->monsters[y][x].alive = true;
                r->monsters[y][x].symbol = MONSTER_SYMBOL;
            } else {
                // Cella vuota (niente mostro)
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
