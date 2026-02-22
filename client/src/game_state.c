#include "../header/game_state.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>

/*
 * Libera una griglia 2D di interi.
 * Assume che ogni riga sia stata allocata separatamente con calloc/malloc.
 */
static void free_int_grid(int **grid, int rows) {
  if (grid == NULL) return;
  for (int i = 0; i < rows; i++)
    free(grid[i]);
  free(grid);
}

/*
 * Rilascia tutte le risorse dinamiche contenute in GameState
 * e reimposta i campi a zero, mantenendo player_id a -1.
 */
void free_game_state(GameState *state) {
  if (state == NULL)
    return;
  free_int_grid(state->adj, state->map_size);
  free(state->players);
  free(state->pos_y);
  free(state->pos_x);
  free_int_grid(state->room_items, state->room_h);
  free_int_grid(state->room_monsters, state->room_h);
  memset(state, 0, sizeof(GameState));
  state->player_id = -1;
}

/* ---------- JSON parse helpers ---------- */

/*
 * Estrae eventuale stringa di errore dal JSON del server.
 * Se presente, copia in err_buf troncando a err_len.
 */
static void parse_error(const cJSON *root, char *err_buf, size_t err_len) {
  cJSON *err = cJSON_GetObjectItemCaseSensitive(root, "error");
  if (cJSON_IsString(err) && err_buf != NULL && err_len > 0) {
    strncpy(err_buf, err->valuestring, err_len - 1);
    err_buf[err_len - 1] = '\0';
  }
}

/*
 * Parsa i campi base obbligatori: map_size, current_room, player_id.
 * Valida map_size per evitare allocazioni spropositate.
 */
static int parse_base_fields(const cJSON *root, GameState *state) {
  cJSON *map_size = cJSON_GetObjectItemCaseSensitive(root, "map_size");
  cJSON *current_room = cJSON_GetObjectItemCaseSensitive(root, "current_room");
  cJSON *player_id = cJSON_GetObjectItemCaseSensitive(root, "player_id");

  if (!cJSON_IsNumber(map_size) || !cJSON_IsNumber(current_room) ||
      !cJSON_IsNumber(player_id))
    return -1;
  //assegnazione dei campi base, con validazione di map_size
  int size = map_size->valueint;
  if (size <= 0 || size > 64)
    return -1;

  state->map_size = size;
  state->current_room = current_room->valueint;
  state->player_id = player_id->valueint;
  return 0;
}

/*
 * Parsa un array JSON 2D in una matrice int**.
 * Celle non numeriche restano a 0; righe/colonne in eccesso vengono ignorate.
 */
static int **parse_int_grid(const cJSON *jarr, int rows, int cols) {
  if (!cJSON_IsArray(jarr) || rows <= 0 || cols <= 0)
    return NULL;

  int **grid = (int **)calloc(rows, sizeof(int *));
  if (grid == NULL)
    return NULL;

  for (int y = 0; y < rows; y++) {
    grid[y] = (int *)calloc(cols, sizeof(int));
    if (grid[y] == NULL)
      return grid; /* la free_game_state gestirà la pulizia */
  }

  int nrows = cJSON_GetArraySize(jarr);
  for (int y = 0; y < nrows && y < rows; y++) {
    cJSON *row = cJSON_GetArrayItem(jarr, y);
    if (!cJSON_IsArray(row))
      continue;
    int ncols = cJSON_GetArraySize(row);
    for (int x = 0; x < ncols && x < cols; x++) {
      cJSON *cell = cJSON_GetArrayItem(row, x);
      if (cJSON_IsNumber(cell))
        grid[y][x] = cell->valueint;
    }
  }
  return grid;
}

/*
 * Parsa la matrice di adiacenza della mappa (map_size x map_size).
 * Ogni cella indica la presenza di un arco tra due stanze.
 */
static int parse_adjacency(const cJSON *root, GameState *state) {
  cJSON *adj = cJSON_GetObjectItemCaseSensitive(root, "adj");
  if (!cJSON_IsArray(adj))
    return -1;

  int size = state->map_size;
  state->adj = parse_int_grid(adj, size, size);
  if (state->adj == NULL)
    return -1;
  return 0;
}

/*
 * Parsa le stanze in cui si trovano i giocatori.
 * Inizializza a -1 gli slot non presenti nel JSON.
 */
static int parse_player_positions(const cJSON *root, GameState *state) {
  cJSON *players = cJSON_GetObjectItemCaseSensitive(root, "players");
  if (!cJSON_IsArray(players))
    return -1;

  state->players = (int *)calloc(MAX_PLAYERS, sizeof(int));
  if (state->players == NULL)
    return -1;

  for (int i = 0; i < MAX_PLAYERS; i++)
    state->players[i] = -1;

  int pcount = cJSON_GetArraySize(players);
  for (int i = 0; i < pcount && i < MAX_PLAYERS; i++) {
    cJSON *p = cJSON_GetArrayItem(players, i);
    state->players[i] = cJSON_IsNumber(p) ? p->valueint : -1;
  }
  return 0;
}

