#ifndef SERVER_PLAYER_H
#define SERVER_PLAYER_H

#include "../../utils/enums.h"
#include "../header/connection.h"
#include "../header/game_state.h"
#include "../header/player_state.h"
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

extern int plid_seq;

extern Conn connections[MAX_PLAYERS];
extern Player players[MAX_PLAYERS];

extern pthread_mutex_t plid_mutex;

int get_connected_players(void);
void init_newplayer(int, Conn *);
int player_lose_life(void);
bool player_collect_item(int player_id, int x, int y);
void player_kill_monster(int player_id);

#endif
