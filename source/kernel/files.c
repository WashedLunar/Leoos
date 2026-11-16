/* LeoOS - Files (a small File Explorer). Browses the RAM filesystem. */
#include "files.h"
#include "gui.h"
#include "gfx.h"
#include "ramfs.h"
#include "cpu.h"
#include "string.h"

#define TEXT_DARK RGB(0x10, 0x30, 0x18)
#define TOOL_Y 4
#define TOOL_H 28
#define PATH_Y 36
#define PATH_H 26
#define LIST_Y 68
#define ROW_H  28
#define STATUS_H 22

enum { BTN_UP, BTN_NEW, BTN_REN, BTN_DEL, BTN_COPY, BTN_PASTE, NBTNS };
enum { MODE_BROWSE, MODE_NEW, MODE_RENAME };

static const char *btn_label[NBTNS] = { "Up", "New folder", "Rename", "Delete", "Copy", "Paste" };
static const int btn_w[NBTNS] = { 40, 92, 68, 64, 52, 56 };

struct item { char name[FS_NAME_MAX]; bool is_dir; int size; };
static struct item items[FS_MAX_FILES];
static int nitems, selected = -1, scroll;
static char path[FS_PATH_MAX];
static char clip[FS_PATH_MAX];                  /* full path copied, or "" */
static int mode = MODE_BROWSE, pressed = -1;
static char input[FS_NAME_MAX];
static bool input_fresh;
static const char *msg;
static uint32_t msg_until;
static uint32_t last_click_ticks;
static int last_click_item = -1;

static void status(const char *m) { msg = m; msg_until = timer_ticks() + 3 * timer_hz(); }
static bool blink_on(void) { return (timer_ticks() / (timer_hz() / 2)) % 2 == 0; }

static void btn_rect(int i, int *x, int *y, int *w, int *h) {
    int x0 = 6;
    for (int j = 0; j < i; j++) x0 += btn_w[j] + 6;
    *x = x0; *y = TOOL_Y; *w = btn_w[i]; *h = TOOL_H;
}

static int cmp_items(const struct item *a, const struct item *b) {
    if (a->is_dir != b->is_dir) return a->is_dir ? -1 : 1;
    return strcmp(a->name, b->name);
}

static void collect(const char *name, bool is_dir, int size, void *ctx) {
    (void)ctx;
    if (nitems >= FS_MAX_FILES) return;
    strcpy(items[nitems].name, name);
    items[nitems].is_dir = is_dir;
    items[nitems].size = size;
    nitems++;
}

static void refresh(void) {
    char keep[FS_NAME_MAX];
    keep[0] = 0;
    if (selected >= 0 && selected < nitems) strcpy(keep, items[selected].name);
    nitems = 0;
    fs_list(path, collect, NULL);
    for (int i = 0; i < nitems; i++)
        for (int j = i + 1; j < nitems; j++)
            if (cmp_items(&items[i], &items[j]) > 0) {
                struct item t = items[i]; items[i] = items[j]; items[j] = t;
            }
    selected = -1;
    if (keep[0])
        for (int i = 0; i < nitems; i++)
            if (!strcmp(items[i].name, keep)) { selected = i; break; }
    if (scroll > nitems) scroll = 0;
    if (selected >= 0 && selected < scroll) scroll = selected;
}

void files_init(void) {
    strcpy(path, "/");
    clip[0] = 0;
    mode = MODE_BROWSE;
    selected = -1;
    scroll = 0;
    msg = NULL;
    last_click_item = -1;
    refresh();
}

static void go_up(void) {
    if (!strcmp(path, "/")) return;
    int n = (int)strlen(path);
    while (n > 1 && path[n - 1] != '/') n--;
    if (n <= 1) strcpy(path, "/");
    else { path[n - 1] = 0; if (!path[0]) strcpy(path, "/"); }
    selected = -1; scroll = 0;
    refresh();
}

static void join_here(char *buf, const char *name) {
    fs_join(buf, FS_PATH_MAX, path, name);
}

static void open_item(int i) {
    if (i < 0 || i >= nitems) return;
    char full[FS_PATH_MAX];
    join_here(full, items[i].name);
    if (items[i].is_dir) {
        strcpy(path, full);
        selected = -1; scroll = 0;
        refresh();
    } else {
        notepad_open(full);
    }
}

