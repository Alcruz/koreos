#include <stddef.h>

#include "../include/runqueue.h"

void runqueue_init(runqueue_t *rq)
{
    rq->head = NULL;
    rq->tail = NULL;
}

void runqueue_enqueue(runqueue_t *rq, task_t *t)
{
    t->next = NULL;
    if (rq->tail)
        rq->tail->next = t;
    else
        rq->head = t;
    rq->tail = t;
}

task_t *runqueue_dequeue(runqueue_t *rq)
{
    task_t *t = rq->head;
    if (!t)
        return NULL;
    rq->head = t->next;
    if (!rq->head)
        rq->tail = NULL;
    t->next = NULL;
    return t;
}
