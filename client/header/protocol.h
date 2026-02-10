#ifndef CLIENT_PROTOCOL_H
#define CLIENT_PROTOCOL_H

/*
 * Sends a request message to the server on the already-connected socket
 * and waits for a single reply.
 *
 * Returns a heap-allocated NUL-terminated string containing the reply,
 * or NULL on error/timeout.
 *
 * Caller must free

*/

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include "../header/connection.h"
#include "../../utils/enums.h"

char *sendnrecv(Message msg);
char *sendnrecv_payload(Message msg, const char *payload_json);

#endif