static void unique_copy_name(const char *src, char *out) {
    /* "welcome.txt" -> "welcome-copy.txt", then "welcome-copy2.txt", ... */
    char base[FS_NAME_MAX], ext[FS_NAME_MAX];
    int dot = -1;
    for (int i = 0; src[i]; i++) if (src[i] == '.' && i > 0) dot = i;
    if (dot > 0) {
        int i = 0; for (; i < dot && i < FS_NAME_MAX - 1; i++) base[i] = src[i];
        base[i] = 0;
        strcpy(ext, src + dot);
    } else { strcpy(base, src); ext[0] = 0; }
    /* first try the original name, then -copy, -copy2, ... */
    strcpy(out, src);
    char trial[FS_PATH_MAX];
    for (int n = 0; n < 20; n++) {
        if (n == 0) strcpy(out, src);
        else {
            char suf[16]; int k = 0;
            const char *p = (n == 1) ? "-copy" : "-copy";
            for (; p[k]; k++) suf[k] = p[k];
            if (n >= 2) {
                /* append the number */
                int v = n, d[4], nd = 0;
                while (v) { d[nd++] = v % 10; v /= 10; }
                for (int j = nd - 1; j >= 0; j--) suf[k++] = (char)('0' + d[j]);
            }
            suf[k] = 0;
            /* base + suf + ext must fit in FS_NAME_MAX-1 */
            int need = (int)strlen(base) + k + (int)strlen(ext);
            if (need >= FS_NAME_MAX) {
                int keep = FS_NAME_MAX - 1 - k - (int)strlen(ext);
                if (keep < 1) keep = 1;
                char tmp[FS_NAME_MAX];
                int t = 0; for (; t < keep && base[t]; t++) tmp[t] = base[t];
                tmp[t] = 0;
                strcpy(out, tmp); strcpy(out + strlen(out), suf); strcpy(out + strlen(out), ext);
            } else {
                strcpy(out, base); strcpy(out + strlen(out), suf); strcpy(out + strlen(out), ext);
            }
        }
        join_here(trial, out);
        if (fs_find(trial) < 0) return;
    }
}

static void do_new_folder(void) {
    if (!fs_valid_name(input)) { status("Bad name: use a-z 0-9 . _ -"); return; }
    char full[FS_PATH_MAX];
    join_here(full, input);
    int r = fs_mkdir(full);
    if (r == -1) { status("Disk full"); return; }
    if (r == -3) { status("Already exists"); return; }
    if (r < 0) { status("Could not create folder"); return; }
    mode = MODE_BROWSE;
    refresh();
    for (int i = 0; i < nitems; i++) if (!strcmp(items[i].name, input)) selected = i;
    status("Folder created");
}

static void do_rename(void) {
    if (selected < 0 || selected >= nitems) { mode = MODE_BROWSE; return; }
    if (!fs_valid_name(input)) { status("Bad name: use a-z 0-9 . _ -"); return; }
    char src[FS_PATH_MAX], dst[FS_PATH_MAX];
    join_here(src, items[selected].name);
    join_here(dst, input);
    int r = fs_rename(src, dst);
    if (r == -3) { status("Already exists"); return; }
    if (r < 0) { status("Could not rename"); return; }
    mode = MODE_BROWSE;
    refresh();
    for (int i = 0; i < nitems; i++) if (!strcmp(items[i].name, input)) selected = i;
    status("Renamed");
}

static void do_delete(void) {
    if (selected < 0 || selected >= nitems) { status("Select a file first"); return; }
    char full[FS_PATH_MAX];
    join_here(full, items[selected].name);
    int r = fs_remove(full);
    if (r == -2) { status("Folder is not empty"); return; }
    if (r < 0) { status("Could not delete"); return; }
    selected = -1;
    refresh();
    status("Deleted");
}

static void do_copy(void) {
    if (selected < 0 || selected >= nitems) { status("Select a file first"); return; }
    join_here(clip, items[selected].name);
    status("Copied");
}

static void do_paste(void) {
    if (!clip[0]) { status("Clipboard is empty"); return; }
    int src = fs_find(clip);
    if (src < 0) { status("Original is gone"); clip[0] = 0; return; }
    char base[FS_NAME_MAX], dest[FS_PATH_MAX];
    strcpy(base, fs_name(src));
    join_here(dest, base);
    /* pasting into the same folder (or a clash) gets a -copy name */
    if (fs_find(dest) >= 0) {
        unique_copy_name(base, base);
        join_here(dest, base);
    }
    int r = fs_copy(clip, dest);
    if (r == -6 || r == -1) { status("Disk full"); return; }
    if (r < 0) { status("Could not paste"); return; }
    refresh();
    for (int i = 0; i < nitems; i++) if (!strcmp(items[i].name, base)) selected = i;
    status("Pasted");
}

