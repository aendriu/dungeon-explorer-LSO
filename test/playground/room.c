#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

#define RESET_COLOR          "\x1b[0m"

#define BLACK        "\x1b[30m"
#define RED          "\x1b[31m"
#define GREEN        "\x1b[32m"
#define YELLOW       "\x1b[33m"
#define BLUE         "\x1b[34m"
#define MAGENTA      "\x1b[35m"
#define CYAN         "\x1b[36m"
#define WHITE        "\x1b[37m"

#define BACKGROUND_BLACK   "\x1b[40m"
#define BACKGROUND_RED     "\x1b[41m"
#define BACKGROUND_GREEN   "\x1b[42m"
#define BACKGROUND_YELLOW  "\x1b[43m"
#define BACKGROUND_BLUE    "\x1b[44m"
#define BACKGROUND_MAGENTA "\x1b[45m"
#define BACKGROUND_CYAN    "\x1b[46m"
#define BACKGROUND_WHITE   "\x1b[47m"

#define STYLE_BOLD         "\x1b[1m"
#define STYLE_ITALIC       "\x1b[3m"
#define STYLE_UNDERLINE    "\x1b[4m"


/* ***** [ MAP STATUS ] ***** */

uint current_room = 0;

/* *************************************************************************** */


#define MAP_SIZE 10
#define DIFFICULTY ( 3 % MAP_SIZE )

typedef unsigned int uint;

typedef struct {
    uint room_id;
    uint num_of_doors;
    bool is_explored;
} room;

// UTILS
void printMap(uint[MAP_SIZE][MAP_SIZE]);
uint randRoom();
//
void addCorridor(uint[MAP_SIZE][MAP_SIZE], uint, uint);

// ADJ
uint *getAdj(uint[MAP_SIZE][MAP_SIZE], uint);
uint countAjd(uint[MAP_SIZE][MAP_SIZE], uint);
void showAdj(uint[MAP_SIZE][MAP_SIZE], uint room);

// GAME RELATED
void askNextRoom(uint[MAP_SIZE][MAP_SIZE]);
void generateRandomMap(uint[MAP_SIZE][MAP_SIZE]);


/* *************************************************************************** */

void printMap(uint map[MAP_SIZE][MAP_SIZE]) {
    for (int i = 0; i < MAP_SIZE; i++) {
        for (int j = 0; j < MAP_SIZE; j++) {
            printf("%d ", map[i][j]);
        }
        printf("\n");
    }
}

void addCorridor(uint map[MAP_SIZE][MAP_SIZE], uint i, uint j) {
    map[i][j] = 1;
    map[j][i] = 1;
}

uint randRoom() {
    return rand() % MAP_SIZE;
}

// ***** [ ADJ ]

uint *getAdj(uint map[MAP_SIZE][MAP_SIZE], uint room) {
    uint *adjs = (uint*)malloc(countAjd(map, room) * sizeof(uint));    
    for(int i = 0, j = 0; i < MAP_SIZE; i++) {
        if (map[room][i]) {
            adjs[j] = i;
            j++;
        }
    }
    return adjs;
}

uint countAjd(uint map[MAP_SIZE][MAP_SIZE], uint room) {
    uint counter = 0;
    for (int i = 0; i < MAP_SIZE; i++) {
        if(map[room][i] == 1) { counter++;}
    }
    return counter;
}

void showAdj(uint map[MAP_SIZE][MAP_SIZE], uint room) {
    uint *adjs = getAdj(map,room);
    printf("stanza (%d), adiacenti : "GREEN" ", room);
    for(int i = 0; i < countAjd(map,room); i++) { printf("%d ", adjs[i]);}
    printf(""RESET_COLOR"\n");
}

// ***** [ GAME RELATED ]

void askNextRoom(uint map[MAP_SIZE][MAP_SIZE]) {
    printf("In questo momento ti trovi nella stanza %d, dove vuoi muoverti?\n", current_room);
    showAdj(map, current_room);
    scanf("%d", &current_room);
}

void generateRandomMap(uint map[MAP_SIZE][MAP_SIZE]) {
    uint rooms_number = (rand() % MAP_SIZE) * (MAP_SIZE - (MAP_SIZE - DIFFICULTY));
    printf("Stanze da generare: %d\n", rooms_number);
    for(int i = 0; i < rooms_number; i++) {
        addCorridor(map, rand() % MAP_SIZE, rand() % MAP_SIZE);
    }
}


/* *************************************************************************** */


int main() {
    int seed = (int)time(NULL);
    srand(seed);

    room *rooms = (room*)malloc(MAP_SIZE * sizeof(room));
    uint map[MAP_SIZE][MAP_SIZE] = { 0 };
    
    /*
    addCorridor(map,0,1);
    addCorridor(map,0,2);
    addCorridor(map,0,3);
    addCorridor(map,0,4);
    
    addCorridor(map,1,5);
    addCorridor(map,1,6);
    addCorridor(map,1,7);
    
    addCorridor(map,7,8);
    
    addCorridor(map,8,9);
    */

    generateRandomMap(map);
    printMap(map);
    printf("\n");


    while(true) {
        askNextRoom(map);
    }


    return 0;

}