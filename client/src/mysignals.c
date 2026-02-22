#define _POSIX_C_SOURCE 200809L
#include "../header/mysignals.h"
#include <string.h>

/* Flag per il loop principale: 0 = esci */
volatile sig_atomic_t client_running = 1;

/* Handler minimale: alla ricezione segnale termina il loop client. */
static void sig_handler(int sig) {
    (void)sig;
    client_running = 0;
}

/* Registra handler per SIGINT e SIGTERM con sigaction */
void install_signal_handlers(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sig_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}