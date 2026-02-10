#include "../header/connection.h"
#include "../header/gui.h"
#include "../header/messages.h"
#include "../header/mysignals.h"
#include "../header/protocol.h"
#include "../header/types.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static int manage_choice(UserChoice *state);

UserChoice choice;

int main() {
  signal(SIGINT, sig_handlr_shutdown);

  while (1) {
    if (choice == MENU_ENTERLOBBY || choice == MENU_QUIT) {
      choice = welcome_menu();
    }

    if (manage_choice(&choice) < 0) {
      break;
    }
  }

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
      printf("Failed to connect to %s:%s\n", host, port);
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
