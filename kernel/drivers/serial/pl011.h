#ifndef _PL011_H
#define _PL011_H

/* PL011 UART registers for QEMU virt */
#define UART_BASE 0x09000000
#define UART_DR (UART_BASE + 0x00)      /* Data Register */
#define UART_FR (UART_BASE + 0x18)      /* Flag Register */
#define UART_LCRH (UART_BASE + 0x2C)    /* Line Control Register */
#define UART_CR (UART_BASE + 0x30)      /* Control Register */

#define UART_FR_TXFF (1 << 5)           /* TX FIFO Full */
#define UART_FR_RXFE (1 << 4)           /* RX FIFO Empty */

#endif /* _PL011_H */
