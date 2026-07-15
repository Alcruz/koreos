#include "../include/string.h"

int str_eq(const char *a, const char *b)
{
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

int str_prefix(const char *s, const char *prefix)
{
    while (*prefix) {
        if (*s++ != *prefix++)
            return 0;
    }
    return 1;
}

size_t str_len(const char *s)
{
    const char *p = s;
    while (*p)
        p++;
    return (size_t)(p - s);
}

void *memcpy(void *dest, const void *src, size_t n)
{
    unsigned char *d = dest;
    const unsigned char *s = src;
    while (n--)
        *d++ = *s++;
    return dest;
}

void *memmove(void *dest, const void *src, size_t n)
{
    unsigned char *d = dest;
    const unsigned char *s = src;
    if (d == s || n == 0)
        return dest;
    /* Copy backwards when the regions overlap and dest is above src. */
    if (d < s) {
        while (n--)
            *d++ = *s++;
    } else {
        d += n;
        s += n;
        while (n--)
            *--d = *--s;
    }
    return dest;
}

void *memset(void *dest, int c, size_t n)
{
    unsigned char *d = dest;
    while (n--)
        *d++ = (unsigned char)c;
    return dest;
}

int memcmp(const void *a, const void *b, size_t n)
{
    const unsigned char *pa = a;
    const unsigned char *pb = b;
    while (n--) {
        if (*pa != *pb)
            return (int)*pa - (int)*pb;
        pa++;
        pb++;
    }
    return 0;
}
