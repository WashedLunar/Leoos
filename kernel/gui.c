/* LeoOS - desktop, window manager, taskbar and Start menu */
#include "gui.h"
#include "gfx.h"
#include "cpu.h"
#include "io.h"
#include "vga.h"
#include "string.h"
#include "version.h"

#define TITLE_H   26
#define BORDER    2
#define TASKBAR_H 40
#define MENU_W    230
#define MENU_HEAD 34
#define MENU_ITEM 38
#define MENU_SYS  30

/* Leo's green theme */
#define C_DESK_TOP    RGB(0x3D, 0xB8, 0x5C)
#define C_DESK_BOTTOM RGB(0x0B, 0x47, 0x1E)
#define C_TITLE_A     RGB(0x2F, 0xA8, 0x4F)
#define C_TITLE_B     RGB(0x17, 0x6E, 0x32)
#define C_TITLE_OFF   RGB(0x7F, 0x99, 0x86)
#define C_FRAME       RGB(0x0E, 0x4D, 0x22)
#define C_TASK_A      RGB(0x1A, 0x5E, 0x30)
#define C_TASK_B      RGB(0x0A, 0x30, 0x17)
#define C_BUTTON      RGB(0x24, 0x80, 0x40)

struct multiboot_info *gui_mbi;

struct window { bool open; int x, y; };
static struct window wins[NAPPS];
static int zorder[NAPPS], zcount;              /* bottom ... top */
static int mx, my, buttons;                    /* mouse state */
static int drag_app = -1, drag_dx, drag_dy;    /* window being moved */
static int capture_app = -1;                   /* app receiving a mouse drag */
static bool menu_open;
static bool start_pressed;

static const int default_pos[NAPPS][2] = {
    { 150, 40 }, { 760, 60 }, { 280, 120 }, { 400, 170 }, { 590, 330 },
};

static int win_w(int a) { return apps[a].w + 2 * BORDER; }
static int win_h(int a) { return apps[a].h + TITLE_H + BORDER; }
static int top_app(void) { return zcount ? zorder[zcount - 1] : -1; }
static bool inside(int px, int py, int x, int y, int w, int h) { return px >= x && py >= y && px < x + w && py < y + h; }

/* ------------------------------------------------------------- window list */
static void raise(int a) {
    int i;
    for (i = 0; i < zcount && zorder[i] != a; i++) ;
    if (i == zcount) return;
    for (; i < zcount - 1; i++) zorder[i] = zorder[i + 1];
    zorder[zcount - 1] = a;
}

void gui_open_app(int a) {
    if (!wins[a].open) {
        wins[a].open = true;
        wins[a].x = default_pos[a][0];
        wins[a].y = default_pos[a][1];
        if (wins[a].x + win_w(a) > gfx_w) wins[a].x = gfx_w - win_w(a);
        if (wins[a].y + win_h(a) > gfx_h - TASKBAR_H) wins[a].y = gfx_h - TASKBAR_H - win_h(a);
        if (wins[a].x < 0) wins[a].x = 0;
        if (wins[a].y < 0) wins[a].y = 0;
        zorder[zcount++] = a;
    }
    raise(a);
}

void gui_close_app(int a) {
    if (!wins[a].open) return;
    wins[a].open = false;
    int i;
    for (i = 0; i < zcount && zorder[i] != a; i++) ;
    for (; i < zcount - 1; i++) zorder[i] = zorder[i + 1];
    zcount--;
    if (capture_app == a) capture_app = -1;
    if (drag_app == a) drag_app = -1;
}

/* ----------------------------------------------------------------- desktop */
/* integer sine (x in degrees, result scaled by 1000) using Bhaskara's approximation */
static int sin_deg(int x) {
    x %= 360; if (x < 0) x += 360;
    int sign = 1;
    if (x >= 180) { x -= 180; sign = -1; }
    return sign * 4000 * x * (180 - x) / (40500 - x * (180 - x));
}

