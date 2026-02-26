#include "../header/protocol.h"
#include "../header/player.h"
#include "../header/monster.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* ========== Parsing della richiesta client ========== */

/*
 * Estrae dal payload JSON i campi opzionali:
 * - target stanza per MOVE_PLAYER
 * - coordinate y/x per UPDATE_PLAYER_POSITION
 */
static void parse_payload(cJSON *payload, ClientRequest *req) {
    cJSON *target = cJSON_GetObjectItemCaseSensitive(payload, "target");
    if (cJSON_IsNumber(target)) {
        req->target_room = target->valueint;
        req->has_target  = true;
    }

    cJSON *py = cJSON_GetObjectItemCaseSensitive(payload, "y");
    cJSON *px = cJSON_GetObjectItemCaseSensitive(payload, "x");
    if (cJSON_IsNumber(py) && cJSON_IsNumber(px)) {
        req->pos_y   = py->valueint;
        req->pos_x   = px->valueint;
        req->has_pos = true;
    }
}

/*
 * Parsa una richiesta JSON nella forma {"cmd":..., "payload":{...}}.
 * Ignora campi mancanti e lascia i default gia' inizializzati in req.
 */
static void parse_json_request(const char *msg, ClientRequest *req) {
    cJSON *root = cJSON_Parse(msg);
    if (root == NULL) return;
//il comando è il messaggio nel JSON
    cJSON *cmd = cJSON_GetObjectItemCaseSensitive(root, "cmd");
    if (cJSON_IsNumber(cmd))
        req->cmd = (Message)cmd->valueint;

    cJSON *payload = cJSON_GetObjectItemCaseSensitive(root, "payload");
    if (cJSON_IsObject(payload))
        parse_payload(payload, req);

    cJSON_Delete(root);
}

/*
 * Entry-point di parsing: accetta sia JSON sia comandi numerici "grezzi".
 * Ritorna sempre una struct valida con default sensati.
 */
ClientRequest parse_client_request(const char *msg) {
    ClientRequest req = {
        .cmd         = GET_N_OF_CONNECTED_PLAYERS,
        .target_room = -1,
        .has_target  = false,
        .pos_y       = -1,
        .pos_x       = -1,
        .has_pos     = false
    };

    if (msg == NULL) return req;

    if (*msg == '{')
        parse_json_request(msg, &req);
    else {
        char *endptr;
        long val = strtol(msg, &endptr, 10);
        if (endptr != msg && val >= 0)
            req.cmd = (Message)val;
    }
    return req;
}

/* ========== Utility di risposta ========== */

/*
 * Copia il JSON di risposta nel buffer di output.
 * Gestisce null/overflow e libera la stringa allocata da cJSON.
 */
static int copy_json_response(char *response, size_t res_len, char *json) {
    if (json == NULL) {
        snprintf(response, res_len, "{\"error\":\"server_error\"}");
        return -1;
    }
    if (strlen(json) >= res_len) {
        snprintf(response, res_len, "{\"error\":\"response_too_large\"}");
        cJSON_free(json);
        return -1;
    }
    strncpy(response, json, res_len - 1);
    response[res_len - 1] = '\0';
    cJSON_free(json);
    return 0;
}

/* ========== Handler dei comandi ========== */

/*
 * Assegna ai mostri della stanza un target casuale tra i player presenti.
 * Viene invocato quando un player entra in una stanza.
 */
static void assign_room_monsters(Room *r, int room_id) {
    if (!r) return;
    int pids[MAX_PLAYERS];
    int np = 0;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (game_state.positions[i] == room_id)
            pids[np++] = i;
    }
    if (np > 0)
        room_assign_monsters_to_players(r, pids, np);
}

/* Ritorna true se pid e' dentro i limiti consentiti. */
static bool valid_pid(int pid) {
    return pid >= 0 && pid < MAX_PLAYERS;
}

/* Handler per GET_N_OF_CONNECTED_PLAYERS: risposta solo numerica. */
static void handle_get_connected_players(char *response, size_t res_len) {
    snprintf(response, res_len, "%d", get_connected_players());
}

/*
 * Handler START_GAME: inizializza la partita se serve e spawna il player.
 * Restituisce lo stato completo al client.
 */
