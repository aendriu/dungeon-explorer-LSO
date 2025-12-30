#include "header/player.h"

#include <stdio.h>

void *handle_player(void* args) {
	(void)args;
    printf("Im handling player!\n");
    return NULL;
}
