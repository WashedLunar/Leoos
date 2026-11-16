#ifndef LEO_RAMFS_H
#define LEO_RAMFS_H
/* LeoOS RAM filesystem: a handful of small text files kept in memory.
 * There is no disk driver yet, so everything is lost on restart. */
#include "types.h"
#define FS_MAX_FILES 16
#define FS_NAME_MAX  24          /* including the terminating 0 */
#define FS_DATA_MAX  4000

void        fs_init(void);
int         fs_count(void);                         /* number of slots (FS_MAX_FILES) */
bool        fs_used(int i);
const char *fs_name(int i);
int         fs_size(int i);
int         fs_find(const char *name);              /* slot index or -1 */
bool        fs_valid_name(const char *name);
int         fs_write(const char *name, const char *data, int len);   /* 0 ok, -1 full, -2 bad name */
int         fs_read(const char *name, char *buf, int max);           /* length or -1 */
int         fs_remove(const char *name);            /* 0 ok, -1 not found */
int         fs_files(void);                         /* how many files exist */
#endif
