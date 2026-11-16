/* LeoOS - desktop apps: Terminal, Calculator, Paint, Notepad and About */
#include "gui.h"
#include "gfx.h"
#include "cpu.h"
#include "vga.h"
#include "shell.h"
#include "string.h"
#include "version.h"

static const uint32_t vga_palette[16] = {
    0x000000, 0x0000AA, 0x00AA00, 0x00AAAA, 0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
    0x555555, 0x5555FF, 0x55FF55, 0x55FFFF, 0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF,
};
#define TEXT_DARK RGB(0x10, 0x30, 0x18)

static bool blink_on(void) { return (timer_ticks() / (timer_hz() / 2)) % 2 == 0; }

static void center_text(int x, int w, int y, const char *s, uint32_t c, int scale) {
    gfx_text(x + (w - (int)strlen(s) * FONT_W * scale) / 2, y, s, c, TRANSPARENT, scale);
}

/* ================================================================ Terminal */
static uint16_t term_cells[VGA_WIDTH * VGA_HEIGHT];

static void term_draw(int x, int y, int w, int h, bool focused) {
    (void)w; (void)h;
    for (int r = 0; r < VGA_HEIGHT; r++)
        for (int c = 0; c < VGA_WIDTH; c++) {
            uint16_t cell = term_cells[r * VGA_WIDTH + c];
            uint8_t ch = cell & 0xFF, attr = cell >> 8;
            uint32_t bg = vga_palette[(attr >> 4) & 0xF];
            if (bg != 0) gfx_fill(x + 4 + c * FONT_W, y + 4 + r * FONT_H, FONT_W, FONT_H, bg);
            if (ch != ' ' && ch) gfx_char(x + 4 + c * FONT_W, y + 4 + r * FONT_H, ch, vga_palette[attr & 0xF], TRANSPARENT, 1);
        }
    int cr, cc; vga_get_cursor(&cr, &cc);
    if (focused && blink_on()) gfx_fill(x + 4 + cc * FONT_W, y + 4 + cr * FONT_H + 13, FONT_W, 2, 0xC0C0C0);
}

static void term_key(int c) { shell_key(c); }

static void term_icon(int x, int y) {
    gfx_fill(x, y + 2, 32, 26, RGB(0x30, 0x30, 0x30));
    gfx_fill(x + 2, y + 4, 28, 22, 0x000000);
    gfx_text(x + 4, y + 7, ">_", RGB(0x55, 0xFF, 0x55), TRANSPARENT, 1);
    gfx_fill(x + 10, y + 28, 12, 2, RGB(0x30, 0x30, 0x30));
}

/* ============================================================== Calculator */
#define CALC_BW 50
#define CALC_BH 42
static const char *calc_labels[20] = {
    "C", "<-", "%", "/",
    "7", "8", "9", "*",
    "4", "5", "6", "-",
    "1", "2", "3", "+",
    "+/-", "0", "00", "=",
};
static int64_t calc_acc, calc_cur, calc_disp;
static char calc_op;
static bool calc_entering, calc_error;
static int calc_pressed = -1;

static void i64_to_str(int64_t v, char *buf) {
    char tmp[24]; int i = 0; bool neg = v < 0;
    uint64_t u = neg ? (uint64_t)(-v) : (uint64_t)v;
    if (u == 0) tmp[i++] = '0';
    while (u) { tmp[i++] = (char)('0' + u % 10); u /= 10; }
    int j = 0;
    if (neg) buf[j++] = '-';
    while (i) buf[j++] = tmp[--i];
    buf[j] = 0;
}

#define CALC_LIMIT 999999999999LL
static int64_t calc_apply(int64_t a, char op, int64_t b) {
    int64_t r = b;
    switch (op) {
    case '+': r = a + b; break;
    case '-': r = a - b; break;
    case '*': r = a * b; break;
    case '/': if (b == 0) { calc_error = true; return 0; } r = a / b; break;
    }
    if (r > CALC_LIMIT || r < -CALC_LIMIT) { calc_error = true; return 0; }
    return r;
}

