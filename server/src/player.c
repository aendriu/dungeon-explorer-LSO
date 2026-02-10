#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../header/player.h"

// player id sequence
int plid_seq = 0;

enum { FRAME_SIZE = 256 };

// *****

void init_newplayer(int sockfd, Conn *connections) {
  pthread_t tid;
  connections[plid_seq].player_id = plid_seq;
  connections[plid_seq].sockfd = sockfd;
  int rc = pthread_create(&tid, NULL, handle_player,
                          (void *)(&connections[plid_seq]));
  if (rc != 0) {
    perror("pthread_create ERROR in init_newplayer: ");
  }

  connections[plid_seq].tid = tid;

  plid_seq += 1;
}

// *****

void *handle_player(void *args) {
  Conn *myconn = (Conn *)args;
  printf("Im handling player!\n");
  while (1) {
    char msg[FRAME_SIZE];
    memset(msg, 0, sizeof(msg));

    int r = recv(myconn->sockfd, msg, sizeof(msg), 0);
    if (r <= 0) {
      break;
    }

    char response[FRAME_SIZE];
    memset(response, 0, sizeof(response));

    /* Remove any trailing newlines/spaces if present. */
    for (int i = 0; i < FRAME_SIZE; i++) {
      if (msg[i] == '\r' || msg[i] == '\n') {
        msg[i] = '\0';
        break;
      }
    }

    if (strncmp(msg, SEND_N_OF_CONNECTED_PLAYERS,
                strlen(SEND_N_OF_CONNECTED_PLAYERS)) == 0) {
      /* plid_seq is the current number of connected players (0..MAX_PLAYERS) */
      snprintf(response, sizeof(response), "%d", plid_seq);
    } else {
      strncpy(response, "OK", sizeof(response) - 1);
    }

    if (send(myconn->sockfd, response, sizeof(response), 0) < 0) {
      break;
    }
  }

  close(myconn->sockfd);
  myconn->sockfd = 0;
  return NULL;
}
