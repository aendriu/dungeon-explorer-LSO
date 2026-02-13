#include "../header/gui.h"

#include "../header/connection.h"
#include "../header/messages.h"
#include "../header/protocol.h"

#include <string.h>
#include <stdlib.h>

int start_y, start_x;

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
    init_ncurses();

    int height = 7;
    int width  = 22;
    int y = (start_y - height) / 2;
    int x = (start_x  - width)  / 2;

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
        if (title_x < 0) title_x = 0;
        int title_y = y - 2;
        if (title_y < 0) title_y = 0;

        attron(A_BOLD);
        mvprintw(title_y, title_x, "%s", title);
        attroff(A_BOLD);

        int by_x = x + (width - (int)strlen(by_line)) / 2;
        if (by_x < 0) by_x = 0;
        int by_y = y + height + 1;
        if (by_y < start_y) {
            mvprintw(by_y, by_x, "%s", by_line);

            int credits_x = x + (width - (int)strlen(credits)) / 2;
            if (credits_x < 0) credits_x = 0;
            if (by_y + 1 < start_y) {
                mvprintw(by_y + 1, credits_x, "%s", credits);
            }
        }

        refresh();

        werase(menu);
        box(menu, 0, 0);

        if (choice == 0) wattron(menu, A_REVERSE);
        mvwprintw(menu, 1, 2, "entra in lobby");
        wattroff(menu, A_REVERSE);

        if (choice == 1) wattron(menu, A_REVERSE);
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
                endwin();
                return (UserChoice)choice;
        }
    }
}

/*
    Se l'utente non inserisce ip o porta allora
    si usano parametri di def (localhost, 9090)
 */
