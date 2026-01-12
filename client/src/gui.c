#include "../header/gui.h"

#include "../header/connection.h"

#include <string.h>

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
