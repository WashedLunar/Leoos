/* LeoOS - Tetris (by Starlink) */
#include "tetris.h"
#include "gfx.h"
#include "string.h"

#define TEXT_DARK RGB(0x10, 0x30, 0x18)
/* Keyboard arrow values from the LeoOS keyboard driver (KEY_* in cpu.h).
 * Declared locally so this file only includes gfx.h, string.h and types.h. */
enum { TKEY_UP = 0x100, TKEY_DOWN = 0x101, TKEY_LEFT = 0x102, TKEY_RIGHT = 0x103 };
enum { ST_READY, ST_PLAYING, ST_PAUSED, ST_OVER };
enum { BTN_ROT, BTN_LEFT, BTN_DOWN, BTN_RIGHT, BTN_DROP, BTN_PAUSE, BTN_NEW, NBTNS };

/* 4x4 bitmasks, bit 15 = top-left. Four clockwise rotations per piece. */
static const uint16_t SHAPE[7][4] = {
    { 0x0F00, 0x2222, 0x00F0, 0x4444 }, /* I */
    { 0x0660, 0x0660, 0x0660, 0x0660 }, /* O */
    { 0x4E00, 0x4C40, 0x0E40, 0x4640 }, /* T */
    { 0x6C00, 0x4620, 0x6C00, 0x4620 }, /* S */
    { 0xC600, 0x2640, 0xC600, 0x2640 }, /* Z */
    { 0x8E00, 0x6440, 0x0E20, 0x44C0 }, /* J */
    { 0x2E00, 0x4460, 0x0E80, 0xC440 }, /* L */
};
static const uint32_t PCOL[7] = {
    RGB(0x2A, 0xC8, 0xD8), RGB(0xF5, 0xD0, 0x2A), RGB(0xB0, 0x5C, 0xE0),
    RGB(0x4C, 0xD0, 0x6C), RGB(0xE0, 0x3C, 0x31), RGB(0x3B, 0x6B, 0xE0),
    RGB(0xF0, 0x8A, 0x24),
};

static uint8_t grid[TETRIS_ROWS][TETRIS_COLS];
static int state, typ, rot, px, py, next, bag[7], bag_n;
static uint32_t score, best, lines, level, rng = 0xA5A5A5A5;
static uint32_t fall_acc, soft_acc;
static int pressed = -1;
static bool paused_by_focus, soft_drop;

static uint32_t rnd(void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }

static int bag_draw(void) {
    if (!bag_n) {
        for (int i = 0; i < 7; i++) bag[i] = i;
        for (int i = 6; i > 0; i--) {
            int j = (int)(rnd() % (uint32_t)(i + 1)), t = bag[i];
            bag[i] = bag[j]; bag[j] = t;
        }
        bag_n = 7;
    }
    return bag[--bag_n];
}

static bool cell_free(int r, int c) {
    if (c < 0 || c >= TETRIS_COLS || r >= TETRIS_ROWS) return false;
    if (r < 0) return true;
    return !grid[r][c];
}

static bool fits(int t, int r, int x, int y) {
    uint16_t s = SHAPE[t][r & 3];
    for (int i = 0; i < 16; i++) if (s & (uint16_t)(1u << (15 - i)))
        if (!cell_free(y + i / 4, x + i % 4)) return false;
    return true;
}

static void each_block(int t, int r, int x, int y, void (*fn)(int, int, int)) {
    uint16_t s = SHAPE[t][r & 3];
    for (int i = 0; i < 16; i++) if (s & (uint16_t)(1u << (15 - i)))
        fn(y + i / 4, x + i % 4, t);
}

static void paint_cell(int r, int c, int t) {
    if (r >= 0 && r < TETRIS_ROWS && c >= 0 && c < TETRIS_COLS)
        grid[r][c] = (uint8_t)(t + 1);
}

static int clear_lines(void) {
    int n = 0;
    for (int r = TETRIS_ROWS - 1; r >= 0; r--) {
        int full = 1;
        for (int c = 0; c < TETRIS_COLS; c++) if (!grid[r][c]) { full = 0; break; }
        if (!full) continue;
        n++;
        for (int rr = r; rr > 0; rr--)
            for (int c = 0; c < TETRIS_COLS; c++) grid[rr][c] = grid[rr - 1][c];
        for (int c = 0; c < TETRIS_COLS; c++) grid[0][c] = 0;
        r++;                                   /* re-check this row after the shift */
    }
    return n;
}

