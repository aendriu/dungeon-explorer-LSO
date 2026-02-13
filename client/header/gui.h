#ifndef GUI
#define GUI

#include <ncurses.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../header/connection.h"
#include "../../utils/enums.h"
#include "../header/mysignals.h"
#include "../header/protocol.h"

typedef struct {
  int map_size;
  int current_room;
  int player_id;
  int **adj;
  int *players;
  int *pos_y;
  int *pos_x;
} GameState;

void init_ncurses();
UserChoice welcome_menu();

int prompt_server_address(char *host, size_t host_sz, char *port,
                          size_t port_sz);
int lobby_screen(void);
int game_screen(GameState *state);

#endif
