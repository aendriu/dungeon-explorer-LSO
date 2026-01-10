#define _POSIX_C_SOURCE 200112L
#include "../header/connection.h"


int sock;
struct addrinfo *servinfo;

// *****

void init_servinfo() {
    struct addrinfo hints;
    memset(&hints, 0, sizeof hints); 
    hints.ai_family = AF_INET;     
    hints.ai_socktype = SOCK_STREAM; 
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = 0;
    
    int status;
    if ((status = getaddrinfo(IP_SERVER, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "[CLIENT] gai error: %s\n", gai_strerror(status));
        exit(1);
    }
    if(servinfo == NULL) {
        fprintf(stderr, "[CLIENT] init_servinfo: servinfo is NULL\n");
		exit(1);
    }
    printf("- servinfo initialized\n");
}

// *****

void socket_create() {
    sock = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol); 
    if( sock == -1 ) {
        perror("[CLIENT] - socket()");
        exit(1);
    }
}

// *****

void connect_to_server() {
    init_servinfo();
    socket_create();
	if (connect(sock, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
		perror("[CLIENT] - connect");
		close(sock);
	}
}



