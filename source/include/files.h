/* LeoOS - Files app (File Explorer) for 0.4 */
#ifndef LEO_FILES_H
#define LEO_FILES_H
#include "types.h"

#define FILES_W 540
#define FILES_H 380
#define FILES_BG 0xF7FBF8

void files_init(void);
void files_draw(int x, int y, int w, int h, bool focused);
void files_click(int lx, int ly);
void files_release(void);
void files_key(int c);
void files_icon(int x, int y);
#endif
