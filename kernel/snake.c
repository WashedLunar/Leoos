/* LeoOS - Snake (new in 0.3). Arrow keys / WASD, or tap the big on-screen arrows
 * (or tap the board on the side you want to turn to), so it works on a phone too.
 * The PIT timer drives the game: one step every few 10 ms ticks. */
#include "snake.h"
#include "gui.h"
#include "gfx.h"
#include "cpu.h"
#include "string.h"

#define TEXT_DARK RGB(0x10, 0x30, 0x18)
#define MAXLEN (SNAKE_COLS * SNAKE_ROWS)
#define PAD    52                                   /* arrow pad button size */
enum { READY, RUNNING, OVER };
enum { UP, RIGHT, DOWN, LEFT };
static const int DX[4] = { 0, 1, 0, -1 }, DY[4] = { -1, 0, 1, 0 };

static uint8_t sx[MAXLEN], sy[MAXLEN];              /* body, sx[0] = head */
static int len, dir, state, score, best, food_x, food_y;
static int queue[2], queued;                        /* buffered turns so quick taps aren't lost */
static uint32_t last_step, rng = 0x2545F491;
static int pressed = -1;                            /* 0..3 arrows, 4 = new game */
static bool paused_by_focus;

static uint32_t rnd(void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }

static bool on_snake(int x, int y, int upto) {
    for (int i = 0; i < upto; i++) if (sx[i] == x && sy[i] == y) return true;
    return false;
}

static void place_food(void) {
    if (len >= MAXLEN) return;
    do { food_x = (int)(rnd() % SNAKE_COLS); food_y = (int)(rnd() % SNAKE_ROWS); } while (on_snake(food_x, food_y, len));
}

void snake_init(void) {
    len = 4; dir = RIGHT; state = READY; score = 0; queued = 0;
    for (int i = 0; i < len; i++) { sx[i] = (uint8_t)(SNAKE_COLS / 2 - i); sy[i] = SNAKE_ROWS / 2; }
    rng ^= timer_ticks() * 2654435761u; if (!rng) rng = 1;
    place_food();
}

static void turn(int d) {
    if (state == OVER) return;
    if (state == READY) {                           /* first arrow starts the game */
        if (d == LEFT) d = UP;                      /* can't reverse into ourselves */
        dir = d; state = RUNNING; last_step = timer_ticks(); queued = 0;
        return;
    }
    int last = queued ? queue[queued - 1] : dir;
    if (d == last || (d + 2) % 4 == last) return;   /* same way, or straight back */
    if (queued < 2) queue[queued++] = d;
}

/* Starts gently (0.22 s per step) and speeds up as you eat. 0.3 started at 0.15 s, which,
 * with the walls, ended most games within 2 seconds in the browser. */
static int step_ticks(void) { int t = 22 - score / 2; return t < 8 ? 8 : t; }

static void step(void) {
    if (queued) { dir = queue[0]; queue[0] = queue[1]; queued--; }
    int nx = sx[0] + DX[dir], ny = sy[0] + DY[dir];
    bool eat = nx == food_x && ny == food_y;
    /* the walls wrap round: leave on one side, come back on the other. Only biting yourself ends the game */
    if (nx < 0) nx = SNAKE_COLS - 1;
    if (nx >= SNAKE_COLS) nx = 0;
    if (ny < 0) ny = SNAKE_ROWS - 1;
    if (ny >= SNAKE_ROWS) ny = 0;
    eat = nx == food_x && ny == food_y;
    if (on_snake(nx, ny, eat ? len : len - 1)) {
        state = OVER;
        if (score > best) best = score;
        return;
    }
    if (eat && len < MAXLEN) len++;
    for (int i = len - 1; i > 0; i--) { sx[i] = sx[i - 1]; sy[i] = sy[i - 1]; }
    sx[0] = (uint8_t)nx; sy[0] = (uint8_t)ny;
    if (eat) { score++; if (score > best) best = score; place_food(); }
}

bool snake_tick(bool focused) {
    paused_by_focus = !focused && state == RUNNING;
    if (state != RUNNING) return false;
    uint32_t now = timer_ticks();
    if (!focused) { last_step = now; return false; }   /* pause while another window has focus */
    if (now - last_step < (uint32_t)step_ticks()) return false;
    last_step = now;
    step();
    return true;
}

/* arrow pad layout (client coordinates) */
static void pad_rect(int i, int *x, int *y) {
    static const int col[4] = { 1, 2, 1, 0 }, row[4] = { 0, 1, 2, 1 };
    *x = SNAKE_PANEL + col[i] * PAD; *y = 92 + row[i] * PAD;
}
#define NEW_X SNAKE_PANEL
#define NEW_Y (92 + 3 * PAD + 14)
#define NEW_W (3 * PAD - 4)
#define NEW_H 34

