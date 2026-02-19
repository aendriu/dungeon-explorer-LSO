#include "../header/player.h"
#include "../header/game_state.h"
#include "../header/monster.h"
#include "../header/protocol.h"
#include "../../utils/enums.h"
#include "cJSON.h"

int plid_seq = 0;
Player players[MAX_PLAYERS] = {0};

static pthread_mutex_t plid_mutex = PTHREAD_MUTEX_INITIALIZER;

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

void print_player_items(int player_id) {
    if (player_id < 0 || player_id >= MAX_PLAYERS) {
        printf("[ERROR] Invalid player_id: %d\n", player_id);
        return;
    }

    Player *ps = &players[player_id];
    printf("\n========================================\n");
    printf("INVENTORY PLAYER %d:\n", player_id);
    printf("- Item Normali: %d\n", ps->normal_items_count);
    printf("- Item Quest: %d\n", ps->quest_items_count);
    printf("- Total Items: %d/%d\n", ps->items_collected, MAX_ITEMS_PER_PLAYER);
    printf("- Trappole Attivate: %d\n", ps->traps_triggered);
    printf("\nDettagli:\n");

    if (ps->items_collected == 0) {
        printf("  (Nessun item collezionato)\n");
    } else {
        for (int i = 0; i < ps->items_collected; i++) {
            const char *type_str = (ps->items[i].type == ITEM_QUEST) ? "QUEST" : "NORMAL";
            printf("  [%d] %s\n", i + 1, type_str);
        }
    }
    printf("========================================\n\n");
}

void broadcast_all_inventories(void) {
    cJSON *root = cJSON_CreateObject();
    cJSON *players_arr = cJSON_CreateArray();

    pthread_mutex_lock(&conn_mutex);
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (connections[i].sockfd == 0) continue;

        cJSON *player_obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(player_obj, "player_id", i);
        cJSON_AddNumberToObject(player_obj, "normal_items", players[i].normal_items_count);
        cJSON_AddNumberToObject(player_obj, "quest_items", players[i].quest_items_count);
        cJSON_AddNumberToObject(player_obj, "total_items", players[i].items_collected);
        cJSON_AddNumberToObject(player_obj, "traps_triggered", players[i].traps_triggered);

        cJSON *items_arr = cJSON_CreateArray();
        for (int j = 0; j < players[i].items_collected; j++) {
            cJSON *item_obj = cJSON_CreateObject();
            const char *type_str = (players[i].items[j].type == ITEM_QUEST) ? "QUEST" : "NORMAL";
            cJSON_AddStringToObject(item_obj, "type", type_str);
            cJSON_AddItemToArray(items_arr, item_obj);
        }
        cJSON_AddItemToObject(player_obj, "items", items_arr);
        cJSON_AddItemToArray(players_arr, player_obj);
    }

    cJSON_AddItemToObject(root, "inventories", players_arr);
    char *json_str = cJSON_Print(root);

    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (connections[i].sockfd != 0) {
            send_all(connections[i].sockfd, json_str, strlen(json_str) + 1);
        }
    }
    pthread_mutex_unlock(&conn_mutex);

    cJSON_free(json_str);
    cJSON_Delete(root);
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

  players[idx].player_id = idx;
  players[idx].items_collected = 0;
  players[idx].normal_items_count = 0;
  players[idx].quest_items_count = 0;
  players[idx].traps_triggered = 0;
  players[idx].monsters_killed = 0;
  memset(players[idx].items, 0, sizeof(players[idx].items));

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

  pthread_detach(tid);
  connections[idx].tid = tid;
  plid_seq += 1;
  pthread_mutex_unlock(&plid_mutex);
}

int player_lose_life() {
    pthread_mutex_lock(&team.lives_mutex);
    if (team.shared_lives > 0)
        team.shared_lives--;
    int remaining_lives = team.shared_lives;
    pthread_mutex_unlock(&team.lives_mutex);
    printf("[TEAM] Lost a life! Remaining: %d\n", remaining_lives);
  if (remaining_lives <= 0 && game_dungeon != NULL) {
    dungeon_kill_all_monsters(game_dungeon);
  }
    return remaining_lives;
}

void player_kill_monster(int player_id) {
  if (player_id < 0 || player_id >= MAX_PLAYERS) {
    return;
  }

  pthread_mutex_lock(&conn_mutex);
  players[player_id].monsters_killed++;
  pthread_mutex_unlock(&conn_mutex);

  pthread_mutex_lock(&team.lives_mutex);
  team.monsters_killed++;
  pthread_mutex_unlock(&team.lives_mutex);
}

bool player_collect_item(int player_id, int x, int y) {
    if (!game_dungeon) return false;

    int room_id = (player_id >= 0 && player_id < MAX_PLAYERS)
                      ? game_state.positions[player_id] : -1;
    if (room_id < 0) return false;

    Room *r = dungeon_get_room(game_dungeon, room_id);
    Item *item = room_get_item(r, x, y);
    if (!item) return false;

    ItemType t = item->type;
    bool team_won = item_collect(game_dungeon, player_id, room_id, x, y);

    if (t == ITEM_TRAP || t == ITEM_BOOBYTRAP) {
        if (player_id >= 0 && player_id < MAX_PLAYERS)
            players[player_id].traps_triggered++;
        player_lose_life();
        return false;
    }

    if (player_id >= 0 && player_id < MAX_PLAYERS) {
        Player *p = &players[player_id];
        if (p->items_collected < MAX_ITEMS_PER_PLAYER) {
            p->items[p->items_collected] = *item;
            p->items_collected++;
            if (t == ITEM_QUEST)  p->quest_items_count++;
            if (t == ITEM_NORMAL) p->normal_items_count++;
        }
    }

    return team_won;
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
    if (r == 0) {
        break;
    }

    char response[FRAME_SIZE];
    memset(response, 0, sizeof(response));
    ClientRequest req = parse_client_request(msg);
    manage_response(req.cmd, &req, myconn, response, sizeof(response));

    if (send_all(myconn->sockfd, response, sizeof(response)) < 0) {
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
