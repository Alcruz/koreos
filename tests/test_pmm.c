/* Host-side unit tests for the physical memory manager (kernel/mm/pmm.c).
 *
 * The PMM manages physical addresses and only ever dereferences one thing: its
 * own bitmap. So we stand in real "RAM" by allocating a page-aligned host
 * buffer and describing it to the allocator at its actual address — the bitmap
 * writes and every frame it hands back then land in genuine, sanitizer-tracked
 * memory. Addresses that the allocator never dereferences (e.g. in the
 * bitmap-won't-fit case, which fails before touching memory) can be fabricated.
 */

#include <stdlib.h>
#include <stdint.h>

#include "unity.h"
#include "pmm.h"
#include "memmap.h"

#define RAM_BYTES   (16 * 1024 * 1024)          /* 16 MiB of pretend RAM     */
#define RAM_FRAMES  (RAM_BYTES / PAGE_SIZE)     /* 4096 frames               */

/* Page-aligned backing memory standing in for physical RAM, fresh per test. */
static uint8_t *ram;

void setUp(void)
{
    ram = aligned_alloc(PAGE_SIZE, RAM_BYTES);
    TEST_ASSERT_NOT_NULL(ram);
}

void tearDown(void)
{
    free(ram);
    ram = NULL;
}

/* Describe `ram` as one usable region at its real address. */
static void map_over(memmap_t *map)
{
    memmap_init(map);
    memmap_add_ram(map, (uint64_t)(uintptr_t)ram, RAM_BYTES);
}

/* True if `page` lies inside [ram, ram + RAM_BYTES). */
static int in_ram(const void *page)
{
    uintptr_t p = (uintptr_t)page, base = (uintptr_t)ram;
    return p >= base && p < base + RAM_BYTES;
}

/* --- bring-up ---------------------------------------------------------- */

static void test_pmm_init_reports_expected_counts(void)
{
    memmap_t map;
    map_over(&map);

    pmm_t pmm;
    TEST_ASSERT_TRUE(pmm_init(&map, &pmm));

    TEST_ASSERT_EQUAL_UINT64(RAM_FRAMES, pmm_total_pages(&pmm));
    /* Exactly one frame is spent on the bitmap (512 bytes rounds up to a whole
     * frame that never re-enters circulation). */
    TEST_ASSERT_EQUAL_UINT64(RAM_FRAMES - 1, pmm_free_pages(&pmm));
}

static void test_pmm_init_fails_on_empty_map(void)
{
    memmap_t map;
    memmap_init(&map);              /* no ranges at all */

    pmm_t pmm;
    TEST_ASSERT_FALSE(pmm_init(&map, &pmm));
}

static void test_pmm_init_fails_when_bitmap_wont_fit(void)
{
    /* Two one-frame usable regions 4 GiB apart: the bitmap must span the whole
     * extent (~128 KiB) but neither region can hold it, so init fails. The
     * addresses are never dereferenced because find_free returns 0 first. */
    memmap_t map;
    memmap_init(&map);
    memmap_add_ram(&map, 0x1000, PAGE_SIZE);
    memmap_add_ram(&map, 0x100000000ULL, PAGE_SIZE);

    pmm_t pmm;
    TEST_ASSERT_FALSE(pmm_init(&map, &pmm));
}

/* --- allocation -------------------------------------------------------- */

static void test_pmm_alloc_is_aligned_in_range_and_distinct(void)
{
    memmap_t map;
    map_over(&map);
    pmm_t pmm;
    TEST_ASSERT_TRUE(pmm_init(&map, &pmm));

    size_t before = pmm_free_pages(&pmm);
    pmm_page_t *a = pmm_alloc_page(&pmm);
    pmm_page_t *b = pmm_alloc_page(&pmm);
    pmm_page_t *c = pmm_alloc_page(&pmm);

    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);
    TEST_ASSERT_NOT_NULL(c);
    TEST_ASSERT_TRUE(in_ram(a) && in_ram(b) && in_ram(c));
    TEST_ASSERT_EQUAL_UINT64(0, (uintptr_t)a % PAGE_SIZE);
    TEST_ASSERT_EQUAL_UINT64(0, (uintptr_t)b % PAGE_SIZE);
    TEST_ASSERT_EQUAL_UINT64(0, (uintptr_t)c % PAGE_SIZE);
    TEST_ASSERT_TRUE(a != b && b != c && a != c);
    TEST_ASSERT_EQUAL_UINT64(before - 3, pmm_free_pages(&pmm));

    /* Handed-back frames are real, writable memory. */
    a->bytes[0] = 0xAA;
    c->bytes[PAGE_SIZE - 1] = 0x55;
    TEST_ASSERT_EQUAL_UINT8(0xAA, a->bytes[0]);
    TEST_ASSERT_EQUAL_UINT8(0x55, c->bytes[PAGE_SIZE - 1]);
}

