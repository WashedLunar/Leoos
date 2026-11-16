#ifndef LEO_STRING_H
#define LEO_STRING_H
#include "types.h"
size_t strlen(const char *s);
int    strcmp(const char *a, const char *b);
int    strncmp(const char *a, const char *b, size_t n);
char  *strcpy(char *d, const char *s);
void  *memset(void *d, int c, size_t n);
void  *memcpy(void *d, const void *s, size_t n);
void  *memmove(void *d, const void *s, size_t n);
void   itoa(int32_t v, char *buf);
void   utoa(uint32_t v, char *buf, int base);
#endif
