#ifndef PLAYER
#define PLAYER

#include "../../utils/enums.h"
#include "../header/connection.h"
#include "../header/game_state.h"
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

#define MAX_PLAYERS 4

extern int plid_seq;

extern Conn connections[MAX_PLAYERS];

int get_connected_players(void);

void init_newplayer(int, Conn *);
void *handle_player(void *args);

int player_lose_life();
int send_all(int sockfd, const char *msg, size_t len);
bool player_collect_item(int player_id, int x, int y);
void print_player_items(int player_id);

#endif
