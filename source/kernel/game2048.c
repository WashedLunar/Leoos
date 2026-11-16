/* LeoOS - 2048 (by Luna) */
#include "game2048.h"
#include "gfx.h"
#include "cpu.h"
#include "string.h"

#define TEXT_DARK RGB(0x10, 0x30, 0x18)
#define PAD 48
enum { UP, RIGHT, DOWN, LEFT };
static int board[G2048_N][G2048_N];               /* 0 = empty, else the tile value */
static uint32_t score, best, rng = 0x9E3779B9;
static bool won, over, keep_going;
static int pressed = -1, sw_x = -1, sw_y, sw_lx, sw_ly;

static uint32_t rnd(void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }

static void add_tile(void) {
    int empty = 0;
    for (int r = 0; r < G2048_N; r++) for (int c = 0; c < G2048_N; c++) if (!board[r][c]) empty++;
    if (!empty) return;
    int k = (int)(rnd() % (uint32_t)empty);
    for (int r = 0; r < G2048_N; r++) for (int c = 0; c < G2048_N; c++)
        if (!board[r][c] && k-- == 0) { board[r][c] = rnd() % 10 ? 2 : 4; return; }
}

static bool can_move(void) {
    for (int r = 0; r < G2048_N; r++) for (int c = 0; c < G2048_N; c++) {
        if (!board[r][c]) return true;
        if (c + 1 < G2048_N && board[r][c] == board[r][c + 1]) return true;
        if (r + 1 < G2048_N && board[r][c] == board[r + 1][c]) return true;
    }
    return false;
}

void g2048_init(void) {
    memset(board, 0, sizeof board);
    score = 0; won = over = keep_going = false;
    rng ^= timer_ticks() * 2654435761u; if (!rng) rng = 1;
    add_tile(); add_tile();
}

/* cell i (0..3) of line l when sliding in direction d, i = 0 is the side tiles slide towards */
static int *cell(int d, int l, int i) {
    switch (d) {
    case LEFT:  return &board[l][i];
    case RIGHT: return &board[l][G2048_N - 1 - i];
    case UP:    return &board[i][l];
    default:    return &board[G2048_N - 1 - i][l];
    }
}

static void move(int d) {
    if (over || (won && !keep_going)) return;
    bool moved = false;
    for (int l = 0; l < G2048_N; l++) {
        int vals[G2048_N], n = 0;
        for (int i = 0; i < G2048_N; i++) if (*cell(d, l, i)) vals[n++] = *cell(d, l, i);
        int out[G2048_N] = { 0 }, m = 0;
        for (int i = 0; i < n; i++) {
            if (i + 1 < n && vals[i] == vals[i + 1]) {
                out[m] = vals[i] * 2; score += (uint32_t)out[m];
                if (out[m] == 2048) won = true;
                m++; i++;
            } else out[m++] = vals[i];
        }
        for (int i = 0; i < G2048_N; i++) {
            if (*cell(d, l, i) != out[i]) moved = true;
            *cell(d, l, i) = out[i];
        }
    }
    if (moved) add_tile();
    if (score > best) best = score;
    if (!can_move()) over = true;
}

static uint32_t tile_colour(int v) {
    switch (v) {
    case 0:    return RGB(0xC2, 0xD6, 0xC6);
    case 2:    return RGB(0xEE, 0xF5, 0xEF);
    case 4:    return RGB(0xDC, 0xF0, 0xD0);
    case 8:    return RGB(0xA8, 0xE0, 0x9A);
    case 16:   return RGB(0x7C, 0xD0, 0x7A);
    case 32:   return RGB(0x4E, 0xBE, 0x62);
    case 64:   return RGB(0x2F, 0xA8, 0x4F);
    case 128:  return RGB(0xF5, 0xD0, 0x2A);
    case 256:  return RGB(0xF0, 0xB0, 0x24);
    case 512:  return RGB(0xF0, 0x8A, 0x24);
    case 1024: return RGB(0xE0, 0x6A, 0x5A);
    default:   return RGB(0x0B, 0x6B, 0x26);
    }
}

static void pad_rect(int i, int *x, int *y) {          /* 0 up, 1 right, 2 down, 3 left, 4 new */
    static const int cx[5] = { 1, 2, 1, 0, 0 }, cy[5] = { 0, 1, 2, 1, 4 };
    *x = G2048_PANEL + cx[i] * PAD; *y = G2048_BY + 60 + cy[i] * PAD;
}

