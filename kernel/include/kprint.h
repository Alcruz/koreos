#ifndef _KPRINT_H
#define _KPRINT_H

#include <stdint.h>

/* Tiny formatted output over the serial console (no libc). This is the single
 * front door for kernel console output: strings, characters, and numbers all
 * go through here so callers depend on kprint rather than the serial driver. */

/* Write a single character. */
void kprint_putc(char c);

/* Write a NUL-terminated string. */
void kprint_puts(const char *s);

/* "0x" followed by 16 lowercase hex digits. */
void kprint_hex(uint64_t v);

/* Unsigned decimal, no leading zeros. */
void kprint_dec(uint64_t v);

#endif /* _KPRINT_H */
