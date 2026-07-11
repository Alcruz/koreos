/* Host-side unit tests for the flattened device tree parser (kernel/lib/fdt.c).
 *
 * fdt.c reads the DTB QEMU hands the kernel in x0 and pulls the RAM regions out
 * of its /memory nodes — a boot-critical path (a wrong parse means the kernel
 * mismeasures RAM and panics or corrupts memory). To exercise it on the host we
 * assemble minimal but real device trees byte by byte with the builder below:
 * a big-endian header, an empty reservation block, a structure block of tokens,
 * and a strings block. The parser only ever reads back through the public
 * fdt_valid / fdt_totalsize / fdt_get_memory entry points.
 *
 * The assembled blob is 8-byte aligned so the parser's 32-bit big-endian reads
 * are well-defined under UBSan.
 */

#include <stdint.h>
#include <stddef.h>

#include "unity.h"
#include "fdt.h"
#include "memmap.h"

/* Structure-block tokens (mirrors the private constants in fdt.c). */
#define FDT_BEGIN_NODE 0x1U
#define FDT_END_NODE 0x2U
#define FDT_PROP 0x3U
#define FDT_END 0x9U
#define FDT_MAGIC 0xd00dfeedU

#define HEADER_BYTES 40 /* ten u32 header fields                 */
#define RSVMAP_BYTES 16 /* one all-zero terminating reserve entry */

/* Byte buffer for one section, filled big-endian. */
typedef struct
{
    uint8_t buf[4096];
    size_t len;
} blob_t;

static blob_t st;              /* structure block               */
static blob_t strtab;          /* strings block                 */
static _Alignas(8) blob_t dtb; /* assembled, alignment-critical */

static void put_be32(blob_t *b, uint32_t v)
{
    b->buf[b->len++] = (uint8_t)(v >> 24);
    b->buf[b->len++] = (uint8_t)(v >> 16);
    b->buf[b->len++] = (uint8_t)(v >> 8);
    b->buf[b->len++] = (uint8_t)v;
}

/* Append a NUL-terminated string to the strings block, returning its offset. */
static uint32_t str_off(const char *s)
{
    uint32_t off = (uint32_t)strtab.len;
    do
    {
        strtab.buf[strtab.len++] = (uint8_t)*s;
    } while (*s++);
    return off;
}

/* Pad the structure block up to a 4-byte boundary with zeros. */
static void st_pad4(void)
{
    while (st.len % 4)
        st.buf[st.len++] = 0;
}

static void begin_node(const char *name)
{
    put_be32(&st, FDT_BEGIN_NODE);
    do
    {
        st.buf[st.len++] = (uint8_t)*name;
    } while (*name++);
    st_pad4();
}

static void end_node(void)
{
    put_be32(&st, FDT_END_NODE);
}

/* Emit a property whose value is `len` raw bytes (padded to 4 in the stream,
 * but the recorded length is the unpadded value length, as the spec requires). */
static void prop_bytes(const char *name, const void *data, uint32_t len)
{
    put_be32(&st, FDT_PROP);
    put_be32(&st, len);
    put_be32(&st, str_off(name));
    const uint8_t *d = data;
    for (uint32_t i = 0; i < len; i++)
        st.buf[st.len++] = d[i];
    st_pad4();
}

/* Emit a property whose value is `n` big-endian 32-bit cells. */
static void prop_cells(const char *name, const uint32_t *cells, int n)
{
    put_be32(&st, FDT_PROP);
    put_be32(&st, (uint32_t)(n * 4));
    put_be32(&st, str_off(name));
    for (int i = 0; i < n; i++)
        put_be32(&st, cells[i]);
}

static void prop_u32(const char *name, uint32_t v)
{
    prop_cells(name, &v, 1);
}

/* Start a fresh tree. */
static void dtb_reset(void)
{
    st.len = 0;
    strtab.len = 0;
    dtb.len = 0;
}

/* Stitch header + reservation block + structure + strings into `dtb` and hand
 * back the (aligned) blob pointer for the parser. */
