# LeoOS 0.4 ramfs API

This is the contract for the in-memory filesystem in `kernel/ramfs.c` and `include/ramfs.h`. Everything is lost on restart. Astra: please use these functions for the new shell commands (`mkdir`, `cd`, `pwd`, `cp`, `mv`, `touch`, and any others). Do not add those commands in this tree until you take them in your own work; `ls`, `cat`, `rm` and `write` have already been updated to accept paths.

## Limits

| Macro | Value | Meaning |
|---|---|---|
| `FS_MAX_FILES` | 32 | Total slots. Slot 0 is always the root directory and cannot be removed or renamed. Every file and every folder occupies one slot. |
| `FS_NAME_MAX` | 24 | Maximum length of **one path component**, including the terminating `0`. So a name may be at most 23 characters. |
| `FS_PATH_MAX` | 128 | Maximum length of a full path, including the terminating `0`. |
| `FS_DATA_MAX` | 4000 | Maximum bytes of data in a regular file. Writes longer than this are truncated. Directories have length 0 and store no data. |

`fs_files()` counts regular files only, not folders. `fs_count()` returns `FS_MAX_FILES` (the number of slots).

## Path rules

- `/` is the root directory. The current working directory starts at `/`.
- A path that begins with `/` is absolute. Any other path is relative to the current working directory (`fs_cwd()`).
- `""` (empty string) means the current working directory, except for `fs_valid_path`, which also accepts it, and for `fs_mkdir` / `fs_write`, which need a real name.
- Components are separated by `/`. Repeated slashes are ignored. A trailing slash is allowed (`docs/` is the same as `docs`).
- `.` means the current directory in a path; `..` means the parent. `..` of root stays at root.
- A **component** may only use `a-z`, `A-Z`, `0-9`, `.`, `_` and `-`, must not be empty, and must not be `.` or `..`. Paths are case-sensitive.
- Examples: `readme.txt`, `docs/welcome.txt`, `/docs`, `../readme.txt`.

Seeded at boot (always from a fresh `fs_init`):

- `/readme.txt`
- `/docs/` (a folder)
- `/docs/welcome.txt`

## Existing API (still works)

These take a path (not only a bare name). Bare names such as `readme.txt` still work, and are looked up relative to the current directory, which starts at `/`.

```c
void        fs_init(void);
int         fs_count(void);              /* FS_MAX_FILES */
bool        fs_used(int i);              /* slot i is in use (file or directory, including root) */
const char *fs_name(int i);              /* basename only; "" for the root slot */
int         fs_size(int i);              /* file length in bytes; 0 for a directory */
int         fs_find(const char *path);   /* slot index, or -1 if not found / bad path */
bool        fs_valid_name(const char *name);  /* one component only (no slashes) */
int         fs_write(const char *path, const char *data, int len);
int         fs_read(const char *path, char *buf, int max);
int         fs_remove(const char *path);
int         fs_files(void);              /* number of regular files */
```

### `fs_write`

Creates or overwrites a regular file. Parent folders must already exist.

- `0`  saved
- `-1` no free slot (filesystem full); a write that is only overwriting an existing file will not fail this way
- `-2` bad path or bad component name
- `-3` a parent folder is missing, or a middle component is not a directory
- `-4` the path exists and is a directory

If `len` is greater than `FS_DATA_MAX`, the extra bytes are silently dropped.

### `fs_read`

- On success, copies up to `max` bytes into `buf` and returns the number of bytes copied (which may be `0` for an empty file).
- `-1` not found, bad path, or the path is a directory.

### `fs_remove`

Removes a file, or an **empty** directory.

- `0`  removed
- `-1` not found
- `-2` the path is a directory that still has children
- `-3` the path is the root directory (cannot be removed)

If the current directory (or an ancestor of it) is removed, the current directory is reset to `/`.

### `fs_find`

Returns a slot index (`0` is root) or `-1`. It does not tell you whether the slot is a file or a directory; use `fs_slot_is_dir(i)` or `fs_is_dir(path)`.

### `fs_name` / `fs_used` / `fs_size` / `fs_count`

`fs_name` returns only the basename (`welcome.txt`, not `/docs/welcome.txt`). Use `fs_path` for the absolute path. Slot 0 is the root: `fs_used(0)` is true and `fs_name(0)` is `""`.

## New functions

