#define _POSIX_C_SOURCE 200112L
#include "../header/connection.h"
#include "../header/game_state.h"
#include <arpa/inet.h>
#include <errno.h>
#include <ifaddrs.h>
#include <netinet/in.h>

// Invia tutti i byte di un messaggio sul socket
// Ritorna 0 se tutto è stato inviato, -1 se errore
int send_all(int sockfd, const char *msg, size_t len) {
  if (sockfd <= 0 || msg == NULL) return -1;

  size_t sent = 0;
  // Continua fino a quando non sono stati inviati tutti i byte
  while (sent < len) {
    ssize_t rc = send(sockfd, msg + sent, len - sent, 0);
    if (rc < 0) {
      if (errno == EINTR) continue;  // Riprova se interrotto
      return -1;
    }
    if (rc == 0) return -1;  // Errore: connessione chiusa
    sent += (size_t)rc;  // Aggiorna il numero di byte inviati
  }
  return 0;
}

static const char *MSG_MAX_PLAYER_REACHED =
    "The maximum player capacity has been reached!";
int welcome_sock = 0;
static bool si_initialized = false;
struct addrinfo *servinfo = NULL;

// Inizializza la struttura delle informazioni del server
// (indirizzo, porta, protocollo)
void init_servinfo() {
    struct addrinfo hints;
    // Configura i parametri per il socket server
    memset(&hints, 0, sizeof hints); 
    hints.ai_family = AF_INET;       // IPv4
    hints.ai_socktype = SOCK_STREAM; // TCP
    hints.ai_flags = AI_PASSIVE;     // Per server
    hints.ai_protocol = IPPROTO_TCP;

    // Ottiene le informazioni di indirizzamento dal SO
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

// Inizializza il socket di benvenuto del server
// Crea, associa e attiva l'ascolto sul socket
void init_welcome_sock() {
    welcome_sock = socket_create();      // Crea il socket
    socket_bind(welcome_sock);           // Lo associa alla porta
    // Attiva l'ascolto di connessioni in arrivo
    if(listen(welcome_sock, BACKLOG) == -1) {
        perror("[SERVER] - listen in init_welcome_sock()");
        exit(1);
    }
    printf("- welcome socket initialized\n");
}

// Chiude e libera tutti i socket (server e client)
void sockets_free(Conn* socks, int size) {
    close(welcome_sock);  // Chiude il socket di benvenuto
    // Chiude tutti i socket dei client connessi
    for(int i = 0; i < size && i < MAX_PLAYERS; i++) {
        if(socks[i].sockfd != 0)
            close(socks[i].sockfd);
    }
    return;
}

// Crea un nuovo socket e configura le opzioni
int socket_create() {
    // Inizializza servinfo se non è già stato fatto
    if(!si_initialized) { init_servinfo();}

    // Crea il socket con i parametri configurati
    int sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol); 
    if( sockfd == -1 ) {
        perror("[SERVER] - socket()");
        exit(1);
    }

    // Abilita il riuso dell'indirizzo per evitare errori TIME_WAIT
    int optval = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
        perror("[SERVER] - setsockopt(SO_REUSEADDR)");
        close(sockfd);
        exit(1);
    }

    return sockfd;
}

// Associa il socket all'indirizzo e porta configurati
void socket_bind(int sockfd) {
    // Lega il socket all'indirizzo e porta del server
    int res = bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen);
    if(res == -1) {
        close(sockfd);
        perror("[SERVER] - bind()");
        exit(1);
    }
}

// Accetta una nuova connessione di client in arrivo
// Ritorna il file descriptor del nuovo socket o -1 se errore
int listen_loop() {
	struct sockaddr_storage their_addr;  // Memorizza l'indirizzo del client
	socklen_t sin_size = sizeof(their_addr);
    // Accetta la connessione e crea un nuovo socket per comunicare
    int new_sockfd = accept(welcome_sock, (struct sockaddr *)&their_addr,&sin_size);
    return new_sockfd;
}

// Accetta una connessione ma la rifiuta (server pieno)
// Avvisa il client che non può connettersi
void listen_loop_refuse() {
	struct sockaddr_storage their_addr;  // Informazioni del client
	socklen_t sin_size = sizeof(their_addr);
    // Accetta la connessione in arrivo
    int new_fd = accept(welcome_sock, (struct sockaddr *)&their_addr,&sin_size);
    if (new_fd != -1) {
        // Prepara il messaggio di rifiuto
        char frame[256];
        memset(frame, 0, sizeof(frame));
        strncpy(frame, MSG_MAX_PLAYER_REACHED, sizeof(frame) - 1);
        // Invia il messaggio di rifiuto e chiude la connessione
        send(new_fd, frame, sizeof(frame), 0);
        close(new_fd);
    }
}

