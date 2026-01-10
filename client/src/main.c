#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include "../header/connection.h"
#include "../header/gui.h"
#include "../header/codes.h"



void sig_handlr_shutdown(int sig) {
    printf("Recieved signal %d, shutting down...\n", sig);
    close(sock);
    freeaddrinfo(servinfo);
    exit(0);
}

int main() {
    signal(SIGINT, sig_handlr_shutdown);
    connect_to_server();
    printf("You are successfully connected to server!\n");
    getchar();

    close(sock);
    freeaddrinfo(servinfo);

    return 0;
}