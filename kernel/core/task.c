#include "../include/task.h"

/* Task ids are monotonic and never reused; there's no task exit path yet to
 * make reuse meaningful. */
static uint32_t next_id = 1;

task_t *task_create(pmm_t *pmm, heap_t *heap)
{
    task_t *task = kzalloc(heap, sizeof(task_t));
    if (!task)
        return NULL;

    pmm_page_t *stack = pmm_alloc_contig(pmm, TASK_STACK_PAGES);
    if (!stack) {
        kfree(heap, task);
        return NULL;
    }

    task->stack_base = stack;
    task->state = TASK_READY;
    task->id = next_id++;
    task->ctx.sp = (uint64_t)(uintptr_t)stack + TASK_STACK_SIZE;
    return task;
}

void task_destroy(pmm_t *pmm, heap_t *heap, task_t *task)
{
    if (!task)
        return;
    pmm_free_contig(pmm, (pmm_page_t *)task->stack_base, TASK_STACK_PAGES);
    kfree(heap, task);
}
