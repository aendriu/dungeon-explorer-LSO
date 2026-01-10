#ifndef PLAYER
#define PLAYER

#include "../header/connection.h"
#include <stdio.h>
#include <pthread.h>

#define PL_1_SYMBOL  "1"
#define PL_2_SYMBOL  "2"
#define PL_3_SYMBOL  "3"
#define PL_4_SYMBOL  "4"

// player id sequence
extern int plid_seq;

// initialize newplayer sock
void init_newplayer(int,Conn*);

void *handle_player(void* args);

#endif






