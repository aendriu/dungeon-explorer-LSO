#ifndef CONNECTION
#define CONNECTION

#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netdb.h>
#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include "dungeon.h"

#define PORT "9090"
#define BACKLOG 10

extern int welcome_sock;
extern bool si_initialized;
extern struct addrinfo *servinfo;
extern Dungeon *game_dungeon;

typedef struct {
    int sockfd;
    pthread_t tid;
    int player_id;
    int items_collected;
    int room_id;
    int x, y;
    char symbol;
} Conn;

void init_servinfo();
void init_welcome_sock();

int socket_create();
void socket_bind(int);

int listen_loop();
void listen_loop_refuse();

void sockets_free(Conn*, int);

#endif