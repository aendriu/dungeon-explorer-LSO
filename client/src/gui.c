#include "../header/gui.h"
#include "../header/connection.h"
#include "../header/game_actions.h"
#include "../header/game_state.h"
#include "../header/ui_helpers.h"
#include <ncurses.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

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
                 state->room_monsters[y][x] != 0) {
        ch = 'M'; /* monster */
      } else if (state->room_items != NULL &&
                 state->room_items[y][x] != 0) {
        int v = state->room_items[y][x];
        if (v == 2)      ch = 'Q'; /* quest */
        else if (v == 3) ch = 'T'; /* trap */
        else if (v == 4) ch = '@'; /* apple */
        else             ch = '?'; /* normal */
      }
      mvwaddch(win, wy, wx, ch);
    }
  }

  /* Calcola una sola volta i target per le 4 porte (N, E, S, W) */
  int doors[4];
  for (int d = 0; d < 4; d++)
    doors[d] = door_target_for_dir(state, d);

  int mid_y = top + rh / 2;
  int mid_x = left + rw / 2;

  if (doors[0] != -1) {
    mvwaddch(win, top, mid_x - 1, ' ');
    mvwaddch(win, top, mid_x,     '+');
    mvwaddch(win, top, mid_x + 1, ' ');
  }
  if (doors[1] != -1) {
    int ex = left + rw - 1;
    mvwaddch(win, mid_y - 1, ex, ' ');
    mvwaddch(win, mid_y,     ex, '+');
    mvwaddch(win, mid_y + 1, ex, ' ');
  }
  if (doors[2] != -1) {
    int sy = top + rh - 1;
    mvwaddch(win, sy, mid_x - 1, ' ');
    mvwaddch(win, sy, mid_x,     '+');
    mvwaddch(win, sy, mid_x + 1, ' ');
  }
  if (doors[3] != -1) {
    mvwaddch(win, mid_y, left, '+');
    mvwaddch(win, mid_y - 1, left, ' ');
    mvwaddch(win, mid_y + 1, left, ' ');
  }

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
            doors[0], doors[1], doors[2], doors[3]);

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

