#ifndef LEO_DEEPDIVE_H
#define LEO_DEEPDIVE_H
#include "types.h"
/* Deep Dive (by Luna): a little underwater game inspired by Subnautica.
 * Swim with arrow keys / WASD, or hold your finger/mouse where you want to go.
 * Collect fish, avoid jellyfish, and swim up to the surface to refill oxygen. */
#define DIVE_W  420
#define DIVE_H  320
#define DIVE_BG 0x0B3B5C
void dive_init(void);
void dive_draw(int x, int y, int w, int h, bool focused);
void dive_click(int lx, int ly);
void dive_drag(int lx, int ly);
void dive_release(void);
void dive_key(int c);
void dive_icon(int x, int y);
bool dive_tick(bool focused);
#endif
