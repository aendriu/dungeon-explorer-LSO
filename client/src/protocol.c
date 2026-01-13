#define _POSIX_C_SOURCE 200112L

#include "../header/protocol.h"

#include "../header/connection.h"

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

enum { CLIENT_FRAME_SIZE = 256 };

static int send_all(int fd, const void *buf, size_t len) {
    const char *p = (const char *)buf;
    size_t sent = 0;

    while (sent < len) {
        ssize_t n = send(fd, p + sent, len - sent, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) {
            return -1;
        }
        sent += (size_t)n;
    }

    return 0;
}

static int recv_all(int fd, void *buf, size_t len) {
    char *p = (char *)buf;
    size_t recvd = 0;

    while (recvd < len) {
        ssize_t n = recv(fd, p + recvd, len - recvd, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) {
            return 0;
        }
        recvd += (size_t)n;
    }

    return 1;
}

char *sendnwait(const char *msg) {
    if (msg == NULL) {
        return NULL;
    }
    if (sock < 0) {
        fprintf(stderr, "[CLIENT] sendnwait: socket not connected\n");
        return NULL;
    }

    char frame[CLIENT_FRAME_SIZE];
    memset(frame, 0, sizeof(frame));
    strncpy(frame, msg, sizeof(frame) - 1);

    if (send_all(sock, frame, sizeof(frame)) < 0) {
        perror("[CLIENT] sendnwait: send");
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

    int r = recv_all(sock, reply, CLIENT_FRAME_SIZE);
    if (r <= 0) {
        free(reply);
        return NULL;
    }

    reply[CLIENT_FRAME_SIZE] = '\0';
    return reply;
}
