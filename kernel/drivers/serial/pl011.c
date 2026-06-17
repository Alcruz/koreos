#include "../../include/serial.h"
#include "../../include/mmio.h"
#include "pl011.h"

void serial_init(void) {
    /* Enable UART and Tx/Rx */
    mmio_write(UART_CR, (1 << 0) | (1 << 8) | (1 << 9));
}

void serial_putc(char c) {
    /* Wait for transmit buffer not full */
    while (mmio_read(UART_FR) & UART_FR_TXFF);
    mmio_write(UART_DR, c);
}

void serial_puts(const char *s) {
    while (*s) {
        if (*s == '\n')
            serial_putc('\r');
        serial_putc(*s++);
    }
}
