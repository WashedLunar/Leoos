/* LeoOS - linear framebuffer graphics with a double-buffered back buffer */
#include "gfx.h"
#include "string.h"

#define MAX_W 1280
#define MAX_H 1024

extern const uint8_t font8x16[256][16];

int gfx_w, gfx_h;
static uint8_t *fb;
static uint32_t pitch;
static uint32_t back[MAX_W * MAX_H];        /* everything is drawn here first...      */
static uint32_t wallpaper[MAX_W * MAX_H];   /* ...and the desktop is cached here       */
static int cx0, cy0, cx1, cy1;              /* clip rectangle [cx0, cx1) x [cy0, cy1)  */

static inline void copy32(void *d, const void *s, uint32_t n) {
    __asm__ volatile ("rep movsl" : "+D"(d), "+S"(s), "+c"(n) : : "memory");
}
static inline void fill32(uint32_t *d, uint32_t v, uint32_t n) {
    __asm__ volatile ("rep stosl" : "+D"(d), "+c"(n) : "a"(v) : "memory");
}

bool gfx_init(struct multiboot_info *mbi) {
    if (!(mbi->flags & MB_INFO_FRAMEBUFFER)) return false;
    if (mbi->framebuffer_type != 1 || mbi->framebuffer_bpp != 32) return false;
    if (mbi->framebuffer_width > MAX_W || mbi->framebuffer_height > MAX_H) return false;
    fb = (uint8_t *)(uint32_t)mbi->framebuffer_addr;
    pitch = mbi->framebuffer_pitch;
    gfx_w = (int)mbi->framebuffer_width;
    gfx_h = (int)mbi->framebuffer_height;
    gfx_reset_clip();
    return true;
}

void gfx_set_clip(int x, int y, int w, int h) {
    cx0 = x < 0 ? 0 : x;            cy0 = y < 0 ? 0 : y;
    cx1 = x + w > gfx_w ? gfx_w : x + w;
    cy1 = y + h > gfx_h ? gfx_h : y + h;
}
void gfx_reset_clip(void) { cx0 = cy0 = 0; cx1 = gfx_w; cy1 = gfx_h; }

void gfx_pixel(int x, int y, uint32_t c) {
    if (x >= cx0 && x < cx1 && y >= cy0 && y < cy1) back[y * gfx_w + x] = c;
}

void gfx_fill(int x, int y, int w, int h, uint32_t c) {
    int x0 = x < cx0 ? cx0 : x, y0 = y < cy0 ? cy0 : y;
    int x1 = x + w > cx1 ? cx1 : x + w, y1 = y + h > cy1 ? cy1 : y + h;
    if (x0 >= x1 || y0 >= y1) return;
    for (int yy = y0; yy < y1; yy++) fill32(&back[yy * gfx_w + x0], c, (uint32_t)(x1 - x0));
}

void gfx_rect(int x, int y, int w, int h, uint32_t c) {
    gfx_fill(x, y, w, 1, c); gfx_fill(x, y + h - 1, w, 1, c);
    gfx_fill(x, y, 1, h, c); gfx_fill(x + w - 1, y, 1, h, c);
}

uint32_t gfx_blend(uint32_t a, uint32_t b, int t) {      /* t = 0..256 from a to b */
    uint32_t r = (((a >> 16) & 255) * (256 - t) + ((b >> 16) & 255) * t) >> 8;
    uint32_t g = (((a >> 8) & 255) * (256 - t) + ((b >> 8) & 255) * t) >> 8;
    uint32_t bl = ((a & 255) * (256 - t) + (b & 255) * t) >> 8;
    return (r << 16) | (g << 8) | bl;
}

void gfx_gradient(int x, int y, int w, int h, uint32_t top, uint32_t bottom) {
    for (int i = 0; i < h; i++) gfx_fill(x, y + i, w, 1, gfx_blend(top, bottom, h > 1 ? i * 256 / (h - 1) : 0));
}

