#ifndef GUI
#define GUI

#include <ncurses.h>
#include "header/types.h"

#include <stddef.h>

void init_ncurses();
UserChoice welcome_menu();

int prompt_server_address(char *host, size_t host_sz, char *port, size_t port_sz);
int lobby_screen(void);

#endif