/* LeoOS - Minesweeper (by Luna) */
#include "minesweeper.h"
#include "gfx.h"
#include "cpu.h"
#include "string.h"

#define TEXT_DARK RGB(0x10, 0x30, 0x18)
enum { ST_READY, ST_PLAYING, ST_WON, ST_LOST };

static bool mine[MINES_ROWS][MINES_COLS], open_[MINES_ROWS][MINES_COLS], flag[MINES_ROWS][MINES_COLS];
static uint8_t near[MINES_ROWS][MINES_COLS];
static int state, flags_left, opened, boom_r = -1, boom_c = -1;
static bool flag_mode;
static int pressed = -1;               /* 0 = New, 1 = Flag */
static uint32_t rng = 0x1234567;

static uint32_t rnd(void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }

void mines_init(void) {
    memset(mine, 0, sizeof mine); memset(open_, 0, sizeof open_);
    memset(flag, 0, sizeof flag); memset(near, 0, sizeof near);
    state = ST_READY; flags_left = MINES_COUNT; opened = 0; boom_r = boom_c = -1;
}

/* Mines are placed on the first click, never on or next to that cell. */
static void place_mines(int sr, int sc) {
    rng ^= timer_ticks() * 2654435761u; if (!rng) rng = 1;
    int placed = 0;
    while (placed < MINES_COUNT) {
        int r = (int)(rnd() % MINES_ROWS), c = (int)(rnd() % MINES_COLS);
        int dr = r - sr, dc = c - sc;
        if (mine[r][c] || (dr >= -1 && dr <= 1 && dc >= -1 && dc <= 1)) continue;
        mine[r][c] = true; placed++;
    }
    for (int r = 0; r < MINES_ROWS; r++)
        for (int c = 0; c < MINES_COLS; c++)
            for (int dr = -1; dr <= 1; dr++)
                for (int dc = -1; dc <= 1; dc++) {
                    int rr = r + dr, cc = c + dc;
                    if ((dr || dc) && rr >= 0 && cc >= 0 && rr < MINES_ROWS && cc < MINES_COLS && mine[rr][cc]) near[r][c]++;
                }
}

/* Flood-open with an explicit stack (no deep recursion on the kernel stack).
 * Each cell is pushed at most once (tracked in `queued`), so the stack can't overflow. */
static void reveal(int r0, int c0) {
    static int8_t stack[MINES_ROWS * MINES_COLS][2];
    static bool queued[MINES_ROWS][MINES_COLS];
    memset(queued, 0, sizeof queued);
    int sp = 0;
    stack[sp][0] = (int8_t)r0; stack[sp][1] = (int8_t)c0; sp++; queued[r0][c0] = true;
    while (sp) {
        sp--; int r = stack[sp][0], c = stack[sp][1];
        if (open_[r][c] || flag[r][c]) continue;
        open_[r][c] = true; opened++;
        if (near[r][c]) continue;
        for (int dr = -1; dr <= 1; dr++)
            for (int dc = -1; dc <= 1; dc++) {
                int rr = r + dr, cc = c + dc;
                if (rr >= 0 && cc >= 0 && rr < MINES_ROWS && cc < MINES_COLS && !open_[rr][cc] && !flag[rr][cc] && !queued[rr][cc]) {
                    queued[rr][cc] = true;
                    stack[sp][0] = (int8_t)rr; stack[sp][1] = (int8_t)cc; sp++;
                }
            }
    }
}

static void open_cell(int r, int c) {
    if (open_[r][c] || flag[r][c]) return;
    if (state == ST_READY) { place_mines(r, c); state = ST_PLAYING; }
    if (mine[r][c]) {
        state = ST_LOST; boom_r = r; boom_c = c;
        for (int i = 0; i < MINES_ROWS; i++) for (int j = 0; j < MINES_COLS; j++) if (mine[i][j]) open_[i][j] = true;
        return;
    }
    reveal(r, c);
    if (opened == MINES_ROWS * MINES_COLS - MINES_COUNT) {
        state = ST_WON;
        for (int i = 0; i < MINES_ROWS; i++) for (int j = 0; j < MINES_COLS; j++) if (mine[i][j]) flag[i][j] = true;
        flags_left = 0;
    }
}

static void toggle_flag(int r, int c) {
    if (open_[r][c]) return;
    if (flag[r][c]) { flag[r][c] = false; flags_left++; }
    else if (flags_left > 0) { flag[r][c] = true; flags_left--; }
}

