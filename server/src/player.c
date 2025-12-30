#include "header/player.h"

#include <stdio.h>

const char* pl_1_symbol = "1";
const char* pl_2_symbol = "2";
const char* pl_3_symbol = "3";
const char* pl_4_symbol = "4";

void *handle_player(void* args) {
	(void)args;
    printf("Im handling player!\n");
    return NULL;
}
