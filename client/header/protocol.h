#ifndef CLIENT_PROTOCOL_H
#define CLIENT_PROTOCOL_H

#include "../../utils/enums.h"

/* Invio/ricezione messaggi verso il server. Il caller deve free-are il risultato. */
char *sendnrecv(Message msg);
char *sendnrecv_payload(Message msg, const char *payload_json);

#endif