/* toolbar: [New] at left, status text in middle, [Flag] at right */
#define BTN_Y 8
#define BTN_H 28
#define NEW_X MINES_PAD
#define NEW_W 60
#define FLAG_W 70
#define FLAG_X (MINES_W - MINES_PAD - FLAG_W)

static const uint32_t num_colours[9] = {
    0, 0x1B3B8C, 0x0B6B26, 0xD9453B, 0x4B1C7A, 0x8B2A1B, 0x147A7A, 0x000000, 0x606060,
};

static void draw_flag(int x, int y) {
    gfx_fill(x + 13, y + 6, 2, 16, 0x202020);
    gfx_fill(x + 8, y + 21, 12, 2, 0x202020);
    for (int i = 0; i < 6; i++) gfx_fill(x + 7 + i, y + 6 + i / 2, 6 - i, 8 - i, 0xD9453B);
    gfx_fill(x + 7, y + 6, 6, 8, 0xD9453B);
}

static void draw_mine(int x, int y) {
    gfx_line(x + 7, y + 14, x + 21, y + 14, 2, 0x101010);
    gfx_line(x + 14, y + 7, x + 14, y + 21, 2, 0x101010);
    gfx_disc(x + 14, y + 14, 6, 0x101010);
    gfx_fill(x + 11, y + 11, 2, 2, 0xFFFFFF);
}

void mines_draw(int x, int y, int w, int h, bool focused) {
    (void)w; (void)h; (void)focused;
    char buf[24], num[8];
    gfx_bevel(x + NEW_X, y + BTN_Y, NEW_W, BTN_H, 0xF2F2F2, pressed == 0);
    {   /* smiley face: happy, cool when won, sad when lost */
        int fx = x + NEW_X + 15, fy = y + BTN_Y + BTN_H / 2;
        gfx_disc(fx, fy, 9, RGB(0xF5, 0xD0, 0x2A));
        if (state == ST_WON) { gfx_fill(fx - 6, fy - 4, 12, 3, 0x101010); }
        else if (state == ST_LOST) {
            gfx_line(fx - 6, fy - 5, fx - 3, fy - 2, 1, 0x101010); gfx_line(fx - 6, fy - 2, fx - 3, fy - 5, 1, 0x101010);
            gfx_line(fx + 2, fy - 5, fx + 5, fy - 2, 1, 0x101010); gfx_line(fx + 2, fy - 2, fx + 5, fy - 5, 1, 0x101010);
        } else { gfx_fill(fx - 4, fy - 4, 2, 3, 0x101010); gfx_fill(fx + 3, fy - 4, 2, 3, 0x101010); }
        if (state == ST_LOST) gfx_line(fx - 4, fy + 5, fx + 4, fy + 5, 1, 0x101010);
        else { gfx_fill(fx - 4, fy + 3, 9, 1, 0x101010); gfx_fill(fx - 3, fy + 4, 7, 1, 0x101010); }
        gfx_text(x + NEW_X + 28, y + BTN_Y + 6, "New", TEXT_DARK, TRANSPARENT, 1);
    }
    gfx_bevel(x + FLAG_X, y + BTN_Y, FLAG_W, BTN_H, flag_mode ? RGB(0xD9, 0x45, 0x3B) : 0xF2F2F2, flag_mode || pressed == 1);
    gfx_text(x + FLAG_X + 8, y + BTN_Y + 6, flag_mode ? "Flag:ON" : "Flag", flag_mode ? 0xFFFFFF : TEXT_DARK, TRANSPARENT, 1);

    if (state == ST_WON) strcpy(buf, "You win!");
    else if (state == ST_LOST) strcpy(buf, "Boom!");
    else { strcpy(buf, "Mines: "); utoa((uint32_t)flags_left, num, 10); strcpy(buf + strlen(buf), num); }
    uint32_t sc = state == ST_WON ? RGB(0x0B, 0x6B, 0x26) : state == ST_LOST ? RGB(0xB0, 0x20, 0x20) : TEXT_DARK;
    int mid = NEW_X + NEW_W, span = FLAG_X - mid;
    gfx_text(x + mid + (span - (int)strlen(buf) * FONT_W) / 2, y + BTN_Y + 6, buf, sc, TRANSPARENT, 1);

    for (int r = 0; r < MINES_ROWS; r++)
        for (int c = 0; c < MINES_COLS; c++) {
            int cx = x + MINES_PAD + c * MINES_CELL, cy = y + MINES_TOP + r * MINES_CELL;
            if (!open_[r][c]) {
                gfx_bevel(cx, cy, MINES_CELL, MINES_CELL, RGB(0x8F, 0xC9, 0x9C), false);
                if (flag[r][c]) draw_flag(cx, cy);
                continue;
            }
            bool boom = r == boom_r && c == boom_c;
            gfx_fill(cx, cy, MINES_CELL, MINES_CELL, boom ? RGB(0xF0, 0x70, 0x60) : RGB(0xEE, 0xF5, 0xEF));
            gfx_rect(cx, cy, MINES_CELL, MINES_CELL, RGB(0xB0, 0xC4, 0xB4));
            if (mine[r][c]) draw_mine(cx, cy);
            else if (near[r][c]) {
                char s[2] = { (char)('0' + near[r][c]), 0 };
                gfx_text(cx + (MINES_CELL - FONT_W) / 2 + 1, cy + (MINES_CELL - FONT_H) / 2, s, num_colours[near[r][c]], TRANSPARENT, 1);
                gfx_text(cx + (MINES_CELL - FONT_W) / 2, cy + (MINES_CELL - FONT_H) / 2, s, num_colours[near[r][c]], TRANSPARENT, 1);
            }
        }
    if (state == ST_WON || state == ST_LOST) {          /* banner over the board */
        const char *m = state == ST_WON ? "You win!" : "Boom!";
        int bw = 200, bh = 54;
        int bx = x + (MINES_W - bw) / 2, by = y + MINES_TOP + (MINES_ROWS * MINES_CELL - bh) / 2;
        gfx_fill(bx, by, bw, bh, state == ST_WON ? RGB(0x1C, 0x75, 0x36) : RGB(0xA0, 0x28, 0x22));
        gfx_rect(bx, by, bw, bh, 0xFFFFFF);
        gfx_text(bx + (bw - (int)strlen(m) * FONT_W * 2) / 2, by + 6, m, 0xFFFFFF, TRANSPARENT, 2);
        gfx_text(bx + (bw - 21 * FONT_W) / 2, by + 36, "Tap New to play again", RGB(0xF5, 0xD0, 0x2A), TRANSPARENT, 1);
    }
}

