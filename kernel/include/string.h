#ifndef _STRING_H
#define _STRING_H

#include <stddef.h>

/* Nonzero if a and b are equal NUL-terminated strings. */
int str_eq(const char *a, const char *b);

/* Nonzero if s begins with prefix. */
int str_prefix(const char *s, const char *prefix);

/* Length of a NUL-terminated string, excluding the terminator. */
size_t str_len(const char *s);

/*
 * Standard freestanding mem* routines. These keep the C-standard signatures
 * (destination first) because the compiler may emit calls to these symbols
 * during codegen (struct copies, zeroing, etc.), so the ABI is fixed and the
 * project's out-params-last convention does not apply here.
 */

/* Copy n bytes from src to dest (must not overlap); returns dest. */
void *memcpy(void *dest, const void *src, size_t n);

/* Copy n bytes from src to dest, handling overlap; returns dest. */
void *memmove(void *dest, const void *src, size_t n);

/* Fill n bytes of dest with byte c; returns dest. */
void *memset(void *dest, int c, size_t n);

/* Compare n bytes; <0, 0, >0 as a is less than, equal to, greater than b. */
int memcmp(const void *a, const void *b, size_t n);

#endif /* _STRING_H */
