#define _POSIX_C_SOURCE 200112L
#include "header/connection.h"

// ************************************************** //

void init_servinfo() {
    struct addrinfo hints;
    memset(&hints, 0, sizeof hints); 
    hints.ai_family = AF_INET;     
    hints.ai_socktype = SOCK_STREAM; 
    hints.ai_flags = AI_PASSIVE;
    
    int status;
    if ((status = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "gai error: %s\n", gai_strerror(status));
        exit(1);
    }
    if(servinfo == NULL) {
        fprintf(stderr, "init_servinfo: servinfo is NULL\n");
		exit(1);
    }
    si_initialized = true;
    printf("- servinfo initialized.");
}

// *****

void init_welcome_sock() {
    welcome_sock = socket_create();
    socket_bind(welcome_sock);
    printf("- welcome socket initialized\n");
}

// *****

void sockets_free(Conn* socks, int size) {
    close(welcome_sock);
    for(int i = 0; i < size; i++) {
        if(socks[i].sockfd != 0)
            close(socks[i].sockfd);
    }
    return;
}

// ************************************************** //

int socket_create() {
    int status;
    if(!si_initialized) { init_servinfo();}

    int sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol); 
    if( sockfd == -1 ) {
        perror("socket() : ");
        exit(1);
    }
    return sockfd;
}

// *****

void socket_bind(int sockfd) {
    int res = bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen);
    if(res == -1) {
        close(sockfd);
        perror("server: bind");
    }
}

// *****

void connect_loop() {
    if(listen(welcome_sock, BACKLOG) == -1) {
		perror("listen in connect_loop()");
		exit(1);
	}
	struct sockaddr_storage their_addr;
	socklen_t sin_size = sizeof(their_addr);
    int new_fd = accept(welcome_sock, (struct sockaddr *)&their_addr,&sin_size);
    init_newplayer(new_fd);
}

