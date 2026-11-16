#ifndef LEO_GAME2048_H
#define LEO_GAME2048_H
#include "types.h"
/* 2048 (by Luna): 4x4 board, arrow keys / WASD, swipe on the board, or tap the arrow pad */
#define G2048_N     4
#define G2048_CELL  64
#define G2048_GAP   8
#define G2048_BX    10
#define G2048_BY    40
#define G2048_BOARD (G2048_N * G2048_CELL + (G2048_N + 1) * G2048_GAP)
#define G2048_PANEL (G2048_BX + G2048_BOARD + 12)
#define G2048_W     (G2048_PANEL + 3 * 48 + 10)
#define G2048_H     (G2048_BY + G2048_BOARD + 10)
#define G2048_BG    0xD4E6D8
void g2048_init(void);
void g2048_draw(int x, int y, int w, int h, bool focused);
void g2048_click(int lx, int ly);
void g2048_drag(int lx, int ly);
void g2048_release(void);
void g2048_key(int c);
void g2048_icon(int x, int y);
#endif
