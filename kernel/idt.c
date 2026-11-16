/* LeoOS - IDT, CPU exception handlers, 8259 PIC remapping and IRQ dispatch */
#include "cpu.h"
#include "io.h"
#include "vga.h"

struct idt_entry {
    uint16_t base_low, sel;
    uint8_t  zero, flags;
    uint16_t base_high;
} __attribute__((packed));

struct idt_ptr { uint16_t limit; uint32_t base; } __attribute__((packed));

static struct idt_entry idt[256];
static struct idt_ptr ip;
static irq_handler_t irq_handlers[16];
extern void idt_load(uint32_t);

#define DECL(n) extern void n(void);
DECL(isr0) DECL(isr1) DECL(isr2) DECL(isr3) DECL(isr4) DECL(isr5) DECL(isr6) DECL(isr7)
DECL(isr8) DECL(isr9) DECL(isr10) DECL(isr11) DECL(isr12) DECL(isr13) DECL(isr14) DECL(isr15)
DECL(isr16) DECL(isr17) DECL(isr18) DECL(isr19) DECL(isr20) DECL(isr21) DECL(isr22) DECL(isr23)
DECL(isr24) DECL(isr25) DECL(isr26) DECL(isr27) DECL(isr28) DECL(isr29) DECL(isr30) DECL(isr31)
DECL(irq0) DECL(irq1) DECL(irq2) DECL(irq3) DECL(irq4) DECL(irq5) DECL(irq6) DECL(irq7)
DECL(irq8) DECL(irq9) DECL(irq10) DECL(irq11) DECL(irq12) DECL(irq13) DECL(irq14) DECL(irq15)

static void (*const stubs[48])(void) = {
    isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7, isr8, isr9, isr10, isr11, isr12, isr13, isr14, isr15,
    isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23, isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31,
    irq0, irq1, irq2, irq3, irq4, irq5, irq6, irq7, irq8, irq9, irq10, irq11, irq12, irq13, irq14, irq15,
};

static const char *exception_names[32] = {
    "Divide by zero", "Debug", "Non-maskable interrupt", "Breakpoint", "Overflow",
    "Bound range exceeded", "Invalid opcode", "Device not available", "Double fault",
    "Coprocessor segment overrun", "Invalid TSS", "Segment not present", "Stack-segment fault",
    "General protection fault", "Page fault", "Reserved", "x87 floating-point exception",
    "Alignment check", "Machine check", "SIMD floating-point exception", "Virtualisation exception",
    "Control protection exception", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Hypervisor injection", "VMM communication", "Security exception", "Reserved",
};

static void idt_set(int n, uint32_t base) {
    idt[n].base_low  = base & 0xFFFF;
    idt[n].base_high = (base >> 16) & 0xFFFF;
    idt[n].sel       = 0x08;
    idt[n].zero      = 0;
    idt[n].flags     = 0x8E;   /* present, ring 0, 32-bit interrupt gate */
}

/* Move the master PIC to vectors 32-39 and the slave to 40-47 so hardware
 * IRQs no longer collide with CPU exceptions. */
static void pic_remap(void) {
    outb(0x20, 0x11); io_wait(); outb(0xA0, 0x11); io_wait();
    outb(0x21, 0x20); io_wait(); outb(0xA1, 0x28); io_wait();
    outb(0x21, 0x04); io_wait(); outb(0xA1, 0x02); io_wait();
    outb(0x21, 0x01); io_wait(); outb(0xA1, 0x01); io_wait();
    outb(0x21, 0xFC);          /* unmask IRQ0 (timer) and IRQ1 (keyboard) only */
    outb(0xA1, 0xFF);
}

void irq_install_handler(int irq, irq_handler_t h) { irq_handlers[irq] = h; }

void interrupt_dispatch(struct regs *r) {
    if (r->int_no < 32) {
        vga_set_colour(WHITE, RED);
        kprintf("\n*** LeoOS kernel panic: %s (int %u, err %x) at eip %x ***\n",
                exception_names[r->int_no], r->int_no, r->err_code, r->eip);
        vga_puts("System halted.");
        for (;;) { cpu_cli(); cpu_hlt(); }
    }
    if (r->int_no < 48) {
        int irq = r->int_no - 32;
        if (irq_handlers[irq]) irq_handlers[irq](r);
        if (irq >= 8) outb(0xA0, 0x20);   /* EOI to slave */
        outb(0x20, 0x20);                 /* EOI to master */
    }
}

void idt_init(void) {
    ip.limit = sizeof(idt) - 1;
    ip.base  = (uint32_t)&idt;
    for (int i = 0; i < 48; i++) idt_set(i, (uint32_t)stubs[i]);
    pic_remap();
    idt_load((uint32_t)&ip);
}