static void handle_start_game(Conn *conn, char *response, size_t res_len) {
    init_game_state_if_needed();
    int pid = conn->player_id;
    pthread_mutex_lock(&game_state.mutex);
    /* Se il player non ha ancora posizione, assegna la stanza iniziale. */
    if (valid_pid(pid) && game_state.positions[pid] < 0) {
        game_state.positions[pid] = 0;
        Room *spawn_room = dungeon_get_room(game_dungeon, 0);
        game_state.pos_y[pid]     = spawn_room ? spawn_room->height / 2 : 0;
        game_state.pos_x[pid]     = spawn_room ? spawn_room->width  / 2 : 0;

        if (game_dungeon != NULL)
            assign_room_monsters(dungeon_get_room(game_dungeon, 0), 0);
    }
    char *json = build_game_state_json(pid, NULL);
    pthread_mutex_unlock(&game_state.mutex);
    copy_json_response(response, res_len, json);
}

/*
 * Handler GET_GAME_STATE: aggiorna eventuali movimenti mostri e ritorna JSON.
 * Se il player e' idle, i mostri della sua stanza avanzano.
 */
static void handle_get_game_state(Conn *conn, char *response, size_t res_len) {
    const char *err = NULL;
    pthread_mutex_lock(&game_state.mutex);

    if (!game_state.started) {
        pthread_mutex_unlock(&game_state.mutex);
        snprintf(response, res_len, "{\"error\":\"game_not_started\"}");
        return;
    }

    int pid = conn->player_id;

    /* Controlla se la partita e' gia' finita */
    pthread_mutex_lock(&team.lives_mutex);
    int lives = team.shared_lives;
    pthread_mutex_unlock(&team.lives_mutex);
    if (lives <= 0) {
        err = MSG_TEAM_DEFEATED;
    } else {
        pthread_mutex_lock(&quest_items_mutex);
        if (quest_items_collected >= QUEST_ITEMS_TO_WIN)
            err = MSG_TEAM_WON;
        pthread_mutex_unlock(&quest_items_mutex);
    }

    /*
     * Idle mechanism: solo se la partita e' ancora in corso.
     * Se il player e' fermo da IDLE_THRESHOLD secondi, i mostri avanzano.
     */
    if (err == NULL && valid_pid(pid) && idle_countdown[pid] == 0 &&
        game_state.positions[pid] >= 0 && game_dungeon != NULL) {
        int rid = game_state.positions[pid];
        Room *r = dungeon_get_room(game_dungeon, rid);
        if (r != NULL) {
            int hits = room_move_monsters_toward_player(
                r, pid, game_state.pos_y[pid], game_state.pos_x[pid], true);
            /* Ogni hit corrisponde a una vita persa dal team. */
            for (int h = 0; h < hits; h++)
                player_lose_life();

            /* Controlla fine partita */
            pthread_mutex_lock(&team.lives_mutex);
            int lives = team.shared_lives;
            pthread_mutex_unlock(&team.lives_mutex);
            if (lives <= 0) {
                dungeon_kill_all_monsters(game_dungeon);
                err = MSG_TEAM_DEFEATED;
            }
        }
    }

    char *json = build_game_state_json(pid, err);
    pthread_mutex_unlock(&game_state.mutex);
    copy_json_response(response, res_len, json);
}

/*
 * Applica gli effetti della casella (x,y):
 * - raccolta item
 * - uccisione mostri presenti
 * - movimento mostri verso il player
 * Ritorna un eventuale messaggio di evento per il client.
 */
static const char *process_tile(int pid, Room *r, int x, int y) {
    const char *err = NULL;
    bool team_won = false;

    /* Raccolta item */
    Item *item = room_get_item(r, x, y);
    if (item) {
        ItemType t = item->type;
        item->collected = true;

        if (t == ITEM_TRAP || t == ITEM_BOOBYTRAP) {
            players[pid].traps_triggered++;
            player_lose_life();
            if (t == ITEM_BOOBYTRAP)
                err = MSG_BOOBY_TRAP;
        } else if (t == ITEM_APPLE) {
            pthread_mutex_lock(&team.lives_mutex);
            team.shared_lives++;
            pthread_mutex_unlock(&team.lives_mutex);
            err = MSG_APPLE_HEAL;
        } else {
            Player *p = &players[pid];
            if (p->items_collected < MAX_ITEMS_PER_PLAYER) {
                p->items[p->items_collected] = *item;
                p->items_collected++;
            }
            if (t == ITEM_QUEST) {
                p->quest_items_count++;
                pthread_mutex_lock(&quest_items_mutex);
                quest_items_collected++;
                team_won = (quest_items_collected >= QUEST_ITEMS_TO_WIN);
                pthread_mutex_unlock(&quest_items_mutex);
            } else if (t == ITEM_NORMAL) {
                p->normal_items_count++;
            }
        }
    }

    /* Uccisione mostro sulla casella */
    Monster *mon = room_get_monster(r, x, y);
    if (mon) {
        free(mon);
        r->monsters[y][x] = NULL;
        player_kill_monster(pid);
    }

    /* Mostri inseguono il giocatore (piu' aggressivi se fermo da 3s) */
    /*
     * Idle mechanism: determina se il player e' fermo da 3 secondi.
     * Se idle_countdown[pid] == 0, il player e' inattivo e i mostri
     * ottengono un bonus di movement opportunity (vedi room_move_monsters_toward_player).
     */
    bool player_idle = (idle_countdown[pid] == 0);
    int hits = room_move_monsters_toward_player(r, pid, y, x, player_idle);
    /* Applica danno cumulativo dal numero di mostri che hanno catturato. */
    for (int h = 0; h < hits; h++)
        player_lose_life();

    /* Condizioni di fine partita */
    if (team_won) {
        dungeon_kill_all_monsters(game_dungeon);
        return MSG_TEAM_WON;
    }
    pthread_mutex_lock(&team.lives_mutex);
    int lives = team.shared_lives;
    pthread_mutex_unlock(&team.lives_mutex);
    if (lives <= 0) {
        dungeon_kill_all_monsters(game_dungeon);
        return MSG_TEAM_DEFEATED;
    }

    return err;
}

