#ifndef UTILS_ENUMS_H
#define UTILS_ENUMS_H

typedef enum { 
    MENU_ENTERLOBBY, 
    MENU_QUIT, 
    TO_LOBBY 
} UserChoice;

typedef enum { 
    GET_N_OF_CONNECTED_PLAYERS, 
    START_GAME,
    MOVE_PLAYER,
    GET_GAME_STATE,
    UPDATE_PLAYER_POSITION
} Message;

#endif