/* Controlla se la partita è finita: 1=vittoria, -1=sconfitta, 0=in corso */
static int check_game_over(const GameState *state) {
  if (state->quest_items_collected >= QUEST_ITEMS_TO_WIN)
    return 1;
  if (state->team_lives <= 0)
    return -1;
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

static int game_screen(GameState *state) {
  int win_h = start_y - 2;
  int win_w = start_x - 2;
  if (win_h < 10)
    win_h = 10;
  if (win_w < 40)
    win_w = 40;

  WINDOW *win = newwin(win_h, win_w, 1, 1);
  keypad(win, TRUE);
  wtimeout(win, 200); /* non-bloccante: ritorna ERR dopo 200ms */
  wclear(win);
  wrefresh(win);

  int py = state->room_h / 2;
  int px = state->room_w / 2;

  /* Avvia il thread di polling in background */
  Poller poller;
  poller_start(&poller, state, py, px);

  int prev_lives = 0;
  int game_over = 0;
  char status[128] = {0};

  /* Prima renderizzazione */
  pthread_mutex_lock(&poller.mutex);
  prev_lives = poller.state.team_lives;
  render_game(win, &poller.state, poller.py, poller.px, NULL);
  pthread_mutex_unlock(&poller.mutex);

  while (game_over == 0) {
    int ch = wgetch(win);

    if (ch == 'q' || ch == 'Q')
      break;

    /* Gestione input: azioni di gioco sotto mutex */
    int dy = 0, dx = 0;
    if (ch != ERR && is_move_key(ch, &dy, &dx)) {
      pthread_mutex_lock(&poller.mutex);
      status[0] = '\0';
      int new_y = poller.py + dy;
      int new_x = poller.px + dx;
      bool at_wall = (new_y <= 0 || new_y >= poller.state.room_h - 1 ||
                      new_x <= 0 || new_x >= poller.state.room_w - 1);

      if (at_wall) {
        int dir = wall_dir(&poller.state, new_y, new_x);
        int target = (dir < 0) ? -1 : door_target_for_dir(&poller.state, dir);
        if (target == -1) {
          if (action_poll_state(&poller.state, status, sizeof(status),
                                &poller.py, &poller.px) == 0
              && status[0] == '\0')
            snprintf(status, sizeof(status), "Porta chiusa");
        } else {
          if (action_move_to_room(&poller.state, target, status,
                                  sizeof(status), &poller.py, &poller.px) == 0
              && status[0] == '\0')
            snprintf(status, sizeof(status), "Sei entrato nella stanza %d",
                     target);
        }
      } else {
        poller.py = new_y;
        poller.px = new_x;
        action_update_pos(&poller.state, poller.py, poller.px, status,
                          sizeof(status), &poller.py, &poller.px);
      }
      pthread_mutex_unlock(&poller.mutex);
    }

    /* Lettura stato condiviso, controllo eventi, render (sotto mutex) */
    pthread_mutex_lock(&poller.mutex);

    if (poller.state.team_lives < prev_lives && status[0] == '\0')
      snprintf(status, sizeof(status),
               "Il team ha perso una vita! Vite: %d", poller.state.team_lives);
    prev_lives = poller.state.team_lives;

    game_over = check_game_over(&poller.state);
    if (game_over == 0)
      render_game(win, &poller.state, poller.py, poller.px,
                  status[0] ? status : NULL);

    pthread_mutex_unlock(&poller.mutex);
    status[0] = '\0';
  }

  /* Ferma il thread poller e recupera lo stato finale */
  poller_stop(&poller, state);
  delwin(win);

  /* Mostra le statistiche finali se la partita è terminata */
  if (game_over != 0)
    return show_end_stats(state, game_over > 0) ? 2 : 1;

  return 0;
}

void init_ncurses(void) {
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

UserChoice welcome_menu(void) {

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
      choice = (choice + 1) % 2;
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

int lobby_screen(void) {
  int my_player_id = -1;
  getmaxyx(stdscr, start_y, start_x);
  clear();
  refresh();
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
      char err[80] = {0};
      int n = action_poll_lobby(err, sizeof(err));
      if (n < 0) {
        const char *l1 = (n == -1)
            ? "Connessione persa o server non risponde."
            : "Impossibile entrare in lobby.";
        const char *err_hint = "Premi un tasto per tornare al menu.";
        show_popup(win, l1, (n == -2) ? err : err_hint, (n == -2) ? err_hint : NULL);
        wgetch(win);
        delwin(win);
        return -1;
      }
      connected_players = n;
      if (my_player_id == -1)
        my_player_id = n - 1;
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

    int col_w = (win_w - 2) / 4;
    if (col_w < 10)
      col_w = 10;

    int sep[3] = { 1 + col_w, 1 + col_w * 2, 1 + col_w * 3 };
    int col_x[4] = { 2, sep[0] + 2, sep[1] + 2, sep[2] + 2 };
    static const char psym[4] = {
        PLAYER_1_SYMBOL, PLAYER_2_SYMBOL,
        PLAYER_3_SYMBOL, PLAYER_4_SYMBOL };

    for (int r = 3; r < win_h - 1; r++)
      for (int s = 0; s < 3; s++)
        if (sep[s] < win_w - 1)
          mvwaddch(win, r, sep[s], ACS_VLINE);

    mvwprintw(win, 3, 2, "Connessi: %d/4", connected_players);
    for (int i = 0; i < 4; i++) {
      mvwprintw(win, 4, col_x[i], "Giocatore %d", i + 1);
      mvwprintw(win, 5, col_x[i], "%s",
                (connected_players >= i + 1) ? "Connesso" : "Vuoto");
    }

    mvwhline(win, 6, 1, ' ', win_w - 2);
    for (int s = 0; s < 3; s++)
      if (sep[s] < win_w - 1)
        mvwaddch(win, 6, sep[s], ACS_VLINE);

    if (my_player_id >= 0 && my_player_id < 4 &&
        connected_players >= my_player_id + 1)
      mvwaddch(win, 6, col_x[my_player_id], psym[my_player_id]);

    wattron(win, A_REVERSE);
    mvwprintw(win, 7, 4, "[ Inizia ]");
    wattroff(win, A_REVERSE);

    wrefresh(win);

    int ch = wgetch(win);
    if (ch == '\n' || ch == KEY_ENTER) {
      GameState state = {0};
      char err[128] = {0};
      if (action_start_game(&state, err, sizeof(err)) < 0) {
        show_popup(win, err, "Premi un tasto per tornare al menu.", NULL);
        wgetch(win);
        delwin(win);
        return -1;
      }

      delwin(win);
      clear();
      refresh();
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
}
