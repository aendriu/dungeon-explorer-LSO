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

// Protocol/message string constants
#define GET_N_OF_CONNECTED_PLAYERS_STR "NOCP"
#define SEND_N_OF_CONNECTED_PLAYERS "NOCP"
#define MSG_TEAM_DEFEATED "TEAM_DEFEATED"
#define MSG_TEAM_WON "TEAM_WON"
#define MSG_QUEST_UPDATE "QUEST_UPDATE"
#define MSG_ITEM_MISSED "ITEM_MISSED"

// Display symbols
#define ITEM_SYMBOL '?'
#define DOOR_ENTRANCE 'E'
#define DOOR_EXIT 'U'
#define PLAYER_1_SYMBOL '@'
#define PLAYER_2_SYMBOL '#'
#define PLAYER_3_SYMBOL '$'
#define PLAYER_4_SYMBOL '%'

#endif
