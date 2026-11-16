#ifndef LEO_IO_H
#define LEO_IO_H
#include "types.h"
static inline void outb(uint16_t port, uint8_t val) { __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port)); }
static inline void outw(uint16_t port, uint16_t val) { __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port)); }
static inline uint8_t inb(uint16_t port) { uint8_t r; __asm__ volatile ("inb %1, %0" : "=a"(r) : "Nd"(port)); return r; }
static inline void io_wait(void) { outb(0x80, 0); }
static inline void cpu_sti(void) { __asm__ volatile ("sti"); }
static inline void cpu_cli(void) { __asm__ volatile ("cli"); }
static inline void cpu_hlt(void) { __asm__ volatile ("hlt"); }
#endif
