#ifndef _FDT_H
#define _FDT_H

#include <stdint.h>
#include <stdbool.h>

/* True if the blob at dtb begins with a valid FDT magic. */
bool fdt_valid(const void *dtb);

/* Total size in bytes of the flattened device tree blob. */
uint32_t fdt_totalsize(const void *dtb);

/* One device's register window: a physical [base, base + size) range decoded
 * from a single <addr,size> pair of a node's `reg`. */
typedef struct fdt_device {
    uint64_t base;
    uint64_t size;
} fdt_device_t;

/* Collect the register windows of every direct child of the root whose node
 * name begins with `name_prefix` (e.g. "memory" matches both "memory" and
 * "memory@40000000"). Each <addr,size> pair in a matching node's `reg` becomes
 * one entry, written in tree order into `out[0..max)`; the root's
 * #address-cells/#size-cells govern the encoding. Purely a tree walk — it
 * attaches no meaning to the windows; the caller does.
 *
 * Returns the number of entries written, capped at `max` (a return of `max` may
 * mean the tree held more and the caller should retry with a larger buffer), or
 * -1 if the blob is invalid. Out param last per the codebase convention. */
int fdt_get_all_devices(const void *dtb, const char *name_prefix,
                        int max, fdt_device_t *out);

/* Read one (base, size) pair from the `reg` of the first node whose
 * "compatible" stringlist contains `compatible`. `index` selects which pair
 * (0 = first). The node is assumed to be a direct child of the root, so the
 * root's #address-cells/#size-cells govern the reg encoding — true for the
 * top-level devices QEMU virt describes (interrupt controller, UART, ...).
 *
 * Returns 0 on success (filling the base and size out params), or -1 if the blob is invalid,
 * no matching node exists, or it has no `reg` pair at `index`. Out params last
 * per the codebase convention. */
int fdt_get_reg(const void *dtb, const char *compatible, int index,
                uint64_t *base, uint64_t *size);

#endif /* _FDT_H */
