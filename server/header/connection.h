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



void init_servinfo();
void init_welcome_sock();
int socket_create();
void socket_bind(int);
void connect_loop();
void connect_loop_refuse();

void init_newplayer(int sockfd);


void sockets_free(Conn*, int);





#endif