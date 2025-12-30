#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "header/color.h"
#include "header/gui.h"


void print_line(char c, int n) {
    for(int i = 0; i < n; i++) {
        printf("%c",c);
        fflush(stdout);
    }
}

void welcome_print() {
    print_line('-', 40);
    print_line('\n', 10);
    print_line(' ', 5); printf("welcome to\n");
    print_line(' ', 5); printf("<== DUNGEON EXPLORER ==>\n");
    print_line(' ', 5); printf("Press ENTER to enter lobby!\n");
    print_line('\n', 10);
    print_line('-', 40);
}