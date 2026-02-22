#define _POSIX_C_SOURCE 200112L
#include "../header/connection.h"
#include "../header/game_state.h"
#include <arpa/inet.h>
#include <errno.h>
#include <ifaddrs.h>
#include <netinet/in.h>

// Invia tutti i byte sul socket, gestendo invii parziali
int send_all(int sockfd, const char *msg, size_t len) {
  if (sockfd <= 0 || msg == NULL) return -1;

  size_t sent = 0;
  while (sent < len) {
    ssize_t rc = send(sockfd, msg + sent, len - sent, 0);
    if (rc < 0) {
      if (errno == EINTR) continue;
      return -1;
    }
    if (rc == 0) return -1;
    sent += (size_t)rc;
  }
  return 0;
}

static const char *MSG_MAX_PLAYER_REACHED =
    "The maximum player capacity has been reached!";
int welcome_sock = 0;
static bool si_initialized = false;
struct addrinfo *servinfo = NULL;

// Risolve indirizzo e porta del server
void init_servinfo() {
    struct addrinfo hints;
    memset(&hints, 0, sizeof hints); 
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_protocol = IPPROTO_TCP;

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
    printf("- servinfo initialized\n");
}

// Crea il socket di benvenuto e mette in ascolto
void init_welcome_sock() {
    welcome_sock = socket_create();
    socket_bind(welcome_sock);
    if(listen(welcome_sock, BACKLOG) == -1) {
        perror("[SERVER] - listen in init_welcome_sock()");
        exit(1);
    }
    printf("- welcome socket initialized\n");
}

// Chiude tutti i socket aperti
void sockets_free(Conn* socks, int size) {
    close(welcome_sock);
    for(int i = 0; i < size && i < MAX_PLAYERS; i++) {
        if(socks[i].sockfd != 0)
            close(socks[i].sockfd);
    }
    return;
}

// Crea un socket TCP con SO_REUSEADDR
int socket_create() {
    if(!si_initialized) { init_servinfo();}

    int sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol); 
    if( sockfd == -1 ) {
        perror("[SERVER] - socket()");
        exit(1);
    }

    // SO_REUSEADDR per evitare TIME_WAIT
    int optval = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
        perror("[SERVER] - setsockopt(SO_REUSEADDR)");
        close(sockfd);
        exit(1);
    }

    return sockfd;
}

// Associa il socket all'indirizzo/porta risolti da servinfo
void socket_bind(int sockfd) {
    int res = bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen);
    if(res == -1) {
        close(sockfd);
        perror("[SERVER] - bind()");
        exit(1);
    }
}

// Accetta una connessione in arrivo
int listen_loop() {
	struct sockaddr_storage their_addr;
	socklen_t sin_size = sizeof(their_addr);
    int new_sockfd = accept(welcome_sock, (struct sockaddr *)&their_addr,&sin_size);
    return new_sockfd;
}

// Accetta e rifiuta subito (server pieno)
void listen_loop_refuse() {
	struct sockaddr_storage their_addr;
	socklen_t sin_size = sizeof(their_addr);
    int new_fd = accept(welcome_sock, (struct sockaddr *)&their_addr,&sin_size);
    if (new_fd != -1) {
        char frame[256];
        memset(frame, 0, sizeof(frame));
        strncpy(frame, MSG_MAX_PLAYER_REACHED, sizeof(frame) - 1);
        send(new_fd, frame, sizeof(frame), 0);
        close(new_fd);
    }
}

