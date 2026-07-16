#include "../include/irq.h"
#include "../include/core/irqchip/gicv2.h"
#include "../include/kprint.h"

/* GICv2 INTIDs run 0-1019 (1020-1023 are reserved/spurious), so a flat table
 * covers every routable interrupt with no hashing. Lives in BSS, zeroed
 * (NULL) before kernel_main runs. */
#define IRQ_MAX 1020

static irq_handler_t table[IRQ_MAX];

void irq_register(uint32_t irq, irq_handler_t fn)
{
    if (irq < IRQ_MAX)
        table[irq] = fn;
}

void irq_dispatch(void)
{
    uint32_t irq = gic_ack();
    if (irq == GIC_SPURIOUS_IRQ)
        return;

    if (irq < IRQ_MAX && table[irq]) {
        table[irq](irq);
    } else {
        kprint_puts("irq: unhandled IRQ ");
        kprint_dec(irq);
        kprint_putc('\n');
    }

    gic_eoi(irq);
}