static void draw_arrow(int cx, int cy, int d, uint32_t c) {
    for (int i = 0; i < 10; i++) {                  /* a filled triangle, 10 rows */
        int half = i;
        if (d == UP)    gfx_fill(cx - half, cy - 6 + i, 2 * half + 1, 1, c);
        if (d == DOWN)  gfx_fill(cx - half, cy + 6 - i, 2 * half + 1, 1, c);
        if (d == LEFT)  gfx_fill(cx - 6 + i, cy - half, 1, 2 * half + 1, c);
        if (d == RIGHT) gfx_fill(cx + 6 - i, cy - half, 1, 2 * half + 1, c);
    }
}

void snake_draw(int x, int y, int w, int h, bool focused) {
    (void)w; (void)h; (void)focused;
    char buf[40], num[12];
    strcpy(buf, "Score: "); utoa((uint32_t)score, num, 10); strcpy(buf + strlen(buf), num);
    gfx_text(x + SNAKE_BX, y + 10, buf, TEXT_DARK, TRANSPARENT, 1);
    strcpy(buf, "Best: "); utoa((uint32_t)best, num, 10); strcpy(buf + strlen(buf), num);
    gfx_text(x + SNAKE_BX + 140, y + 10, buf, RGB(0x40, 0x60, 0x48), TRANSPARENT, 1);

    int bx = x + SNAKE_BX, by = y + SNAKE_BY;
    gfx_rect(bx - 2, by - 2, SNAKE_COLS * SNAKE_CELL + 4, SNAKE_ROWS * SNAKE_CELL + 4, RGB(0x0E, 0x4D, 0x22));
    for (int r = 0; r < SNAKE_ROWS; r++)
        for (int c = 0; c < SNAKE_COLS; c++)
            gfx_fill(bx + c * SNAKE_CELL, by + r * SNAKE_CELL, SNAKE_CELL, SNAKE_CELL,
                     (r + c) & 1 ? RGB(0x1E, 0x6B, 0x34) : RGB(0x23, 0x77, 0x3A));
    /* apple */
    int fx = bx + food_x * SNAKE_CELL, fy = by + food_y * SNAKE_CELL;
    gfx_disc(fx + 9, fy + 10, 7, RGB(0xE0, 0x3C, 0x31));
    gfx_fill(fx + 9, fy + 1, 2, 4, RGB(0x6B, 0x45, 0x1E));
    gfx_fill(fx + 11, fy + 2, 4, 2, RGB(0x7C, 0xE0, 0x6A));
    /* snake */
    for (int i = len - 1; i >= 0; i--) {
        int px = bx + sx[i] * SNAKE_CELL, py = by + sy[i] * SNAKE_CELL;
        uint32_t c = i == 0 ? RGB(0xF5, 0xD0, 0x2A) : (i & 1 ? RGB(0x5C, 0xE0, 0x7C) : RGB(0x4C, 0xD0, 0x6C));
        gfx_fill(px + 1, py + 1, SNAKE_CELL - 2, SNAKE_CELL - 2, c);
        if (i == 0) {                                /* eyes */
            gfx_fill(px + 5, py + 5, 3, 3, 0x000000);
            gfx_fill(px + 11, py + 5, 3, 3, 0x000000);
        }
    }
    /* messages */
    const char *msg = NULL, *msg2 = NULL;
    if (state == READY) { msg = "Ready?"; msg2 = "Tap an arrow (or arrow key) to go"; }
    else if (state == OVER) { msg = "Game over!"; msg2 = "Tap New game (or Enter) to retry"; }
    else if (paused_by_focus) { msg = "Paused"; msg2 = "Click here to carry on"; }
    if (msg) {
        int mw = 300, mh = 56, mxx = bx + (SNAKE_COLS * SNAKE_CELL - mw) / 2, myy = by + (SNAKE_ROWS * SNAKE_CELL - mh) / 2;
        gfx_shade(mxx, myy, mw, mh, 120);
        int tw = (int)strlen(msg) * FONT_W * 2;
        gfx_text(mxx + (mw - tw) / 2, myy + 6, msg, state == OVER ? RGB(0xFF, 0xD0, 0x60) : 0xFFFFFF, TRANSPARENT, 2);
        tw = (int)strlen(msg2) * FONT_W;
        gfx_text(mxx + (mw - tw) / 2, myy + 38, msg2, RGB(0xD8, 0xFF, 0xE0), TRANSPARENT, 1);
    }
    /* control panel */
    gfx_text(x + SNAKE_PANEL + 2, y + SNAKE_BY, "Controls", TEXT_DARK, TRANSPARENT, 1);
    gfx_text(x + SNAKE_PANEL + 2, y + SNAKE_BY + 18, "tap the arrows", RGB(0x40, 0x60, 0x48), TRANSPARENT, 1);
    gfx_text(x + SNAKE_PANEL + 2, y + SNAKE_BY + 34, "Walls wrap round!", RGB(0x40, 0x60, 0x48), TRANSPARENT, 1);
    for (int i = 0; i < 4; i++) {
        int ax, ay; pad_rect(i, &ax, &ay);
        gfx_bevel(x + ax, y + ay, PAD - 4, PAD - 4, RGB(0x3B, 0xB0, 0x5A), pressed == i);
        draw_arrow(x + ax + (PAD - 4) / 2, y + ay + (PAD - 4) / 2, i, 0xFFFFFF);
    }
    gfx_disc(x + SNAKE_PANEL + PAD + (PAD - 4) / 2, y + 92 + PAD + (PAD - 4) / 2, 8, RGB(0x1C, 0x75, 0x36));
    gfx_bevel(x + NEW_X, y + NEW_Y, NEW_W, NEW_H, RGB(0x1C, 0x75, 0x36), pressed == 4);
    const char *nl = "New game";
    gfx_text(x + NEW_X + (NEW_W - (int)strlen(nl) * FONT_W) / 2, y + NEW_Y + 9, nl, 0xFFFFFF, TRANSPARENT, 1);
}

