#ifndef CLIENT_GAME_ACTIONS_H
#define CLIENT_GAME_ACTIONS_H

#include "../header/game_state.h"
#include <pthread.h>
#include <stddef.h>

/* Thread di polling: aggiorna lo stato di gioco ogni ~500ms */
typedef struct {
    GameState       state;
    pthread_mutex_t mutex;
    int             py, px;
    volatile int    running;
    pthread_t       tid;
} Poller;

void poller_start(Poller *p, GameState *initial, int py, int px);
void poller_stop(Poller *p, GameState *out);

/* Azioni del game loop: 0 = ok, -1 = errore */
int action_poll_state(GameState *state, char *status, size_t slen,
                      int *py, int *px);
int action_move_to_room(GameState *state, int target, char *status,
                        size_t slen, int *py, int *px);
int action_update_pos(GameState *state, int y, int x, char *status,
                      size_t slen, int *py, int *px);

/* Azioni della lobby */
int action_poll_lobby(char *err, size_t elen);
int action_start_game(GameState *out, char *err, size_t elen);

#endif