static const void *dtb_finalize(void)
{
    uint32_t off_struct = HEADER_BYTES + RSVMAP_BYTES;
    uint32_t off_strings = off_struct + (uint32_t)st.len;
    uint32_t total = off_strings + (uint32_t)strtab.len;

    put_be32(&dtb, FDT_MAGIC);
    put_be32(&dtb, total);
    put_be32(&dtb, off_struct);
    put_be32(&dtb, off_strings);
    put_be32(&dtb, HEADER_BYTES);         /* off_mem_rsvmap        */
    put_be32(&dtb, 17);                   /* version               */
    put_be32(&dtb, 16);                   /* last_comp_version     */
    put_be32(&dtb, 0);                    /* boot_cpuid_phys       */
    put_be32(&dtb, (uint32_t)strtab.len); /* size_dt_strings       */
    put_be32(&dtb, (uint32_t)st.len);     /* size_dt_struct        */

    for (int i = 0; i < RSVMAP_BYTES; i++) /* empty reservation block */
        dtb.buf[dtb.len++] = 0;
    for (size_t i = 0; i < st.len; i++)
        dtb.buf[dtb.len++] = st.buf[i];
    for (size_t i = 0; i < strtab.len; i++)
        dtb.buf[dtb.len++] = strtab.buf[i];

    return dtb.buf;
}

/* Open a root node carrying the given cell counts; caller adds memory children
 * then calls end_node() + dtb_finalize(). */
static void begin_root(uint32_t addr_cells, uint32_t size_cells)
{
    dtb_reset();
    begin_node(""); /* root has an empty name */
    prop_u32("#address-cells", addr_cells);
    prop_u32("#size-cells", size_cells);
}

void setUp(void) {}
void tearDown(void) {}

/* --- validity & size --------------------------------------------------- */

static void test_fdt_valid_accepts_good_magic_rejects_bad(void)
{
    begin_root(2, 1);
    end_node();
    const void *blob = dtb_finalize();

    TEST_ASSERT_TRUE(fdt_valid(blob));
    TEST_ASSERT_FALSE(fdt_valid(NULL));

    dtb.buf[0] ^= 0xFF; /* corrupt the magic */
    TEST_ASSERT_FALSE(fdt_valid(blob));
}

static void test_fdt_totalsize_matches_assembled_length(void)
{
    begin_root(2, 1);
    end_node();
    const void *blob = dtb_finalize();

    TEST_ASSERT_EQUAL_UINT32(dtb.len, fdt_totalsize(blob));
}

/* --- memory discovery -------------------------------------------------- */

static void test_get_memory_reads_single_region(void)
{
    /* addr = 2 cells, size = 1 cell: reg = <0x0 0x40000000  0x20000000>. */
    uint32_t reg[] = {0x00000000, 0x40000000, 0x20000000};
    begin_root(2, 1);
    begin_node("memory@40000000");
    prop_bytes("device_type", "memory", 7);
    prop_cells("reg", reg, 3);
    end_node();
    end_node();
    const void *blob = dtb_finalize();

    memmap_t m;
    TEST_ASSERT_EQUAL_INT(1, fdt_get_memory(blob, &m));
    TEST_ASSERT_EQUAL_UINT64(1, m.count);
    TEST_ASSERT_EQUAL_UINT64(0x40000000, m.range[0].base);
    TEST_ASSERT_EQUAL_UINT64(0x20000000, m.range[0].size);
    TEST_ASSERT_EQUAL_INT(MEM_USABLE, m.range[0].kind);
}

static void test_get_memory_reads_multiple_regions_in_one_reg(void)
{
    /* Two <addr,size> pairs packed into one reg property. */
    uint32_t reg[] = {
        0x00000000,
        0x40000000,
        0x10000000, /* 256 MiB at 1 GiB */
        0x00000000,
        0x80000000,
        0x08000000, /* 128 MiB at 2 GiB */
    };
    begin_root(2, 1);
    begin_node("memory@40000000");
    prop_cells("reg", reg, 6);
    end_node();
    end_node();
    const void *blob = dtb_finalize();

    memmap_t m;
    TEST_ASSERT_EQUAL_INT(2, fdt_get_memory(blob, &m));
    TEST_ASSERT_EQUAL_UINT64(2, m.count);
    TEST_ASSERT_EQUAL_UINT64(0x40000000, m.range[0].base);
    TEST_ASSERT_EQUAL_UINT64(0x10000000, m.range[0].size);
    TEST_ASSERT_EQUAL_UINT64(0x80000000, m.range[1].base);
    TEST_ASSERT_EQUAL_UINT64(0x08000000, m.range[1].size);
}

