/* LeoOS - 8253/8254 PIT driver: periodic IRQ0 tick and uptime status bar */
#include "cpu.h"
#include "io.h"
#include "vga.h"
#include "string.h"

static volatile uint32_t ticks;
static uint32_t hz = 100;

static void two_digits(char *p, uint32_t v) { p[0] = (char)('0' + (v / 10) % 10); p[1] = (char)('0' + v % 10); }

static void refresh_status(void) {
    uint32_t s = ticks / hz;
    char bar[81];
    strcpy(bar, " LeoOS 0.3.1                                     uptime 00:00:00    kbd: UK ");
    two_digits(bar + 56, (s / 3600) % 100);
    two_digits(bar + 59, (s / 60) % 60);
    two_digits(bar + 62, s % 60);
    if (!keyboard_is_uk()) { bar[74] = 'U'; bar[75] = 'S'; }
    vga_status(bar);
}

static void timer_handler(struct regs *r) {
    (void)r;
    ticks++;
    if (ticks % hz == 0) refresh_status();
}

void timer_init(uint32_t freq) {
    hz = freq;
    uint32_t divisor = 1193182 / freq;
    outb(0x43, 0x36);                       /* channel 0, lo/hi, mode 3 */
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)(divisor >> 8));
    irq_install_handler(0, timer_handler);
    refresh_status();
}

uint32_t timer_ticks(void) { return ticks; }
uint32_t timer_hz(void) { return hz; }

void timer_sleep(uint32_t ms) {
    uint32_t end = ticks + (ms * hz) / 1000 + 1;
    while (ticks < end) cpu_hlt();
}