void snake_click(int lx, int ly) {
    for (int i = 0; i < 4; i++) {
        int ax, ay; pad_rect(i, &ax, &ay);
        if (lx >= ax && ly >= ay && lx < ax + PAD - 4 && ly < ay + PAD - 4) { pressed = i; turn(i); return; }
    }
    if (lx >= NEW_X && ly >= NEW_Y && lx < NEW_X + NEW_W && ly < NEW_Y + NEW_H) { pressed = 4; snake_init(); return; }
    /* tapping the board turns towards the tap, relative to the head */
    int bx = lx - SNAKE_BX, by = ly - SNAKE_BY;
    if (bx < 0 || by < 0 || bx >= SNAKE_COLS * SNAKE_CELL || by >= SNAKE_ROWS * SNAKE_CELL) return;
    if (state == OVER) { snake_init(); return; }
    int hx = sx[0] * SNAKE_CELL + SNAKE_CELL / 2, hy = sy[0] * SNAKE_CELL + SNAKE_CELL / 2;
    int ddx = bx - hx, ddy = by - hy;
    int cur = queued ? queue[queued - 1] : dir;
    if (state == READY) turn((ddx < 0 ? -ddx : ddx) > (ddy < 0 ? -ddy : ddy) ? (ddx > 0 ? RIGHT : LEFT) : (ddy > 0 ? DOWN : UP));
    else if (cur == UP || cur == DOWN) turn(ddx > 0 ? RIGHT : LEFT);
    else turn(ddy > 0 ? DOWN : UP);
}

void snake_release(void) { pressed = -1; }

void snake_key(int c) {
    switch (c) {
    case KEY_UP: case 'w': case 'W': turn(UP); break;
    case KEY_DOWN: case 's': case 'S': turn(DOWN); break;
    case KEY_LEFT: case 'a': case 'A': turn(LEFT); break;
    case KEY_RIGHT: case 'd': case 'D': turn(RIGHT); break;
    case '\n': case ' ': if (state != RUNNING) { snake_init(); if (c == ' ') turn(RIGHT); } break;
    case 'n': case 'N': snake_init(); break;
    }
}

void snake_icon(int x, int y) {
    gfx_fill(x + 2, y + 2, 28, 28, RGB(0x1E, 0x6B, 0x34));
    gfx_rect(x + 2, y + 2, 28, 28, RGB(0x0E, 0x4D, 0x22));
    gfx_fill(x + 6, y + 20, 14, 5, RGB(0x5C, 0xE0, 0x7C));
    gfx_fill(x + 15, y + 10, 5, 12, RGB(0x5C, 0xE0, 0x7C));
    gfx_fill(x + 15, y + 8, 9, 5, RGB(0xF5, 0xD0, 0x2A));
    gfx_disc(x + 9, y + 10, 3, RGB(0xE0, 0x3C, 0x31));
}
