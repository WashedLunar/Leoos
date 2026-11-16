/* LeoOS - COM1 serial port (mirrors console output for debugging) */
#include "io.h"
#include "cpu.h"
#define COM1 0x3F8
static bool serial_ok;

void serial_init(void) {
    outb(COM1 + 1, 0x00);  /* disable interrupts */
    outb(COM1 + 3, 0x80);  /* DLAB on */
    outb(COM1 + 0, 0x03);  /* 38400 baud */
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);  /* 8N1 */
    outb(COM1 + 2, 0xC7);  /* FIFO */
    outb(COM1 + 4, 0x0B);
    serial_ok = true;
}

void serial_putc(char c) {
    if (!serial_ok) return;
    if (c == '\n') serial_putc('\r');
    if ((uint8_t)c == 0xDB) c = '#';       /* keep the serial log plain ASCII */
    for (int i = 0; i < 100000 && !(inb(COM1 + 5) & 0x20); i++) ;
    outb(COM1, (uint8_t)c);
}
