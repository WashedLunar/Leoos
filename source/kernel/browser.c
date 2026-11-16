/* LeoOS - Browser (by Vega)
 *
 * Page format (a tiny markdown):
 *   # Heading        big green title
 *   ## Subheading    bold line
 *   - item           bullet point
 *   [text](address)  a link, anywhere in a line
 *   blank line       paragraph gap
 * Addresses: leo://home, leo://apps, ... (built in), leo://files (lists your
 * RAM files) and file:NAME (opens a file you saved in Notepad as a page).
 *
 * Phone friendly: drag the page to scroll, tap a link to follow it, and the
 * big buttons on the right scroll too. Keys: arrows scroll, Backspace = Back,
 * click the address bar to type an address and press Enter. */
#include "browser.h"
#include "gfx.h"
#include "cpu.h"
#include "string.h"
#include "ramfs.h"
#include "version.h"

#define TOOLBAR   40
#define SCROLLW   28
#define MARGIN    16
#define URL_MAX   80
#define HIST_MAX  16
#define LINKS_MAX 96

#define C_GREEN   RGB(0x2F, 0xA8, 0x4F)
#define C_DGREEN  RGB(0x1C, 0x75, 0x36)
#define C_TEXT    RGB(0x10, 0x30, 0x18)
#define C_LINK    RGB(0x1A, 0x5F, 0xC8)
#define C_BAR     RGB(0xD4, 0xE6, 0xD8)

/* ---------------------------------------------------------------- pages */
struct page { const char *url; const char *title; const char *text; };

