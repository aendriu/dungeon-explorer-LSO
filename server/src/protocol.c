#include "../header/protocol.h"
#include "../header/player.h"
#include "../header/monster.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* ========== Parsing della richiesta client ========== */

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

static void parse_json_request(const char *msg, ClientRequest *req) {
    cJSON *root = cJSON_Parse(msg);
    if (root == NULL) return;

    cJSON *cmd = cJSON_GetObjectItemCaseSensitive(root, "cmd");
    if (cJSON_IsNumber(cmd))
        req->cmd = (Message)cmd->valueint;

    cJSON *payload = cJSON_GetObjectItemCaseSensitive(root, "payload");
    if (cJSON_IsObject(payload))
        parse_payload(payload, req);

    cJSON_Delete(root);
}

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

static bool valid_pid(int pid) {
    return pid >= 0 && pid < MAX_PLAYERS;
}

static void handle_get_connected_players(char *response, size_t res_len) {
    snprintf(response, res_len, "%d", get_connected_players());
}

static void handle_start_game(Conn *conn, char *response, size_t res_len) {
    init_game_state_if_needed();
    int pid = conn->player_id;
    pthread_mutex_lock(&game_state.mutex);
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

static void handle_get_game_state(Conn *conn, char *response, size_t res_len) {
    init_game_state_if_needed();
    pthread_mutex_lock(&game_state.mutex);
    char *json = build_game_state_json(conn->player_id, NULL);
    pthread_mutex_unlock(&game_state.mutex);
    copy_json_response(response, res_len, json);
}

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

    /* Mostri inseguono il giocatore */
    int hits = room_move_monsters_toward_player(r, pid, y, x);
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

static void handle_update_player_position(const ClientRequest *req, Conn *conn,
                                          char *response, size_t res_len) {
    init_game_state_if_needed();
    const char *err = NULL;
    pthread_mutex_lock(&game_state.mutex);
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

        if (req->pos_y <= 0 || req->pos_y >= rh - 1 ||
            req->pos_x <= 0 || req->pos_x >= rw - 1) {
            err = MSG_INVALID_POS;
        } else {
            game_state.pos_y[pid] = req->pos_y;
            game_state.pos_x[pid] = req->pos_x;

            if (r != NULL)
                err = process_tile(pid, r, req->pos_x, req->pos_y);
        }
    }
    char *json = build_game_state_json(conn->player_id, err);
    pthread_mutex_unlock(&game_state.mutex);
    copy_json_response(response, res_len, json);
}

static void handle_move_player(const ClientRequest *req, Conn *conn,
                               char *response, size_t res_len) {
    init_game_state_if_needed();
    const char *err = NULL;
    int pid = conn->player_id;
    pthread_mutex_lock(&game_state.mutex);
    int current = valid_pid(pid) ? game_state.positions[pid] : -1;
    if (!req || !req->has_target) {
        err = "missing_target";
    } else if (current < 0 || current >= game_state.map_size) {
        err = "invalid_current_room";
    } else if (req->target_room < 0 ||
               req->target_room >= game_state.map_size) {
        err = "invalid_target";
    } else if (game_state.map[current][req->target_room] == 0) {
        err = "invalid_move";
    } else {
        game_state.positions[pid] = req->target_room;
        Room *tr = (game_dungeon != NULL)
                       ? dungeon_get_room(game_dungeon, req->target_room) : NULL;
        game_state.pos_y[pid] = tr ? tr->height / 2 : 0;
        game_state.pos_x[pid] = tr ? tr->width  / 2 : 0;

        if (tr != NULL)
            assign_room_monsters(tr, req->target_room);
    }
    char *json = build_game_state_json(pid, err);
    pthread_mutex_unlock(&game_state.mutex);
    copy_json_response(response, res_len, json);
}

/* ========== Dispatcher e loop del player ========== */

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

void *handle_player(void *args) {
  Conn *myconn = (Conn *)args;
  printf("[SERVER] Handling player %d\n", myconn->player_id);
  
  while (server_running) {
    char msg[FRAME_SIZE];
    memset(msg, 0, sizeof(msg));

    ssize_t r = recv(myconn->sockfd, msg, sizeof(msg), 0);
    if (r < 0) {
      if (errno == EINTR) continue;
      perror("[server, recv, handle_player]");
      break;
    }
    if (r == 0) break;

    char response[FRAME_SIZE];
    memset(response, 0, sizeof(response));
    ClientRequest req = parse_client_request(msg);
    manage_response(req.cmd, &req, myconn, response, sizeof(response));

    if (send_all(myconn->sockfd, response, sizeof(response)) < 0)
      break;
  }

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
