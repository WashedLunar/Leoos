/* LeoOS - Tetris app (by Starlink). 10x20 well, 7-bag randomiser, next piece,
 * score/level, ghost piece. Arrow keys / WASD, or tap the on-screen pad
 * (phone friendly). Space hard-drops; P pauses; N starts a new game. */
#ifndef LEO_TETRIS_H
#define LEO_TETRIS_H
#include "types.h"

#define TETRIS_COLS 10
#define TETRIS_ROWS 20
#define TETRIS_CELL 24
#define TETRIS_BX   10
#define TETRIS_BY   10
#define TETRIS_BOARD_W (TETRIS_COLS * TETRIS_CELL)
#define TETRIS_BOARD_H (TETRIS_ROWS * TETRIS_CELL)
#define TETRIS_PANEL   (TETRIS_BX + TETRIS_BOARD_W + 12)
#define TETRIS_PADW    48
#define TETRIS_W (TETRIS_PANEL + 3 * TETRIS_PADW + 10)
#define TETRIS_H (TETRIS_BY + TETRIS_BOARD_H + 10)
#define TETRIS_BG 0xD4E6D8

void tetris_init(void);
void tetris_draw(int x, int y, int w, int h, bool focused);
void tetris_click(int lx, int ly);
void tetris_release(void);
void tetris_key(int c);
void tetris_icon(int x, int y);
/* Called every GUI frame (~100 Hz when idle). Return true to redraw.
 * Counts its own frames so this file does not include cpu.h. */
bool tetris_tick(bool focused);
#endif