static const struct page pages[] = {
    { "leo://home", "LeoOS Home",
      "# Welcome to LeoOS\n"
      "This is the LeoOS Browser. It opens pages that live inside LeoOS, and pages you write yourself.\n"
      "\n"
      "## Explore\n"
      "- [What's new in 0.4](leo://new)\n"
      "- [Apps and games](leo://apps)\n"
      "- [Terminal tips](leo://terminal)\n"
      "- [Your files](leo://files)\n"
      "- [Make your own page](leo://make)\n"
      "- [Meet the team](leo://team)\n"
      "- [About LeoOS](leo://about)\n"
      "\n"
      "Tip: drag the page up and down to scroll, or use the arrow keys.\n" },
    { "leo://new", "What's new",
      "# What's new in 0.4\n"
      "- A Files app, like File Explorer, with folders, rename, delete and copy\n"
      "- Folders in the RAM filesystem. Paths like docs/welcome.txt work in ls, cat, cd and write\n"
      "- A web browser (you're using it!) with built-in pages, links, Back and Forward\n"
      "- Pages you write in Notepad open in the browser, including files in a folder. See [Make your own page](leo://make)\n"
      "- New games: 2048, Deep Dive and Tetris. See [Apps and games](leo://apps)\n"
      "- New terminal commands: mkdir, cd, pwd, tree, neofetch, open, and more. See [Terminal tips](leo://terminal)\n"
      "\n"
      "[Back to Home](leo://home)\n" },
    { "leo://apps", "Apps and games",
      "# Apps and games\n"
      "Open any app from the Start menu or by clicking its desktop icon.\n"
      "\n"
      "## Apps\n"
      "- Terminal: the classic LeoOS shell. See [Terminal tips](leo://terminal)\n"
      "- Calculator: tap the buttons or type a sum\n"
      "- Paint: 12 colours and 3 brush sizes\n"
      "- Notepad: write and save text files, including in folders\n"
      "- Files: browse folders, open files in Notepad, new folder, rename, delete, copy\n"
      "- Browser: this app\n"
      "\n"
      "## Games\n"
      "- Snake: arrow keys, WASD or the on-screen arrows. The walls wrap round\n"
      "- Minesweeper: 9 x 9 with 10 mines. Your first click is always safe\n"
      "- 2048: swipe, arrows or the on-screen pad. Merge the tiles up to 2048\n"
      "- Deep Dive: swim with the mouse or arrows, collect fish, watch your oxygen\n"
      "- Tetris: classic 10 x 20 well, on-screen pad, next piece and a ghost piece\n"
      "\n"
      "[Back to Home](leo://home)\n" },
    { "leo://terminal", "Terminal tips",
      "# Terminal tips\n"
      "Type help in the Terminal to see every command (help 2 for the next page). A few favourites:\n"
      "- ls lists a folder, e.g. ls docs, and cat docs/welcome.txt shows a file\n"
      "- mkdir, cd, pwd and tree work with folders; write, rm, cp and mv take paths\n"
      "- neofetch shows a LeoOS logo with system info\n"
      "- open snake (or just type snake, 2048, tetris, files, browser) opens an app\n"
      "- calc 6*7 does a sum, and colour changes the text colour\n"
      "\n"
      "[Back to Home](leo://home)\n" },
    { "leo://make", "Make your own page",
      "# Make your own page\n"
      "You can write web pages for LeoOS in Notepad!\n"
      "\n"
      "## How\n"
      "- Open Notepad and type your page\n"
      "- Save it with a name like mypage.txt, or a path like docs/mypage.txt\n"
      "- Open [Your files](leo://files) and tap it, or type file:docs/mypage.txt in the address bar\n"
      "\n"
      "## What you can type\n"
      "- A line starting with # and a space is a big heading\n"
      "- A line starting with ## and a space is a smaller heading\n"
      "- A line starting with - and a space is a bullet point\n"
      "- [Some text](leo://home) makes a link. The address goes in the round brackets\n"
      "\n"
      "Files are kept in memory, so they disappear when LeoOS restarts.\n"
      "\n"
      "[Back to Home](leo://home)\n" },
    { "leo://team", "Meet the team",
      "# Meet the team\n"
      "LeoOS is Leo's operating system, built with help from his bots.\n"
      "- Leo: the boss, ideas and testing\n"
      "- Nova: the kernel, the desktop, folders and the Files app\n"
      "- Luna: Minesweeper, 2048 and Deep Dive\n"
      "- Starlink: Tetris, testing and the changelog\n"
      "- Astra: terminal commands\n"
      "- Vega: this browser\n"
      "\n"
      "[Back to Home](leo://home)\n" },
    { "leo://about", "About LeoOS",
      "# About LeoOS\n"
      "LeoOS " LEOOS_VERSION " is a small hobby operating system for 32-bit PCs, written from scratch in C for Leo.\n"
      "\n"
      "## Under the bonnet\n"
      "- Boots with GRUB into a 1024 x 768 green desktop\n"
      "- Its own window manager, mouse and keyboard drivers\n"
      "- A RAM filesystem, so files are lost when you restart\n"
      "- No network driver yet, so no real internet. See [No internet yet](leo://internet)\n"
      "\n"
      "[Back to Home](leo://home)\n" },
    { "leo://internet", "No internet yet",
      "# No internet yet\n"
      "LeoOS can't reach real websites yet. Before it can, it needs a driver for the network card (like the Intel one in Leo's EliteBook), "
      "code that speaks the internet's language, and the encryption that secure websites use.\n"
      "\n"
      "That's a big goal for a future version. Until then, enjoy the pages built into LeoOS.\n"
      "\n"
      "[Back to Home](leo://home)\n" },
};
#define NPAGES ((int)(sizeof pages / sizeof pages[0]))

/* ---------------------------------------------------------------- state */
static char cur_url[URL_MAX];
static char hist[HIST_MAX][URL_MAX];
static int hist_len, hist_pos;
static const char *src;                 /* text of the current page */
static char dyn[FS_DATA_MAX + 600];     /* buffer for generated / file pages */
static int scroll, content_h, view_h, last_w = BROWSER_W;

static bool addr_edit;
static char addr[URL_MAX];
static int addr_len;

static struct { int x, y, w, h; char url[URL_MAX]; } links[LINKS_MAX];
static int nlinks;

static int pressed = -1;                /* toolbar/scroll button being held */
static bool pressing, moved;
static int press_x, press_y, press_scroll;

enum { BTN_BACK, BTN_FWD, BTN_HOME, BTN_UP, BTN_DOWN };

