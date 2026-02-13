#ifndef PLAYER
#define PLAYER

#include <stdio.h>
#include <pthread.h>
#include "../header/connection.h"
#include "../header/messages.h"

#define PL_1_SYMBOL  "1"
#define PL_2_SYMBOL  "2"
#define PL_3_SYMBOL  "3"
#define PL_4_SYMBOL  "4"

// player id sequence
extern int plid_seq;
extern Conn connections[4];

// initialize newplayer sock
void init_newplayer(int,Conn*);

void *handle_player(void* args);

// team lives management
int player_lose_life();

// item management
bool player_collect_item(int player_id, int x, int y);

#endif