/*
 * Handler UPDATE_PLAYER_POSITION: valida coordinate e aggiorna lo stato.
 * Se la casella contiene eventi, processa item/mostri e ritorna un messaggio.
 */
static void handle_update_player_position(const ClientRequest *req, Conn *conn,
                                          char *response, size_t res_len) {
    const char *err = NULL;
    pthread_mutex_lock(&game_state.mutex);

    /* Se la partita e' finita, ritorna lo stato con l'errore */
    pthread_mutex_lock(&team.lives_mutex);
    int lives = team.shared_lives;
    pthread_mutex_unlock(&team.lives_mutex);
    if (lives <= 0) {
        err = MSG_TEAM_DEFEATED;
    } else {
        pthread_mutex_lock(&quest_items_mutex);
        if (quest_items_collected >= QUEST_ITEMS_TO_WIN)
            err = MSG_TEAM_WON;
        pthread_mutex_unlock(&quest_items_mutex);
    }

    if (err != NULL) {
        /* Partita finita: non processare azioni, ritorna solo lo stato */
        char *json = build_game_state_json(conn->player_id, err);
        pthread_mutex_unlock(&game_state.mutex);
        copy_json_response(response, res_len, json);
        return;
    }

    if (!valid_pid(conn->player_id)) {
        err = MSG_INVALID_PLAYER;
    } else if (!req || !req->has_pos) {
        err = MSG_MISSING_POS;
    } else {
        int pid = conn->player_id;
        int rid = game_state.positions[pid];
        Room *r = (game_dungeon != NULL) ? dungeon_get_room(game_dungeon, rid) : NULL;
        int rh = r ? r->height : 0;
        int rw = r ? r->width  : 0;

        /* Coord fuori dai muri: rifiuta. */
        if (req->pos_y <= 0 || req->pos_y >= rh - 1 ||
            req->pos_x <= 0 || req->pos_x >= rw - 1) {
            err = MSG_INVALID_POS;
        } else {
            /* Aggiorna posizione e resetta countdown idle. */
            /*
             * Resetta idle_countdown a IDLE_THRESHOLD (3) ogni volta che il
             * player si muove. Questo fa ripartire il conteggio da 3 secondi.
             * Se il player rimane fermo per 3s senza chiamare UPDATE_PLAYER_POSITION,
             * idle_countdown arriva a 0 e i mostri diventano piu' aggressivi.
             */
            game_state.pos_y[pid] = req->pos_y;
            game_state.pos_x[pid] = req->pos_x;
            idle_countdown[pid] = IDLE_THRESHOLD;

            if (r != NULL)
                err = process_tile(pid, r, req->pos_x, req->pos_y);
        }
    }
    char *json = build_game_state_json(conn->player_id, err);
    pthread_mutex_unlock(&game_state.mutex);
    copy_json_response(response, res_len, json);
}

/*
 * Handler MOVE_PLAYER: valida la stanza target e aggiorna la stanza corrente.
 * Posiziona il player al centro della nuova stanza.
 */