/* ---------------------------------------------------------------- helpers */
static void copy(char *d, const char *s, int max) {
    int i = 0;
    for (; s[i] && i < max - 1; i++) d[i] = s[i];
    d[i] = 0;
}
static void append(char *d, const char *s, int max) {
    int n = (int)strlen(d);
    copy(d + n, s, max - n);
}
static bool starts(const char *s, const char *p) { return !strncmp(s, p, strlen(p)); }

/* Turns whatever was typed into an address: "home" -> leo://home. */
static void normalise(const char *in, char *out) {
    while (*in == ' ') in++;
    if (starts(in, "leo://") || starts(in, "file:") || starts(in, "http://") || starts(in, "https://") || starts(in, "www."))
        copy(out, in, URL_MAX);
    else if (fs_find(in) >= 0) { copy(out, "file:", URL_MAX); append(out, in, URL_MAX); }
    else { copy(out, "leo://", URL_MAX); append(out, in, URL_MAX); }
    int n = (int)strlen(out);                       /* drop trailing spaces */
    while (n > 0 && out[n - 1] == ' ') out[--n] = 0;
}

static void load(const char *url) {
    copy(cur_url, url, URL_MAX);
    scroll = 0;
    if (starts(url, "http://") || starts(url, "https://") || starts(url, "www.")) {
        for (int i = 0; i < NPAGES; i++) if (!strcmp(pages[i].url, "leo://internet")) src = pages[i].text;
        return;
    }
    for (int i = 0; i < NPAGES; i++) if (!strcmp(pages[i].url, url)) { src = pages[i].text; return; }

    if (!strcmp(url, "leo://files")) {
        copy(dyn, "# Your files\n", sizeof dyn);
        if (!fs_files()) append(dyn, "You haven't saved any files yet. Write something in Notepad and tap Save.\n", sizeof dyn);
        else append(dyn, "Tap a file to open it as a page. Files in folders show their full path.\n\n", sizeof dyn);
        for (int i = 0; i < fs_count(); i++) {
            if (!fs_used(i) || fs_slot_is_dir(i)) continue;
            char path[FS_PATH_MAX], num[12];
            if (fs_path(i, path, FS_PATH_MAX) < 0) continue;
            append(dyn, "- [", sizeof dyn); append(dyn, path, sizeof dyn);
            append(dyn, "](file:", sizeof dyn); append(dyn, path, sizeof dyn);
            append(dyn, ") ", sizeof dyn); utoa((uint32_t)fs_size(i), num, 10);
            append(dyn, num, sizeof dyn); append(dyn, " bytes\n", sizeof dyn);
        }
        append(dyn, "\n[How to make your own page](leo://make)\n[Back to Home](leo://home)\n", sizeof dyn);
        src = dyn; return;
    }
    if (starts(url, "file:")) {
        const char *path = url + 5;
        if (fs_is_dir(path) == 1) {
            copy(dyn, "# That's a folder\n", sizeof dyn);
            append(dyn, path, sizeof dyn);
            append(dyn, " is a folder. Open a file inside it, or see [Your files](leo://files).\n", sizeof dyn);
            src = dyn; return;
        }
        int n = fs_read(path, dyn, FS_DATA_MAX);
        if (n >= 0) { dyn[n] = 0; src = dyn; return; }
    }
    copy(dyn, "# Page not found\nLeoOS couldn't find ", sizeof dyn);
    append(dyn, url, sizeof dyn);
    append(dyn, ".\n\n- [Go to Home](leo://home)\n- [Your files](leo://files)\n", sizeof dyn);
    src = dyn;
}

static void go(const char *url) {
    char u[URL_MAX];
    normalise(url, u);
    if (hist_len && !strcmp(hist[hist_pos], u)) { load(u); return; }   /* reload */
    if (hist_len) hist_len = hist_pos + 1;          /* drop forward history */
    if (hist_len == HIST_MAX) {
        for (int i = 1; i < HIST_MAX; i++) copy(hist[i - 1], hist[i], URL_MAX);
        hist_len--;
    }
    copy(hist[hist_len], u, URL_MAX);
    hist_pos = hist_len++;
    load(u);
}
static void back(void) { if (hist_pos > 0) load(hist[--hist_pos]); }
static void fwd(void)  { if (hist_pos < hist_len - 1) load(hist[++hist_pos]); }

