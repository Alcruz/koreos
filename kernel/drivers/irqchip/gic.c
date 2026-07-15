#include "../../include/core/irqchip/gic.h"
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

    /* Open this core's CPU interface: unmask all priorities, then enable it so
     * the GIC actually signals IRQs to the core. */
    mmio_write(gicc_base + GICC_PMR, GICC_PMR_ALL);
    mmio_write(gicc_base + GICC_CTLR, GICC_CTLR_ENABLE);
}
