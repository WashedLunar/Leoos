#ifndef LEO_CPU_H
#define LEO_CPU_H
#include "types.h"
struct regs {
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags, useresp, ss;
};
typedef void (*irq_handler_t)(struct regs *r);
void gdt_init(void);
void idt_init(void);
void irq_install_handler(int irq, irq_handler_t h);
void timer_init(uint32_t hz);
uint32_t timer_ticks(void);
uint32_t timer_hz(void);
void timer_sleep(uint32_t ms);
void keyboard_init(void);
int  keyboard_getchar(void);        /* blocks; returns ASCII or KEY_* */
void keyboard_set_layout(int uk);
int  keyboard_is_uk(void);
void serial_init(void);
void serial_putc(char c);
#define KEY_UP    0x100
#define KEY_DOWN  0x101
#define KEY_LEFT  0x102
#define KEY_RIGHT 0x103
#endif
