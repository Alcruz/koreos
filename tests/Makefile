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
# harmless; add here as new suites need them. Modules with unresolved externals
# (e.g. kprint.c needs a serial backend) can't live here — see EXTRA_* below.
KERNEL_SRC := ../kernel/mm/pmm.c ../kernel/mm/memmap.c ../kernel/mm/kmalloc.c \
              ../kernel/lib/string.c ../kernel/lib/fdt.c

# Per-suite extra sources, appended only to that one suite's link. kprint.c calls
# the serial backend, which test_kprint.c stubs out, so it must not be linked
# into suites that don't provide that stub.
EXTRA_kprint := ../kernel/lib/kprint.c

# test_pmm.c -> build/pmm, etc.
SUITES := $(patsubst test_%.c,$(BUILD)/%,$(wildcard test_*.c))

all: test

test: $(SUITES)
	@fail=0; for r in $(SUITES); do echo "=== $$r ==="; ./$$r || fail=1; done; exit $$fail

$(BUILD)/%: test_%.c $(UNITY_SRC) $(KERNEL_SRC) $(EXTRA_kprint) unity/unity.h ../kernel/include/*.h
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -o $@ test_$*.c $(UNITY_SRC) $(KERNEL_SRC) $(EXTRA_$*)

clean:
	rm -rf build

.PHONY: all test clean
