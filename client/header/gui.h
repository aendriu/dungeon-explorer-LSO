#ifndef CLIENT_GUI_H
#define CLIENT_GUI_H

#include <stddef.h>
#include "../../utils/enums.h"

extern int start_y, start_x;

void init_ncurses(void);
UserChoice welcome_menu(void);
int prompt_server_address(char *host, size_t host_sz, char *port,
                          size_t port_sz);
int lobby_screen(void);

#endif