static void clamp_scroll(void) {
    int max = content_h - view_h;
    if (max < 0) max = 0;
    if (scroll > max) scroll = max;
    if (scroll < 0) scroll = 0;
}

void browser_init(void) {
    hist_len = hist_pos = 0;
    addr_edit = false;
    go("leo://home");
}

/* ---------------------------------------------------------------- layout */
/* Lays out (and draws) the page. Coordinates: cx/cy = content origin on screen,
 * cw = usable width. Returns total content height. Links are recorded in
 * client coordinates (relative to the window's client area). */
static int render(int ox, int oy, int cx, int cy, int cw, int vh, bool draw) {
    int y = 10;
    nlinks = 0;
    const char *p = src;
    while (*p) {
        const char *eol = p; while (*eol && *eol != '\n') eol++;
        int scale = 1, indent = 0, lh = FONT_H + 4;
        uint32_t fg = C_TEXT; bool bold = false;
        if (eol == p) { y += 8; p = *eol ? eol + 1 : eol; continue; }
        if (starts(p, "# "))       { p += 2; scale = 2; fg = C_DGREEN; lh = FONT_H * 2 + 8; }
        else if (starts(p, "## ")) { p += 3; bold = true; fg = C_DGREEN; y += 4; }
        else if (starts(p, "- "))  {
            p += 2; indent = 18;
            if (draw && cy + y - scroll > cy - lh && cy + y - scroll < cy + vh)
                gfx_disc(cx + 6, cy + y - scroll + 8, 3, C_GREEN);
        }
        int x = indent, cwid = FONT_W * scale;
        /* walk words; a word is either plain text or part of a [link](url) */
        const char *q = p;
        char url[URL_MAX]; url[0] = 0;
        const char *link_end = NULL;        /* where the link text ends (at ']') */
        while (q < eol) {
            if (!link_end && *q == '[') {   /* is this a link? find ](url) */
                const char *rb = q + 1; while (rb < eol && *rb != ']') rb++;
                if (rb + 1 < eol && rb[1] == '(') {
                    const char *rp = rb + 2; while (rp < eol && *rp != ')') rp++;
                    if (rp < eol) {
                        int n = (int)(rp - (rb + 2)); if (n > URL_MAX - 1) n = URL_MAX - 1;
                        memcpy(url, rb + 2, (size_t)n); url[n] = 0;
                        link_end = rb; q++; continue;
                    }
                }
            }
            if (link_end && q == link_end) {    /* skip "](url)" */
                while (q < eol && *q != ')') q++;
                if (q < eol) q++;
                link_end = NULL; url[0] = 0; continue;
            }
            if (*q == ' ') { x += cwid; q++; continue; }
            const char *we = q;
            while (we < eol && *we != ' ' && !(link_end && we == link_end) && !(!link_end && *we == '[' && we != q)) we++;
            int n = (int)(we - q), ww = n * cwid;
            if (x + ww > cw && x > indent) { x = indent; y += lh; }
            int sx = cx + x, sy = cy + y - scroll;
            bool vis = sy > cy - lh && sy < cy + vh;
            uint32_t col = link_end ? C_LINK : fg;
            if (draw && vis) {
                char word[128]; int m = n < 127 ? n : 127;
                memcpy(word, q, (size_t)m); word[m] = 0;
                gfx_text(sx, sy, word, col, TRANSPARENT, scale);
                if (bold) gfx_text(sx + 1, sy, word, col, TRANSPARENT, scale);
                if (link_end) gfx_fill(sx, sy + FONT_H * scale - 1, ww, 1, col);
            }
            if (link_end && nlinks < LINKS_MAX) {
                links[nlinks].x = sx - ox; links[nlinks].y = sy - oy;
                links[nlinks].w = ww + cwid; links[nlinks].h = FONT_H * scale + 4;
                copy(links[nlinks].url, url, URL_MAX); nlinks++;
            }
            x += ww; q = we;
        }
        y += lh;
        p = *eol ? eol + 1 : eol;
    }
    return y + 10;
}

