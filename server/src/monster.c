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

void dungeon_kill_all_monsters(Dungeon *d) {
    if (!d) return;

    for (int i = 0; i < d->num_rooms; i++) {
        Room *r = d->rooms[i];
        if (!r) continue;
        for (int y = 0; y < r->height; y++) {
            for (int x = 0; x < r->width; x++) {
                free(r->monsters[y][x]);
                r->monsters[y][x] = NULL;
            }
        }
    }
}

/* Assegna a ogni mostro vivo nella stanza un giocatore casuale da inseguire.
   player_ids: array di player_id presenti nella stanza, num_players: quanti sono. */
void room_assign_monsters_to_players(Room *r, int *player_ids, int num_players) {
    if (!r || !player_ids || num_players <= 0) return;

    unsigned int *seed = get_rand_seed();

    for (int y = 0; y < r->height; y++) {
        for (int x = 0; x < r->width; x++) {
            Monster *m = r->monsters[y][x];
            if (m == NULL) continue;
            if (m->state == MONSTER_DEAD) continue;

            int chosen = player_ids[rand_r(seed) % num_players];
            m->target_player_id = chosen;
            m->state = MONSTER_IN_PURSUIT;
        }
    }
}

/* Muove tutti i mostri che inseguono player_id di 1 passo verso (target_y, target_x).
   Se un mostro raggiunge il giocatore viene catturato.
   Se un mostro trova un item normale (non quest) davanti, piazza una trappola e si ferma.
   Il movimento ha probabilità: 1/3 (failed_moves=0), 2/3 (failed_moves=1), 3/3 (failed_moves=2).
   Restituisce il numero di mostri catturati dal giocatore. */
int room_move_monsters_toward_player(Room *r, int player_id, int target_y, int target_x) {
    if (!r) return 0;
    int monster_kills = 0;

    unsigned int *seed = get_rand_seed();

    for (int y = 0; y < r->height; y++) {
        for (int x = 0; x < r->width; x++) {
            Monster *m = r->monsters[y][x];
            if (m == NULL || m->state != MONSTER_IN_PURSUIT) continue;
            if (m->target_player_id != player_id) continue;

            /* Calcola la probabilità di movimento basata su failed_moves */
            int probability_denom = 3 - m->failed_moves;  // 3, 2, o 1
            int move_roll = (rand_r(seed) % 3) + 1;       // Risultato 1-3

            /* Se il movimento fallisce, incrementa failed_moves e salta */
            if (move_roll > probability_denom) {
                m->failed_moves++;
                if (m->failed_moves > 2) m->failed_moves = 2;
                continue;
            }

            /* Il movimento ha successo, resetta il contatore */
            m->failed_moves = 0;

            int new_x = m->x;
            int new_y = m->y;

            if (new_x < target_x) new_x++;
            else if (new_x > target_x) new_x--;

            if (new_y < target_y) new_y++;
            else if (new_y > target_y) new_y--;

            /* Se ha raggiunto il giocatore, cattura */
            if (new_x == target_x && new_y == target_y) {
                r->monsters[y][x] = NULL;
                free(m);
                monster_kills++;
                continue;
            }

            /* Se la casella ha un item normale (non quest), piazza una trappola */
            Item *item = &r->items[new_y][new_x];
            if (item != NULL && item->type == ITEM_NORMAL && !item->collected) {
                /* Trasforma l'item in trappola */
                item->type = ITEM_BOOBYTRAP;
                /* Il mostro non si muove questo turno (si ferma per piazzare la trappola) */
                continue;
            }

            /* Se la cella è libera, sposta il mostro */
            if (r->monsters[new_y][new_x] == NULL) {
                r->monsters[y][x] = NULL;
                r->monsters[new_y][new_x] = m;
                m->x = new_x;
                m->y = new_y;
            }
        }
    }

    return monster_kills;
}
