/* LeoOS - interactive command shell */
#include "shell.h"
#include "vga.h"
#include "cpu.h"
#include "io.h"
#include "string.h"
#include "version.h"

#define LINE_MAX  128
#define HIST_MAX  16
#define MAX_ARGS  16

static struct multiboot_info *mb;
static bool mb_ok;
static char history[HIST_MAX][LINE_MAX];
static int hist_count;

/* ---------------------------------------------------------------- helpers */
static const char *colour_names[16] = {
    "black", "blue", "green", "cyan", "red", "magenta", "brown", "lightgrey",
    "darkgrey", "lightblue", "lightgreen", "lightcyan", "lightred", "pink", "yellow", "white",
};

static int colour_from_name(const char *s) {
    for (int i = 0; i < 16; i++) if (!strcmp(s, colour_names[i])) return i;
    if (!strcmp(s, "grey") || !strcmp(s, "gray") || !strcmp(s, "lightgray")) return LIGHT_GREY;
    if (!strcmp(s, "darkgray")) return DARK_GREY;
    if (!strcmp(s, "orange")) return BROWN;
    if (!strcmp(s, "purple") || !strcmp(s, "lightmagenta")) return LIGHT_MAGENTA;
    return -1;
}

static void prompt(void) {
    uint8_t f = vga_get_fg(), b = vga_get_bg();
    vga_set_colour(LIGHT_GREEN, b); vga_puts("leo");
    vga_set_colour(WHITE, b);       vga_puts("@");
    vga_set_colour(LIGHT_CYAN, b);  vga_puts("leoos");
    vga_set_colour(WHITE, b);       vga_puts(":~$ ");
    vga_set_colour(f, b);
}

static void erase_line(int len) { while (len--) vga_putc('\b'); }

/* The line editor is event driven: shell_key() is fed one key at a time, either
 * from a blocking loop (text mode) or from the GUI's Terminal window. */
static char line[LINE_MAX];
static int len, hist_pos;
static void execute(char *cmdline);

void shell_key(int c) {
    if (c == '\n') {
        vga_putc('\n'); line[len] = 0;
        if (len) execute(line);
        len = 0; hist_pos = hist_count;
        prompt();
        return;
    }
    if (c == '\b') { if (len) { len--; vga_putc('\b'); } return; }
    if (c == 0x03) { vga_puts("^C\n"); len = 0; hist_pos = hist_count; prompt(); return; }
    if (c == 0x0C) { vga_clear(); prompt(); line[len] = 0; vga_puts(line); return; }
    if (c == KEY_UP || c == KEY_DOWN) {
        if (c == KEY_UP && hist_pos > 0 && hist_pos > hist_count - HIST_MAX) hist_pos--;
        else if (c == KEY_DOWN && hist_pos < hist_count) hist_pos++;
        else return;
        erase_line(len);
        if (hist_pos < hist_count) strcpy(line, history[hist_pos % HIST_MAX]);
        else line[0] = 0;
        len = (int)strlen(line);
        vga_puts(line);
        return;
    }
    if (c < 0 || c >= 0x100 || c == '\t' || c == 27) return;
    if (len < LINE_MAX - 1) { line[len++] = (char)c; vga_putc((char)c); }
}

static int split(char *line, char **argv) {
    int argc = 0;
    while (*line && argc < MAX_ARGS) {
        while (*line == ' ') *line++ = 0;
        if (!*line) break;
        argv[argc++] = line;
        while (*line && *line != ' ') line++;
    }
    return argc;
}

/* ----------------------------------------------------- calculator (calc) */
static const char *cp;
static bool calc_err;
static const char *calc_msg;
static int32_t expr(void);

static void skip_ws(void) { while (*cp == ' ') cp++; }

static int32_t factor(void) {
    skip_ws();
    if (*cp == '-') { cp++; return -factor(); }
    if (*cp == '+') { cp++; return factor(); }
    if (*cp == '(') {
        cp++;
        int32_t v = expr();
        skip_ws();
        if (*cp == ')') cp++; else { calc_err = true; calc_msg = "missing ')'"; }
        return v;
    }
    if (*cp < '0' || *cp > '9') { calc_err = true; calc_msg = "expected a number"; return 0; }
    int32_t v = 0;
    while (*cp >= '0' && *cp <= '9') v = v * 10 + (*cp++ - '0');
    return v;
}

static int32_t term(void) {
    int32_t v = factor();
    for (;;) {
        skip_ws();
        char op = *cp;
        if (op != '*' && op != '/' && op != '%') return v;
        cp++;
        int32_t r = factor();
        if (op == '*') v *= r;
        else if (r == 0) { calc_err = true; calc_msg = "division by zero"; return 0; }
        else v = (op == '/') ? v / r : v % r;
    }
}

static int32_t expr(void) {
    int32_t v = term();
    for (;;) {
        skip_ws();
        if (*cp == '+') { cp++; v += term(); }
        else if (*cp == '-') { cp++; v -= term(); }
        else return v;
    }
}