/*
 * Parsa le coordinate (y, x) di ogni giocatore nella stanza.
 * Il campo pos e' opzionale: se manca, lascia coordinate a -1.
 */
static int parse_player_coords(const cJSON *root, GameState *state) {
  state->pos_y = (int *)calloc(MAX_PLAYERS, sizeof(int));
  state->pos_x = (int *)calloc(MAX_PLAYERS, sizeof(int));
  if (state->pos_y == NULL || state->pos_x == NULL)
    return -1;

  for (int i = 0; i < MAX_PLAYERS; i++) {
    state->pos_y[i] = -1;
    state->pos_x[i] = -1;
  }

  cJSON *pos = cJSON_GetObjectItemCaseSensitive(root, "pos");
  if (!cJSON_IsArray(pos))
    return 0; /* pos è opzionale */

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
  return 0;
}

/*
 * Parsa dimensioni stanza e griglie item/mostri.
 * Se room_h/room_w non sono validi, non tocca le griglie.
 */
static void parse_room(const cJSON *root, GameState *state) {
  cJSON *jroom_h = cJSON_GetObjectItemCaseSensitive(root, "room_h");
  cJSON *jroom_w = cJSON_GetObjectItemCaseSensitive(root, "room_w");
  if (!cJSON_IsNumber(jroom_h) || !cJSON_IsNumber(jroom_w))
    return;

  state->room_h = jroom_h->valueint;
  state->room_w = jroom_w->valueint;

  cJSON *jitems = cJSON_GetObjectItemCaseSensitive(root, "room_items");
  state->room_items = parse_int_grid(jitems, state->room_h, state->room_w);

  cJSON *jmonsters = cJSON_GetObjectItemCaseSensitive(root, "room_monsters");
  state->room_monsters = parse_int_grid(jmonsters, state->room_h, state->room_w);
}

/*
 * Parsa statistiche del team, se presenti.
 * Campi mancanti restano con il valore di default (0).
 */
static void parse_team_stats(const cJSON *root, GameState *state) {
  cJSON *jquest = cJSON_GetObjectItemCaseSensitive(root, "quest_items_collected");
  if (cJSON_IsNumber(jquest))
    state->quest_items_collected = jquest->valueint;

  cJSON *jlives = cJSON_GetObjectItemCaseSensitive(root, "team_lives");
  if (cJSON_IsNumber(jlives))
    state->team_lives = jlives->valueint;

  cJSON *jkills = cJSON_GetObjectItemCaseSensitive(root, "team_kills");
  if (cJSON_IsNumber(jkills))
    state->team_kills = jkills->valueint;
}

/*
 * Parsa statistiche individuali del giocatore.
 * Richiede che player_stats sia un oggetto JSON.
 */
static void parse_player_stats(const cJSON *root, GameState *state) {
  cJSON *pstats = cJSON_GetObjectItemCaseSensitive(root, "player_stats");
  if (!cJSON_IsObject(pstats))
    return;

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

/*
 * Decodifica il JSON del server e popola la struttura GameState.
 * Ritorna 0 se ok, -1 se errore di parsing o campi obbligatori mancanti.
 */
int parse_game_state(const char *json, GameState *state, char *err_buf,
                     size_t err_len) {
  if (json == NULL || state == NULL)
    return -1;

  memset(state, 0, sizeof(GameState));

  cJSON *root = cJSON_Parse(json);
  if (root == NULL)
    return -1;

  parse_error(root, err_buf, err_len);

  /* Parsing dei campi obbligatori — se uno fallisce, pulizia e uscita */
  int ok = (parse_base_fields(root, state) == 0 &&
            parse_adjacency(root, state) == 0 &&
            parse_player_positions(root, state) == 0 &&
            parse_player_coords(root, state) == 0);

  if (!ok) {
    cJSON_Delete(root);
    free_game_state(state);
    return -1;
  }

  parse_room(root, state);
  parse_team_stats(root, state);
  parse_player_stats(root, state);

  cJSON_Delete(root);
  return 0;
}

/*
 * Trova le stanze adiacenti a quella corrente.
 * Ritorna al massimo 4 vicini (N,E,S,W) nell'array neighbors.
 */
static void get_adjacent_rooms(const GameState *state, int *neighbors, int *count) {
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

/*
 * Direzione del muro toccato: 0=N, 1=E, 2=S, 3=W.
 * Ritorna -1 se la posizione non e' in corrispondenza di una porta.
 */
int wall_dir(const GameState *state, int new_y, int new_x) {
  if (new_y == 0 && new_x == state->room_w / 2)
    return 0; /* N */
  if (new_x == state->room_w - 1 && new_y == state->room_h / 2)
    return 1; /* E */
  if (new_y == state->room_h - 1 && new_x == state->room_w / 2)
    return 2; /* S */
  if (new_x == 0 && new_y == state->room_h / 2)
    return 3; /* W */
  return -1;
}

/*
 * Data una direzione, ritorna la stanza raggiungibile o -1.
 * Usa l'ordine dei vicini restituito da get_adjacent_rooms.
 */
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
