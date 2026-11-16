/* LeoOS - in-memory filesystem with folders (new in 0.4). Lost on restart. */
#include "ramfs.h"
#include "string.h"

struct node {
    bool used, is_dir;
    int parent;                     /* -1 for the root slot (always slot 0) */
    char name[FS_NAME_MAX];         /* basename; empty for the root */
    int len;
    char data[FS_DATA_MAX];
};
static struct node files[FS_MAX_FILES];
static int cwd;                     /* slot of the current directory */
static char cwd_buf[FS_PATH_MAX];

int fs_count(void) { return FS_MAX_FILES; }
bool fs_used(int i) { return i >= 0 && i < FS_MAX_FILES && files[i].used; }
bool fs_slot_is_dir(int i) { return fs_used(i) && files[i].is_dir; }
const char *fs_name(int i) { return files[i].name; }
int fs_parent(int i) { return files[i].parent; }
int fs_size(int i) { return files[i].len; }

int fs_files(void) {
    int n = 0;
    for (int i = 0; i < FS_MAX_FILES; i++)
        if (files[i].used && !files[i].is_dir) n++;
    return n;
}

bool fs_valid_name(const char *s) {
    if (!s) return false;
    int n = 0;
    for (; s[n]; n++) {
        char c = s[n];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-'))
            return false;
    }
    if (n == 0 || n >= FS_NAME_MAX) return false;
    if (!strcmp(s, ".") || !strcmp(s, "..")) return false;
    return true;
}

bool fs_valid_path(const char *path) {
    if (!path || (int)strlen(path) >= FS_PATH_MAX) return false;
    const char *p = path;
    int n = 0;
    char name[FS_NAME_MAX];
    if (*p == '/') p++;
    if (!*p) return true;                          /* "/" or "" */
    while (*p) {
        n = 0;
        while (*p && *p != '/') {
            if (n >= FS_NAME_MAX - 1) return false;
            name[n++] = *p++;
        }
        name[n] = 0;
        if (!n) { if (*p == '/') { p++; continue; } return false; }
        if (strcmp(name, ".") && strcmp(name, "..") && !fs_valid_name(name)) return false;
        if (*p == '/') p++;
    }
    return true;
}

static int alloc_slot(void) {
    for (int i = 1; i < FS_MAX_FILES; i++) if (!files[i].used) return i;
    return -1;
}

static int find_child(int parent, const char *name) {
    for (int i = 0; i < FS_MAX_FILES; i++)
        if (files[i].used && files[i].parent == parent && !strcmp(files[i].name, name))
            return i;
    return -1;
}

/* Walk parents to build an absolute path into buf. Returns the length, or -1. */
int fs_path(int i, char *buf, int max) {
    if (!buf || max < 2 || !fs_used(i)) return -1;
    if (i == 0) { strcpy(buf, "/"); return 1; }
    int stack[FS_MAX_FILES], n = 0, s = i;
    while (s > 0 && files[s].used && n < FS_MAX_FILES) {
        stack[n++] = s;
        s = files[s].parent;
    }
    int pos = 0;
    buf[0] = 0;
    for (int k = n - 1; k >= 0; k--) {
        const char *nm = files[stack[k]].name;
        int need = 1 + (int)strlen(nm);
        if (pos + need >= max) { buf[pos] = 0; return -1; }
        buf[pos++] = '/';
        for (int c = 0; nm[c]; c++) buf[pos++] = nm[c];
    }
    buf[pos] = 0;
    return pos;
}

const char *fs_cwd(void) {
    if (cwd < 0 || cwd >= FS_MAX_FILES || !files[cwd].used || !files[cwd].is_dir) cwd = 0;
    fs_path(cwd, cwd_buf, FS_PATH_MAX);
    return cwd_buf;
}

/* Pull the next component of *pp into name. Returns 1, 0 at end, -1 if too long. */
static int next_comp(const char **pp, char *name) {
    const char *p = *pp;
    while (*p == '/') p++;
    if (!*p) { *pp = p; return 0; }
    int n = 0;
    while (*p && *p != '/') {
        if (n >= FS_NAME_MAX - 1) return -1;
        name[n++] = *p++;
    }
    name[n] = 0;
    *pp = p;
    return 1;
}

/* Resolve path to a slot.
 *  >=0  found
 *  -1   a component is missing (and it is not only the last one, unless partial is false)
 *  -2   bad path
 *  -3   every component but the last exists; *parent_out is the parent, base_out is the last name
 */
