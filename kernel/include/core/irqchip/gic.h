#ifndef _GIC_H
#define _GIC_H

#include <stdint.h>

/* GICv2 register offsets from each bank's base. The bases themselves are a
 * board property, resolved from the DTB at boot and handed to gic_init — never
 * a hardcoded address here. */

/* Distributor (GICD) — offsets from gicd_base. */
#define GICD_CTLR 0x000                 /* Distributor Control Register */

#define GICD_CTLR_ENABLE (1 << 0)       /* Forward pending interrupts to the CPUs */

/* CPU interface (GICC) — offsets from gicc_base. */
#define GICC_CTLR 0x000                 /* CPU Interface Control Register */
#define GICC_PMR 0x004                  /* Interrupt Priority Mask Register */

#define GICC_CTLR_ENABLE (1 << 0)       /* Signal interrupts to this core */
#define GICC_PMR_ALL 0xFF               /* Lowest mask: let every priority through */

/* Bring up the GICv2 interrupt controller. Unlike the UART, a GICv2 has two
 * register windows — the distributor (GICD) and this core's CPU interface
 * (GICC) — so init takes both bases (resolved from the DTB by the caller). */
void gic_init(uintptr_t gicd_base, uintptr_t gicc_base);

#endif /* _GIC_H */
