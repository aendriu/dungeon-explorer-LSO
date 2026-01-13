#ifndef CLIENT_PROTOCOL_H
#define CLIENT_PROTOCOL_H

/*
 * Sends a request message to the server on the already-connected socket
 * and waits for a single reply.
 *
 * Returns a heap-allocated NUL-terminated string containing the reply,
 * or NULL on error/timeout.
 *
 * Caller must free() the returned pointer.
 */
char *sendnwait(const char *msg);

#endif
