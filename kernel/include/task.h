#ifndef _TASK_H
#define _TASK_H

#include <stdint.h>
#include "pmm.h"
#include "kmalloc.h"

/* Kernel task: a stack plus enough saved state to eventually resume it. This
 * header defines the data structure and its allocation; nothing here wires a
 * task up to any code yet. Priming ctx.lr to a start-up trampoline and
 * actually swapping between tasks is switch_to's job (context-switch phase),
 * since that phase owns the resume mechanism this state feeds. The run-queue
 * link is unused until the scheduler phase links tasks together. */

typedef enum {
    TASK_READY,
    TASK_RUNNING,
} task_state_t;

/* Callee-saved integer register file (AAPCS64 x19-x28, fp, lr) plus sp — the
 * minimal state a context switch must save/restore. Caller-saved registers
 * are already spilled by the compiler across any call that can block. */
typedef struct cpu_context {
    uint64_t x19, x20, x21, x22, x23, x24, x25, x26, x27, x28;
    uint64_t fp; /* x29 */
    uint64_t lr; /* x30 */
    uint64_t sp;
} cpu_context_t;

/* 16 KiB kernel stack: page-aligned and a page-multiple size so the region
 * can later have a guard page unmapped directly below it. */
#define TASK_STACK_PAGES 4
#define TASK_STACK_SIZE  (TASK_STACK_PAGES * PAGE_SIZE)

typedef struct task {
    cpu_context_t ctx;
    void *stack_base; /* lowest address of the TASK_STACK_PAGES region */
    task_state_t state;
    uint32_t id;
    struct task *next; /* intrusive run-queue link */
} task_t;

/* Allocate a task struct from `heap` and a TASK_STACK_PAGES kernel stack from
 * `pmm` (physically contiguous, via pmm_alloc_contig). Sets state = READY, a
 * monotonically increasing id, and ctx.sp to the top of the stack; the rest
 * of ctx is zeroed. Returns NULL, freeing anything already allocated, if
 * either allocation fails. */
task_t *task_create(pmm_t *pmm, heap_t *heap);

/* Free a task created by task_create(): returns its stack to `pmm` and the
 * struct to `heap`. `task` may be NULL (no-op). */
void task_destroy(pmm_t *pmm, heap_t *heap, task_t *task);

/* Save prev's callee-saved integer context (x19-x28, fp, lr, sp) into
 * prev->ctx, restore next->ctx into the live registers, and return via the
 * newly loaded lr. Caller-saved registers are the compiler's responsibility
 * across this call. Both pointers must be non-NULL. */
void switch_to(task_t *prev, task_t *next);

/* Signature of a kernel task's entry function. */
typedef void (*task_entry_fn)(void *arg);

/* Prime `task` (fresh from task_create) so the next switch_to into it enters
 * `fn(arg)` on its own kernel stack. Sets ctx.lr to the trampoline and stashes
 * fn/arg in the callee-saved slots switch_to restores. */
void task_start(task_t *task, task_entry_fn fn, void *arg);

#endif /* _TASK_H */
