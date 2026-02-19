#ifndef SERVER_CONNECTION_H
#define SERVER_CONNECTION_H

#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netdb.h>
#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#define PORT "9090"
#define BACKLOG 10

extern int welcome_sock;
extern struct addrinfo *servinfo;
extern volatile sig_atomic_t server_running;
extern pthread_mutex_t conn_mutex;

typedef struct {
    int sockfd;
    pthread_t tid;
    int player_id;
} Conn;

int send_all(int sockfd, const char *msg, size_t len);

void init_servinfo();
void init_welcome_sock();

int socket_create();
void socket_bind(int);

int listen_loop();
void listen_loop_refuse();

void sockets_free(Conn*, int);

#endif