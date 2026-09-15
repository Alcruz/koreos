#include "smoke.h"

#include "../../kernel/include/core/serial/serial.h"
#include "../../kernel/include/kprint.h"
#include "../../kernel/include/memmap.h"
#include "../../kernel/include/core/fdt.h"
#include "../../kernel/include/pmm.h"
#include "../../kernel/include/kmalloc.h"
#include "../../kernel/include/mmu.h"
#include "../../kernel/include/mmio.h"
#include "../../kernel/include/core/irqchip/gicv2.h"
#include "../../kernel/include/panic.h"

/* Provided by the linker script. */
extern char _kernel_start[];
extern char _kernel_end[];

/* State backing smoke_env_t.{pmm,heap}; kept in the data segment so it
 * outlives the caller's stack frame. */
static pmm_t s_pmm;
static heap_t s_heap;

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

static void memory_init(const void *dtb, memmap_t *map)
{
    fdt_device_t ram[8];
    int n = fdt_get_all_devices(dtb, "memory", 8, ram);
    if (n <= 0)
        panic("memory: no /memory node in DTB");
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

void smoke_boot(void *dtb, smoke_env_t *env)
{
    mmio_region_t uart;
    if (!uart_resolve(dtb, &uart))
        panic("uart: no PL011 node in DTB");
    serial_init(uart.base);

    memmap_t map;
    memory_init(dtb, &map);
    if (!pmm_init(&map, &s_pmm))
        panic("memory: not enough usable RAM to start the allocator");

    mmu_init_mair();

    mmio_region_t gic[2];
    if (!gic_resolve(dtb, gic))
        panic("gic: no GICv2 interrupt-controller node in DTB");
    const mmio_region_t mmio[] = {uart, gic[0], gic[1]};
    uint64_t *root = mmu_build_page_tables(&map, mmio,
                                           sizeof(mmio) / sizeof(*mmio), &s_pmm);
    if (!root)
        panic("mmu: out of memory building page tables");
    mmu_enable(root);

    gic_init(gic[0].base, gic[1].base);
    heap_init(&s_pmm, &s_heap);

    env->pmm = &s_pmm;
    env->heap = &s_heap;
    env->gicd_base = gic[0].base;
}

/* PSCI SYSTEM_OFF (function id 0x84000008). QEMU virt honours this and exits
 * cleanly with status 0. HVC conduit is the virt default when no `-cpu ... -smp
 * ... -machine virt,secure=off` overrides are given. */
void smoke_exit(void)
{
    register uint64_t x0 __asm__("x0") = 0x84000008;
    __asm__ volatile("hvc #0" : "+r"(x0) : : "memory");
    /* Should not return; if it does, park the core. */
    for (;;)
        __asm__ volatile("wfe");
}
