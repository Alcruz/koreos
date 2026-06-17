#include "../include/serial.h"

void kernel_main(void) {
    serial_init();
    serial_puts("Koreos!\n");

    /* Idle loop */
    while (1) {
        __asm__ volatile("wfe");
    }
}