/* --------------------------------------------------------------- commands */
static void cmd_help(int argc, char **argv);

static void cmd_clear(int argc, char **argv) { (void)argc; (void)argv; vga_clear(); }

static void cmd_echo(int argc, char **argv) {
    for (int i = 1; i < argc; i++) { if (i > 1) vga_putc(' '); vga_puts(argv[i]); }
    vga_putc('\n');
}

static void cmd_uptime(int argc, char **argv) {
    (void)argc; (void)argv;
    uint32_t t = timer_ticks(), hz = timer_hz(), s = t / hz;
    kprintf("up %u h %u min %u.%u%u s  (%u timer ticks at %u Hz)\n",
            s / 3600, (s / 60) % 60, s % 60, (t % hz) * 10 / hz, ((t % hz) * 100 / hz) % 10, t, hz);
}

static void cmd_about(int argc, char **argv) {
    (void)argc; (void)argv;
    uint8_t f = vga_get_fg(), b = vga_get_bg();
    vga_set_colour(LIGHT_CYAN, b); vga_puts("LeoOS " LEOOS_VERSION);
    vga_set_colour(f, b);
    vga_puts(" - a tiny hobby operating system built from scratch for Leo.\n"
             "  * 32-bit x86 protected mode kernel in C with a NASM entry stub\n"
             "  * Boots via GRUB (Multiboot), flat GDT, IDT + remapped 8259 PIC\n"
             "  * PIT timer (100 Hz), PS/2 keyboard (UK/US), VGA text console\n"
             "  * This shell, with history (up/down arrows), Ctrl+L and Ctrl+C\n"
             "  * 0.2: a green desktop GUI with windows, mouse and apps\n");
}

static void cmd_colour(int argc, char **argv) {
    if (argc < 2) {
        vga_puts("usage: colour <fg> [bg]   e.g. 'colour yellow' or 'colour white blue'\ncolours: ");
        uint8_t f = vga_get_fg(), b = vga_get_bg();
        for (int i = 0; i < 16; i++) {
            vga_set_colour((uint8_t)i, i == BLACK ? LIGHT_GREY : BLACK);
            vga_puts(colour_names[i]);
            vga_set_colour(f, b);
            vga_putc(' ');
        }
        vga_putc('\n');
        return;
    }
    int f = colour_from_name(argv[1]);
    int b = argc > 2 ? colour_from_name(argv[2]) : vga_get_bg();
    if (f < 0 || b < 0) { vga_puts("unknown colour - type 'colour' to list them\n"); return; }
    vga_set_colour((uint8_t)f, (uint8_t)b);
    kprintf("text colour is now %s", colour_names[f]);
    if (argc > 2) kprintf(" on %s", colour_names[b]);
    vga_putc('\n');
}

static void cmd_reboot(int argc, char **argv) {
    (void)argc; (void)argv;
    vga_puts("Rebooting...\n");
    timer_sleep(300);
    cpu_cli();
    while (inb(0x64) & 2) ;
    outb(0x64, 0xFE);                         /* pulse the CPU reset line */
    struct { uint16_t l; uint32_t b; } __attribute__((packed)) null_idt = { 0, 0 };
    __asm__ volatile ("lidt %0; int $3" : : "m"(null_idt));   /* triple fault fallback */
    for (;;) cpu_hlt();
}

static void cmd_shutdown(int argc, char **argv) {
    (void)argc; (void)argv;
    vga_puts("Powering off... goodbye, Leo!\n");
    timer_sleep(300);
    outw(0x604, 0x2000);                      /* QEMU (q35/pc) ACPI power-off */
    outw(0xB004, 0x2000);                     /* older QEMU / Bochs */
    outw(0x4004, 0x3400);                     /* VirtualBox */
    vga_puts("Power-off not supported here; it is now safe to close the window.\n");
    cpu_cli();
    for (;;) cpu_hlt();
}

static void print_kib(uint64_t bytes) {
    uint32_t kib = (uint32_t)(bytes >> 10);
    if (kib >= 10240) kprintf("%u MiB", kib >> 10);
    else kprintf("%u KiB", kib);
}

static void cmd_mem(int argc, char **argv) {
    (void)argc; (void)argv;
    if (!mb_ok) { vga_puts("no Multiboot information available\n"); return; }
    if (mb->flags & 1)
        kprintf("lower memory: %u KiB, upper memory: %u KiB (%u MiB)\n",
                mb->mem_lower, mb->mem_upper, mb->mem_upper >> 10);
    if (!(mb->flags & (1 << 6))) { vga_puts("no memory map from the bootloader\n"); return; }
    vga_puts("BIOS memory map:\n");
    uint64_t usable = 0;
    uint32_t p = mb->mmap_addr, end = mb->mmap_addr + mb->mmap_length;
    while (p < end) {
        struct multiboot_mmap_entry *e = (struct multiboot_mmap_entry *)p;
        const char *type = e->type == 1 ? "usable" : e->type == 3 ? "ACPI reclaimable" :
                           e->type == 4 ? "ACPI NVS" : e->type == 5 ? "bad RAM" : "reserved";
        kprintf("  %x - %x  ", (uint32_t)e->addr, (uint32_t)(e->addr + e->len - 1));
        print_kib(e->len);
        kprintf("  %s\n", type);
        if (e->type == 1) usable += e->len;
        p += e->size + sizeof(e->size);
    }
    vga_puts("total usable RAM: "); print_kib(usable); vga_putc('\n');
}