static void test_get_memory_merges_separate_memory_nodes_sorted(void)
{
    uint32_t hi[] = {0x00000000, 0x80000000, 0x08000000};
    uint32_t lo[] = {0x00000000, 0x40000000, 0x10000000};
    begin_root(2, 1);
    begin_node("memory@80000000"); /* higher address first... */
    prop_cells("reg", hi, 3);
    end_node();
    begin_node("memory@40000000"); /* ...lower address second */
    prop_cells("reg", lo, 3);
    end_node();
    end_node();
    const void *blob = dtb_finalize();

    memmap_t m;
    TEST_ASSERT_EQUAL_INT(2, fdt_get_memory(blob, &m));
    /* memmap keeps ranges sorted by base regardless of node order. */
    TEST_ASSERT_EQUAL_UINT64(0x40000000, m.range[0].base);
    TEST_ASSERT_EQUAL_UINT64(0x80000000, m.range[1].base);
}

static void test_get_memory_honours_custom_cell_counts(void)
{
    /* 32-bit addressing: addr = 1 cell, size = 1 cell. */
    uint32_t reg[] = {0x40000000, 0x20000000};
    begin_root(1, 1);
    begin_node("memory"); /* bare "memory" name is valid */
    prop_cells("reg", reg, 2);
    end_node();
    end_node();
    const void *blob = dtb_finalize();

    memmap_t m;
    TEST_ASSERT_EQUAL_INT(1, fdt_get_memory(blob, &m));
    TEST_ASSERT_EQUAL_UINT64(0x40000000, m.range[0].base);
    TEST_ASSERT_EQUAL_UINT64(0x20000000, m.range[0].size);
}

static void test_get_memory_ignores_non_memory_nodes(void)
{
    /* A sibling node with a reg property must not be mistaken for RAM. */
    uint32_t cpureg[] = {0x00000000};
    uint32_t memreg[] = {0x00000000, 0x40000000, 0x20000000};
    begin_root(2, 1);
    begin_node("cpus");
    prop_cells("reg", cpureg, 1);
    end_node();
    begin_node("memory@40000000");
    prop_cells("reg", memreg, 3);
    end_node();
    end_node();
    const void *blob = dtb_finalize();

    memmap_t m;
    TEST_ASSERT_EQUAL_INT(1, fdt_get_memory(blob, &m));
    TEST_ASSERT_EQUAL_UINT64(0x40000000, m.range[0].base);
}

static void test_get_memory_rejects_invalid_blob(void)
{
    memmap_t m;
    /* NULL and a bad-magic blob both fail, leaving an empty map. */
    TEST_ASSERT_EQUAL_INT(-1, fdt_get_memory(NULL, &m));
    TEST_ASSERT_EQUAL_UINT64(0, m.count);

    begin_root(2, 1);
    end_node();
    const void *blob = dtb_finalize();
    dtb.buf[1] ^= 0xFF; /* break the magic */

    TEST_ASSERT_EQUAL_INT(-1, fdt_get_memory(blob, &m));
    TEST_ASSERT_EQUAL_UINT64(0, m.count);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_fdt_valid_accepts_good_magic_rejects_bad);
    RUN_TEST(test_fdt_totalsize_matches_assembled_length);
    RUN_TEST(test_get_memory_reads_single_region);
    RUN_TEST(test_get_memory_reads_multiple_regions_in_one_reg);
    RUN_TEST(test_get_memory_merges_separate_memory_nodes_sorted);
    RUN_TEST(test_get_memory_honours_custom_cell_counts);
    RUN_TEST(test_get_memory_ignores_non_memory_nodes);
    RUN_TEST(test_get_memory_rejects_invalid_blob);
    return UNITY_END();
}
