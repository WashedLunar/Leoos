#ifndef LEO_RAMFS_H
#define LEO_RAMFS_H
/* LeoOS RAM filesystem (0.4): folders, paths and a current directory.
 * There is no disk driver yet, so everything is lost on restart.
 * See FS_API.md for the full contract. */
#include "types.h"
#define FS_MAX_FILES 32
#define FS_NAME_MAX  24          /* one path component, including the terminating 0 */
#define FS_PATH_MAX  128         /* a full path, including the terminating 0 */
#define FS_DATA_MAX  4000        /* bytes of file data; directories have length 0 */

void        fs_init(void);
int         fs_count(void);                         /* number of slots (FS_MAX_FILES) */
bool        fs_used(int i);
bool        fs_slot_is_dir(int i);                  /* true if slot i is a used directory */
const char *fs_name(int i);                         /* basename only ("" for the root slot) */
int         fs_parent(int i);                       /* parent slot, or -1 for the root */
int         fs_size(int i);
int         fs_path(int i, char *buf, int max);     /* absolute path; length or -1 */
int         fs_find(const char *path);              /* slot index or -1 (cwd-relative) */
bool        fs_valid_name(const char *name);        /* one component: a-z A-Z 0-9 . _ - */
bool        fs_valid_path(const char *path);        /* well-formed path, not whether it exists */
int         fs_write(const char *path, const char *data, int len);
                                                    /* 0 ok, -1 full, -2 bad path, -3 no parent, -4 is a dir */
int         fs_read(const char *path, char *buf, int max); /* length, or -1 not found / is a dir */
int         fs_remove(const char *path);            /* 0 ok, -1 not found, -2 dir not empty, -3 is root */
int         fs_files(void);                         /* how many regular files exist (not folders) */

int         fs_mkdir(const char *path);             /* 0 ok, -1 full, -2 bad path, -3 exists, -4 no parent */
int         fs_is_dir(const char *path);            /* 1 dir, 0 file, -1 not found */
const char *fs_cwd(void);                           /* absolute path of the current directory */
int         fs_chdir(const char *path);             /* 0 ok, -1 not found, -2 not a dir, -3 bad path */
int         fs_rename(const char *old, const char *newp);
                                                    /* 0 ok, -1 no source, -2 bad dest, -3 dest exists,
                                                     * -4 no dest parent, -5 invalid (root / into self) */
int         fs_copy(const char *src, const char *dst);
                                                    /* 0 ok, -1 no source, -2 bad dest, -3 dest exists
                                                     * as a conflict, -4 no dest parent, -5 invalid,
                                                     * -6 filesystem full mid-copy */
int         fs_list(const char *dir,
                    void (*cb)(const char *name, bool is_dir, int size, void *ctx),
                    void *ctx);                     /* entry count, or -1 if dir missing / not a dir */
int         fs_dir_count(const char *dir);          /* same as fs_list without a callback */
int         fs_dir_get(const char *dir, int index, char *name, int namemax,
                       bool *is_dir, int *size);    /* 0 ok, -1 bad dir / index */
int         fs_join(char *buf, int max, const char *dir, const char *name);
#endif
