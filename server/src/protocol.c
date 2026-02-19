#include "../header/protocol.h"
#include "../header/player.h"
#include "../header/monster.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *skip_whitespace(const char *s) {
    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')
        s++;
    return s;
}

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

    msg = skip_whitespace(msg);

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

static void handle_get_connected_players(char *response, size_t res_len) {
    snprintf(response, res_len, "%d", get_connected_players());
}

static void handle_start_game(Conn *conn, char *response, size_t res_len) {
    init_game_state_if_needed();
    pthread_mutex_lock(&game_state.mutex);
    if (conn->player_id >= 0 && conn->player_id < MAX_PLAYERS &&
        game_state.positions[conn->player_id] < 0) {
        game_state.positions[conn->player_id] = 0;
        game_state.pos_y[conn->player_id]     = ROOM_H / 2;
        game_state.pos_x[conn->player_id]     = ROOM_W / 2;
    }
    char *json = build_game_state_json(conn->player_id, NULL);
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

static void handle_update_player_position(const ClientRequest *req, Conn *conn,
                                          char *response, size_t res_len) {
    init_game_state_if_needed();
    const char *err = NULL;
    pthread_mutex_lock(&game_state.mutex);
    if (conn->player_id < 0 || conn->player_id >= MAX_PLAYERS) {
        err = "invalid_player";
    } else if (!req || !req->has_pos) {
        err = "missing_pos";
    } else if (req->pos_y <= 0 || req->pos_y >= ROOM_H - 1 ||
               req->pos_x <= 0 || req->pos_x >= ROOM_W - 1) {
        err = "invalid_pos";
    } else {
        game_state.pos_y[conn->player_id] = req->pos_y;
        game_state.pos_x[conn->player_id] = req->pos_x;

        if (game_dungeon != NULL) {
            int rid = game_state.positions[conn->player_id];
            Room *r = dungeon_get_room(game_dungeon, rid);

            /* Verifica il tipo di item prima di raccoglierlo */
            Item *item_check = room_get_item(r, req->pos_x, req->pos_y);
            ItemType item_type = (item_check != NULL) ? item_check->type : ITEM_NONE;

            bool team_won = player_collect_item(conn->player_id, req->pos_x, req->pos_y);

            /* Se è una booby trap piazzata da un mostro, mostra messaggio personalizzato */
            if (item_type == ITEM_BOOBYTRAP) {
                printf("[PLAYER %d] A monster had placed a trap and you picked it up at room %d!\n", 
                       conn->player_id, rid);
            }

            Monster *mon = room_get_monster(r, req->pos_x, req->pos_y);
            if (mon) {
                free(mon);
                r->monsters[req->pos_y][req->pos_x] = NULL;
                player_kill_monster(conn->player_id);
            }

            /* Muovi i mostri che inseguono questo giocatore */
            int monster_kills = room_move_monsters_toward_player(
                r, conn->player_id, req->pos_y, req->pos_x);
            for (int h = 0; h < monster_kills; h++) {
                player_kill_monster(conn->player_id);
            }

            if (team_won) {
                dungeon_kill_all_monsters(game_dungeon);
                err = "team_won";
            }
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
    pthread_mutex_lock(&game_state.mutex);
    int current = (conn->player_id >= 0 && conn->player_id < MAX_PLAYERS)
                      ? game_state.positions[conn->player_id]
                      : -1;
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
        game_state.positions[conn->player_id] = req->target_room;
        game_state.pos_y[conn->player_id]     = ROOM_H / 2;
        game_state.pos_x[conn->player_id]     = ROOM_W / 2;

        /* Assegna i mostri della nuova stanza ai giocatori presenti */
        if (game_dungeon != NULL) {
            Room *r = dungeon_get_room(game_dungeon, req->target_room);
            if (r != NULL) {
                int pids[MAX_PLAYERS];
                int np = 0;
                for (int i = 0; i < MAX_PLAYERS; i++) {
                    if (game_state.positions[i] == req->target_room)
                        pids[np++] = i;
                }
                if (np > 0)
                    room_assign_monsters_to_players(r, pids, np);
            }
        }
    }

    char *json = build_game_state_json(conn->player_id, err);
    pthread_mutex_unlock(&game_state.mutex);
    copy_json_response(response, res_len, json);
}

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
