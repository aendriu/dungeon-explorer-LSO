#include "../header/ui_helpers.h"
#include <string.h>

void show_popup(WINDOW *win, const char *line1, const char *line2,
                const char *line3) {
  werase(win);
  box(win, 0, 0);
  if (line1)
    mvwprintw(win, 2, 2, "%s", line1);
  if (line2)
    mvwprintw(win, 3, 2, "%s", line2);
  if (line3)
    mvwprintw(win, 5, 2, "%s", line3);
  wrefresh(win);
}

void copy_default_or_input(const char *src, const char *fallback, char *dst,
                           size_t dst_sz) {
  if (dst == NULL || dst_sz == 0)
    return;
  const char *val = (src && src[0] != '\0') ? src : fallback;
  strncpy(dst, val, dst_sz - 1);
  dst[dst_sz - 1] = '\0';
}

void draw_centered_title(int screen_width, const char *title, int y) {
  int x = (screen_width - (int)strlen(title)) / 2;
  if (x < 0)
    x = 0;
  attron(A_BOLD);
  mvprintw(y, x, "%s", title);
  attroff(A_BOLD);
}