static void calc_input(const char *k) {
    if (calc_error && strcmp(k, "C")) return;
    if (k[0] >= '0' && k[0] <= '9') {
        for (; *k; k++) {
            if (!calc_entering) { calc_cur = 0; calc_entering = true; }
            if (calc_cur < 99999999999LL && calc_cur > -99999999999LL)
                calc_cur = calc_cur * 10 + (calc_cur < 0 ? -(*k - '0') : (*k - '0'));
        }
        calc_disp = calc_cur;
    } else if (!strcmp(k, "C")) {
        calc_acc = calc_cur = calc_disp = 0; calc_op = 0; calc_entering = false; calc_error = false;
    } else if (!strcmp(k, "<-")) {
        if (calc_entering) { calc_cur /= 10; calc_disp = calc_cur; }
    } else if (!strcmp(k, "+/-")) {
        if (calc_entering) calc_disp = calc_cur = -calc_cur;
        else { calc_acc = -calc_disp; calc_disp = calc_acc; calc_cur = calc_acc; }
    } else if (!strcmp(k, "%")) {
        calc_cur = calc_op ? calc_acc * calc_disp / 100 : calc_disp / 100;
        calc_disp = calc_cur; calc_entering = true;
    } else if (!strcmp(k, "=")) {
        if (calc_op) {
            calc_acc = calc_apply(calc_acc, calc_op, calc_entering ? calc_cur : calc_disp);
            calc_disp = calc_acc; calc_op = 0;
        }
        calc_cur = calc_disp; calc_entering = false;
    } else {                                     /* + - * / */
        if (calc_op && calc_entering) calc_acc = calc_apply(calc_acc, calc_op, calc_cur);
        else if (!calc_op) calc_acc = calc_disp;
        calc_disp = calc_acc; calc_op = k[0]; calc_entering = false;
    }
}

static void calc_button_rect(int i, int *x, int *y) { *x = 10 + (i % 4) * (CALC_BW + 6); *y = 76 + (i / 4) * (CALC_BH + 6); }

static void calc_draw(int x, int y, int w, int h, bool focused) {
    (void)h; (void)focused;
    gfx_bevel(x + 10, y + 10, w - 20, 56, RGB(0xDF, 0xF3, 0xE3), true);
    char buf[32];
    if (calc_op) {
        i64_to_str(calc_acc, buf);
        int n = (int)strlen(buf); buf[n] = ' '; buf[n + 1] = calc_op; buf[n + 2] = 0;
        gfx_text(x + w - 18 - (int)strlen(buf) * FONT_W, y + 13, buf, RGB(0x50, 0x70, 0x58), TRANSPARENT, 1);
    }
    if (calc_error) strcpy(buf, "Error");
    else i64_to_str(calc_disp, buf);
    gfx_text(x + w - 18 - (int)strlen(buf) * 16, y + 30, buf, TEXT_DARK, TRANSPARENT, 2);
    for (int i = 0; i < 20; i++) {
        int bx, by; calc_button_rect(i, &bx, &by);
        const char *l = calc_labels[i];
        uint32_t face = 0xF2F2F2, fg = TEXT_DARK;
        if (i % 4 == 3 || i == 2) { face = RGB(0x3B, 0xB0, 0x5A); fg = 0xFFFFFF; }
        if (i == 19) { face = RGB(0x1C, 0x75, 0x36); fg = 0xFFFFFF; }
        if (i == 0) { face = RGB(0xE0, 0x6A, 0x5A); fg = 0xFFFFFF; }
        gfx_bevel(x + bx, y + by, CALC_BW, CALC_BH, face, i == calc_pressed);
        center_text(x + bx, CALC_BW, y + by + 13, l, fg, 1);
    }
}

static void calc_click(int lx, int ly) {
    for (int i = 0; i < 20; i++) {
        int bx, by; calc_button_rect(i, &bx, &by);
        if (lx >= bx && ly >= by && lx < bx + CALC_BW && ly < by + CALC_BH) { calc_pressed = i; calc_input(calc_labels[i]); return; }
    }
}
static void calc_release(void) { calc_pressed = -1; }

