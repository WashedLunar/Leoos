/* LeoOS - Deep Dive (by Luna). Positions are in 1/16 px fixed point. */
#include "deepdive.h"
#include "gfx.h"
#include "cpu.h"
#include "string.h"

#define FP        16
#define TOP       28                        /* status bar height */
#define SURFACE   (TOP + 18)                /* above this line you're breathing */
#define SEA_W     DIVE_W
#define SEA_H     DIVE_H
#define NFISH     6
#define NJELLY    4
#define O2_MAX    1000
#define STEP      2                         /* timer ticks per game step (20 ms at 100 Hz) */
enum { READY, PLAYING, OVER };

struct thing { int x, y, vx, vy; bool alive; uint32_t col; };
static struct thing fish[NFISH], jelly[NJELLY];
static int px, py, state, o2, score, best, hurt;   /* hurt = flash frames after a sting */
static int target_x = -1, target_y, held_keys;     /* held_keys: bit per direction, decays */
static int key_dx, key_dy, key_timer;
static uint32_t last_step, frame, rng = 0x6C8E9CF5;

static uint32_t rnd(void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }
static int rrange(int lo, int hi) { return lo + (int)(rnd() % (uint32_t)(hi - lo + 1)); }

static const uint32_t fish_cols[4] = { 0xF5D02A, 0xF08A24, 0x55FF99, 0xFF77AA };

static void spawn_fish(struct thing *f) {
    bool left = rnd() & 1;
    f->x = (left ? -10 : SEA_W + 10) * FP;
    f->y = rrange(SURFACE + 20, SEA_H - 20) * FP;
    f->vx = (left ? 1 : -1) * rrange(10, 24);
    f->vy = 0; f->alive = true; f->col = fish_cols[rnd() % 4];
}

static void spawn_jelly(struct thing *j) {
    j->x = rrange(20, SEA_W - 20) * FP;
    j->y = (SEA_H + rrange(10, 120)) * FP;
    j->vx = 0; j->vy = -rrange(6, 14); j->alive = true; j->col = 0xD58CFF;
}

void dive_init(void) {
    rng ^= timer_ticks() * 2654435761u; if (!rng) rng = 1;
    px = SEA_W / 2 * FP; py = (SURFACE + 40) * FP;
    o2 = O2_MAX; score = 0; hurt = 0; state = READY; target_x = -1; key_timer = 0;
    for (int i = 0; i < NFISH; i++) { spawn_fish(&fish[i]); fish[i].x = rrange(20, SEA_W - 20) * FP; }
    for (int i = 0; i < NJELLY; i++) spawn_jelly(&jelly[i]);
    held_keys = 0;
}

static int iabs(int v) { return v < 0 ? -v : v; }

static void step(void) {
    frame++;
    /* swim: towards the held pointer, or in the last arrow-key direction */
    int dx = 0, dy = 0;
    if (target_x >= 0) {
        int tx = target_x * FP - px, ty = target_y * FP - py;
        int d = iabs(tx) + iabs(ty);
        if (d > 4 * FP) { dx = tx * 40 / d; dy = ty * 40 / d; }
    } else if (key_timer > 0) { dx = key_dx * 40; dy = key_dy * 40; key_timer--; }
    px += dx; py += dy;
    if (px < 8 * FP) px = 8 * FP;
    if (px > (SEA_W - 8) * FP) px = (SEA_W - 8) * FP;
    if (py < (TOP + 6) * FP) py = (TOP + 6) * FP;
    if (py > (SEA_H - 8) * FP) py = (SEA_H - 8) * FP;

    /* oxygen: refill at the surface, drain faster the deeper you are */
    if (py < SURFACE * FP) { o2 += 12; if (o2 > O2_MAX) o2 = O2_MAX; }
    else o2 -= 1 + (py / FP - SURFACE) / 90;
    if (hurt) hurt--;

    for (int i = 0; i < NFISH; i++) {
        struct thing *f = &fish[i];
        f->x += f->vx;
        f->y += ((int)(frame + (uint32_t)i * 7) % 40 < 20) ? 2 : -2;
        if (f->x < -20 * FP || f->x > (SEA_W + 20) * FP) spawn_fish(f);
        if (iabs(f->x - px) < 12 * FP && iabs(f->y - py) < 10 * FP) {
            score++; o2 += 40; if (o2 > O2_MAX) o2 = O2_MAX;
            spawn_fish(f);
        }
    }
    for (int i = 0; i < NJELLY; i++) {
        struct thing *j = &jelly[i];
        j->y += j->vy;
        j->x += ((int)(frame / 25 + (uint32_t)i) % 2) ? 4 : -4;
        if (j->y < (SURFACE + 10) * FP) spawn_jelly(j);
        if (!hurt && iabs(j->x - px) < 13 * FP && iabs(j->y - py) < 13 * FP) { o2 -= 200; hurt = 40; }
    }
    if (o2 <= 0) { o2 = 0; state = OVER; if (score > best) best = score; }
}

bool dive_tick(bool focused) {
    uint32_t now = timer_ticks();
    if (state != PLAYING || !focused) { last_step = now; return false; }
    bool changed = false;
    while (now - last_step >= STEP && state == PLAYING) { last_step += STEP; step(); changed = true; }
    return changed;
}

static void start(void) { if (state == READY) { state = PLAYING; last_step = timer_ticks(); } }

