/* LeoOS 0.4 - extra terminal commands (written by Astra)
 *   date, time, whoami, hostname, uname, neofetch, apps, open + app shortcuts,
 *   pwd, cd, mkdir, rmdir, tree, touch, cp, mv, wc, head, grep, fortune, cowsay
 * File commands use Nova's 0.4 folder API (see FS_API.md). */
#include "shell2.h"
#include "vga.h"
#include "cpu.h"
#include "io.h"
#include "string.h"
#include "version.h"
#include "ramfs.h"
#include "gfx.h"
#include "gui.h"

bool gui_active;
static struct multiboot_info *mb;
static bool mb_ok;

void shell2_init(struct multiboot_info *mbi, bool mb_valid) { mb = mbi; mb_ok = mb_valid; }

static void put2(uint32_t v) { vga_putc((char)('0' + (v / 10) % 10)); vga_putc((char)('0' + v % 10)); }

/* ------------------------------------------------------------ real-time clock */
struct rtc { uint32_t sec, min, hour, day, mon, year; };

static uint8_t cmos(uint8_t reg) { outb(0x70, reg); io_wait(); return inb(0x71); }
static uint32_t bcd(uint8_t v) { return (uint32_t)((v & 0x0F) + (v >> 4) * 10); }

static void rtc_read(struct rtc *t) {
    uint8_t s, m, h, d, mo, y, s2, m2, h2, d2, mo2, y2;
    while (cmos(0x0A) & 0x80) ;               /* wait out an update in progress */
    s = cmos(0x00); m = cmos(0x02); h = cmos(0x04); d = cmos(0x07); mo = cmos(0x08); y = cmos(0x09);
    do {                                        /* read twice until both reads agree */
        s2 = s; m2 = m; h2 = h; d2 = d; mo2 = mo; y2 = y;
        while (cmos(0x0A) & 0x80) ;
        s = cmos(0x00); m = cmos(0x02); h = cmos(0x04); d = cmos(0x07); mo = cmos(0x08); y = cmos(0x09);
    } while (s != s2 || m != m2 || h != h2 || d != d2 || mo != mo2 || y != y2);
    uint8_t b = cmos(0x0B);
    bool pm = h & 0x80; h &= 0x7F;
    if (!(b & 0x04)) { t->sec = bcd(s); t->min = bcd(m); t->hour = bcd(h); t->day = bcd(d); t->mon = bcd(mo); t->year = bcd(y); }
    else { t->sec = s; t->min = m; t->hour = h; t->day = d; t->mon = mo; t->year = y; }
    if (!(b & 0x02) && pm) t->hour = (t->hour + 12) % 24;   /* 12-hour clock */
    t->year += 2000;
}

static const char *day_names[7] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
static const char *month_names[12] = { "January", "February", "March", "April", "May", "June", "July",
                                       "August", "September", "October", "November", "December" };

static int weekday(int y, int m, int d) {      /* Sakamoto's method, 0 = Sunday */
    static const int t[12] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
    if (m < 3) y--;
    return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}

static void print_time(const struct rtc *t) { put2(t->hour); vga_putc(':'); put2(t->min); vga_putc(':'); put2(t->sec); }

static void cmd_date(void) {
    struct rtc t; rtc_read(&t);
    if (t.mon < 1 || t.mon > 12) { vga_puts("date: the clock gave a strange answer\n"); return; }
    kprintf("%s %u %s %u, ", day_names[weekday((int)t.year, (int)t.mon, (int)t.day)], t.day, month_names[t.mon - 1], t.year);
    print_time(&t);
    vga_puts("  (from the PC's clock)\n");
}

static void cmd_time(void) { struct rtc t; rtc_read(&t); print_time(&t); vga_putc('\n'); }

