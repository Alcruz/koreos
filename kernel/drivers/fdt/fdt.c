/* Minimal read-only parser for the Flattened Device Tree (FDT / DTB) that
 * QEMU hands the kernel in x0. It knows only how to walk the tree and locate
 * entries — validity, total size, a node's `reg` by compatible string, and the
 * register windows of all nodes matching a name. It has no knowledge of what
 * any given node means:
 * turning /memory reg pairs into a RAM map, or an interrupt-controller node
 * into GIC banks, is the caller's job (see core/main.c, drivers/gic).
 *
 * Reference: the Devicetree Specification, "Flattened Devicetree (DTB)
 * Format". All multi-byte values in the blob are big-endian.
 */

#include "../../include/core/fdt.h"
#include "../../include/string.h"

#define FDT_MAGIC       0xd00dfeedU

/* Structure-block tokens. */
#define FDT_BEGIN_NODE  0x1U
#define FDT_END_NODE    0x2U
#define FDT_PROP        0x3U
#define FDT_NOP         0x4U
#define FDT_END         0x9U

/* Default cell counts when a parent node does not override them. */
#define DEFAULT_ADDRESS_CELLS 2
#define DEFAULT_SIZE_CELLS    1

struct fdt_header {
    uint32_t magic;
    uint32_t totalsize;
    uint32_t off_dt_struct;
    uint32_t off_dt_strings;
    uint32_t off_mem_rsvmap;
    uint32_t version;
    uint32_t last_comp_version;
    uint32_t boot_cpuid_phys;
    uint32_t size_dt_strings;
    uint32_t size_dt_struct;
};

/* Load a big-endian 32-bit word from the blob into native byte order. */
static uint32_t fdt32(const uint32_t *p)
{
    return __builtin_bswap32(*p);
}

/* Combine `n` big-endian cells (32 bits each) starting at `cells` into a
 * single 64-bit value. */
static uint64_t read_cells(const uint32_t *cells, int n)
{
    uint64_t v = 0;
    for (int i = 0; i < n; i++)
        v = (v << 32) | fdt32(&cells[i]);
    return v;
}

bool fdt_valid(const void *dtb)
{
    if (!dtb)
        return false;
    const struct fdt_header *h = dtb;
    return fdt32(&h->magic) == FDT_MAGIC;
}

uint32_t fdt_totalsize(const void *dtb)
{
    const struct fdt_header *h = dtb;
    return fdt32(&h->totalsize);
}

int fdt_get_all_devices(const void *dtb, const char *name_prefix,
                        int max, fdt_device_t *out)
{
    if (!fdt_valid(dtb) || max < 0)
        return -1;

    const struct fdt_header *h = dtb;
    const uint32_t *p =
        (const uint32_t *)((const uint8_t *)dtb + fdt32(&h->off_dt_struct));
    const char *strings = (const char *)dtb + fdt32(&h->off_dt_strings);

    int depth = 0;
    int match = 0;              /* current node's name begins with name_prefix */
    int addr_cells = DEFAULT_ADDRESS_CELLS;
    int size_cells = DEFAULT_SIZE_CELLS;
    int count = 0;

    for (;;) {
        uint32_t token = fdt32(p++);

        if (token == FDT_BEGIN_NODE) {
            const char *name = (const char *)p;
            p += (str_len(name) + 1 + 3) / 4;   /* skip name, padded to 4 */
            depth++;
            /* Only match direct children of the root; their reg is governed by
             * the root's cell counts, tracked below. */
            match = (depth == 2) && str_prefix(name, name_prefix);
        } else if (token == FDT_END_NODE) {
            depth--;
            match = 0;
        } else if (token == FDT_PROP) {
            uint32_t len = fdt32(p++);
            uint32_t nameoff = fdt32(p++);
            const char *pname = strings + nameoff;
            const uint32_t *val = p;
            p += (len + 3) / 4;                 /* skip value, padded to 4 */

            /* The root node's cell counts govern its children's reg. */
            if (depth == 1) {
                if (str_eq(pname, "#address-cells"))
                    addr_cells = (int)fdt32(val);
                else if (str_eq(pname, "#size-cells"))
                    size_cells = (int)fdt32(val);
            } else if (match && str_eq(pname, "reg")) {
                int per = addr_cells + size_cells;
                uint32_t ncells = len / 4;
                const uint32_t *c = val;
                while (per > 0 && ncells >= (uint32_t)per && count < max) {
                    out[count].base = read_cells(c, addr_cells);
                    out[count].size = read_cells(c + addr_cells, size_cells);
                    count++;
                    c += per;
                    ncells -= (uint32_t)per;
                }
            }
        } else if (token == FDT_NOP) {
            /* nothing */
        } else if (token == FDT_END) {
            break;
        } else {
            break;                              /* malformed blob */
        }
    }

    return count;
}

