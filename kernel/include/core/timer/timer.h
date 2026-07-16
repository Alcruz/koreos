#ifndef _TIMER_H
#define _TIMER_H

#include <stdint.h>

/* ARM generic timer, non-secure EL1 physical timer (CNTP). Unlike the UART
 * or GIC, this isn't a DTB-resolved MMIO device — it's accessed through
 * system registers (CNTP_*_EL0) and wired to the GIC as a per-core PPI, fixed
 * by the architecture at INTID 30 (banked, so no distributor routing needed). */
#define TIMER_IRQ 30

/* Arm CNTP to fire `hz` times per second, derived from CNTFRQ_EL0, and enable
 * it. The IRQ still needs enabling at the GIC (gic_enable_irq) and the core
 * (unmasking IRQs in DAIF) before it is actually delivered. */
void timer_init(uint32_t hz);

/* Re-arm the countdown for one more period at the rate passed to timer_init.
 * CNTP_TVAL_EL0 is a one-shot down-counter, not auto-reloading, so call this
 * from the IRQ handler every time it fires to keep the timer periodic. */
void timer_reload(void);

#endif /* _TIMER_H */
