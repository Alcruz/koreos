#include "../include/core/serial/serial.h"
#include "../include/kprint.h"
#include "../include/memmap.h"
#include "../include/core/fdt.h"
#include "../include/pmm.h"
#include "../include/kmalloc.h"
#include "../include/mmu.h"
#include "../include/mmio.h"
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

/* Turn whatever the firmware told us into a physical RAM layout: size RAM from
 * the DTB's /memory nodes, then carve out the regions that are already spoken
 * for (the kernel image and the device tree itself). The fdt layer just walks
 * the tree and hands back the /memory register windows; deciding they are
 * usable RAM is our call. */
static void memory_init(const void *dtb, memmap_t *map)
{
    /* More /memory windows than any board we target describes. */
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

/* "compatible" strings QEMU virt uses for its GICv2 node, most-specific first.
 * This board knowledge lives in main.c; fdt.c only walks the tree. */
static const char *const GICV2_COMPATIBLE[] = {
    "arm,cortex-a15-gic", /* what -cpu cortex-a57 on virt presents */
    "arm,gic-400",
};

/* Resolve the GICv2 register banks from the DTB's interrupt-controller node
 * into two MMIO regions: `banks[0]` is the distributor (GICD, reg[0]) and
 * `banks[1]` the per-CPU interface (GICC, reg[1]), each a physical range the
 * caller maps as Device memory. The addresses are a board property, never an
 * architectural constant, so the DTB is the sole authority — there is no
 * hardcoded fallback, exactly as RAM is sized from /memory. Returns true on
 * success, or false if the blob has no usable node, which the caller treats as
 * fatal since there is nothing to fall back to. Out param last per the codebase
 * convention. */
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

/* Resolve the PL011 UART's register window from the DTB into `out`. This is the
 * sole authority for the console base: serial_init is handed out->base, and the
 * MMU later maps the same region as Device memory. The address is a board
 * property, never an architectural constant, so a missing PL011 node is fatal —
 * there is nothing to fall back to. Returns true on success, false if the blob
 * has no PL011 node. Out param last per the codebase convention. */
static bool uart_resolve(const void *dtb, mmio_region_t *out)
{
    return fdt_get_reg(dtb, "arm,pl011", 0, &out->base, &out->size) == 0;
}

/* Stand up the frame allocator over `map`. pmm_init reserves its own metadata
 * inside `map`, so the map is updated in place. */
static void pmm_bootstrap(memmap_t *map, pmm_t *pmm)
{
    if (!pmm_init(map, pmm))
        panic("memory: not enough usable RAM to start the allocator");
}

/* Dump the physical memory map and frame-allocator totals. */
static void memory_log(const memmap_t *map, const pmm_t *pmm)
{
    kprint_puts("memory: physical map:\n");
    for (size_t i = 0; i < map->count; i++)
    {
        kprint_puts("  ");
        kprint_hex(map->range[i].base);
        kprint_puts(" - ");
        kprint_hex(map->range[i].base + map->range[i].size);
        kprint_puts(map->range[i].kind == MEM_USABLE ? "  usable   (" : "  reserved (");
        kprint_dec(map->range[i].size >> 10);
        kprint_puts(" KiB)\n");
    }

    kprint_puts("pmm: ");
    kprint_dec(pmm_free_pages(pmm));
    kprint_puts(" free / ");
    kprint_dec(pmm_total_pages(pmm));
    kprint_puts(" total frames (");
    kprint_dec(pmm_free_pages(pmm) * (PAGE_SIZE >> 10));
    kprint_puts(" KiB free)\n");
}

/* Walk the freshly built tables for `va` and report the leaf descriptor and its
 * memory type. Lets us confirm the mapping before the MMU is ever switched on. */
static void mmu_check(const uint64_t *root, uint64_t va, const char *what)
{
    uint64_t desc = mmu_lookup(root, va);
    kprint_puts("  ");
    kprint_puts(what);
    kprint_puts(" ");
    kprint_hex(va);
    if (!(desc & PTE_VALID))
    {
        kprint_puts(" -> UNMAPPED\n");
        return;
    }
    uint64_t idx = (desc >> 2) & 7;
    kprint_puts(" -> desc ");
    kprint_hex(desc);
    kprint_puts(idx == MAIR_IDX_NORMAL   ? "  Normal\n"
                : idx == MAIR_IDX_DEVICE ? "  Device\n"
                                         : "  ??\n");
}

/* Smoke check that the heap is alive after bring-up: one alloc round-trips and
 * kzalloc actually zeroes. Exhaustive corruption/leak/stress coverage lives in
 * the host suite (tests/test_kmalloc.c); this only proves the on-target heap
 * came up. Returns nonzero on pass. */
static int heap_smoke(heap_t *h)
{
    void *p = kmalloc(h, 64);
    if (!p || ((uintptr_t)p % HEAP_ALIGN) != 0)
        return 0;
    kfree(h, p);

    uint8_t *z = kzalloc(h, 64);
    if (!z)
        return 0;
    for (size_t j = 0; j < 64; j++)
        if (z[j] != 0)
            return 0;
    kfree(h, z);

    return 1;
}

/* x0 on entry (the DTB pointer) is passed straight through from _start. */
void kernel_main(void *dtb)
{
    /* Resolve the console's register window from the DTB before anything can
     * print: the PL011 base is a board property, not a compile-time constant.
     * A missing node is fatal, though the panic itself has no console to speak
     * to — there is nothing to fall back to. */
    mmio_region_t uart;
    if (!uart_resolve(dtb, &uart))
        panic("uart: no PL011 node in DTB");

    serial_init(uart.base);
    kprint_puts("Koreos!\n");

    memmap_t map;
    memory_init(dtb, &map);
    pmm_bootstrap(&map, &pmm);
    memory_log(&map, &pmm);

    mmu_init_mair();
    uint64_t mair;
    __asm__ volatile("mrs %0, mair_el1" : "=r"(mair));
    kprint_puts("mmu: MAIR_EL1 = ");
    kprint_hex(mair);
    kprint_puts("\n");

    /* Resolve the GICv2 register banks from the DTB, then hand the MMU the
     * platform MMIO it must map as Device memory. main.c owns this board
     * knowledge; mmu.c just maps whatever ranges it is given. As with RAM
     * sizing, a missing node is fatal rather than guessed. The UART region was
     * already resolved above to bring the console up. */
    mmio_region_t gic[2];
    if (!gic_resolve(dtb, gic))
        panic("gic: no GICv2 interrupt-controller node in DTB");
    const mmio_region_t mmio[] = {
        uart,   /* PL011 UART register page   */
        gic[0], /* GICD distributor           */
        gic[1], /* GICC CPU interface         */
    };

    /* Build the identity translation tables (MMU stays off for now). */
    size_t pt_before = pmm_free_pages(&pmm);
    uint64_t *root = mmu_build_page_tables(&map, mmio,
                                           sizeof(mmio) / sizeof(*mmio), &pmm);
    if (!root)
        panic("mmu: out of memory building page tables");
    kprint_puts("mmu: page tables built, TTBR0 root = ");
    kprint_hex((uint64_t)root);
    kprint_puts(", ");
    kprint_dec(pt_before - pmm_free_pages(&pmm));
    kprint_puts(" frames used\n");
    mmu_check(root, (uint64_t)_kernel_start, "kernel");
    mmu_check(root, uart.base, "uart  ");
    mmu_check(root, gic[0].base, "gicd  ");
    mmu_check(root, gic[1].base, "gicc  ");

    /* Switch translation on. If any of the identity map, the UART mapping, or
     * TCR is wrong, the CPU faults here and the next line never prints. That it
     * does print is the proof: code fetch, stack/globals, and MMIO all
     * translate correctly. */
    mmu_enable(root);
    uint64_t sctlr;
    __asm__ volatile("mrs %0, sctlr_el1" : "=r"(sctlr));
    kprint_puts("mmu: enabled, SCTLR_EL1 = ");
    kprint_hex(sctlr);
    kprint_puts(" (translation live)\n");

    size_t before = pmm_free_pages(&pmm);
    pmm_page_t *a = pmm_alloc_page(&pmm);
    pmm_page_t *b = pmm_alloc_page(&pmm);
    int ok = a && b && a != b && pmm_free_pages(&pmm) == before - 2;
    pmm_free_page(&pmm, a);
    pmm_free_page(&pmm, b);
    ok = ok && pmm_free_pages(&pmm) == before;
    kprint_puts(ok ? "pmm: alloc/free invariants OK\n"
                   : "pmm: alloc/free invariants FAILED\n");

    /* Bring up the kernel heap over the frame allocator and smoke-check it
     * before anything relies on it. */
    heap_init(&pmm, &heap);
    kprint_puts(heap_smoke(&heap)
                    ? "heap: smoke check OK (alloc/free/zero)\n"
                    : "heap: smoke check FAILED\n");

    /* Idle loop */
    while (1)
    {
        __asm__ volatile("wfe");
    }
}
