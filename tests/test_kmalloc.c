/* Host-side unit tests for the kernel heap (kernel/mm/kmalloc.c).
 *
 * The heap pulls its backing store from the physical frame allocator and writes
 * a block header into every frame it grows into, so — unlike the pure-arithmetic
 * memmap tests — these need genuine memory behind every address. As in the PMM
 * suite we stand in real "RAM" with a page-aligned host buffer, bring a PMM up
 * over it, then layer the heap on top; the sanitizer then watches every header
 * write and payload access the allocator makes.
 *
 * block_header_t is private to kmalloc.c, so tests can only observe the heap
 * through kmalloc/kfree/kzalloc and heap_free_bytes. The header is exactly one
 * HEAP_ALIGN unit (a static_assert in kmalloc.c pins this), which lets the
 * split/coalesce tests predict free-byte totals: a fresh one-page block holds
 * PAGE_SIZE - HEAP_ALIGN payload bytes, and each split spends one more header.
 */

#include <stdlib.h>
#include <stdint.h>

#include "unity.h"
#include "pmm.h"
#include "kmalloc.h"
#include "memmap.h"

/* Local byte-fill: the kernel's own <string.h> shadows the libc one on the
 * include path, so memset isn't available in this suite. */
static void fill(void *p, uint8_t v, size_t n)
{
    uint8_t *b = p;
    for (size_t i = 0; i < n; i++)
        b[i] = v;
}

#define RAM_BYTES   (16 * 1024 * 1024)          /* 16 MiB of pretend RAM     */

/* Payload of the single free block a fresh one-page grow yields. */
#define PAGE_PAYLOAD (PAGE_SIZE - HEAP_ALIGN)

static uint8_t *ram;
static pmm_t    pmm;
static heap_t   heap;

void setUp(void)
{
    ram = aligned_alloc(PAGE_SIZE, RAM_BYTES);
    TEST_ASSERT_NOT_NULL(ram);

    memmap_t map;
    memmap_init(&map);
    memmap_add_ram(&map, (uint64_t)(uintptr_t)ram, RAM_BYTES);
    TEST_ASSERT_TRUE(pmm_init(&map, &pmm));

    heap_init(&pmm, &heap);
}

void tearDown(void)
{
    free(ram);
    ram = NULL;
}

/* True if `p` lies inside the backing RAM buffer. */
static int in_ram(const void *p, size_t size)
{
    uintptr_t a = (uintptr_t)p, base = (uintptr_t)ram;
    return a >= base && a + size <= base + RAM_BYTES;
}

/* --- basic allocation -------------------------------------------------- */

static void test_kmalloc_returns_aligned_usable_memory(void)
{
    void *p = kmalloc(&heap, 100);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_EQUAL_UINT64(0, (uintptr_t)p % HEAP_ALIGN);
    TEST_ASSERT_TRUE(in_ram(p, 100));

    /* Every byte of the request is writable and read-backs are stable. */
    fill(p, 0xAB, 100);
    TEST_ASSERT_EQUAL_UINT8(0xAB, ((uint8_t *)p)[0]);
    TEST_ASSERT_EQUAL_UINT8(0xAB, ((uint8_t *)p)[99]);
}

static void test_kmalloc_zero_returns_null(void)
{
    TEST_ASSERT_NULL(kmalloc(&heap, 0));
}

static void test_kmalloc_hands_out_distinct_nonoverlapping_blocks(void)
{
    uint8_t *a = kmalloc(&heap, 64);
    uint8_t *b = kmalloc(&heap, 64);
    uint8_t *c = kmalloc(&heap, 64);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);
    TEST_ASSERT_NOT_NULL(c);

    /* Fill each with a distinct pattern; if any two overlapped, a later fill
     * would corrupt an earlier block's bytes. */
    fill(a, 0x11, 64);
    fill(b, 0x22, 64);
    fill(c, 0x33, 64);
    for (int i = 0; i < 64; i++) {
        TEST_ASSERT_EQUAL_UINT8(0x11, a[i]);
        TEST_ASSERT_EQUAL_UINT8(0x22, b[i]);
        TEST_ASSERT_EQUAL_UINT8(0x33, c[i]);
    }
}

static void test_kzalloc_zeroes_the_block(void)
{
    /* Dirty a block, free it, then kzalloc the same size: coalescing hands the
     * very same bytes back, so a clean read proves kzalloc zeroed them. */
    uint8_t *dirty = kmalloc(&heap, 128);
    TEST_ASSERT_NOT_NULL(dirty);
    fill(dirty, 0xFF, 128);
    kfree(&heap, dirty);

    uint8_t *z = kzalloc(&heap, 128);
    TEST_ASSERT_NOT_NULL(z);
    for (int i = 0; i < 128; i++)
        TEST_ASSERT_EQUAL_UINT8(0, z[i]);
}

/* --- splitting & growth ------------------------------------------------ */