static void start_dialog(int m) {
    if (m == MODE_RENAME) {
        if (selected < 0 || selected >= nitems) { status("Select a file first"); return; }
        strcpy(input, items[selected].name);
    } else {
        strcpy(input, "New-folder");
        char trial[FS_PATH_MAX];
        int n = 2;
        join_here(trial, input);
        while (fs_find(trial) >= 0 && n < 20) {
            strcpy(input, "New-folder");
            char num[8]; utoa((uint32_t)n, num, 10);
            strcpy(input + strlen(input), num);
            join_here(trial, input);
            n++;
        }
    }
    input_fresh = true;
    mode = m;
}

static void draw_folder_icon(int x, int y) {
    gfx_fill(x + 2, y + 10, 20, 12, RGB(0xE6, 0xC0, 0x4A));
    gfx_fill(x + 2, y + 6, 9, 6, RGB(0xD4, 0xA8, 0x2E));
    gfx_fill(x + 2, y + 10, 20, 3, RGB(0xF5, 0xD0, 0x6A));
    gfx_rect(x + 2, y + 6, 20, 16, RGB(0x8A, 0x6A, 0x18));
}

static void draw_file_icon(int x, int y) {
    gfx_fill(x + 5, y + 4, 14, 18, 0xFFFFFF);
    gfx_rect(x + 5, y + 4, 14, 18, RGB(0x70, 0x70, 0x70));
    gfx_fill(x + 14, y + 4, 5, 5, RGB(0xE0, 0xE0, 0xE0));
    gfx_line(x + 14, y + 4, x + 18, y + 8, 1, RGB(0x70, 0x70, 0x70));
    for (int i = 0; i < 3; i++) gfx_fill(x + 8, y + 12 + i * 3, 8, 1, RGB(0x50, 0x80, 0x60));
}

void files_icon(int x, int y) {
    gfx_fill(x + 4, y + 12, 24, 16, RGB(0xE6, 0xC0, 0x4A));
    gfx_fill(x + 4, y + 8, 11, 8, RGB(0xD4, 0xA8, 0x2E));
    gfx_fill(x + 4, y + 12, 24, 4, RGB(0xF5, 0xD0, 0x6A));
    gfx_rect(x + 4, y + 8, 24, 20, RGB(0x8A, 0x6A, 0x18));
}

static void dialog_rect(int w, int h, int *dx, int *dy, int *dw, int *dh) {
    *dw = w - 40 < 340 ? w - 40 : 340;
    *dh = 120;
    *dx = (w - *dw) / 2; *dy = LIST_Y + (h - LIST_Y - STATUS_H - *dh) / 2;
    if (*dy < LIST_Y) *dy = LIST_Y;
}

static void dlg_btn(int i, int dx, int dy, int dw, int dh, int *bx, int *by) {
    *bx = dx + dw - (2 - i) * 88; *by = dy + dh - 36;
}

