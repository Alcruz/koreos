#include "../include/kprint.h"
#include "../include/serial.h"

/* serial_* is referenced only here, so the serial backend can be swapped out
 * by changing these two wrappers. */
void kprint_putc(char c)
{
    serial_putc(c);
}

void kprint_puts(const char *s)
{
    serial_puts(s);
}

void kprint_hex(uint64_t v)
{
    static const char digits[] = "0123456789abcdef";
    kprint_puts("0x");
    for (int shift = 60; shift >= 0; shift -= 4)
        kprint_putc(digits[(v >> shift) & 0xf]);
}

void kprint_dec(uint64_t v)
{
    char buf[20];
    int i = 0;

    if (v == 0) {
        kprint_putc('0');
        return;
    }
    while (v) {
        buf[i++] = (char)('0' + (v % 10));
        v /= 10;
    }
    while (i--)
        kprint_putc(buf[i]);
}
