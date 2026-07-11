/* Host-side unit tests for the physical memory map (kernel/mm/memmap.c).
 *
 * memmap is pure address bookkeeping: it sorts, splits and searches ranges but
 * never dereferences any address it stores. So every address here is fabricated
 * — the numbers only have to be internally consistent, not point at real memory.
 * The memmap_t layout is public, so tests read m->count and m->range[i] directly
 * to check the exact shape of the map after each operation.
 */

#include <stdint.h>

#include "unity.h"
#include "memmap.h"

void setUp(void) {}
void tearDown(void) {}

/* Assert that range `i` is exactly [base, base+size) with kind `kind`. */
static void assert_range(const memmap_t *m, size_t i, uint64_t base,
                         uint64_t size, mem_kind_t kind)
{
    TEST_ASSERT_TRUE(i < m->count);
    TEST_ASSERT_EQUAL_UINT64(base, m->range[i].base);
    TEST_ASSERT_EQUAL_UINT64(size, m->range[i].size);
    TEST_ASSERT_EQUAL_INT(kind, m->range[i].kind);
}

/* --- init & add_ram ---------------------------------------------------- */

static void test_init_empties_the_map(void)
{
    memmap_t m;
    m.count = 123;                  /* garbage that init must clear */
    memmap_init(&m);
    TEST_ASSERT_EQUAL_UINT64(0, m.count);
}

static void test_add_ram_keeps_ranges_sorted_by_base(void)
{
    memmap_t m;
    memmap_init(&m);

    /* Insert deliberately out of order. */
    TEST_ASSERT_TRUE(memmap_add_ram(&m, 0x8000, 0x1000));
    TEST_ASSERT_TRUE(memmap_add_ram(&m, 0x1000, 0x1000));
    TEST_ASSERT_TRUE(memmap_add_ram(&m, 0x4000, 0x1000));

    TEST_ASSERT_EQUAL_UINT64(3, m.count);
    assert_range(&m, 0, 0x1000, 0x1000, MEM_USABLE);
    assert_range(&m, 1, 0x4000, 0x1000, MEM_USABLE);
    assert_range(&m, 2, 0x8000, 0x1000, MEM_USABLE);
}

static void test_add_ram_skips_zero_length(void)
{
    memmap_t m;
    memmap_init(&m);
    TEST_ASSERT_TRUE(memmap_add_ram(&m, 0x1000, 0));   /* reports success... */
    TEST_ASSERT_EQUAL_UINT64(0, m.count);              /* ...but adds nothing */
}

static void test_add_ram_fails_at_capacity_leaving_map_unchanged(void)
{
    memmap_t m;
    memmap_init(&m);
    for (size_t i = 0; i < MAX_MEM_RANGES; i++)
        TEST_ASSERT_TRUE(memmap_add_ram(&m, (i + 1) * 0x10000, 0x1000));
    TEST_ASSERT_EQUAL_UINT64(MAX_MEM_RANGES, m.count);

    /* One past capacity: rejected, and nothing else disturbed. */
    TEST_ASSERT_FALSE(memmap_add_ram(&m, 0x900000000ULL, 0x1000));
    TEST_ASSERT_EQUAL_UINT64(MAX_MEM_RANGES, m.count);
    assert_range(&m, 0, 0x10000, 0x1000, MEM_USABLE);
}

/* --- find_free --------------------------------------------------------- */

static void test_find_free_returns_aligned_address_in_a_usable_range(void)
{
    memmap_t m;
    memmap_init(&m);
    memmap_add_ram(&m, 0x1000, 0x8000);

    /* base is already 0x1000-aligned, so the first fit is the range base. */
    TEST_ASSERT_EQUAL_UINT64(0x1000, memmap_find_free(&m, 0x1000, 0x1000));
}

static void test_find_free_rounds_base_up_to_alignment(void)
{
    memmap_t m;
    memmap_init(&m);
    memmap_add_ram(&m, 0x1200, 0x4000);    /* base not 0x1000-aligned */

    /* Must skip forward to the next aligned address inside the range. */
    TEST_ASSERT_EQUAL_UINT64(0x2000, memmap_find_free(&m, 0x100, 0x1000));
}

static void test_find_free_returns_zero_when_nothing_fits(void)
{
    memmap_t m;
    memmap_init(&m);
    memmap_add_ram(&m, 0x1000, 0x1000);    /* only one page */

    TEST_ASSERT_EQUAL_UINT64(0, memmap_find_free(&m, 0x2000, 0x1000));
}

static void test_find_free_skips_reserved_ranges(void)
{
    memmap_t m;
    memmap_init(&m);
    memmap_add_ram(&m, 0x1000, 0x1000);
    memmap_add_ram(&m, 0x2000, 0x4000);
    TEST_ASSERT_TRUE(memmap_reserve(&m, 0x1000, 0x1000));  /* reserve the first */

    /* The reserved first page is unusable; the fit comes from the second range. */
    TEST_ASSERT_EQUAL_UINT64(0x2000, memmap_find_free(&m, 0x1000, 0x1000));
}

/* --- reserve ----------------------------------------------------------- */

