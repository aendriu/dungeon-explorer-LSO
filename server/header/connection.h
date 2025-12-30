#ifndef CONNECTION
#define CONNECTION

#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netdb.h>
#include <stdio.h>
#include <stdbool.h>

#define PORT "9090"
#define BACKLOG 10

int welcome_sock = 0;
bool si_initialized = false;
struct addrinfo *servinfo; 

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


void sockets_free(Conn*, int);





#endif