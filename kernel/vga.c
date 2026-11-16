/* LeoOS - VGA text mode driver: colours, scrolling, hardware cursor, status bar */
#include <stdarg.h>
#include "vga.h"
#include "io.h"
#include "cpu.h"
#include "string.h"

#define VGA_MEM   ((volatile uint16_t *)0xB8000)
#define TOP_ROW   1                      /* row 0 is reserved for the status bar */
#define TAB_WIDTH 4

static int row = TOP_ROW, col = 0;
static uint8_t fg = LIGHT_GREY, bg = BLACK;

static inline uint16_t cell(char c, uint8_t f, uint8_t b) {
    return (uint16_t)(uint8_t)c | (uint16_t)((b << 4) | (f & 0x0F)) << 8;
}

static void update_cursor(void) {
    uint16_t pos = (uint16_t)(row * VGA_WIDTH + col);
    outb(0x3D4, 0x0F); outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E); outb(0x3D5, (uint8_t)(pos >> 8));
}

static void enable_cursor(uint8_t start, uint8_t end) {
    outb(0x3D4, 0x0A); outb(0x3D5, (inb(0x3D5) & 0xC0) | start);
    outb(0x3D4, 0x0B); outb(0x3D5, (inb(0x3D5) & 0xE0) | end);
}

static void scroll(void) {
    for (int y = TOP_ROW; y < VGA_HEIGHT - 1; y++)
        for (int x = 0; x < VGA_WIDTH; x++)
            VGA_MEM[y * VGA_WIDTH + x] = VGA_MEM[(y + 1) * VGA_WIDTH + x];
    for (int x = 0; x < VGA_WIDTH; x++)
        VGA_MEM[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = cell(' ', fg, bg);
    row = VGA_HEIGHT - 1;
}

void vga_clear(void) {
    for (int y = TOP_ROW; y < VGA_HEIGHT; y++)
        for (int x = 0; x < VGA_WIDTH; x++)
            VGA_MEM[y * VGA_WIDTH + x] = cell(' ', fg, bg);
    row = TOP_ROW; col = 0;
    update_cursor();
}

void vga_init(void) {
    enable_cursor(13, 15);
    vga_status("");
    vga_clear();
}

void vga_set_colour(uint8_t f, uint8_t b) { fg = f & 0x0F; bg = b & 0x0F; }
uint8_t vga_get_fg(void) { return fg; }
uint8_t vga_get_bg(void) { return bg; }

void vga_putc(char c) {
    serial_putc(c);
    switch (c) {
    case '\n': col = 0; row++; break;
    case '\r': col = 0; break;
    case '\t': col = (col + TAB_WIDTH) & ~(TAB_WIDTH - 1); break;
    case '\b':
        if (col > 0) col--;
        else if (row > TOP_ROW) { row--; col = VGA_WIDTH - 1; }
        VGA_MEM[row * VGA_WIDTH + col] = cell(' ', fg, bg);
        break;
    default:
        VGA_MEM[row * VGA_WIDTH + col] = cell(c, fg, bg);
        col++;
    }
    if (col >= VGA_WIDTH) { col = 0; row++; }
    if (row >= VGA_HEIGHT) scroll();
    update_cursor();
}

void vga_puts(const char *s) { while (*s) vga_putc(*s++); }
void vga_put_dec(int32_t v) { char b[16]; itoa(v, b); vga_puts(b); }
void vga_put_udec(uint32_t v) { char b[16]; utoa(v, b, 10); vga_puts(b); }
void vga_put_hex(uint32_t v) { char b[16]; utoa(v, b, 16); vga_puts("0x"); vga_puts(b); }

/* The status bar lives on row 0 and is written directly, so it never
 * disturbs the cursor or the scrolling console beneath it. */
void vga_status(const char *text) {
    const uint8_t sf = BLACK, sb = LIGHT_CYAN;
    int x = 0;
    for (; text[x] && x < VGA_WIDTH; x++) VGA_MEM[x] = cell(text[x], sf, sb);
    for (; x < VGA_WIDTH; x++) VGA_MEM[x] = cell(' ', sf, sb);
}

void kprintf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    for (; *fmt; fmt++) {
        if (*fmt != '%') { vga_putc(*fmt); continue; }
        switch (*++fmt) {
        case 's': { const char *s = va_arg(ap, const char *); vga_puts(s ? s : "(null)"); break; }
        case 'd': vga_put_dec(va_arg(ap, int32_t)); break;
        case 'u': vga_put_udec(va_arg(ap, uint32_t)); break;
        case 'x': vga_put_hex(va_arg(ap, uint32_t)); break;
        case 'c': vga_putc((char)va_arg(ap, int)); break;
        case '%': vga_putc('%'); break;
        case 0: va_end(ap); return;
        default: vga_putc('%'); vga_putc(*fmt);
        }
    }
    va_end(ap);
}
