#include "../header/player.h"
#include "../../utils/enums.h"
#include "cJSON.h"
#include <time.h>

// player count
int plid_seq = 0;

static pthread_mutex_t plid_mutex = PTHREAD_MUTEX_INITIALIZER;

enum { FRAME_SIZE = 4096 };

enum { MAP_SIZE = 10, DIFFICULTY = 3 };
enum { ROOM_H = 9, ROOM_W = 21 };

typedef struct {
  int map_size;
  int map[MAP_SIZE][MAP_SIZE];
  int positions[MAX_PLAYERS];
  int pos_y[MAX_PLAYERS];
  int pos_x[MAX_PLAYERS];
  bool started;
  pthread_mutex_t mutex;
} GameState;

static GameState game_state = {.map_size = MAP_SIZE,
                               .map = {{0}},
                               .positions = {0},
                               .pos_y = {0},
                               .pos_x = {0},
                               .started = false,
                               .mutex = PTHREAD_MUTEX_INITIALIZER};

static void add_corridor(int map[MAP_SIZE][MAP_SIZE], int i, int j) {
  map[i][j] = 1;
  map[j][i] = 1;
}

static int rand_room(void) { return rand() % MAP_SIZE; }

static void generate_random_map(int map[MAP_SIZE][MAP_SIZE]) {
  int rooms_number = (rand() % MAP_SIZE) * DIFFICULTY;
  for (int i = 0; i < rooms_number; i++) {
    add_corridor(map, rand_room(), rand_room());
  }
}

static void init_game_state_if_needed(void) {
  static int seeded = 0;
  if (!seeded) {
    seeded = 1;
    srand((unsigned int)time(NULL));
  }
  
  pthread_mutex_lock(&game_state.mutex);
  if (!game_state.started) {
    memset(game_state.map, 0, sizeof(game_state.map));
    generate_random_map(game_state.map);
    for (int i = 0; i < MAX_PLAYERS; i++) {
      game_state.positions[i] = -1;
      game_state.pos_y[i] = -1;
      game_state.pos_x[i] = -1;
    }
    game_state.started = true;
  }
  pthread_mutex_unlock(&game_state.mutex);
}

static int find_free_slot(Conn *connections) {
  for (int i = 0; i < MAX_PLAYERS; i++) {
    if (connections[i].sockfd == 0) {
      return i;
    }
  }
  return -1;
}
int send_all(int sockfd, const char *msg, size_t len) {
  if (sockfd <= 0 || msg == NULL) {
    return -1;
  }

  size_t sent = 0;

  while (sent < len) {
    ssize_t rc = send(sockfd, msg + sent, len - sent, 0);

    if (rc < 0) {
      if (errno == EINTR) {
        continue; // interrotta da segnale, riprova
      }
      return -1; // errore reale
    }

    if (rc == 0) {
      return -1; // connessione chiusa
    }

    sent += (size_t)rc;
  }

  return 0; // tutto inviato correttamente
}
int get_connected_players(void) {
  int count = 0;
  pthread_mutex_lock(&plid_mutex);
  count = plid_seq;
  pthread_mutex_unlock(&plid_mutex);
  return count;
}

void init_newplayer(int sockfd, Conn *connections) {
  pthread_t tid;

  pthread_mutex_lock(&plid_mutex);
  int idx = find_free_slot(connections);
  if (idx < 0) {
    pthread_mutex_unlock(&plid_mutex);
    close(sockfd);
    return;
  }

  connections[idx].player_id = idx;
  connections[idx].sockfd = sockfd;

  pthread_mutex_lock(&game_state.mutex);
  if (game_state.started) {
    game_state.positions[idx] = 0;
    game_state.pos_y[idx] = ROOM_H / 2;
    game_state.pos_x[idx] = ROOM_W / 2;
  }
  pthread_mutex_unlock(&game_state.mutex);

  int rc = pthread_create(&tid, NULL, handle_player,
                          (void *)(&connections[idx]));
  if (rc != 0) {
    perror("pthread_create ERROR in init_newplayer: ");
    connections[idx].sockfd = 0;
    pthread_mutex_unlock(&plid_mutex);
    close(sockfd);
    return;
  }

  connections[idx].tid = tid;
  plid_seq += 1;
  pthread_mutex_unlock(&plid_mutex);
}
static char *build_game_state_json(int player_id, const char *error) {
  cJSON *root = cJSON_CreateObject();
  if (root == NULL) {
    return NULL;
  }

  cJSON_AddNumberToObject(root, "map_size", game_state.map_size);
  cJSON_AddNumberToObject(root, "player_id", player_id);
  cJSON_AddNumberToObject(root, "current_room",
                          (player_id >= 0 && player_id < MAX_PLAYERS)
                              ? game_state.positions[player_id]
                              : -1);
  if (error != NULL) {
    cJSON_AddStringToObject(root, "error", error);
  }

  cJSON *players = cJSON_AddArrayToObject(root, "players");
  if (players == NULL) {
    cJSON_Delete(root);
    return NULL;
  }
  for (int i = 0; i < MAX_PLAYERS; i++) {
    cJSON_AddItemToArray(players, cJSON_CreateNumber(game_state.positions[i]));
  }

  cJSON *pos = cJSON_AddArrayToObject(root, "pos");
  if (pos == NULL) {
    cJSON_Delete(root);
    return NULL;
  }
  for (int i = 0; i < MAX_PLAYERS; i++) {
    cJSON *pair = cJSON_CreateArray();
    if (pair == NULL) {
      cJSON_Delete(root);
      return NULL;
    }
    cJSON_AddItemToArray(pair, cJSON_CreateNumber(game_state.pos_y[i]));
    cJSON_AddItemToArray(pair, cJSON_CreateNumber(game_state.pos_x[i]));
    cJSON_AddItemToArray(pos, pair);
  }

  cJSON *adj = cJSON_AddArrayToObject(root, "adj");
  if (adj == NULL) {
    cJSON_Delete(root);
    return NULL;
  }

  for (int i = 0; i < game_state.map_size; i++) {
    cJSON *row = cJSON_CreateArray();
    if (row == NULL) {
      cJSON_Delete(root);
      return NULL;
    }
    for (int j = 0; j < game_state.map_size; j++) {
      cJSON_AddItemToArray(row, cJSON_CreateNumber(game_state.map[i][j]));
    }
    cJSON_AddItemToArray(adj, row);
  }

  char *json = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  return json;
}