static void icon_tile(int i, int *x, int *y) { *x = 18; *y = 18 + i * 86; }

static void draw_wallpaper(void) {
    gfx_gradient(0, 0, gfx_w, gfx_h, C_DESK_TOP, C_DESK_BOTTOM);
    /* two layers of soft rolling hills */
    int base = gfx_h - TASKBAR_H;
    for (int layer = 0; layer < 2; layer++) {
        uint32_t col = layer ? RGB(0x14, 0x62, 0x2B) : RGB(0x22, 0x84, 0x3E);
        for (int x = 0; x < gfx_w; x++) {
            int yy = base - (layer ? 70 : 140) - (sin_deg(x * (layer ? 5 : 3) / 10 + layer * 70) * (layer ? 24 : 40)) / 1000
                          - (sin_deg(x * 7 / 10 + 30) * 10) / 1000;
            for (int y = yy; y < base; y++) gfx_pixel(x, y, gfx_blend(col, C_DESK_BOTTOM, (y - yy) * 200 / (base - yy + 1)));
        }
    }
    /* watermark */
    uint32_t wm = gfx_blend(C_DESK_TOP, 0xFFFFFF, 60);
    gfx_text(gfx_w - 5 * 8 * 7 - 40, gfx_h - TASKBAR_H - 7 * 16 - 60, "LeoOS", gfx_blend(C_DESK_BOTTOM, 0, 60), TRANSPARENT, 7);
    gfx_text(gfx_w - 5 * 8 * 7 - 44, gfx_h - TASKBAR_H - 7 * 16 - 64, "LeoOS", wm, TRANSPARENT, 7);
    gfx_text(gfx_w - 15 * 8 * 2 - 40, gfx_h - TASKBAR_H - 50, "version " LEOOS_VERSION, wm, TRANSPARENT, 2);
    /* desktop icons */
    for (int i = 0; i < NAPPS; i++) {
        int x, y; icon_tile(i, &x, &y);
        apps[i].icon(x + 24, y + 4);
        const char *n = apps[i].name;
        int tw = (int)strlen(n) * FONT_W;
        int tx = x + 40 - tw / 2;
        gfx_text(tx + 1, y + 43, n, RGB(0, 0x30, 0x10), TRANSPARENT, 1);
        gfx_text(tx, y + 42, n, 0xFFFFFF, TRANSPARENT, 1);
    }
}

/* ------------------------------------------------------------------ windows */

static void close_rect(int a, int *x, int *y, int *w, int *h) {
    *w = 22; *h = 18;
    *x = wins[a].x + win_w(a) - BORDER - *w - 4;
    *y = wins[a].y + 4;
}

static void draw_window(int a, bool focused) {
    int x = wins[a].x, y = wins[a].y, w = win_w(a), h = win_h(a);
    gfx_shade(x + 6, y + h, w, 6, 90);
    gfx_shade(x + w, y + 6, 6, h - 6, 90);
    gfx_rect(x, y, w, h, focused ? C_FRAME : RGB(0x4E, 0x66, 0x55));
    if (focused) gfx_gradient(x + 1, y + 1, w - 2, TITLE_H - 1, C_TITLE_A, C_TITLE_B);
    else gfx_fill(x + 1, y + 1, w - 2, TITLE_H - 1, C_TITLE_OFF);
    gfx_fill(x + 1, y + TITLE_H, w - 2, h - TITLE_H - 1, focused ? C_FRAME : RGB(0x4E, 0x66, 0x55));
    gfx_set_clip(x, y, w - 34, TITLE_H);
    gfx_text(x + 11, y + 6, apps[a].name, RGB(0, 0x28, 0x10), TRANSPARENT, 1);
    gfx_text(x + 10, y + 5, apps[a].name, 0xFFFFFF, TRANSPARENT, 1);
    gfx_reset_clip();
    int cx, cy, cw, ch; close_rect(a, &cx, &cy, &cw, &ch);
    gfx_bevel(cx, cy, cw, ch, focused ? RGB(0xD9, 0x45, 0x3B) : RGB(0xB0, 0x80, 0x7C), false);
    gfx_line(cx + 7, cy + 5, cx + cw - 8, cy + ch - 6, 1, 0xFFFFFF);
    gfx_line(cx + 7, cy + ch - 6, cx + cw - 8, cy + 5, 1, 0xFFFFFF);
    gfx_line(cx + 8, cy + 5, cx + cw - 7, cy + ch - 6, 1, 0xFFFFFF);
    gfx_line(cx + 8, cy + ch - 6, cx + cw - 7, cy + 5, 1, 0xFFFFFF);
    int clx = x + BORDER, cly = y + TITLE_H;
    gfx_set_clip(clx, cly, apps[a].w, apps[a].h);
    gfx_fill(clx, cly, apps[a].w, apps[a].h, apps[a].bg);
    apps[a].draw(clx, cly, apps[a].w, apps[a].h, focused);
    gfx_reset_clip();
}

