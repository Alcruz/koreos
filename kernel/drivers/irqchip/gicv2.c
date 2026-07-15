#include "../../include/core/irqchip/gicv2.h"
#include "../../include/mmio.h"

/* Bases of the GICv2 register windows, resolved from the DTB and handed to
 * gic_init at boot. Zero until then. */
static uintptr_t gicd_base;             /* distributor   */
static uintptr_t gicc_base;             /* CPU interface */

void gic_init(uintptr_t gicd, uintptr_t gicc) {
    gicd_base = gicd;
    gicc_base = gicc;

    /* Bring the distributor up: enable forwarding of pending interrupts to the
     * CPU interfaces. Individual interrupt sources stay disabled until a driver
     * enables its line. */
    mmio_write(gicd_base + GICD_CTLR, GICD_CTLR_ENABLE);

    /* Open this core's CPU interface: unmask all priorities, use the finest
     * pre-emption grouping (no sub-priority split), then enable it so the GIC
     * actually signals IRQs to the core. */
    mmio_write(gicc_base + GICC_PMR, GICC_PMR_ALL);
    mmio_write(gicc_base + GICC_BPR, GICC_BPR_MIN);
    mmio_write(gicc_base + GICC_CTLR, GICC_CTLR_ENABLE);
}

void gic_enable_irq(uint32_t irq) {
    /* ISENABLER is write-1-to-set and banked 32 IRQs per word, so writing the
     * single target bit leaves the other 31 lines untouched. */
    mmio_write(gicd_base + GICD_ISENABLER + (irq / 32) * 4, 1u << (irq % 32));
}

void gic_disable_irq(uint32_t irq) {
    /* ICENABLER is the write-1-to-clear counterpart of ISENABLER. */
    mmio_write(gicd_base + GICD_ICENABLER + (irq / 32) * 4, 1u << (irq % 32));
}

void gic_set_priority(uint32_t irq, uint8_t prio) {
    /* IPRIORITYR is one byte per IRQ, addressed by byte offset. */
    mmio_write8(gicd_base + GICD_IPRIORITYR + irq, prio);
}

void gic_set_target(uint32_t irq, uint8_t cpu_mask) {
    /* Only SPIs are routable; SGIs and PPIs are banked to the requesting core,
     * and their ITARGETSR bytes are read-only. */
    if (irq < GIC_SPI_BASE)
        return;
    mmio_write8(gicd_base + GICD_ITARGETSR + irq, cpu_mask);
}

uint32_t gic_ack(void) {
    /* The IAR carries the source CPU ID in bits [12:10] for SGIs; mask it off
     * to leave just the interrupt ID. */
    return mmio_read(gicc_base + GICC_IAR) & GICC_IAR_ID_MASK;
}

void gic_eoi(uint32_t irq) {
    mmio_write(gicc_base + GICC_EOIR, irq);
}
