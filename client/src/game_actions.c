#include "../header/game_actions.h"
#include "../header/protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ---- private helpers ---- */

static void sync_pos(const GameState *s, int *py, int *px) {
  if (s->pos_y && s->pos_x && s->player_id >= 0 &&
      s->player_id < MAX_PLAYERS) {
    int sy = s->pos_y[s->player_id];
    int sx = s->pos_x[s->player_id];
    if (sy >= 1 && sy < s->room_h - 1 && sx >= 1 && sx < s->room_w - 1) {
      *py = sy;
      *px = sx;
    }
  }
}

static void sync_room(GameState *s) {
  if (s->players && s->player_id >= 0 && s->player_id < MAX_PLAYERS)
    s->current_room = s->players[s->player_id];
}

static int apply_reply(GameState *state, char *reply, char *st, size_t slen,
                       int *py, int *px) {
  GameState u = {0};
  char pe[64] = {0};
  if (parse_game_state(reply, &u, pe, sizeof(pe)) < 0) {
    snprintf(st, slen, "Errore parsing risposta");
    free(reply);
    return -1;
  }
  free(reply);
  free_game_state(state);
  *state = u;
  sync_room(state);
  sync_pos(state, py, px);

  if (pe[0] == '\0')
    return 0;

  if (strcmp(pe, MSG_BOOBY_TRAP) == 0)
    snprintf(st, slen, "Un mostro aveva piazzato una trappola qui!");
  else if (strcmp(pe, MSG_APPLE_HEAL) == 0)
    snprintf(st, slen, "Hai trovato una mela! +1 vita");
  else if (strcmp(pe, MSG_TEAM_WON) == 0 ||
           strcmp(pe, MSG_TEAM_DEFEATED) == 0)
    snprintf(st, slen, "%s", pe);
  else {
    snprintf(st, slen, "Errore: %s", pe);
    return -1;
  }
  return 0;
}

/* ---- public: game loop ---- */

int action_poll_state(GameState *s, char *st, size_t slen, int *py, int *px) {
  char *r = sendnrecv(GET_GAME_STATE);
  if (!r) {
    snprintf(st, slen, "Server non risponde");
    return -1;
  }
  return apply_reply(s, r, st, slen, py, px);
}

int action_move_to_room(GameState *s, int target, char *st, size_t slen,
                        int *py, int *px) {
  char buf[64];
  snprintf(buf, sizeof(buf), "{\"target\":%d}", target);
  char *r = sendnrecv_payload(MOVE_PLAYER, buf);
  if (!r) {
    snprintf(st, slen, "Server non risponde");
    return -1;
  }
  *py = s->room_h / 2;
  *px = s->room_w / 2;
  return apply_reply(s, r, st, slen, py, px);
}

int action_update_pos(GameState *s, int y, int x, char *st, size_t slen,
                      int *py, int *px) {
  char buf[64];
  snprintf(buf, sizeof(buf), "{\"y\":%d,\"x\":%d}", y, x);
  char *r = sendnrecv_payload(UPDATE_PLAYER_POSITION, buf);
  if (!r) {
    snprintf(st, slen, "Server non risponde");
    return -1;
  }
  return apply_reply(s, r, st, slen, py, px);
}

/* ---- public: lobby ---- */

int action_poll_lobby(char *err, size_t elen) {
  char *reply = sendnrecv(GET_N_OF_CONNECTED_PLAYERS);
  if (!reply) {
    snprintf(err, elen, "Connessione persa o server non risponde.");
    return -1;
  }
  char *end;
  long n = strtol(reply, &end, 10);
  if (end == reply || n < 0 || n > MAX_PLAYERS) {
    snprintf(err, elen, "%.*s", (int)(elen > 1 ? elen - 1 : 0), reply);
    free(reply);
    return -2;
  }
  free(reply);
  return (int)n;
}

int action_start_game(GameState *out, char *err, size_t elen) {
  char *reply = sendnrecv(START_GAME);
  if (!reply) {
    snprintf(err, elen, "Server non risponde all'avvio partita.");
    return -1;
  }
  char pe[64] = {0};
  if (parse_game_state(reply, out, pe, sizeof(pe)) < 0) {
    snprintf(err, elen, "Errore nel parsing dello stato di gioco.");
    free(reply);
    return -1;
  }
  if (pe[0] != '\0') {
    snprintf(err, elen, "Errore avvio partita: %s", pe);
    free(reply);
    free_game_state(out);
    memset(out, 0, sizeof(*out));
    return -1;
  }
  free(reply);
  sync_room(out);
  return 0;
}

/* ---- poller thread ---- */

/* Thread: ogni ~500ms chiede GET_GAME_STATE e aggiorna lo stato condiviso */
static void *poller_fn(void *arg) {
  Poller *p = (Poller *)arg;
  while (p->running) {
    /* ~500ms di sleep, ma controlla running ogni 20ms */
    for (int i = 0; i < 25 && p->running; i++)
      usleep(20000);
    if (!p->running)
      break;

    char *r = sendnrecv(GET_GAME_STATE);
    if (!r)
      continue;

    GameState tmp = {0};
    if (parse_game_state(r, &tmp, NULL, 0) < 0) {
      free(r);
      continue;
    }
    free(r);
    sync_room(&tmp);

    /* Swap dello stato condiviso sotto mutex */
    pthread_mutex_lock(&p->mutex);
    int py = p->py, px = p->px;
    sync_pos(&tmp, &py, &px);
    free_game_state(&p->state);
    p->state = tmp;
    p->py = py;
    p->px = px;
    pthread_mutex_unlock(&p->mutex);
  }
  return NULL;
}

/* Avvia il poller, trasferisce ownership dello stato iniziale */
void poller_start(Poller *p, GameState *initial, int py, int px) {
  memset(p, 0, sizeof(*p));
  pthread_mutex_init(&p->mutex, NULL);
  p->state = *initial;
  memset(initial, 0, sizeof(*initial));
  p->py = py;
  p->px = px;
  p->running = 1;
  pthread_create(&p->tid, NULL, poller_fn, p);
}

/* Ferma il poller e restituisce lo stato finale */
void poller_stop(Poller *p, GameState *out) {
  p->running = 0;
  pthread_join(p->tid, NULL);
  *out = p->state;
  memset(&p->state, 0, sizeof(p->state));
  pthread_mutex_destroy(&p->mutex);
}
