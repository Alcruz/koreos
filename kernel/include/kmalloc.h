#ifndef _KMALLOC_H
#define _KMALLOC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "pmm.h"

/* Kernel heap: an explicit free list of variable-size blocks, backed by frames
 * pulled from the physical frame allocator on demand. The per-block header
 * layout is private to kmalloc.c; callers only touch the pointers kmalloc hands
 * back and the opaque heap_t below.
 */

/* AArch64 AAPCS64 keeps the stack — and, by convention, any general allocator's
 * returned pointers — 16-byte aligned, so 128-bit accesses (q registers,
 * LDP/STP on 16-byte types) are always legal. Every pointer the heap hands back
 * is a multiple of this, and every request size is rounded up to it. */
#define HEAP_ALIGN 16UL

/* Per-block bookkeeping; layout is defined in kmalloc.c. heap_t only stores a
 * pointer to it, so an incomplete type is all this header needs. */
struct block_header;

/* One kernel heap. Callers treat this as opaque and pass it to the heap_*
 * entry points; heap_init fills it in. The free list is kept address-ordered so
 * kfree can coalesce with adjacent neighbours (task 1.3.4). */
typedef struct heap {
    struct block_header *free_list;   /* address-ordered free blocks, NULL when empty */
    pmm_t               *frames;      /* frame source used to grow the heap */
} heap_t;

/* Bring up an empty heap that grows by pulling frames from `frames`. No frames
 * are reserved up front; the first allocation grows the heap (task 1.3.2). */
void heap_init(pmm_t *frames, heap_t *out);

/* Allocate at least `size` bytes from `heap`, 16-byte aligned. First-fit over
 * the free list; a block big enough is split when the leftover can stand on its
 * own, otherwise handed over whole. Grows the heap once and retries if nothing
 * fits. Returns NULL on a zero request or when growth fails. The memory is not
 * zeroed (see kzalloc). */
void *kmalloc(heap_t *heap, size_t size);

/* Return a block obtained from kmalloc back to `heap`, coalescing it with any
 * adjacent free neighbours. `ptr` may be NULL (no-op). Passing a pointer not
 * from kmalloc, or freeing the same pointer twice, is undefined — though a
 * plain double free is caught and ignored. */
void kfree(heap_t *heap, void *ptr);

/* Like kmalloc, but the returned `size` bytes are zeroed. Returns NULL on the
 * same conditions as kmalloc. Frames from the PMM come back non-zeroed, so
 * callers wanting clean memory (task structs, page tables) use this. */
void *kzalloc(heap_t *heap, size_t size);

/* Total free payload currently on the heap's free list, in bytes (excludes
 * block headers). For diagnostics and leak checks, not the allocation path. */
size_t heap_free_bytes(const heap_t *heap);

#endif /* _KMALLOC_H */