static void calc_key(int c) {
    char k[2] = { (char)c, 0 };
    if (c >= '0' && c <= '9') calc_input(k);
    else if (c == '+' || c == '-' || c == '*' || c == '/' || c == '%') calc_input(k);
    else if (c == '=' || c == '\n') calc_input("=");
    else if (c == '\b') calc_input("<-");
    else if (c == 27 || c == 'c' || c == 'C') calc_input("C");
}

static void calc_icon(int x, int y) {
    gfx_bevel(x + 4, y, 24, 32, RGB(0x5A, 0x6A, 0x60), false);
    gfx_fill(x + 7, y + 3, 18, 7, RGB(0xBF, 0xF0, 0xC8));
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++)
            gfx_fill(x + 7 + c * 6, y + 13 + r * 6, 5, 4, c == 2 ? RGB(0x3B, 0xD0, 0x5A) : 0xF0F0F0);
}

/* =================================================================== Paint */
#define PAINT_W 520
#define PAINT_H 340
#define PAINT_TOOLBAR 36
static uint32_t canvas[PAINT_W * PAINT_H];
static const uint32_t paint_colours[12] = {
    0x000000, 0xFFFFFF, 0x808080, 0xD9453B, 0xF08A24, 0xF5D02A,
    0x2FA84F, 0x0B6B26, 0x2E86DE, 0x1B3B8C, 0x9B59B6, 0x8B5A2B,
};
static uint32_t paint_colour = 0x2FA84F;
static int paint_size = 4, paint_lx = -1, paint_ly = -1;
static const int paint_sizes[3] = { 2, 5, 10 };

static void canvas_dot(int cx, int cy, int r) {
    for (int dy = -r; dy <= r; dy++)
        for (int dx = -r; dx <= r; dx++) {
            int x = cx + dx, y = cy + dy;
            if (dx * dx + dy * dy <= r * r + r && x >= 0 && y >= 0 && x < PAINT_W && y < PAINT_H)
                canvas[y * PAINT_W + x] = paint_colour;
        }
}

