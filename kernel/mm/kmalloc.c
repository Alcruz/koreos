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

block_header_t *heap_grow(heap_t *heap, size_t need)
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
