#include "../header/gui.h"
#include "cJSON.h"
#include "../header/game_state.h"
#include "../header/ui_helpers.h"

int start_y, start_x;

static bool is_move_key(int ch, int *dy, int *dx) {
  *dy = 0;
  *dx = 0;
  if (ch == 'w' || ch == 'W' || ch == KEY_UP)
    *dy = -1;
  else if (ch == 's' || ch == 'S' || ch == KEY_DOWN)
    *dy = 1;
  else if (ch == 'a' || ch == 'A' || ch == KEY_LEFT)
    *dx = -1;
  else if (ch == 'd' || ch == 'D' || ch == KEY_RIGHT)
    *dx = 1;
  else
    return false;
  return true;
}

static int wall_dir(const GameState *state, int new_y, int new_x) {
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

static void sync_self_pos(const GameState *state, int *py, int *px) {
  if (state->pos_y && state->pos_x && state->player_id >= 0 &&
      state->player_id < MAX_PLAYERS) {
    int sy = state->pos_y[state->player_id];
    int sx = state->pos_x[state->player_id];
    if (sy >= 1 && sy < state->room_h - 1 && sx >= 1 && sx < state->room_w - 1) {
      *py = sy;
      *px = sx;
    }
  }
}

static int update_state_from_reply(GameState *state, char *reply, char *status,
                                   size_t status_len, int *py, int *px) {
  GameState updated = {0};
  char parse_err[64] = {0};
  if (parse_game_state(reply, &updated, parse_err, sizeof(parse_err)) < 0) {
    snprintf(status, status_len, "Errore parsing risposta");
    free(reply);
    return -1;
  }

  free(reply);
  free_game_state(state);
  *state = updated;
  if (state->players && state->player_id >= 0 &&
      state->player_id < MAX_PLAYERS) {
    state->current_room = state->players[state->player_id];
  }
  sync_self_pos(state, py, px);

  if (parse_err[0] != '\0') {
    snprintf(status, status_len, "Errore: %s", parse_err);
    return -1;
  }

  return 0;
}

static void render_minimap(WINDOW *win, const GameState *state, int top, int left);
static int show_end_stats(const GameState *state, bool won);

static void render_room(WINDOW *win, const GameState *state, int py, int px,
                        const char *msg) {
  int win_h, win_w;
  getmaxyx(win, win_h, win_w);
  int rh = state->room_h;
  int rw = state->room_w;
  int top = (win_h - rh) / 2;
  int left = (win_w - rw) / 2;
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

  for (int y = 0; y < rh; y++) {
    for (int x = 0; x < rw; x++) {
      int wy = top + y;
      int wx = left + x;
      if (wy >= win_h - 1 || wx >= win_w - 1)
        continue;
      char ch = ' ';
      if (y == 0 || y == rh - 1 || x == 0 || x == rw - 1) {
        ch = '#';
      } else if (state->room_monsters != NULL &&
                 y < state->room_h && x < state->room_w &&
                 state->room_monsters[y][x] != 0) {
        ch = 'M'; /* monster */
      } else if (state->room_items != NULL &&
                 y < state->room_h && x < state->room_w &&
                 state->room_items[y][x] != 0) {
        ch = '?'; /* item */
      }
      mvwaddch(win, wy, wx, ch);
    }
  }

  int door_y_n = top + 0;
  int door_x_n = left + rw / 2;
  int door_y_s = top + rh - 1;
  int door_x_s = left + rw / 2;
  int door_y_w = top + rh / 2;
  int door_x_w = left + 0;
  int door_y_e = top + rh / 2;
  int door_x_e = left + rw - 1;

  if (door_target_for_dir(state, 0) != -1)
    mvwaddch(win, door_y_n, door_x_n, '+');
  if (door_target_for_dir(state, 1) != -1)
    mvwaddch(win, door_y_e, door_x_e, '+');
  if (door_target_for_dir(state, 2) != -1)
    mvwaddch(win, door_y_s, door_x_s, '+');
  if (door_target_for_dir(state, 3) != -1)
    mvwaddch(win, door_y_w, door_x_w, '+');

  char self_sym = (state->player_id >= 0 && state->player_id < MAX_PLAYERS)
                      ? (char)('1' + state->player_id)
                      : '@';
  mvwaddch(win, top + py, left + px, self_sym);

  if (state->players != NULL) {
    const int slots[4][2] = {
        {rh / 2, rw / 2}, {rh / 2, rw / 2 - 2},
        {rh / 2, rw / 2 + 2}, {rh / 2 + 2, rw / 2}};
    bool occupied[rh][rw];
    memset(occupied, 0, sizeof(occupied));
    occupied[py][px] = true;

    for (int i = 0; i < MAX_PLAYERS; i++) {
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
        if (py_other >= 1 && py_other < rh - 1 && px_other >= 1 &&
            px_other < rw - 1 && !occupied[py_other][px_other]) {
          oy = py_other;
          ox = px_other;
          occupied[oy][ox] = true;
          placed = true;
        }
      }

      if (!placed) {
        if (oy >= 1 && oy < rh - 1 && ox >= 1 && ox < rw - 1 &&
            !occupied[oy][ox]) {
          occupied[oy][ox] = true;
          placed = true;
        }
      }

      if (!placed) {
        for (int y = 1; y < rh - 1 && !placed; y++) {
          for (int x = 1; x < rw - 1; x++) {
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

  int list_y = top + rh + 1;
  mvwprintw(win, list_y, 2, "Porte: N=%d E=%d S=%d W=%d",
            door_target_for_dir(state, 0), door_target_for_dir(state, 1),
            door_target_for_dir(state, 2), door_target_for_dir(state, 3));

  render_minimap(win, state, top, left);
}

static void render_minimap(WINDOW *win, const GameState *state, int top, int left) {
  int mx = left + state->room_w + 3;
  int my = top;

  wattron(win, A_BOLD);
  mvwprintw(win, my, mx, "MAPPA");
  wattroff(win, A_BOLD);
  my += 1;

  /* Draw rooms 0-9 in a 2x5 grid with connections */
  for (int row = 0; row < 2; row++) {
    int cx = mx;
    for (int col = 0; col < 5; col++) {
      int room = row * 5 + col;
      if (room >= state->map_size) break;

      bool is_current = (room == state->current_room);
      bool is_connected = false;
      if (state->adj && state->current_room >= 0 &&
          state->current_room < state->map_size) {
        is_connected = state->adj[state->current_room][room] != 0;
      }

      if (is_current) wattron(win, A_REVERSE);
      else if (is_connected) wattron(win, A_BOLD);
      mvwprintw(win, my, cx, "%d", room);
      wattroff(win, A_REVERSE);
      wattroff(win, A_BOLD);
      cx++;

      if (col < 4) {
        int next = row * 5 + col + 1;
        if (next < state->map_size && state->adj &&
            state->adj[room][next])
          mvwaddch(win, my, cx, ACS_HLINE);
        else
          mvwaddch(win, my, cx, ' ');
        cx++;
      }
    }
    my++;

    if (row == 0) {
      cx = mx;
      for (int col = 0; col < 5; col++) {
        int room_top = col;
        int room_bot = 5 + col;
        if (room_bot < state->map_size && state->adj &&
            state->adj[room_top][room_bot])
          mvwaddch(win, my, cx, ACS_VLINE);
        else
          mvwaddch(win, my, cx, ' ');
        cx += 2; /* skip the horizontal-connection column */
      }
      my++;
    }
  }

  my += 1;
  mvwprintw(win, my, mx, "Quest: %d/%d", state->quest_items_collected, QUEST_ITEMS_TO_WIN);
  my++;
  mvwprintw(win, my, mx, "Vite:  %d", state->team_lives);
}

static void render_game(WINDOW *win, const GameState *state, int py, int px,
                        const char *msg) {
  werase(win);
  box(win, 0, 0);
  render_room(win, state, py, px, msg);
  wrefresh(win);
}

static int check_game_over(const GameState *state) {
  if (state->quest_items_collected >= QUEST_ITEMS_TO_WIN) {
    return show_end_stats(state, true) ? 2 : 1;
  }
  if (state->team_lives <= 0) {
    return show_end_stats(state, false) ? 2 : 1;
  }
  return 0;
}

static int show_end_stats(const GameState *state, bool won) {
  WINDOW *win = newwin(0, 0, 0, 0);
  const char *title = won ? "HAI VINTO!" : "SCONFITTA!";

  werase(win);
  box(win, 0, 0);
  wattron(win, A_BOLD);
  mvwprintw(win, 1, 2, "%s", title);
  wattroff(win, A_BOLD);

  int row = 3;
  int player_label = (state->player_id >= 0) ? (state->player_id + 1) : 0;
  mvwprintw(win, row++, 2, "Statistiche giocatore %d", player_label);
  mvwprintw(win, row++, 4, "Mostri uccisi: %d", state->player_monsters_killed);
  mvwprintw(win, row++, 4, "Item normali: %d", state->player_normal_items);
  mvwprintw(win, row++, 4, "Item quest: %d", state->player_quest_items);
  mvwprintw(win, row++, 4, "Totale item: %d", state->player_total_items);
  mvwprintw(win, row++, 4, "Trappole attivate: %d", state->player_traps_triggered);

  row++;
  mvwprintw(win, row++, 2, "Statistiche team");
  mvwprintw(win, row++, 4, "Mostri uccisi: %d", state->team_kills);
  mvwprintw(win, row++, 4, "Quest: %d/%d", state->quest_items_collected,
            QUEST_ITEMS_TO_WIN);
  mvwprintw(win, row++, 4, "Vite residue: %d", state->team_lives);

  row++;
  wattron(win, A_BOLD);
  mvwprintw(win, row++, 2, "Rigiocare? (s/n)");
  wattroff(win, A_BOLD);
  wrefresh(win);

  int ch;
  while ((ch = wgetch(win)) != ERR) {
    if (ch == 's' || ch == 'S') { delwin(win); return 1; }
    if (ch == 'n' || ch == 'N') { delwin(win); return 0; }
  }

  delwin(win);
  return 0;
}

static void show_inventories(const char *json_str) {
  WINDOW *win = newwin(0, 0, 0, 0);
  wattron(win, A_BOLD);
  mvwprintw(win, 1, 2, ">>> INVENTARI DEI GIOCATORI <<<");
  wattroff(win, A_BOLD);

  cJSON *root = cJSON_Parse(json_str);
  if (!root) {
    mvwprintw(win, 3, 2, "Errore nel parsing degli inventari");
    wrefresh(win);
    sleep(2);
    delwin(win);
    return;
  }

  cJSON *inventories = cJSON_GetObjectItemCaseSensitive(root, "inventories");
  if (!cJSON_IsArray(inventories)) {
    mvwprintw(win, 3, 2, "Nessun inventario disponibile");
    wrefresh(win);
    sleep(2);
    cJSON_Delete(root);
    delwin(win);
    return;
  }

  int row = 3;
  cJSON *player_obj = NULL;
  cJSON_ArrayForEach(player_obj, inventories) {
    cJSON *player_id = cJSON_GetObjectItemCaseSensitive(player_obj, "player_id");
    cJSON *normal = cJSON_GetObjectItemCaseSensitive(player_obj, "normal_items");
    cJSON *quest = cJSON_GetObjectItemCaseSensitive(player_obj, "quest_items");
    cJSON *total = cJSON_GetObjectItemCaseSensitive(player_obj, "total_items");

    if (!cJSON_IsNumber(player_id) || !cJSON_IsNumber(normal) || 
        !cJSON_IsNumber(quest) || !cJSON_IsNumber(total)) {
      continue;
    }

    wattron(win, A_BOLD);
    mvwprintw(win, row++, 2, "Giocatore %d:", player_id->valueint);
    wattroff(win, A_BOLD);
    mvwprintw(win, row++, 4, "Item Normali: %d", normal->valueint);
    mvwprintw(win, row++, 4, "Item Quest: %d", quest->valueint);
    mvwprintw(win, row++, 4, "Totale: %d", total->valueint);
    row++;
  }

  mvwprintw(win, row + 1, 2, "Premi un tasto per continuare...");
  wrefresh(win);
  wgetch(win);
  cJSON_Delete(root);
  delwin(win);
}

int game_screen(GameState *state) {
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

  int py = state->room_h / 2;
  int px = state->room_w / 2;

  int prev_lives = state->team_lives;
  char status[128] = {0};
  render_game(win, state, py, px, NULL);

  while (1) {
    int ch = wgetch(win);
    if (ch == 'q' || ch == 'Q') {
      break;
    }
    int dy = 0, dx = 0;
    if (!is_move_key(ch, &dy, &dx))
      continue;

    int new_y = py + dy;
    int new_x = px + dx;
    int at_wall = (new_y <= 0 || new_y >= state->room_h - 1 || new_x <= 0 ||
                   new_x >= state->room_w - 1);
    if (at_wall) {
      int dir = wall_dir(state, new_y, new_x);
      int target = (dir < 0) ? -1 : door_target_for_dir(state, dir);
      if (target == -1) {
        char *poll = sendnrecv(GET_GAME_STATE);
        if (poll == NULL) {
          snprintf(status, sizeof(status), "Server non risponde");
          render_game(win, state, py, px, status);
          continue;
        }
        if (update_state_from_reply(state, poll, status, sizeof(status), &py,
                                    &px) < 0) {
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
      py = state->room_h / 2;
      px = state->room_w / 2;
      if (update_state_from_reply(state, reply, status, sizeof(status), &py,
                                  &px) < 0) {
        render_game(win, state, py, px, status);
        continue;
      }
      snprintf(status, sizeof(status), "Sei entrato nella stanza %d", target);
      render_game(win, state, py, px, status);
      int go1 = check_game_over(state);
      if (go1) { delwin(win); return go1; }
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
    if (update_state_from_reply(state, reply, status, sizeof(status), &py,
                                &px) < 0) {
      render_game(win, state, py, px, status);
      continue;
    }
    if (state->team_lives < prev_lives) {
      snprintf(status, sizeof(status), "Il team ha perso una vita! Vite: %d", state->team_lives);
      render_game(win, state, py, px, status);
    } else {
      render_game(win, state, py, px, NULL);
    }
    prev_lives = state->team_lives;
    int go2 = check_game_over(state);
    if (go2) { delwin(win); return go2; }
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
  clear();
  refresh();
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

    int title_y = y - 2;
    if (title_y < 0)
      title_y = 0;
    draw_centered_title(start_x, title, title_y);

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

  draw_centered_title(start_x, title, 1);
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

  copy_default_or_input(ip_buf, IP_SERVER, host, host_sz);
  copy_default_or_input(port_buf, PORT, port, port_sz);

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
  int my_player_id = -1;
  init_ncurses();
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
      char *reply = sendnrecv(GET_N_OF_CONNECTED_PLAYERS);
      if (reply == NULL) {
        show_popup(win, "Connessione persa o server non risponde.",
                   "Premi un tasto per tornare al menu.", NULL);
        wgetch(win);
        delwin(win);
        return -1;
      }

      /* Check if it's inventories JSON */
      if (reply[0] == '{') {
        show_inventories(reply);
        free(reply);
        continue;
      }

      /* Check if team was defeated */
      if (strncmp(reply, MSG_TEAM_DEFEATED, strlen(MSG_TEAM_DEFEATED)) == 0) {
        werase(win);
        box(win, 0, 0);
        mvwprintw(win, 2, 2, "IL TEAM E' STATO SCONFITTO!");
        mvwprintw(win, 3, 2, "Torniamo alla lobby...");
        wrefresh(win);
        sleep(2);
        free(reply);
        delwin(win);
        endwin();
        return -1;
      }

      /* Check if team won */
      if (strncmp(reply, MSG_TEAM_WON, strlen(MSG_TEAM_WON)) == 0) {
        werase(win);
        box(win, 0, 0);
        mvwprintw(win, 2, 2, "IL TEAM HA VINTO!");
        mvwprintw(win, 3, 2, "Hai collezionato tutti gli item quest!");
        mvwprintw(win, 5, 2, "Torniamo alla lobby...");
        wrefresh(win);
        sleep(3);
        free(reply);
        delwin(win);
        endwin();
        return -1;
      }

      int parsed = parse_connected_players(reply);
      if (parsed < 0) {
        char reason[80] = {0};
        snprintf(reason, sizeof(reason), "%.*s", 60, reply);
        show_popup(win, "Impossibile entrare in lobby.", reason,
                   "Premi un tasto per tornare al menu.");
        free(reply);
        wgetch(win);
        delwin(win);
        return -1;
      }

      connected_players = parsed;

      if (my_player_id == -1) {
        my_player_id = connected_players - 1;
      }

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

    mvwhline(win, 6, 1, ' ', win_w - 2);
    if (sep1 < win_w - 1)
      mvwaddch(win, 6, sep1, ACS_VLINE);
    if (sep2 < win_w - 1)
      mvwaddch(win, 6, sep2, ACS_VLINE);
    if (sep3 < win_w - 1)
      mvwaddch(win, 6, sep3, ACS_VLINE);

    if (my_player_id == 0 && connected_players >= 1) {
      mvwaddch(win, 6, 2, PLAYER_1_SYMBOL);
    }
    if (my_player_id == 1 && connected_players >= 2) {
      mvwaddch(win, 6, sep1 + 2, PLAYER_2_SYMBOL);
    }
    if (my_player_id == 2 && connected_players >= 3) {
      mvwaddch(win, 6, sep2 + 2, PLAYER_3_SYMBOL);
    }
    if (my_player_id == 3 && connected_players >= 4) {
      mvwaddch(win, 6, sep3 + 2, PLAYER_4_SYMBOL);
    }

    wattron(win, A_REVERSE);
    mvwprintw(win, 7, 4, "[ Inizia ]");
    wattroff(win, A_REVERSE);

    wrefresh(win);

    int ch = wgetch(win);
    if (ch == '\n' || ch == KEY_ENTER) {
      char *reply = sendnrecv(START_GAME);
      if (reply == NULL) {
        show_popup(win, "Server non risponde all'avvio partita.",
                   "Premi un tasto per tornare al menu.", NULL);
        wgetch(win);
        delwin(win);
        return -1;
      }

      GameState state = {0};
      char parse_err[64] = {0};
      if (parse_game_state(reply, &state, parse_err, sizeof(parse_err)) < 0) {
        show_popup(win, "Errore nel parsing dello stato di gioco.",
                   "Premi un tasto per tornare al menu.", NULL);
        free(reply);
        wgetch(win);
        delwin(win);
        return -1;
      }

      if (parse_err[0] != '\0') {
        char errline[128];
        snprintf(errline, sizeof(errline), "Errore avvio partita: %s",
                 parse_err);
        show_popup(win, errline, "Premi un tasto per tornare al menu.", NULL);
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
      if (state.players && state.player_id >= 0 &&
          state.player_id < MAX_PLAYERS) {
        state.current_room = state.players[state.player_id];
      }
      int result = game_screen(&state);
      free_game_state(&state);

      if (result == 2) {
        /* Rigiocare: torna alla lobby */
        win = newwin(win_h, win_w, win_y, win_x);
        keypad(win, TRUE);
        wtimeout(win, 400);
        tick = 0;
        continue;
      }
      return -1;
    }
  }

  delwin(win);
  return 0;
}
