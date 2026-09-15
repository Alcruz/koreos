#include "smoke.h"
#include "../../kernel/include/mmio.h"
#include "../../kernel/include/core/irqchip/gicv2.h"

static smoke_env_t env;

void kernel_main(void *dtb)
{
    smoke_boot(dtb, &env);

    const uint32_t spi = 42; /* spare SPI, unused by any device we drive */
    gic_enable_irq(spi);
    uint32_t en = mmio_read(env.gicd_base + GICD_ISENABLER + (spi / 32) * 4);
    int ok = (en & (1u << (spi % 32))) != 0;

    gic_disable_irq(spi);
    uint32_t dis = mmio_read(env.gicd_base + GICD_ISENABLER + (spi / 32) * 4);
    ok = ok && (dis & (1u << (spi % 32))) == 0;

    SMOKE_REPORT("gic", ok);
}
