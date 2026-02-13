#ifndef PLAYER
#define PLAYER

#include "../../utils/enums.h"
#include "../header/connection.h"
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PL_1_SYMBOL "1"
#define PL_2_SYMBOL "2"
#define PL_3_SYMBOL "3"
#define PL_4_SYMBOL "4"

// max players supported by the server
#define MAX_PLAYERS 4

// player count
extern int plid_seq;
extern Conn connections[4];

// connections array (from main)
extern Conn connections[MAX_PLAYERS];

// get connected players (thread-safe)
int get_connected_players(void);

// initialize newplayer sock
void init_newplayer(int, Conn *);
void *handle_player(void *args);

// team lives management
int player_lose_life();
int send_all(int sockfd, const char *msg, size_t len);
// item management
bool player_collect_item(int player_id, int x, int y);

#endif
