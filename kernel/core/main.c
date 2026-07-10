#include "../include/serial.h"
#include "../include/kprint.h"
#include "../include/memmap.h"
#include "../include/fdt.h"
#include "../include/pmm.h"
#include "../include/mmu.h"
#include "../include/panic.h"

/* Provided by the linker script (arch/arm64/boot/linker.ld). */
extern char _kernel_start[];
extern char _kernel_end[];

/* The kernel's physical frame allocator. Lives in the data segment so
 * it outlives kernel_main's stack frame and can be shared with other
 * subsystems. */
static pmm_t pmm;

/* Turn whatever the firmware told us into a usable RAM layout and hand it to
 * the physical memory manager. */
static void memory_init(const void *dtb, pmm_t *pmm)
{
    memmap_t map;
    if (fdt_get_memory(dtb, &map) <= 0)
        panic("memory: no /memory node in DTB; cannot size RAM");

    if (!memmap_reserve(&map, (uintptr_t)_kernel_start,
                        (uintptr_t)_kernel_end - (uintptr_t)_kernel_start))
        panic("memory: map full reserving kernel image");

    if (fdt_valid(dtb))
        if (!memmap_reserve(&map, (uintptr_t)dtb, fdt_totalsize(dtb)))
            panic("memory: map full reserving device tree");

    if (!pmm_init(&map, pmm))
        panic("memory: not enough usable RAM to start the allocator");

    kprint_puts("memory: physical map:\n");
    for (size_t i = 0; i < map.count; i++)
    {
        kprint_puts("  ");
        kprint_hex(map.range[i].base);
        kprint_puts(" - ");
        kprint_hex(map.range[i].base + map.range[i].size);
        kprint_puts(map.range[i].kind == MEM_USABLE ? "  usable   (" : "  reserved (");
        kprint_dec(map.range[i].size >> 10);
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

/* x0 on entry (the DTB pointer) is passed straight through from _start. */
void kernel_main(void *dtb)
{
    serial_init();
    kprint_puts("Koreos!\n");

    memory_init(dtb, &pmm);

    mmu_init_mair();
    uint64_t mair;
    __asm__ volatile("mrs %0, mair_el1" : "=r"(mair));
    kprint_puts("mmu: MAIR_EL1 = ");
    kprint_hex(mair);
    kprint_puts("\n");

    size_t before = pmm_free_pages(&pmm);
    page_t *a = pmm_alloc_page(&pmm);
    page_t *b = pmm_alloc_page(&pmm);
    int ok = a && b && a != b && pmm_free_pages(&pmm) == before - 2;
    pmm_free_page(&pmm, a);
    pmm_free_page(&pmm, b);
    ok = ok && pmm_free_pages(&pmm) == before;
    kprint_puts(ok ? "pmm: alloc/free invariants OK\n"
                   : "pmm: alloc/free invariants FAILED\n");

    /* Idle loop */
    while (1)
    {
        __asm__ volatile("wfe");
    }
}
