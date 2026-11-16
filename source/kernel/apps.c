/* LeoOS - desktop apps: Terminal, Calculator, Paint, Notepad and About
 * (Snake and Minesweeper live in snake.c and minesweeper.c) */
#include "gui.h"
#include "gfx.h"
#include "cpu.h"
#include "vga.h"
#include "shell.h"
#include "string.h"
#include "version.h"
#include "ramfs.h"
#include "snake.h"
#include "minesweeper.h"

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
    /* The console is always 80x25. A smaller window shows the part around the cursor. */
    int rows = (h - 8) / FONT_H, cols = (w - 8) / FONT_W;
    if (rows > VGA_HEIGHT) rows = VGA_HEIGHT;
    if (cols > VGA_WIDTH) cols = VGA_WIDTH;
    if (rows < 1) rows = 1;
    if (cols < 1) cols = 1;
    int cr, cc; vga_get_cursor(&cr, &cc);
    int r0 = cr >= rows ? cr - rows + 1 : 0, c0 = cc >= cols ? cc - cols + 1 : 0;
    for (int r = 0; r < rows; r++)
        for (int c = 0; c < cols; c++) {
            uint16_t cell = term_cells[(r0 + r) * VGA_WIDTH + c0 + c];
            uint8_t ch = cell & 0xFF, attr = cell >> 8;
            uint32_t bg = vga_palette[(attr >> 4) & 0xF];
            if (bg != 0) gfx_fill(x + 4 + c * FONT_W, y + 4 + r * FONT_H, FONT_W, FONT_H, bg);
            if (ch != ' ' && ch) gfx_char(x + 4 + c * FONT_W, y + 4 + r * FONT_H, ch, vga_palette[attr & 0xF], TRANSPARENT, 1);
        }
    if (focused && blink_on()) gfx_fill(x + 4 + (cc - c0) * FONT_W, y + 4 + (cr - r0) * FONT_H + 13, FONT_W, 2, 0xC0C0C0);
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
#define PAINT_W 520             /* default canvas size */
#define PAINT_H 340
#define PAINT_MAXW 1024         /* the canvas buffer is big enough for a maximised window */
#define PAINT_MAXH 720
#define PAINT_TOOLBAR 36
static uint32_t canvas[PAINT_MAXW * PAINT_MAXH];
static const uint32_t paint_colours[12] = {
    0x000000, 0xFFFFFF, 0x808080, 0xD9453B, 0xF08A24, 0xF5D02A,
    0x2FA84F, 0x0B6B26, 0x2E86DE, 0x1B3B8C, 0x9B59B6, 0x8B5A2B,
};
static uint32_t paint_colour = 0x2FA84F;
static int paint_size = 4, paint_lx = -1, paint_ly = -1;
static const int paint_sizes[3] = { 2, 5, 10 };
static int paint_vw = PAINT_W, paint_vh = PAINT_H;     /* visible part of the canvas */