/* ---------------------------------------------------------- taskbar + menu */
static int task_button_x(int i) { return 112 + i * 156; }

static int menu_h(void) { return MENU_HEAD + NAPPS * MENU_ITEM + 10 + 2 * MENU_SYS + 6; }
static int menu_y(void) { return gfx_h - TASKBAR_H - menu_h(); }

/* returns 0..NAPPS-1 for apps, NAPPS for reboot, NAPPS+1 for shut down, -1 none */
static int menu_item_at(int px, int py) {
    int y0 = menu_y();
    if (!inside(px, py, 4, y0, MENU_W, menu_h())) return -1;
    int ry = py - y0 - MENU_HEAD;
    if (ry >= 0 && ry < NAPPS * MENU_ITEM) return ry / MENU_ITEM;
    ry -= NAPPS * MENU_ITEM + 10;
    if (ry >= 0 && ry < 2 * MENU_SYS) return NAPPS + ry / MENU_SYS;
    return -1;
}

static void two(char *p, uint32_t v) { p[0] = (char)('0' + (v / 10) % 10); p[1] = (char)('0' + v % 10); }

static void draw_taskbar(void) {
    int ty = gfx_h - TASKBAR_H;
    gfx_gradient(0, ty, gfx_w, TASKBAR_H, C_TASK_A, C_TASK_B);
    gfx_fill(0, ty, gfx_w, 1, RGB(0x5C, 0xD6, 0x7C));
    /* Start button */
    gfx_bevel(4, ty + 5, 100, 30, RGB(0x37, 0xB0, 0x56), menu_open || start_pressed);
    gfx_disc(22, ty + 20, 8, RGB(0xF5, 0xD0, 0x2A));
    gfx_disc(22, ty + 20, 4, RGB(0x1F, 0x7A, 0x39));
    gfx_text(37, ty + 12, "Start", RGB(0, 0x33, 0x12), TRANSPARENT, 1);
    gfx_text(36, ty + 11, "Start", 0xFFFFFF, TRANSPARENT, 1);
    /* one button per open window */
    int n = 0, top = top_app();
    for (int a = 0; a < NAPPS; a++) {
        if (!wins[a].open) continue;
        int bx = task_button_x(n++);
        if (bx + 150 > gfx_w - 140) break;
        gfx_bevel(bx, ty + 5, 150, 30, a == top ? RGB(0x3A, 0x9E, 0x55) : C_BUTTON, a == top);
        gfx_set_clip(bx + 4, ty + 5, 142, 30);
        gfx_text(bx + 10, ty + 12, apps[a].name, 0xFFFFFF, TRANSPARENT, 1);
        gfx_reset_clip();
    }
    /* uptime clock */
    uint32_t s = timer_ticks() / timer_hz();
    char clock[] = "up 00:00:00";
    two(clock + 3, (s / 3600) % 100); two(clock + 6, (s / 60) % 60); two(clock + 9, s % 60);
    gfx_bevel(gfx_w - 124, ty + 5, 120, 30, RGB(0x12, 0x48, 0x22), true);
    gfx_text(gfx_w - 108, ty + 12, clock, RGB(0xD8, 0xFF, 0xE0), TRANSPARENT, 1);
}

