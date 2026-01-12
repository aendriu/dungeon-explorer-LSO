#include "../header/mysignals.h"

void sig_handlr_shutdown(int sig) {
    printf("Recieved signal %d, shutting down...\n", sig);
    if (sock != -1) {
        close(sock);
        sock = -1;
    }
    if (servinfo != NULL) {
        freeaddrinfo(servinfo);
        servinfo = NULL;
    }
    exit(0);
}