void g2048_draw(int x, int y, int w, int h, bool focused) {
    (void)w; (void)h; (void)focused;
    char buf[32], num[12];
    strcpy(buf, "Score: "); utoa(score, num, 10); strcpy(buf + strlen(buf), num);
    gfx_text(x + G2048_BX, y + 12, buf, TEXT_DARK, TRANSPARENT, 1);
    strcpy(buf, "Best: "); utoa(best, num, 10); strcpy(buf + strlen(buf), num);
    gfx_text(x + G2048_PANEL, y + 12, buf, TEXT_DARK, TRANSPARENT, 1);

    gfx_fill(x + G2048_BX, y + G2048_BY, G2048_BOARD, G2048_BOARD, RGB(0x8F, 0xB0, 0x96));
    for (int r = 0; r < G2048_N; r++)
        for (int c = 0; c < G2048_N; c++) {
            int v = board[r][c];
            int tx = x + G2048_BX + G2048_GAP + c * (G2048_CELL + G2048_GAP);
            int ty = y + G2048_BY + G2048_GAP + r * (G2048_CELL + G2048_GAP);
            gfx_fill(tx, ty, G2048_CELL, G2048_CELL, tile_colour(v));
            if (!v) continue;
            utoa((uint32_t)v, num, 10);
            int len = (int)strlen(num), scale = len <= 2 ? 2 : 1;
            uint32_t fg = v <= 4 ? TEXT_DARK : 0xFFFFFF;
            gfx_text(tx + (G2048_CELL - len * FONT_W * scale) / 2, ty + (G2048_CELL - FONT_H * scale) / 2, num, fg, TRANSPARENT, scale);
        }

    static const char *labels[5] = { "^", ">", "v", "<", "New" };
    for (int i = 0; i < 5; i++) {
        int bx, by; pad_rect(i, &bx, &by);
        int bw = i == 4 ? PAD * 3 - 4 : PAD - 4;
        gfx_bevel(x + bx, y + by, bw, PAD - 4, i == 4 ? RGB(0x2F, 0xA8, 0x4F) : 0xF2F2F2, pressed == i);
        gfx_text(x + bx + (bw - (int)strlen(labels[i]) * FONT_W * 2) / 2, y + by + 8, labels[i], i == 4 ? 0xFFFFFF : TEXT_DARK, TRANSPARENT, 2);
    }

    const char *banner = over ? "Game over" : (won && !keep_going) ? "You made 2048!" : NULL;
    if (banner) {
        int by = y + G2048_BY + G2048_BOARD / 2 - 24;
        gfx_shade(x + G2048_BX, by, G2048_BOARD, 48, 140);
        gfx_text(x + G2048_BX + (G2048_BOARD - (int)strlen(banner) * FONT_W * 2) / 2, by + 8, banner, 0xFFFFFF, TRANSPARENT, 2);
        if (won && !over) gfx_text(x + G2048_BX + (G2048_BOARD - 22 * FONT_W) / 2, by + 52, "Tap the board to go on", TEXT_DARK, TRANSPARENT, 1);
    }
}

static bool on_board(int lx, int ly) {
    return lx >= G2048_BX && ly >= G2048_BY && lx < G2048_BX + G2048_BOARD && ly < G2048_BY + G2048_BOARD;
}

void g2048_click(int lx, int ly) {
    for (int i = 0; i < 5; i++) {
        int bx, by; pad_rect(i, &bx, &by);
        int bw = i == 4 ? PAD * 3 - 4 : PAD - 4;
        if (lx >= bx && ly >= by && lx < bx + bw && ly < by + PAD - 4) {
            pressed = i;
            if (i == 4) g2048_init(); else move(i);
            return;
        }
    }
    if (on_board(lx, ly)) {
        if (won && !keep_going && !over) { keep_going = true; return; }
        sw_x = sw_lx = lx; sw_y = sw_ly = ly;            /* start of a swipe */
    }
}

void g2048_drag(int lx, int ly) { if (sw_x >= 0) { sw_lx = lx; sw_ly = ly; } }

void g2048_release(void) {
    pressed = -1;
    if (sw_x < 0) return;
    int dx = sw_lx - sw_x, dy = sw_ly - sw_y;
    int ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
    sw_x = -1;
    if (ax < 20 && ay < 20) return;                     /* too short to be a swipe */
    if (ax > ay) move(dx > 0 ? RIGHT : LEFT);
    else move(dy > 0 ? DOWN : UP);
}

void g2048_key(int c) {
    if (c == KEY_UP || c == 'w' || c == 'W') move(UP);
    else if (c == KEY_RIGHT || c == 'd' || c == 'D') move(RIGHT);
    else if (c == KEY_DOWN || c == 's' || c == 'S') move(DOWN);
    else if (c == KEY_LEFT || c == 'a' || c == 'A') move(LEFT);
    else if (c == 'n' || c == 'N') g2048_init();
    else if (c == '\n' && won && !keep_going) keep_going = true;
}

void g2048_icon(int x, int y) {
    gfx_fill(x + 1, y + 1, 30, 30, RGB(0x8F, 0xB0, 0x96));
    gfx_fill(x + 3, y + 3, 12, 12, RGB(0xF5, 0xD0, 0x2A));
    gfx_fill(x + 17, y + 3, 12, 12, RGB(0x2F, 0xA8, 0x4F));
    gfx_fill(x + 3, y + 17, 12, 12, RGB(0xEE, 0xF5, 0xEF));
    gfx_fill(x + 17, y + 17, 12, 12, RGB(0x0B, 0x6B, 0x26));
    gfx_text(x + 6, y + 3, "2", TEXT_DARK, TRANSPARENT, 1);
}
