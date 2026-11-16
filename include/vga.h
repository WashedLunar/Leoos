#ifndef LEO_VGA_H
#define LEO_VGA_H
#include "types.h"
enum vga_colour {
    BLACK = 0, BLUE, GREEN, CYAN, RED, MAGENTA, BROWN, LIGHT_GREY,
    DARK_GREY, LIGHT_BLUE, LIGHT_GREEN, LIGHT_CYAN, LIGHT_RED, LIGHT_MAGENTA, YELLOW, WHITE
};
#define VGA_WIDTH  80
#define VGA_HEIGHT 25
void vga_init(void);
void vga_clear(void);
void vga_set_colour(uint8_t fg, uint8_t bg);
uint8_t vga_get_fg(void);
uint8_t vga_get_bg(void);
void vga_putc(char c);
void vga_puts(const char *s);
void vga_put_dec(int32_t v);
void vga_put_udec(uint32_t v);
void vga_put_hex(uint32_t v);
void vga_status(const char *text);   /* top status bar */
void kprintf(const char *fmt, ...);  /* %s %d %u %x %c %% */
#endif
