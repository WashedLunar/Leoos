/* LeoOS - kernel entry point */
#include "vga.h"
#include "cpu.h"
#include "io.h"
#include "multiboot.h"
#include "shell.h"

#include "version.h"
#include "gfx.h"
#include "gui.h"
#include "string.h"

static void ok(const char *what) {
    vga_set_colour(LIGHT_GREY, BLACK); vga_puts(" [");
    vga_set_colour(LIGHT_GREEN, BLACK); vga_puts(" OK ");
    vga_set_colour(LIGHT_GREY, BLACK); vga_puts("] ");
    vga_puts(what); vga_putc('\n');
}

/* Big "LeoOS" logo drawn with the VGA font's solid block character (0xDB). */
static void banner(void) {
    static const char *glyphs[5][5] = {
        { "#    ", "#    ", "#    ", "#    ", "#####" },   /* L */
        { "     ", " ### ", "#####", "#    ", " ### " },   /* e */
        { "     ", " ### ", "#   #", "#   #", " ### " },   /* o */
        { " ### ", "#   #", "#   #", "#   #", " ### " },   /* O */
        { " ####", "#    ", " ### ", "    #", "#### " },   /* S */
    };
    static const uint8_t colours[5] = { LIGHT_CYAN, LIGHT_CYAN, LIGHT_CYAN, YELLOW, YELLOW };
    vga_putc('\n');
    for (int row = 0; row < 5; row++) {
        vga_puts("           ");
        for (int g = 0; g < 5; g++) {
            vga_set_colour(colours[g], BLACK);
            for (const char *p = glyphs[g][row]; *p; p++) {
                char c = (*p == '#') ? (char)0xDB : ' ';
                vga_putc(c); vga_putc(c);              /* double width = square pixels */
            }
            vga_puts("  ");
        }
        vga_putc('\n');
    }
    vga_set_colour(WHITE, BLACK);
    vga_puts("\n          Welcome to LeoOS version " LEOOS_VERSION " - a tiny 32-bit hobby OS\n\n");
}

static bool cmdline_has(struct multiboot_info *mbi, const char *word) {
    if (!(mbi->flags & MB_INFO_CMDLINE) || !mbi->cmdline) return false;
    const char *c = (const char *)mbi->cmdline;
    size_t n = strlen(word);
    for (; *c; c++) if (!strncmp(c, word, n) && (c[n] == 0 || c[n] == ' ')) return true;
    return false;
}

static void start_cpu_and_drivers(bool gui) {
    gdt_init();
    idt_init();
    timer_init(100);
    keyboard_init();
    if (gui) mouse_init();
}

void kmain(uint32_t magic, struct multiboot_info *mbi) {
    serial_init();
    bool mb_valid = (magic == MULTIBOOT_BOOTLOADER_MAGIC);

    /* Desktop mode: GRUB gave us a 32bpp linear framebuffer and we weren't asked for text. */
    if (mb_valid && !cmdline_has(mbi, "text") && gfx_init(mbi)) {
        apps_init();                         /* points the console at the Terminal window */
        for (const char *m = "LeoOS " LEOOS_VERSION ": starting the desktop\n"; *m; m++) serial_putc(*m);
        start_cpu_and_drivers(true);
        shell_init(mbi, mb_valid);
        cpu_sti();
        gui_run(mbi);
    }

    /* Classic text mode (LeoOS 0.1 behaviour) */
    vga_init();
    banner();
    if (mb_valid) ok("Booted by a Multiboot loader (GRUB)");
    else { vga_set_colour(LIGHT_RED, BLACK); vga_puts(" [WARN] Not booted via Multiboot\n"); }
    start_cpu_and_drivers(false);
    ok("GDT loaded (flat code/data segments)");
    ok("IDT loaded, PIC remapped to vectors 32-47");
    ok("PIT timer running at 100 Hz");
    ok("PS/2 keyboard driver ready (UK layout)");
    cpu_sti();           ok("Interrupts enabled");

    vga_set_colour(LIGHT_GREY, BLACK);
    vga_puts("\n Type 'help' to see what LeoOS can do.\n\n");
    shell_run(mbi, mb_valid);
}
