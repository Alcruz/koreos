#ifndef _SERIAL_H
#define _SERIAL_H

#include <stdint.h>

/* PL011 UART register offsets from the device base. The base itself is a board
 * property, resolved from the DTB at boot and handed to serial_init — never a
 * hardcoded address here. */
#define UART_DR 0x00                    /* Data Register */
#define UART_FR 0x18                    /* Flag Register */
#define UART_LCRH 0x2C                  /* Line Control Register */
#define UART_CR 0x30                    /* Control Register */

#define UART_FR_TXFF (1 << 5)           /* TX FIFO Full */
#define UART_FR_RXFE (1 << 4)           /* RX FIFO Empty */

/* Bring up the console on the PL011 whose register window starts at `base`
 * (resolved from the DTB by the caller). */
void serial_init(uintptr_t base);
void serial_putc(char c);
void serial_puts(const char *s);

#endif /* _SERIAL_H */
