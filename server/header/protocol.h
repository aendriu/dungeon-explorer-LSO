#ifndef SERVER_PROTOCOL_H
#define SERVER_PROTOCOL_H

#include "../../utils/enums.h"
#include "connection.h"   /* Conn */
#include "game_state.h"
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    Message cmd;
    int     target_room;
    bool    has_target;
    int     pos_y;
    int     pos_x;
    bool    has_pos;
} ClientRequest;

ClientRequest parse_client_request(const char *msg);

void manage_response(Message cmd, const ClientRequest *req,
                     Conn *conn, char *response, size_t res_len);

#endif