static void draw_menu(void) {
    int y0 = menu_y(), h = menu_h();
    gfx_shade(10, y0 + 6, MENU_W, h - 6, 90);
    gfx_fill(4, y0, MENU_W, h, RGB(0xF4, 0xFA, 0xF5));
    gfx_rect(4, y0, MENU_W, h, C_FRAME);
    gfx_gradient(5, y0 + 1, MENU_W - 2, MENU_HEAD - 2, C_TITLE_A, C_TITLE_B);
    gfx_text(16, y0 + 9, "LeoOS " LEOOS_VERSION, 0xFFFFFF, TRANSPARENT, 1);
    gfx_text(MENU_W - 70, y0 + 9, "Leo", RGB(0xF5, 0xD0, 0x2A), TRANSPARENT, 1);
    int hover = menu_item_at(mx, my);
    for (int a = 0; a < NAPPS; a++) {
        int iy = y0 + MENU_HEAD + a * MENU_ITEM;
        if (hover == a) gfx_fill(6, iy, MENU_W - 4, MENU_ITEM, RGB(0xC6, 0xEE, 0xD0));
        apps[a].icon(14, iy + 3);
        gfx_text(56, iy + 11, apps[a].name, RGB(0x10, 0x30, 0x18), TRANSPARENT, 1);
    }
    int sy = y0 + MENU_HEAD + NAPPS * MENU_ITEM + 4;
    gfx_fill(12, sy, MENU_W - 16, 1, RGB(0xA8, 0xC8, 0xB0));
    static const char *sys[2] = { "Restart", "Shut down" };
    for (int i = 0; i < 2; i++) {
        int iy = sy + 6 + i * MENU_SYS;
        if (hover == NAPPS + i) gfx_fill(6, iy, MENU_W - 4, MENU_SYS, RGB(0xC6, 0xEE, 0xD0));
        gfx_disc(24, iy + 15, 6, i ? RGB(0xD9, 0x45, 0x3B) : RGB(0xE8, 0xA0, 0x20));
        gfx_text(56, iy + 7, sys[i], RGB(0x10, 0x30, 0x18), TRANSPARENT, 1);
    }
}

static const char *cursor_img[] = {
    "X", "XX", "X.X", "X..X", "X...X", "X....X", "X.....X", "X......X", "X.......X",
    "X........X", "X.........X", "X......XXXXX", "X...X..X", "X..XX..X", "X.X  X..X",
    "XX   X..X", "X     X..X", "      X..X", "       XX",
};

static void draw_cursor(void) {
    for (int r = 0; r < 19; r++)
        for (int c = 0; cursor_img[r][c]; c++) {
            char p = cursor_img[r][c];
            if (p == 'X') gfx_pixel(mx + c, my + r, 0x000000);
            else if (p == '.') gfx_pixel(mx + c, my + r, 0xFFFFFF);
        }
}

static void compose(void) {
    gfx_restore_background();
    int top = top_app();
    for (int i = 0; i < zcount; i++) draw_window(zorder[i], zorder[i] == top);
    draw_taskbar();
    if (menu_open) draw_menu();
    draw_cursor();
    gfx_flip();
}

/* ------------------------------------------------------------------ input */
extern void shell_command_reboot(void);
extern void shell_command_shutdown(void);

