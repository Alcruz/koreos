#ifndef _RUNQUEUE_H
#define _RUNQUEUE_H

#include "task.h"

/* FIFO run queue over task_t.next. The scheduler dequeues from the head to
 * pick the next task, and enqueues a preempted task at the tail — round-robin
 * falls out of that pair of operations. Single-CPU for now; a real spinlock
 * arrives with the synchronization phase. */

typedef struct runqueue {
    task_t *head;
    task_t *tail;
} runqueue_t;

void runqueue_init(runqueue_t *rq);

/* Append `t` to the tail. `t->next` is cleared. */
void runqueue_enqueue(runqueue_t *rq, task_t *t);

/* Detach and return the head, or NULL if empty. The returned task's `next`
 * link is cleared so callers can safely re-enqueue it. */
task_t *runqueue_dequeue(runqueue_t *rq);

#endif /* _RUNQUEUE_H */
