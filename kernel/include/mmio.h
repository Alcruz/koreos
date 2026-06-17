#ifndef _MMIO_H
#define _MMIO_H

typedef unsigned int uint32_t;
typedef unsigned long uintptr_t;

static inline void mmio_write(uintptr_t reg, uint32_t val) {
    *(volatile uint32_t *)reg = val;
}

static inline uint32_t mmio_read(uintptr_t reg) {
    return *(volatile uint32_t *)reg;
}

#endif /* _MMIO_H */
