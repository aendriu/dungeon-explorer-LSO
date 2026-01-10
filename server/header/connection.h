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

#define PORT "9090"
#define BACKLOG 10

extern int welcome_sock;
extern bool si_initialized;
extern struct addrinfo *servinfo;

typedef struct {
    int sockfd;
    pthread_t tid;
    int player_id;
} Conn;


// initializations
void init_servinfo();
void init_welcome_sock();

// sockets
int socket_create();
void socket_bind(int);

// listen
int listen_loop();
void listen_loop_refuse();

//free
void sockets_free(Conn*, int);





#endif