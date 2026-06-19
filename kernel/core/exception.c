#include "../include/serial.h"

/* Names for the 16 vector table entries, indexed by the type passed from
 * vectors.S. */
static const char *const exception_names[16] = {
    "SYNC  (Current EL, SP0)",
    "IRQ   (Current EL, SP0)",
    "FIQ   (Current EL, SP0)",
    "SError(Current EL, SP0)",
    "SYNC  (Current EL, SPx)",
    "IRQ   (Current EL, SPx)",
    "FIQ   (Current EL, SPx)",
    "SError(Current EL, SPx)",
    "SYNC  (Lower EL, AArch64)",
    "IRQ   (Lower EL, AArch64)",
    "FIQ   (Lower EL, AArch64)",
    "SError(Lower EL, AArch64)",
    "SYNC  (Lower EL, AArch32)",
    "IRQ   (Lower EL, AArch32)",
    "FIQ   (Lower EL, AArch32)",
    "SError(Lower EL, AArch32)",
};

/* Print a 64-bit value as "0x" + 16 hex digits over the serial console. */
static void serial_puthex(unsigned long val) {
    static const char hex[] = "0123456789abcdef";
    serial_puts("0x");
    for (int shift = 60; shift >= 0; shift -= 4)
        serial_putc(hex[(val >> shift) & 0xf]);
}

void handle_exception(unsigned long type, unsigned long *frame) {
    (void)frame; /* register file is saved; not decoded yet */

    unsigned long esr, elr, far, spsr;
    __asm__ volatile("mrs %0, esr_el1" : "=r"(esr));
    __asm__ volatile("mrs %0, elr_el1" : "=r"(elr));
    __asm__ volatile("mrs %0, far_el1" : "=r"(far));
    __asm__ volatile("mrs %0, spsr_el1" : "=r"(spsr));

    serial_puts("\n*** EXCEPTION ***\n");
    serial_puts("vector:   ");
    serial_puts(exception_names[type & 0xf]);
    serial_putc('\n');

    serial_puts("ESR_EL1:  "); serial_puthex(esr);  serial_putc('\n');
    serial_puts("  EC:     "); serial_puthex((esr >> 26) & 0x3f); serial_putc('\n');
    serial_puts("  ISS:    "); serial_puthex(esr & 0x1ffffff);    serial_putc('\n');
    serial_puts("ELR_EL1:  "); serial_puthex(elr);  serial_putc('\n');
    serial_puts("FAR_EL1:  "); serial_puthex(far);  serial_putc('\n');
    serial_puts("SPSR_EL1: "); serial_puthex(spsr); serial_putc('\n');

    serial_puts("System halted.\n");
    for (;;)
        __asm__ volatile("wfe");
}