static int resolve(const char *path, int *parent_out, char *base_out, bool partial) {
    if (!path || (int)strlen(path) >= FS_PATH_MAX) return -2;
    int cur = (*path == '/') ? 0 : cwd;
    if (cwd < 0 || cwd >= FS_MAX_FILES || !files[cwd].used) cur = (*path == '/') ? 0 : 0;
    if (!*path) {
        if (parent_out) *parent_out = (cur == 0) ? -1 : files[cur].parent;
        if (base_out) { if (cur == 0) base_out[0] = 0; else strcpy(base_out, files[cur].name); }
        return cur;
    }
    const char *p = path;
    char name[FS_NAME_MAX];
    int got;
    int last_parent = (cur == 0) ? -1 : files[cur].parent;
    while ((got = next_comp(&p, name)) == 1) {
        if (!strcmp(name, ".")) continue;
        if (!strcmp(name, "..")) {
            if (cur != 0) cur = files[cur].parent;
            last_parent = (cur == 0) ? -1 : files[cur].parent;
            if (base_out) { if (cur == 0) base_out[0] = 0; else strcpy(base_out, files[cur].name); }
            continue;
        }
        if (!fs_valid_name(name)) return -2;
        int child = find_child(cur, name);
        if (child < 0) {
            if (!partial) return -1;
            char tmp[FS_NAME_MAX];
            const char *q = p;
            int more = next_comp(&q, tmp);
            if (more == 1) return -1;              /* a middle component is missing */
            if (more < 0) return -2;
            if (parent_out) *parent_out = cur;
            if (base_out) strcpy(base_out, name);
            return -3;
        }
        last_parent = cur;
        cur = child;
        if (base_out) strcpy(base_out, name);
    }
    if (got < 0) return -2;
    if (parent_out) *parent_out = last_parent;
    if (base_out && cur == 0) base_out[0] = 0;
    return cur;
}

int fs_find(const char *path) {
    int r = resolve(path, NULL, NULL, false);
    return r >= 0 ? r : -1;
}

int fs_is_dir(const char *path) {
    int i = resolve(path, NULL, NULL, false);
    if (i < 0) return -1;
    return files[i].is_dir ? 1 : 0;
}

int fs_join(char *buf, int max, const char *dir, const char *name) {
    if (!buf || max < 2 || !name) return -1;
    char abs[FS_PATH_MAX];
    const char *base;
    if (!dir || !dir[0] || !strcmp(dir, "/")) base = "";
    else {
        int d = resolve(dir, NULL, NULL, false);
        if (d >= 0) { fs_path(d, abs, FS_PATH_MAX); base = abs; }
        else {
            /* use the string as given, so callers can join against a known path */
            if (dir[0] == '/') base = dir;
            else {
                const char *c = fs_cwd();
                int n = (int)strlen(c);
                if (n + 1 + (int)strlen(dir) >= FS_PATH_MAX) return -1;
                strcpy(abs, c);
                if (n > 1) { abs[n] = '/'; strcpy(abs + n + 1, dir); }
                else strcpy(abs + n, dir);
                base = abs;
            }
        }
    }
    int n = (int)strlen(base);
    int need = (n <= 1 ? 1 : n + 1) + (int)strlen(name);
    if (need >= max) return -1;
    if (n <= 1) { buf[0] = '/'; strcpy(buf + 1, name); }
    else { strcpy(buf, base); buf[n] = '/'; strcpy(buf + n + 1, name); }
    return (int)strlen(buf);
}

static void fix_cwd(void) {
    if (cwd < 0 || cwd >= FS_MAX_FILES || !files[cwd].used || !files[cwd].is_dir) cwd = 0;
}

int fs_chdir(const char *path) {
    if (!fs_valid_path(path) && path && path[0]) return -3;
    int i = resolve(path ? path : "", NULL, NULL, false);
    if (i == -2) return -3;
    if (i < 0) return -1;
    if (!files[i].is_dir) return -2;
    cwd = i;
    return 0;
}

int fs_read(const char *path, char *buf, int max) {
    int i = resolve(path, NULL, NULL, false);
    if (i < 0) return -1;
    if (files[i].is_dir) return -1;
    int n = files[i].len < max ? files[i].len : max;
    if (buf && n > 0) memcpy(buf, files[i].data, (size_t)n);
    return n;
}

int fs_write(const char *path, const char *data, int len) {
    int parent;
    char base[FS_NAME_MAX];
    int i = resolve(path, &parent, base, true);
    if (i == -2 || !fs_valid_path(path)) return -2;
    if (i >= 0) {
        if (files[i].is_dir) return -4;
        if (len > FS_DATA_MAX) len = FS_DATA_MAX;
        if (data && len > 0) memcpy(files[i].data, data, (size_t)len);
        files[i].len = len < 0 ? 0 : len;
        return 0;
    }
    if (i != -3) return -3;                        /* missing parent */
    if (parent < 0 || !files[parent].used || !files[parent].is_dir) return -3;
    if (!fs_valid_name(base)) return -2;
    int s = alloc_slot();
    if (s < 0) return -1;
    if (len > FS_DATA_MAX) len = FS_DATA_MAX;
    if (len < 0) len = 0;
    files[s].used = true;
    files[s].is_dir = false;
    files[s].parent = parent;
    strcpy(files[s].name, base);
    if (data && len > 0) memcpy(files[s].data, data, (size_t)len);
    files[s].len = len;
    return 0;
}