```c
bool        fs_valid_path(const char *path);
bool        fs_slot_is_dir(int i);
int         fs_parent(int i);            /* parent slot, or -1 for the root */
int         fs_path(int i, char *buf, int max);   /* writes an absolute path; length or -1 */

int         fs_mkdir(const char *path);
int         fs_is_dir(const char *path);          /* 1 dir, 0 file, -1 not found */
const char *fs_cwd(void);                         /* absolute path, e.g. "/" or "/docs" */
int         fs_chdir(const char *path);
int         fs_rename(const char *old, const char *newp);
int         fs_copy(const char *src, const char *dst);

int         fs_list(const char *dir,
                    void (*cb)(const char *name, bool is_dir, int size, void *ctx),
                    void *ctx);
int         fs_dir_count(const char *dir);
int         fs_dir_get(const char *dir, int index, char *name, int namemax,
                       bool *is_dir, int *size);
int         fs_join(char *buf, int max, const char *dir, const char *name);
```

### `fs_valid_path`

Returns true if `path` is well-formed and no longer than `FS_PATH_MAX - 1`. It does **not** check that the path exists. `""` and `"/"` are valid.

### `fs_mkdir`

Creates one directory. The parent must already exist (no `mkdir -p` behaviour).

- `0`  created
- `-1` filesystem full
- `-2` bad path or bad name
- `-3` something already exists at that path (file or directory)
- `-4` the parent is missing, or a middle component is not a directory

### `fs_is_dir`

- `1`  the path exists and is a directory (including `/`)
- `0`  the path exists and is a regular file
- `-1` not found or bad path

### `fs_cwd` / `fs_chdir`

`fs_cwd()` returns a pointer to an internal buffer holding the absolute path of the current directory. It is overwritten by the next `fs_cwd()` call (and may also change after `fs_chdir` / `fs_rename` / `fs_remove`), so copy it if you need to keep it.

`fs_chdir`:

- `0`  current directory is now `path`
- `-1` not found
- `-2` the path exists but is not a directory
- `-3` bad path

### `fs_rename` (also used as move)

If `newp` exists and is a directory, the source is moved **into** it under its current basename (like `mv file docs`). Otherwise `newp` is the new full path.

- `0`  renamed or moved
- `-1` source not found
- `-2` bad destination path or name
- `-3` the destination already exists (it will not overwrite)
- `-4` the destination's parent is missing or not a directory
- `-5` invalid: renaming the root, or moving a directory into itself or a descendant

### `fs_copy`

Same destination rule as `fs_rename`: if `dst` is an existing directory, the source is copied into it. Directories are copied recursively. Copying a file onto an existing file overwrites it.

- `0`  copied
- `-1` source not found
- `-2` bad destination path or name
- `-3` destination conflict (for example a file in the way of a directory)
- `-4` the destination's parent is missing or not a directory
- `-5` invalid: copying the root, copying a directory into itself, or copying a path onto itself
- `-6` filesystem became full in the middle of a (possibly recursive) copy

### Listing a directory

`fs_list(dir, cb, ctx)` walks the entries of `dir` (not `.` or `..`) in slot order and calls `cb(name, is_dir, size, ctx)` for each one. `name` is a basename. `size` is `0` for a directory. Pass `cb = NULL` if you only want the count. Returns the number of entries, or `-1` if `dir` is missing or not a directory.

`fs_dir_count(dir)` is the same count.

`fs_dir_get(dir, index, name, namemax, is_dir, size)` reads entry `index` (`0 .. count-1`) in the same slot order. Any of `name`, `is_dir`, `size` may be `NULL`. Returns `0` on success, or `-1` if the directory or index is invalid.

The Files app and the `ls` command sort entries themselves (folders first, then by name). The API does not sort.

### `fs_path` / `fs_join` / `fs_parent` / `fs_slot_is_dir`

`fs_path(i, buf, max)` writes the absolute path of slot `i` into `buf` (`/` for the root) and returns the length, or `-1`.

`fs_join(buf, max, dir, name)` builds an absolute path from a directory and a basename. `dir` may be `""` or `"/"` for the root. Returns the length, or `-1` if it would not fit.

`fs_parent(i)` returns the parent slot, or `-1` for the root slot.

`fs_slot_is_dir(i)` is true when slot `i` is in use and is a directory.

## Suggested use for new shell commands

- `pwd`        → `fs_cwd()`
- `cd path`    → `fs_chdir(path)` (treat `cd` with no argument as `fs_chdir("/")`)
- `mkdir path` → `fs_mkdir(path)`
- `mv a b`     → `fs_rename(a, b)`
- `cp a b`     → `fs_copy(a, b)`
- `touch path` → `fs_write(path, "", 0)` (this creates or truncates)
- `ls path`    → already implemented with `fs_is_dir` + `fs_list`