static void test_reserve_splits_interior_into_three(void)
{
    memmap_t m;
    memmap_init(&m);
    memmap_add_ram(&m, 0x1000, 0x4000);           /* [0x1000, 0x5000)      */

    TEST_ASSERT_TRUE(memmap_reserve(&m, 0x2000, 0x1000));  /* carve the middle */

    TEST_ASSERT_EQUAL_UINT64(3, m.count);
    assert_range(&m, 0, 0x1000, 0x1000, MEM_USABLE);      /* left  */
    assert_range(&m, 1, 0x2000, 0x1000, MEM_RESERVED);    /* middle */
    assert_range(&m, 2, 0x3000, 0x2000, MEM_USABLE);      /* right */
}

static void test_reserve_at_range_start_has_no_left_part(void)
{
    memmap_t m;
    memmap_init(&m);
    memmap_add_ram(&m, 0x1000, 0x4000);

    TEST_ASSERT_TRUE(memmap_reserve(&m, 0x1000, 0x1000));

    TEST_ASSERT_EQUAL_UINT64(2, m.count);
    assert_range(&m, 0, 0x1000, 0x1000, MEM_RESERVED);
    assert_range(&m, 1, 0x2000, 0x3000, MEM_USABLE);
}

static void test_reserve_covering_whole_range_leaves_one_reserved(void)
{
    memmap_t m;
    memmap_init(&m);
    memmap_add_ram(&m, 0x1000, 0x4000);

    /* Reservation extends past both ends; only the in-RAM part is reserved. */
    TEST_ASSERT_TRUE(memmap_reserve(&m, 0x0, 0x100000));

    TEST_ASSERT_EQUAL_UINT64(1, m.count);
    assert_range(&m, 0, 0x1000, 0x4000, MEM_RESERVED);
}

static void test_reserve_zero_size_is_a_noop(void)
{
    memmap_t m;
    memmap_init(&m);
    memmap_add_ram(&m, 0x1000, 0x4000);

    TEST_ASSERT_TRUE(memmap_reserve(&m, 0x2000, 0));

    TEST_ASSERT_EQUAL_UINT64(1, m.count);
    assert_range(&m, 0, 0x1000, 0x4000, MEM_USABLE);
}

static void test_reserve_in_a_gap_changes_nothing(void)
{
    memmap_t m;
    memmap_init(&m);
    memmap_add_ram(&m, 0x1000, 0x1000);
    memmap_add_ram(&m, 0x8000, 0x1000);

    /* [0x4000, 0x5000) lies in the gap between the two ranges. */
    TEST_ASSERT_TRUE(memmap_reserve(&m, 0x4000, 0x1000));

    TEST_ASSERT_EQUAL_UINT64(2, m.count);
    assert_range(&m, 0, 0x1000, 0x1000, MEM_USABLE);
    assert_range(&m, 1, 0x8000, 0x1000, MEM_USABLE);
}

static void test_reserve_is_idempotent_on_already_reserved(void)
{
    memmap_t m;
    memmap_init(&m);
    memmap_add_ram(&m, 0x1000, 0x4000);
    TEST_ASSERT_TRUE(memmap_reserve(&m, 0x2000, 0x1000));
    size_t after_first = m.count;

    /* Reserving the same span again finds only already-reserved bytes to touch. */
    TEST_ASSERT_TRUE(memmap_reserve(&m, 0x2000, 0x1000));

    TEST_ASSERT_EQUAL_UINT64(after_first, m.count);
    assert_range(&m, 1, 0x2000, 0x1000, MEM_RESERVED);
}

static void test_reserve_fails_when_split_would_overflow(void)
{
    memmap_t m;
    memmap_init(&m);

    /* Fill to one below capacity; the final range is large enough to split. */
    for (size_t i = 0; i < MAX_MEM_RANGES - 1; i++)
        TEST_ASSERT_TRUE(memmap_add_ram(&m, (i + 1) * 0x10000, 0x3000));
    TEST_ASSERT_EQUAL_UINT64(MAX_MEM_RANGES - 1, m.count);

    /* Splitting the last range into three needs two more slots than we have. */
    uint64_t last = (uint64_t)(MAX_MEM_RANGES - 1) * 0x10000;
    TEST_ASSERT_FALSE(memmap_reserve(&m, last + 0x1000, 0x1000));

    /* Failure must leave the map exactly as it was — still all usable. */
    TEST_ASSERT_EQUAL_UINT64(MAX_MEM_RANGES - 1, m.count);
    assert_range(&m, MAX_MEM_RANGES - 2, last, 0x3000, MEM_USABLE);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_empties_the_map);
    RUN_TEST(test_add_ram_keeps_ranges_sorted_by_base);
    RUN_TEST(test_add_ram_skips_zero_length);
    RUN_TEST(test_add_ram_fails_at_capacity_leaving_map_unchanged);
    RUN_TEST(test_find_free_returns_aligned_address_in_a_usable_range);
    RUN_TEST(test_find_free_rounds_base_up_to_alignment);
    RUN_TEST(test_find_free_returns_zero_when_nothing_fits);
    RUN_TEST(test_find_free_skips_reserved_ranges);
    RUN_TEST(test_reserve_splits_interior_into_three);
    RUN_TEST(test_reserve_at_range_start_has_no_left_part);
    RUN_TEST(test_reserve_covering_whole_range_leaves_one_reserved);
    RUN_TEST(test_reserve_zero_size_is_a_noop);
    RUN_TEST(test_reserve_in_a_gap_changes_nothing);
    RUN_TEST(test_reserve_is_idempotent_on_already_reserved);
    RUN_TEST(test_reserve_fails_when_split_would_overflow);
    return UNITY_END();
}
