#ifndef CLIENT_GAME_ACTIONS_H
#define CLIENT_GAME_ACTIONS_H

#include "../header/game_state.h"
#include <pthread.h>
#include <stddef.h>

/*
 * Poller: thread in background che aggiorna lo stato di gioco ogni ~500ms.
 * Lo stato condiviso è protetto da mutex.
 */
typedef struct {
    GameState       state;      /* stato di gioco condiviso */
    pthread_mutex_t mutex;      /* protegge accesso allo stato */
    char            status[128]; /* ultimo messaggio evento */
    int             py, px;      /* posizione corrente del giocatore */
    volatile int    running;     /* flag di arresto per il thread */
    pthread_t       tid;         /* id del thread di polling */
} Poller;

/* Avvia il poller: prende ownership di *initial (lo azzera) */
void poller_start(Poller *p, GameState *initial, int py, int px);

/* Ferma il poller e restituisce lo stato finale in *out */
void poller_stop(Poller *p, GameState *out);

/*
 * Game-loop actions: return 0 on success, -1 on error.
 * On return, status contains an informational or error message.
 */
int action_poll_state(GameState *state, char *status, size_t slen,
                      int *py, int *px);
int action_move_to_room(GameState *state, int target, char *status,
                        size_t slen, int *py, int *px);
int action_update_pos(GameState *state, int y, int x, char *status,
                      size_t slen, int *py, int *px);

/*
 * Lobby actions.
 * action_poll_lobby: returns player count >= 0, -1 (conn lost), -2 (parse err).
 * action_start_game: returns 0 on success (out filled), -1 on error.
 * Both write a description into err on failure.
 */
int action_poll_lobby(char *err, size_t elen);
int action_start_game(GameState *out, char *err, size_t elen);

#endif
