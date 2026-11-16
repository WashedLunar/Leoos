#ifndef LEO_GUI_H
#define LEO_GUI_H
#include "types.h"
#include "multiboot.h"

enum { APP_TERMINAL, APP_CALC, APP_PAINT, APP_NOTEPAD, APP_FILES, APP_BROWSER, APP_SNAKE, APP_MINES, APP_2048, APP_DIVE, APP_TETRIS, APP_ABOUT, NAPPS };

/* Every app gets one window. Coordinates passed to click/drag are relative to
 * the window's client area; draw() receives the client rect in screen space
 * with the clip already set to it. */
struct app {
    const char *name;
    int w, h;                                   /* default client area size */
    uint32_t bg;
    void (*draw)(int x, int y, int w, int h, bool focused);
    void (*click)(int lx, int ly);
    void (*drag)(int lx, int ly);
    void (*release)(void);
    void (*key)(int c);
    void (*icon)(int x, int y);                 /* 32x32 icon */
    /* new in 0.3 (all optional) */
    bool resizable;
    int min_w, min_h;                           /* smallest client size when resizing */
    int max_w, max_h;                           /* largest client size, 0 = screen size */
    bool (*tick)(bool focused);                 /* called every frame; return true to redraw */
    void (*rclick)(int lx, int ly);             /* right mouse button */
};

extern const struct app apps[NAPPS];
extern struct multiboot_info *gui_mbi;

void gui_run(struct multiboot_info *mbi);
void gui_open_app(int app);
void notepad_open(const char *path);   /* load a file in Notepad and raise the window */
void gui_close_app(int app);
void gui_client_size(int app, int *w, int *h);   /* current client size of an app's window */
void apps_init(void);
#endif
