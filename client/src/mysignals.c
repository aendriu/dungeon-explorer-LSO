#define _POSIX_C_SOURCE 200809L
#include "../header/mysignals.h"
#include <string.h>

volatile sig_atomic_t client_running = 1;

static void sig_handler(int sig) {
    (void)sig;
    client_running = 0;
}

void install_signal_handlers(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sig_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}