#ifndef _ALIGN_H
#define _ALIGN_H

#include <stdint.h>

/* Round to a power-of-two alignment boundary `a`. Behaviour is undefined if `a`
 * is not a power of two. These are the one place the mask arithmetic lives;
 * callers pass their own boundary (PAGE_SIZE, HEAP_ALIGN, ...). */

static inline uint64_t align_up(uint64_t x, uint64_t a)
{
    return (x + (a - 1)) & ~(a - 1);
}

static inline uint64_t align_down(uint64_t x, uint64_t a)
{
    return x & ~(a - 1);
}

#endif /* _ALIGN_H */