static void handle_move_player(const ClientRequest *req, Conn *conn,
                               char *response, size_t res_len) {
    const char *err = NULL;
    int pid = conn->player_id;
    pthread_mutex_lock(&game_state.mutex);

    /* Se la partita e' finita, ritorna lo stato con l'errore */
    pthread_mutex_lock(&team.lives_mutex);
    int lives = team.shared_lives;
    pthread_mutex_unlock(&team.lives_mutex);
    if (lives <= 0) {
        err = MSG_TEAM_DEFEATED;
    } else {
        pthread_mutex_lock(&quest_items_mutex);
        if (quest_items_collected >= QUEST_ITEMS_TO_WIN)
            err = MSG_TEAM_WON;
        pthread_mutex_unlock(&quest_items_mutex);
    }

    if (err != NULL) {
        char *json = build_game_state_json(pid, err);
        pthread_mutex_unlock(&game_state.mutex);
        copy_json_response(response, res_len, json);
        return;
    }

    int current = valid_pid(pid) ? game_state.positions[pid] : -1;
    if (!req || !req->has_target) {
        err = MSG_MISSING_TARGET;
    } else if (current < 0 || current >= game_state.map_size) {
        err = MSG_INVALID_CURRENT_ROOM;
    } else if (req->target_room < 0 ||
               req->target_room >= game_state.map_size) {
        err = MSG_INVALID_TARGET;
    } else if (game_state.map[current][req->target_room] == 0) {
        err = MSG_INVALID_MOVE;
    } else {
        game_state.positions[pid] = req->target_room;
        Room *tr = (game_dungeon != NULL)
                       ? dungeon_get_room(game_dungeon, req->target_room) : NULL;
        game_state.pos_y[pid] = tr ? tr->height / 2 : 0;
        game_state.pos_x[pid] = tr ? tr->width  / 2 : 0;
        /*
         * Resetta idle_countdown quando il player si sposta tra stanze.
         * Questo assicura che ai mostri della nuova stanza sara' data opportunita'
         * di movimento solo dopo altri 3 secondi di inattivita'.
         */
        idle_countdown[pid] = IDLE_THRESHOLD;

        if (tr != NULL)
            assign_room_monsters(tr, req->target_room);
    }
    char *json = build_game_state_json(pid, err);
    pthread_mutex_unlock(&game_state.mutex);
    copy_json_response(response, res_len, json);
}

/* ========== Dispatcher e loop del player ========== */

/*
 * Dispatcha il comando verso l'handler corretto.
 * Mantiene la logica di routing centralizzata.
 */
void manage_response(Message cmd, const ClientRequest *req, Conn *conn,
                     char *response, size_t res_len) {
    switch (cmd) {
    case GET_N_OF_CONNECTED_PLAYERS:
        handle_get_connected_players(response, res_len);
        break;
    case START_GAME:
        handle_start_game(conn, response, res_len);
        break;
    case GET_GAME_STATE:
        handle_get_game_state(conn, response, res_len);
        break;
    case UPDATE_PLAYER_POSITION:
        handle_update_player_position(req, conn, response, res_len);
        break;
    case MOVE_PLAYER:
        handle_move_player(req, conn, response, res_len);
        break;
    }
}

/*
 * Thread principale per una connessione client:
 * - riceve comandi
 * - produce risposte
 * - gestisce disconnessioni e pulizia stato
 */
void *handle_player(void *args) {
  Conn *myconn = (Conn *)args;
  printf("[SERVER] Handling player %d\n", myconn->player_id);
  
  while (server_running) {
    char msg[FRAME_SIZE];
    memset(msg, 0, sizeof(msg));

    /* 
       MSG_WAITALL garantisce che recv torni solo quando ha letto
       esattamente sizeof(msg) byte, evitando read parziali.
    */
    ssize_t r = recv(myconn->sockfd, msg, sizeof(msg), MSG_WAITALL);
    if (r < 0) {
      if (errno == EINTR) continue;
      perror("[server, recv, handle_player]");
      break;
    }
    if (r == 0) break;

    char response[FRAME_SIZE];
    memset(response, 0, sizeof(response));
    /* Parsing e dispatch del comando ricevuto. */
    ClientRequest req = parse_client_request(msg);
    manage_response(req.cmd, &req, myconn, response, sizeof(response));

        /* Invio della risposta serializzata al client. */
        if (send_all(myconn->sockfd, response, sizeof(response)) < 0)
      break;
  }

    /* Cleanup connessione e stato player. */
    close(myconn->sockfd);
  myconn->sockfd = 0;

  pthread_mutex_lock(&game_state.mutex);
  if (myconn->player_id >= 0 && myconn->player_id < MAX_PLAYERS) {
    game_state.positions[myconn->player_id] = -1;
    game_state.pos_y[myconn->player_id] = -1;
    game_state.pos_x[myconn->player_id] = -1;
  }
  pthread_mutex_unlock(&game_state.mutex);

  pthread_mutex_lock(&plid_mutex);
  if (plid_seq > 0) plid_seq -= 1;
  pthread_mutex_unlock(&plid_mutex);
  return NULL;
}
