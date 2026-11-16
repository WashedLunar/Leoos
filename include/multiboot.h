#ifndef LEO_MULTIBOOT_H
#define LEO_MULTIBOOT_H
#include "types.h"
#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002
struct multiboot_info {
    uint32_t flags, mem_lower, mem_upper, boot_device, cmdline;
    uint32_t mods_count, mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length, mmap_addr;
    uint32_t drives_length, drives_addr, config_table, boot_loader_name;
} __attribute__((packed));
struct multiboot_mmap_entry {
    uint32_t size;
    uint64_t addr;
    uint64_t len;
    uint32_t type;
} __attribute__((packed));
#endif
