#ifndef CLIENT_GAME_STATE_H
#define CLIENT_GAME_STATE_H

#include <stddef.h>

enum { MAX_PLAYERS = 4 };

typedef struct {
  int map_size;
  int current_room;
  int player_id;
  int **adj;
  int *players;
  int *pos_y;
  int *pos_x;
  int room_h;
  int room_w;
  int **room_items;
  int **room_monsters;
  int quest_items_collected;
  int team_lives;
  int team_kills;
  int player_normal_items;
  int player_quest_items;
  int player_total_items;
  int player_traps_triggered;
  int player_monsters_killed;
} GameState;

void free_game_state(GameState *state);
int parse_game_state(const char *json, GameState *state, char *err_buf,
                     size_t err_len);
int wall_dir(const GameState *state, int new_y, int new_x);
int door_target_for_dir(const GameState *state, int dir);

#endif
