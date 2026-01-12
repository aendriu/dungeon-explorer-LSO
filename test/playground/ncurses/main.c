#include <stdio.h>
#include <ncurses.h>

#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) < (b) ? (a) : (b))

int y=0,x=0;

void menu() {
    int ch;
    int menu_y = (y/2) - 2;
    int menu_x = (x/2) - 10;

    char *items[] = { "entra nel gioco", "opzioni", "(q) esci" };
    int n_items = 3;
    int current = 0;

    curs_set(0);      // nasconde cursore
    keypad(stdscr, TRUE); // frecce

    while((ch = getch()) != 'q') {
        clear();
        mvaddstr(menu_y, menu_x, "SAVE MAN");

        // disegna menu
        for(int i = 0; i < n_items; i++) {
            if(i == current) attron(A_REVERSE); // evidenzia
            mvaddstr(menu_y+1+i, menu_x+3, items[i]);
            if(i == current) attroff(A_REVERSE);
        }

        refresh();

        // input
        if(ch == KEY_UP && current > 0) current--;
        else if(ch == KEY_DOWN && current < n_items-1) current++;
    }

    clear();
    refresh();
}


int main() {

	char ch;
	initscr();
	noecho();
	keypad(stdscr, TRUE); // permette di leggere frecce e tasti speciali
	
	getmaxyx(stdscr, y,x);
	while( (ch = getch()) != '\n') {
		clear();
		getmaxyx(stdscr, y,x);
		printw("Screen size : (y = %d, x = %d)\n",y,x);
		printw("You've pressed %c\n", ch);
		if(ch == 'w' || ch == 'W') {
			printw("you walk north");
		}

		if(ch == 'm' || ch == 'M') {
			menu();
		}
		refresh();
	}
	endwin();  // chiude ncurses e ripristina terminale

	return 0;
}
