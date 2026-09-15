#include "../include/core/serial/serial.h"
#include "../include/kprint.h"
#include "../include/memmap.h"
#include "../include/core/fdt.h"
#include "../include/pmm.h"
#include "../include/kmalloc.h"
#include "../include/mmu.h"
#include "../include/mmio.h"
#include "../include/core/irqchip/gicv2.h"
#include "../include/core/timer/timer.h"
#include "../include/irq.h"
#include "../include/panic.h"

/* Provided by the linker script (arch/arm64/boot/linker.ld). */
extern char _kernel_start[];
extern char _kernel_end[];

/* The kernel's physical frame allocator. Lives in the data segment so
 * it outlives kernel_main's stack frame and can be shared with other
 * subsystems. */
static pmm_t pmm;

/* The kernel heap. Grows out of `pmm` on demand; lives in the data segment for
 * the same reason as `pmm`. */
static heap_t heap;

static void memory_init(const void *dtb, memmap_t *map)
{
    fdt_device_t ram[8];
    int n = fdt_get_all_devices(dtb, "memory", 8, ram);
    if (n <= 0)
        panic("memory: no /memory node in DTB; cannot size RAM");

    memmap_init(map);
    for (int i = 0; i < n; i++)
        memmap_add_ram(map, ram[i].base, ram[i].size);

    if (!memmap_reserve(map, (uintptr_t)_kernel_start,
                        (uintptr_t)_kernel_end - (uintptr_t)_kernel_start))
        panic("memory: map full reserving kernel image");

    if (fdt_valid(dtb))
        if (!memmap_reserve(map, (uintptr_t)dtb, fdt_totalsize(dtb)))
            panic("memory: map full reserving device tree");
}

/* "compatible" strings QEMU virt uses for its GICv2 node, most-specific first. */
static const char *const GICV2_COMPATIBLE[] = {
    "arm,cortex-a15-gic",
    "arm,gic-400",
};

static bool gic_resolve(const void *dtb, mmio_region_t banks[2])
{
    for (unsigned i = 0; i < sizeof(GICV2_COMPATIBLE) / sizeof(*GICV2_COMPATIBLE);
         i++)
    {
        if (fdt_get_reg(dtb, GICV2_COMPATIBLE[i], 0,
                        &banks[0].base, &banks[0].size) == 0 &&
            fdt_get_reg(dtb, GICV2_COMPATIBLE[i], 1,
                        &banks[1].base, &banks[1].size) == 0)
            return true;
    }
    return false;
}

static bool uart_resolve(const void *dtb, mmio_region_t *out)
{
    return fdt_get_reg(dtb, "arm,pl011", 0, &out->base, &out->size) == 0;
}

/* Ticks per second for the periodic timer. */
#define TIMER_HZ 4

static volatile unsigned tick_count;

static void timer_tick(uint32_t irq)
{
    (void)irq;
    timer_reload();
    tick_count++;
    kprint_puts("timer: tick ");
    kprint_dec(tick_count);
    kprint_putc('\n');
}

/* x0 on entry (the DTB pointer) is passed straight through from _start. */
void kernel_main(void *dtb)
{
    mmio_region_t uart;
    if (!uart_resolve(dtb, &uart))
        panic("uart: no PL011 node in DTB");
    serial_init(uart.base);
    kprint_puts("Koreos!\n");

    memmap_t map;
    memory_init(dtb, &map);
    if (!pmm_init(&map, &pmm))
        panic("memory: not enough usable RAM to start the allocator");

    mmu_init_mair();

    mmio_region_t gic[2];
    if (!gic_resolve(dtb, gic))
        panic("gic: no GICv2 interrupt-controller node in DTB");
    const mmio_region_t mmio[] = {uart, gic[0], gic[1]};

    uint64_t *root = mmu_build_page_tables(&map, mmio,
                                           sizeof(mmio) / sizeof(*mmio), &pmm);
    if (!root)
        panic("mmu: out of memory building page tables");
    mmu_enable(root);

    gic_init(gic[0].base, gic[1].base);
    heap_init(&pmm, &heap);

    irq_register(TIMER_IRQ, timer_tick);
    gic_enable_irq(TIMER_IRQ);
    timer_init(TIMER_HZ);
    __asm__ volatile("msr daifclr, #2"); /* clear PSTATE.I: unmask IRQs */

    while (1)
    {
        __asm__ volatile("wfe");
    }
}
