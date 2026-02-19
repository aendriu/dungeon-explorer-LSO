#ifndef SERVER_GAME_STATE_H
#define SERVER_GAME_STATE_H

#include <stdbool.h>
#include <pthread.h>
#include "dungeon.h"

#define FRAME_SIZE 4096

extern Dungeon *game_dungeon;
#define MAP_SIZE   10 // how many rooms
#define ROOM_H_MIN  16
#define ROOM_H_MAX  28
#define ROOM_W_MIN  24
#define ROOM_W_MAX  40
#define MAX_PLAYERS 4
#define INITIAL_LIVES 12

typedef struct {
    int  map_size;
    int  map[MAP_SIZE][MAP_SIZE];
    int  positions[MAX_PLAYERS]; // what room are they in
    int  pos_y[MAX_PLAYERS];
    int  pos_x[MAX_PLAYERS];
    bool started;
    pthread_mutex_t mutex;
} GameState;

extern GameState game_state;
extern Dungeon *game_dungeon;

typedef struct {
    int shared_lives;
    int monsters_killed;
    pthread_mutex_t lives_mutex;
} Team;

extern Team team;

void  init_teams(void);

void  init_game_state_if_needed(void);
unsigned int *get_rand_seed(void);

char *build_game_state_json(int player_id, const char *error);

#endif
