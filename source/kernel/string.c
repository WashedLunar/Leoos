/* LeoOS - tiny freestanding string library */
#include "string.h"

size_t strlen(const char *s) { size_t n = 0; while (s[n]) n++; return n; }

int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (uint8_t)*a - (uint8_t)*b;
}

int strncmp(const char *a, const char *b, size_t n) {
    while (n && *a && *a == *b) { a++; b++; n--; }
    return n ? (uint8_t)*a - (uint8_t)*b : 0;
}

char *strcpy(char *d, const char *s) { char *r = d; while ((*d++ = *s++)) ; return r; }

void *memset(void *d, int c, size_t n) { uint8_t *p = d; while (n--) *p++ = (uint8_t)c; return d; }

void *memcpy(void *d, const void *s, size_t n) {
    uint8_t *dp = d; const uint8_t *sp = s; while (n--) *dp++ = *sp++; return d;
}

void *memmove(void *d, const void *s, size_t n) {
    uint8_t *dp = d; const uint8_t *sp = s;
    if (dp < sp) { while (n--) *dp++ = *sp++; }
    else { dp += n; sp += n; while (n--) *--dp = *--sp; }
    return d;
}

void utoa(uint32_t v, char *buf, int base) {
    char tmp[33]; int i = 0;
    const char *digits = "0123456789ABCDEF";
    if (v == 0) tmp[i++] = '0';
    while (v) { tmp[i++] = digits[v % base]; v /= base; }
    int j = 0; while (i) buf[j++] = tmp[--i];
    buf[j] = 0;
}

void itoa(int32_t v, char *buf) {
    if (v < 0) { *buf++ = '-'; utoa((uint32_t)(-(int64_t)v), buf, 10); }
    else utoa((uint32_t)v, buf, 10);
}