static void test_pmm_free_returns_frame_to_pool(void)
{
    memmap_t map;
    map_over(&map);
    pmm_t pmm;
    TEST_ASSERT_TRUE(pmm_init(&map, &pmm));

    size_t before = pmm_free_pages(&pmm);
    pmm_page_t *p = pmm_alloc_page(&pmm);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_EQUAL_UINT64(before - 1, pmm_free_pages(&pmm));

    pmm_free_page(&pmm, p);
    TEST_ASSERT_EQUAL_UINT64(before, pmm_free_pages(&pmm));

    /* Pool is whole again, so another allocation must succeed. */
    TEST_ASSERT_NOT_NULL(pmm_alloc_page(&pmm));
}

static void test_pmm_exhaustion_then_recovery(void)
{
    memmap_t map;
    map_over(&map);
    pmm_t pmm;
    TEST_ASSERT_TRUE(pmm_init(&map, &pmm));

    size_t free_at_start = pmm_free_pages(&pmm);

    /* Draining the pool yields exactly free_at_start frames, then NULL. */
    pmm_page_t *last = NULL;
    size_t got = 0;
    for (;;) {
        pmm_page_t *p = pmm_alloc_page(&pmm);
        if (!p)
            break;
        TEST_ASSERT_TRUE(in_ram(p));
        last = p;
        got++;
        TEST_ASSERT_TRUE(got <= free_at_start);   /* never over-hand-out */
    }
    TEST_ASSERT_EQUAL_UINT64(free_at_start, got);
    TEST_ASSERT_EQUAL_UINT64(0, pmm_free_pages(&pmm));
    TEST_ASSERT_NULL(pmm_alloc_page(&pmm));

    /* Free one and the allocator recovers exactly that one frame. */
    pmm_free_page(&pmm, last);
    TEST_ASSERT_EQUAL_UINT64(1, pmm_free_pages(&pmm));
    TEST_ASSERT_NOT_NULL(pmm_alloc_page(&pmm));
    TEST_ASSERT_EQUAL_UINT64(0, pmm_free_pages(&pmm));
}

/* --- robustness -------------------------------------------------------- */

static void test_pmm_free_ignores_invalid_pointers(void)
{
    memmap_t map;
    map_over(&map);
    pmm_t pmm;
    TEST_ASSERT_TRUE(pmm_init(&map, &pmm));

    size_t before = pmm_free_pages(&pmm);

    pmm_free_page(&pmm, NULL);                              /* below base    */
    pmm_free_page(&pmm, (pmm_page_t *)(ram + 1));           /* misaligned    */
    pmm_free_page(&pmm, (pmm_page_t *)(ram + RAM_BYTES));   /* one past end  */

    TEST_ASSERT_EQUAL_UINT64(before, pmm_free_pages(&pmm)); /* nothing changed */
}

static void test_pmm_double_free_is_idempotent(void)
{
    memmap_t map;
    map_over(&map);
    pmm_t pmm;
    TEST_ASSERT_TRUE(pmm_init(&map, &pmm));

    pmm_page_t *p = pmm_alloc_page(&pmm);
    TEST_ASSERT_NOT_NULL(p);

    pmm_free_page(&pmm, p);
    size_t after_first = pmm_free_pages(&pmm);
    pmm_free_page(&pmm, p);                                 /* already free  */
    TEST_ASSERT_EQUAL_UINT64(after_first, pmm_free_pages(&pmm));  /* no bump */
}

static void test_pmm_reserved_region_never_allocated(void)
{
    memmap_t map;
    map_over(&map);

    /* Reserve a 64 KiB hole one megabyte in, before bringing the PMM up. */
    uint64_t hole_base = (uint64_t)(uintptr_t)ram + (1 << 20);
    uint64_t hole_size = 64 * 1024;
    TEST_ASSERT_TRUE(memmap_reserve(&map, hole_base, hole_size));

    pmm_t pmm;
    TEST_ASSERT_TRUE(pmm_init(&map, &pmm));

    /* Drain the pool; no frame may fall inside the reserved hole. */
    for (;;) {
        pmm_page_t *p = pmm_alloc_page(&pmm);
        if (!p)
            break;
        uintptr_t a = (uintptr_t)p;
        TEST_ASSERT_TRUE(a < hole_base || a >= hole_base + hole_size);
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_pmm_init_reports_expected_counts);
    RUN_TEST(test_pmm_init_fails_on_empty_map);
    RUN_TEST(test_pmm_init_fails_when_bitmap_wont_fit);
    RUN_TEST(test_pmm_alloc_is_aligned_in_range_and_distinct);
    RUN_TEST(test_pmm_free_returns_frame_to_pool);
    RUN_TEST(test_pmm_exhaustion_then_recovery);
    RUN_TEST(test_pmm_free_ignores_invalid_pointers);
    RUN_TEST(test_pmm_double_free_is_idempotent);
    RUN_TEST(test_pmm_reserved_region_never_allocated);
    return UNITY_END();
}
