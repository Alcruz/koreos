/* Host-side unit tests for the freestanding string helpers (kernel/lib/string.c).
 *
 * These are the kernel's stand-ins for the libc string functions it can't link
 * against. Pure and address-agnostic, so they test with plain string literals.
 */

#include "unity.h"
#include "string.h"

void setUp(void) {}
void tearDown(void) {}

/* --- str_eq ------------------------------------------------------------ */

static void test_str_eq_true_for_identical(void)
{
    TEST_ASSERT_TRUE(str_eq("memory", "memory"));
    TEST_ASSERT_TRUE(str_eq("", ""));
}

static void test_str_eq_false_for_different(void)
{
    TEST_ASSERT_FALSE(str_eq("memory", "cpus"));
    TEST_ASSERT_FALSE(str_eq("reg", "regs"));   /* one is a prefix of the other */
    TEST_ASSERT_FALSE(str_eq("regs", "reg"));   /* ...and the other way around  */
    TEST_ASSERT_FALSE(str_eq("a", ""));
    TEST_ASSERT_FALSE(str_eq("", "a"));
}

/* --- str_prefix -------------------------------------------------------- */

static void test_str_prefix_matches_leading_substring(void)
{
    TEST_ASSERT_TRUE(str_prefix("memory@40000000", "memory"));
    TEST_ASSERT_TRUE(str_prefix("memory", "memory"));   /* exact is a prefix */
}

static void test_str_prefix_empty_prefix_always_matches(void)
{
    TEST_ASSERT_TRUE(str_prefix("anything", ""));
    TEST_ASSERT_TRUE(str_prefix("", ""));
}

static void test_str_prefix_rejects_non_prefix(void)
{
    TEST_ASSERT_FALSE(str_prefix("memory", "mem0"));
    TEST_ASSERT_FALSE(str_prefix("cpus", "memory"));
    /* A prefix longer than the string can never match. */
    TEST_ASSERT_FALSE(str_prefix("mem", "memory"));
    TEST_ASSERT_FALSE(str_prefix("", "x"));
}

/* --- str_len ----------------------------------------------------------- */

static void test_str_len_counts_without_terminator(void)
{
    TEST_ASSERT_EQUAL_UINT64(0, str_len(""));
    TEST_ASSERT_EQUAL_UINT64(6, str_len("memory"));
    TEST_ASSERT_EQUAL_UINT64(1, str_len("x"));
}

/* --- memcpy ------------------------------------------------------------ */

static void test_memcpy_copies_all_bytes_and_returns_dest(void)
{
    /* Embedded NUL proves it copies raw bytes, not a C string. */
    const unsigned char src[6] = {0x11, 0x00, 0x22, 0x33, 0xff, 0x44};
    unsigned char dst[6] = {0};

    TEST_ASSERT_EQUAL_PTR(dst, memcpy(dst, src, sizeof src));
    TEST_ASSERT_EQUAL_MEMORY(src, dst, sizeof src);
}

static void test_memcpy_zero_length_leaves_dest_untouched(void)
{
    unsigned char dst[3] = {0xaa, 0xbb, 0xcc};
    const unsigned char src[3] = {0x00, 0x11, 0x22};

    TEST_ASSERT_EQUAL_PTR(dst, memcpy(dst, src, 0));
    TEST_ASSERT_EQUAL_UINT8(0xaa, dst[0]);
    TEST_ASSERT_EQUAL_UINT8(0xbb, dst[1]);
    TEST_ASSERT_EQUAL_UINT8(0xcc, dst[2]);
}

static void test_memcpy_does_not_write_past_n(void)
{
    unsigned char dst[4] = {0, 0, 0, 0xde};
    const unsigned char src[3] = {0x01, 0x02, 0x03};

    memcpy(dst, src, 3);
    TEST_ASSERT_EQUAL_UINT8(0xde, dst[3]);   /* trailing byte preserved */
}

/* --- memset ------------------------------------------------------------ */

