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

typedef struct {
    int shared_lives;
    pthread_mutex_t lives_mutex;
} Team;

extern Team team;
extern Dungeon *game_dungeon;

typedef struct {
    int sockfd;
    pthread_t tid;
    int player_id;
    int items_collected;  // Normal items collected by this player
    int room_id;          // Current room player is in
    int x, y;             // Player position in room
    char symbol;          // Player display symbol
} Conn;


// initializations
void init_servinfo();
void init_welcome_sock();
void init_teams();
void init_dungeon(int width, int height);

// sockets
int socket_create();
void socket_bind(int);

// listen
int listen_loop();
void listen_loop_refuse();

//free
void sockets_free(Conn*, int);





#endif