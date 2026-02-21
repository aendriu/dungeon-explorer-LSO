#ifndef CLIENT_CONNECTION_H
#define CLIENT_CONNECTION_H

#include <netdb.h>

#define PORT "9090"
#define IP_SERVER "127.0.0.1"

extern int sock;
extern struct addrinfo *servinfo;
int connect_to_server(const char *host, const char *port);

#endif