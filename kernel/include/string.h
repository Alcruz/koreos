#ifndef _STRING_H
#define _STRING_H

#include <stddef.h>

/* Nonzero if a and b are equal NUL-terminated strings. */
int str_eq(const char *a, const char *b);

/* Nonzero if s begins with prefix. */
int str_prefix(const char *s, const char *prefix);

/* Length of a NUL-terminated string, excluding the terminator. */
size_t str_len(const char *s);

#endif /* _STRING_H */
