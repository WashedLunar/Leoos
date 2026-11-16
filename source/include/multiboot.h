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
    uint32_t apm_table;
    uint32_t vbe_control_info, vbe_mode_info;
    uint16_t vbe_mode, vbe_interface_seg, vbe_interface_off, vbe_interface_len;
    uint64_t framebuffer_addr;          /* valid when flags bit 12 is set */
    uint32_t framebuffer_pitch, framebuffer_width, framebuffer_height;
    uint8_t  framebuffer_bpp, framebuffer_type;   /* type 1 = RGB, 2 = EGA text */
    uint8_t  color_info[6];
} __attribute__((packed));
#define MB_INFO_CMDLINE     (1 << 2)
#define MB_INFO_MMAP        (1 << 6)
#define MB_INFO_FRAMEBUFFER (1 << 12)
struct multiboot_mmap_entry {
    uint32_t size;
    uint64_t addr;
    uint64_t len;
    uint32_t type;
} __attribute__((packed));
#endif
