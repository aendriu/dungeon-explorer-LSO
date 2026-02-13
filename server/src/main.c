#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>

#include "../header/connection.h"
#include "../header/player.h"
#include "../header/game_state.h"

Conn connections[MAX_PLAYERS] = {0}; 

void sig_handlr_shutdown(int sig) {
    printf("Revieved signal %d, shutting down...\n", sig);  
    freeaddrinfo(servinfo);
    sockets_free(connections, 4);
    exit(0);
}

int main (){
    memset(connections, 0, sizeof(connections));
    init_welcome_sock();
    signal(SIGINT, sig_handlr_shutdown);

    while(true) {
        if(get_connected_players() < MAX_PLAYERS) {
            int player_sockfd = listen_loop();
            init_newplayer(player_sockfd, connections);
            printf("Players connected: %d\n", get_connected_players());
        } else {
            listen_loop_refuse();   
        }
    }

    freeaddrinfo(servinfo);
    sockets_free(connections, 4);
    return 0;
}

    
