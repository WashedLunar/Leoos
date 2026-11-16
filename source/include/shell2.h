#ifndef LEO_SHELL2_H
#define LEO_SHELL2_H
/* LeoOS 0.4 extra terminal commands (by Astra). Kept in their own file so
 * shell.c only needs two small hooks. */
#include "types.h"
#include "multiboot.h"
void shell2_init(struct multiboot_info *mbi, bool mb_valid);
bool shell2_run(int argc, char **argv);   /* true if argv[0] was one of ours */
void shell2_help(void);                   /* prints our lines for 'help' */
int         shell2_cmd_count(void);       /* for paged help in shell.c; keep when refreshing this file */
const char *shell2_cmd_name(int i);
const char *shell2_cmd_help(int i);
extern bool gui_active;                   /* set by gui_run() */
#endif