void gfx_bevel(int x, int y, int w, int h, uint32_t face, bool pressed) {
    uint32_t light = gfx_blend(face, 0xFFFFFF, 140), dark = gfx_blend(face, 0x000000, 110);
    gfx_fill(x, y, w, h, pressed ? gfx_blend(face, 0x000000, 40) : face);
    gfx_fill(x, y, w, 1, pressed ? dark : light);
    gfx_fill(x, y, 1, h, pressed ? dark : light);
    gfx_fill(x, y + h - 1, w, 1, pressed ? light : dark);
    gfx_fill(x + w - 1, y, 1, h, pressed ? light : dark);
}

void gfx_char(int x, int y, uint8_t ch, uint32_t fg, uint32_t bg, int scale) {
    const uint8_t *g = font8x16[ch];
    if (scale <= 1) {
        if (x >= cx1 || y >= cy1 || x + FONT_W <= cx0 || y + FONT_H <= cy0) return;
        for (int r = 0; r < FONT_H; r++) {
            int py = y + r;
            if (py < cy0 || py >= cy1) continue;
            uint8_t bits = g[r];
            uint32_t *row = &back[py * gfx_w];
            for (int c = 0; c < FONT_W; c++) {
                int px = x + c;
                if (px < cx0 || px >= cx1) continue;
                if (bits & (0x80 >> c)) row[px] = fg;
                else if (bg != TRANSPARENT) row[px] = bg;
            }
        }
        return;
    }
    for (int r = 0; r < FONT_H; r++)
        for (int c = 0; c < FONT_W; c++) {
            bool on = g[r] & (0x80 >> c);
            if (on) gfx_fill(x + c * scale, y + r * scale, scale, scale, fg);
            else if (bg != TRANSPARENT) gfx_fill(x + c * scale, y + r * scale, scale, scale, bg);
        }
}

void gfx_text(int x, int y, const char *s, uint32_t fg, uint32_t bg, int scale) {
    if (scale < 1) scale = 1;
    for (int sx = x; *s; s++) {
        if (*s == '\n') { y += FONT_H * scale; sx = x; continue; }
        gfx_char(sx, y, (uint8_t)*s, fg, bg, scale);
        sx += FONT_W * scale;
    }
}

void gfx_disc(int cx, int cy, int r, uint32_t c) {
    for (int dy = -r; dy <= r; dy++)
        for (int dx = -r; dx <= r; dx++)
            if (dx * dx + dy * dy <= r * r + r) gfx_pixel(cx + dx, cy + dy, c);
}

void gfx_line(int x0, int y0, int x1, int y1, int size, uint32_t c) {   /* Bresenham */
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        if (size <= 1) gfx_pixel(x0, y0, c); else gfx_disc(x0, y0, size / 2, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void gfx_blit(int x, int y, int w, int h, const uint32_t *src, int stride) {
    int x0 = x < cx0 ? cx0 : x, y0 = y < cy0 ? cy0 : y;
    int x1 = x + w > cx1 ? cx1 : x + w, y1 = y + h > cy1 ? cy1 : y + h;
    if (x0 >= x1 || y0 >= y1) return;
    for (int yy = y0; yy < y1; yy++)
        copy32(&back[yy * gfx_w + x0], &src[(yy - y) * stride + (x0 - x)], (uint32_t)(x1 - x0));
}

void gfx_shade(int x, int y, int w, int h, int t) {      /* darken a rect (window shadows) */
    int x0 = x < cx0 ? cx0 : x, y0 = y < cy0 ? cy0 : y;
    int x1 = x + w > cx1 ? cx1 : x + w, y1 = y + h > cy1 ? cy1 : y + h;
    for (int yy = y0; yy < y1; yy++)
        for (int xx = x0; xx < x1; xx++) back[yy * gfx_w + xx] = gfx_blend(back[yy * gfx_w + xx], 0, t);
}

void gfx_save_background(void)    { copy32(wallpaper, back, (uint32_t)(gfx_w * gfx_h)); }
void gfx_restore_background(void) { copy32(back, wallpaper, (uint32_t)(gfx_w * gfx_h)); }

void gfx_flip(void) {
    for (int y = 0; y < gfx_h; y++) copy32(fb + y * pitch, &back[y * gfx_w], (uint32_t)gfx_w);
}
