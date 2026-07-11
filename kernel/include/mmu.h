#ifndef _MMU_H
#define _MMU_H

#include "types.h"
#include "memmap.h"
#include "pmm.h"

/* AArch64 memory management unit setup.
 *
 * The MMU is brought up in stages: the memory attribute table (MAIR_EL1), then
 * the translation tables built here, then TCR_EL1/TTBRn_EL1 and finally the
 * SCTLR_EL1.M enable bit. This header covers the first two. */

/* ---- MAIR_EL1: memory attribute table -------------------------------------
 *
 * AArch64 indirects the memory *type* through MAIR_EL1: the register holds
 * eight 8-bit attribute fields (Attr0..Attr7). A translation-table block/page
 * descriptor does not encode the type directly — it carries a 3-bit AttrIndx
 * that selects one of these eight fields. We use two of the eight slots. */
#define MAIR_IDX_NORMAL   0   /* Normal, cacheable RAM */
#define MAIR_IDX_DEVICE   1   /* Device MMIO, strongly ordered */

/* Attribute byte encodings stored in the MAIR_EL1 fields above.
 *
 * MAIR_ATTR_NORMAL = 0xFF: Normal memory, Outer + Inner Write-Back
 *   Non-transient, Read-Allocate + Write-Allocate. Ordinary cacheable RAM —
 *   write-back, write-allocate ("WB/WA"). Encoding: outer attrs in bits[7:4],
 *   inner attrs in bits[3:0]; 0b1111 in each nibble = WB non-transient RW-alloc.
 *
 * MAIR_ATTR_DEVICE = 0x00: Device-nGnRnE — non-Gathering, non-Reordering,
 *   no-Early-write-acknowledgement. The strictest device type; every access
 *   reaches the peripheral in program order with no merging. Correct for MMIO
 *   registers, where reads/writes have side effects. */
#define MAIR_ATTR_NORMAL  0xFFUL
#define MAIR_ATTR_DEVICE  0x00UL

/* Value written to MAIR_EL1: attribute i occupies byte i of the register. */
#define MAIR_EL1_VALUE \
    ((MAIR_ATTR_NORMAL << (MAIR_IDX_NORMAL * 8)) | \
     (MAIR_ATTR_DEVICE << (MAIR_IDX_DEVICE * 8)))

/* ---- Translation table descriptor format ----------------------------------
 *
 * 4 KiB granule, 48-bit VA: four levels (L0..L3), each table one 4 KiB page of
 * 512 eight-byte descriptors. An L0 entry spans 512 GiB, L1 1 GiB, L2 2 MiB,
 * L3 4 KiB. We map with 2 MiB blocks at L2 where alignment allows and fall back
 * to 4 KiB pages at L3 otherwise. */
#define PT_ENTRIES        512
#define BLOCK_2M_SIZE     (2UL * 1024 * 1024)

/* Descriptor type, bits[1:0]. Bit 0 is the valid bit; bit 1 selects table vs
 * block at L0-L2, and must be set for a valid page at L3. */
#define PTE_VALID         (1UL << 0)
#define PTE_TABLE         (3UL)   /* 0b11: valid, points to next-level table  */
#define PTE_BLOCK         (1UL)   /* 0b01: valid L1/L2 block (leaf)           */
#define PTE_PAGE          (3UL)   /* 0b11: valid L3 page (leaf)               */

/* Output-address field of block/page/table descriptors: bits[47:12]. */
#define PTE_ADDR_MASK     0x0000FFFFFFFFF000UL

/* Lower attributes of block/page descriptors, bits[11:2]. */
#define PTE_ATTRINDX(i)   ((uint64_t)(i) << 2)   /* MAIR index, bits[4:2]     */
#define PTE_AP_RW_EL1     (0UL << 6)   /* EL1 read/write, no EL0 access       */
#define PTE_SH_NONE       (0UL << 8)   /* non-shareable (fine for Device)     */
#define PTE_SH_INNER      (3UL << 8)   /* inner shareable (Normal cacheable)  */
#define PTE_AF            (1UL << 10)  /* access flag; access faults if clear */

/* Upper attributes, bits[54:53]: execute-never at EL1 (PXN) and EL0 (UXN). */
#define PTE_PXN           (1UL << 53)
#define PTE_UXN           (1UL << 54)

