#ifndef LEO_GUI_H
#define LEO_GUI_H
#include "types.h"
#include "multiboot.h"

enum { APP_TERMINAL, APP_CALC, APP_PAINT, APP_NOTEPAD, APP_ABOUT, NAPPS };

/* Every app gets one window. Coordinates passed to click/drag are relative to
 * the window's client area; draw() receives the client rect in screen space
 * with the clip already set to it. */
struct app {
    const char *name;
    int w, h;                                   /* client area size */
    uint32_t bg;
    void (*draw)(int x, int y, int w, int h, bool focused);
    void (*click)(int lx, int ly);
    void (*drag)(int lx, int ly);
    void (*release)(void);
    void (*key)(int c);
    void (*icon)(int x, int y);                 /* 32x32 icon */
};

extern const struct app apps[NAPPS];
extern struct multiboot_info *gui_mbi;

void gui_run(struct multiboot_info *mbi);
void gui_open_app(int app);
void gui_close_app(int app);
void apps_init(void);
#endif
