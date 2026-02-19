#include "../header/player.h"
#include "../header/game_state.h"
#include "../header/monster.h"
#include "../header/protocol.h"
#include "../../utils/enums.h"
#include "cJSON.h"

int plid_seq = 0;
Player players[MAX_PLAYERS] = {0};

pthread_mutex_t plid_mutex = PTHREAD_MUTEX_INITIALIZER;

static int find_free_slot(Conn *connections) {
  for (int i = 0; i < MAX_PLAYERS; i++) {
    if (connections[i].sockfd == 0) {
      return i;
    }
  }
  return -1;
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

  int rc = pthread_create(&tid, NULL, handle_player, (void *)(&connections[idx]));
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
    int remaining = team.shared_lives;
    pthread_mutex_unlock(&team.lives_mutex);
    printf("[TEAM] Lost a life! Remaining: %d\n", remaining);
    return remaining;
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