static void spawn(void) {
    typ = next; next = bag_draw(); rot = 0; px = 3; py = 0; fall_acc = 0; soft_acc = 0;
    if (!fits(typ, rot, px, py)) state = ST_OVER;
}

void tetris_init(void) {
    memset(grid, 0, sizeof grid);
    state = ST_READY; score = 0; lines = 0; level = 0;
    bag_n = 0; pressed = -1; soft_drop = false; paused_by_focus = false;
    rng ^= rng << 7; rng += 0x9E3779B9u; if (!rng) rng = 1;
    next = bag_draw(); typ = -1;
}

static void start_if_needed(void) {
    if (state == ST_READY) { state = ST_PLAYING; spawn(); }
}

static const uint32_t LINE_PTS[5] = { 0, 100, 300, 500, 800 };

static void lock_piece(void) {
    each_block(typ, rot, px, py, paint_cell);
    int n = clear_lines();
    if (n) {
        score += LINE_PTS[n] * (level + 1);
        lines += (uint32_t)n;
        level = lines / 10;
    }
    if (score > best) best = score;
    spawn();
}

static void move_x(int dx) {
    start_if_needed();
    if (state != ST_PLAYING) return;
    if (fits(typ, rot, px + dx, py)) px += dx;
}

static void rotate_cw(void) {
    start_if_needed();
    if (state != ST_PLAYING) return;
    int nr = (rot + 1) & 3;
    static const int kickx[] = { 0, -1, 1, 0, -2, 2, -1, 1 };
    static const int kicky[] = { 0,  0, 0,-1,  0, 0, -1,-1 };
    for (int i = 0; i < 8; i++)
        if (fits(typ, nr, px + kickx[i], py + kicky[i])) {
            rot = nr; px += kickx[i]; py += kicky[i]; return;
        }
}

static int ghost_y(void) {
    int y = py;
    while (fits(typ, rot, px, y + 1)) y++;
    return y;
}

static void soft_one(void) {
    start_if_needed();
    if (state != ST_PLAYING) return;
    if (fits(typ, rot, px, py + 1)) { py++; score += 1; if (score > best) best = score; }
    else lock_piece();
}

static void hard_drop(void) {
    start_if_needed();
    if (state != ST_PLAYING) return;
    int y = ghost_y();
    score += (uint32_t)(y - py) * 2;
    if (score > best) best = score;
    py = y;
    lock_piece();
}

/* Frames between automatic drops. The GUI calls tetris_tick about 100 times
 * a second (once per PIT tick when idle). Level 0 is ~0.48 s per row. */
static uint32_t fall_period(void) {
    int t = 48 - (int)level * 4;
    return (uint32_t)(t < 8 ? 8 : t);
}

bool tetris_tick(bool focused) {
    paused_by_focus = !focused && state == ST_PLAYING;
    if (state != ST_PLAYING) { fall_acc = 0; soft_acc = 0; return false; }
    if (!focused) { fall_acc = 0; soft_acc = 0; return false; }
    fall_acc++;
    if (soft_drop) {
        soft_acc++;
        if (soft_acc >= 3) { soft_acc = 0; soft_one(); return true; }
        return false;
    }
    if (fall_acc < fall_period()) return false;
    fall_acc = 0;
    if (fits(typ, rot, px, py + 1)) py++;
    else lock_piece();
    return true;
}