static void draw_fish(int x, int y, uint32_t c, bool right) {
    gfx_disc(x, y, 5, c);
    int tx = right ? x - 7 : x + 7;
    gfx_fill(tx - 2, y - 4, 4, 8, c);
    gfx_fill(right ? x + 2 : x - 3, y - 2, 2, 2, 0x000000);
}

static void draw_jelly(int x, int y) {
    gfx_disc(x, y, 8, 0xD58CFF);
    gfx_fill(x - 8, y, 17, 8, DIVE_BG + 0x000A10);
    for (int t = -6; t <= 6; t += 4) gfx_fill(x + t, y, 2, 10, 0xB070E0);
}

static void draw_diver(int x, int y) {
    gfx_fill(x - 10, y - 5, 20, 10, hurt && (hurt / 4) % 2 ? 0xFF5555 : 0xF5D02A);
    gfx_disc(x + 9, y, 5, 0xF5D02A);
    gfx_disc(x + 9, y, 3, 0x7FD8FF);
    gfx_fill(x - 14, y - 3, 4, 6, 0x909090);
}

void dive_draw(int x, int y, int w, int h, bool focused) {
    (void)w; (void)h; (void)focused;
    char buf[32], num[12];
    gfx_gradient(x, y + TOP, SEA_W, SEA_H - TOP, RGB(0x2E, 0x86, 0xDE), RGB(0x05, 0x1A, 0x30));
    gfx_fill(x, y + SURFACE, SEA_W, 1, RGB(0xBF, 0xF0, 0xFF));
    for (int i = 0; i < 6; i++) gfx_fill(x + 30 + i * 70, y + SEA_H - 10 - (i % 3) * 6, 16, 10 + (i % 3) * 6, RGB(0x2F, 0xA8, 0x4F));

    for (int i = 0; i < NJELLY; i++) draw_jelly(x + jelly[i].x / FP, y + jelly[i].y / FP);
    for (int i = 0; i < NFISH; i++) draw_fish(x + fish[i].x / FP, y + fish[i].y / FP, fish[i].col, fish[i].vx > 0);
    draw_diver(x + px / FP, y + py / FP);

    gfx_fill(x, y, SEA_W, TOP, RGB(0xD4, 0xE6, 0xD8));
    strcpy(buf, "Fish: "); utoa((uint32_t)score, num, 10); strcpy(buf + strlen(buf), num);
    strcpy(buf + strlen(buf), "  Best: "); utoa((uint32_t)best, num, 10); strcpy(buf + strlen(buf), num);
    gfx_text(x + 8, y + 6, buf, RGB(0x10, 0x30, 0x18), TRANSPARENT, 1);
    gfx_text(x + SEA_W - 176, y + 6, "O2", RGB(0x10, 0x30, 0x18), TRANSPARENT, 1);
    gfx_rect(x + SEA_W - 156, y + 7, 146, 14, RGB(0x10, 0x30, 0x18));
    uint32_t oc = o2 > 300 ? RGB(0x2E, 0x86, 0xDE) : RGB(0xD9, 0x45, 0x3B);
    gfx_fill(x + SEA_W - 155, y + 8, 144 * o2 / O2_MAX, 12, oc);

    const char *msg = state == READY ? "Tap or press an arrow to dive" : state == OVER ? "Out of oxygen!" : NULL;
    if (msg) {
        gfx_shade(x + 40, y + SEA_H / 2 - 30, SEA_W - 80, 60, 150);
        gfx_text(x + (SEA_W - (int)strlen(msg) * FONT_W) / 2, y + SEA_H / 2 - 20, msg, 0xFFFFFF, TRANSPARENT, 1);
        const char *sub = state == OVER ? "Tap to dive again" : "Surface for air, dodge jellyfish";
        gfx_text(x + (SEA_W - (int)strlen(sub) * FONT_W) / 2, y + SEA_H / 2 + 2, sub, RGB(0xBF, 0xF0, 0xFF), TRANSPARENT, 1);
    }
}

void dive_click(int lx, int ly) {
    if (state == OVER) { dive_init(); return; }
    start();
    if (ly >= TOP) { target_x = lx; target_y = ly; }
}
void dive_drag(int lx, int ly) { if (target_x >= 0) { target_x = lx; target_y = ly; } }
void dive_release(void) { target_x = -1; }

void dive_key(int c) {
    int dx = 0, dy = 0;
    if (c == KEY_UP || c == 'w' || c == 'W') dy = -1;
    else if (c == KEY_DOWN || c == 's' || c == 'S') dy = 1;
    else if (c == KEY_LEFT || c == 'a' || c == 'A') dx = -1;
    else if (c == KEY_RIGHT || c == 'd' || c == 'D') dx = 1;
    else if (c == 'n' || c == 'N' || c == '\n') { if (state != PLAYING) { dive_init(); start(); } return; }
    else return;
    if (state == OVER) dive_init();
    start();
    key_dx = dx; key_dy = dy; key_timer = 12;          /* keep swimming ~0.25 s per press (key repeat keeps it going) */
}

void dive_icon(int x, int y) {
    gfx_gradient(x + 1, y + 1, 30, 30, RGB(0x2E, 0x86, 0xDE), RGB(0x05, 0x1A, 0x30));
    gfx_fill(x + 6, y + 13, 14, 7, 0xF5D02A);
    gfx_disc(x + 21, y + 16, 4, 0xF5D02A);
    gfx_disc(x + 21, y + 16, 2, 0x7FD8FF);
    gfx_disc(x + 10, y + 26, 3, 0xF08A24);
}
