#ifndef _GICV2_H
#define _GICV2_H

#include <stdint.h>

/* GICv2 register offsets from each bank's base. The bases themselves are a
 * board property, resolved from the DTB at boot and handed to gic_init — never
 * a hardcoded address here. */

/* Distributor (GICD) — offsets from gicd_base. The ISENABLER/ICENABLER arrays
 * are banked one 32-bit word per 32 interrupts; IPRIORITYR/ITARGETSR are one
 * byte per interrupt (byte-addressed). */
#define GICD_CTLR 0x000                 /* Distributor Control Register        */
#define GICD_ISENABLER 0x100            /* Set-Enable, 1 bit/IRQ, 32/word       */
#define GICD_ICENABLER 0x180            /* Clear-Enable, 1 bit/IRQ, 32/word     */
#define GICD_IPRIORITYR 0x400           /* Priority, 1 byte/IRQ                 */
#define GICD_ITARGETSR 0x800            /* CPU targets, 1 byte/IRQ              */
#define GICD_ICFGR 0xC00                /* Config (edge/level), 2 bits/IRQ      */

#define GICD_CTLR_ENABLE (1 << 0)       /* Forward pending interrupts to the CPUs */

/* CPU interface (GICC) — offsets from gicc_base. */
#define GICC_CTLR 0x000                 /* CPU Interface Control Register       */
#define GICC_PMR 0x004                  /* Interrupt Priority Mask Register     */
#define GICC_BPR 0x008                  /* Binary Point Register                */
#define GICC_IAR 0x00C                  /* Interrupt Acknowledge Register       */
#define GICC_EOIR 0x010                 /* End Of Interrupt Register            */

#define GICC_CTLR_ENABLE (1 << 0)       /* Signal interrupts to this core       */
#define GICC_PMR_ALL 0xFF               /* Lowest mask: let every priority through */
#define GICC_BPR_MIN 0x00               /* No sub-priority grouping (minimal)   */
#define GICC_IAR_ID_MASK 0x3FF          /* Interrupt ID lives in bits [9:0]     */

/* First Shared Peripheral Interrupt. IDs 0-15 are SGIs and 16-31 are PPIs,
 * both banked per-CPU; 32+ are SPIs routable to specific cores. */
#define GIC_SPI_BASE 32

/* Returned by gic_ack when there is no pending interrupt to service. */
#define GIC_SPURIOUS_IRQ 1023

/* Bring up the GICv2 interrupt controller. Unlike the UART, a GICv2 has two
 * register windows — the distributor (GICD) and this core's CPU interface
 * (GICC) — so init takes both bases (resolved from the DTB by the caller). */
void gic_init(uintptr_t gicd_base, uintptr_t gicc_base);

/* Enable/disable forwarding of a single interrupt at the distributor. */
void gic_enable_irq(uint32_t irq);
void gic_disable_irq(uint32_t irq);

/* Set an interrupt's priority (lower value = higher priority). */
void gic_set_priority(uint32_t irq, uint8_t prio);

/* Route an SPI to the CPUs in `cpu_mask` (bit N = CPU N). SGIs/PPIs (irq < 32)
 * are implicitly banked to the requesting core, so this is a no-op for them. */
void gic_set_target(uint32_t irq, uint8_t cpu_mask);

/* Acknowledge the highest-priority pending interrupt and return its ID, or
 * GIC_SPURIOUS_IRQ if none is pending. Pair every non-spurious ack with a
 * matching gic_eoi. */
uint32_t gic_ack(void);

/* Signal end-of-interrupt for `irq` (the value returned by gic_ack). */
void gic_eoi(uint32_t irq);

#endif /* _GICV2_H */
