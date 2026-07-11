/* Physical memory map: a sorted, non-overlapping list of typed ranges.
 *
 * This unifies "what RAM exists" and "what is reserved" into one structure:
 * reserved bytes are simply ranges whose kind is not MEM_USABLE.
 */

#include "../include/memmap.h"
#include "../include/align.h"

static inline uint64_t max_u64(uint64_t a, uint64_t b) { return a > b ? a : b; }
static inline uint64_t min_u64(uint64_t a, uint64_t b) { return a < b ? a : b; }

static void set_range(struct mem_range *r, uint64_t base, uint64_t size,
                      mem_kind_t kind)
{
    r->base = base;
    r->size = size;
    r->kind = kind;
}

/* Append one range to the end of `m`, skipping zero-length ranges. Returns
 * false if the map is full. */
static bool push(memmap_t *m, uint64_t base, uint64_t size, mem_kind_t kind)
{
    if (size == 0)
        return true;
    if (m->count >= MAX_MEM_RANGES)
        return false;
    set_range(&m->range[m->count], base, size, kind);
    m->count++;
    return true;
}

void memmap_init(memmap_t *m)
{
    m->count = 0;
}

bool memmap_add_ram(memmap_t *m, uint64_t base, uint64_t size)
{
    if (size == 0)
        return true;
    if (m->count >= MAX_MEM_RANGES)
        return false;

    /* Find the sorted insertion point (first range with a greater base). */
    size_t idx = m->count;
    for (size_t i = 0; i < m->count; i++)
    {
        if (base < m->range[i].base)
        {
            idx = i;
            break;
        }
    }

    /* Shift the tail up by one to open a slot at idx. */
    for (size_t i = m->count; i > idx; i--)
        set_range(&m->range[i], m->range[i - 1].base, m->range[i - 1].size,
                  m->range[i - 1].kind);

    set_range(&m->range[idx], base, size, MEM_USABLE);
    m->count++;
    return true;
}

uint64_t memmap_find_free(const memmap_t *m, uint64_t size, uint64_t align)
{
    for (size_t i = 0; i < m->count; i++)
    {
        if (m->range[i].kind != MEM_USABLE)
            continue;
        uint64_t base = m->range[i].base;
        uint64_t end = base + m->range[i].size;
        uint64_t cur = align_up(base, align);
        if (cur + size <= end)
            return cur;
    }
    return 0;
}

bool memmap_reserve(memmap_t *m, uint64_t base, uint64_t size)
{
    if (size == 0)
        return true;

    uint64_t rb = base;
    uint64_t re = base + size;

    /* Rebuild into a temporary so a capacity failure leaves `m` untouched. */
    memmap_t tmp;
    tmp.count = 0;

    for (size_t i = 0; i < m->count; i++)
    {
        uint64_t b = m->range[i].base;
        uint64_t e = b + m->range[i].size;
        mem_kind_t k = m->range[i].kind;

        /* Untouched: not usable, or no overlap with [rb, re). */
        if (k != MEM_USABLE || re <= b || rb >= e)
        {
            if (!push(&tmp, b, m->range[i].size, k))
                return false;
            continue;
        }

        /* Usable range crossed by the reservation: split into up to three. */
        uint64_t os = max_u64(b, rb); /* overlap start */
        uint64_t oe = min_u64(e, re); /* overlap end   */

        if (!push(&tmp, b, os - b, MEM_USABLE)) /* left  (b < rb)  */
            return false;
        if (!push(&tmp, os, oe - os, MEM_RESERVED)) /* reserved middle */
            return false;
        if (!push(&tmp, oe, e - oe, MEM_USABLE)) /* right (e > re)  */
            return false;
    }

    /* Commit. */
    m->count = tmp.count;
    for (size_t i = 0; i < tmp.count; i++)
        set_range(&m->range[i], tmp.range[i].base, tmp.range[i].size,
                  tmp.range[i].kind);
    return true;
}
