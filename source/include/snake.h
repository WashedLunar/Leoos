#ifndef LEO_SNAKE_H
#define LEO_SNAKE_H
#include "types.h"
/* Board: 20 x 15 cells of 18 px, plus a control panel with big tap-friendly arrows */
#define SNAKE_COLS  20
#define SNAKE_ROWS  15
#define SNAKE_CELL  18
#define SNAKE_BX    10                              /* board position inside the window */
#define SNAKE_BY    36
#define SNAKE_PANEL (SNAKE_BX + SNAKE_COLS * SNAKE_CELL + 12)
#define SNAKE_W     (SNAKE_PANEL + 3 * 52 + 10)
#define SNAKE_H     (SNAKE_BY + SNAKE_ROWS * SNAKE_CELL + 10)
#define SNAKE_BG    0xD4E6D8

void snake_init(void);
void snake_draw(int x, int y, int w, int h, bool focused);
void snake_click(int lx, int ly);
void snake_release(void);
void snake_key(int c);
void snake_icon(int x, int y);
bool snake_tick(bool focused);
#endif
