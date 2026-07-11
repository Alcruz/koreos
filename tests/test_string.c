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

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_str_eq_true_for_identical);
    RUN_TEST(test_str_eq_false_for_different);
    RUN_TEST(test_str_prefix_matches_leading_substring);
    RUN_TEST(test_str_prefix_empty_prefix_always_matches);
    RUN_TEST(test_str_prefix_rejects_non_prefix);
    RUN_TEST(test_str_len_counts_without_terminator);
    return UNITY_END();
}
