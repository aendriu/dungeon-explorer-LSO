#define _POSIX_C_SOURCE 200112L

#include "../header/protocol.h"

enum { CLIENT_FRAME_SIZE = 4096 };

static char *sendnrecv_frame(const char *frame) {
  if (sock < 0) {
    fprintf(stderr, "[CLIENT] sendnrecv: socket not connected\n");
    return NULL;
  }

  char buffer[CLIENT_FRAME_SIZE];
  memset(buffer, 0, sizeof(buffer));
  strncpy(buffer, frame, sizeof(buffer) - 1);

  if (send(sock, buffer, sizeof(buffer), 0) < 0) {
    perror("[CLIENT] sendnrecv: send");
    return NULL;
  }

  char *reply = (char *)calloc(CLIENT_FRAME_SIZE + 1, 1);
  if (reply == NULL) {
    return NULL;
  }

  ssize_t r = recv(sock, reply, CLIENT_FRAME_SIZE, 0);
  if (r <= 0) {
    free(reply);
    return NULL;
  }

  reply[r] = '\0';
  return reply;
}

char *sendnrecv(Message msg) {
  char frame[CLIENT_FRAME_SIZE];
  memset(frame, 0, sizeof(frame));
  snprintf(frame, sizeof(frame), "%d", msg);
  return sendnrecv_frame(frame);
}

char *sendnrecv_payload(Message msg, const char *payload_json) {
  char frame[CLIENT_FRAME_SIZE];
  memset(frame, 0, sizeof(frame));
  if (payload_json && payload_json[0] != '\0') {
    snprintf(frame, sizeof(frame), "{\"cmd\":%d,\"payload\":%s}", msg,
             payload_json);
  } else {
    snprintf(frame, sizeof(frame), "{\"cmd\":%d}", msg);
  }
  return sendnrecv_frame(frame);
}

char *sendnwait(const char *frame) {
  if (frame == NULL) {
    return NULL;
  }
  return sendnrecv_frame(frame);
}
