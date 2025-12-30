#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "header/connection.h"
#include "header/player.h"

#define MAX_PLAYERS 4

Conn connections[MAX_PLAYERS] = {0}; 
int nof_connected_players=0;


void init_newplayer(int sockfd) {
    connections[nof_connected_players].sockfd = sockfd;
	pthread_t tid;
	int rc = pthread_create(&tid, NULL, handle_player, NULL);
	if (rc != 0) {
        perror("pthread_create ERROR in init_newplayer: ");
    }
    connections[nof_connected_players].tid = tid;
}


// ******************************************************* //

int main (){
    memset(connections, 0, sizeof(connections));
    init_welcome_sock();

    while(true) {
        if(nof_connected_players < MAX_PLAYERS) {
            connect_loop();
            printf("Player %d is connected\n", nof_connected_players + 1);
            connections[nof_connected_players].player_id = nof_connected_players + 1;
            nof_connected_players+=1;
        } else {
            connect_loop_refuse();   
        }
    }


    freeaddrinfo(servinfo);
    sockets_free(connections, 4);
    return 0;
}



    