static void canvas_dot(int cx, int cy, int r) {
    for (int dy = -r; dy <= r; dy++)
        for (int dx = -r; dx <= r; dx++) {
            int x = cx + dx, y = cy + dy;
            if (dx * dx + dy * dy <= r * r + r && x >= 0 && y >= 0 && x < paint_vw && y < paint_vh)
                canvas[y * PAINT_MAXW + x] = paint_colour;
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

static void paint_clear(void) { for (int i = 0; i < PAINT_MAXW * PAINT_MAXH; i++) canvas[i] = 0xFFFFFF; }

static void paint_draw(int x, int y, int w, int h, bool focused) {
    (void)focused;
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
    int cw = w < PAINT_MAXW ? w : PAINT_MAXW, ch = h - PAINT_TOOLBAR < PAINT_MAXH ? h - PAINT_TOOLBAR : PAINT_MAXH;
    paint_vw = cw; paint_vh = ch;
    gfx_blit(x, y + PAINT_TOOLBAR, cw, ch, canvas, PAINT_MAXW);
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
#define NOTE_MAX FS_DATA_MAX
#define NOTE_BAR 34                       /* toolbar: New, Open, Save + file name */
static char note[NOTE_MAX + 1];
static int note_len;
static char note_name[FS_NAME_MAX];       /* "" = untitled */
static bool note_dirty;
enum { NOTE_EDIT, NOTE_SAVE, NOTE_OPEN };
static int note_mode, note_pressed = -1;
static char note_input[FS_NAME_MAX];      /* file name being typed in the Save box */
static bool note_input_fresh;             /* suggested name is "selected": typing replaces it */
static const char *note_msg;              /* short status message, e.g. "Saved!" */
static uint32_t note_msg_until;
static int note_open_files[FS_MAX_FILES], note_open_count;

static const char *note_btn_label[3] = { "New", "Open", "Save" };
static void note_btn_rect(int i, int *x, int *y, int *w, int *h) { *x = 6 + i * 62; *y = 5; *w = 56; *h = 24; }

static void note_status(const char *m) { note_msg = m; note_msg_until = timer_ticks() + 3 * timer_hz(); }

/* dialog box inside the client area */
static void note_dialog_rect(int w, int h, int *dx, int *dy, int *dw, int *dh, int rows) {
    *dw = w - 40 < 340 ? w - 40 : 340;
    *dh = rows ? 70 + rows * 30 : 120;
    if (*dh > h - NOTE_BAR - 10) *dh = h - NOTE_BAR - 10;
    *dx = (w - *dw) / 2; *dy = NOTE_BAR + (h - NOTE_BAR - *dh) / 2;
}
/* OK / Cancel buttons at the bottom of the dialog */
static void note_dlg_btn(int i, int dx, int dy, int dw, int dh, int *bx, int *by) { *bx = dx + dw - (2 - i) * 88; *by = dy + dh - 36; }

static void note_draw_text(int x, int y, int w, int h, bool focused) {
    int cols = (w - 16) / FONT_W, rows = (h - 12) / FONT_H;
    if (cols < 1) cols = 1;
    if (rows < 1) rows = 1;
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
    if (focused && note_mode == NOTE_EDIT && blink_on())
        gfx_fill(x + 8 + col * FONT_W, y + 6 + (line - first) * FONT_H, 2, FONT_H - 1, TEXT_DARK);
}

static void note_draw(int x, int y, int w, int h, bool focused) {
    /* toolbar */
    gfx_fill(x, y, w, NOTE_BAR, RGB(0xE4, 0xF2, 0xE7));
    gfx_fill(x, y + NOTE_BAR - 1, w, 1, RGB(0xA8, 0xC8, 0xB0));
    for (int i = 0; i < 3; i++) {
        int bx, by, bw, bh; note_btn_rect(i, &bx, &by, &bw, &bh);
        gfx_bevel(x + bx, y + by, bw, bh, i == 2 ? RGB(0x3B, 0xB0, 0x5A) : 0xF2F2F2, note_pressed == i);
        center_text(x + bx, bw, y + by + 4, note_btn_label[i], i == 2 ? 0xFFFFFF : TEXT_DARK, 1);
    }
    char title[FS_NAME_MAX + 4];
    strcpy(title, note_name[0] ? note_name : "untitled");
    if (note_dirty) strcpy(title + strlen(title), " *");
    gfx_text(x + 196, y + 9, title, TEXT_DARK, TRANSPARENT, 1);
    if (note_msg && timer_ticks() < note_msg_until) {
        int tw = (int)strlen(note_msg) * FONT_W;
        gfx_text(x + w - tw - 10, y + 9, note_msg, RGB(0x1C, 0x75, 0x36), TRANSPARENT, 1);
    }
    gfx_set_clip(x, y + NOTE_BAR, w, h - NOTE_BAR);
    note_draw_text(x, y + NOTE_BAR, w, h - NOTE_BAR, focused);
    if (note_mode == NOTE_EDIT) return;

    /* dialogs */
    gfx_shade(x, y + NOTE_BAR, w, h - NOTE_BAR, 60);
    int dx, dy, dw, dh;
    note_dialog_rect(w, h, &dx, &dy, &dw, &dh, note_mode == NOTE_OPEN ? (note_open_count ? note_open_count : 1) : 0);
    gfx_set_clip(x + dx, y + dy, dw, dh);
    gfx_shade(x + dx + 4, y + dy + 4, dw, dh, 80);
    gfx_fill(x + dx, y + dy, dw, dh, RGB(0xF4, 0xFA, 0xF5));
    gfx_rect(x + dx, y + dy, dw, dh, RGB(0x0E, 0x4D, 0x22));
    gfx_gradient(x + dx + 1, y + dy + 1, dw - 2, 24, RGB(0x2F, 0xA8, 0x4F), RGB(0x17, 0x6E, 0x32));
    gfx_text(x + dx + 10, y + dy + 5, note_mode == NOTE_SAVE ? "Save as" : "Open a file", 0xFFFFFF, TRANSPARENT, 1);
    if (note_mode == NOTE_SAVE) {
        gfx_text(x + dx + 12, y + dy + 34, "File name:", TEXT_DARK, TRANSPARENT, 1);
        gfx_fill(x + dx + 100, y + dy + 30, dw - 112, 24, 0xFFFFFF);
        gfx_rect(x + dx + 100, y + dy + 30, dw - 112, 24, RGB(0x6C, 0x9C, 0x78));
        if (note_input_fresh) {
            gfx_fill(x + dx + 105, y + dy + 33, (int)strlen(note_input) * FONT_W + 2, FONT_H + 2, RGB(0x2F, 0xA8, 0x4F));
            gfx_text(x + dx + 106, y + dy + 34, note_input, 0xFFFFFF, TRANSPARENT, 1);
        } else gfx_text(x + dx + 106, y + dy + 34, note_input, TEXT_DARK, TRANSPARENT, 1);
        if (blink_on()) gfx_fill(x + dx + 106 + (int)strlen(note_input) * FONT_W, y + dy + 34, 2, FONT_H, TEXT_DARK);
        gfx_text(x + dx + 12, y + dy + 62, "Kept in RAM - lost on restart", RGB(0x60, 0x80, 0x68), TRANSPARENT, 1);
    } else if (!note_open_count) {
        gfx_text(x + dx + 12, y + dy + 36, "No saved files yet.", TEXT_DARK, TRANSPARENT, 1);
    } else {
        for (int i = 0; i < note_open_count; i++) {
            int f = note_open_files[i], ry = dy + 32 + i * 30;
            gfx_bevel(x + dx + 10, y + ry, dw - 20, 26, RGB(0xE4, 0xF2, 0xE7), false);
            gfx_text(x + dx + 18, y + ry + 5, fs_name(f), TEXT_DARK, TRANSPARENT, 1);
            char num[16]; utoa((uint32_t)fs_size(f), num, 10); strcpy(num + strlen(num), " B");
            gfx_text(x + dx + dw - 20 - (int)strlen(num) * FONT_W - 8, y + ry + 5, num, RGB(0x60, 0x80, 0x68), TRANSPARENT, 1);
        }
    }
    for (int i = note_mode == NOTE_SAVE ? 0 : 1; i < 2; i++) {
        int bx, by; note_dlg_btn(i, dx, dy, dw, dh, &bx, &by);
        gfx_bevel(x + bx, y + by, 80, 28, i == 0 ? RGB(0x3B, 0xB0, 0x5A) : 0xF2F2F2, note_pressed == 10 + i);
        center_text(x + bx, 80, y + by + 6, i == 0 ? "Save" : "Cancel", i == 0 ? 0xFFFFFF : TEXT_DARK, 1);
    }
}

static void note_do_save(void) {
    if (!fs_valid_name(note_input)) { note_status("Bad name: use a-z 0-9 . _ -"); return; }
    int r = fs_write(note_input, note, note_len);
    if (r == -1) { note_status("Disk full - rm a file"); return; }
    strcpy(note_name, note_input);
    note_dirty = false; note_mode = NOTE_EDIT;
    note_status("Saved!");
}

static void note_open_list(void) {
    note_open_count = 0;
    for (int i = 0; i < fs_count(); i++) if (fs_used(i)) note_open_files[note_open_count++] = i;
    note_mode = NOTE_OPEN;
}

static void note_click(int lx, int ly) {
    int w, h; gui_client_size(APP_NOTEPAD, &w, &h);
    if (note_mode == NOTE_EDIT) {
        for (int i = 0; i < 3; i++) {
            int bx, by, bw, bh; note_btn_rect(i, &bx, &by, &bw, &bh);
            if (lx < bx || ly < by || lx >= bx + bw || ly >= by + bh) continue;
            note_pressed = i;
            if (i == 0) { note_len = 0; note_name[0] = 0; note_dirty = false; note_status("New note"); }
            else if (i == 1) note_open_list();
            else { strcpy(note_input, note_name[0] ? note_name : "notes.txt"); note_input_fresh = true; note_mode = NOTE_SAVE; }
        }
        return;
    }
    int dx, dy, dw, dh;
    note_dialog_rect(w, h, &dx, &dy, &dw, &dh, note_mode == NOTE_OPEN ? (note_open_count ? note_open_count : 1) : 0);
    for (int i = 0; i < 2; i++) {
        int bx, by; note_dlg_btn(i, dx, dy, dw, dh, &bx, &by);
        if (lx >= bx && ly >= by && lx < bx + 80 && ly < by + 28) {
            if (i == 0 && note_mode != NOTE_SAVE) continue;
            note_pressed = 10 + i;
            if (i == 0) note_do_save(); else note_mode = NOTE_EDIT;
            return;
        }
    }
    if (note_mode == NOTE_OPEN) {
        for (int i = 0; i < note_open_count; i++) {
            int ry = dy + 32 + i * 30;
            if (lx >= dx + 10 && lx < dx + dw - 10 && ly >= ry && ly < ry + 26) {
                int f = note_open_files[i];
                strcpy(note_name, fs_name(f));
                note_len = fs_read(note_name, note, NOTE_MAX);
                if (note_len < 0) note_len = 0;
                note_dirty = false; note_mode = NOTE_EDIT;
                note_status("Opened");
                return;
            }
        }
    }
}
static void note_release(void) { note_pressed = -1; }

static void note_key(int c) {
    if (note_mode == NOTE_SAVE) {
        if (note_input_fresh && c != '\n' && c != 27 && c < 0x100) note_input[0] = 0;   /* typing replaces the suggestion */
        if (c != '\n') note_input_fresh = false;
        int n = (int)strlen(note_input);
        if (c == '\n') note_do_save();
        else if (c == 27) note_mode = NOTE_EDIT;
        else if (c == '\b') { if (n) note_input[n - 1] = 0; }
        else if (n < FS_NAME_MAX - 1 && c < 0x100 &&
                 ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-')) {
            note_input[n] = (char)c; note_input[n + 1] = 0;
        }
        return;
    }
    if (note_mode == NOTE_OPEN) { if (c == 27) note_mode = NOTE_EDIT; return; }
    if (c == '\b') { if (note_len) { note_len--; note_dirty = true; } return; }
    if (c == '\t') { for (int i = 0; i < 4; i++) note_key(' '); return; }
    if (c == '\n' || (c >= 32 && c < 256 && c != 127)) {
        if (note_len < NOTE_MAX) { note[note_len++] = (char)c; note_dirty = true; }
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
    center_text(x, w, y + 232, "New in 0.3: Snake, Minesweeper, saving files,", RGB(0x1C, 0x75, 0x36), 1);
    center_text(x, w, y + 250, "minimise and resize. Files are kept in RAM", RGB(0x1C, 0x75, 0x36), 1);
    center_text(x, w, y + 268, "only, so they are lost when you restart.", RGB(0x1C, 0x75, 0x36), 1);
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
                       term_draw, NULL, NULL, NULL, term_key, term_icon,
                       .resizable = true, .min_w = 240, .min_h = 120,
                       .max_w = VGA_WIDTH * FONT_W + 8, .max_h = VGA_HEIGHT * FONT_H + 8 },
    [APP_CALC]     = { "Calculator", 4 * CALC_BW + 3 * 6 + 20, 76 + 5 * (CALC_BH + 6) + 4, RGB(0xD4, 0xE6, 0xD8),
                       calc_draw, calc_click, NULL, calc_release, calc_key, calc_icon },
    [APP_PAINT]    = { "Paint", PAINT_W, PAINT_H + PAINT_TOOLBAR, 0xFFFFFF,
                       paint_draw, paint_click, paint_drag, paint_release, NULL, paint_icon,
                       .resizable = true, .min_w = 480, .min_h = PAINT_TOOLBAR + 120 },
    [APP_NOTEPAD]  = { "Notepad", 480, 300, 0xFFFFFF,
                       note_draw, note_click, NULL, note_release, note_key, note_icon,
                       .resizable = true, .min_w = 320, .min_h = 200 },
    [APP_SNAKE]    = { "Snake", SNAKE_W, SNAKE_H, SNAKE_BG,
                       snake_draw, snake_click, NULL, snake_release, snake_key, snake_icon,
                       .tick = snake_tick },
    [APP_MINES]    = { "Minesweeper", MINES_W, MINES_H, MINES_BG,
                       mines_draw, mines_click, NULL, mines_release, mines_key, mines_icon,
                       .rclick = mines_rclick },
    [APP_ABOUT]    = { "About LeoOS", 400, 330, 0xF7FBF8,
                       about_draw, about_click, NULL, NULL, about_key, about_icon },
};

void apps_init(void) {
    paint_clear();
    snake_init();
    mines_init();
    strcpy(note, "Dear Leo,\n\nThis is Notepad on LeoOS " LEOOS_VERSION ". Type anything you like,\nthen tap Save. Open the Terminal and type 'ls' to see it!\n\n");
    note_len = (int)strlen(note);
    vga_use_buffer(term_cells);
    vga_set_colour(LIGHT_GREEN, BLACK);
    vga_puts("LeoOS " LEOOS_VERSION " terminal");
    vga_set_colour(LIGHT_GREY, BLACK);
    vga_puts(" - the classic shell, now in a window. Type 'help'.\n\n");
}