void files_draw(int x, int y, int w, int h, bool focused) {
    (void)focused;
    /* Re-read the folder every frame so mkdir/write in the Terminal show up. */
    if (mode == MODE_BROWSE) refresh();
    /* toolbar */
    gfx_fill(x, y, w, LIST_Y, RGB(0xE4, 0xF2, 0xE7));
    gfx_fill(x, y + LIST_Y - 1, w, 1, RGB(0xA8, 0xC8, 0xB0));
    for (int i = 0; i < NBTNS; i++) {
        int bx, by, bw, bh; btn_rect(i, &bx, &by, &bw, &bh);
        bool sunken = pressed == i;
        uint32_t face = 0xF2F2F2;
        if (i == BTN_NEW) face = RGB(0x3B, 0xB0, 0x5A);
        gfx_bevel(x + bx, y + by, bw, bh, face, sunken);
        uint32_t fg = i == BTN_NEW ? 0xFFFFFF : TEXT_DARK;
        if (i == BTN_PASTE && !clip[0]) fg = RGB(0x90, 0xA8, 0x98);
        gfx_text(x + bx + (bw - (int)strlen(btn_label[i]) * FONT_W) / 2, y + by + 6, btn_label[i], fg, TRANSPARENT, 1);
    }
    /* path bar */
    gfx_text(x + 8, y + PATH_Y + 5, "Path", TEXT_DARK, TRANSPARENT, 1);
    int px = 48, pw = w - 58;
    gfx_fill(x + px, y + PATH_Y, pw, 22, 0xFFFFFF);
    gfx_rect(x + px, y + PATH_Y, pw, 22, RGB(0x6C, 0x9C, 0x78));
    gfx_set_clip(x + px + 4, y + PATH_Y, pw - 8, 22);
    gfx_text(x + px + 6, y + PATH_Y + 3, path, TEXT_DARK, TRANSPARENT, 1);
    gfx_set_clip(x, y, w, h);

    /* list */
    int list_h = h - LIST_Y - STATUS_H;
    if (list_h < ROW_H) list_h = ROW_H;
    int rows = list_h / ROW_H;
    if (rows < 1) rows = 1;
    gfx_fill(x, y + LIST_Y, w, list_h, 0xFFFFFF);
    if (selected >= 0 && selected < scroll) scroll = selected;
    if (selected >= scroll + rows) scroll = selected - rows + 1;
    if (scroll < 0) scroll = 0;
    for (int r = 0; r < rows; r++) {
        int i = scroll + r;
        int ry = y + LIST_Y + r * ROW_H;
        if (i >= nitems) continue;
        bool sel = i == selected;
        if (sel) gfx_fill(x + 4, ry + 1, w - 8, ROW_H - 2, RGB(0xC6, 0xEE, 0xD0));
        else if (r % 2) gfx_fill(x + 4, ry + 1, w - 8, ROW_H - 2, RGB(0xF4, 0xFA, 0xF5));
        if (items[i].is_dir) draw_folder_icon(x + 8, ry);
        else draw_file_icon(x + 8, ry);
        gfx_text(x + 36, ry + 6, items[i].name, TEXT_DARK, TRANSPARENT, 1);
        if (items[i].is_dir) {
            gfx_text(x + w - 8 - 6 * FONT_W, ry + 6, "folder", RGB(0x60, 0x80, 0x68), TRANSPARENT, 1);
        } else {
            char num[16]; utoa((uint32_t)items[i].size, num, 10);
            strcpy(num + strlen(num), " B");
            gfx_text(x + w - 8 - (int)strlen(num) * FONT_W, ry + 6, num, RGB(0x60, 0x80, 0x68), TRANSPARENT, 1);
        }
    }

    /* status bar */
    int sy = y + h - STATUS_H;
    gfx_fill(x, sy, w, STATUS_H, RGB(0xE4, 0xF2, 0xE7));
    gfx_fill(x, sy, w, 1, RGB(0xA8, 0xC8, 0xB0));
    char buf[64], num[16];
    utoa((uint32_t)nitems, num, 10);
    strcpy(buf, num); strcpy(buf + strlen(buf), " item");
    if (nitems != 1) strcpy(buf + strlen(buf), "s");
    if (msg && timer_ticks() < msg_until) {
        strcpy(buf + strlen(buf), "  -  ");
        strcpy(buf + strlen(buf), msg);
    }
    gfx_text(x + 8, sy + 4, buf, TEXT_DARK, TRANSPARENT, 1);

    if (mode == MODE_BROWSE) return;

    /* name dialog */
    gfx_shade(x, y + LIST_Y, w, h - LIST_Y - STATUS_H, 60);
    int dx, dy, dw, dh;
    dialog_rect(w, h, &dx, &dy, &dw, &dh);
    gfx_set_clip(x + dx, y + dy, dw, dh);
    gfx_shade(x + dx + 4, y + dy + 4, dw, dh, 80);
    gfx_fill(x + dx, y + dy, dw, dh, RGB(0xF4, 0xFA, 0xF5));
    gfx_rect(x + dx, y + dy, dw, dh, RGB(0x0E, 0x4D, 0x22));
    gfx_gradient(x + dx + 1, y + dy + 1, dw - 2, 24, RGB(0x2F, 0xA8, 0x4F), RGB(0x17, 0x6E, 0x32));
    gfx_text(x + dx + 10, y + dy + 5, mode == MODE_NEW ? "New folder" : "Rename", 0xFFFFFF, TRANSPARENT, 1);
    gfx_text(x + dx + 12, y + dy + 34, "Name:", TEXT_DARK, TRANSPARENT, 1);
    gfx_fill(x + dx + 64, y + dy + 30, dw - 76, 24, 0xFFFFFF);
    gfx_rect(x + dx + 64, y + dy + 30, dw - 76, 24, RGB(0x6C, 0x9C, 0x78));
    if (input_fresh) {
        gfx_fill(x + dx + 69, y + dy + 33, (int)strlen(input) * FONT_W + 2, FONT_H + 2, RGB(0x2F, 0xA8, 0x4F));
        gfx_text(x + dx + 70, y + dy + 34, input, 0xFFFFFF, TRANSPARENT, 1);
    } else gfx_text(x + dx + 70, y + dy + 34, input, TEXT_DARK, TRANSPARENT, 1);
    if (blink_on()) gfx_fill(x + dx + 70 + (int)strlen(input) * FONT_W, y + dy + 34, 2, FONT_H, TEXT_DARK);
    for (int i = 0; i < 2; i++) {
        int bx, by; dlg_btn(i, dx, dy, dw, dh, &bx, &by);
        gfx_bevel(x + bx, y + by, 80, 28, i == 0 ? RGB(0x3B, 0xB0, 0x5A) : 0xF2F2F2, pressed == 10 + i);
        const char *lab = i == 0 ? (mode == MODE_NEW ? "Create" : "Rename") : "Cancel";
        gfx_text(x + bx + (80 - (int)strlen(lab) * FONT_W) / 2, y + by + 6, lab,
                 i == 0 ? 0xFFFFFF : TEXT_DARK, TRANSPARENT, 1);
    }
    gfx_set_clip(x, y, w, h);
}

