/* Minimal read-only parser for the Flattened Device Tree (FDT / DTB) that
 * QEMU hands the kernel in x0. Only enough is implemented to enumerate the
 * RAM regions described by /memory nodes.
 *
 * Reference: the Devicetree Specification, "Flattened Devicetree (DTB)
 * Format". All multi-byte values in the blob are big-endian.
 */

#include "../include/fdt.h"
#include "../include/string.h"

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

int fdt_get_memory(const void *dtb, memmap_t *out)
{
    memmap_init(out);
    if (!fdt_valid(dtb))
        return -1;

    const struct fdt_header *h = dtb;
    const uint32_t *p =
        (const uint32_t *)((const uint8_t *)dtb + fdt32(&h->off_dt_struct));
    const char *strings = (const char *)dtb + fdt32(&h->off_dt_strings);

    int depth = 0;
    int in_memory = 0;          /* current node is a /memory node */
    int addr_cells = DEFAULT_ADDRESS_CELLS;
    int size_cells = DEFAULT_SIZE_CELLS;
    int count = 0;

    for (;;) {
        uint32_t token = fdt32(p++);

        if (token == FDT_BEGIN_NODE) {
            const char *name = (const char *)p;
            p += (str_len(name) + 1 + 3) / 4;   /* skip name, padded to 4 */
            depth++;
            /* Memory nodes are named "memory" or "memory@<addr>" and live
             * directly under the root. */
            in_memory = (depth == 2) && str_prefix(name, "memory");
        } else if (token == FDT_END_NODE) {
            depth--;
            in_memory = 0;
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
            } else if (in_memory && str_eq(pname, "reg")) {
                int per = addr_cells + size_cells;
                uint32_t ncells = len / 4;
                const uint32_t *c = val;
                while (per > 0 && ncells >= (uint32_t)per) {
                    uint64_t base = read_cells(c, addr_cells);
                    uint64_t size = read_cells(c + addr_cells, size_cells);
                    if (!memmap_add_ram(out, base, size))
                        break;              /* map at capacity */
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
