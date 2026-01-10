#include "../header/player.h"

// player id sequence
int plid_seq=0;

// *****

void init_newplayer(int sockfd, Conn* connections) {
    pthread_t tid;
	int rc = pthread_create(&tid, NULL, handle_player, NULL);
	if (rc != 0) {
        perror("pthread_create ERROR in init_newplayer: ");
    }
    connections[plid_seq].sockfd = sockfd;
    connections[plid_seq].tid = tid;
    connections[plid_seq].player_id = plid_seq;
    plid_seq+=1;
}

// *****

void *handle_player(void* args) {
	(void)args;
    printf("Im handling player!\n");
    return NULL;
}


