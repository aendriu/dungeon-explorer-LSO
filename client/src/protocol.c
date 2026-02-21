#define _POSIX_C_SOURCE 200112L

#include "../header/protocol.h"
#include "../header/connection.h"
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

/* Dimensione frame di comunicazione (4 KB) */
enum { CLIENT_FRAME_SIZE = 4096 };

/* Mutex per accesso al socket tra main thread e poller */
static pthread_mutex_t net_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Invia len byte sul socket, gestendo invii parziali */
static int send_all(int fd, const char *buf, size_t len) {
  size_t sent = 0;
  while (sent < len) {
    ssize_t rc = send(fd, buf + sent, len - sent, 0);
    if (rc < 0) {
      if (errno == EINTR) continue;
      return -1;
    }
    if (rc == 0) return -1;
    sent += (size_t)rc;
  }
  return 0;
}

/* Riceve esattamente len byte dal socket */
static int recv_all(int fd, char *buf, size_t len) {
  size_t received = 0;
  while (received < len) {
    ssize_t rc = recv(fd, buf + received, len - received, 0);
    if (rc < 0) {
      if (errno == EINTR) continue;
      return -1;
    }
    if (rc == 0) return -1;
    received += (size_t)rc;
  }
  return 0;
}

/* Logica interna di invio/ricezione (mutex già acquisito) */
static char *sendnrecv_frame_locked(const char *frame) {
  if (sock < 0) {
    fprintf(stderr, "[CLIENT] sendnrecv: socket not connected\n");
    return NULL;
  }

  char buffer[CLIENT_FRAME_SIZE];
  memset(buffer, 0, sizeof(buffer));
  strncpy(buffer, frame, sizeof(buffer) - 1);

  if (send_all(sock, buffer, sizeof(buffer)) < 0) {
    perror("[CLIENT] sendnrecv: send");
    return NULL;
  }

  char *reply = (char *)calloc(CLIENT_FRAME_SIZE + 1, 1);
  if (reply == NULL)
    return NULL;

  if (recv_all(sock, reply, CLIENT_FRAME_SIZE) < 0) {
    free(reply);
    return NULL;
  }

  return reply;
}

/* Invia una frame e riceve la risposta, serializzando con mutex */
static char *sendnrecv_frame(const char *frame) {
  pthread_mutex_lock(&net_mutex);
  char *result = sendnrecv_frame_locked(frame);
  pthread_mutex_unlock(&net_mutex);
  return result;
}

/* Invia un comando (intero) al server, ritorna la risposta (da free-are) */
char *sendnrecv(Message msg) {
  char frame[16];
  snprintf(frame, sizeof(frame), "%d", msg);
  return sendnrecv_frame(frame);
}

/* Invia un comando con payload JSON, ritorna la risposta (da free-are) */
char *sendnrecv_payload(Message msg, const char *payload_json) {
  char frame[256];
  
  if (payload_json && payload_json[0] != '\0') {
    snprintf(frame, sizeof(frame), "{\"cmd\":%d,\"payload\":%s}", msg,
             payload_json);
  } else {
    snprintf(frame, sizeof(frame), "{\"cmd\":%d}", msg);
  }
  return sendnrecv_frame(frame);
}

