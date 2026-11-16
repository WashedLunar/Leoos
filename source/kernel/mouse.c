/* LeoOS - PS/2 mouse driver (IRQ12, standard 3-byte packets) */
#include "cpu.h"
#include "io.h"

#define PKT_BUF 256
struct packet { int16_t dx, dy; uint8_t buttons; uint32_t seq; };
extern volatile uint32_t input_seq;
static volatile struct packet pkts[PKT_BUF];
static volatile uint32_t head, tail;
static uint8_t bytes[3];
static int cycle;

static void wait_write(void) { for (int i = 0; i < 100000 && (inb(0x64) & 2); i++) ; }
static void wait_read(void)  { for (int i = 0; i < 100000 && !(inb(0x64) & 1); i++) ; }

static void ctrl_cmd(uint8_t c) { wait_write(); outb(0x64, c); }

static uint8_t mouse_write(uint8_t v) {         /* send a byte to the mouse, return its ACK */
    ctrl_cmd(0xD4);
    wait_write(); outb(0x60, v);
    wait_read();  return inb(0x60);
}

static void mouse_handler(struct regs *r) {
    (void)r;
    uint8_t status = inb(0x64);
    if (!(status & 0x01) || !(status & 0x20)) return;   /* not mouse data */
    uint8_t b = inb(0x60);
    if (cycle == 0 && !(b & 0x08)) return;     /* byte 0 always has bit 3 set: resync */
    bytes[cycle++] = b;
    if (cycle < 3) return;
    cycle = 0;
    if (bytes[0] & 0xC0) return;               /* overflow: drop the packet */
    int dx = bytes[1] - ((bytes[0] << 4) & 0x100);   /* 9-bit two's complement */
    int dy = bytes[2] - ((bytes[0] << 3) & 0x100);
    uint32_t next = (head + 1) % PKT_BUF;
    if (next == tail) return;
    pkts[head].dx = (int16_t)dx;
    pkts[head].dy = (int16_t)-dy;              /* PS/2 y grows upwards; screen y grows down */
    pkts[head].buttons = bytes[0] & 0x07;
    pkts[head].seq = ++input_seq;
    head = next;
}

bool mouse_poll(int *dx, int *dy, int *buttons) {
    if (head == tail) return false;
    *dx = pkts[tail].dx; *dy = pkts[tail].dy; *buttons = pkts[tail].buttons;
    tail = (tail + 1) % PKT_BUF;
    return true;
}

uint32_t mouse_next_seq(void) { return head == tail ? 0xFFFFFFFFu : pkts[tail].seq; }

void mouse_init(void) {
    ctrl_cmd(0xA8);                            /* enable the auxiliary (mouse) port */
    ctrl_cmd(0x20);                            /* read controller config byte */
    wait_read();
    uint8_t cfg = inb(0x60);
    cfg |= 0x02;                               /* enable IRQ12 */
    cfg &= ~0x20;                              /* enable mouse clock */
    ctrl_cmd(0x60); wait_write(); outb(0x60, cfg);
    mouse_write(0xF6);                         /* defaults */
    mouse_write(0xF4);                         /* start streaming packets */
    while (inb(0x64) & 1) inb(0x60);           /* flush */
    irq_install_handler(12, mouse_handler);
    outb(0x21, inb(0x21) & ~0x04);             /* unmask IRQ2 (cascade to slave PIC) */
    outb(0xA1, inb(0xA1) & ~0x10);             /* unmask IRQ12 */
}