/* 0 rotate, 1 left, 2 down, 3 right, 4 drop, 5 pause, 6 new */
static void pad_rect(int i, int *x, int *y, int *w, int *h) {
    int p = TETRIS_PANEL, s = TETRIS_PADW;
    *w = s - 4; *h = s - 4;
    switch (i) {
    case BTN_ROT:   *x = p;       *y = 180; *w = 3 * s - 4; *h = 40; break;
    case BTN_LEFT:  *x = p;       *y = 228; break;
    case BTN_DOWN:  *x = p + s;   *y = 228; break;
    case BTN_RIGHT: *x = p + 2 * s; *y = 228; break;
    case BTN_DROP:  *x = p;       *y = 284; *w = 3 * s - 4; *h = 40; break;
    case BTN_PAUSE: *x = p;       *y = 332; *w = 3 * s - 4; *h = 36; break;
    default:        *x = p;       *y = 376; *w = 3 * s - 4; *h = 36; break; /* New */
    }
}

static uint32_t lighten(uint32_t c, int add) {
    int r = (int)((c >> 16) & 0xFF) + add, g = (int)((c >> 8) & 0xFF) + add, b = (int)(c & 0xFF) + add;
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    if (r < 0) r = 0;
    if (g < 0) g = 0;
    if (b < 0) b = 0;
    return RGB(r, g, b);
}

static void draw_block(int x, int y, int sz, uint32_t c, bool ghost) {
    if (ghost) {
        gfx_rect(x + 1, y + 1, sz - 2, sz - 2, lighten(c, -20));
        gfx_rect(x + 2, y + 2, sz - 4, sz - 4, lighten(c, -40));
        return;
    }
    gfx_fill(x + 1, y + 1, sz - 2, sz - 2, c);
    gfx_fill(x + 1, y + 1, sz - 2, 2, lighten(c, 50));
    gfx_fill(x + 1, y + 1, 2, sz - 2, lighten(c, 40));
    gfx_fill(x + 1, y + sz - 3, sz - 2, 2, lighten(c, -50));
    gfx_fill(x + sz - 3, y + 1, 2, sz - 2, lighten(c, -40));
}

static void draw_shape(int ox, int oy, int t, int r, int sz, int gx, int gy, bool ghost) {
    uint16_t s = SHAPE[t][r & 3];
    for (int i = 0; i < 16; i++) if (s & (uint16_t)(1u << (15 - i))) {
        int c = gx + i % 4, row = gy + i / 4;
        if (row < 0) continue;
        draw_block(ox + c * sz, oy + row * sz, sz, PCOL[t], ghost);
    }
}

static void draw_arrow(int cx, int cy, int d, uint32_t c) {
    for (int i = 0; i < 10; i++) {
        int half = i;
        if (d == 0) gfx_fill(cx - half, cy - 6 + i, 2 * half + 1, 1, c); /* up / rotate hint */
        if (d == 2) gfx_fill(cx - half, cy + 6 - i, 2 * half + 1, 1, c);
        if (d == 1) gfx_fill(cx - 6 + i, cy - half, 1, 2 * half + 1, c);
        if (d == 3) gfx_fill(cx + 6 - i, cy - half, 1, 2 * half + 1, c);
    }
}

