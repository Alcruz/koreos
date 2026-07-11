#ifndef _KMALLOC_H
#define _KMALLOC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "pmm.h"

/* Kernel heap: an explicit free list of variable-size blocks, backed by frames
 * pulled from the physical frame allocator on demand.
 *
 * This header defines the on-heap layout (task 1.3.1). Growth (heap_grow),
 * kmalloc, kfree, and kzalloc build on these types in later tasks.
 */

/* AArch64 AAPCS64 keeps the stack — and, by convention, any general allocator's
 * returned pointers — 16-byte aligned, so 128-bit accesses (q registers,
 * LDP/STP on 16-byte types) are always legal. Every pointer the heap hands back
 * is a multiple of this, and every request size is rounded up to it. */
#define HEAP_ALIGN 16UL

/* Per-block bookkeeping, stored immediately before the payload. The pointer a
 * caller receives is the byte just past this header, i.e. (block_header_t *)h + 1.
 *
 * Kept to exactly one alignment unit (16 bytes) so that, given a 16-aligned
 * block start, the payload right after the header is 16-aligned too. `size` and
 * `free` share a single 64-bit word to hit that size: because sizes are always
 * rounded up to HEAP_ALIGN the top bit is free to spare for the flag.
 *
 *   next  links free blocks together; only meaningful while `free` is set.
 *   size  payload capacity in bytes, excluding this header.
 *   free  1 while the block sits on the free list, 0 once handed out.
 */
typedef struct block_header {
    struct block_header *next;
    size_t               size : 63;
    size_t               free : 1;
} block_header_t;

_Static_assert(sizeof(block_header_t) == HEAP_ALIGN,
               "block header must be exactly one alignment unit so payloads stay 16-aligned");

/* One kernel heap. Callers treat this as opaque and pass it to the heap_*
 * entry points; heap_init fills it in. The free list is kept address-ordered so
 * kfree can coalesce with adjacent neighbours (task 1.3.4). */
typedef struct heap {
    block_header_t *free_list;   /* address-ordered free blocks, NULL when empty */
    pmm_t          *frames;      /* frame source used to grow the heap */
} heap_t;

/* Bring up an empty heap that grows by pulling frames from `frames`. No frames
 * are reserved up front; the first allocation grows the heap (task 1.3.2). */
void heap_init(pmm_t *frames, heap_t *out);

#endif /* _KMALLOC_H */