// *****

int player_lose_life() {
    pthread_mutex_lock(&team.lives_mutex);
    team.shared_lives--;
    int remaining_lives = team.shared_lives;
    printf("[TEAM] Player lost a life! Remaining: %d\n", remaining_lives);
    
    if (remaining_lives <= 0) {
        printf("[TEAM] TEAM DEFEATED!\n");
        // Inviare il messaggio di sconfitta a tutti i giocatori
        for (int i = 0; i < plid_seq; i++) {
            if (connections[i].sockfd != 0) {
                send_all(connections[i].sockfd, MSG_TEAM_DEFEATED, strlen(MSG_TEAM_DEFEATED) + 1);
            }
        }
    }
    
    pthread_mutex_unlock(&team.lives_mutex);
    return remaining_lives;
}

bool player_collect_item(int player_id, int x, int y) {
    if (!game_dungeon) {
        return false;
    }

    // For now, use room 0 as default (can be expanded with player tracking)
    int room_id = 0;
    bool team_won = item_collect(game_dungeon, player_id, room_id, x, y);

    if (team_won) {
        printf("[GAME] TEAM WON! All quest items collected!\n");
        // Inviare il messaggio di vittoria a tutti i giocatori
        for (int i = 0; i < plid_seq; i++) {
            if (connections[i].sockfd != 0) {
                send_all(connections[i].sockfd, MSG_TEAM_WON, strlen(MSG_TEAM_WON) + 1);
            }
        }
        return true;
    }

    // Broadcast item collected message with player name
    char item_msg[256];
    snprintf(item_msg, sizeof(item_msg), "[PLAYER %d] Hai preso un oggetto!", player_id);
    for (int i = 0; i < plid_seq; i++) {
        if (connections[i].sockfd != 0) {
            send_all(connections[i].sockfd, item_msg, strlen(item_msg) + 1);
        }
    }

    // Inviare aggiornamento del quest status ai giocatori
    char quest_msg[256];
    snprintf(quest_msg, sizeof(quest_msg), "%s:%d", MSG_QUEST_UPDATE, quest_items_collected);
    for (int i = 0; i < plid_seq; i++) {
        if (connections[i].sockfd != 0) {
            send_all(connections[i].sockfd, quest_msg, strlen(quest_msg) + 1);
        }
    }

    // Incrementare counter items per il giocatore
    if (player_id >= 0 && player_id < plid_seq) {
        connections[player_id].items_collected++;
    }

    return false;
}

typedef struct {
  Message cmd;
  int target_room;
  bool has_target;
  int pos_y;
  int pos_x;
  bool has_pos;
} ClientRequest;

