#include "../include/serial.h"
#include "../include/kprint.h"
#include "../include/memmap.h"
#include "../include/fdt.h"
#include "../include/pmm.h"
#include "../include/kmalloc.h"
#include "../include/mmu.h"
#include "../include/panic.h"
#include "../drivers/serial/pl011.h"   /* UART_BASE, for the mapping check */

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
 * for (the kernel image and the device tree itself). */
static void memory_init(const void *dtb, memmap_t *map)
{
    if (fdt_get_memory(dtb, map) <= 0)
        panic("memory: no /memory node in DTB; cannot size RAM");

    if (!memmap_reserve(map, (uintptr_t)_kernel_start,
                        (uintptr_t)_kernel_end - (uintptr_t)_kernel_start))
        panic("memory: map full reserving kernel image");

    if (fdt_valid(dtb))
        if (!memmap_reserve(map, (uintptr_t)dtb, fdt_totalsize(dtb)))
            panic("memory: map full reserving device tree");
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
    kprint_puts(idx == MAIR_IDX_NORMAL ? "  Normal\n"
              : idx == MAIR_IDX_DEVICE ? "  Device\n"
                                       : "  ??\n");
}

/* x0 on entry (the DTB pointer) is passed straight through from _start. */
void kernel_main(void *dtb)
{
    serial_init();
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

    /* Build the identity translation tables (MMU stays off for now). */
    size_t pt_before = pmm_free_pages(&pmm);
    uint64_t *root = mmu_build_page_tables(&map, &pmm);
    if (!root)
        panic("mmu: out of memory building page tables");
    kprint_puts("mmu: page tables built, TTBR0 root = ");
    kprint_hex((uint64_t)root);
    kprint_puts(", ");
    kprint_dec(pt_before - pmm_free_pages(&pmm));
    kprint_puts(" frames used\n");
    mmu_check(root, (uint64_t)_kernel_start, "kernel");
    mmu_check(root, UART_BASE, "uart  ");

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

    /* Bring up the kernel heap over the frame allocator and exercise kmalloc:
     * small requests are carved (with splitting) out of a single grown frame,
     * and a request larger than one frame's payload forces a fresh multi-frame
     * grow. */
    heap_init(&pmm, &heap);
    size_t heap_before = pmm_free_pages(&pmm);
    void *h0 = kmalloc(&heap, 32);
    void *h1 = kmalloc(&heap, 100);
    void *h2 = kmalloc(&heap, 16);
    void *hbig = kmalloc(&heap, 5000);   /* larger than one frame's payload */

    uintptr_t any = (uintptr_t)h0 | (uintptr_t)h1 | (uintptr_t)h2 | (uintptr_t)hbig;
    int heap_ok = h0 && h1 && h2 && hbig && (any % HEAP_ALIGN) == 0;
    if (heap_ok) {
        /* Distinct, writable storage: each sentinel must survive the others. */
        *(uint64_t *)h0   = 0x1111;
        *(uint64_t *)h1   = 0x2222;
        *(uint64_t *)h2   = 0x3333;
        *(uint64_t *)hbig = 0x4444;
        heap_ok = *(uint64_t *)h0 == 0x1111 && *(uint64_t *)h1 == 0x2222
               && *(uint64_t *)h2 == 0x3333 && *(uint64_t *)hbig == 0x4444;
    }
    /* h0/h1/h2 share one grown frame; hbig pulls two more -> three total. */
    heap_ok = heap_ok && pmm_free_pages(&pmm) == heap_before - 3;
    kprint_puts(heap_ok ? "heap: kmalloc OK (first-fit + split)\n"
                        : "heap: kmalloc FAILED\n");

    /* Idle loop */
    while (1)
    {
        __asm__ volatile("wfe");
    }
}
