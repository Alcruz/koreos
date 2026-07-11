#ifndef _SERIAL_H
#define _SERIAL_H

#include <stdint.h>

void serial_init(void);
void serial_putc(char c);
void serial_puts(const char *s);

#endif /* _SERIAL_H */