int fs_mkdir(const char *path) {
    int parent;
    char base[FS_NAME_MAX];
    int i = resolve(path, &parent, base, true);
    if (i == -2 || !fs_valid_path(path)) return -2;
    if (i >= 0) return -3;                         /* already exists (file or dir) */
    if (i != -3) return -4;
    if (parent < 0 || !files[parent].used || !files[parent].is_dir) return -4;
    if (!fs_valid_name(base)) return -2;
    int s = alloc_slot();
    if (s < 0) return -1;
    files[s].used = true;
    files[s].is_dir = true;
    files[s].parent = parent;
    strcpy(files[s].name, base);
    files[s].len = 0;
    return 0;
}

static bool dir_empty(int i) {
    for (int j = 0; j < FS_MAX_FILES; j++)
        if (files[j].used && files[j].parent == i) return false;
    return true;
}

int fs_remove(const char *path) {
    int i = resolve(path, NULL, NULL, false);
    if (i < 0) return -1;
    if (i == 0) return -3;
    if (files[i].is_dir && !dir_empty(i)) return -2;
    /* if we deleted the cwd (or an ancestor), snap back to root */
    int c = cwd;
    while (c > 0) {
        if (c == i) { cwd = 0; break; }
        c = files[c].parent;
    }
    files[i].used = false;
    files[i].is_dir = false;
    files[i].len = 0;
    files[i].name[0] = 0;
    fix_cwd();
    return 0;
}

/* True if `anc` is slot `desc` or a parent of it. */
static bool is_ancestor(int anc, int desc) {
    int s = desc;
    while (s >= 0) {
        if (s == anc) return true;
        if (s == 0) break;
        s = files[s].parent;
    }
    return false;
}

/* Destination for rename/copy: if `newp` exists and is a directory, the
 * real destination is that directory plus the source's basename.
 * Returns dest slot if it already exists, -3 if only the last name is missing
 * (parent/base filled), or a negative error. */
static int resolve_dest(const char *dst, const char *src_base, int *parent, char *base) {
    int i = resolve(dst, parent, base, true);
    if (i == -2) return -2;
    if (i >= 0 && files[i].is_dir) {
        /* move/copy *into* this directory */
        int child = find_child(i, src_base);
        strcpy(base, src_base);
        *parent = i;
        if (child >= 0) return child;
        return -3;
    }
    if (i >= 0) return i;                          /* existing file (or non-dir) */
    if (i != -3) return -4;
    if (*parent < 0 || !files[*parent].used || !files[*parent].is_dir) return -4;
    if (!fs_valid_name(base)) return -2;
    return -3;
}

int fs_rename(const char *old, const char *newp) {
    int src = resolve(old, NULL, NULL, false);
    if (src == -2) return -2;
    if (src < 0) return -1;
    if (src == 0) return -5;
    char src_base[FS_NAME_MAX];
    strcpy(src_base, files[src].name);
    int parent;
    char base[FS_NAME_MAX];
    int dest = resolve_dest(newp, src_base, &parent, base);
    if (dest == -2) return -2;
    if (dest == -4) return -4;
    if (dest >= 0) {
        if (dest == src) return 0;                 /* same place */
        return -3;                                 /* would overwrite */
    }
    /* dest == -3: parent/base is the new home. Don't move a dir into itself. */
    if (files[src].is_dir && is_ancestor(src, parent)) return -5;
    if (!fs_valid_name(base)) return -2;
    files[src].parent = parent;
    strcpy(files[src].name, base);
    fix_cwd();
    return 0;
}

static int copy_into(int src, int dest_parent, const char *base) {
    int existing = find_child(dest_parent, base);
    if (files[src].is_dir) {
        int d = existing;
        if (d >= 0) {
            if (!files[d].is_dir) return -3;
        } else {
            d = alloc_slot();
            if (d < 0) return -6;
            files[d].used = true;
            files[d].is_dir = true;
            files[d].parent = dest_parent;
            strcpy(files[d].name, base);
            files[d].len = 0;
        }
        for (int i = 0; i < FS_MAX_FILES; i++) {
            if (!files[i].used || files[i].parent != src) continue;
            int r = copy_into(i, d, files[i].name);
            if (r < 0) return r;
        }
        return 0;
    }
    if (existing >= 0) {
        if (files[existing].is_dir) return -3;
        memcpy(files[existing].data, files[src].data, (size_t)files[src].len);
        files[existing].len = files[src].len;
        return 0;
    }
    int d = alloc_slot();
    if (d < 0) return -6;
    files[d].used = true;
    files[d].is_dir = false;
    files[d].parent = dest_parent;
    strcpy(files[d].name, base);
    files[d].len = files[src].len;
    if (files[src].len > 0) memcpy(files[d].data, files[src].data, (size_t)files[src].len);
    return 0;
}

