#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

#include "../header/mysignals.h"
#include "../header/types.h"
#include "../header/connection.h"
#include "../header/gui.h"


int manage_choice(UserChoice choice);

int main() {
    signal(SIGINT, sig_handlr_shutdown);

    while (1) {
        UserChoice choice = welcome_menu();
        if (manage_choice(choice) < 0) {
            break;
        }
    }
    
    
    if (sock != -1) {
        close(sock);
        sock = -1;
    }
    if (servinfo != NULL) {
        freeaddrinfo(servinfo);
        servinfo = NULL;
    }

    return 0;
}

// *****

int manage_choice(UserChoice choice) {
    switch (choice)
    {
    case MENU_ENTERLOBBY:
        char host[256] = {0};
        char port[32] = {0};

        if (prompt_server_address(host, sizeof(host), port, sizeof(port)) < 0) {
            break;
        }

        if (connect_to_server(host, port) == 0) {
            printf("Connected to %s:%s\n", host, port);
        } else {
            printf("Failed to connect to %s:%s\n", host, port);
        }
        getchar();
        break;
    
    
    case MENU_QUIT:
        return -1;
    
    default:
        break;
    }

    return 0;
}