static void test_first_alloc_grows_one_page_and_splits_off_remainder(void)
{
    TEST_ASSERT_EQUAL_UINT64(0, heap_free_bytes(&heap));   /* nothing yet */

    void *p = kmalloc(&heap, 16);
    TEST_ASSERT_NOT_NULL(p);

    /* One page grown; the 16-byte allocation and its header are carved off the
     * front, leaving a single remainder block on the free list. */
    TEST_ASSERT_EQUAL_UINT64(PAGE_PAYLOAD - 2 * HEAP_ALIGN,
                             heap_free_bytes(&heap));
}

static void test_large_alloc_spans_multiple_pages_contiguously(void)
{
    size_t big = 2 * PAGE_SIZE;                 /* needs >2 frames with header */
    uint8_t *p = kmalloc(&heap, big);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_TRUE(in_ram(p, big));

    /* Writing the first and last byte exercises the whole contiguous span. */
    p[0] = 0x5A;
    p[big - 1] = 0xA5;
    TEST_ASSERT_EQUAL_UINT8(0x5A, p[0]);
    TEST_ASSERT_EQUAL_UINT8(0xA5, p[big - 1]);
}

/* --- freeing & coalescing ---------------------------------------------- */

static void test_kfree_null_is_a_noop(void)
{
    void *p = kmalloc(&heap, 32);
    TEST_ASSERT_NOT_NULL(p);
    size_t before = heap_free_bytes(&heap);

    kfree(&heap, NULL);
    TEST_ASSERT_EQUAL_UINT64(before, heap_free_bytes(&heap));
}

static void test_double_free_is_ignored(void)
{
    void *p = kmalloc(&heap, 32);
    TEST_ASSERT_NOT_NULL(p);

    kfree(&heap, p);
    size_t after_first = heap_free_bytes(&heap);
    kfree(&heap, p);                             /* already free */
    TEST_ASSERT_EQUAL_UINT64(after_first, heap_free_bytes(&heap));
}

static void test_freeing_all_neighbours_coalesces_back_to_one_page(void)
{
    /* Three 16-byte blocks split off the same freshly grown page, so they are
     * physically adjacent. Freeing all three must merge them — and the trailing
     * remainder — back into a single whole-page free block. */
    void *a = kmalloc(&heap, 16);
    void *b = kmalloc(&heap, 16);
    void *c = kmalloc(&heap, 16);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);
    TEST_ASSERT_NOT_NULL(c);

    kfree(&heap, a);
    kfree(&heap, c);
    kfree(&heap, b);                             /* fills the gap; full merge */

    /* Fully coalesced: the free bytes are back to a pristine one-page block,
     * every split header reclaimed as payload. */
    TEST_ASSERT_EQUAL_UINT64(PAGE_PAYLOAD, heap_free_bytes(&heap));
}

static void test_free_then_reuse_serves_from_freed_space(void)
{
    /* After freeing everything back to one page, an allocation just under a full
     * page must be satisfied from that reclaimed block, not by growing again. */
    void *a = kmalloc(&heap, 512);
    void *b = kmalloc(&heap, 512);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);
    kfree(&heap, a);
    kfree(&heap, b);
    TEST_ASSERT_EQUAL_UINT64(PAGE_PAYLOAD, heap_free_bytes(&heap));

    /* A near-page request fits the coalesced block exactly; the free list
     * empties rather than growing a second page. */
    void *whole = kmalloc(&heap, PAGE_PAYLOAD);
    TEST_ASSERT_NOT_NULL(whole);
    TEST_ASSERT_EQUAL_UINT64(0, heap_free_bytes(&heap));
}

/* --- exhaustion -------------------------------------------------------- */

static void test_exhaustion_then_recovery(void)
{
    /* Drain the heap a page at a time until the PMM can grow no further. */
    void *blocks[RAM_BYTES / PAGE_SIZE];
    size_t n = 0;
    for (;;) {
        void *p = kmalloc(&heap, PAGE_SIZE);
        if (!p)
            break;
        TEST_ASSERT_TRUE(n < RAM_BYTES / PAGE_SIZE);
        blocks[n++] = p;
    }
    TEST_ASSERT_TRUE(n > 0);
    TEST_ASSERT_NULL(kmalloc(&heap, PAGE_SIZE));   /* genuinely dry */

    /* Give one block back and the heap can satisfy that size once more. */
    kfree(&heap, blocks[0]);
    void *again = kmalloc(&heap, PAGE_SIZE);
    TEST_ASSERT_NOT_NULL(again);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_kmalloc_returns_aligned_usable_memory);
    RUN_TEST(test_kmalloc_zero_returns_null);
    RUN_TEST(test_kmalloc_hands_out_distinct_nonoverlapping_blocks);
    RUN_TEST(test_kzalloc_zeroes_the_block);
    RUN_TEST(test_first_alloc_grows_one_page_and_splits_off_remainder);
    RUN_TEST(test_large_alloc_spans_multiple_pages_contiguously);
    RUN_TEST(test_kfree_null_is_a_noop);
    RUN_TEST(test_double_free_is_ignored);
    RUN_TEST(test_freeing_all_neighbours_coalesces_back_to_one_page);
    RUN_TEST(test_free_then_reuse_serves_from_freed_space);
    RUN_TEST(test_exhaustion_then_recovery);
    return UNITY_END();
}
