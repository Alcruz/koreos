#include "../include/kprint.h"
#include "../include/panic.h"

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

void handle_exception(unsigned long type, unsigned long *frame) {
    (void)frame; /* register file is saved; not decoded yet */

    unsigned long esr, elr, far, spsr;
    __asm__ volatile("mrs %0, esr_el1" : "=r"(esr));
    __asm__ volatile("mrs %0, elr_el1" : "=r"(elr));
    __asm__ volatile("mrs %0, far_el1" : "=r"(far));
    __asm__ volatile("mrs %0, spsr_el1" : "=r"(spsr));

    kprint_puts("\n*** EXCEPTION ***\n");
    kprint_puts("vector:   ");
    kprint_puts(exception_names[type & 0xf]);
    kprint_putc('\n');

    kprint_puts("ESR_EL1:  "); kprint_hex(esr);  kprint_putc('\n');
    kprint_puts("  EC:     "); kprint_hex((esr >> 26) & 0x3f); kprint_putc('\n');
    kprint_puts("  ISS:    "); kprint_hex(esr & 0x1ffffff);    kprint_putc('\n');
    kprint_puts("ELR_EL1:  "); kprint_hex(elr);  kprint_putc('\n');
    kprint_puts("FAR_EL1:  "); kprint_hex(far);  kprint_putc('\n');
    kprint_puts("SPSR_EL1: "); kprint_hex(spsr); kprint_putc('\n');

    halt();
}