/* Attribute presets for the two mappings we build.
 *
 * PTE_NORMAL: cacheable RAM, inner-shareable, kernel RW, executable (the kernel
 *   runs from RAM, so no execute-never here).
 * PTE_DEVICE: MMIO, kernel RW, execute-never at both ELs so nothing is ever
 *   speculatively fetched from a peripheral. Shareability is ignored for
 *   Device memory, so it is left non-shareable. */
#define PTE_NORMAL \
    (PTE_ATTRINDX(MAIR_IDX_NORMAL) | PTE_AP_RW_EL1 | PTE_SH_INNER | PTE_AF)
#define PTE_DEVICE \
    (PTE_ATTRINDX(MAIR_IDX_DEVICE) | PTE_AP_RW_EL1 | PTE_SH_NONE | PTE_AF | \
     PTE_PXN | PTE_UXN)

/* ---- TCR_EL1: translation control -----------------------------------------
 *
 * We use the TTBR0_EL1 (low-half) regime for the identity map and disable
 * TTBR1_EL1 walks entirely. 48-bit VA => TxSZ = 64 - 48 = 16. Table walks are
 * themselves Normal, Inner+Outer Write-Back Write-Allocate, Inner Shareable, so
 * the walker's own accesses are cacheable and coherent. */
#define MMU_T0SZ          16UL          /* 64 - 48-bit VA */

#define TCR_T0SZ(x)       ((uint64_t)(x) << 0)
#define TCR_IRGN0_WBWA    (1UL << 8)    /* TTBR0 walk inner cacheability: WBWA */
#define TCR_ORGN0_WBWA    (1UL << 10)   /* TTBR0 walk outer cacheability: WBWA */
#define TCR_SH0_INNER     (3UL << 12)   /* TTBR0 walk shareability: inner      */
#define TCR_TG0_4K        (0UL << 14)   /* TTBR0 granule: 4 KiB                */
#define TCR_T1SZ(x)       ((uint64_t)(x) << 16)
#define TCR_EPD1          (1UL << 23)   /* disable TTBR1 table walks           */
#define TCR_TG1_4K        (2UL << 30)   /* TTBR1 granule: 4 KiB (TG1 encoding  */
                                        /*   differs from TG0!)                */
#define TCR_IPS_SHIFT     32            /* intermediate PA size, bits[34:32]   */

/* ---- SCTLR_EL1: system control (the bits we flip) ------------------------- */
#define SCTLR_M           (1UL << 0)    /* MMU enable                          */
#define SCTLR_C           (1UL << 2)    /* data & unified cache enable         */
#define SCTLR_I           (1UL << 12)   /* instruction cache enable            */

/* Program MAIR_EL1 with the attribute table defined above. Must run before any
 * translation regime that references these attribute indices is enabled. */
void mmu_init_mair(void);

/* Turn on the MMU using `root` (an identity-mapped L0 table from
 * mmu_build_page_tables) as the TTBR0_EL1 base: configure TCR_EL1, load
 * TTBR0_EL1, flush stale TLB/instruction-cache state, then set SCTLR_EL1.M
 * (and the cache-enable bits). Returns with translation live. `root` must map
 * the currently-executing code, the stack, and any MMIO touched afterwards, or
 * the CPU faults the moment translation switches on. */
void mmu_enable(const uint64_t *root);

/* Build the kernel's initial identity translation tables from `map`: every RAM
 * region (usable and reserved alike) is mapped as Normal cacheable memory, and
 * the platform's PL011 UART MMIO page is mapped as Device-nGnRnE. Tables are
 * allocated one 4 KiB frame at a time from `pmm`. Returns the L0 table — which
 * is itself identity-mapped, so its VA equals the PA to load into TTBR0_EL1
 * later — or NULL if the pmm ran out of frames. Does NOT enable translation;
 * that is a separate step. */
uint64_t *mmu_build_page_tables(const memmap_t *map, pmm_t *pmm);

/* Walk `root` for virtual address `va` and return the leaf block/page
 * descriptor, or 0 if `va` is unmapped. For verifying the tables before the
 * MMU is switched on. */
uint64_t mmu_lookup(const uint64_t *root, uint64_t va);

#endif /* _MMU_H */
