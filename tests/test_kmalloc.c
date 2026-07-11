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

/* True iff all `n` bytes of `p` equal `v`. A failed stamp check means two live
 * blocks overlapped or a split/coalesce corrupted a neighbour. */
static int stamped(const void *p, uint8_t v, size_t n)
{
    const uint8_t *b = p;
    for (size_t i = 0; i < n; i++)
        if (b[i] != v)
            return 0;
    return 1;
}

/* Deterministic LCG so the stress churn is reproducible across runs (no
 * dependence on the C library's rand()). Numerical Recipes constants. */
static uint32_t lcg(uint32_t *s)
{
    *s = *s * 1664525u + 1013904223u;
    return *s;
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

/* --- leaks, corruption & stress ---------------------------------------- */

/* An oversized request — larger than all of RAM — must fail gracefully (NULL,
 * no crash) and leave the heap fully usable for the next allocation. This is a
 * different path from draining page-by-page: the size itself is rejected. */
static void test_oversized_request_fails_gracefully(void)
{
    TEST_ASSERT_NULL(kmalloc(&heap, (size_t)1 << 40));

    void *after = kmalloc(&heap, 32);
    TEST_ASSERT_NOT_NULL(after);
    kfree(&heap, after);
}

/* Leak check: once the heap has grown, an alloc-everything / free-everything
 * cycle must return the free pool to the exact same size every time. A drifting
 * total means a block or a split header went missing on the round trip. */
static void test_repeated_full_cycle_is_leak_free(void)
{
    enum { N = 8 };
    static const size_t sizes[N] = { 16, 24, 64, 100, 500, 1000, 4096, 9000 };
    void *p[N];

    /* Warm the heap up to its working size, then record the baseline. */
    for (int i = 0; i < N; i++)
        p[i] = kmalloc(&heap, sizes[i]);
    for (int i = 0; i < N; i++) {
        TEST_ASSERT_NOT_NULL(p[i]);
        kfree(&heap, p[i]);
    }
    size_t baseline = heap_free_bytes(&heap);

    /* Every subsequent identical cycle must reclaim back to that baseline. */
    for (int cycle = 0; cycle < 5; cycle++) {
        for (int i = 0; i < N; i++)
            p[i] = kmalloc(&heap, sizes[i]);
        for (int i = 0; i < N; i++)
            kfree(&heap, p[i]);
        TEST_ASSERT_EQUAL_UINT64(baseline, heap_free_bytes(&heap));
    }
}

/* Corruption check: freeing and reallocating a subset of live blocks must not
 * disturb the blocks left in place. Each block carries a unique byte stamp; if
 * a realloc handed back memory overlapping a survivor, the survivor's stamp
 * would be clobbered. */
static void test_interleaved_free_preserves_survivors(void)
{
    enum { N = 8 };
    static const size_t sizes[N] = { 16, 24, 64, 100, 500, 1000, 4096, 9000 };
    void *p[N];

    for (int i = 0; i < N; i++) {
        p[i] = kmalloc(&heap, sizes[i]);
        TEST_ASSERT_NOT_NULL(p[i]);
        TEST_ASSERT_EQUAL_UINT64(0, (uintptr_t)p[i] % HEAP_ALIGN);
        fill(p[i], (uint8_t)(i + 1), sizes[i]);
    }

    /* Free the even-indexed blocks, then reallocate the same sizes. The churn
     * reuses the just-freed holes. */
    for (int i = 0; i < N; i += 2)
        kfree(&heap, p[i]);
    for (int i = 0; i < N; i += 2) {
        p[i] = kmalloc(&heap, sizes[i]);
        TEST_ASSERT_NOT_NULL(p[i]);
        fill(p[i], (uint8_t)(i + 1), sizes[i]);
    }

    /* The odd-indexed blocks were never freed; their stamps must be intact. */
    for (int i = 1; i < N; i += 2)
        TEST_ASSERT_TRUE(stamped(p[i], (uint8_t)(i + 1), sizes[i]));

    for (int i = 0; i < N; i++)
        kfree(&heap, p[i]);
}

/* One deterministic churn run: `rounds` of pseudo-random alloc/free across a
 * pool of slots, every live block stamped and re-verified on each touch so any
 * overlap or header corruption surfaces as a bad stamp (and ASan/UBSan watch
 * every access). Drains the pool at the end and returns the resulting free
 * total. The heap only ever grows (frames are never returned to the PMM), so
 * two runs from the same seed must drain to the same total — a leak would force
 * the second run to grow further. */
static size_t stress_churn(uint32_t seed, int rounds)
{
    enum { SLOTS = 64 };
    struct { void *ptr; size_t size; uint8_t stamp; } slot[SLOTS] = {0};
    uint32_t rng = seed;
    uint8_t next_stamp = 1;

    for (int r = 0; r < rounds; r++) {
        int i = lcg(&rng) % SLOTS;
        if (slot[i].ptr) {
            /* Occupied: the stamp must still be intact, then free it. */
            TEST_ASSERT_TRUE(stamped(slot[i].ptr, slot[i].stamp, slot[i].size));
            kfree(&heap, slot[i].ptr);
            slot[i].ptr = NULL;
        } else {
            /* Empty: allocate a varied size, stamp every byte, verify readback. */
            size_t size = 1 + (lcg(&rng) % 2000);
            void *p = kmalloc(&heap, size);
            if (!p)
                continue;                       /* transient exhaustion is fine */
            TEST_ASSERT_EQUAL_UINT64(0, (uintptr_t)p % HEAP_ALIGN);
            TEST_ASSERT_TRUE(in_ram(p, size));
            uint8_t s = next_stamp++;
            if (next_stamp == 0)
                next_stamp = 1;                 /* keep stamps nonzero */
            fill(p, s, size);
            slot[i].ptr = p;
            slot[i].size = size;
            slot[i].stamp = s;
        }
    }

    /* Drain the pool; surviving stamps must still be intact on the way out. */
    for (int i = 0; i < SLOTS; i++) {
        if (slot[i].ptr) {
            TEST_ASSERT_TRUE(stamped(slot[i].ptr, slot[i].stamp, slot[i].size));
            kfree(&heap, slot[i].ptr);
        }
    }

    return heap_free_bytes(&heap);
}

static void test_stress_churn_keeps_integrity(void)
{
    /* Two identical deterministic runs. Each verifies stamps throughout; the
     * matching drained totals prove the churn neither corrupted nor leaked
     * (a lost block would make the second run grow the heap further). */
    size_t first = stress_churn(0xC0FFEEu, 20000);
    size_t second = stress_churn(0xC0FFEEu, 20000);
    TEST_ASSERT_EQUAL_UINT64(first, second);
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
    RUN_TEST(test_oversized_request_fails_gracefully);
    RUN_TEST(test_repeated_full_cycle_is_leak_free);
    RUN_TEST(test_interleaved_free_preserves_survivors);
    RUN_TEST(test_stress_churn_keeps_integrity);
    return UNITY_END();
}
