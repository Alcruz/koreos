#include <stddef.h>

#include "../include/task.h"

/* switch.S hardcodes cpu_context_t offsets; keep the two in lockstep. */
_Static_assert(offsetof(task_t, ctx) == 0,          "switch.S assumes ctx at task offset 0");
_Static_assert(offsetof(cpu_context_t, x19) == 0,   "switch.S offset x19");
_Static_assert(offsetof(cpu_context_t, fp)  == 80,  "switch.S offset fp");
_Static_assert(offsetof(cpu_context_t, lr)  == 88,  "switch.S offset lr");
_Static_assert(offsetof(cpu_context_t, sp)  == 96,  "switch.S offset sp");
_Static_assert(sizeof(cpu_context_t)        == 104, "switch.S size cpu_context_t");

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

/* Defined in arch/arm64/task/trampoline.S; called via ctx.lr on first entry. */
extern void task_trampoline(void);

void task_start(task_t *task, task_entry_fn fn, void *arg)
{
    task->ctx.x19 = (uint64_t)(uintptr_t)fn;
    task->ctx.x20 = (uint64_t)(uintptr_t)arg;
    task->ctx.lr  = (uint64_t)(uintptr_t)&task_trampoline;
}
