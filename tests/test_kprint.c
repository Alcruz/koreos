/* Host-side unit tests for the console formatting helpers (kernel/lib/kprint.c).
 *
 * kprint is the kernel's front door for console output; its only dependency is
 * the serial backend (serial_putc/serial_puts). We link kprint.c against a stub
 * backend defined right here that captures every byte into a buffer, so the
 * number-formatting logic (kprint_hex, kprint_dec) can be checked against exact
 * expected strings without any hardware.
 */

#include <stddef.h>

#include "unity.h"
#include "kprint.h"
#include "serial.h"

/* Captured console output; reset before each test. */
static char cap[256];
static size_t cap_len;

/* Stub serial backend standing in for the PL011 driver. */
void serial_putc(char c)
{
    if (cap_len < sizeof(cap) - 1)
        cap[cap_len++] = c;
    cap[cap_len] = '\0';
}

void serial_puts(const char *s)
{
    while (*s)
        serial_putc(*s++);
}

void setUp(void)
{
    cap_len = 0;
    cap[0] = '\0';
}

void tearDown(void) {}

/* --- passthrough ------------------------------------------------------- */

static void test_putc_and_puts_reach_the_backend(void)
{
    kprint_putc('h');
    kprint_puts("i!");
    TEST_ASSERT_EQUAL_STRING("hi!", cap);
}

/* --- hex --------------------------------------------------------------- */

static void test_hex_is_always_16_padded_digits(void)
{
    kprint_hex(0);
    TEST_ASSERT_EQUAL_STRING("0x0000000000000000", cap);
}

static void test_hex_formats_value_with_leading_zeros(void)
{
    kprint_hex(0xDEADBEEF);
    TEST_ASSERT_EQUAL_STRING("0x00000000deadbeef", cap);
}

static void test_hex_formats_full_width_value(void)
{
    kprint_hex(0xFFFFFFFFFFFFFFFFULL);
    TEST_ASSERT_EQUAL_STRING("0xffffffffffffffff", cap);
}

/* --- dec --------------------------------------------------------------- */

static void test_dec_zero(void)
{
    kprint_dec(0);
    TEST_ASSERT_EQUAL_STRING("0", cap);
}

static void test_dec_no_leading_zeros(void)
{
    kprint_dec(12345);
    TEST_ASSERT_EQUAL_STRING("12345", cap);
}

static void test_dec_max_uint64(void)
{
    kprint_dec(18446744073709551615ULL);   /* UINT64_MAX */
    TEST_ASSERT_EQUAL_STRING("18446744073709551615", cap);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_putc_and_puts_reach_the_backend);
    RUN_TEST(test_hex_is_always_16_padded_digits);
    RUN_TEST(test_hex_formats_value_with_leading_zeros);
    RUN_TEST(test_hex_formats_full_width_value);
    RUN_TEST(test_dec_zero);
    RUN_TEST(test_dec_no_leading_zeros);
    RUN_TEST(test_dec_max_uint64);
    return UNITY_END();
}
