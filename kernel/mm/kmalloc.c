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
