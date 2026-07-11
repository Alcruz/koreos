/* Kernel heap allocator: an explicit free list of variable-size blocks.
 *
 * Each block carries a 16-byte header (block_header_t) sitting immediately
 * before its payload; free blocks are chained through header.next in an
 * address-ordered list. Backing memory comes from the physical frame allocator:
 * the heap grows a frame at a time on demand rather than reserving a fixed
 * arena up front.
 *
 * Blocks are split on allocation and coalesced with adjacent free neighbours on
 * free, so the list holds at most one free block per contiguous free region.
 * kzalloc still lands in a later task.
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

/* One past the block's last payload byte: where the next physically adjacent
 * block would begin. */
static inline uint8_t *block_end(const block_header_t *b)
{
    return (uint8_t *)(b + 1) + b->size;
}

/* Splice `block` into the address-ordered free list and merge it with either
 * immediate neighbour it is physically contiguous with, so a run of freed
 * blocks collapses back into one. Absorbing a neighbour reclaims that
 * neighbour's header as payload too. Returns the surviving (possibly merged)
 * block. */
static block_header_t *freelist_insert(heap_t *heap, block_header_t *block)
{
    block->free = 1;

    /* Locate the insertion point, tracking the predecessor so we can check both
     * sides for adjacency. */
    block_header_t *prev = NULL;
    block_header_t *next = heap->free_list;
    while (next && next < block) {
        prev = next;
        next = next->next;
    }

    block->next = next;
    if (prev)
        prev->next = block;
    else
        heap->free_list = block;

    /* Merge forward: block's end meets next's start. */
    if (next && block_end(block) == (uint8_t *)next) {
        block->size += sizeof(block_header_t) + next->size;
        block->next = next->next;
    }

    /* Merge backward: prev's end meets block's start. */
    if (prev && block_end(prev) == (uint8_t *)block) {
        prev->size += sizeof(block_header_t) + block->size;
        prev->next = block->next;
        block = prev;
    }

    return block;
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
    return freelist_insert(heap, block);
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

void kfree(heap_t *heap, void *ptr)
{
    if (!ptr)
        return;

    block_header_t *block = (block_header_t *)ptr - 1;

    /* Guard against a double free: re-inserting a block already on the list
     * would corrupt it. A live allocation always has free == 0. */
    if (block->free)
        return;

    freelist_insert(heap, block);
}

void *kzalloc(heap_t *heap, size_t size)
{
    void *ptr = kmalloc(heap, size);
    if (ptr) {
        uint8_t *b = ptr;
        for (size_t i = 0; i < size; i++)
            b[i] = 0;
    }
    return ptr;
}

size_t heap_free_bytes(const heap_t *heap)
{
    size_t total = 0;
    for (const block_header_t *b = heap->free_list; b; b = b->next)
        total += b->size;
    return total;
}