static void cmd_calc(int argc, char **argv) {
    if (argc < 2) { vga_puts("usage: calc <expression>   e.g. calc (6 + 1) * 6\n"); return; }
    char buf[LINE_MAX]; buf[0] = 0;
    for (int i = 1; i < argc; i++) {             /* glue the args back together */
        if (strlen(buf) + strlen(argv[i]) + 2 >= LINE_MAX) break;
        if (i > 1) { size_t n = strlen(buf); buf[n] = ' '; buf[n + 1] = 0; }
        strcpy(buf + strlen(buf), argv[i]);
    }
    cp = buf; calc_err = false; calc_msg = "";
    int32_t v = expr();
    skip_ws();
    if (!calc_err && *cp) { calc_err = true; calc_msg = "unexpected character"; }
    if (calc_err) kprintf("calc: error: %s\n", calc_msg);
    else kprintf("%s = %d\n", buf, v);
}

static void cmd_kbd(int argc, char **argv) {
    if (argc > 1 && !strcmp(argv[1], "uk")) keyboard_set_layout(1);
    else if (argc > 1 && !strcmp(argv[1], "us")) keyboard_set_layout(0);
    else if (argc > 1) { vga_puts("usage: kbd [uk|us]\n"); return; }
    kprintf("keyboard layout: %s\n", keyboard_is_uk() ? "UK" : "US");
}

static void cmd_history(int argc, char **argv) {
    (void)argc; (void)argv;
    int start = hist_count > HIST_MAX ? hist_count - HIST_MAX : 0;
    for (int i = start; i < hist_count; i++) kprintf("  %d  %s\n", i + 1, history[i % HIST_MAX]);
}

void shell_command_reboot(void)   { cmd_reboot(0, NULL); }
void shell_command_shutdown(void) { cmd_shutdown(0, NULL); }

struct command { const char *name; void (*fn)(int, char **); const char *help; };
static const struct command commands[] = {
    { "help",     cmd_help,     "show this list" },
    { "clear",    cmd_clear,    "clear the screen (or press Ctrl+L)" },
    { "echo",     cmd_echo,     "print some text: echo hello leo" },
    { "uptime",   cmd_uptime,   "time since boot" },
    { "about",    cmd_about,    "about LeoOS" },
    { "colour",   cmd_colour,   "change text colour: colour yellow [blue]" },
    { "color",    cmd_colour,   NULL },
    { "mem",      cmd_mem,      "show the memory map from the bootloader" },
    { "calc",     cmd_calc,     "integer calculator: calc (2 + 3) * 7" },
    { "kbd",      cmd_kbd,      "show/switch keyboard layout: kbd uk | kbd us" },
    { "history",  cmd_history,  "list recent commands (up/down to recall)" },
    { "reboot",   cmd_reboot,   "restart the machine" },
    { "shutdown", cmd_shutdown, "power off (in QEMU)" },
};
#define NCOMMANDS (sizeof(commands) / sizeof(commands[0]))

static void cmd_help(int argc, char **argv) {
    (void)argc; (void)argv;
    uint8_t f = vga_get_fg(), b = vga_get_bg();
    vga_puts("LeoOS commands:\n");
    for (size_t i = 0; i < NCOMMANDS; i++) {
        if (!commands[i].help) continue;
        vga_set_colour(YELLOW, b);
        vga_puts("  "); vga_puts(commands[i].name);
        for (size_t n = strlen(commands[i].name); n < 10; n++) vga_putc(' ');
        vga_set_colour(f, b);
        vga_puts(commands[i].help); vga_putc('\n');
    }
}

static void execute(char *cmdline) {
    char *argv[MAX_ARGS];
    if (hist_count == 0 || strcmp(history[(hist_count - 1) % HIST_MAX], cmdline))
        strcpy(history[hist_count++ % HIST_MAX], cmdline);
    int argc = split(cmdline, argv);
    if (!argc) return;
    size_t i;
    for (i = 0; i < NCOMMANDS; i++)
        if (!strcmp(argv[0], commands[i].name)) { commands[i].fn(argc, argv); break; }
    if (i == NCOMMANDS) kprintf("%s: command not found (try 'help')\n", argv[0]);
}

void shell_init(struct multiboot_info *mbi, bool mb_valid) {
    mb = mbi; mb_ok = mb_valid;
    len = 0; hist_pos = hist_count;
    prompt();
}

void shell_run(struct multiboot_info *mbi, bool mb_valid) {
    shell_init(mbi, mb_valid);
    for (;;) shell_key(keyboard_getchar());
}
