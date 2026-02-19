#include "../header/game_state.h"
#include "../header/connection.h"
#include "../header/player.h"
#include "../header/monster.h"
#include "../../utils/enums.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

Dungeon *game_dungeon = NULL;

Team team = {
    .shared_lives = 3,
    .monsters_killed = 0,
    .lives_mutex  = PTHREAD_MUTEX_INITIALIZER
};

static unsigned int g_rand_seed = 0;

GameState game_state = {
    .map_size   = MAP_SIZE,
    .map        = {{0}},
    .positions  = {0},
    .pos_y      = {0},
    .pos_x      = {0},
    .started    = false,
    .mutex      = PTHREAD_MUTEX_INITIALIZER
};

static void add_corridor(int map[MAP_SIZE][MAP_SIZE], int i, int j) {
    if (i == j) return;
    map[i][j] = 1;
    map[j][i] = 1;
}

static void shuffle(int *arr, int n) {
    for (int i = n - 1; i > 0; i--) {
        int j = rand_r(&g_rand_seed) % (i + 1);
        int tmp = arr[i]; arr[i] = arr[j]; arr[j] = tmp;
    }
}

static void generate_random_map(int map[MAP_SIZE][MAP_SIZE]) {
    int order[MAP_SIZE];
    for (int i = 0; i < MAP_SIZE; i++) order[i] = i;
    shuffle(order + 1, MAP_SIZE - 1);

    for (int i = 0; i < MAP_SIZE - 1; i++) {
        add_corridor(map, order[i], order[i + 1]);
    }

    int extras = 2 + rand_r(&g_rand_seed) % 3;
    for (int i = 0; i < extras; i++) {
        int a = rand_r(&g_rand_seed) % MAP_SIZE;
        int b = rand_r(&g_rand_seed) % MAP_SIZE;
        add_corridor(map, a, b);
    }
}

void init_teams(void) {
    pthread_mutex_lock(&team.lives_mutex);
    team.shared_lives = 3;
    team.monsters_killed = 0;
    pthread_mutex_unlock(&team.lives_mutex);
    printf("- team initialized with 3 shared lives\n");
}

unsigned int *get_rand_seed(void) {
    return &g_rand_seed;
}

void init_game_state_if_needed(void) {
    pthread_mutex_lock(&game_state.mutex);

    /* Se la partita è finita (vite esaurite o quest completata), resetta */
    if (game_state.started) {
        bool game_over = false;
        pthread_mutex_lock(&team.lives_mutex);
        if (team.shared_lives <= 0) game_over = true;
        pthread_mutex_unlock(&team.lives_mutex);

        pthread_mutex_lock(&quest_items_mutex);
        if (quest_items_collected >= QUEST_ITEMS_TO_WIN) game_over = true;
        pthread_mutex_unlock(&quest_items_mutex);

        if (game_over) {
            printf("[GAME] Partita terminata, reset per nuova partita\n");
            game_state.started = false;
        }
    }

    if (!game_state.started) {
        g_rand_seed = (unsigned int)time(NULL);
        memset(game_state.map, 0, sizeof(game_state.map));
        generate_random_map(game_state.map);
        for (int i = 0; i < MAX_PLAYERS; i++) {
            game_state.positions[i] = -1;
            game_state.pos_y[i]     = -1;
            game_state.pos_x[i]     = -1;
        }

        if (game_dungeon != NULL) {
            dungeon_destroy(game_dungeon);
            game_dungeon = NULL;
        }
        game_dungeon = dungeon_create(MAP_SIZE, ROOM_W, ROOM_H);
        if (game_dungeon) {
            quest_items_collected = 0;
            for (int i = 0; i < MAP_SIZE; i++) {
                room_generate_items(game_dungeon->rooms[i]);
                room_generate_monsters(game_dungeon->rooms[i]);
            }
        }

        init_teams();
        game_state.started = true;
    }
    pthread_mutex_unlock(&game_state.mutex);
}

