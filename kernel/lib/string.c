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
