#define _POSIX_C_SOURCE 200809L
#include "../header/mysignals.h"
#include <string.h>

/* 
 * Flag globale per controllare il loop principale del client
 * Diventa 0 quando riceve SIGINT (Ctrl+C) o SIGTERM (termina processo)
 * Usato per uscire gracefully dal programma
 */
volatile sig_atomic_t client_running = 1;

/*
 * Handler per i segnali SIGINT (Ctrl+C) e SIGTERM
 * Imposta client_running a 0 per far terminare il loop principale
 * Il parametro 'sig' non viene usato (void cast per evitare warning)
 */
static void sig_handler(int sig) {
    (void)sig;  /* Scarta il parametro inutilizzato */
    client_running = 0;  /* Segnala al loop di uscire */
}

/*
 * Installa i gestori di segnali SIGINT (Ctrl+C) e SIGTERM (terminate)
 * Permette al client di uscire gracefully quando riceve questi segnali
 * Usa la funzione 'sigaction' (più affidabile di signal per applicazioni complesse)
 */
void install_signal_handlers(void) {
    struct sigaction sa;
    
    /* Azzera la struttura sigaction */
    memset(&sa, 0, sizeof(sa));
    
    /* Configura l'handler personalizzato */
    sa.sa_handler = sig_handler;
    
    /* Crea un insieme di segnali vuoto (nessun segnale bloccato durante l'handler) */
    sigemptyset(&sa.sa_mask);
    
    /* Nessun flag speciale */
    sa.sa_flags = 0;
    
    /* Installa l'handler per SIGINT (Ctrl+C) */
    sigaction(SIGINT, &sa, NULL);
    
    /* Installa l'handler per SIGTERM (termina processo) */
    sigaction(SIGTERM, &sa, NULL);
}