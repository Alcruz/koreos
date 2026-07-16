#include "../../include/core/timer/timer.h"

/* Reload value in timer ticks, computed once from CNTFRQ_EL0 at init time. */
static uint32_t reload_ticks;

void timer_init(uint32_t hz)
{
    uint64_t freq;
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(freq));
    reload_ticks = (uint32_t)(freq / hz);

    __asm__ volatile("msr cntp_tval_el0, %0" :: "r"((uint64_t)reload_ticks));
    /* CNTP_CTL_EL0: bit 0 ENABLE, bit 1 IMASK (0 = unmasked at the timer). */
    __asm__ volatile("msr cntp_ctl_el0, %0" :: "r"((uint64_t)1));
}

void timer_reload(void)
{
    __asm__ volatile("msr cntp_tval_el0, %0" :: "r"((uint64_t)reload_ticks));
}
