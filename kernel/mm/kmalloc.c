/* Kernel heap allocator: an explicit free list of variable-size blocks.
 *
 * Each block carries a 16-byte header (block_header_t) sitting immediately
 * before its payload; free blocks are chained through header.next in an
 * address-ordered list. Backing memory comes from the physical frame allocator:
 * the heap grows a frame at a time on demand rather than reserving a fixed
 * arena up front.
 *
 * This file lays out the foundation (task 1.3.1): block header + free-list
 * state, 16-byte alignment, and heap bring-up. heap_grow, kmalloc, kfree, and
 * kzalloc land in later tasks.
 */

#include "../include/kmalloc.h"
#include "../include/align.h"

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

void heap_init(pmm_t *frames, heap_t *out)
{
    out->free_list = NULL;
    out->frames = frames;
}

/* Splice `block` into the address-ordered free list. Keeping the list sorted by
 * address is what lets kfree find and merge physical neighbours later (task
 * 1.3.4); no merging happens here yet. */
static void freelist_insert(heap_t *heap, block_header_t *block)
{
    block_header_t **link = &heap->free_list;
    while (*link && *link < block)
        link = &(*link)->next;
    block->next = *link;
    *link = block;
}

/* Grow the heap by enough whole frames to hold a `need`-byte payload plus its
 * header, wrap them in one free block, and link it (address-ordered) into the
 * free list. Returns the new block, or NULL if the frame allocator can't hand
 * back a contiguous run that large. kmalloc falls back to this when no existing
 * block fits. */
static block_header_t *heap_grow(heap_t *heap, size_t need)
{
    /* One header plus the payload, rounded up to whole frames. */
    size_t bytes = align_up(sizeof(block_header_t) + need, PAGE_SIZE);
    size_t npages = bytes / PAGE_SIZE;

    pmm_page_t *first = pmm_alloc_page(heap->frames);
    if (!first)
        return NULL;

    /* Extend the run one frame at a time; the frames must be adjacent so the
     * result is a single contiguous block. If the allocator returns a gap (or
     * runs dry), give every frame back and fail rather than build a block that
     * straddles a hole. */
    pmm_page_t *prev = first;
    for (size_t i = 1; i < npages; i++) {
        pmm_page_t *p = pmm_alloc_page(heap->frames);
        if (p != prev + 1) {
            if (p)
                pmm_free_page(heap->frames, p);
            for (pmm_page_t *f = first; f <= prev; f++)
                pmm_free_page(heap->frames, f);
            return NULL;
        }
        prev = p;
    }

    block_header_t *block = (block_header_t *)first;
    block->size = bytes - sizeof(block_header_t);
    block->free = 1;
    freelist_insert(heap, block);
    return block;
}

void *kmalloc(heap_t *heap, size_t size)
{
    if (size == 0)
        return NULL;
    size_t need = align_up(size, HEAP_ALIGN);

    /* First-fit, growing once and retrying if nothing fits. `grown` caps the
     * retry so a grow that still can't satisfy the request can't loop forever. */
    for (bool grown = false; ; grown = true) {
        for (block_header_t **link = &heap->free_list; *link; link = &(*link)->next) {
            block_header_t *block = *link;
            if (block->size < need)
                continue;

            /* Split off the tail only if the leftover can hold a header plus a
             * minimum payload; otherwise hand over the whole block so we never
             * leave an unusable sliver on the list. */
            size_t remainder = block->size - need;
            if (remainder >= sizeof(block_header_t) + HEAP_ALIGN) {
                block_header_t *tail =
                    (block_header_t *)((uint8_t *)(block + 1) + need);
                tail->size = remainder - sizeof(block_header_t);
                tail->free = 1;
                tail->next = block->next;
                *link = tail;                 /* remainder takes block's slot */
                block->size = need;
            } else {
                *link = block->next;          /* unlink the whole block */
            }
            block->free = 0;
            block->next = NULL;
            return (void *)(block + 1);
        }

        if (grown || !heap_grow(heap, need))
            return NULL;
    }
}
