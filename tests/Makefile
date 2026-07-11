# Host-side unit tests, built with the NATIVE compiler (not the cross-toolchain)
# and run directly on the host — no QEMU. Kernel modules under test are plain,
# address-agnostic C compiled straight into the test binary, with the Unity test
# framework (see unity/) providing assertions and reporting.
#
# One executable is built per test_<suite>.c, each with its own main().

CC ?= cc

# Sanitizers are the point of running on the host: they watch every bitmap and
# frame access the allocator makes. UNITY_SUPPORT_64 enables the 64-bit
# assertions (TEST_ASSERT_EQUAL_UINT64) the kernel's addresses/sizes need.
CFLAGS := -std=c11 -Wall -Wextra -g -O1 \
          -fsanitize=address,undefined -fno-omit-frame-pointer \
          -DUNITY_SUPPORT_64 \
          -I../kernel/include -Iunity

# Arch/OS-specific so host-native and container (cross-env) builds never collide
# in the same tree — these are host binaries and may be built in either place.
BUILD := build/$(shell uname -s)-$(shell uname -m)
UNITY_SRC := unity/unity.c

# Kernel modules linked into every suite. Extra modules a suite doesn't use are
# harmless; add here as new suites need them.
KERNEL_SRC := ../kernel/mm/pmm.c ../kernel/mm/memmap.c

# test_pmm.c -> build/pmm, etc.
SUITES := $(patsubst test_%.c,$(BUILD)/%,$(wildcard test_*.c))

all: test

test: $(SUITES)
	@fail=0; for r in $(SUITES); do echo "=== $$r ==="; ./$$r || fail=1; done; exit $$fail

$(BUILD)/%: test_%.c $(UNITY_SRC) $(KERNEL_SRC) unity/unity.h ../kernel/include/*.h
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -o $@ test_$*.c $(UNITY_SRC) $(KERNEL_SRC)

clean:
	rm -rf build

.PHONY: all test clean
