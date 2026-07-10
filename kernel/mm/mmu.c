#include "../include/mmu.h"

/* Load the memory attribute indirection register. An ISB afterwards forces the
 * write to take effect before subsequent context-dependent instructions —
 * translation setup that follows relies on this table already being in place. */
void mmu_init_mair(void)
{
    __asm__ volatile("msr mair_el1, %0" :: "r"((uint64_t)MAIR_EL1_VALUE));
    __asm__ volatile("isb");
}
