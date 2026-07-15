#ifndef _MMIO_H
#define _MMIO_H

#include <stdint.h>

/* A physical MMIO region to identity-map as Device-nGnRnE memory. The caller
 * (which owns board knowledge) supplies these; mmu.c does not reach into any
 * driver header for fixed addresses. */
typedef struct mmio_region {
    uint64_t base;
    uint64_t size;
} mmio_region_t;

static inline void mmio_write(uintptr_t reg, uint32_t val) {
    *(volatile uint32_t *)reg = val;
}

static inline uint32_t mmio_read(uintptr_t reg) {
    return *(volatile uint32_t *)reg;
}

#endif /* _MMIO_H */
