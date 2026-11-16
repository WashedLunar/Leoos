#ifndef LEO_SHELL_H
#define LEO_SHELL_H
#include "multiboot.h"
void shell_init(struct multiboot_info *mbi, bool mb_valid);
void shell_key(int key);
void shell_run(struct multiboot_info *mbi, bool mb_valid);
#endif
