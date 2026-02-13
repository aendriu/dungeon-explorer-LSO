#ifndef CLIENT_UI_HELPERS_H
#define CLIENT_UI_HELPERS_H

#include <ncurses.h>
#include <stddef.h>

void show_popup(WINDOW *win, const char *line1, const char *line2,
                const char *line3);
void copy_default_or_input(const char *src, const char *fallback, char *dst,
                           size_t dst_sz);
void draw_centered_title(int screen_width, const char *title, int y);

#endif