void tetris_draw(int x, int y, int w, int h, bool focused) {
    (void)w; (void)h; (void)focused;
    char buf[40], num[12];
    int bx = x + TETRIS_BX, by = y + TETRIS_BY;

    gfx_rect(bx - 2, by - 2, TETRIS_BOARD_W + 4, TETRIS_BOARD_H + 4, RGB(0x0E, 0x4D, 0x22));
    for (int r = 0; r < TETRIS_ROWS; r++)
        for (int c = 0; c < TETRIS_COLS; c++)
            gfx_fill(bx + c * TETRIS_CELL, by + r * TETRIS_CELL, TETRIS_CELL, TETRIS_CELL,
                     (r + c) & 1 ? RGB(0x16, 0x2E, 0x24) : RGB(0x1A, 0x34, 0x28));
    for (int r = 0; r < TETRIS_ROWS; r++)
        for (int c = 0; c < TETRIS_COLS; c++)
            if (grid[r][c]) draw_block(bx + c * TETRIS_CELL, by + r * TETRIS_CELL,
                                       TETRIS_CELL, PCOL[grid[r][c] - 1], false);
    if (state == ST_PLAYING || state == ST_PAUSED) {
        int gy = ghost_y();
        if (gy != py) draw_shape(bx, by, typ, rot, TETRIS_CELL, px, gy, true);
        draw_shape(bx, by, typ, rot, TETRIS_CELL, px, py, false);
    }

    /* side panel: next + stats */
    gfx_text(x + TETRIS_PANEL, y + 8, "Next", TEXT_DARK, TRANSPARENT, 1);
    gfx_fill(x + TETRIS_PANEL, y + 26, 3 * TETRIS_PADW - 4, 64, RGB(0x16, 0x2E, 0x24));
    gfx_rect(x + TETRIS_PANEL, y + 26, 3 * TETRIS_PADW - 4, 64, RGB(0x0E, 0x4D, 0x22));
    {   /* centre the 4x4 preview in the box (cells are 12 px) */
        int psz = 12, pw = 3 * TETRIS_PADW - 4;
        draw_shape(x + TETRIS_PANEL + (pw - 4 * psz) / 2, y + 26 + (64 - 4 * psz) / 2,
                   next, 0, psz, 0, 0, false);
    }
    strcpy(buf, "Score "); utoa(score, num, 10); strcpy(buf + strlen(buf), num);
    gfx_text(x + TETRIS_PANEL, y + 98, buf, TEXT_DARK, TRANSPARENT, 1);
    strcpy(buf, "Best  "); utoa(best, num, 10); strcpy(buf + strlen(buf), num);
    gfx_text(x + TETRIS_PANEL, y + 116, buf, RGB(0x40, 0x60, 0x48), TRANSPARENT, 1);
    strcpy(buf, "Lines "); utoa(lines, num, 10); strcpy(buf + strlen(buf), num);
    gfx_text(x + TETRIS_PANEL, y + 134, buf, TEXT_DARK, TRANSPARENT, 1);
    strcpy(buf, "Level "); utoa(level, num, 10); strcpy(buf + strlen(buf), num);
    gfx_text(x + TETRIS_PANEL, y + 152, buf, TEXT_DARK, TRANSPARENT, 1);

    static const char *labels[NBTNS] = { "Rotate", 0, 0, 0, "Drop", "Pause", "New game" };
    for (int i = 0; i < NBTNS; i++) {
        int ax, ay, aw, ah; pad_rect(i, &ax, &ay, &aw, &ah);
        uint32_t face = (i == BTN_NEW) ? RGB(0x1C, 0x75, 0x36)
                      : (i == BTN_DROP) ? RGB(0x2F, 0xA8, 0x4F)
                      : (i == BTN_PAUSE && (state == ST_PAUSED || paused_by_focus)) ? RGB(0xD9, 0x45, 0x3B)
                      : 0xF2F2F2;
        uint32_t fg = (i == BTN_NEW || i == BTN_DROP ||
                       (i == BTN_PAUSE && (state == ST_PAUSED || paused_by_focus))) ? 0xFFFFFF : TEXT_DARK;
        gfx_bevel(x + ax, y + ay, aw, ah, face, pressed == i);
        if (labels[i]) {
            int scale = (i == BTN_ROT || i == BTN_DROP) ? 2 : 1;
            int tw = (int)strlen(labels[i]) * FONT_W * scale;
            gfx_text(x + ax + (aw - tw) / 2, y + ay + (ah - FONT_H * scale) / 2, labels[i], fg, TRANSPARENT, scale);
        } else {
            int dir = (i == BTN_LEFT) ? 1 : (i == BTN_RIGHT) ? 3 : 2;
            draw_arrow(x + ax + aw / 2, y + ay + ah / 2, dir, TEXT_DARK);
        }
    }

    const char *msg = NULL, *msg2 = NULL;
    if (state == ST_READY) { msg = "Ready?"; msg2 = "Tap Rotate or an arrow to go"; }
    else if (state == ST_OVER) { msg = "Game over"; msg2 = "Tap New game (or N) to retry"; }
    else if (state == ST_PAUSED || paused_by_focus) { msg = "Paused"; msg2 = "Click here or press P to go on"; }
    if (msg) {
        int mw = TETRIS_BOARD_W, mh = 56;
        int mx = bx, myy = by + (TETRIS_BOARD_H - mh) / 2;
        gfx_shade(mx, myy, mw, mh, 130);
        gfx_text(mx + (mw - (int)strlen(msg) * FONT_W * 2) / 2, myy + 6, msg,
                 state == ST_OVER ? RGB(0xFF, 0xD0, 0x60) : 0xFFFFFF, TRANSPARENT, 2);
        gfx_text(mx + (mw - (int)strlen(msg2) * FONT_W) / 2, myy + 38, msg2, RGB(0xD8, 0xFF, 0xE0), TRANSPARENT, 1);
    }
}

