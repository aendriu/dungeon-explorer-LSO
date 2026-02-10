#ifndef PLAYER
#define PLAYER

#include "../header/connection.h"
#include "../header/messages.h"
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

// player id sequence
extern int plid_seq;

// initialize newplayer sock
void init_newplayer(int, Conn *);
void *handle_player(void *args);

#endif
