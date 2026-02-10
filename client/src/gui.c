#include "../header/gui.h"
#include "cJSON.h"

int start_y, start_x;

typedef struct {
  int map_size;
  int current_room;
  int player_id;
  int **adj;
  int *players;
  int *pos_y;
  int *pos_x;
} GameState;

static void free_game_state(GameState *state) {
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
  state->adj = NULL;
  state->players = NULL;
  state->pos_y = NULL;
  state->pos_x = NULL;
  state->map_size = 0;
  state->current_room = 0;
  state->player_id = -1;
}

static int parse_game_state(const char *json, GameState *state, char *err_buf,
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
  int max_players = 4;
  state->players = (int *)calloc(max_players, sizeof(int));
  if (state->players == NULL) {
    cJSON_Delete(root);
    free_game_state(state);
    return -1;
  }
  for (int i = 0; i < max_players; i++) {
    state->players[i] = -1;
  }
  for (int i = 0; i < pcount && i < max_players; i++) {
    cJSON *p = cJSON_GetArrayItem(players, i);
    if (cJSON_IsNumber(p)) {
      state->players[i] = p->valueint;
    } else {
      state->players[i] = -1;
    }
  }

  state->pos_y = (int *)calloc(max_players, sizeof(int));
  state->pos_x = (int *)calloc(max_players, sizeof(int));
  if (state->pos_y == NULL || state->pos_x == NULL) {
    cJSON_Delete(root);
    free_game_state(state);
    return -1;
  }
  for (int i = 0; i < max_players; i++) {
    state->pos_y[i] = -1;
    state->pos_x[i] = -1;
  }

  if (cJSON_IsArray(pos)) {
    int pos_count = cJSON_GetArraySize(pos);
    for (int i = 0; i < pos_count && i < max_players; i++) {
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

  cJSON_Delete(root);
  return 0;
}

static void get_adjacent_rooms(const GameState *state, int *neighbors,
                               int *count) {
  *count = 0;
  if (!state || !state->adj)
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

static int door_target_for_dir(const GameState *state, int dir) {
  int neighbors[4] = {-1, -1, -1, -1};
  int count = 0;
  get_adjacent_rooms(state, neighbors, &count);
  if (dir < 0 || dir > 3)
    return -1;
  if (dir >= count)
    return -1;
  return neighbors[dir];
}

static void render_room(WINDOW *win, const GameState *state, int py, int px,
                        const char *msg) {
  const int room_h = 9;
  const int room_w = 21;
  int win_h, win_w;
  getmaxyx(win, win_h, win_w);
  int top = (win_h - room_h) / 2;
  int left = (win_w - room_w) / 2;
  if (top < 4)
    top = 4;
  if (left < 2)
    left = 2;

  mvwprintw(win, 1, 2, "DUNGEON - Room %d (Player %d)",
            state->current_room, state->player_id + 1);
  mvwprintw(win, 2, 2, "WASD/frecce per muovere, q per uscire");
  if (msg != NULL) {
    mvwprintw(win, 3, 2, "%s", msg);
  }

  for (int y = 0; y < room_h; y++) {
    for (int x = 0; x < room_w; x++) {
      int wy = top + y;
      int wx = left + x;
      if (wy >= win_h - 1 || wx >= win_w - 1)
        continue;
      char ch = ' ';
      if (y == 0 || y == room_h - 1 || x == 0 || x == room_w - 1) {
        ch = '#';
      }
      mvwaddch(win, wy, wx, ch);
    }
  }

  int neighbors[4] = {-1, -1, -1, -1};
  int count = 0;
  get_adjacent_rooms(state, neighbors, &count);

  int door_y_n = top + 0;
  int door_x_n = left + room_w / 2;
  int door_y_s = top + room_h - 1;
  int door_x_s = left + room_w / 2;
  int door_y_w = top + room_h / 2;
  int door_x_w = left + 0;
  int door_y_e = top + room_h / 2;
  int door_x_e = left + room_w - 1;

  if (door_target_for_dir(state, 0) != -1)
    mvwaddch(win, door_y_n, door_x_n, '+');
  if (door_target_for_dir(state, 1) != -1)
    mvwaddch(win, door_y_e, door_x_e, '+');
  if (door_target_for_dir(state, 2) != -1)
    mvwaddch(win, door_y_s, door_x_s, '+');
  if (door_target_for_dir(state, 3) != -1)
    mvwaddch(win, door_y_w, door_x_w, '+');

  char self_sym = (state->player_id >= 0 && state->player_id < 4)
                      ? (char)('1' + state->player_id)
                      : '@';
  mvwaddch(win, top + py, left + px, self_sym);

  if (state->players != NULL) {
    const int slots[4][2] = {
        {room_h / 2, room_w / 2}, {room_h / 2, room_w / 2 - 2},
        {room_h / 2, room_w / 2 + 2}, {room_h / 2 + 2, room_w / 2}};
    bool occupied[9][21] = {0};
    occupied[py][px] = true;

    for (int i = 0; i < 4; i++) {
      if (i == state->player_id)
        continue;
      if (state->players[i] != state->current_room)
        continue;

      int oy = slots[i][0];
      int ox = slots[i][1];
      bool placed = false;

      if (state->pos_y != NULL && state->pos_x != NULL) {
        int py_other = state->pos_y[i];
        int px_other = state->pos_x[i];
        if (py_other >= 1 && py_other < room_h - 1 && px_other >= 1 &&
            px_other < room_w - 1 && !occupied[py_other][px_other]) {
          oy = py_other;
          ox = px_other;
          occupied[oy][ox] = true;
          placed = true;
        }
      }

      if (!placed) {
        if (oy >= 1 && oy < room_h - 1 && ox >= 1 && ox < room_w - 1 &&
            !occupied[oy][ox]) {
          occupied[oy][ox] = true;
          placed = true;
        }
      }

      if (!placed) {
        for (int y = 1; y < room_h - 1 && !placed; y++) {
          for (int x = 1; x < room_w - 1; x++) {
            if (!occupied[y][x]) {
              oy = y;
              ox = x;
              occupied[oy][ox] = true;
              placed = true;
              break;
            }
          }
        }
      }

      char sym = (char)('1' + i);
      mvwaddch(win, top + oy, left + ox, sym);
    }
  }

  int list_y = top + room_h + 1;
  mvwprintw(win, list_y, 2, "Porte: N=%d E=%d S=%d W=%d",
            door_target_for_dir(state, 0), door_target_for_dir(state, 1),
            door_target_for_dir(state, 2), door_target_for_dir(state, 3));
}

static void render_game(WINDOW *win, const GameState *state, int py, int px,
                        const char *msg) {
  werase(win);
  box(win, 0, 0);
  render_room(win, state, py, px, msg);
  wrefresh(win);
}

static int game_screen(GameState *state) {
  int win_h = start_y - 2;
  int win_w = start_x - 2;
  if (win_h < 10)
    win_h = 10;
  if (win_w < 40)
    win_w = 40;

  WINDOW *win = newwin(win_h, win_w, 1, 1);
  keypad(win, TRUE);
  wclear(win);
  wrefresh(win);

  const int room_h = 9;
  const int room_w = 21;
  int py = room_h / 2;
  int px = room_w / 2;

  char status[128] = {0};
  render_game(win, state, py, px, NULL);

  while (1) {
    int ch = wgetch(win);
    if (ch == 'q' || ch == 'Q') {
      break;
    }
    int dy = 0, dx = 0;
    if (ch == 'w' || ch == 'W' || ch == KEY_UP)
      dy = -1;
    else if (ch == 's' || ch == 'S' || ch == KEY_DOWN)
      dy = 1;
    else if (ch == 'a' || ch == 'A' || ch == KEY_LEFT)
      dx = -1;
    else if (ch == 'd' || ch == 'D' || ch == KEY_RIGHT)
      dx = 1;
    else
      continue;

    int new_y = py + dy;
    int new_x = px + dx;
    int at_wall = (new_y <= 0 || new_y >= room_h - 1 || new_x <= 0 ||
                   new_x >= room_w - 1);
    if (at_wall) {
      int dir = -1;
      if (new_y == 0 && new_x == room_w / 2)
        dir = 0; /* N */
      else if (new_x == room_w - 1 && new_y == room_h / 2)
        dir = 1; /* E */
      else if (new_y == room_h - 1 && new_x == room_w / 2)
        dir = 2; /* S */
      else if (new_x == 0 && new_y == room_h / 2)
        dir = 3; /* W */

      int target = door_target_for_dir(state, dir);
      if (target == -1) {
        char *poll = sendnrecv(GET_GAME_STATE);
        if (poll == NULL) {
          snprintf(status, sizeof(status), "Server non risponde");
          render_game(win, state, py, px, status);
          continue;
        }

        GameState updated = {0};
        char parse_err[64] = {0};
        if (parse_game_state(poll, &updated, parse_err, sizeof(parse_err)) < 0) {
          snprintf(status, sizeof(status), "Errore parsing risposta");
          free(poll);
          render_game(win, state, py, px, status);
          continue;
        }

        free(poll);
        free_game_state(state);
        *state = updated;
        if (state->players && state->player_id >= 0 && state->player_id < 4) {
          state->current_room = state->players[state->player_id];
        }

        if (state->pos_y && state->pos_x && state->player_id >= 0 &&
            state->player_id < 4) {
          int sy = state->pos_y[state->player_id];
          int sx = state->pos_x[state->player_id];
          if (sy >= 1 && sy < room_h - 1 && sx >= 1 && sx < room_w - 1) {
            py = sy;
            px = sx;
          }
        }

        if (parse_err[0] != '\0') {
          snprintf(status, sizeof(status), "Errore: %s", parse_err);
          render_game(win, state, py, px, status);
          continue;
        }

        snprintf(status, sizeof(status), "Porta chiusa");
        render_game(win, state, py, px, status);
        continue;
      }

      char payload[64];
      snprintf(payload, sizeof(payload), "{\"target\":%d}", target);
      char *reply = sendnrecv_payload(MOVE_PLAYER, payload);
      if (reply == NULL) {
        snprintf(status, sizeof(status), "Server non risponde");
        render_game(win, state, py, px, status);
        continue;
      }

      GameState updated = {0};
      char parse_err[64] = {0};
      if (parse_game_state(reply, &updated, parse_err, sizeof(parse_err)) < 0) {
        snprintf(status, sizeof(status), "Errore parsing risposta");
        free(reply);
        render_game(win, state, py, px, status);
        continue;
      }

      free(reply);
      free_game_state(state);
      *state = updated;
      if (state->players && state->player_id >= 0 && state->player_id < 4) {
        state->current_room = state->players[state->player_id];
      }

      py = room_h / 2;
      px = room_w / 2;
      if (state->pos_y && state->pos_x && state->player_id >= 0 &&
          state->player_id < 4) {
        int sy = state->pos_y[state->player_id];
        int sx = state->pos_x[state->player_id];
        if (sy >= 1 && sy < room_h - 1 && sx >= 1 && sx < room_w - 1) {
          py = sy;
          px = sx;
        }
      }
      if (parse_err[0] != '\0') {
        snprintf(status, sizeof(status), "Errore: %s", parse_err);
      } else {
        snprintf(status, sizeof(status), "Sei entrato nella stanza %d", target);
      }
      render_game(win, state, py, px, status);
      continue;
    }

    py = new_y;
    px = new_x;
    char payload[64];
    snprintf(payload, sizeof(payload), "{\"y\":%d,\"x\":%d}", py, px);
    char *reply = sendnrecv_payload(UPDATE_PLAYER_POSITION, payload);
    if (reply == NULL) {
      snprintf(status, sizeof(status), "Server non risponde");
      render_game(win, state, py, px, status);
      continue;
    }

    GameState updated = {0};
    char parse_err[64] = {0};
    if (parse_game_state(reply, &updated, parse_err, sizeof(parse_err)) < 0) {
      snprintf(status, sizeof(status), "Errore parsing risposta");
      free(reply);
      render_game(win, state, py, px, status);
      continue;
    }

    free(reply);
    free_game_state(state);
    *state = updated;
    if (state->players && state->player_id >= 0 && state->player_id < 4) {
      state->current_room = state->players[state->player_id];
    }

    if (state->pos_y && state->pos_x && state->player_id >= 0 &&
        state->player_id < 4) {
      int sy = state->pos_y[state->player_id];
      int sx = state->pos_x[state->player_id];
      if (sy >= 1 && sy < room_h - 1 && sx >= 1 && sx < room_w - 1) {
        py = sy;
        px = sx;
      }
    }

    if (parse_err[0] != '\0') {
      snprintf(status, sizeof(status), "Errore: %s", parse_err);
      render_game(win, state, py, px, status);
      continue;
    }

    render_game(win, state, py, px, NULL);
  }

  delwin(win);
  return 0;
}

void init_ncurses() {
  initscr();
  cbreak();
  noecho();
  curs_set(0);
  keypad(stdscr, TRUE);
  getmaxyx(stdscr, start_y, start_x);
}

/*
    Welcome menu è la prima schermata.
    Welcome menu
        entra -> promt_server_adress
        esci
*/

UserChoice welcome_menu() {

  int height = 7;
  int width = 22;
  int y = (start_y - height) / 2;
  int x = (start_x - width) / 2;

  WINDOW *menu = newwin(height, width, y, x);
  keypad(menu, TRUE);

  const char *title = "DUNGEON CRAWLER";
  const char *by_line = "by";
  const char *credits = "Antimoandrea di luise & Adriano di Giovanni";

  int choice = 0;
  int ch;

  while (1) {
    erase();

    int title_x = x + (width - (int)strlen(title)) / 2;
    if (title_x < 0)
      title_x = 0;
    int title_y = y - 2;
    if (title_y < 0)
      title_y = 0;

    attron(A_BOLD);
    mvprintw(title_y, title_x, "%s", title);
    attroff(A_BOLD);

    int by_x = x + (width - (int)strlen(by_line)) / 2;
    if (by_x < 0)
      by_x = 0;
    int by_y = y + height + 1;
    if (by_y < start_y) {
      mvprintw(by_y, by_x, "%s", by_line);

      int credits_x = x + (width - (int)strlen(credits)) / 2;
      if (credits_x < 0)
        credits_x = 0;
      if (by_y + 1 < start_y) {
        mvprintw(by_y + 1, credits_x, "%s", credits);
      }
    }

    refresh();

    werase(menu);
    box(menu, 0, 0);

    if (choice == 0)
      wattron(menu, A_REVERSE);
    mvwprintw(menu, 1, 2, "entra in lobby");
    wattroff(menu, A_REVERSE);

    if (choice == 1)
      wattron(menu, A_REVERSE);
    mvwprintw(menu, 2, 2, "esci");
    wattroff(menu, A_REVERSE);

    wrefresh(menu);

    ch = wgetch(menu);

    switch (ch) {
    case KEY_UP:
      choice = (choice + 2) % 2;
      break;
    case KEY_DOWN:
      choice = (choice + 1) % 2;
      break;
    case '\n':
      delwin(menu);
      return (UserChoice)choice;
    }
  }
}

/*
    Se l'utente non inserisce ip o porta allora
    si usano parametri di def (localhost, 9090)
 */
int prompt_server_address(char *host, size_t host_sz, char *port,
                          size_t port_sz) {
  if (host == NULL || port == NULL || host_sz == 0 || port_sz == 0) {
    return -1;
  }

  echo();
  curs_set(1);

  erase();
  const char *title = "DUNGEON CRAWLER";
  const char *subtitle = "Inserisci IP e porta del server";

  int title_x = (start_x - (int)strlen(title)) / 2;
  if (title_x < 0)
    title_x = 0;
  int subtitle_x = (start_x - (int)strlen(subtitle)) / 2;
  if (subtitle_x < 0)
    subtitle_x = 0;

  attron(A_BOLD);
  mvprintw(1, title_x, "%s", title);
  attroff(A_BOLD);
  mvprintw(3, subtitle_x, "%s", subtitle);

  int form_y = 6;
  int form_x = 4;
  if (start_x >= 60) {
    form_x = (start_x - 52) / 2;
    if (form_x < 0)
      form_x = 0;
  }

  char ip_buf[256] = {0};
  char port_buf[64] = {0};

  mvprintw(form_y, form_x, "IP [%s]: ", IP_SERVER);
  move(form_y, form_x + 5 + (int)strlen(IP_SERVER) + 3);
  getnstr(ip_buf, (int)sizeof(ip_buf) - 1);

  mvprintw(form_y + 2, form_x, "Porta [%s]: ", PORT);
  move(form_y + 2, form_x + 8 + (int)strlen(PORT) + 3);
  getnstr(port_buf, (int)sizeof(port_buf) - 1);

  if (ip_buf[0] == '\0') {
    strncpy(host, IP_SERVER, host_sz - 1);
    host[host_sz - 1] = '\0';
  } else {
    strncpy(host, ip_buf, host_sz - 1);
    host[host_sz - 1] = '\0';
  }

  if (port_buf[0] == '\0') {
    strncpy(port, PORT, port_sz - 1);
    port[port_sz - 1] = '\0';
  } else {
    strncpy(port, port_buf, port_sz - 1);
    port[port_sz - 1] = '\0';
  }

  noecho();
  curs_set(0);

  return 0;
}

/*
    Lobby screen: 4 colonne (1 per giocatore).
    Il giocatore 1 ha il pulsante "Inizia".
*/
static int parse_connected_players(const char *reply) {
  if (reply == NULL)
    return -1;

  while (*reply == ' ' || *reply == '\t' || *reply == '\r' || *reply == '\n')
    reply++;
  if (*reply < '0' || *reply > '9')
    return -1;

  int n = atoi(reply);
  if (n < 0)
    n = 0;
  if (n > 4)
    n = 4;
  return n;
}

int lobby_screen(void) {
  keypad(stdscr, TRUE);

  const char *title = "LOBBY";
  const char *hint = "In attesa di giocatori...";

  int win_h = start_y - 6;
  int win_w = start_x - 6;
  if (win_h < 10)
    win_h = 10;
  if (win_w < 50)
    win_w = 50;

  int win_y = (start_y - win_h) / 2;
  int win_x = (start_x - win_w) / 2;
  if (win_y < 0)
    win_y = 0;
  if (win_x < 0)
    win_x = 0;

  WINDOW *win = newwin(win_h, win_w, win_y, win_x);
  keypad(win, TRUE);
  wtimeout(win, 400);

  int connected_players = 0;
  int tick = 0;

  while (1) {
    /* Poll server every ~2 ticks (about 800ms) */
    if ((tick++ % 2) == 0) {
      Message msg = GET_N_OF_CONNECTED_PLAYERS;
      char *reply = sendnrecv(msg);
      if (reply == NULL) {
        werase(win);
        box(win, 0, 0);
        mvwprintw(win, 2, 2, "Connessione persa o server non risponde.");
        mvwprintw(win, 3, 2, "Premi un tasto per tornare al menu.");
        wrefresh(win);
        wgetch(win);
        delwin(win);
        return -1;
      }

      int parsed = parse_connected_players(reply);
      if (parsed < 0) {
        /* Likely server full message or protocol mismatch */
        werase(win);
        box(win, 0, 0);
        mvwprintw(win, 2, 2, "Impossibile entrare in lobby.");
        mvwprintw(win, 3, 2, "%.*s", 60, reply);
        mvwprintw(win, 5, 2, "Premi un tasto per tornare al menu.");
        wrefresh(win);
        free(reply);
        wgetch(win);
        delwin(win);
        return -1;
      }

      connected_players = parsed;
      free(reply);
    }

    werase(win);
    box(win, 0, 0);

    int title_x = (win_w - (int)strlen(title)) / 2;
    if (title_x < 1)
      title_x = 1;
    wattron(win, A_BOLD);
    mvwprintw(win, 1, title_x, "%s", title);
    wattroff(win, A_BOLD);

    int hint_x = (win_w - (int)strlen(hint)) / 2;
    if (hint_x < 1)
      hint_x = 1;
    mvwprintw(win, 2, hint_x, "%s", hint);

    int inner_w = win_w - 2;
    int col_w = inner_w / 4;
    if (col_w < 10)
      col_w = 10;

    int sep1 = 1 + col_w;
    int sep2 = 1 + col_w * 2;
    int sep3 = 1 + col_w * 3;

    for (int r = 3; r < win_h - 1; r++) {
      if (sep1 < win_w - 1)
        mvwaddch(win, r, sep1, ACS_VLINE);
      if (sep2 < win_w - 1)
        mvwaddch(win, r, sep2, ACS_VLINE);
      if (sep3 < win_w - 1)
        mvwaddch(win, r, sep3, ACS_VLINE);
    }

    mvwprintw(win, 3, 2, "Connessi: %d/4", connected_players);
    mvwprintw(win, 4, 2, "Giocatore 1");
    mvwprintw(win, 4, sep1 + 2, "Giocatore 2");
    mvwprintw(win, 4, sep2 + 2, "Giocatore 3");
    mvwprintw(win, 4, sep3 + 2, "Giocatore 4");

    mvwprintw(win, 5, 2, "%s", (connected_players >= 1) ? "Connesso" : "Vuoto");
    mvwprintw(win, 5, sep1 + 2, "%s",
              (connected_players >= 2) ? "Connesso" : "Vuoto");
    mvwprintw(win, 5, sep2 + 2, "%s",
              (connected_players >= 3) ? "Connesso" : "Vuoto");
    mvwprintw(win, 5, sep3 + 2, "%s",
              (connected_players >= 4) ? "Connesso" : "Vuoto");

    /* The start button is shown (activation logic will be tied to player_id
     * later). */
    wattron(win, A_REVERSE);
    mvwprintw(win, 7, 4, "[ Inizia ]");
    wattroff(win, A_REVERSE);

    wrefresh(win);

    int ch = wgetch(win);
    if (ch == '\n' || ch == KEY_ENTER) {
      char *reply = sendnrecv(START_GAME);
      if (reply == NULL) {
        werase(win);
        box(win, 0, 0);
        mvwprintw(win, 2, 2, "Server non risponde all'avvio partita.");
        mvwprintw(win, 3, 2, "Premi un tasto per tornare al menu.");
        wrefresh(win);
        wgetch(win);
        delwin(win);
        return -1;
      }

      GameState state = {0};
      char parse_err[64] = {0};
      if (parse_game_state(reply, &state, parse_err, sizeof(parse_err)) < 0) {
        werase(win);
        box(win, 0, 0);
        mvwprintw(win, 2, 2, "Errore nel parsing dello stato di gioco.");
        mvwprintw(win, 3, 2, "Premi un tasto per tornare al menu.");
        wrefresh(win);
        free(reply);
        wgetch(win);
        delwin(win);
        return -1;
      }

      if (parse_err[0] != '\0') {
        werase(win);
        box(win, 0, 0);
        mvwprintw(win, 2, 2, "Errore avvio partita: %s", parse_err);
        mvwprintw(win, 3, 2, "Premi un tasto per tornare al menu.");
        wrefresh(win);
        free(reply);
        free_game_state(&state);
        wgetch(win);
        delwin(win);
        return -1;
      }

      free(reply);
      delwin(win);
      clear();
      refresh();
      if (state.players && state.player_id >= 0 && state.player_id < 4) {
        state.current_room = state.players[state.player_id];
      }
      game_screen(&state);
      free_game_state(&state);
      return -1;
    }
  }

  delwin(win);
  return 0;
}
