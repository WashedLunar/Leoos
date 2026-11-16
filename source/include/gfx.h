#ifndef LEO_GFX_H
#define LEO_GFX_H
#include "types.h"
#include "multiboot.h"

#define RGB(r, g, b) ((uint32_t)(((r) << 16) | ((g) << 8) | (b)))
#define TRANSPARENT  0xFF000000u
#define FONT_W 8
#define FONT_H 16

extern int gfx_w, gfx_h;

bool gfx_init(struct multiboot_info *mbi);        /* false if no usable 32bpp framebuffer */
void gfx_set_clip(int x, int y, int w, int h);    /* all drawing is clipped to this rect */
void gfx_reset_clip(void);
void gfx_pixel(int x, int y, uint32_t c);
void gfx_fill(int x, int y, int w, int h, uint32_t c);
void gfx_rect(int x, int y, int w, int h, uint32_t c);     /* 1px outline */
void gfx_gradient(int x, int y, int w, int h, uint32_t top, uint32_t bottom);
void gfx_bevel(int x, int y, int w, int h, uint32_t face, bool pressed);
void gfx_char(int x, int y, uint8_t ch, uint32_t fg, uint32_t bg, int scale);
void gfx_text(int x, int y, const char *s, uint32_t fg, uint32_t bg, int scale);
void gfx_line(int x0, int y0, int x1, int y1, int size, uint32_t c);
void gfx_disc(int cx, int cy, int r, uint32_t c);
void gfx_blit(int x, int y, int w, int h, const uint32_t *src, int stride);
void gfx_shade(int x, int y, int w, int h, int t256);    /* darken (for shadows) */
void gfx_save_background(void);                   /* snapshot back buffer */
void gfx_restore_background(void);                /* restore that snapshot (fast) */
void gfx_flip(void);                              /* copy back buffer to the screen */
uint32_t gfx_blend(uint32_t a, uint32_t b, int t256);
#endif