/* ---------------------------------------------------------------- drawing */
static void button(int x, int y, int w, int h, const char *label, bool down, bool enabled) {
    gfx_bevel(x, y, w, h, enabled ? C_GREEN : RGB(0xA8, 0xC4, 0xAE), down);
    int tw = (int)strlen(label) * FONT_W;
    gfx_text(x + (w - tw) / 2 + (down ? 1 : 0), y + (h - FONT_H) / 2 + (down ? 1 : 0), label, 0xFFFFFF, TRANSPARENT, 1);
}

void browser_draw(int x, int y, int w, int h, bool focused) {
    last_w = w;
    view_h = h - TOOLBAR;
    /* page */
    gfx_fill(x, y + TOOLBAR, w, view_h, BROWSER_BG);
    gfx_set_clip(x, y + TOOLBAR, w - SCROLLW, view_h);
    content_h = render(x, y, x + MARGIN, y + TOOLBAR, w - SCROLLW - MARGIN * 2, view_h, true);
    gfx_set_clip(x, y, w, h);
    clamp_scroll();
    /* scroll column: up/down buttons and a thumb */
    int sx = x + w - SCROLLW;
    gfx_fill(sx, y + TOOLBAR, SCROLLW, view_h, RGB(0xE6, 0xF0, 0xE8));
    button(sx + 2, y + TOOLBAR + 2, SCROLLW - 4, 32, "^", pressed == BTN_UP, scroll > 0);
    button(sx + 2, y + TOOLBAR + 36, SCROLLW - 4, 32, "v", pressed == BTN_DOWN, scroll < content_h - view_h);
    int track = view_h - 76 - 40;          /* leave room for the resize grip */
    if (content_h > view_h && track > 20) {
        int th = track * view_h / content_h; if (th < 16) th = 16;
        int ty = y + TOOLBAR + 72 + (track - th) * scroll / (content_h - view_h);
        gfx_fill(sx + 8, ty, SCROLLW - 16, th, RGB(0x8F, 0xC9, 0x9F));
    }
    /* toolbar */
    gfx_fill(x, y, w, TOOLBAR, C_BAR);
    gfx_fill(x, y + TOOLBAR - 1, w, 1, RGB(0xA8, 0xC4, 0xAE));
    button(x + 6, y + 6, 34, 28, "<", pressed == BTN_BACK, hist_pos > 0);
    button(x + 44, y + 6, 34, 28, ">", pressed == BTN_FWD, hist_pos < hist_len - 1);
    button(x + 82, y + 6, 50, 28, "Home", pressed == BTN_HOME, true);
    int ax = x + 140, aw = w - 140 - 8;
    gfx_fill(ax, y + 6, aw, 28, 0xFFFFFF);
    gfx_rect(ax, y + 6, aw, 28, addr_edit && focused ? C_GREEN : RGB(0x8F, 0xA8, 0x95));
    if (addr_edit && focused) gfx_rect(ax + 1, y + 7, aw - 2, 26, C_GREEN);
    const char *shown = addr_edit ? addr : cur_url;
    int maxc = (aw - 16) / FONT_W, len = (int)strlen(shown);
    const char *vis = len > maxc ? shown + (len - maxc) : shown;   /* keep the end visible */
    gfx_text(ax + 8, y + 12, vis, C_TEXT, TRANSPARENT, 1);
    if (addr_edit && focused) {
        int cxp = ax + 8 + (int)strlen(vis) * FONT_W;
        gfx_fill(cxp, y + 11, 2, 18, C_TEXT);
    }
}

/* ---------------------------------------------------------------- input */
static int hit_button(int lx, int ly) {
    if (ly >= 6 && ly < 34) {
        if (lx >= 6 && lx < 40) return BTN_BACK;
        if (lx >= 44 && lx < 78) return BTN_FWD;
        if (lx >= 82 && lx < 132) return BTN_HOME;
    }
    if (lx >= last_w - SCROLLW) {
        if (ly >= TOOLBAR && ly < TOOLBAR + 36) return BTN_UP;
        if (ly >= TOOLBAR + 36 && ly < TOOLBAR + 70) return BTN_DOWN;
    }
    return -1;
}