/* True if the NUL-separated stringlist [list, list + len) contains `s`. A
 * device node's "compatible" property is exactly such a list, most-specific
 * entry first (e.g. "arm,cortex-a15-gic\0arm,cortex-a9-gic\0"). */
static bool stringlist_has(const char *list, uint32_t len, const char *s)
{
    uint32_t i = 0;
    while (i < len) {
        const char *entry = list + i;
        if (str_eq(entry, s))
            return true;
        i += (uint32_t)str_len(entry) + 1;      /* past this entry's NUL */
    }
    return false;
}

int fdt_get_reg(const void *dtb, const char *compatible, int index,
                uint64_t *base, uint64_t *size)
{
    if (!fdt_valid(dtb) || index < 0)
        return -1;

    const struct fdt_header *h = dtb;
    const uint32_t *p =
        (const uint32_t *)((const uint8_t *)dtb + fdt32(&h->off_dt_struct));
    const char *strings = (const char *)dtb + fdt32(&h->off_dt_strings);

    int depth = 0;
    int addr_cells = DEFAULT_ADDRESS_CELLS;
    int size_cells = DEFAULT_SIZE_CELLS;

    /* Per-node state for the child of root we are currently inside. `compatible`
     * and `reg` can appear in either order, so we remember reg's location and
     * whether compatible matched, then resolve when the node closes. */
    bool match = false;
    const uint32_t *reg = NULL;
    uint32_t reg_cells = 0;

    for (;;) {
        uint32_t token = fdt32(p++);

        if (token == FDT_BEGIN_NODE) {
            const char *name = (const char *)p;
            p += (str_len(name) + 1 + 3) / 4;   /* skip name, padded to 4 */
            depth++;
            if (depth == 2) {                   /* entering a child of root */
                match = false;
                reg = NULL;
                reg_cells = 0;
            }
        } else if (token == FDT_END_NODE) {
            if (depth == 2 && match && reg) {
                /* reg holds <addr,size> pairs of (addr_cells + size_cells)
                 * cells each; pick the requested pair if it is present. */
                uint32_t per = (uint32_t)(addr_cells + size_cells);
                uint32_t off = per * (uint32_t)index;
                if (per > 0 && reg_cells >= off + per) {
                    const uint32_t *c = reg + off;
                    if (base)
                        *base = read_cells(c, addr_cells);
                    if (size)
                        *size = read_cells(c + addr_cells, size_cells);
                    return 0;
                }
                return -1;              /* matched, but no reg pair at index */
            }
            depth--;
        } else if (token == FDT_PROP) {
            uint32_t len = fdt32(p++);
            uint32_t nameoff = fdt32(p++);
            const char *pname = strings + nameoff;
            const uint32_t *val = p;
            p += (len + 3) / 4;                 /* skip value, padded to 4 */

            if (depth == 1) {
                if (str_eq(pname, "#address-cells"))
                    addr_cells = (int)fdt32(val);
                else if (str_eq(pname, "#size-cells"))
                    size_cells = (int)fdt32(val);
            } else if (depth == 2) {
                if (str_eq(pname, "compatible"))
                    match = stringlist_has((const char *)val, len, compatible);
                else if (str_eq(pname, "reg")) {
                    reg = val;
                    reg_cells = len / 4;
                }
            }
        } else if (token == FDT_NOP) {
            /* nothing */
        } else if (token == FDT_END) {
            break;
        } else {
            break;                              /* malformed blob */
        }
    }

    return -1;                                  /* no matching node */
}
