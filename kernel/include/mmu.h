#ifndef _MMU_H
#define _MMU_H

#include "types.h"

/* AArch64 memory management unit setup.
 *
 * The MMU is brought up in stages; this header currently covers the memory
 * attribute table (MAIR_EL1). Translation table setup (TCR_EL1, TTBRn_EL1,
 * page tables) builds on the attribute indices defined here. */

/* Memory attribute indices into MAIR_EL1.
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

/* Program MAIR_EL1 with the attribute table defined above. Must run before any
 * translation regime that references these attribute indices is enabled. */
void mmu_init_mair(void);

#endif /* _MMU_H */