char *build_game_state_json(int player_id, const char *error) {
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) return NULL;

    cJSON_AddNumberToObject(root, "map_size", game_state.map_size);
    cJSON_AddNumberToObject(root, "player_id", player_id);
    cJSON_AddNumberToObject(root, "current_room",
                            (player_id >= 0 && player_id < MAX_PLAYERS)
                                ? game_state.positions[player_id]
                                : -1);
    if (error != NULL) {
        cJSON_AddStringToObject(root, "error", error);
    }

    cJSON *players_json = cJSON_AddArrayToObject(root, "players");
    if (players_json == NULL) { cJSON_Delete(root); return NULL; }
    for (int i = 0; i < MAX_PLAYERS; i++) {
        cJSON_AddItemToArray(players_json, cJSON_CreateNumber(game_state.positions[i]));
    }

    cJSON *pos = cJSON_AddArrayToObject(root, "pos");
    if (pos == NULL) { cJSON_Delete(root); return NULL; }
    for (int i = 0; i < MAX_PLAYERS; i++) {
        cJSON *pair = cJSON_CreateArray();
        if (pair == NULL) { cJSON_Delete(root); return NULL; }
        cJSON_AddItemToArray(pair, cJSON_CreateNumber(game_state.pos_y[i]));
        cJSON_AddItemToArray(pair, cJSON_CreateNumber(game_state.pos_x[i]));
        cJSON_AddItemToArray(pos, pair);
    }

    cJSON *adj = cJSON_AddArrayToObject(root, "adj");
    if (adj == NULL) { cJSON_Delete(root); return NULL; }
    for (int i = 0; i < game_state.map_size; i++) {
        cJSON *row = cJSON_CreateArray();
        if (row == NULL) { cJSON_Delete(root); return NULL; }
        for (int j = 0; j < game_state.map_size; j++) {
            cJSON_AddItemToArray(row, cJSON_CreateNumber(game_state.map[i][j]));
        }
        cJSON_AddItemToArray(adj, row);
    }

    int cur = (player_id >= 0 && player_id < MAX_PLAYERS)
                  ? game_state.positions[player_id]
                  : -1;
    cJSON_AddNumberToObject(root, "room_h", ROOM_H);
    cJSON_AddNumberToObject(root, "room_w", ROOM_W);
    if (game_dungeon != NULL) {
        Room *room = dungeon_get_room(game_dungeon, cur);
        if (room != NULL) {
            cJSON *items = cJSON_AddArrayToObject(root, "room_items");
            if (items != NULL) {
                for (int y = 0; y < room->height; y++) {
                    cJSON *row = cJSON_CreateArray();
                    if (row == NULL) break;
                    for (int x = 0; x < room->width; x++) {
                        Item *it = &room->items[y][x];
                        int val = 0;
                        if (!it->collected && it->type != ITEM_NONE) {
                            val = (it->type == ITEM_QUEST) ? 2 : 1;
                        }
                        cJSON_AddItemToArray(row, cJSON_CreateNumber(val));
                    }
                    cJSON_AddItemToArray(items, row);
                }
            }

            cJSON *monsters = cJSON_AddArrayToObject(root, "room_monsters");
            if (monsters != NULL) {
                for (int y = 0; y < room->height; y++) {
                    cJSON *row = cJSON_CreateArray();
                    if (row == NULL) break;
                    for (int x = 0; x < room->width; x++) {
                        Monster *m = room->monsters[y][x];
                        int val = (m != NULL && m->state != MONSTER_DEAD) ? 1 : 0;
                        cJSON_AddItemToArray(row, cJSON_CreateNumber(val));
                    }
                    cJSON_AddItemToArray(monsters, row);
                }
            }
        }
    }

    pthread_mutex_lock(&quest_items_mutex);
    cJSON_AddNumberToObject(root, "quest_items_collected", quest_items_collected);
    pthread_mutex_unlock(&quest_items_mutex);

    pthread_mutex_lock(&team.lives_mutex);
    cJSON_AddNumberToObject(root, "team_lives", team.shared_lives);
    cJSON_AddNumberToObject(root, "team_kills", team.monsters_killed);
    pthread_mutex_unlock(&team.lives_mutex);

    cJSON *pstats = cJSON_AddObjectToObject(root, "player_stats");
    if (pstats != NULL) {
        int pid = (player_id >= 0 && player_id < MAX_PLAYERS) ? player_id : -1;
        if (pid >= 0) {
            pthread_mutex_lock(&conn_mutex);
            Player *p = &players[pid];
            cJSON_AddNumberToObject(pstats, "normal_items", p->normal_items_count);
            cJSON_AddNumberToObject(pstats, "quest_items", p->quest_items_count);
            cJSON_AddNumberToObject(pstats, "total_items", p->items_collected);
            cJSON_AddNumberToObject(pstats, "traps_triggered", p->traps_triggered);
            cJSON_AddNumberToObject(pstats, "monsters_killed", p->monsters_killed);
            pthread_mutex_unlock(&conn_mutex);
        } else {
            cJSON_AddNumberToObject(pstats, "normal_items", 0);
            cJSON_AddNumberToObject(pstats, "quest_items", 0);
            cJSON_AddNumberToObject(pstats, "total_items", 0);
            cJSON_AddNumberToObject(pstats, "traps_triggered", 0);
            cJSON_AddNumberToObject(pstats, "monsters_killed", 0);
        }
    }

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}
