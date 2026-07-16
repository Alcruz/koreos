#ifndef _IRQ_H
#define _IRQ_H

#include <stdint.h>

/* Generic IRQ dispatch, sitting between the exception vector and each
 * device's own handler. Device drivers register against an interrupt ID
 * (the GIC INTID) without knowing anything about the GIC ack/EOI protocol or
 * the exception vector that got us here. */

typedef void (*irq_handler_t)(uint32_t irq);

/* Register `fn` to run when `irq` fires. */
void irq_register(uint32_t irq, irq_handler_t fn);

/* Acknowledge the pending interrupt, run its registered handler (or log it as
 * unhandled), and signal end-of-interrupt. Called from the IRQ exception
 * vectors; never call this from anywhere else. */
void irq_dispatch(void);

#endif /* _IRQ_H */
