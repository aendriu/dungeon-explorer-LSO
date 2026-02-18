#include "../header/game_state.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>

void free_game_state(GameState *state) {
  if (state == NULL)
    return;
  if (state->adj != NULL) {
    for (int i = 0; i < state->map_size; i++) {
      free(state->adj[i]);
    }
    free(state->adj);
  }
  free(state->players);
  free(state->pos_y);
  free(state->pos_x);
  if (state->room_items != NULL) {
    for (int i = 0; i < state->room_h; i++) {
      free(state->room_items[i]);
    }
    free(state->room_items);
  }
  if (state->room_monsters != NULL) {
    for (int i = 0; i < state->room_h; i++) {
      free(state->room_monsters[i]);
    }
    free(state->room_monsters);
  }
  state->adj = NULL;
  state->players = NULL;
  state->pos_y = NULL;
  state->pos_x = NULL;
  state->room_items = NULL;
  state->room_monsters = NULL;
  state->room_h = 0;
  state->room_w = 0;
  state->map_size = 0;
  state->current_room = 0;
  state->player_id = -1;
}

int parse_game_state(const char *json, GameState *state, char *err_buf,
                     size_t err_len) {
  if (json == NULL || state == NULL)
    return -1;

  memset(state, 0, sizeof(GameState));

  cJSON *root = cJSON_Parse(json);
  if (root == NULL)
    return -1;

  cJSON *err = cJSON_GetObjectItemCaseSensitive(root, "error");
  if (cJSON_IsString(err) && err_buf != NULL && err_len > 0) {
    strncpy(err_buf, err->valuestring, err_len - 1);
    err_buf[err_len - 1] = '\0';
  }

  cJSON *map_size = cJSON_GetObjectItemCaseSensitive(root, "map_size");
  cJSON *current_room = cJSON_GetObjectItemCaseSensitive(root, "current_room");
  cJSON *player_id = cJSON_GetObjectItemCaseSensitive(root, "player_id");
  cJSON *players = cJSON_GetObjectItemCaseSensitive(root, "players");
  cJSON *pos = cJSON_GetObjectItemCaseSensitive(root, "pos");
  cJSON *adj = cJSON_GetObjectItemCaseSensitive(root, "adj");

  if (!cJSON_IsNumber(map_size) || !cJSON_IsNumber(current_room) ||
      !cJSON_IsNumber(player_id) || !cJSON_IsArray(players) ||
      !cJSON_IsArray(adj)) {
    cJSON_Delete(root);
    return -1;
  }

  int size = map_size->valueint;
  if (size <= 0 || size > 64) {
    cJSON_Delete(root);
    return -1;
  }

  state->map_size = size;
  state->current_room = current_room->valueint;
  state->player_id = player_id->valueint;
  state->adj = (int **)calloc(size, sizeof(int *));
  if (state->adj == NULL) {
    cJSON_Delete(root);
    free_game_state(state);
    return -1;
  }

  for (int i = 0; i < size; i++) {
    state->adj[i] = (int *)calloc(size, sizeof(int));
    if (state->adj[i] == NULL) {
      cJSON_Delete(root);
      free_game_state(state);
      return -1;
    }
  }

  int rows = cJSON_GetArraySize(adj);
  for (int i = 0; i < rows && i < size; i++) {
    cJSON *row = cJSON_GetArrayItem(adj, i);
    if (!cJSON_IsArray(row))
      continue;
    int cols = cJSON_GetArraySize(row);
    for (int j = 0; j < cols && j < size; j++) {
      cJSON *cell = cJSON_GetArrayItem(row, j);
      if (cJSON_IsNumber(cell)) {
        state->adj[i][j] = cell->valueint ? 1 : 0;
      }
    }
  }

  int pcount = cJSON_GetArraySize(players);
  state->players = (int *)calloc(MAX_PLAYERS, sizeof(int));
  if (state->players == NULL) {
    cJSON_Delete(root);
    free_game_state(state);
    return -1;
  }
  for (int i = 0; i < MAX_PLAYERS; i++) {
    state->players[i] = -1;
  }
  for (int i = 0; i < pcount && i < MAX_PLAYERS; i++) {
    cJSON *p = cJSON_GetArrayItem(players, i);
    if (cJSON_IsNumber(p)) {
      state->players[i] = p->valueint;
    } else {
      state->players[i] = -1;
    }
  }

  state->pos_y = (int *)calloc(MAX_PLAYERS, sizeof(int));
  state->pos_x = (int *)calloc(MAX_PLAYERS, sizeof(int));
  if (state->pos_y == NULL || state->pos_x == NULL) {
    cJSON_Delete(root);
    free_game_state(state);
    return -1;
  }
  for (int i = 0; i < MAX_PLAYERS; i++) {
    state->pos_y[i] = -1;
    state->pos_x[i] = -1;
  }

  if (cJSON_IsArray(pos)) {
    int pos_count = cJSON_GetArraySize(pos);
    for (int i = 0; i < pos_count && i < MAX_PLAYERS; i++) {
      cJSON *pair = cJSON_GetArrayItem(pos, i);
      if (!cJSON_IsArray(pair))
        continue;
      cJSON *py = cJSON_GetArrayItem(pair, 0);
      cJSON *px = cJSON_GetArrayItem(pair, 1);
      if (cJSON_IsNumber(py) && cJSON_IsNumber(px)) {
        state->pos_y[i] = py->valueint;
        state->pos_x[i] = px->valueint;
      }
    }
  }

  /* Parse room dimensions and item grid */
  cJSON *jroom_h = cJSON_GetObjectItemCaseSensitive(root, "room_h");
  cJSON *jroom_w = cJSON_GetObjectItemCaseSensitive(root, "room_w");
  if (cJSON_IsNumber(jroom_h) && cJSON_IsNumber(jroom_w)) {
    state->room_h = jroom_h->valueint;
    state->room_w = jroom_w->valueint;
  }

  cJSON *jitems = cJSON_GetObjectItemCaseSensitive(root, "room_items");
  if (cJSON_IsArray(jitems) && state->room_h > 0 && state->room_w > 0) {
    state->room_items = (int **)calloc(state->room_h, sizeof(int *));
    if (state->room_items != NULL) {
      for (int y = 0; y < state->room_h; y++) {
        state->room_items[y] = (int *)calloc(state->room_w, sizeof(int));
      }
      int nrows = cJSON_GetArraySize(jitems);
      for (int y = 0; y < nrows && y < state->room_h; y++) {
        cJSON *row = cJSON_GetArrayItem(jitems, y);
        if (!cJSON_IsArray(row))
          continue;
        int ncols = cJSON_GetArraySize(row);
        for (int x = 0; x < ncols && x < state->room_w; x++) {
          cJSON *cell = cJSON_GetArrayItem(row, x);
          if (cJSON_IsNumber(cell)) {
            state->room_items[y][x] = cell->valueint;
          }
        }
      }
    }
  }

  cJSON *jmonsters = cJSON_GetObjectItemCaseSensitive(root, "room_monsters");
  if (cJSON_IsArray(jmonsters) && state->room_h > 0 && state->room_w > 0) {
    state->room_monsters = (int **)calloc(state->room_h, sizeof(int *));
    if (state->room_monsters != NULL) {
      for (int y = 0; y < state->room_h; y++) {
        state->room_monsters[y] = (int *)calloc(state->room_w, sizeof(int));
      }
      int mrows = cJSON_GetArraySize(jmonsters);
      for (int y = 0; y < mrows && y < state->room_h; y++) {
        cJSON *row = cJSON_GetArrayItem(jmonsters, y);
        if (!cJSON_IsArray(row))
          continue;
        int mcols = cJSON_GetArraySize(row);
        for (int x = 0; x < mcols && x < state->room_w; x++) {
          cJSON *cell = cJSON_GetArrayItem(row, x);
          if (cJSON_IsNumber(cell)) {
            state->room_monsters[y][x] = cell->valueint;
          }
        }
      }
    }
  }

  /* Parse quest progress and team lives */
  cJSON *jquest = cJSON_GetObjectItemCaseSensitive(root, "quest_items_collected");
  if (cJSON_IsNumber(jquest))
    state->quest_items_collected = jquest->valueint;

  cJSON *jlives = cJSON_GetObjectItemCaseSensitive(root, "team_lives");
  if (cJSON_IsNumber(jlives))
    state->team_lives = jlives->valueint;

  cJSON *jkills = cJSON_GetObjectItemCaseSensitive(root, "team_kills");
  if (cJSON_IsNumber(jkills))
    state->team_kills = jkills->valueint;

  cJSON *pstats = cJSON_GetObjectItemCaseSensitive(root, "player_stats");
  if (cJSON_IsObject(pstats)) {
    cJSON *pnormal = cJSON_GetObjectItemCaseSensitive(pstats, "normal_items");
    cJSON *pquest = cJSON_GetObjectItemCaseSensitive(pstats, "quest_items");
    cJSON *ptotal = cJSON_GetObjectItemCaseSensitive(pstats, "total_items");
    cJSON *ptraps = cJSON_GetObjectItemCaseSensitive(pstats, "traps_triggered");
    cJSON *pkills = cJSON_GetObjectItemCaseSensitive(pstats, "monsters_killed");

    if (cJSON_IsNumber(pnormal))
      state->player_normal_items = pnormal->valueint;
    if (cJSON_IsNumber(pquest))
      state->player_quest_items = pquest->valueint;
    if (cJSON_IsNumber(ptotal))
      state->player_total_items = ptotal->valueint;
    if (cJSON_IsNumber(ptraps))
      state->player_traps_triggered = ptraps->valueint;
    if (cJSON_IsNumber(pkills))
      state->player_monsters_killed = pkills->valueint;
  }

  cJSON_Delete(root);
  return 0;
}

void get_adjacent_rooms(const GameState *state, int *neighbors, int *count) {
  *count = 0;
  if (!state || !state->adj)
    return;
  if (state->current_room < 0 || state->current_room >= state->map_size)
    return;
  for (int i = 0; i < state->map_size; i++) {
    if (state->adj[state->current_room][i]) {
      neighbors[*count] = i;
      (*count)++;
      if (*count >= 4)
        break;
    }
  }
}

int door_target_for_dir(const GameState *state, int dir) {
  int neighbors[4] = {-1, -1, -1, -1};
  int count = 0;
  get_adjacent_rooms(state, neighbors, &count);
  if (dir < 0 || dir > 3)
    return -1;
  if (dir >= count)
    return -1;
  return neighbors[dir];
}
