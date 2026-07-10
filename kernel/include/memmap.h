#ifndef _MEMMAP_H
#define _MEMMAP_H

#include "types.h"

/* Kind of a physical range. */
typedef enum
{
    MEM_USABLE = 0, /* free RAM the allocator may hand out              */
    MEM_RESERVED,   /* occupied: kernel image, DTB, the bitmap, ...     */
} mem_kind_t;

/* A contiguous physical range [base, base + size) with a kind. */
struct mem_range
{
    uint64_t base;
    uint64_t size;
    mem_kind_t kind;
};

/* Capacity. A reservation can split one usable range into three, so this must
 * bound the number of *ranges after splitting*, not just the RAM banks. */
#define MAX_MEM_RANGES 32

/* A physical memory map: a list of ranges kept sorted by base and
 * non-overlapping. Gaps between consecutive ranges are implicitly non-RAM and
 * are never allocatable. This single structure carries both what RAM exists
 * and which parts of it are reserved (via each range's kind). */
typedef struct memmap
{
    struct mem_range range[MAX_MEM_RANGES];
    size_t count;
} memmap_t;

/* Reset the map to empty. */
void memmap_init(memmap_t *m);

/* Add a span of usable RAM (e.g. one DTB /memory region), inserted in sorted
 * position. Assumes the span does not overlap ranges already in the map, which
 * holds for the disjoint regions a device tree describes. Returns false (and
 * leaves the map unchanged) if it is at capacity.
 *
 * Out param `m` is first per the codebase convention; it is the object being
 * built, not an output-only pointer. */
bool memmap_add_ram(memmap_t *m, uint64_t base, uint64_t size);

/* Lowest `align`-aligned address inside a MEM_USABLE range where `size` bytes
 * fit contiguously, or 0 if there is nowhere. `align` must be a power of two.
 * Read-only. Reserved bytes are already carved out of the usable ranges, so a
 * usable range is entirely free. */
uint64_t memmap_find_free(const memmap_t *m, uint64_t size, uint64_t align);

/* Mark the portion of [base, base + size) that currently falls in usable RAM
 * as MEM_RESERVED, splitting the usable ranges it crosses (into up to a left
 * usable part, the reserved middle, and a right usable part). Portions of the
 * request that lie in gaps or already-reserved ranges are ignored, since those
 * are non-allocatable already. Returns false (leaving the map unchanged) if
 * the splitting would exceed MAX_MEM_RANGES. */
bool memmap_reserve(memmap_t *m, uint64_t base, uint64_t size);

#endif /* _MEMMAP_H */
