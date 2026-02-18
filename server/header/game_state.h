#ifndef SERVER_GAME_STATE_H
#define SERVER_GAME_STATE_H

#include <stdbool.h>
#include <pthread.h>

#define FRAME_SIZE 4096
#define MAP_SIZE   10
#define ROOM_H      9
#define ROOM_W     21
#define MAX_PLAYERS 4

typedef struct {
    int  map_size;
    int  map[MAP_SIZE][MAP_SIZE];
    int  positions[MAX_PLAYERS];
    int  pos_y[MAX_PLAYERS];
    int  pos_x[MAX_PLAYERS];
    bool started;
    pthread_mutex_t mutex;
} GameState;

extern GameState game_state;

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
