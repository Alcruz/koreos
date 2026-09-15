/* Host-side unit tests for the kernel run queue (kernel/core/runqueue.c).
 *
 * The run queue is a pure data structure over task_t.next — no MMIO, no CPU
 * state — so we exercise it directly on the host. Tasks used here come from
 * task_create() so their intrusive `next` links are set up exactly as the
 * scheduler will see them; that also lets ASan/UBSan watch the linkage.
 */

#include <stdlib.h>
#include <stdint.h>

#include "unity.h"
#include "pmm.h"
#include "kmalloc.h"
#include "task.h"
#include "memmap.h"
#include "runqueue.h"

#define RAM_BYTES (16 * 1024 * 1024)

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

/* Host stub for the arm64 asm trampoline referenced by task.c. */
void task_trampoline(void) {}

static void test_empty_dequeue_returns_null(void)
{
    runqueue_t rq;
    runqueue_init(&rq);
    TEST_ASSERT_NULL(runqueue_dequeue(&rq));
}

static void test_single_enqueue_dequeue_round_trip(void)
{
    runqueue_t rq;
    runqueue_init(&rq);

    task_t *a = task_create(&pmm, &heap);
    TEST_ASSERT_NOT_NULL(a);

    runqueue_enqueue(&rq, a);
    TEST_ASSERT_EQUAL_PTR(a, runqueue_dequeue(&rq));
    TEST_ASSERT_NULL(runqueue_dequeue(&rq));

    task_destroy(&pmm, &heap, a);
}

static void test_fifo_order(void)
{
    runqueue_t rq;
    runqueue_init(&rq);

    task_t *a = task_create(&pmm, &heap);
    task_t *b = task_create(&pmm, &heap);
    task_t *c = task_create(&pmm, &heap);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);
    TEST_ASSERT_NOT_NULL(c);

    runqueue_enqueue(&rq, a);
    runqueue_enqueue(&rq, b);
    runqueue_enqueue(&rq, c);

    TEST_ASSERT_EQUAL_PTR(a, runqueue_dequeue(&rq));
    TEST_ASSERT_EQUAL_PTR(b, runqueue_dequeue(&rq));
    TEST_ASSERT_EQUAL_PTR(c, runqueue_dequeue(&rq));
    TEST_ASSERT_NULL(runqueue_dequeue(&rq));

    task_destroy(&pmm, &heap, a);
    task_destroy(&pmm, &heap, b);
    task_destroy(&pmm, &heap, c);
}

static void test_round_robin_via_dequeue_then_enqueue(void)
{
    runqueue_t rq;
    runqueue_init(&rq);

    task_t *a = task_create(&pmm, &heap);
    task_t *b = task_create(&pmm, &heap);
    task_t *c = task_create(&pmm, &heap);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);
    TEST_ASSERT_NOT_NULL(c);

    runqueue_enqueue(&rq, a);
    runqueue_enqueue(&rq, b);
    runqueue_enqueue(&rq, c);

    /* Rotate the head to the tail six times; the sequence must cycle. */
    task_t *expected[6] = { a, b, c, a, b, c };
    for (int i = 0; i < 6; i++) {
        task_t *t = runqueue_dequeue(&rq);
        TEST_ASSERT_EQUAL_PTR(expected[i], t);
        runqueue_enqueue(&rq, t);
    }

    /* Drain to clean up. */
    (void)runqueue_dequeue(&rq);
    (void)runqueue_dequeue(&rq);
    (void)runqueue_dequeue(&rq);

    task_destroy(&pmm, &heap, a);
    task_destroy(&pmm, &heap, b);
    task_destroy(&pmm, &heap, c);
}

static void test_dequeued_task_has_null_next(void)
{
    /* Callers that re-enqueue a task shouldn't see a stale link back into
     * the queue's interior. */
    runqueue_t rq;
    runqueue_init(&rq);

    task_t *a = task_create(&pmm, &heap);
    task_t *b = task_create(&pmm, &heap);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);

    runqueue_enqueue(&rq, a);
    runqueue_enqueue(&rq, b);

    task_t *x = runqueue_dequeue(&rq);
    TEST_ASSERT_EQUAL_PTR(a, x);
    TEST_ASSERT_NULL(x->next);

    task_t *y = runqueue_dequeue(&rq);
    TEST_ASSERT_EQUAL_PTR(b, y);
    TEST_ASSERT_NULL(y->next);

    task_destroy(&pmm, &heap, a);
    task_destroy(&pmm, &heap, b);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_empty_dequeue_returns_null);
    RUN_TEST(test_single_enqueue_dequeue_round_trip);
    RUN_TEST(test_fifo_order);
    RUN_TEST(test_round_robin_via_dequeue_then_enqueue);
    RUN_TEST(test_dequeued_task_has_null_next);
    return UNITY_END();
}
