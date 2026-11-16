/* LeoOS - tiny in-memory filesystem (new in 0.3). Flat: no folders, no disk. */
#include "ramfs.h"
#include "string.h"

struct file { bool used; char name[FS_NAME_MAX]; int len; char data[FS_DATA_MAX]; };
static struct file files[FS_MAX_FILES];

int fs_count(void) { return FS_MAX_FILES; }
bool fs_used(int i) { return i >= 0 && i < FS_MAX_FILES && files[i].used; }
const char *fs_name(int i) { return files[i].name; }
int fs_size(int i) { return files[i].len; }

int fs_files(void) {
    int n = 0;
    for (int i = 0; i < FS_MAX_FILES; i++) if (files[i].used) n++;
    return n;
}

bool fs_valid_name(const char *s) {
    int n = 0;
    for (; s[n]; n++) {
        char c = s[n];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-'))
            return false;
    }
    return n > 0 && n < FS_NAME_MAX;
}

int fs_find(const char *name) {
    for (int i = 0; i < FS_MAX_FILES; i++) if (files[i].used && !strcmp(files[i].name, name)) return i;
    return -1;
}

int fs_write(const char *name, const char *data, int len) {
    if (!fs_valid_name(name)) return -2;
    int i = fs_find(name);
    if (i < 0) for (i = 0; i < FS_MAX_FILES && files[i].used; i++) ;
    if (i == FS_MAX_FILES) return -1;
    if (len > FS_DATA_MAX) len = FS_DATA_MAX;
    files[i].used = true;
    strcpy(files[i].name, name);
    memcpy(files[i].data, data, (size_t)len);
    files[i].len = len;
    return 0;
}

int fs_read(const char *name, char *buf, int max) {
    int i = fs_find(name);
    if (i < 0) return -1;
    int n = files[i].len < max ? files[i].len : max;
    memcpy(buf, files[i].data, (size_t)n);
    return n;
}

int fs_remove(const char *name) {
    int i = fs_find(name);
    if (i < 0) return -1;
    files[i].used = false;
    return 0;
}

void fs_init(void) {
    static const char readme[] =
        "Welcome to the LeoOS 0.3 file system!\n"
        "Files live in memory (RAM) only, so they are lost when you restart.\n"
        "Save a note in Notepad, then try ls, cat and rm in the Terminal.\n";
    fs_write("readme.txt", readme, (int)strlen(readme));
}
