#include "smoke.h"
#include "../../kernel/include/task.h"

static smoke_env_t env;

void kernel_main(void *dtb)
{
    smoke_boot(dtb, &env);

    /* Warm the heap: the first task_create grows the heap out of the PMM
     * (kmalloc grabs whole frames on demand), which would skew the round-trip
     * accounting below. After one create/destroy the heap has capacity and the
     * next allocation is served from the free list. */
    task_t *warm = task_create(env.pmm, env.heap);
    if (!warm) smoke_fail("task", "warm task_create returned NULL");
    task_destroy(env.pmm, env.heap, warm);

    size_t pages_before = pmm_free_pages(env.pmm);
    size_t heap_before = heap_free_bytes(env.heap);

    task_t *a = task_create(env.pmm, env.heap);
    task_t *b = task_create(env.pmm, env.heap);
    if (!a || !b) smoke_fail("task", "task_create returned NULL");
    if (a == b) smoke_fail("task", "same task twice");
    if (a->id == b->id) smoke_fail("task", "duplicate id");
    if ((uintptr_t)a->stack_base % PAGE_SIZE) smoke_fail("task", "a stack unaligned");
    if ((uintptr_t)b->stack_base % PAGE_SIZE) smoke_fail("task", "b stack unaligned");
    if (a->ctx.sp % 16) smoke_fail("task", "a sp unaligned");
    if (b->ctx.sp % 16) smoke_fail("task", "b sp unaligned");

    uintptr_t a_lo = (uintptr_t)a->stack_base, a_hi = a_lo + TASK_STACK_SIZE;
    uintptr_t b_lo = (uintptr_t)b->stack_base, b_hi = b_lo + TASK_STACK_SIZE;
    if (!(a_hi <= b_lo || b_hi <= a_lo)) smoke_fail("task", "stacks overlap");

    if (pmm_free_pages(env.pmm) != pages_before - 2 * TASK_STACK_PAGES)
        smoke_fail("task", "pmm not decremented by 2*stack_pages");

    task_destroy(env.pmm, env.heap, a);
    task_destroy(env.pmm, env.heap, b);

    if (pmm_free_pages(env.pmm) != pages_before)
        smoke_fail("task", "pmm not restored after destroy");
    if (heap_free_bytes(env.heap) != heap_before)
        smoke_fail("task", "heap not restored after destroy");

    smoke_pass("task");
}
