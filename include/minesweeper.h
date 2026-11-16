/* LeoOS - Minesweeper app (by Luna). 9x9 grid, 10 mines, safe first click,
 * Flag-mode button for phone taps (or the F key); right-click also flags. */
#ifndef LEO_MINESWEEPER_H
#define LEO_MINESWEEPER_H
#include "types.h"

#define MINES_COLS  9
#define MINES_ROWS  9
#define MINES_COUNT 10
#define MINES_CELL  28
#define MINES_TOP   44                 /* toolbar height above the grid */
#define MINES_PAD   10
#define MINES_W (MINES_PAD * 2 + MINES_COLS * MINES_CELL)
#define MINES_H (MINES_TOP + MINES_PAD + MINES_ROWS * MINES_CELL)
#define MINES_BG 0xC8DCCB

void mines_init(void);
void mines_draw(int x, int y, int w, int h, bool focused);
void mines_click(int lx, int ly);
void mines_release(void);
void mines_rclick(int lx, int ly);
void mines_key(int c);
void mines_icon(int x, int y);
#endif
