#include "../../include/core/serial/serial.h"
#include "../../include/mmio.h"

/* Base of the PL011 register window, resolved from the DTB and handed to
 * serial_init at boot. Zero until then; nothing prints before serial_init. */
static uintptr_t uart_base;

void serial_init(uintptr_t base) {
    uart_base = base;
    /* Enable UART and Tx/Rx */
    mmio_write(uart_base + UART_CR, (1 << 0) | (1 << 8) | (1 << 9));
}

void serial_putc(char c) {
    /* Wait for transmit buffer not full */
    while (mmio_read(uart_base + UART_FR) & UART_FR_TXFF);
    mmio_write(uart_base + UART_DR, c);
}

void serial_puts(const char *s) {
    while (*s) {
        if (*s == '\n')
            serial_putc('\r');
        serial_putc(*s++);
    }
}
