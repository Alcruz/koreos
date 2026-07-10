#include "../include/panic.h"
#include "../include/kprint.h"

/* Single-core for now: this stops the calling core only. Once secondary cores
 * are brought up, a real halt has to stop them too. */
void halt(void) {
    kprint_puts("System halted.\n");
    for (;;)
        __asm__ volatile("wfe");
}

void panic(const char *msg) {
    kprint_puts("\n*** PANIC ***\n");
    kprint_puts(msg);
    kprint_putc('\n');
    halt();
}