static void test_memset_fills_and_returns_dest(void)
{
    unsigned char buf[5] = {0};

    TEST_ASSERT_EQUAL_PTR(buf, memset(buf, 0xab, sizeof buf));
    for (size_t i = 0; i < sizeof buf; i++)
        TEST_ASSERT_EQUAL_UINT8(0xab, buf[i]);
}

static void test_memset_truncates_value_to_a_byte(void)
{
    unsigned char buf[2] = {0};

    /* Only the low 8 bits of c are written. */
    memset(buf, 0x1ff, sizeof buf);
    TEST_ASSERT_EQUAL_UINT8(0xff, buf[0]);
    TEST_ASSERT_EQUAL_UINT8(0xff, buf[1]);
}

static void test_memset_zero_length_leaves_dest_untouched(void)
{
    unsigned char buf[3] = {0x01, 0x02, 0x03};

    memset(buf, 0xff, (0));   /* parenthesized: intentional zero length, not transposed args */
    TEST_ASSERT_EQUAL_UINT8(0x01, buf[0]);
    TEST_ASSERT_EQUAL_UINT8(0x02, buf[1]);
    TEST_ASSERT_EQUAL_UINT8(0x03, buf[2]);
}

/* --- memmove ----------------------------------------------------------- */

static void test_memmove_non_overlapping_copies_and_returns_dest(void)
{
    const unsigned char src[4] = {0x0a, 0x0b, 0x0c, 0x0d};
    unsigned char dst[4] = {0};

    TEST_ASSERT_EQUAL_PTR(dst, memmove(dst, src, sizeof src));
    TEST_ASSERT_EQUAL_MEMORY(src, dst, sizeof src);
}

static void test_memmove_overlap_dest_above_src(void)
{
    /* dst > src: a naive forward copy would clobber; memmove copies backward. */
    unsigned char buf[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    const unsigned char expected[8] = {1, 2, 1, 2, 3, 4, 7, 8};

    memmove(buf + 2, buf, 4);
    TEST_ASSERT_EQUAL_MEMORY(expected, buf, sizeof buf);
}

static void test_memmove_overlap_dest_below_src(void)
{
    /* dst < src: forward copy is correct. */
    unsigned char buf[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    const unsigned char expected[8] = {3, 4, 5, 6, 5, 6, 7, 8};

    memmove(buf, buf + 2, 4);
    TEST_ASSERT_EQUAL_MEMORY(expected, buf, sizeof buf);
}

static void test_memmove_same_pointer_is_noop(void)
{
    unsigned char buf[4] = {9, 8, 7, 6};
    const unsigned char expected[4] = {9, 8, 7, 6};

    TEST_ASSERT_EQUAL_PTR(buf, memmove(buf, buf, sizeof buf));
    TEST_ASSERT_EQUAL_MEMORY(expected, buf, sizeof buf);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_str_eq_true_for_identical);
    RUN_TEST(test_str_eq_false_for_different);
    RUN_TEST(test_str_prefix_matches_leading_substring);
    RUN_TEST(test_str_prefix_empty_prefix_always_matches);
    RUN_TEST(test_str_prefix_rejects_non_prefix);
    RUN_TEST(test_str_len_counts_without_terminator);
    RUN_TEST(test_memcpy_copies_all_bytes_and_returns_dest);
    RUN_TEST(test_memcpy_zero_length_leaves_dest_untouched);
    RUN_TEST(test_memcpy_does_not_write_past_n);
    RUN_TEST(test_memset_fills_and_returns_dest);
    RUN_TEST(test_memset_truncates_value_to_a_byte);
    RUN_TEST(test_memset_zero_length_leaves_dest_untouched);
    RUN_TEST(test_memmove_non_overlapping_copies_and_returns_dest);
    RUN_TEST(test_memmove_overlap_dest_above_src);
    RUN_TEST(test_memmove_overlap_dest_below_src);
    RUN_TEST(test_memmove_same_pointer_is_noop);
    return UNITY_END();
}