int fs_copy(const char *src_path, const char *dst) {
    int src = resolve(src_path, NULL, NULL, false);
    if (src == -2) return -2;
    if (src < 0) return -1;
    if (src == 0) return -5;                       /* don't copy the root itself */
    char src_base[FS_NAME_MAX];
    strcpy(src_base, files[src].name);
    int parent;
    char base[FS_NAME_MAX];
    int dest = resolve_dest(dst, src_base, &parent, base);
    if (dest == -2) return -2;
    if (dest == -4) return -4;
    if (dest >= 0 && dest == src) return -5;
    if (files[src].is_dir && dest >= 0 && files[dest].is_dir && is_ancestor(src, dest)) return -5;
    if (files[src].is_dir && dest < 0 && is_ancestor(src, parent)) return -5;
    /* copying a file onto an existing file overwrites; a dir onto a file fails */
    if (dest >= 0 && files[src].is_dir && !files[dest].is_dir) return -3;
    if (dest >= 0 && !files[src].is_dir && files[dest].is_dir) {
        /* resolve_dest already would have placed us inside the dir; this is a name clash */
        return -3;
    }
    if (dest >= 0 && !files[src].is_dir && !files[dest].is_dir) {
        memcpy(files[dest].data, files[src].data, (size_t)files[src].len);
        files[dest].len = files[src].len;
        return 0;
    }
    if (dest >= 0 && files[src].is_dir && files[dest].is_dir) {
        /* copy contents into the existing directory */
        for (int i = 0; i < FS_MAX_FILES; i++) {
            if (!files[i].used || files[i].parent != src) continue;
            int r = copy_into(i, dest, files[i].name);
            if (r < 0) return r;
        }
        return 0;
    }
    /* dest == -3: create under parent/base */
    if (!fs_valid_name(base)) return -2;
    return copy_into(src, parent, base);
}

static int dir_slot(const char *dir) {
    int i = resolve(dir ? dir : "", NULL, NULL, false);
    if (i < 0 || !files[i].is_dir) return -1;
    return i;
}

int fs_list(const char *dir, void (*cb)(const char *name, bool is_dir, int size, void *ctx), void *ctx) {
    int d = dir_slot(dir);
    if (d < 0) return -1;
    int n = 0;
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (!files[i].used || files[i].parent != d) continue;
        if (cb) cb(files[i].name, files[i].is_dir, files[i].is_dir ? 0 : files[i].len, ctx);
        n++;
    }
    return n;
}

int fs_dir_count(const char *dir) { return fs_list(dir, NULL, NULL); }

int fs_dir_get(const char *dir, int index, char *name, int namemax, bool *is_dir, int *size) {
    int d = dir_slot(dir);
    if (d < 0 || index < 0) return -1;
    int n = 0;
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (!files[i].used || files[i].parent != d) continue;
        if (n == index) {
            if (name && namemax > 0) {
                int k = 0;
                for (; files[i].name[k] && k < namemax - 1; k++) name[k] = files[i].name[k];
                name[k] = 0;
            }
            if (is_dir) *is_dir = files[i].is_dir;
            if (size) *size = files[i].is_dir ? 0 : files[i].len;
            return 0;
        }
        n++;
    }
    return -1;
}

void fs_init(void) {
    memset(files, 0, sizeof files);
    files[0].used = true;
    files[0].is_dir = true;
    files[0].parent = -1;
    files[0].name[0] = 0;
    cwd = 0;

    static const char readme[] =
        "Welcome to the LeoOS 0.4 file system!\n"
        "Files and folders live in memory (RAM) only, so they are lost when you restart.\n"
        "Open the Files app to browse, or try ls, cat and rm in the Terminal.\n"
        "A sample folder called docs is already here - type: ls docs\n";
    fs_write("/readme.txt", readme, (int)strlen(readme));
    fs_mkdir("/docs");
    static const char welcome[] =
        "Hello from the docs folder!\n"
        "This is a sample file in LeoOS 0.4.\n"
        "You can open it from the Files app, or type: cat docs/welcome.txt\n";
    fs_write("/docs/welcome.txt", welcome, (int)strlen(welcome));
}