static ClientRequest parse_client_request(const char *msg) {
  ClientRequest req = {.cmd = GET_N_OF_CONNECTED_PLAYERS,
                       .target_room = -1,
                       .has_target = false,
                       .pos_y = -1,
                       .pos_x = -1,
                       .has_pos = false};

  if (msg == NULL) {
    return req;
  }

  while (*msg == ' ' || *msg == '\t' || *msg == '\r' || *msg == '\n') {
    msg++;
  }

  if (*msg == '{') {
    cJSON *root = cJSON_Parse(msg);
    if (root != NULL) {
      cJSON *cmd = cJSON_GetObjectItemCaseSensitive(root, "cmd");
      if (cJSON_IsNumber(cmd)) {
        req.cmd = (Message)cmd->valueint;
      }
      cJSON *payload = cJSON_GetObjectItemCaseSensitive(root, "payload");
      if (cJSON_IsObject(payload)) {
        cJSON *target = cJSON_GetObjectItemCaseSensitive(payload, "target");
        if (cJSON_IsNumber(target)) {
          req.target_room = target->valueint;
          req.has_target = true;
        }
        cJSON *py = cJSON_GetObjectItemCaseSensitive(payload, "y");
        cJSON *px = cJSON_GetObjectItemCaseSensitive(payload, "x");
        if (cJSON_IsNumber(py) && cJSON_IsNumber(px)) {
          req.pos_y = py->valueint;
          req.pos_x = px->valueint;
          req.has_pos = true;
        }
      }
      cJSON_Delete(root);
      return req;
    }
  }

  req.cmd = (Message)atoi(msg);
  return req;
}

void manage_response(Message cmd, const ClientRequest *req, Conn *conn,
                     char *response, size_t res_len) {
  switch (cmd) {
  case GET_N_OF_CONNECTED_PLAYERS:
    snprintf(response, res_len, "%d", get_connected_players());
    break;
  case START_GAME:
  {
    init_game_state_if_needed();
    pthread_mutex_lock(&game_state.mutex);
    char *json = build_game_state_json(conn->player_id, NULL);
    pthread_mutex_unlock(&game_state.mutex);
    if (json == NULL) {
      snprintf(response, res_len, "{\"error\":\"server_error\"}");
      break;
    }
    if (strlen(json) >= res_len) {
      snprintf(response, res_len, "{\"error\":\"response_too_large\"}");
      cJSON_free(json);
      break;
    }
    strncpy(response, json, res_len - 1);
    response[res_len - 1] = '\0';
    cJSON_free(json);
    break;
  }
  case GET_GAME_STATE:
  {
    init_game_state_if_needed();
    pthread_mutex_lock(&game_state.mutex);
    char *json = build_game_state_json(conn->player_id, NULL);
    pthread_mutex_unlock(&game_state.mutex);
    if (json == NULL) {
      snprintf(response, res_len, "{\"error\":\"server_error\"}");
      break;
    }
    if (strlen(json) >= res_len) {
      snprintf(response, res_len, "{\"error\":\"response_too_large\"}");
      cJSON_free(json);
      break;
    }
    strncpy(response, json, res_len - 1);
    response[res_len - 1] = '\0';
    cJSON_free(json);
    break;
  }
  case UPDATE_PLAYER_POSITION:
  {
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
    }

    char *json = build_game_state_json(conn->player_id, err);
    pthread_mutex_unlock(&game_state.mutex);
    if (json == NULL) {
      snprintf(response, res_len, "{\"error\":\"server_error\"}");
      break;
    }
    if (strlen(json) >= res_len) {
      snprintf(response, res_len, "{\"error\":\"response_too_large\"}");
      cJSON_free(json);
      break;
    }
    strncpy(response, json, res_len - 1);
    response[res_len - 1] = '\0';
    cJSON_free(json);
    break;
  }
  case MOVE_PLAYER: {
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
    } else if (req->target_room < 0 || req->target_room >= game_state.map_size) {
      err = "invalid_target";
    } else if (game_state.map[current][req->target_room] == 0) {
      err = "invalid_move";
    } else {
      game_state.positions[conn->player_id] = req->target_room;
      game_state.pos_y[conn->player_id] = ROOM_H / 2;
      game_state.pos_x[conn->player_id] = ROOM_W / 2;
    }

    char *json = build_game_state_json(conn->player_id, err);
    pthread_mutex_unlock(&game_state.mutex);
    if (json == NULL) {
      snprintf(response, res_len, "{\"error\":\"server_error\"}");
      break;
    }
    if (strlen(json) >= res_len) {
      snprintf(response, res_len, "{\"error\":\"response_too_large\"}");
      cJSON_free(json);
      break;
    }
    strncpy(response, json, res_len - 1);
    response[res_len - 1] = '\0';
    cJSON_free(json);
    break;
  }
  }
}

void *handle_player(void *args) {
  Conn *myconn = (Conn *)args;
  printf("Im handling player!\n");
  while (1) {
    char msg[FRAME_SIZE];
    memset(msg, 0, sizeof(msg));

    int r = recv(myconn->sockfd, msg, sizeof(msg), 0);
    if (r < 0) {
      perror("[server, recv, handle_player]");
      break;
    }
    if (r == 0) {
      /* peer closed connection */
      break;
    }

    char response[FRAME_SIZE];
    memset(response, 0, sizeof(response));
    ClientRequest req = parse_client_request(msg);
    manage_response(req.cmd, &req, myconn, response, sizeof(response));

    if (send(myconn->sockfd, response, sizeof(response), 0) < 0) {
      break;
    }
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
  if (plid_seq > 0) {
    plid_seq -= 1;
  }
  pthread_mutex_unlock(&plid_mutex);
  return NULL;
}