void mines_click(int lx, int ly) {
    if (ly >= BTN_Y && ly < BTN_Y + BTN_H) {
        if (lx >= NEW_X && lx < NEW_X + NEW_W) { pressed = 0; mines_init(); return; }
        if (lx >= FLAG_X && lx < FLAG_X + FLAG_W) { pressed = 1; flag_mode = !flag_mode; return; }
    }
    if (state == ST_WON || state == ST_LOST) return;
    int c = (lx - MINES_PAD) / MINES_CELL, r = (ly - MINES_TOP) / MINES_CELL;
    if (lx < MINES_PAD || ly < MINES_TOP || c >= MINES_COLS || r >= MINES_ROWS) return;
    if (flag_mode) { if (state != ST_READY) toggle_flag(r, c); }
    else open_cell(r, c);
}

void mines_release(void) { pressed = -1; }

/* Right-click places a flag directly (added by Nova for mouse users). */
void mines_rclick(int lx, int ly) {
    if (state != ST_PLAYING) return;
    int c = (lx - MINES_PAD) / MINES_CELL, r = (ly - MINES_TOP) / MINES_CELL;
    if (lx < MINES_PAD || ly < MINES_TOP || c >= MINES_COLS || r >= MINES_ROWS) return;
    toggle_flag(r, c);
}

void mines_key(int c) {
    if (c == 'n' || c == 'N' || c == '\n') mines_init();
    else if (c == 'f' || c == 'F') flag_mode = !flag_mode;
}

void mines_icon(int x, int y) {
    for (int r = 0; r < 2; r++)
        for (int c = 0; c < 2; c++)
            gfx_bevel(x + 2 + c * 14, y + 2 + r * 14, 14, 14, RGB(0x8F, 0xC9, 0x9C), false);
    gfx_disc(x + 22, y + 22, 7, 0x101010);
    gfx_fill(x + 19, y + 19, 2, 2, 0xFFFFFF);
    gfx_fill(x + 8, y + 5, 2, 9, 0x202020);
    gfx_fill(x + 4, y + 5, 4, 4, 0xD9453B);
}
