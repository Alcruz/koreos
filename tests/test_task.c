/* Host-side unit tests for kernel tasks (kernel/core/task.c).
 *
 * Tasks pull their stack from the physical frame allocator and their struct
 * from the kernel heap, so — as in the kmalloc suite — these need genuine
 * memory behind every address. We stand in real "RAM" with a page-aligned
 * host buffer, bring a PMM and heap up over it, then exercise task_create/
 * task_destroy through the sanitizer.
 */

#include <stdlib.h>
#include <stdint.h>

#include "unity.h"
#include "pmm.h"
#include "kmalloc.h"
#include "task.h"
#include "memmap.h"

#define RAM_BYTES (16 * 1024 * 1024) /* 16 MiB of pretend RAM */

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

static int in_ram(const void *p)
{
    uintptr_t a = (uintptr_t)p, base = (uintptr_t)ram;
    return a >= base && a < base + RAM_BYTES;
}

static void test_task_create_gives_aligned_stack_and_sp(void)
{
    task_t *t = task_create(&pmm, &heap);
    TEST_ASSERT_NOT_NULL(t);

    TEST_ASSERT_TRUE(in_ram(t->stack_base));
    TEST_ASSERT_EQUAL_UINT64(0, (uintptr_t)t->stack_base % PAGE_SIZE);
    TEST_ASSERT_EQUAL_UINT64(0, t->ctx.sp % 16);
    TEST_ASSERT_EQUAL_UINT64((uint64_t)(uintptr_t)t->stack_base + TASK_STACK_SIZE,
                              t->ctx.sp);
    TEST_ASSERT_EQUAL_INT(TASK_READY, t->state);
    TEST_ASSERT_NULL(t->next);

    task_destroy(&pmm, &heap, t);
}

static void test_task_create_assigns_increasing_ids(void)
{
    task_t *a = task_create(&pmm, &heap);
    task_t *b = task_create(&pmm, &heap);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);
    TEST_ASSERT_TRUE(b->id > a->id);

    task_destroy(&pmm, &heap, a);
    task_destroy(&pmm, &heap, b);
}

static void test_task_create_stacks_do_not_overlap(void)
{
    task_t *a = task_create(&pmm, &heap);
    task_t *b = task_create(&pmm, &heap);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);

    uintptr_t a_lo = (uintptr_t)a->stack_base, a_hi = a_lo + TASK_STACK_SIZE;
    uintptr_t b_lo = (uintptr_t)b->stack_base, b_hi = b_lo + TASK_STACK_SIZE;
    TEST_ASSERT_TRUE(a_hi <= b_lo || b_hi <= a_lo);

    task_destroy(&pmm, &heap, a);
    task_destroy(&pmm, &heap, b);
}

static void test_task_create_and_destroy_round_trip_frames_and_heap(void)
{
    /* Force the heap's one-time first-growth frame (kmalloc.h: "no frames are
     * reserved up front") before measuring baselines, so it isn't mistaken
     * for stack accounting below — mirrors production, where main.c's
     * heap_smoke() already primed the heap before task creation runs. */
    void *warm = kmalloc(&heap, 1);
    TEST_ASSERT_NOT_NULL(warm);
    kfree(&heap, warm);

    size_t pages_before = pmm_free_pages(&pmm);
    size_t heap_before = heap_free_bytes(&heap);

    task_t *a = task_create(&pmm, &heap);
    task_t *b = task_create(&pmm, &heap);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);
    TEST_ASSERT_EQUAL_UINT64(pages_before - 2 * TASK_STACK_PAGES, pmm_free_pages(&pmm));

    task_destroy(&pmm, &heap, a);
    task_destroy(&pmm, &heap, b);

    TEST_ASSERT_EQUAL_UINT64(pages_before, pmm_free_pages(&pmm));
    TEST_ASSERT_EQUAL_UINT64(heap_before, heap_free_bytes(&heap));
}

static void test_task_destroy_ignores_null(void)
{
    task_destroy(&pmm, &heap, NULL); /* must not crash */
}

static void test_task_stack_is_real_writable_memory(void)
{
    task_t *t = task_create(&pmm, &heap);
    TEST_ASSERT_NOT_NULL(t);

    uint8_t *stack = (uint8_t *)t->stack_base;
    stack[0] = 0xAA;
    stack[TASK_STACK_SIZE - 1] = 0x55;
    TEST_ASSERT_EQUAL_UINT8(0xAA, stack[0]);
    TEST_ASSERT_EQUAL_UINT8(0x55, stack[TASK_STACK_SIZE - 1]);

    task_destroy(&pmm, &heap, t);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_task_create_gives_aligned_stack_and_sp);
    RUN_TEST(test_task_create_assigns_increasing_ids);
    RUN_TEST(test_task_create_stacks_do_not_overlap);
    RUN_TEST(test_task_create_and_destroy_round_trip_frames_and_heap);
    RUN_TEST(test_task_destroy_ignores_null);
    RUN_TEST(test_task_stack_is_real_writable_memory);
    return UNITY_END();
}
