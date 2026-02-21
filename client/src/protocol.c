#define _POSIX_C_SOURCE 200112L

#include "../header/protocol.h"
#include "../header/connection.h"
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

/* Dimensione massima di un frame di comunicazione (4 KB) */
enum { CLIENT_FRAME_SIZE = 4096 };

/* Mutex per serializzare l'accesso al socket tra thread principale e poller */
static pthread_mutex_t net_mutex = PTHREAD_MUTEX_INITIALIZER;

/*
 * Invia ALL i dati dal buffer al socket
 * I dati potrebbero essere inviati in più pacchetti quindi ripete finché tutto è inviato
 * Parametri:
 *   fd: file descriptor del socket
 *   buf: dati da inviare
 *   len: numero di byte da inviare
 * Ritorna: 0 se successo, -1 se errore
 */
static int send_all(int fd, const char *buf, size_t len) {
  size_t sent = 0;
  while (sent < len) {
    /* Invia i dati rimanenti */
    ssize_t rc = send(fd, buf + sent, len - sent, 0);
    if (rc < 0) {
      /* Se il segnale ha interrotto, riprova */
      if (errno == EINTR) continue;
      return -1;  /* Errore vero */
    }
    if (rc == 0) return -1;  /* Connessione chiusa dal server */
    sent += (size_t)rc;  /* Aggiorna quanti byte sono stati inviati */
  }
  return 0;  /* Tutti i dati inviati con successo */
}

/*
 * Riceve TUTTI i dati nel buffer dal socket
 * I dati potrebbero arrivare in più pacchetti quindi ripete finché tutto è ricevuto
 * Parametri:
 *   fd: file descriptor del socket
 *   buf: buffer dove mettere i dati ricevuti
 *   len: numero di byte da ricevere
 * Ritorna: 0 se successo, -1 se errore
 */
static int recv_all(int fd, char *buf, size_t len) {
  size_t received = 0;
  while (received < len) {
    /* Riceve i dati rimanenti */
    ssize_t rc = recv(fd, buf + received, len - received, 0);
    if (rc < 0) {
      /* Se il segnale ha interrotto, riprova */
      if (errno == EINTR) continue;
      return -1;  /* Errore vero (es: timeout) */
    }
    if (rc == 0) return -1;  /* Connessione chiusa dal server */
    received += (size_t)rc;  /* Aggiorna quanti byte sono stati ricevuti */
  }
  return 0;  /* Tutti i dati ricevuti con successo */
}

/*
 * Invia una frame al server e attende la risposta
 * Parametri:
 *   frame: stringa da inviare (JSON o numero intero)
 * Ritorna: stringa malloc'ata con la risposta del server, NULL se errore
 *          Il caller deve liberare questa memoria
 */
static char *sendnrecv_frame(const char *frame) {
  pthread_mutex_lock(&net_mutex);

  /* Controlla che il socket sia connesso */
  if (sock < 0) {
    fprintf(stderr, "[CLIENT] sendnrecv: socket not connected\n");
    goto fail;
  }

  /* Prepara un buffer di dimensione fissa e copia la frame */
  char buffer[CLIENT_FRAME_SIZE];
  memset(buffer, 0, sizeof(buffer));
  strncpy(buffer, frame, sizeof(buffer) - 1);

  /* Invia la frame al server */
  if (send_all(sock, buffer, sizeof(buffer)) < 0) {
    perror("[CLIENT] sendnrecv: send");
    goto fail;
  }

  /* Alloca memoria per la risposta del server */
  char *reply = (char *)calloc(CLIENT_FRAME_SIZE + 1, 1);
  if (reply == NULL)
    goto fail;

  /* Riceve la risposta dal server */
  if (recv_all(sock, reply, CLIENT_FRAME_SIZE) < 0) {
    free(reply);
    goto fail;
  }

  pthread_mutex_unlock(&net_mutex);
  return reply;

fail:
  pthread_mutex_unlock(&net_mutex);
  return NULL;
}

/*
 * Invia un messaggio al server (numero intero) senza payload
 * Parametri:
 *   msg: numero del messaggio/comando (es: MOVE_NORTH, ATTACK, ecc)
 * Ritorna: risposta del server in JSON, NULL se errore
 *          Il caller deve liberare questa memoria
 */
char *sendnrecv(Message msg) {
  char frame[CLIENT_FRAME_SIZE];
  memset(frame, 0, sizeof(frame));
  /* Formatta il messaggio come numero intero */
  snprintf(frame, sizeof(frame), "%d", msg);
  return sendnrecv_frame(frame);
}

/*
 * Invia un messaggio al server con dati JSON nel payload
 * Parametri:
 *   msg: numero del messaggio/comando
 *   payload_json: stringa JSON con i dati (es: posizione, item scelto)
 * Ritorna: risposta del server in JSON, NULL se errore
 *          Il caller deve liberare questa memoria
 */
char *sendnrecv_payload(Message msg, const char *payload_json) {
  char frame[CLIENT_FRAME_SIZE];
  memset(frame, 0, sizeof(frame));
  
  /* Se il payload non è vuoto, lo aggiunge alla frame */
  if (payload_json && payload_json[0] != '\0') {
    snprintf(frame, sizeof(frame), "{\"cmd\":%d,\"payload\":%s}", msg,
             payload_json);
  } else {
    /* Se il payload è vuoto, invia solo il comando */
    snprintf(frame, sizeof(frame), "{\"cmd\":%d}", msg);
  }
  return sendnrecv_frame(frame);
}