static void canvas_line(int x0, int y0, int x1, int y1) {
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        canvas_dot(x0, y0, paint_size / 2);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static void paint_clear(void) { for (int i = 0; i < PAINT_W * PAINT_H; i++) canvas[i] = 0xFFFFFF; }

static void paint_draw(int x, int y, int w, int h, bool focused) {
    (void)h; (void)focused;
    gfx_fill(x, y, w, PAINT_TOOLBAR, RGB(0xE4, 0xF2, 0xE7));
    for (int i = 0; i < 12; i++) {
        int sx = x + 8 + i * 24;
        gfx_fill(sx, y + 8, 20, 20, paint_colours[i]);
        gfx_rect(sx, y + 8, 20, 20, paint_colours[i] == paint_colour ? 0x000000 : RGB(0x90, 0xA0, 0x94));
        if (paint_colours[i] == paint_colour) gfx_rect(sx - 2, y + 6, 24, 24, RGB(0x0E, 0x4D, 0x22));
    }
    for (int i = 0; i < 3; i++) {
        int bx = x + 310 + i * 30;
        gfx_bevel(bx, y + 7, 26, 22, 0xF2F2F2, paint_sizes[i] == paint_size);
        gfx_disc(bx + 13, y + 18, paint_sizes[i] / 2 + 1, 0x000000);
    }
    gfx_bevel(x + 410, y + 7, 64, 22, RGB(0xE0, 0x6A, 0x5A), false);
    gfx_text(x + 422, y + 10, "Clear", 0xFFFFFF, TRANSPARENT, 1);
    gfx_blit(x, y + PAINT_TOOLBAR, PAINT_W, PAINT_H, canvas, PAINT_W);
}

static void paint_click(int lx, int ly) {
    paint_lx = -1;
    if (ly < PAINT_TOOLBAR) {
        for (int i = 0; i < 12; i++)
            if (lx >= 8 + i * 24 && lx < 28 + i * 24 && ly >= 8 && ly < 28) paint_colour = paint_colours[i];
        for (int i = 0; i < 3; i++)
            if (lx >= 310 + i * 30 && lx < 336 + i * 30) paint_size = paint_sizes[i];
        if (lx >= 410 && lx < 474) paint_clear();
        return;
    }
    paint_lx = lx; paint_ly = ly - PAINT_TOOLBAR;
    canvas_dot(paint_lx, paint_ly, paint_size / 2);
}

static void paint_drag(int lx, int ly) {
    if (paint_lx < 0) return;
    ly -= PAINT_TOOLBAR;
    canvas_line(paint_lx, paint_ly, lx, ly);
    paint_lx = lx; paint_ly = ly;
}
static void paint_release(void) { paint_lx = -1; }

static void paint_icon(int x, int y) {
    gfx_fill(x + 2, y + 2, 28, 28, 0xFFFFFF);
    gfx_rect(x + 2, y + 2, 28, 28, RGB(0x60, 0x60, 0x60));
    gfx_disc(x + 10, y + 10, 4, 0xD9453B);
    gfx_disc(x + 21, y + 11, 4, 0x2E86DE);
    gfx_disc(x + 12, y + 21, 4, 0xF5D02A);
    gfx_line(x + 16, y + 26, x + 28, y + 14, 3, 0x2FA84F);
}

/* ================================================================= Notepad */
#define NOTE_MAX 4000
static char note[NOTE_MAX + 1];
static int note_len;

static void note_draw(int x, int y, int w, int h, bool focused) {
    int cols = (w - 16) / FONT_W, rows = (h - 12) / FONT_H;
    /* first pass: count wrapped lines so we can keep the end in view */
    int line = 0, col = 0;
    for (int i = 0; i < note_len; i++) {
        if (note[i] == '\n' || col == cols) { line++; col = 0; }
        if (note[i] != '\n') col++;
    }
    int first = line >= rows ? line - rows + 1 : 0;
    for (int r = 0; r < rows; r++) gfx_fill(x + 6, y + 6 + r * FONT_H + FONT_H - 1, w - 12, 1, RGB(0xE6, 0xF0, 0xE8));
    line = 0; col = 0;
    for (int i = 0; i < note_len; i++) {
        if (note[i] == '\n' || col == cols) { line++; col = 0; }
        if (note[i] == '\n') continue;
        if (line >= first) gfx_char(x + 8 + col * FONT_W, y + 6 + (line - first) * FONT_H, (uint8_t)note[i], TEXT_DARK, TRANSPARENT, 1);
        col++;
    }
    if (col == cols) { line++; col = 0; }
    if (focused && blink_on()) gfx_fill(x + 8 + col * FONT_W, y + 6 + (line - first) * FONT_H, 2, FONT_H - 1, TEXT_DARK);
}

static void note_key(int c) {
    if (c == '\b') { if (note_len) note_len--; return; }
    if (c == '\t') { for (int i = 0; i < 4; i++) note_key(' '); return; }
    if (c == '\n' || (c >= 32 && c < 256 && c != 127)) {
        if (note_len < NOTE_MAX) note[note_len++] = (char)c;
    }
}

static void note_icon(int x, int y) {
    gfx_fill(x + 5, y + 1, 22, 30, 0xFFFFFF);
    gfx_rect(x + 5, y + 1, 22, 30, RGB(0x70, 0x70, 0x70));
    gfx_fill(x + 6, y + 2, 20, 5, RGB(0xF5, 0xD0, 0x2A));
    for (int i = 0; i < 5; i++) gfx_fill(x + 8, y + 11 + i * 4, i == 4 ? 9 : 16, 1, RGB(0x50, 0x80, 0x60));
}

/* =================================================================== About */
static void about_draw(int x, int y, int w, int h, bool focused) {
    (void)focused;
    char buf[64], num[16];
    gfx_gradient(x, y, w, 96, RGB(0xE8, 0xF8, 0xEC), RGB(0xC4, 0xEA, 0xCE));
    center_text(x + 3, w, y + 13, "LeoOS", RGB(0x0B, 0x47, 0x1E), 4);
    center_text(x, w, y + 10, "LeoOS", RGB(0x2F, 0xA8, 0x4F), 4);
    center_text(x, w, y + 106, "Version " LEOOS_VERSION, TEXT_DARK, 2);
    center_text(x, w, y + 144, "A tiny 32-bit hobby operating system", TEXT_DARK, 1);
    center_text(x, w, y + 162, "built from scratch for Leo.", TEXT_DARK, 1);
    strcpy(buf, "Screen: "); utoa((uint32_t)gfx_w, num, 10); strcpy(buf + strlen(buf), num);
    strcpy(buf + strlen(buf), " x "); utoa((uint32_t)gfx_h, num, 10); strcpy(buf + strlen(buf), num);
    strcpy(buf + strlen(buf), ", 32-bit colour");
    center_text(x, w, y + 190, buf, RGB(0x40, 0x60, 0x48), 1);
    if (gui_mbi->flags & 1) {
        strcpy(buf, "Memory: "); utoa((gui_mbi->mem_upper + 1024) / 1024, num, 10);
        strcpy(buf + strlen(buf), num); strcpy(buf + strlen(buf), " MiB");
        center_text(x, w, y + 208, buf, RGB(0x40, 0x60, 0x48), 1);
    }
    gfx_bevel(x + w - 96, y + h - 40, 84, 28, RGB(0x2F, 0xA8, 0x4F), false);
    center_text(x + w - 96, 84, y + h - 34, "OK", 0xFFFFFF, 1);
}

static void about_click(int lx, int ly) {
    if (lx >= apps[APP_ABOUT].w - 96 && lx < apps[APP_ABOUT].w - 12 && ly >= apps[APP_ABOUT].h - 40 && ly < apps[APP_ABOUT].h - 12)
        gui_close_app(APP_ABOUT);
}
static void about_key(int c) { if (c == '\n' || c == 27) gui_close_app(APP_ABOUT); }

static void about_icon(int x, int y) {
    gfx_disc(x + 16, y + 16, 15, RGB(0x2F, 0xA8, 0x4F));
    gfx_disc(x + 16, y + 16, 12, RGB(0x3D, 0xC8, 0x62));
    gfx_text(x + 12, y + 7, "i", 0xFFFFFF, TRANSPARENT, 1);
    gfx_text(x + 13, y + 7, "i", 0xFFFFFF, TRANSPARENT, 1);
}

/* ================================================================ registry */
const struct app apps[NAPPS] = {
    [APP_TERMINAL] = { "Terminal", VGA_WIDTH * FONT_W + 8, VGA_HEIGHT * FONT_H + 8, 0x000000,
                       term_draw, NULL, NULL, NULL, term_key, term_icon },
    [APP_CALC]     = { "Calculator", 4 * CALC_BW + 3 * 6 + 20, 76 + 5 * (CALC_BH + 6) + 4, RGB(0xD4, 0xE6, 0xD8),
                       calc_draw, calc_click, NULL, calc_release, calc_key, calc_icon },
    [APP_PAINT]    = { "Paint", PAINT_W, PAINT_H + PAINT_TOOLBAR, 0xFFFFFF,
                       paint_draw, paint_click, paint_drag, paint_release, NULL, paint_icon },
    [APP_NOTEPAD]  = { "Notepad", 480, 300, 0xFFFFFF,
                       note_draw, NULL, NULL, NULL, note_key, note_icon },
    [APP_ABOUT]    = { "About LeoOS", 400, 280, 0xF7FBF8,
                       about_draw, about_click, NULL, NULL, about_key, about_icon },
};

void apps_init(void) {
    paint_clear();
    strcpy(note, "Dear Leo,\n\nThis is Notepad on LeoOS " LEOOS_VERSION ". Type anything you like!\n\n");
    note_len = (int)strlen(note);
    vga_use_buffer(term_cells);
    vga_set_colour(LIGHT_GREEN, BLACK);
    vga_puts("LeoOS " LEOOS_VERSION " terminal");
    vga_set_colour(LIGHT_GREY, BLACK);
    vga_puts(" - the classic shell, now in a window. Type 'help'.\n\n");
}