static void do_pause(void) {
    if (state == ST_PLAYING) state = ST_PAUSED;
    else if (state == ST_PAUSED) state = ST_PLAYING;
}

void tetris_click(int lx, int ly) {
    rng ^= (uint32_t)(lx * 131 + ly * 17 + 1);
    for (int i = 0; i < NBTNS; i++) {
        int ax, ay, aw, ah; pad_rect(i, &ax, &ay, &aw, &ah);
        if (lx >= ax && ly >= ay && lx < ax + aw && ly < ay + ah) {
            pressed = i;
            if (i == BTN_NEW) { tetris_init(); return; }
            if (i == BTN_PAUSE) { do_pause(); return; }
            if (state == ST_OVER) return;
            if (i == BTN_ROT) rotate_cw();
            else if (i == BTN_LEFT) move_x(-1);
            else if (i == BTN_RIGHT) move_x(1);
            else if (i == BTN_DOWN) { soft_drop = true; soft_one(); }
            else if (i == BTN_DROP) hard_drop();
            return;
        }
    }
    /* tapping the well: left/right of the falling piece moves it; above rotates; below soft-drops */
    int bx = lx - TETRIS_BX, by = ly - TETRIS_BY;
    if (bx < 0 || by < 0 || bx >= TETRIS_BOARD_W || by >= TETRIS_BOARD_H) return;
    if (state == ST_OVER) { tetris_init(); return; }
    if (state == ST_PAUSED) { state = ST_PLAYING; return; }
    start_if_needed();
    if (state != ST_PLAYING) return;
    int hx = px * TETRIS_CELL + 2 * TETRIS_CELL, hy = py * TETRIS_CELL + 2 * TETRIS_CELL;
    int dx = bx - hx, dy = by - hy;
    int ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
    if (ay > ax && dy < 0) rotate_cw();
    else if (ay > ax) soft_one();
    else move_x(dx > 0 ? 1 : -1);
}

void tetris_release(void) { pressed = -1; soft_drop = false; }

void tetris_key(int c) {
    rng ^= (uint32_t)c * 2654435761u;
    if (c == 'n' || c == 'N') { tetris_init(); return; }
    if (c == 'p' || c == 'P') { do_pause(); return; }
    if (state == ST_OVER && (c == '\n' || c == ' ')) { tetris_init(); return; }
    if (c == TKEY_LEFT  || c == 'a' || c == 'A') move_x(-1);
    else if (c == TKEY_RIGHT || c == 'd' || c == 'D') move_x(1);
    else if (c == TKEY_DOWN  || c == 's' || c == 'S') soft_one();
    else if (c == TKEY_UP    || c == 'w' || c == 'W' || c == 'x' || c == 'X' || c == 'z' || c == 'Z') rotate_cw();
    else if (c == ' ') hard_drop();
    else if (c == '\n' && state == ST_READY) { start_if_needed(); }
}

void tetris_icon(int x, int y) {
    gfx_fill(x + 1, y + 1, 30, 30, RGB(0x16, 0x2E, 0x24));
    gfx_rect(x + 1, y + 1, 30, 30, RGB(0x0E, 0x4D, 0x22));
    draw_block(x + 5,  y + 17, 8, PCOL[5], false); /* J */
    draw_block(x + 5,  y + 9,  8, PCOL[5], false);
    draw_block(x + 13, y + 17, 8, PCOL[5], false);
    draw_block(x + 21, y + 17, 8, PCOL[2], false); /* T */
    draw_block(x + 13, y + 9,  8, PCOL[6], false); /* L */
    draw_block(x + 21, y + 9,  8, PCOL[6], false);
    draw_block(x + 21, y + 1,  8, PCOL[1], false); /* O top */
}