/* ------------------------------------------------------------------- system */
static void cpu_name(char *out) {             /* "GenuineIntel" or the full brand string */
    uint32_t a, b, c, d;
    __asm__ volatile ("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(0x80000000u));
    if (a >= 0x80000004u) {
        uint32_t *w = (uint32_t *)out;
        for (uint32_t leaf = 0x80000002u; leaf <= 0x80000004u; leaf++, w += 4)
            __asm__ volatile ("cpuid" : "=a"(w[0]), "=b"(w[1]), "=c"(w[2]), "=d"(w[3]) : "a"(leaf));
        out[48] = 0;
        char *s = out; while (*s == ' ') s++;   /* trim leading spaces */
        if (s != out) memmove(out, s, strlen(s) + 1);
        return;
    }
    __asm__ volatile ("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(0));
    memcpy(out, &b, 4); memcpy(out + 4, &d, 4); memcpy(out + 8, &c, 4); out[12] = 0;
}

static void cmd_uname(int argc, char **argv) {
    if (argc > 1 && !strcmp(argv[1], "-a")) kprintf("LeoOS leoos %s i386 (32-bit x86)\n", LEOOS_VERSION);
    else vga_puts("LeoOS\n");
}

static void cmd_neofetch(void) {
    static const char *logo[] = {
        "   _                 ",
        "  | |    ___  ___    ",
        "  | |   / _ \\/ _ \\   ",
        "  | |__|  __/ (_) |  ",
        "  |_____\\___|\\___/   ",
        "        ___  ___     ",
        "       / _ \\/ __|    ",
        "      | (_) \\__ \\    ",
        "       \\___/|___/    ",
        "                     ",
    };
    uint8_t f = vga_get_fg(), b = vga_get_bg();
    char cpu[52]; cpu_name(cpu);
    uint32_t s = timer_ticks() / timer_hz();
    for (int i = 0; i < 10; i++) {
        vga_set_colour(LIGHT_GREEN, b); vga_puts(logo[i]);
        vga_set_colour(LIGHT_GREEN, b);
        switch (i) {
        case 0: vga_puts("leo"); vga_set_colour(WHITE, b); vga_puts("@"); vga_set_colour(LIGHT_CYAN, b); vga_puts("leoos"); break;
        case 1: vga_set_colour(DARK_GREY, b); vga_puts("---------"); break;
        case 2: vga_puts("OS: "); vga_set_colour(f, b); kprintf("LeoOS %s i386", LEOOS_VERSION); break;
        case 3: vga_puts("Kernel: "); vga_set_colour(f, b); vga_puts("LeoOS kernel (C + NASM)"); break;
        case 4: vga_puts("Uptime: "); vga_set_colour(f, b); kprintf("%u h %u min %u s", s / 3600, (s / 60) % 60, s % 60); break;
        case 5: vga_puts("Shell: "); vga_set_colour(f, b); vga_puts("leosh"); break;
        case 6: vga_puts("Display: "); vga_set_colour(f, b);
                if (gui_active) kprintf("%u x %u, 32-bit colour", (uint32_t)gfx_w, (uint32_t)gfx_h);
                else vga_puts("80 x 25 text mode");
                break;
        case 7: vga_puts("CPU: "); vga_set_colour(f, b); vga_puts(cpu); break;
        case 8: vga_puts("Memory: "); vga_set_colour(f, b);
                if (mb_ok && (mb->flags & 1)) kprintf("%u MiB", (mb->mem_upper + 1024) / 1024); else vga_puts("unknown");
                break;
        case 9: for (int c = 0; c < 8; c++) { vga_set_colour((uint8_t)c, (uint8_t)c); vga_puts("   "); }
                break;
        }
        vga_set_colour(f, b); vga_putc('\n');
    }
}

/* --------------------------------------------------------------------- apps */
static const struct { const char *name; int app; } app_names[] = {
    { "terminal", APP_TERMINAL }, { "calculator", APP_CALC }, { "calc-app", APP_CALC },
    { "paint", APP_PAINT }, { "notepad", APP_NOTEPAD }, { "files", APP_FILES }, { "explorer", APP_FILES },
    { "browser", APP_BROWSER }, { "snake", APP_SNAKE },
    { "minesweeper", APP_MINES }, { "mines", APP_MINES },
    { "2048", APP_2048 }, { "dive", APP_DIVE }, { "deepdive", APP_DIVE }, { "deep-dive", APP_DIVE },
    { "tetris", APP_TETRIS }, { "about", APP_ABOUT },
    /* MERGE: keep browser/2048/dive/tetris entries when refreshing this file from astra-terminal */
};
#define NAPPNAMES (sizeof(app_names) / sizeof(app_names[0]))

static int find_app(const char *s) {
    for (size_t i = 0; i < NAPPNAMES; i++) if (!strcmp(s, app_names[i].name)) return app_names[i].app;
    return -1;
}

static void open_app(int app) {
    if (!gui_active) { vga_puts("that app needs the desktop - pick 'LeoOS' (not text mode) in the boot menu\n"); return; }
    kprintf("opening %s\n", apps[app].name);
    gui_open_app(app);
}

static void cmd_apps(void) {
    vga_puts("apps you can open: terminal, calculator, paint, notepad, files, browser,\n"
             "  snake, minesweeper, 2048, dive, tetris, about\n"
             "try: open snake   (or just type: snake, mines, 2048, tetris, browser, files)\n");
}

static void cmd_open(int argc, char **argv) {
    if (argc < 2) { cmd_apps(); return; }
    int a = find_app(argv[1]);
    if (a < 0) { kprintf("open: no app called '%s' (type 'apps' to list them)\n", argv[1]); return; }
    open_app(a);
}

/* -------------------------------------------------------------------- files */
static char fbuf[FS_DATA_MAX];

static bool read_file(const char *cmd, const char *name, int *n) {
    *n = fs_read(name, fbuf, FS_DATA_MAX);
    if (*n < 0) { kprintf("%s: %s: no such file\n", cmd, name); return false; }
    return true;
}

static void write_error(const char *cmd, const char *path, int r) {
    if (r == -1) kprintf("%s: the file system is full - rm something first\n", cmd);
    else if (r == -2) kprintf("%s: %s: bad name (use letters, digits, . _ -)\n", cmd, path);
    else if (r == -3) kprintf("%s: %s: that folder doesn't exist\n", cmd, path);
    else if (r == -4) kprintf("%s: %s: is a folder\n", cmd, path);
}

static void cmd_touch(int argc, char **argv) {
    if (argc < 2) { vga_puts("usage: touch <file>   (makes an empty file)\n"); return; }
    for (int i = 1; i < argc; i++) {
        if (fs_find(argv[i]) >= 0) continue;           /* already there: leave it alone */
        int r = fs_write(argv[i], "", 0);
        if (r) write_error("touch", argv[i], r); else kprintf("created %s\n", argv[i]);
    }
}

static void cmd_cp(int argc, char **argv, bool move) {
    const char *cmd = move ? "mv" : "cp";
    if (argc != 3) { kprintf("usage: %s <from> <to>   (<to> can be a folder)\n", cmd); return; }
    int r = move ? fs_rename(argv[1], argv[2]) : fs_copy(argv[1], argv[2]);
    switch (r) {
    case 0:  kprintf("%s %s to %s\n", move ? "moved" : "copied", argv[1], argv[2]); break;
    case -1: kprintf("%s: %s: no such file or folder\n", cmd, argv[1]); break;
    case -2: kprintf("%s: %s: bad name (use letters, digits, . _ -)\n", cmd, argv[2]); break;
    case -3: kprintf("%s: %s: something with that name is already there\n", cmd, argv[2]); break;
    case -4: kprintf("%s: %s: that folder doesn't exist\n", cmd, argv[2]); break;
    case -5: kprintf("%s: can't do that (into itself, onto itself, or the root folder)\n", cmd); break;
    default: kprintf("%s: the file system got full part way through\n", cmd); break;
    }
}

static void cmd_pwd(void) { vga_puts(fs_cwd()); vga_putc('\n'); }

static void cmd_cd(int argc, char **argv) {
    const char *p = argc > 1 ? argv[1] : "/";
    if (!strcmp(p, "~")) p = "/";
    int r = fs_chdir(p);
    if (r == -1) kprintf("cd: %s: no such folder\n", p);
    else if (r == -2) kprintf("cd: %s: is a file, not a folder\n", p);
    else if (r == -3) kprintf("cd: %s: bad path\n", p);
}

static void cmd_mkdir(int argc, char **argv) {
    if (argc < 2) { vga_puts("usage: mkdir <folder>   e.g. mkdir games\n"); return; }
    for (int i = 1; i < argc; i++) {
        int r = fs_mkdir(argv[i]);
        if (r == 0) kprintf("made folder %s\n", argv[i]);
        else if (r == -1) vga_puts("mkdir: the file system is full - rm something first\n");
        else if (r == -2) kprintf("mkdir: %s: bad name (use letters, digits, . _ -)\n", argv[i]);
        else if (r == -3) kprintf("mkdir: %s: already exists\n", argv[i]);
        else kprintf("mkdir: %s: the folder it goes in doesn't exist\n", argv[i]);
    }
}

static void cmd_rmdir(int argc, char **argv) {
    if (argc < 2) { vga_puts("usage: rmdir <folder>   (the folder must be empty)\n"); return; }
    for (int i = 1; i < argc; i++) {
        int k = fs_is_dir(argv[i]);
        if (k < 0) { kprintf("rmdir: %s: no such folder\n", argv[i]); continue; }
        if (k == 0) { kprintf("rmdir: %s: is a file (use rm)\n", argv[i]); continue; }
        int r = fs_remove(argv[i]);
        if (r == 0) kprintf("removed folder %s\n", argv[i]);
        else if (r == -2) kprintf("rmdir: %s: folder isn't empty\n", argv[i]);
        else kprintf("rmdir: can't remove the root folder\n");
    }
}

/* tree: draws folders with box characters (code page 437 font) */
static uint32_t tree_dirs, tree_files;

static void tree_walk(const char *dir, char *prefix, int depth) {
    int n = fs_dir_count(dir);
    if (n < 0 || depth > 8) return;
    char names[FS_MAX_FILES][FS_NAME_MAX]; bool isdir[FS_MAX_FILES]; int cnt = 0;
    for (int i = 0; i < n && cnt < FS_MAX_FILES; i++)
        if (!fs_dir_get(dir, i, names[cnt], FS_NAME_MAX, &isdir[cnt], NULL)) cnt++;
    for (int i = 1; i < cnt; i++) {                       /* folders first, then by name */
        for (int j = i; j > 0; j--) {
            bool swap = (isdir[j] && !isdir[j - 1]) || (isdir[j] == isdir[j - 1] && strcmp(names[j], names[j - 1]) < 0);
            if (!swap) break;
            char t[FS_NAME_MAX]; strcpy(t, names[j]); strcpy(names[j], names[j - 1]); strcpy(names[j - 1], t);
            bool tb = isdir[j]; isdir[j] = isdir[j - 1]; isdir[j - 1] = tb;
        }
    }
    uint8_t f = vga_get_fg(), b = vga_get_bg();
    size_t plen = strlen(prefix);
    for (int i = 0; i < cnt; i++) {
        bool last = i == cnt - 1;
        vga_puts(prefix);
        vga_putc((char)(last ? 0xC0 : 0xC3)); vga_putc((char)0xC4); vga_putc((char)0xC4); vga_putc(' ');
        if (isdir[i]) { vga_set_colour(LIGHT_BLUE, b); vga_puts(names[i]); vga_putc('/'); tree_dirs++; }
        else { vga_puts(names[i]); tree_files++; }
        vga_set_colour(f, b); vga_putc('\n');
        if (isdir[i] && plen + 4 < 64) {
            char sub[FS_PATH_MAX];
            if (fs_join(sub, FS_PATH_MAX, dir, names[i]) < 0) continue;
            prefix[plen] = last ? ' ' : (char)0xB3; prefix[plen + 1] = ' '; prefix[plen + 2] = ' '; prefix[plen + 3] = ' ';
            prefix[plen + 4] = 0;
            tree_walk(sub, prefix, depth + 1);
            prefix[plen] = 0;
        }
    }
}

static void cmd_tree(int argc, char **argv) {
    char dir[FS_PATH_MAX];
    const char *arg = argc > 1 ? argv[1] : "";
    int k = fs_is_dir(arg);
    if (k < 0) { kprintf("tree: %s: no such folder\n", arg); return; }
    if (k == 0) { kprintf("tree: %s: is a file\n", arg); return; }
    int slot = fs_find(arg);
    if (slot < 0 || fs_path(slot, dir, FS_PATH_MAX) < 0) strcpy(dir, "/");
    uint8_t f = vga_get_fg(), b = vga_get_bg();
    vga_set_colour(LIGHT_BLUE, b); vga_puts(dir); vga_set_colour(f, b); vga_putc('\n');
    static char prefix[80]; prefix[0] = 0;
    tree_dirs = tree_files = 0;
    tree_walk(dir, prefix, 0);
    kprintf("%u folder(s), %u file(s)\n", tree_dirs, tree_files);
}

static void cmd_wc(int argc, char **argv) {
    if (argc < 2) { vga_puts("usage: wc <file>   (counts lines, words and letters)\n"); return; }
    int n; if (!read_file("wc", argv[1], &n)) return;
    uint32_t lines = 0, words = 0; bool in_word = false;
    for (int i = 0; i < n; i++) {
        char c = fbuf[i];
        if (c == '\n') lines++;
        if (c == ' ' || c == '\n' || c == '\t') in_word = false;
        else if (!in_word) { in_word = true; words++; }
    }
    if (n && fbuf[n - 1] != '\n') lines++;
    kprintf("%u lines, %u words, %u characters  %s\n", lines, words, (uint32_t)n, argv[1]);
}

static int to_int(const char *s) { int v = 0; while (*s >= '0' && *s <= '9') v = v * 10 + (*s++ - '0'); return v; }

static void cmd_head(int argc, char **argv) {
    if (argc < 2) { vga_puts("usage: head <file> [lines]   (shows the first 10 lines)\n"); return; }
    int want = argc > 2 ? to_int(argv[2]) : 10;
    int n; if (!read_file("head", argv[1], &n)) return;
    int lines = 0;
    for (int i = 0; i < n && lines < want; i++) { vga_putc(fbuf[i]); if (fbuf[i] == '\n') lines++; }
    if (lines < want && n && fbuf[n - 1] != '\n') vga_putc('\n');
}

static char lower(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c; }

static bool contains(const char *line, int len, const char *word) {
    int wl = (int)strlen(word);
    for (int i = 0; i + wl <= len; i++) {
        int j = 0;
        while (j < wl && lower(line[i + j]) == lower(word[j])) j++;
        if (j == wl) return true;
    }
    return false;
}

static void cmd_grep(int argc, char **argv) {
    if (argc < 3) { vga_puts("usage: grep <word> <file>   (shows lines containing the word)\n"); return; }
    int n; if (!read_file("grep", argv[2], &n)) return;
    uint8_t f = vga_get_fg(), b = vga_get_bg();
    int start = 0, found = 0, lineno = 1;
    for (int i = 0; i <= n; i++) {
        if (i == n || fbuf[i] == '\n') {
            if (i > start && contains(fbuf + start, i - start, argv[1])) {
                vga_set_colour(YELLOW, b); kprintf("%u: ", (uint32_t)lineno); vga_set_colour(f, b);
                for (int k = start; k < i; k++) vga_putc(fbuf[k]);
                vga_putc('\n'); found++;
            }
            start = i + 1; lineno++;
        }
    }
    if (!found) kprintf("no lines with '%s' in %s\n", argv[1], argv[2]);
}

/* --------------------------------------------------------------------- fun */
static const char *fortunes[] = {
    "There's no place like 127.0.0.1",
    "LeoOS tip: press the up arrow to bring back your last command.",
    "A green desktop a day keeps the blue screen away.",
    "Real programmers count from 0.",
    "It works on my machine - and this is my machine.",
    "Snake tip: the walls wrap round, so go off one side to escape.",
    "Every great OS started as a hobby. Even Linux.",
    "Have you tried turning it off and on again? (type 'reboot')",
};

static void cmd_fortune(void) {
    uint32_t n = sizeof(fortunes) / sizeof(fortunes[0]);
    vga_puts(fortunes[(timer_ticks() * 2654435761u >> 16) % n]); vga_putc('\n');
}

static void cmd_cowsay(int argc, char **argv) {
    char msg[100]; msg[0] = 0;
    if (argc < 2) strcpy(msg, "Moo! Hello Leo");
    for (int i = 1; i < argc; i++) {
        if (strlen(msg) + strlen(argv[i]) + 2 >= 60) break;
        if (i > 1) strcpy(msg + strlen(msg), " ");
        strcpy(msg + strlen(msg), argv[i]);
    }
    int w = (int)strlen(msg);
    vga_puts(" "); for (int i = 0; i < w + 2; i++) vga_putc('_'); vga_puts("\n< "); vga_puts(msg); vga_puts(" >\n ");
    for (int i = 0; i < w + 2; i++) vga_putc('-');
    vga_puts("\n        \\   ^__^\n         \\  (oo)\\_______\n            (__)\\       )\\/\\\n                ||----w |\n                ||     ||\n");
}

/* ------------------------------------------------------------------ table */
struct cmd2 { const char *name; const char *help; };
static const struct cmd2 cmds2[] = {
    { "date",     "today's date and time: date" },
    { "time",     "just the time: time" },
    { "whoami",   "who you are logged in as" },
    { "hostname", "this computer's name" },
    { "uname",    "system name: uname -a" },
    { "neofetch", "LeoOS logo with system info" },
    { "apps",     "list the apps you can open" },
    { "open",     "open an app: open snake (or just type snake)" },
    { "pwd",      "show which folder you're in" },
    { "cd",       "go into a folder: cd docs  (cd .. goes up)" },
    { "mkdir",    "make a folder: mkdir games" },
    { "rmdir",    "delete an empty folder: rmdir games" },
    { "tree",     "show all folders and files" },
    { "touch",    "make an empty file: touch notes.txt" },
    { "cp",       "copy: cp a.txt b.txt  or  cp a.txt docs" },
    { "mv",       "move or rename: mv a.txt docs" },
    { "wc",       "count lines and words: wc readme.txt" },
    { "head",     "first lines of a file: head readme.txt 3" },
    { "grep",     "find a word in a file: grep leo readme.txt" },
    { "fortune",  "a random message" },
    { "cowsay",   "a cow says something: cowsay hi leo" },
};

void shell2_help(void) {
    uint8_t f = vga_get_fg(), b = vga_get_bg();
    for (size_t i = 0; i < sizeof(cmds2) / sizeof(cmds2[0]); i++) {
        vga_set_colour(YELLOW, b);
        vga_puts("  "); vga_puts(cmds2[i].name);
        for (size_t n = strlen(cmds2[i].name); n < 10; n++) vga_putc(' ');
        vga_set_colour(f, b);
        vga_puts(cmds2[i].help); vga_putc('\n');
    }
}

int shell2_cmd_count(void) { return (int)(sizeof(cmds2) / sizeof(cmds2[0])); }
const char *shell2_cmd_name(int i) { return cmds2[i].name; }
const char *shell2_cmd_help(int i) { return cmds2[i].help; }

bool shell2_run(int argc, char **argv) {
    const char *c = argv[0];
    if (!strcmp(c, "date"))     { cmd_date(); return true; }
    if (!strcmp(c, "time"))     { cmd_time(); return true; }
    if (!strcmp(c, "whoami"))   { vga_puts("leo\n"); return true; }
    if (!strcmp(c, "hostname")) { vga_puts("leoos\n"); return true; }
    if (!strcmp(c, "uname"))    { cmd_uname(argc, argv); return true; }
    if (!strcmp(c, "neofetch")) { cmd_neofetch(); return true; }
    if (!strcmp(c, "apps"))     { cmd_apps(); return true; }
    if (!strcmp(c, "open"))     { cmd_open(argc, argv); return true; }
    if (!strcmp(c, "pwd"))      { cmd_pwd(); return true; }
    if (!strcmp(c, "cd"))       { cmd_cd(argc, argv); return true; }
    if (!strcmp(c, "mkdir"))    { cmd_mkdir(argc, argv); return true; }
    if (!strcmp(c, "rmdir"))    { cmd_rmdir(argc, argv); return true; }
    if (!strcmp(c, "tree"))     { cmd_tree(argc, argv); return true; }
    if (!strcmp(c, "touch"))    { cmd_touch(argc, argv); return true; }
    if (!strcmp(c, "cp"))       { cmd_cp(argc, argv, false); return true; }
    if (!strcmp(c, "mv"))       { cmd_cp(argc, argv, true); return true; }
    if (!strcmp(c, "wc"))       { cmd_wc(argc, argv); return true; }
    if (!strcmp(c, "head"))     { cmd_head(argc, argv); return true; }
    if (!strcmp(c, "grep"))     { cmd_grep(argc, argv); return true; }
    if (!strcmp(c, "fortune"))  { cmd_fortune(); return true; }
    if (!strcmp(c, "cowsay"))   { cmd_cowsay(argc, argv); return true; }
    int a = find_app(c);                       /* app shortcuts: snake, mines, paint ... */
    if (a >= 0 && a != APP_TERMINAL && strcmp(c, "about")) { open_app(a); return true; }
    return false;
}
