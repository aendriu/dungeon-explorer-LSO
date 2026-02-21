#ifndef CLIENT_MYSIGNALS_H
#define CLIENT_MYSIGNALS_H

#include <signal.h>

extern volatile sig_atomic_t client_running;

void install_signal_handlers(void);

#endif
