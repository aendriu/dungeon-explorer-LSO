#include "../header/connection.h"
#include "../header/gui.h"
#include "../header/mysignals.h"
#include <ncurses.h>
#include <stdio.h>
#include <unistd.h>

static int manage_choice(UserChoice *state);

/*
 * Entry point del client: inizializza UI/segnali e gestisce il ciclo menu-lobby.
 */
int main(void) {
  init_ncurses();
  install_signal_handlers();

  UserChoice choice = MENU_ENTERLOBBY;
  while (client_running) {
    if (choice == MENU_ENTERLOBBY || choice == MENU_QUIT) {
      choice = welcome_menu();
    }

    if (manage_choice(&choice) < 0) {
      break;
    }
  }

  endwin();

  if (sock != -1) {
    close(sock);
    sock = -1;
  }
  if (servinfo != NULL) {
    freeaddrinfo(servinfo);
    servinfo = NULL;
  }

  return 0;
}

/*
 * Traduce la scelta corrente del menu in un'azione concreta.
 * Ritorna -1 quando va terminata l'applicazione.
 */
static int manage_choice(UserChoice *state) {
  switch (*state) {
  case MENU_ENTERLOBBY: {
    char host[256] = {0};
    char port[32] = {0};

    if (prompt_server_address(host, sizeof(host), port, sizeof(port)) < 0) {
      break;
    }

    if (connect_to_server(host, port) == 0) {
      *state = TO_LOBBY;
    } else {
      fprintf(stderr, "Failed to connect to %s:%s\n", host, port);
      getchar();
    }
    return 0;
  }

  case TO_LOBBY: {
    if (lobby_screen() < 0) {
      *state = MENU_ENTERLOBBY;
    }
    return 0;
  }

  case MENU_QUIT:
    return -1;

  default:
    break;
  }

  return 0;
}
