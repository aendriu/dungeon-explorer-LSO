#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <netinet/in.h>

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

/* Ogni secondo decrementa i countdown: se un player non si muove
   per IDLE_THRESHOLD secondi, i mostri diventano piu' aggressivi */
static void alarm_handler(int sig) {
    (void)sig;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (idle_countdown[i] > 0)
            idle_countdown[i]--;
    }
    alarm(1);
}

static void install_signal_handlers(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sig_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    /* SIGALRM per il timer idle dei player */
    struct sigaction sa_alrm;
    memset(&sa_alrm, 0, sizeof(sa_alrm));
    sa_alrm.sa_handler = alarm_handler;
    sigemptyset(&sa_alrm.sa_mask);
    sa_alrm.sa_flags = SA_RESTART;
    sigaction(SIGALRM, &sa_alrm, NULL);
    alarm(1);
}

static void cleanup(void) {
    if (game_dungeon != NULL) {
        dungeon_destroy(game_dungeon);
        game_dungeon = NULL;
    }
    freeaddrinfo(servinfo);
    sockets_free(connections, MAX_PLAYERS);
}

static void print_local_ips(void) {
    struct ifaddrs *ifaddr, *ifa;
    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        return;
    }
    printf("=== Server IPs (porta %s) ===\n", PORT);
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != AF_INET)
            continue;
        char ip[INET_ADDRSTRLEN];
        struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;
        inet_ntop(AF_INET, &sa->sin_addr, ip, sizeof(ip));
        printf("  %s: %s\n", ifa->ifa_name, ip);
    }
    printf("=============================\n");
    freeifaddrs(ifaddr);
}

int main(void) {
    memset(connections, 0, sizeof(connections));
    memset(players, 0, sizeof(players));
    init_welcome_sock();
    install_signal_handlers();
    print_local_ips();

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

    
