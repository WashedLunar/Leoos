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
#define TBTN_W    22          /* title bar button */
#define TBTN_H    18
#define GRIP      22          /* resize grip drawn in the bottom-right corner */
#define GRIP_IN   34          /* ...but it can be grabbed this far inside the corner */
#define GRIP_OUT  12          /* ...and this far outside it (easy to hit with a finger) */

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

struct window {
    bool open, minimised, maximised;
    int x, y, w, h;                            /* position and client size */
    int sx, sy, sw, sh;                        /* saved geometry while maximised */
};
static struct window wins[NAPPS];
static int zorder[NAPPS], zcount;              /* bottom ... top (minimised windows stay in the list) */
static int mx, my, buttons;                    /* mouse state */
static int drag_app = -1, drag_dx, drag_dy;    /* window being moved */
static int size_app = -1, size_dx, size_dy;    /* window being resized */
static int capture_app = -1;                   /* app receiving a mouse drag */
static int pressed_btn = -1, pressed_app = -1; /* title bar button held down */
static bool menu_open;
static bool start_pressed;

static const int default_pos[NAPPS][2] = {
    { 150, 40 }, { 760, 60 }, { 280, 120 }, { 400, 170 }, { 330, 90 }, { 620, 140 }, { 590, 330 },
};

static int win_w(int a) { return wins[a].w + 2 * BORDER; }
static int win_h(int a) { return wins[a].h + TITLE_H + BORDER; }
static bool inside(int px, int py, int x, int y, int w, int h) { return px >= x && py >= y && px < x + w && py < y + h; }

/* The focused window is the topmost one that isn't minimised; it gets the keyboard. */
static int top_app(void) {
    for (int i = zcount - 1; i >= 0; i--) if (!wins[zorder[i]].minimised) return zorder[i];
    return -1;
}

void gui_client_size(int a, int *w, int *h) { *w = wins[a].w; *h = wins[a].h; }

static int max_cw(int a) {
    int m = gfx_w - 2 * BORDER;
    return apps[a].max_w && apps[a].max_w < m ? apps[a].max_w : m;
}
static int max_ch(int a) {
    int m = gfx_h - TASKBAR_H - TITLE_H - BORDER;
    return apps[a].max_h && apps[a].max_h < m ? apps[a].max_h : m;
}
static bool can_maximise(int a) { return apps[a].resizable && !apps[a].max_w && !apps[a].max_h; }

/* ------------------------------------------------------------- window list */
static void raise(int a) {
    int i;
    for (i = 0; i < zcount && zorder[i] != a; i++) ;
    if (i == zcount) return;
    for (; i < zcount - 1; i++) zorder[i] = zorder[i + 1];
    zorder[zcount - 1] = a;
}

static void fit_on_screen(int a) {
    if (wins[a].x + win_w(a) > gfx_w) wins[a].x = gfx_w - win_w(a);
    if (wins[a].y + win_h(a) > gfx_h - TASKBAR_H) wins[a].y = gfx_h - TASKBAR_H - win_h(a);
    if (wins[a].x < 0) wins[a].x = 0;
    if (wins[a].y < 0) wins[a].y = 0;
}

void gui_open_app(int a) {
    if (!wins[a].open) {
        wins[a].open = true;
        wins[a].minimised = wins[a].maximised = false;
        wins[a].w = apps[a].w; wins[a].h = apps[a].h;
        wins[a].x = default_pos[a][0];
        wins[a].y = default_pos[a][1];
        fit_on_screen(a);
        zorder[zcount++] = a;
    }
    wins[a].minimised = false;
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
    if (size_app == a) size_app = -1;
}

static void minimise(int a) {
    wins[a].minimised = true;
    if (capture_app == a) { if (apps[a].release) apps[a].release(); capture_app = -1; }
    if (drag_app == a) drag_app = -1;
    if (size_app == a) size_app = -1;
}