void browser_click(int lx, int ly) {
    int b = hit_button(lx, ly);
    if (b >= 0) {
        pressed = b; addr_edit = false;
        switch (b) {
        case BTN_BACK: back(); break;
        case BTN_FWD:  fwd(); break;
        case BTN_HOME: go("leo://home"); break;
        case BTN_UP:   scroll -= view_h * 3 / 4; clamp_scroll(); break;
        case BTN_DOWN: scroll += view_h * 3 / 4; clamp_scroll(); break;
        }
        return;
    }
    if (ly < TOOLBAR) {                     /* address bar */
        if (lx >= 140) { addr_edit = true; addr[0] = 0; addr_len = 0; }
        return;
    }
    addr_edit = false;
    if (lx >= last_w - SCROLLW) {           /* tap on the scroll track: page up/down */
        int ty = TOOLBAR + 72 + (view_h - 116) * scroll / (content_h > view_h ? content_h - view_h : 1);
        scroll += ly < ty ? -view_h * 3 / 4 : view_h * 3 / 4; clamp_scroll();
        return;
    }
    pressing = true; moved = false;
    press_x = lx; press_y = ly; press_scroll = scroll;
}

void browser_drag(int lx, int ly) {
    (void)lx;
    if (!pressing) return;
    int dy = ly - press_y;
    if (dy > 6 || dy < -6) moved = true;
    if (moved) { scroll = press_scroll - dy; clamp_scroll(); }
}

void browser_release(void) {
    pressed = -1;
    if (!pressing) return;
    pressing = false;
    if (moved) return;
    for (int i = 0; i < nlinks; i++)        /* a tap: follow the link under it */
        if (press_x >= links[i].x - 2 && press_x < links[i].x + links[i].w &&
            press_y >= links[i].y - 2 && press_y < links[i].y + links[i].h) {
            char u[URL_MAX]; copy(u, links[i].url, URL_MAX);
            go(u); return;
        }
}

void browser_key(int c) {
    if (addr_edit) {
        if (c == '\n') { addr_edit = false; if (addr_len) go(addr); }
        else if (c == 27) addr_edit = false;
        else if (c == '\b') { if (addr_len) addr[--addr_len] = 0; }
        else if (c >= 32 && c < 127 && addr_len < URL_MAX - 1) { addr[addr_len++] = (char)c; addr[addr_len] = 0; }
        return;
    }
    switch (c) {
    case KEY_UP:   scroll -= 40; break;
    case KEY_DOWN: scroll += 40; break;
    case ' ':      scroll += view_h * 3 / 4; break;
    case '\b': case KEY_LEFT: back(); break;
    case KEY_RIGHT: fwd(); break;
    case 'h': case 'H': go("leo://home"); break;
    case 'l': case 'L': case '/': addr_edit = true; addr[0] = 0; addr_len = 0; break;
    case 'r': case 'R': load(cur_url); break;
    }
    clamp_scroll();
}

/* ---------------------------------------------------------------- icon */
void browser_icon(int x, int y) {
    gfx_disc(x + 16, y + 16, 15, RGB(0x1A, 0x5F, 0xC8));
    gfx_disc(x + 16, y + 16, 13, RGB(0x4A, 0x9B, 0xE8));
    /* green "continents" */
    gfx_disc(x + 11, y + 11, 5, C_GREEN);
    gfx_disc(x + 14, y + 15, 3, C_GREEN);
    gfx_disc(x + 21, y + 21, 5, C_GREEN);
    gfx_disc(x + 23, y + 10, 2, C_GREEN);
    /* grid lines */
    gfx_fill(x + 3, y + 16, 26, 1, RGB(0xD8, 0xEC, 0xFF));
    gfx_fill(x + 16, y + 3, 1, 26, RGB(0xD8, 0xEC, 0xFF));
}
