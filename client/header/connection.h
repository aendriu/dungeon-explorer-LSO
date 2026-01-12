#ifndef CONNECTION_CLIENT
#define CONNECTION_CLIENT

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
#define IP_SERVER "127.0.0.1"
#define BACKLOG 10

extern int sock;
extern struct addrinfo *servinfo;
int connect_to_server(const char *host, const char *port);

#endif