int prompt_server_address(char *host, size_t host_sz, char *port, size_t port_sz) {
    if (host == NULL || port == NULL || host_sz == 0 || port_sz == 0) {
        return -1;
    }

    init_ncurses();
    echo();
    curs_set(1);

    erase();
    const char *title = "DUNGEON CRAWLER";
    const char *subtitle = "Inserisci IP e porta del server";

    int title_x = (start_x - (int)strlen(title)) / 2;
    if (title_x < 0) title_x = 0;
    int subtitle_x = (start_x - (int)strlen(subtitle)) / 2;
    if (subtitle_x < 0) subtitle_x = 0;

    attron(A_BOLD);
    mvprintw(1, title_x, "%s", title);
    attroff(A_BOLD);
    mvprintw(3, subtitle_x, "%s", subtitle);

    int form_y = 6;
    int form_x = 4;
    if (start_x >= 60) {
        form_x = (start_x - 52) / 2;
        if (form_x < 0) form_x = 0;
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
    endwin();

    return 0;
}

/*
    Lobby screen: 4 colonne (1 per giocatore).
    Il giocatore 1 ha il pulsante "Inizia".
*/
static int parse_connected_players(const char *reply) {
    if (reply == NULL) return -1;

    while (*reply == ' ' || *reply == '\t' || *reply == '\r' || *reply == '\n') reply++;
    if (*reply < '0' || *reply > '9') return -1;

    int n = atoi(reply);
    if (n < 0) n = 0;
    if (n > 4) n = 4;
    return n;
}

int lobby_screen(void) {
    static int my_player_id = -1;  // Traccia il tuo player_id
    init_ncurses();
    keypad(stdscr, TRUE);

    const char *title = "LOBBY";
    const char *hint = "In attesa di giocatori...";

    int win_h = start_y - 6;
    int win_w = start_x - 6;
    if (win_h < 10) win_h = 10;
    if (win_w < 50) win_w = 50;

    int win_y = (start_y - win_h) / 2;
    int win_x = (start_x - win_w) / 2;
    if (win_y < 0) win_y = 0;
    if (win_x < 0) win_x = 0;

    WINDOW *win = newwin(win_h, win_w, win_y, win_x);
    keypad(win, TRUE);
    wtimeout(win, 400);

    int connected_players = 0;
    int tick = 0;

    while (1) {
        /* Poll server every ~2 ticks (about 800ms) */
        if ((tick++ % 2) == 0) {
            char *reply = sendnwait(GET_N_OF_CONNECTED_PLAYERS);
            if (reply == NULL) {
                werase(win);
                box(win, 0, 0);
                mvwprintw(win, 2, 2, "Connessione persa o server non risponde.");
                mvwprintw(win, 3, 2, "Premi un tasto per tornare al menu.");
                wrefresh(win);
                wgetch(win);
                delwin(win);
                endwin();
                return -1;
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
                endwin();
                return -1;
            }

            connected_players = parsed;
            
            // Assegna il mio player_id solo la prima volta che ricevo un numero
            if (my_player_id == -1) {
                my_player_id = connected_players - 1;  // Io sono l'ultimo connesso (0-indexed)
            }
            
            free(reply);
        }

        werase(win);
        box(win, 0, 0);

        int title_x = (win_w - (int)strlen(title)) / 2;
        if (title_x < 1) title_x = 1;
        wattron(win, A_BOLD);
        mvwprintw(win, 1, title_x, "%s", title);
        wattroff(win, A_BOLD);

        int hint_x = (win_w - (int)strlen(hint)) / 2;
        if (hint_x < 1) hint_x = 1;
        mvwprintw(win, 2, hint_x, "%s", hint);

        int inner_w = win_w - 2;
        int col_w = inner_w / 4;
        if (col_w < 10) col_w = 10;

        int sep1 = 1 + col_w;
        int sep2 = 1 + col_w * 2;
        int sep3 = 1 + col_w * 3;

        for (int r = 3; r < win_h - 1; r++) {
            if (sep1 < win_w - 1) mvwaddch(win, r, sep1, ACS_VLINE);
            if (sep2 < win_w - 1) mvwaddch(win, r, sep2, ACS_VLINE);
            if (sep3 < win_w - 1) mvwaddch(win, r, sep3, ACS_VLINE);
        }

        mvwprintw(win, 3, 2, "Connessi: %d/4", connected_players);
        
        // Simboli dei player
        char symbols[] = {PLAYER_1_SYMBOL, PLAYER_2_SYMBOL, PLAYER_3_SYMBOL, PLAYER_4_SYMBOL};
        
        mvwprintw(win, 4, 2, "Giocatore 1");
        mvwprintw(win, 4, sep1 + 2, "Giocatore 2");
        mvwprintw(win, 4, sep2 + 2, "Giocatore 3");
        mvwprintw(win, 4, sep3 + 2, "Giocatore 4");

        mvwprintw(win, 5, 2, "%s", (connected_players >= 1) ? "Connesso" : "Vuoto");
        mvwprintw(win, 5, sep1 + 2, "%s", (connected_players >= 2) ? "Connesso" : "Vuoto");
        mvwprintw(win, 5, sep2 + 2, "%s", (connected_players >= 3) ? "Connesso" : "Vuoto");
        mvwprintw(win, 5, sep3 + 2, "%s", (connected_players >= 4) ? "Connesso" : "Vuoto");
        
        // Mostro il simbolo del mio player SOTTO il suo nome
        if (my_player_id == 0 && connected_players >= 1) {
            mvwaddch(win, 6, 2, symbols[0]);  // Simbolo sotto Giocatore 1
        }
        if (my_player_id == 1 && connected_players >= 2) {
            mvwaddch(win, 6, sep1 + 2, symbols[1]);  // Simbolo sotto Giocatore 2
        }
        if (my_player_id == 2 && connected_players >= 3) {
            mvwaddch(win, 6, sep2 + 2, symbols[2]);  // Simbolo sotto Giocatore 3
        }
        if (my_player_id == 3 && connected_players >= 4) {
            mvwaddch(win, 6, sep3 + 2, symbols[3]);  // Simbolo sotto Giocatore 4
        }

        /* The start button is shown (activation logic will be tied to player_id later). */
        wattron(win, A_REVERSE);
        mvwprintw(win, 7, 4, "[ Inizia ]");
        wattroff(win, A_REVERSE);

        wrefresh(win);

        int ch = wgetch(win);
        if (ch == '\n' || ch == KEY_ENTER) {
            mvwprintw(win, win_h - 2, 2, "Inizio partita...");
            wrefresh(win);
            wgetch(win);
            break;
        }
    }

    delwin(win);
    endwin();
    return 0;
}

// ===== GAME SCREEN =====

int game_screen(int room_width, int room_height, const char *room_map) {
    init_ncurses();

    // Create a window for the map display
    int map_height = room_height + 4;  // +4 for borders and info
    int map_width = room_width + 4;    // +4 for borders

    if (map_height > start_y - 4) map_height = start_y - 4;
    if (map_width > start_x - 4) map_width = start_x - 4;

    int win_y = (start_y - map_height) / 2;
    int win_x = (start_x - map_width) / 2;

    WINDOW *game_win = newwin(map_height, map_width, win_y, win_x);
    keypad(game_win, TRUE);
    wtimeout(game_win, 100);
    
    while (1) {
        werase(game_win);
        box(game_win, 0, 0);

        // Title
        wattron(game_win, A_BOLD);
        mvwprintw(game_win, 1, 2, "DUNGEON");
        wattroff(game_win, A_BOLD);

        // Display room map
        int map_start_y = 2;
        int map_start_x = 2;

        // Draw map from room_map string
        // Format: each row is room_width chars
        size_t map_idx = 0;
        for (int y = 0; y < room_height && y < map_height - 4; y++) {
            for (int x = 0; x < room_width && x < map_width - 4; x++) {
                if (room_map && map_idx < strlen(room_map)) {
                    char ch = room_map[map_idx];
                    // Color for items
                    if (ch == '?') {
                        wattron(game_win, COLOR_PAIR(2));
                    }
                    // Color for player
                    if (ch == '@' || ch == '#' || ch == '$' || ch == '%') {
                        wattron(game_win, COLOR_PAIR(3));
                    }
                    // Color for doors
                    if (ch == 'E' || ch == 'U') {
                        wattron(game_win, COLOR_PAIR(4));
                    }

                    mvwaddch(game_win, map_start_y + y, map_start_x + x, ch);

                    wattroff(game_win, COLOR_PAIR(2));
                    wattroff(game_win, COLOR_PAIR(3));
                    wattroff(game_win, COLOR_PAIR(4));
                }
                map_idx++;
            }
        }

        // Legend
        mvwprintw(game_win, map_height - 2, 2, "E=Entrata  U=Uscita  ?=Oggetto  @#$%%=Player");

        wrefresh(game_win);

        int ch = wgetch(game_win);
        if (ch == 'q' || ch == 'Q') {
            break;
        }
    }

    delwin(game_win);
    endwin();
    return 0;
}

