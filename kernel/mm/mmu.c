#include "../include/mmu.h"
#include "../drivers/serial/pl011.h"   /* UART_BASE: the MMIO page we map */

/* Load the memory attribute indirection register. An ISB afterwards forces the
 * write to take effect before subsequent context-dependent instructions —
 * translation setup that follows relies on this table already being in place. */
void mmu_init_mair(void)
{
    __asm__ volatile("msr mair_el1, %0" :: "r"((uint64_t)MAIR_EL1_VALUE));
    __asm__ volatile("isb");
}

/* Table index for `va` at translation level 0..3. Each level consumes 9 VA
 * bits; L3 starts at bit 12 (the 4 KiB page offset), so level L starts at
 * bit 12 + (3 - L) * 9. */
static uint64_t pt_index(uint64_t va, int level)
{
    int shift = 12 + (3 - level) * 9;
    return (va >> shift) & (PT_ENTRIES - 1);
}

/* Allocate one zeroed 4 KiB frame to use as a translation table. The pmm hands
 * back uninitialised memory, so every descriptor must be cleared to 0 (invalid)
 * before use. Returns NULL if no frame is free. */
static uint64_t *alloc_table(pmm_t *pmm)
{
    uint64_t *tbl = (uint64_t *)pmm_alloc_page(pmm);
    if (!tbl)
        return NULL;
    for (int i = 0; i < PT_ENTRIES; i++)
        tbl[i] = 0;
    return tbl;
}

/* Return the next-level table that `*pte` points to, allocating and linking one
 * if the entry is not yet valid. Only called where a table (never a block) is
 * expected. Returns NULL if a needed table could not be allocated. */
static uint64_t *next_table(uint64_t *pte, pmm_t *pmm)
{
    if (*pte & PTE_VALID)
        return (uint64_t *)(*pte & PTE_ADDR_MASK);

    uint64_t *tbl = alloc_table(pmm);
    if (!tbl)
        return NULL;
    *pte = ((uint64_t)tbl & PTE_ADDR_MASK) | PTE_TABLE;
    return tbl;
}

/* Identity-map [va, va + size) to the same physical addresses with attributes
 * `attr`. Uses a 2 MiB L2 block whenever the current address and remaining span
 * allow, otherwise a 4 KiB L3 page. `va`, `pa` and `size` must be 4 KiB
 * aligned. Returns false if the pmm ran out of frames mid-build. */
static bool map_range(uint64_t *l0, uint64_t va, uint64_t pa, uint64_t size,
                      uint64_t attr, pmm_t *pmm)
{
    uint64_t end = va + size;

    while (va < end)
    {
        uint64_t *l1 = next_table(&l0[pt_index(va, 0)], pmm);
        if (!l1)
            return false;
        uint64_t *l2 = next_table(&l1[pt_index(va, 1)], pmm);
        if (!l2)
            return false;

        /* A 2 MiB block fits when both VA and PA are 2 MiB aligned (they move
         * in lockstep for an identity map) and at least 2 MiB remains. */
        if (((va | pa) & (BLOCK_2M_SIZE - 1)) == 0 && end - va >= BLOCK_2M_SIZE)
        {
            l2[pt_index(va, 2)] = (pa & PTE_ADDR_MASK) | attr | PTE_BLOCK;
            va += BLOCK_2M_SIZE;
            pa += BLOCK_2M_SIZE;
            continue;
        }

        uint64_t *l3 = next_table(&l2[pt_index(va, 2)], pmm);
        if (!l3)
            return false;
        l3[pt_index(va, 3)] = (pa & PTE_ADDR_MASK) | attr | PTE_PAGE;
        va += PAGE_SIZE;
        pa += PAGE_SIZE;
    }

    return true;
}

uint64_t *mmu_build_page_tables(const memmap_t *map, pmm_t *pmm)
{
    uint64_t *l0 = alloc_table(pmm);
    if (!l0)
        return NULL;

    /* Map every RAM range as Normal cacheable memory. Reserved ranges (kernel
     * image, DTB, pmm bitmap) are still RAM the kernel executes from and reads,
     * so they must be mapped too — the memory *type* is the same for all RAM;
     * usable-vs-reserved is only a concern for the frame allocator. */
    for (size_t i = 0; i < map->count; i++)
        if (!map_range(l0, map->range[i].base, map->range[i].base,
                       map->range[i].size, PTE_NORMAL, pmm))
            return NULL;

    /* Platform MMIO: the PL011 UART occupies a single 4 KiB register page. */
    if (!map_range(l0, UART_BASE, UART_BASE, PAGE_SIZE, PTE_DEVICE, pmm))
        return NULL;

    return l0;
}

uint64_t mmu_lookup(const uint64_t *root, uint64_t va)
{
    const uint64_t *tbl = root;

    for (int level = 0; level <= 3; level++)
    {
        uint64_t desc = tbl[pt_index(va, level)];
        if (!(desc & PTE_VALID))
            return 0;

        /* Bit 1 set means "table" at L0-L2 and "page" at L3. */
        bool table_bit = (desc & PTE_TABLE) == PTE_TABLE;

        if (level == 3)
            return table_bit ? desc : 0;   /* only 0b11 is a valid L3 page */
        if (!table_bit)
            return desc;                   /* 0b01 block: a leaf at L1/L2  */

        tbl = (const uint64_t *)(desc & PTE_ADDR_MASK);
    }

    return 0;
}
