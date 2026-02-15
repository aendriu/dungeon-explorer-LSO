#include "../header/player.h"
#include "../header/game_state.h"
#include "../header/protocol.h"
#include "../../utils/enums.h"
#include "cJSON.h"

int plid_seq = 0;

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

    Conn *player = &connections[player_id];
    printf("\n========================================\n");
    printf("INVENTORY PLAYER %d:\n", player_id);
    printf("- Item Normali: %d\n", player->normal_items_count);
    printf("- Item Quest: %d\n", player->quest_items_count);
    printf("- Total Items: %d/%d\n", player->items_collected, MAX_ITEMS_PER_PLAYER);
    printf("- Trappole Attivate: %d\n", player->traps_triggered);
    printf("\nDettagli:\n");

    if (player->items_collected == 0) {
        printf("  (Nessun item collezionato)\n");
    } else {
        for (int i = 0; i < player->items_collected; i++) {
            const char *type_str = (player->items[i].type == ITEM_QUEST) ? "QUEST" : "NORMAL";
            printf("  [%d] %s\n", i + 1, type_str);
        }
    }
    printf("========================================\n\n");
}

void broadcast_all_inventories(void) {
    if (plid_seq <= 0) {
        return;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON *players_arr = cJSON_CreateArray();

    for (int i = 0; i < plid_seq; i++) {
        if (connections[i].sockfd == 0) continue;

        cJSON *player_obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(player_obj, "player_id", i);
        cJSON_AddNumberToObject(player_obj, "normal_items", connections[i].normal_items_count);
        cJSON_AddNumberToObject(player_obj, "quest_items", connections[i].quest_items_count);
        cJSON_AddNumberToObject(player_obj, "total_items", connections[i].items_collected);
        cJSON_AddNumberToObject(player_obj, "traps_triggered", connections[i].traps_triggered);

        cJSON *items_arr = cJSON_CreateArray();
        for (int j = 0; j < connections[i].items_collected; j++) {
            cJSON *item_obj = cJSON_CreateObject();
            const char *type_str = (connections[i].items[j].type == ITEM_QUEST) ? "QUEST" : "NORMAL";
            cJSON_AddStringToObject(item_obj, "type", type_str);
            cJSON_AddItemToArray(items_arr, item_obj);
        }
        cJSON_AddItemToObject(player_obj, "items", items_arr);
        cJSON_AddItemToArray(players_arr, player_obj);
    }

    cJSON_AddItemToObject(root, "inventories", players_arr);
    char *json_str = cJSON_Print(root);

    // Invia a tutti i giocatori
    for (int i = 0; i < plid_seq; i++) {
        if (connections[i].sockfd != 0) {
            send_all(connections[i].sockfd, json_str, strlen(json_str) + 1);
        }
    }

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
  connections[idx].items_collected = 0;
  connections[idx].normal_items_count = 0;
  connections[idx].quest_items_count = 0;
  connections[idx].traps_triggered = 0;
  memset(connections[idx].items, 0, sizeof(connections[idx].items));

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

int player_lose_life() {
    pthread_mutex_lock(&team.lives_mutex);
    team.shared_lives--;
    int remaining_lives = team.shared_lives;
    printf("[TEAM] Player lost a life! Remaining: %d\n", remaining_lives);
    pthread_mutex_unlock(&team.lives_mutex);

    if (remaining_lives <= 0) {
        printf("[TEAM] TEAM DEFEATED!\n");
        broadcast_all_inventories();
        sleep(1);
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (connections[i].sockfd != 0) {
                send_all(connections[i].sockfd, MSG_TEAM_DEFEATED,
                         strlen(MSG_TEAM_DEFEATED) + 1);
            }
        }
    } else {
        char life_msg[64];
        snprintf(life_msg, sizeof(life_msg), "%s:%d",
                 MSG_LIFE_LOST, remaining_lives);
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (connections[i].sockfd != 0) {
                send_all(connections[i].sockfd, life_msg,
                         strlen(life_msg) + 1);
            }
        }
    }

    return remaining_lives;
}

bool player_collect_item(int player_id, int x, int y) {
    if (!game_dungeon) {
        return false;
    }

    int room_id = (player_id >= 0 && player_id < MAX_PLAYERS)
                      ? game_state.positions[player_id]
                      : -1;
    if (room_id < 0)
        return false;

    // Recupera l'item prima di collezionarlo (per salvarlo nell'inventory)
    Room *r = dungeon_get_room(game_dungeon, room_id);
    Item *item = room_get_item(r, x, y);
    ItemType collected_type = ITEM_NONE;
    if (item) {
        collected_type = item->type;
    }

    bool team_won = item_collect(game_dungeon, player_id, room_id, x, y);

    // Gestione trappola: fa perdere una vita
    if (collected_type == ITEM_TRAP) {
        // Incrementa il contatore delle trappole del player
        if (player_id >= 0 && player_id < MAX_PLAYERS) {
            connections[player_id].traps_triggered++;
        }
        
        char trap_msg[256];
        snprintf(trap_msg, sizeof(trap_msg), "[PLAYER %d] Ha attivato una TRAPPOLA!", player_id);
        for (int i = 0; i < plid_seq; i++) {
            if (connections[i].sockfd != 0) {
                send_all(connections[i].sockfd, trap_msg, strlen(trap_msg) + 1);
            }
        }
        
        int remaining = player_lose_life();
        if (remaining <= 0) {
            return true; // Team defeated
        }
        return false;
    }

    // Aggiungi l'item all'inventory del player se è un item valido (non trappola)
    if (collected_type != ITEM_NONE && collected_type != ITEM_TRAP && player_id >= 0 && player_id < MAX_PLAYERS) {
        Conn *player = &connections[player_id];
        if (player->items_collected < MAX_ITEMS_PER_PLAYER) {
            player->items[player->items_collected] = *item;
            player->items_collected++;

            if (collected_type == ITEM_QUEST) {
                player->quest_items_count++;
            } else if (collected_type == ITEM_NORMAL) {
                player->normal_items_count++;
            }
        }
    }

    if (team_won) {
        printf("[GAME] TEAM WON! All quest items collected!\n");
        broadcast_all_inventories();
        sleep(1);
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

    return false;
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
