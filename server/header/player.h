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

int get_connected_players(void);

void init_newplayer(int, Conn *);
void *handle_player(void *args);

int player_lose_life(void);
int send_all(int sockfd, const char *msg, size_t len);
bool player_collect_item(int player_id, int x, int y);
void player_kill_monster(int player_id);
void print_player_items(int player_id);
void broadcast_all_inventories(void);

#endif
