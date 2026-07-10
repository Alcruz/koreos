/* Physical memory manager: a bitmap frame allocator over 4 KiB pages.
 *
 * The caller supplies a memory map (see core/main.c) with usable RAM and any
 * already-occupied ranges marked reserved. pmm_init provisions the allocator's
 * own bitmap out of usable RAM and reserves it in the map; beyond that this
 * module knows nothing about the kernel image, the device tree, or how RAM was
 * discovered. A single bitmap spans the whole physical range [base, end); one
 * bit per frame, where 1 means "in use". Reserved ranges and holes between
 * ranges stay marked used, so they are never handed out.
 */

#include "../include/pmm.h"

#define ALIGN_DOWN(x) ((x) & ~(PAGE_SIZE - 1))
#define ALIGN_UP(x)   (((x) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))

static inline void bm_set(pmm_t *p, size_t i)   { p->bitmap[i >> 3] |= (uint8_t)(1u << (i & 7)); }
static inline void bm_clear(pmm_t *p, size_t i) { p->bitmap[i >> 3] &= (uint8_t)~(1u << (i & 7)); }
static inline int  bm_test(const pmm_t *p, size_t i) { return (p->bitmap[i >> 3] >> (i & 7)) & 1; }

/* --- bitmap range helpers ---------------------------------------------- */

/* Mark every whole frame fully contained in [start, end) as free. */
static void free_range(pmm_t *p, uint64_t start, uint64_t end)
{
    start = ALIGN_UP(start);
    end = ALIGN_DOWN(end);
    for (uint64_t a = start; a < end; a += PAGE_SIZE) {
        size_t i = (a - p->base) >> PAGE_SHIFT;
        if (i < p->total_frames && bm_test(p, i)) {
            bm_clear(p, i);
            p->free_count++;
        }
    }
}

/* --- public API -------------------------------------------------------- */

/* Lowest base and highest end across every range in the map, regardless of
 * kind. The bitmap spans this whole extent so frame indices are contiguous;
 * reserved ranges and gaps within it simply stay marked used. */
static void map_span(const memmap_t *map, uint64_t *lo, uint64_t *hi)
{
    *lo = map->range[0].base;
    *hi = map->range[0].base + map->range[0].size;
    for (size_t i = 1; i < map->count; i++) {
        if (map->range[i].base < *lo)
            *lo = map->range[i].base;
        if (map->range[i].base + map->range[i].size > *hi)
            *hi = map->range[i].base + map->range[i].size;
    }
}

/* Bytes of bitmap storage needed to span the whole map: one bit per 4 KiB
 * frame from the lowest range base to the highest range end. */
static size_t bitmap_size(const memmap_t *map)
{
    uint64_t lo, hi;
    map_span(map, &lo, &hi);
    size_t total = (size_t)((ALIGN_DOWN(hi) - ALIGN_DOWN(lo)) >> PAGE_SHIFT);
    return (total + 7) / 8;
}

bool pmm_init(memmap_t *map, pmm_t *out)
{
    if (map->count == 0)
        return false;

    /* Provision our own bitmap out of usable RAM and reserve it in the map so
     * it is never handed out. */
    size_t need = bitmap_size(map);
    uint64_t storage = memmap_find_free(map, need, PAGE_SIZE);
    if (storage == 0)
        return false;
    if (!memmap_reserve(map, storage, need))
        return false;

    /* One bitmap spans the whole physical extent. */
    uint64_t lo, hi;
    map_span(map, &lo, &hi);

    out->base = ALIGN_DOWN(lo);
    out->end = ALIGN_DOWN(hi);
    out->total_frames = (size_t)((out->end - out->base) >> PAGE_SHIFT);
    out->bitmap = (uint8_t *)(uintptr_t)storage;

    /* Start with everything used, then free only the usable ranges. Reserved
     * ranges (including the bitmap just reserved) and inter-range holes are
     * never freed, so they stay out of circulation with no separate pass. */
    for (size_t i = 0; i < need; i++)
        out->bitmap[i] = 0xff;
    out->free_count = 0;

    for (size_t i = 0; i < map->count; i++)
        if (map->range[i].kind == MEM_USABLE)
            free_range(out, map->range[i].base,
                       map->range[i].base + map->range[i].size);

    out->alloc_cursor = 0;
    return true;
}

page_t *pmm_alloc_page(pmm_t *pmm)
{
    for (size_t step = 0; step < pmm->total_frames; step++) {
        size_t i = pmm->alloc_cursor + step;
        if (i >= pmm->total_frames)
            i -= pmm->total_frames;
        if (!bm_test(pmm, i)) {
            bm_set(pmm, i);
            pmm->free_count--;
            pmm->alloc_cursor = i + 1;
            return (page_t *)(uintptr_t)(pmm->base + ((uint64_t)i << PAGE_SHIFT));
        }
    }
    return NULL;
}

void pmm_free_page(pmm_t *pmm, page_t *page)
{
    uint64_t p = (uint64_t)(uintptr_t)page;
    if (p < pmm->base || p >= pmm->end || (p & (PAGE_SIZE - 1)))
        return;
    size_t i = (size_t)((p - pmm->base) >> PAGE_SHIFT);
    if (bm_test(pmm, i)) {
        bm_clear(pmm, i);
        pmm->free_count++;
    }
}

size_t pmm_free_pages(const pmm_t *pmm)  { return pmm->free_count; }
size_t pmm_total_pages(const pmm_t *pmm) { return pmm->total_frames; }
