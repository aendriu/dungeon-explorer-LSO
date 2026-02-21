#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../header/dungeon.h"
#include "../header/monster.h"
#include "../header/game_state.h"

// Elimina tutti i mostri dal dungeon
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

/* Assegna a ogni mostro vivo nella stanza un giocatore casuale da inseguire */
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

/* Muove i mostri che inseguono player_id di 1 passo verso il giocatore.
   Probabilita' di movimento: 1/3 -> 2/3 -> 3/3 (basato su failed_moves).
   Se idle=true il player e' fermo da 3s, i mostri ottengono +1.
   Se un mostro raggiunge il player, viene catturato.
   Se trova un item normale, ci piazza una trappola.
   Ritorna il numero di catture. */
int room_move_monsters_toward_player(Room *r, int player_id, int target_y, int target_x, bool idle) {
    if (!r) return 0;
    int monster_kills = 0;

    unsigned int *seed = get_rand_seed();

    /* Prima passata: raccoglie i mostri da muovere */
    typedef struct { int oy, ox; Monster *m; } PendingMove;
    PendingMove pending[r->height * r->width];
    int n = 0;

    for (int y = 0; y < r->height; y++) {
        for (int x = 0; x < r->width; x++) {
            Monster *m = r->monsters[y][x];
            if (m == NULL || m->state != MONSTER_IN_PURSUIT) continue;
            if (m->target_player_id != player_id) continue;
            pending[n++] = (PendingMove){y, x, m};
        }
    }

    /* Seconda passata: applica i movimenti */
    for (int i = 0; i < n; i++) {
        Monster *m = pending[i].m;
        int oy = pending[i].oy;
        int ox = pending[i].ox;

        if (r->monsters[oy][ox] != m) continue;

        /* Probabilita' crescente; +1 se il player e' fermo da 3s */
        int move_chance = m->failed_moves + 1 + (idle ? 1 : 0);
        if (move_chance > 3) move_chance = 3;
        int move_roll = (rand_r(seed) % 3) + 1;

        /* Movimento fallito, riprova piu' forte la prossima volta */
        if (move_roll > move_chance) {
            m->failed_moves++;
            if (m->failed_moves > 2) m->failed_moves = 2;
            continue;
        }

        m->failed_moves = 0;

        int new_x = m->x;
        int new_y = m->y;

        if (new_x < target_x) new_x++;
        else if (new_x > target_x) new_x--;

        if (new_y < target_y) new_y++;
        else if (new_y > target_y) new_y--;

        /* Cattura: il mostro ha raggiunto il giocatore */
        if (new_x == target_x && new_y == target_y) {
            r->monsters[oy][ox] = NULL;
            free(m);
            monster_kills++;
            continue;
        }

        /* Se c'e' un item normale, il mostro ci piazza una trappola */
        Item *item = &r->items[new_y][new_x];
        if (item != NULL && item->type == ITEM_NORMAL && !item->collected) {
            item->type = ITEM_BOOBYTRAP;
            continue;
        }

        /* Sposta il mostro se la cella e' libera */
        if (r->monsters[new_y][new_x] == NULL) {
            r->monsters[oy][ox] = NULL;
            r->monsters[new_y][new_x] = m;
            m->x = new_x;
            m->y = new_y;
        }
    }

    return monster_kills;
}