static void mouse_down(void) {
    int ty = gfx_h - TASKBAR_H;
    if (menu_open) {
        int item = menu_item_at(mx, my);
        menu_open = false;
        if (item >= 0 && item < NAPPS) { gui_open_app(item); return; }
        if (item == NAPPS) { shell_command_reboot(); return; }
        if (item == NAPPS + 1) { shell_command_shutdown(); return; }
        if (inside(mx, my, 4, ty + 5, 100, 30)) return;   /* Start clicked again: just close */
    }
    if (my >= ty) {                                 /* taskbar */
        if (inside(mx, my, 4, ty + 5, 100, 30)) { menu_open = true; return; }
        int n = 0;
        for (int a = 0; a < NAPPS; a++) {
            if (!wins[a].open) continue;
            if (inside(mx, my, task_button_x(n++), ty + 5, 150, 30)) { raise(a); return; }
        }
        return;
    }
    for (int i = zcount - 1; i >= 0; i--) {         /* windows, top first */
        int a = zorder[i];
        if (!inside(mx, my, wins[a].x, wins[a].y, win_w(a), win_h(a))) continue;
        raise(a);
        int cx, cy, cw, ch; close_rect(a, &cx, &cy, &cw, &ch);
        if (inside(mx, my, cx, cy, cw, ch)) { gui_close_app(a); return; }
        if (my < wins[a].y + TITLE_H) { drag_app = a; drag_dx = mx - wins[a].x; drag_dy = my - wins[a].y; return; }
        int lx = mx - wins[a].x - BORDER, ly = my - wins[a].y - TITLE_H;
        if (lx >= 0 && ly >= 0 && lx < apps[a].w && ly < apps[a].h) {
            capture_app = a;
            if (apps[a].click) apps[a].click(lx, ly);
        }
        return;
    }
    for (int i = 0; i < NAPPS; i++) {               /* desktop icons */
        int x, y; icon_tile(i, &x, &y);
        if (inside(mx, my, x, y, 80, 64)) { gui_open_app(i); return; }
    }
}

static void mouse_drag(void) {
    if (drag_app >= 0) {
        int a = drag_app, nx = mx - drag_dx, ny = my - drag_dy;
        if (nx < 60 - win_w(a)) nx = 60 - win_w(a);
        if (nx > gfx_w - 60) nx = gfx_w - 60;
        if (ny < 0) ny = 0;
        if (ny > gfx_h - TASKBAR_H - TITLE_H) ny = gfx_h - TASKBAR_H - TITLE_H;
        wins[a].x = nx; wins[a].y = ny;
    } else if (capture_app >= 0 && apps[capture_app].drag) {
        int a = capture_app;
        apps[a].drag(mx - wins[a].x - BORDER, my - wins[a].y - TITLE_H);
    }
}

static void mouse_up(void) {
    if (capture_app >= 0 && apps[capture_app].release) apps[capture_app].release();
    drag_app = -1; capture_app = -1;
}

static void key(int c) {
    if (menu_open && c == 27) { menu_open = false; return; }
    int a = top_app();
    if (a >= 0 && apps[a].key) apps[a].key(c);
}

void gui_run(struct multiboot_info *mbi) {
    gui_mbi = mbi;
    draw_wallpaper();
    gfx_save_background();
    mx = gfx_w / 2; my = gfx_h / 2;
    gui_open_app(APP_TERMINAL);
    gui_open_app(APP_ABOUT);
    uint32_t last_half = 0xFFFFFFFF;
    for (;;) {
        bool redraw = false;
        int c;
        while ((c = keyboard_poll()) >= 0) { key(c); redraw = true; }
        int dx, dy, b;
        while (mouse_poll(&dx, &dy, &b)) {
            mx += dx; my += dy;
            if (mx < 0) mx = 0;
            if (my < 0) my = 0;
            if (mx >= gfx_w) mx = gfx_w - 1;
            if (my >= gfx_h) my = gfx_h - 1;
            bool was = buttons & 1, now = b & 1;
            if (now && !was) mouse_down();
            else if (now && (dx || dy)) mouse_drag();
            else if (!now && was) mouse_up();
            buttons = b;
            redraw = true;
        }
        if (vga_take_dirty()) redraw = true;
        uint32_t half = timer_ticks() / (timer_hz() / 2);   /* clock + caret blink */
        if (half != last_half) { last_half = half; redraw = true; }
        if (redraw) compose();
        else cpu_hlt();
    }
}
