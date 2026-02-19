#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>

#include "../header/connection.h"
#include "../header/player.h"
#include "../header/game_state.h"

Conn connections[MAX_PLAYERS] = {0};
volatile sig_atomic_t server_running = 1;
pthread_mutex_t conn_mutex = PTHREAD_MUTEX_INITIALIZER;

static void sig_handler(int sig) {
    (void)sig;
    server_running = 0;
}

static void install_signal_handlers(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sig_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

static void cleanup(void) {
    if (game_dungeon != NULL) {
        dungeon_destroy(game_dungeon);
        game_dungeon = NULL;
    }
    freeaddrinfo(servinfo);
    sockets_free(connections, MAX_PLAYERS);
}

int main(void) {
    memset(connections, 0, sizeof(connections));
    memset(players, 0, sizeof(players));
    init_welcome_sock();
    install_signal_handlers();

    while (server_running) {
        if (get_connected_players() < MAX_PLAYERS) {
            int player_sockfd = listen_loop();
            if (player_sockfd < 0) {
                if (!server_running) break;
                continue;
            }
            init_newplayer(player_sockfd, connections);
            printf("Players connected: %d\n", get_connected_players());
        } else {
            listen_loop_refuse();
        }
    }

    printf("\nShutting down...\n");
    cleanup();
    return 0;
}

    
