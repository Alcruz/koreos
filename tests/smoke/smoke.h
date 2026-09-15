#ifndef _SMOKE_H
#define _SMOKE_H

#include <stdint.h>
#include "../../kernel/include/kprint.h"
#include "../../kernel/include/pmm.h"
#include "../../kernel/include/kmalloc.h"

/* Shared helpers for on-target smoke tests. Each smoke test is its own
 * kernel binary: it calls smoke_boot to stand the machine up, runs one check,
 * and calls smoke_pass / smoke_fail (which shut QEMU down cleanly via PSCI).
 * The harness greps serial for the canonical marker line.
 *
 * This is test infrastructure — it duplicates the real kernel's boot
 * sequence rather than pulling a shared abstraction into the production
 * tree. See CLAUDE.md. */

typedef struct smoke_env {
    pmm_t *pmm;
    heap_t *heap;
    uint64_t gicd_base; /* for tests that poke the distributor directly */
} smoke_env_t;

/* Run the full early-boot sequence and populate `env`. Panics on any fatal
 * error. Returns with the MMU on, console live, heap ready. IRQs stay masked
 * — tests that need them unmask themselves after registering handlers. */
void smoke_boot(void *dtb, smoke_env_t *env);

/* Power the machine off via PSCI SYSTEM_OFF. QEMU exits with status 0. */
void smoke_exit(void) __attribute__((noreturn));

static inline void smoke_pass(const char *name)
{
    kprint_puts("SMOKE: ");
    kprint_puts(name);
    kprint_puts(" PASS\n");
    smoke_exit();
}

static inline void smoke_fail(const char *name, const char *reason)
{
    kprint_puts("SMOKE: ");
    kprint_puts(name);
    kprint_puts(" FAIL");
    if (reason) {
        kprint_puts(": ");
        kprint_puts(reason);
    }
    kprint_putc('\n');
    smoke_exit();
}

#define SMOKE_REPORT(name, ok) \
    do { if (ok) smoke_pass(name); else smoke_fail(name, NULL); } while (0)

#endif /* _SMOKE_H */