void files_release(void) { pressed = -1; }

static int item_at(int lx, int ly, int w, int h) {
    (void)lx; (void)w;
    if (ly < LIST_Y || ly >= h - STATUS_H) return -1;
    int row = (ly - LIST_Y) / ROW_H;
    int i = scroll + row;
    return (i >= 0 && i < nitems) ? i : -1;
}

void files_click(int lx, int ly) {
    int w, h; gui_client_size(APP_FILES, &w, &h);
    if (mode != MODE_BROWSE) {
        int dx, dy, dw, dh;
        dialog_rect(w, h, &dx, &dy, &dw, &dh);
        for (int i = 0; i < 2; i++) {
            int bx, by; dlg_btn(i, dx, dy, dw, dh, &bx, &by);
            if (lx >= bx && ly >= by && lx < bx + 80 && ly < by + 28) {
                pressed = 10 + i;
                if (i == 0) { if (mode == MODE_NEW) do_new_folder(); else do_rename(); }
                else mode = MODE_BROWSE;
                return;
            }
        }
        return;
    }
    for (int i = 0; i < NBTNS; i++) {
        int bx, by, bw, bh; btn_rect(i, &bx, &by, &bw, &bh);
        if (lx >= bx && ly >= by && lx < bx + bw && ly < by + bh) {
            pressed = i;
            if (i == BTN_UP) go_up();
            else if (i == BTN_NEW) start_dialog(MODE_NEW);
            else if (i == BTN_REN) start_dialog(MODE_RENAME);
            else if (i == BTN_DEL) do_delete();
            else if (i == BTN_COPY) do_copy();
            else if (i == BTN_PASTE) do_paste();
            return;
        }
    }
    int i = item_at(lx, ly, w, h);
    if (i < 0) { selected = -1; return; }
    uint32_t now = timer_ticks();
    bool dbl = (i == last_click_item && now - last_click_ticks < timer_hz() / 2);
    last_click_item = i;
    last_click_ticks = now;
    selected = i;
    if (dbl) { last_click_item = -1; open_item(i); }
}

void files_key(int c) {
    if (mode != MODE_BROWSE) {
        if (input_fresh && c != '\n' && c != 27 && c < 0x100) input[0] = 0;
        if (c != '\n') input_fresh = false;
        int n = (int)strlen(input);
        if (c == '\n') { if (mode == MODE_NEW) do_new_folder(); else do_rename(); }
        else if (c == 27) mode = MODE_BROWSE;
        else if (c == '\b') { if (n) input[n - 1] = 0; }
        else if (n < FS_NAME_MAX - 1 && c < 0x100 &&
                 ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                  c == '.' || c == '_' || c == '-')) {
            input[n] = (char)c; input[n + 1] = 0;
        }
        return;
    }
    if (c == KEY_UP) {
        if (selected > 0) selected--;
        else if (nitems) selected = 0;
    } else if (c == KEY_DOWN) {
        if (selected < nitems - 1) selected++;
        else if (selected < 0 && nitems) selected = 0;
    } else if (c == '\n') open_item(selected);
    else if (c == '\b') go_up();
    else if (c == 27) { /* ignore */ }
    else if (c == 127) do_delete();
}

