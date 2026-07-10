#ifndef _FDT_H
#define _FDT_H

#include "types.h"
#include "memmap.h"

/* True if the blob at dtb begins with a valid FDT magic. */
bool fdt_valid(const void *dtb);

/* Total size in bytes of the flattened device tree blob. */
uint32_t fdt_totalsize(const void *dtb);

/* Discover RAM from /memory nodes */
int fdt_get_memory(const void *dtb, memmap_t *out);

#endif /* _FDT_H */
