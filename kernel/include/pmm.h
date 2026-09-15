#ifndef _PMM_H
#define _PMM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "memmap.h"

#define PAGE_SHIFT 12
#define PAGE_SIZE  (1UL << PAGE_SHIFT)   /* 4 KiB physical frames */

/* One physical frame's worth of storage. Allocation hands back a pointer to
 * one of these: sizeof(pmm_page_t) == PAGE_SIZE, so the type documents "exactly
 * one frame" and pointer arithmetic steps a whole frame at a time. It points at
 * the frame's bytes, not a descriptor — there is no per-frame metadata. */
typedef struct pmm_page {
    uint8_t bytes[PAGE_SIZE];
} pmm_page_t;

/* State of one bitmap frame allocator. A single bitmap spans the whole managed
 * range [base, end); one bit per frame, 1 = in use. Callers treat this as
 * opaque and pass it to the pmm_* entry points; pmm_init fills it in. */
typedef struct pmm {
    uint8_t *bitmap;            /* one bit per frame, 1 = used */
    uint64_t base;             /* physical address of frame 0 */
    uint64_t end;              /* one past the last managed byte */
    size_t total_frames;
    size_t free_count;
    size_t alloc_cursor;        /* next-fit search hint */
} pmm_t;

/* Bring up the frame allocator over `map`, writing its state to *out. The
 * allocator provisions its own metadata: it finds space in a MEM_USABLE range,
 * reserves it in `map` (so `map` is updated), and manages the rest. Frames in
 * MEM_USABLE ranges become free; reserved ranges and the gaps between ranges
 * stay out of circulation. Returns false if `map` has no ranges, or no usable
 * range large enough for the metadata — the caller decides how to fail. */
bool pmm_init(memmap_t *map, pmm_t *out);

/* Allocate one 4 KiB frame. Returns a pointer to it, or NULL if no free frame
 * is available. The returned memory is not zeroed. */
pmm_page_t *pmm_alloc_page(pmm_t *pmm);

/* Return a frame previously handed out by pmm_alloc_page(). Pointers outside
 * managed RAM, or not frame-aligned, are ignored. */
void pmm_free_page(pmm_t *pmm, pmm_page_t *page);

/* Allocate `count` physically contiguous 4 KiB frames (a first-fit scan for a
 * run of free bits). Returns the base frame, or NULL if no such run exists or
 * count is 0 — the bitmap is untouched on failure. The returned memory is not
 * zeroed. Callers that need a whole, page-aligned physical region (e.g. a
 * kernel stack a guard page can later be carved below) use this instead of
 * pmm_alloc_page(), which only guarantees one frame at a time. */
pmm_page_t *pmm_alloc_contig(pmm_t *pmm, size_t count);

/* Return `count` frames previously handed out by pmm_alloc_contig() as a
 * single run starting at `base`. Same bounds/alignment guards as
 * pmm_free_page(), applied per frame. */
void pmm_free_contig(pmm_t *pmm, pmm_page_t *base, size_t count);

size_t pmm_free_pages(const pmm_t *pmm);
size_t pmm_total_pages(const pmm_t *pmm);

#endif /* _PMM_H */