static void toggle_maximise(int a) {
    struct window *w = &wins[a];
    if (!w->maximised) {
        w->sx = w->x; w->sy = w->y; w->sw = w->w; w->sh = w->h;
        w->w = max_cw(a); w->h = max_ch(a);
        w->x = 0; w->y = 0;
        w->maximised = true;
    } else {
        w->x = w->sx; w->y = w->sy; w->w = w->sw; w->h = w->sh;
        w->maximised = false;
        fit_on_screen(a);
    }
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
/* title bar buttons, right to left: 0 = close, 1 = maximise (resizable apps), 2 = minimise */
static int title_buttons(int a) { return can_maximise(a) ? 3 : 2; }

static bool title_button_rect(int a, int b, int *x, int *y) {
    if (b == 1 && !can_maximise(a)) return false;
    int slot = (b == 2 && !can_maximise(a)) ? 1 : b;
    *x = wins[a].x + win_w(a) - BORDER - 4 - (slot + 1) * TBTN_W - slot * 3;
    *y = wins[a].y + 4;
    return true;
}

static void draw_title_button(int a, int b, bool focused) {
    int x, y;
    if (!title_button_rect(a, b, &x, &y)) return;
    bool down = pressed_app == a && pressed_btn == b && inside(mx, my, x, y, TBTN_W, TBTN_H);
    uint32_t face = b == 0 ? (focused ? RGB(0xD9, 0x45, 0x3B) : RGB(0xB0, 0x80, 0x7C))
                           : (focused ? RGB(0x3B, 0xB8, 0x5C) : RGB(0x9A, 0xB4, 0xA0));
    gfx_bevel(x, y, TBTN_W, TBTN_H, face, down);
    if (b == 0) {
        gfx_line(x + 7, y + 5, x + TBTN_W - 8, y + TBTN_H - 6, 1, 0xFFFFFF);
        gfx_line(x + 7, y + TBTN_H - 6, x + TBTN_W - 8, y + 5, 1, 0xFFFFFF);
        gfx_line(x + 8, y + 5, x + TBTN_W - 7, y + TBTN_H - 6, 1, 0xFFFFFF);
        gfx_line(x + 8, y + TBTN_H - 6, x + TBTN_W - 7, y + 5, 1, 0xFFFFFF);
    } else if (b == 1) {
        if (wins[a].maximised) {
            gfx_rect(x + 8, y + 4, 9, 7, 0xFFFFFF);
            gfx_fill(x + 5, y + 7, 9, 7, face);
            gfx_rect(x + 5, y + 7, 9, 7, 0xFFFFFF);
            gfx_fill(x + 5, y + 7, 9, 2, 0xFFFFFF);
        } else {
            gfx_rect(x + 6, y + 4, 10, 10, 0xFFFFFF);
            gfx_fill(x + 6, y + 4, 10, 2, 0xFFFFFF);
        }
    } else {
        gfx_fill(x + 6, y + 12, 10, 2, 0xFFFFFF);
    }
}

static bool in_grip(int a, int px, int py) {
    if (!apps[a].resizable || wins[a].maximised) return false;
    int x = wins[a].x + win_w(a), y = wins[a].y + win_h(a);
    return inside(px, py, x - GRIP_IN, y - GRIP_IN, GRIP_IN + GRIP_OUT, GRIP_IN + GRIP_OUT);
}

static void draw_window(int a, bool focused) {
    int x = wins[a].x, y = wins[a].y, w = win_w(a), h = win_h(a);
    gfx_shade(x + 6, y + h, w, 6, 90);
    gfx_shade(x + w, y + 6, 6, h - 6, 90);
    gfx_rect(x, y, w, h, focused ? C_FRAME : RGB(0x4E, 0x66, 0x55));
    if (focused) gfx_gradient(x + 1, y + 1, w - 2, TITLE_H - 1, C_TITLE_A, C_TITLE_B);
    else gfx_fill(x + 1, y + 1, w - 2, TITLE_H - 1, C_TITLE_OFF);
    gfx_fill(x + 1, y + TITLE_H, w - 2, h - TITLE_H - 1, focused ? C_FRAME : RGB(0x4E, 0x66, 0x55));
    gfx_set_clip(x, y, w - 12 - title_buttons(a) * (TBTN_W + 3), TITLE_H);
    gfx_text(x + 11, y + 6, apps[a].name, RGB(0, 0x28, 0x10), TRANSPARENT, 1);
    gfx_text(x + 10, y + 5, apps[a].name, 0xFFFFFF, TRANSPARENT, 1);
    gfx_reset_clip();
    for (int b = 0; b < 3; b++) draw_title_button(a, b, focused);
    int clx = x + BORDER, cly = y + TITLE_H;
    gfx_set_clip(clx, cly, wins[a].w, wins[a].h);
    gfx_fill(clx, cly, wins[a].w, wins[a].h, apps[a].bg);
    apps[a].draw(clx, cly, wins[a].w, wins[a].h, focused);
    gfx_reset_clip();
    if (apps[a].resizable && !wins[a].maximised) {          /* resize grip: a corner tab with diagonal ridges */
        int gx = x + w - 1, gy = y + h - 1;
        gfx_fill(gx - GRIP + 1, gy - GRIP + 1, GRIP, GRIP, focused ? C_TITLE_A : RGB(0x9A, 0xB4, 0xA0));
        gfx_rect(gx - GRIP + 1, gy - GRIP + 1, GRIP, GRIP, C_FRAME);
        for (int i = 0; i < 3; i++) {
            int o = 6 + i * 5;
            gfx_line(gx - o, gy - 3, gx - 3, gy - o, 1, 0xFFFFFF);
        }
    }
}

/* ---------------------------------------------------------- taskbar + menu */
#define TASK_X0 112
static int task_count(void) { int n = 0; for (int a = 0; a < NAPPS; a++) if (wins[a].open) n++; return n; }
static int task_button_w(void) {
    int n = task_count(), avail = gfx_w - 132 - TASK_X0;
    int w = n ? avail / n - 6 : 150;
    return w > 150 ? 150 : w;
}
static int task_button_x(int i) { return TASK_X0 + i * (task_button_w() + 6); }

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
    /* one button per open window; minimised ones look sunken-dark with a dash */
    int n = 0, top = top_app(), bw = task_button_w();
    for (int a = 0; a < NAPPS; a++) {
        if (!wins[a].open) continue;
        int bx = task_button_x(n++);
        uint32_t face = a == top ? RGB(0x3A, 0x9E, 0x55) : wins[a].minimised ? RGB(0x15, 0x4A, 0x25) : C_BUTTON;
        gfx_bevel(bx, ty + 5, bw, 30, face, a == top);
        gfx_set_clip(bx + 4, ty + 5, bw - 8, 30);
        apps[a].icon(bx + 4, ty + 4);                       /* 32x32 icon, clipped */
        gfx_reset_clip();
        gfx_set_clip(bx + 30, ty + 5, bw - 34, 30);
        gfx_text(bx + 32, ty + 12, apps[a].name, wins[a].minimised ? RGB(0xA8, 0xD8, 0xB4) : 0xFFFFFF, TRANSPARENT, 1);
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
    for (int i = 0; i < zcount; i++) if (!wins[zorder[i]].minimised) draw_window(zorder[i], zorder[i] == top);
    draw_taskbar();
    if (menu_open) draw_menu();
    draw_cursor();
    gfx_flip();
}

/* ------------------------------------------------------------------ input */
extern void shell_command_reboot(void);
extern void shell_command_shutdown(void);

static void taskbar_click(int a) {
    if (wins[a].minimised) { wins[a].minimised = false; raise(a); }
    else if (a == top_app()) minimise(a);       /* clicking the focused window's button hides it */
    else raise(a);
}

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
        int n = 0, bw = task_button_w();
        for (int a = 0; a < NAPPS; a++) {
            if (!wins[a].open) continue;
            if (inside(mx, my, task_button_x(n++), ty + 5, bw, 30)) { taskbar_click(a); return; }
        }
        return;
    }
    for (int i = zcount - 1; i >= 0; i--) {         /* windows, top first */
        int a = zorder[i];
        if (wins[a].minimised) continue;
        if (in_grip(a, mx, my) && !(my >= wins[a].y && my < wins[a].y + TITLE_H)) {
            raise(a);                               /* corner grip, checked first so its zone can */
            size_app = a;                           /* reach a little outside the window */
            size_dx = wins[a].x + win_w(a) - mx; size_dy = wins[a].y + win_h(a) - my;
            return;
        }
        if (!inside(mx, my, wins[a].x, wins[a].y, win_w(a), win_h(a))) continue;
        raise(a);                                   /* any click focuses + raises the window */
        for (int b = 0; b < 3; b++) {
            int bx, by;
            if (title_button_rect(a, b, &bx, &by) && inside(mx, my, bx, by, TBTN_W, TBTN_H)) {
                pressed_app = a; pressed_btn = b;   /* acts on release, like a real button */
                return;
            }
        }
        if (my < wins[a].y + TITLE_H) {
            if (wins[a].maximised) return;          /* maximised windows stay put */
            drag_app = a; drag_dx = mx - wins[a].x; drag_dy = my - wins[a].y; return;
        }
        int lx = mx - wins[a].x - BORDER, ly = my - wins[a].y - TITLE_H;
        if (lx >= 0 && ly >= 0 && lx < wins[a].w && ly < wins[a].h) {
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

static void right_down(void) {
    if (menu_open || my >= gfx_h - TASKBAR_H) return;
    for (int i = zcount - 1; i >= 0; i--) {
        int a = zorder[i];
        if (wins[a].minimised || !inside(mx, my, wins[a].x, wins[a].y, win_w(a), win_h(a))) continue;
        raise(a);
        int lx = mx - wins[a].x - BORDER, ly = my - wins[a].y - TITLE_H;
        if (apps[a].rclick && lx >= 0 && ly >= 0 && lx < wins[a].w && ly < wins[a].h) apps[a].rclick(lx, ly);
        return;
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
    } else if (size_app >= 0) {
        int a = size_app;
        int nw = mx + size_dx - wins[a].x - 2 * BORDER, nh = my + size_dy - wins[a].y - TITLE_H - BORDER;
        int mw = apps[a].min_w ? apps[a].min_w : 160, mh = apps[a].min_h ? apps[a].min_h : 80;
        if (nw > max_cw(a)) nw = max_cw(a);
        if (nh > max_ch(a)) nh = max_ch(a);
        if (nw > gfx_w - wins[a].x - 2 * BORDER) nw = gfx_w - wins[a].x - 2 * BORDER;
        if (nh > gfx_h - TASKBAR_H - wins[a].y - TITLE_H - BORDER) nh = gfx_h - TASKBAR_H - wins[a].y - TITLE_H - BORDER;
        if (nw < mw) nw = mw;
        if (nh < mh) nh = mh;
        wins[a].w = nw; wins[a].h = nh;
    } else if (capture_app >= 0 && apps[capture_app].drag) {
        int a = capture_app;
        apps[a].drag(mx - wins[a].x - BORDER, my - wins[a].y - TITLE_H);
    }
}

static void mouse_up(void) {
    if (pressed_app >= 0) {
        int a = pressed_app, b = pressed_btn, bx, by;
        pressed_app = pressed_btn = -1;
        if (wins[a].open && title_button_rect(a, b, &bx, &by) && inside(mx, my, bx, by, TBTN_W, TBTN_H)) {
            if (b == 0) gui_close_app(a);
            else if (b == 1) toggle_maximise(a);
            else minimise(a);
        }
    }
    if (capture_app >= 0 && apps[capture_app].release) apps[capture_app].release();
    drag_app = -1; capture_app = -1; size_app = -1;
}

static void key(int c) {
    if (menu_open && c == 27) { menu_open = false; return; }
    int a = top_app();                              /* keys always go to the focused window */
    if (a >= 0 && apps[a].key) apps[a].key(c);
}

static void handle_mouse_packet(int dx, int dy, int b) {
    mx += dx; my += dy;
    if (mx < 0) mx = 0;
    if (my < 0) my = 0;
    if (mx >= gfx_w) mx = gfx_w - 1;
    if (my >= gfx_h) my = gfx_h - 1;
    bool was = buttons & 1, now = b & 1;
    if (now && !was) mouse_down();
    else if (now && (dx || dy)) mouse_drag();
    else if (!now && was) mouse_up();
    if ((b & 2) && !(buttons & 2)) right_down();
    buttons = b;
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
        /* Keyboard and mouse events are handled in the order they arrived, so a
         * click on a window always takes effect before the keys typed after it. */
        for (;;) {
            uint32_t ks = keyboard_next_seq(), ms = mouse_next_seq();
            if (ks == 0xFFFFFFFFu && ms == 0xFFFFFFFFu) break;
            if (ms < ks) {
                int dx, dy, b;
                if (mouse_poll(&dx, &dy, &b)) handle_mouse_packet(dx, dy, b);
            } else {
                int c = keyboard_poll();
                if (c >= 0) key(c);
            }
            redraw = true;
        }
        int top = top_app();
        for (int a = 0; a < NAPPS; a++)
            if (wins[a].open && !wins[a].minimised && apps[a].tick && apps[a].tick(a == top)) redraw = true;
        if (vga_take_dirty()) redraw = true;
        uint32_t half = timer_ticks() / (timer_hz() / 2);   /* clock + caret blink */
        if (half != last_half) { last_half = half; redraw = true; }
        if (redraw) compose();
        else cpu_hlt();
    }
}
