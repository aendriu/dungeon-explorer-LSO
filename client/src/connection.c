#define _POSIX_C_SOURCE 200112L
#include "../header/connection.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

/* Socket globale della connessione con il server */
int sock = -1;

/* Informazioni di indirizzo del server (host, porta, protocollo) */
struct addrinfo *servinfo = NULL;

/*
 * Inizializza le informazioni dell'indirizzo del server (host e porta)
 * Usa getaddrinfo() per risolvere l'hostname e preparare la connessione
 */
static void init_servinfo(const char *host, const char *port) {
    /* Libera la memoria della connessione precedente se esiste */
    if (servinfo != NULL) {
        freeaddrinfo(servinfo);
        servinfo = NULL;
    }

    /* Configura i hint per la ricerca dell'indirizzo */
    struct addrinfo hints;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;      /* IPv4 */
    hints.ai_socktype = SOCK_STREAM; /* TCP */
    hints.ai_protocol = IPPROTO_TCP; /* Protocollo TCP */
    hints.ai_flags = 0;
    
    /* Esegue la ricerca DNS dell'hostname e salva il risultato */
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

/*
 * Crea un socket TCP e lo assegna alla variabile globale 'sock'
 * Il socket non è ancora connesso, usare connect_to_server() per connettersi
 */
static void socket_create(void) {
    /* Crea un socket IPv4 TCP */
    sock = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    if( sock == -1 ) {
        perror("[CLIENT] - socket()");
        exit(1);
    }
}

/*
 * Connette il client al server presso l'indirizzo 'host' e la porta 'port'
 * Parametri:
 *   host: indirizzo del server (es: "localhost", "192.168.1.1")
 *   port: porta del server (es: "5000")
 * Ritorna: 0 se successo, -1 se errore
 */
int connect_to_server(const char *host, const char *port) {
    /* Valida i parametri */
    if (host == NULL || port == NULL) {
        return -1;
    }

    /* Chiude la connessione precedente se esiste */
    if (sock != -1) {
        close(sock);
        sock = -1;
    }

    /* Inizializza le informazioni del server (DNS, indirizzo, porta) */
    init_servinfo(host, port);
    
    /* Crea il socket TCP */
    socket_create();
    
    /* Stabilisce la connessione al server */
	if (connect(sock, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
		perror("[CLIENT] - connect");
		close(sock);
        sock = -1;
        return -1;
	}

    /* Imposta il timeout di ricezione a 1 secondo */
    struct timeval tv;
    tv.tv_sec = 1;      /* 1 secondo */
    tv.tv_usec = 0;     /* 0 microsecondi */
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    return 0;
}

