#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../header/dungeon.h"
#include "../header/monster.h"
#include "../header/game_state.h"

// Elimina e libera la memoria di tutti i mostri nel dungeon
void dungeon_kill_all_monsters(Dungeon *d) {
    if (!d) return;

    // Passa per ogni stanza nel dungeon
    for (int i = 0; i < d->num_rooms; i++) {
        Room *r = d->rooms[i];
        if (!r) continue;
        // Elimina ogni mostro in ogni cella della stanza
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

    // Scorre ogni cella della stanza
    for (int y = 0; y < r->height; y++) {
        for (int x = 0; x < r->width; x++) {
            Monster *m = r->monsters[y][x];
            // Salta se non c'è mostro o è già morto
            if (m == NULL) continue;
            if (m->state == MONSTER_DEAD) continue;

            // Assegna un giocatore casuale come bersaglio
            int chosen = player_ids[rand_r(seed) % num_players];
            m->target_player_id = chosen;
            // Metti il mostro in inseguimento
            m->state = MONSTER_IN_PURSUIT;
        }
    }
}

/* Muove tutti i mostri che inseguono player_id di 1 passo verso (target_y, target_x).
   Se un mostro raggiunge il giocatore viene catturato.
   Se un mostro trova un item normale (non quest) davanti, piazza una trappola e si ferma.
   Il movimento ha probabilità: 1/3 (failed_moves=0), 2/3 (failed_moves=1), 3/3 (failed_moves=2).
   Usa un approccio a due passate per evitare di muovere un mostro più volte.
   Restituisce il numero di mostri catturati dal giocatore. */
int room_move_monsters_toward_player(Room *r, int player_id, int target_y, int target_x) {
    if (!r) return 0;
    int monster_kills = 0;

    unsigned int *seed = get_rand_seed();

    /* Prima passata: raccoglie i mostri che devono muoversi */
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

        /* Verifica che il mostro sia ancora nella posizione originale */
        if (r->monsters[oy][ox] != m) continue;

        /* Calcola la probabilità di movimento basata su failed_moves */
        /* failed_moves=0 → 1/3, failed_moves=1 → 2/3, failed_moves=2 → 3/3 (garantito) */
        int move_chance = m->failed_moves + 1;      // 1, 2, o 3
        int move_roll = (rand_r(seed) % 3) + 1;     // Risultato 1-3

        /* Se il movimento fallisce, incrementa failed_moves e salta */
        if (move_roll > move_chance) {
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
            r->monsters[oy][ox] = NULL;
            free(m);
            monster_kills++;
            continue;
        }

        /* Se la casella ha un item normale (non quest), piazza una trappola */
        Item *item = &r->items[new_y][new_x];
        if (item != NULL && item->type == ITEM_NORMAL && !item->collected) {
            item->type = ITEM_BOOBYTRAP;
            /* Il mostro non si muove questo turno (si ferma per piazzare la trappola) */
            continue;
        }

        /* Se la cella è libera, sposta il mostro */
        if (r->monsters[new_y][new_x] == NULL) {
            r->monsters[oy][ox] = NULL;
            r->monsters[new_y][new_x] = m;
            m->x = new_x;
            m->y = new_y;
        }
    }

    return monster_kills;
}
