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
    Room *spawn = dungeon_get_room(game_dungeon, 0);
    game_state.pos_y[idx] = spawn ? spawn->height / 2 : 0;
    game_state.pos_x[idx] = spawn ? spawn->width  / 2 : 0;
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


