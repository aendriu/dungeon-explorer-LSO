#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../header/player.h"

// player id sequence
int plid_seq=0;

enum { FRAME_SIZE = 256 };

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



// *****

void init_newplayer(int sockfd, Conn* connections) {
    pthread_t tid;
    connections[plid_seq].player_id = plid_seq;
    connections[plid_seq].sockfd = sockfd;
    connections[plid_seq].room_id = 0;  // Start in room 0
    connections[plid_seq].x = 0;
    connections[plid_seq].y = 0;
    
    // Assign player symbol based on ID
    char symbols[] = {PLAYER_1_SYMBOL, PLAYER_2_SYMBOL, PLAYER_3_SYMBOL, PLAYER_4_SYMBOL};
    connections[plid_seq].symbol = symbols[plid_seq % 4];
    
	int rc = pthread_create(&tid, NULL, handle_player, (void*)(&connections[plid_seq]));
	if (rc != 0) {
        perror("pthread_create ERROR in init_newplayer: ");
    }

    connections[plid_seq].tid = tid; 

    plid_seq+=1;
}

// *****

void *handle_player(void* args) {
	Conn *myconn = (Conn*)args;
    printf("Im handling player!\n");
    while(1) {
        char msg[FRAME_SIZE];
        memset(msg, 0, sizeof(msg));

        int r = recv_all(myconn->sockfd, msg, sizeof(msg));
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

        if (strncmp(msg, SEND_N_OF_CONNECTED_PLAYERS, strlen(SEND_N_OF_CONNECTED_PLAYERS)) == 0) {
            /* plid_seq is the current number of connected players (0..MAX_PLAYERS) */
            snprintf(response, sizeof(response), "%d", plid_seq);
        } else {
            strncpy(response, "OK", sizeof(response) - 1);
        }

        if (send_all(myconn->sockfd, response, sizeof(response)) < 0) {
            break;
        }
    }

    close(myconn->sockfd);
    myconn->sockfd = 0;
    return NULL;

}

// *****

int player_lose_life() {
    pthread_mutex_lock(&team.lives_mutex);
    team.shared_lives--;
    int remaining_lives = team.shared_lives;
    printf("[TEAM] Player lost a life! Remaining: %d\n", remaining_lives);
    
    if (remaining_lives <= 0) {
        printf("[TEAM] TEAM DEFEATED!\n");
        // Inviare il messaggio di sconfitta a tutti i giocatori
        for (int i = 0; i < plid_seq; i++) {
            if (connections[i].sockfd != 0) {
                send_all(connections[i].sockfd, MSG_TEAM_DEFEATED, strlen(MSG_TEAM_DEFEATED) + 1);
            }
        }
    }
    
    pthread_mutex_unlock(&team.lives_mutex);
    return remaining_lives;
}

// *****

bool player_collect_item(int player_id, int x, int y) {
    if (!game_dungeon) {
        return false;
    }

    // For now, use room 0 as default (can be expanded with player tracking)
    int room_id = 0;
    bool team_won = item_collect(game_dungeon, player_id, room_id, x, y);

    if (team_won) {
        printf("[GAME] TEAM WON! All quest items collected!\n");
        // Inviare il messaggio di vittoria a tutti i giocatori
        for (int i = 0; i < plid_seq; i++) {
            if (connections[i].sockfd != 0) {
                send_all(connections[i].sockfd, MSG_TEAM_WON, strlen(MSG_TEAM_WON) + 1);
            }
        }
        return true;
    }

    // Broadcast item collected message with player name
    char item_msg[256];
    snprintf(item_msg, sizeof(item_msg), "[PLAYER %d] Hai preso un oggetto!", player_id);
    for (int i = 0; i < plid_seq; i++) {
        if (connections[i].sockfd != 0) {
            send_all(connections[i].sockfd, item_msg, strlen(item_msg) + 1);
        }
    }

    // Inviare aggiornamento del quest status ai giocatori
    char quest_msg[256];
    snprintf(quest_msg, sizeof(quest_msg), "%s:%d", MSG_QUEST_UPDATE, quest_items_collected);
    for (int i = 0; i < plid_seq; i++) {
        if (connections[i].sockfd != 0) {
            send_all(connections[i].sockfd, quest_msg, strlen(quest_msg) + 1);
        }
    }

    // Incrementare counter items per il giocatore
    if (player_id >= 0 && player_id < plid_seq) {
        connections[player_id].items_collected++;
    }

    return false;
}

