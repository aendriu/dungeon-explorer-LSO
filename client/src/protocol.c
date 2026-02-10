#define _POSIX_C_SOURCE 200112L

#include "../header/protocol.h"

enum { CLIENT_FRAME_SIZE = 256 };

char *sendnrecv(const char *msg) {
  if (msg == NULL) {
    return NULL;
  }
  if (sock < 0) {
    fprintf(stderr, "[CLIENT] sendnrecv: socket not connected\n");
    return NULL;
  }

  char frame[CLIENT_FRAME_SIZE];
  memset(frame, 0, sizeof(frame));
  strncpy(frame, msg, sizeof(frame) - 1);

  if (send(sock, frame, sizeof(frame), 0) < 0) {
    perror("[CLIENT] sendnrecv: send");
    return NULL;
  }

  struct timeval tv;
  tv.tv_sec = 0;
  tv.tv_usec = 350000;
  (void)setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

  char *reply = (char *)calloc(CLIENT_FRAME_SIZE + 1, 1);
  if (reply == NULL) {
    return NULL;
  }

  int r = recv(sock, reply, CLIENT_FRAME_SIZE, 0);
  if (r <= 0) {
    free(reply);
    return NULL;
  }

  reply[CLIENT_FRAME_SIZE] = '\0';
  return reply;
}
