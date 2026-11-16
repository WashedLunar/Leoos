/* LeoOS - Browser app (by Vega). Browses built-in leo:// pages and pages you
 * write yourself in Notepad (file:NAME). No internet yet: LeoOS has no network
 * driver, so http:// addresses show a friendly explanation page instead. */
#ifndef LEO_BROWSER_H
#define LEO_BROWSER_H
#include "types.h"

#define BROWSER_W  600
#define BROWSER_H  420
#define BROWSER_BG 0xFFFFFF

void browser_init(void);
void browser_draw(int x, int y, int w, int h, bool focused);
void browser_click(int lx, int ly);
void browser_drag(int lx, int ly);
void browser_release(void);
void browser_key(int c);
void browser_icon(int x, int y);
#endif
