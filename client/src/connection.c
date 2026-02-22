#define _POSIX_C_SOURCE 200112L
#include "../header/connection.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

int sock = -1;
struct addrinfo *servinfo = NULL;

/* Risolve hostname e porta con getaddrinfo */
static void init_servinfo(const char *host, const char *port) {
    if (servinfo != NULL) {
        freeaddrinfo(servinfo);
        servinfo = NULL;
    }

    struct addrinfo hints;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = 0;
    
    int status;
    if ((status = getaddrinfo(host, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "[CLIENT] gai error: %s\n", gai_strerror(status));
        exit(1);
    }
    if(servinfo == NULL) {
        fprintf(stderr, "[CLIENT] init_servinfo: servinfo is NULL\n");
		exit(1);
    }
}

/* Crea il socket TCP usando i parametri risolti in servinfo */
static void socket_create(void) {
    sock = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    if( sock == -1 ) {
        perror("[CLIENT] - socket()");
        exit(1);
    }
}

/* Crea il socket TCP e si connette al server */
int connect_to_server(const char *host, const char *port) {
    if (host == NULL || port == NULL) {
        return -1;
    }

    if (sock != -1) {
        close(sock);
        sock = -1;
    }

    init_servinfo(host, port);
    socket_create();
    
	if (connect(sock, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
		perror("[CLIENT] - connect");
		close(sock);
        sock = -1;
        return -1;
	}

    /* Timeout di ricezione: 1 secondo */
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    return 0;
}

