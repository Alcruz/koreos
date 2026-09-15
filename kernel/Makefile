ARCH ?= arm64
CROSS_COMPILE ?= aarch64-linux-gnu-
CC := $(CROSS_COMPILE)gcc
AS := $(CROSS_COMPILE)as
LD := $(CROSS_COMPILE)ld
OBJCOPY := $(CROSS_COMPILE)objcopy

# GCC ships the freestanding headers (stdint.h, stddef.h, stdbool.h) in its own
# builtin include dir. -nostdinc removes ALL default include paths, so add just
# that one back with -isystem — this keeps target libc headers out while making
# the C11 freestanding headers available.
GCC_INCLUDE := $(shell $(CC) -print-file-name=include)

# -mgeneral-regs-only: don't emit FP/SIMD instructions. The kernel runs at EL1
# with CPACR_EL1.FPEN clearing SIMD/FP access; without this flag, later gcc
# versions (13+) auto-vectorise memset/memcpy-like paths and the kernel takes
# a synchronous EC=0x07 the first time one runs.
CFLAGS := -Wall -O2 -ffreestanding -nostdinc -nostdlib -fno-builtin \
          -mgeneral-regs-only -isystem $(GCC_INCLUDE)
ASFLAGS := -march=armv8-a
LDFLAGS := -T arch/$(ARCH)/boot/linker.ld -nostdlib

BUILD_DIR ?= ../build

# SMOKE=<name> selects an on-target smoke test as the entry point:
# tests/smoke/test_<name>.c provides kernel_main, tests/smoke/smoke.c
# provides bring-up + shutdown. The regular kernel image (main.c) is not
# linked in that mode. Output goes to build/smoke/kernel-<name>.bin so smoke
# builds and the real kernel don't collide.
SMOKE ?=

# Kernel objects shared between the real image and every smoke image. Excludes
# core/main.c because SMOKE=<name> replaces it with a test-specific entry.
KERNEL_SRC := $(filter-out core/main.c, $(wildcard core/*.c drivers/*/*.c mm/*.c lib/*.c))
KERNEL_ASM := $(wildcard arch/$(ARCH)/boot/*.S arch/$(ARCH)/task/*.S)
KERNEL_OBJ := $(KERNEL_SRC:%.c=$(BUILD_DIR)/%.o) $(KERNEL_ASM:%.S=$(BUILD_DIR)/%.o)

ifeq ($(SMOKE),)
ENTRY_SRC := core/main.c
ENTRY_OBJ := $(BUILD_DIR)/core/main.o
OUT := $(BUILD_DIR)/kernel.elf
BIN := $(BUILD_DIR)/kernel.bin
else
ENTRY_SRC := ../tests/smoke/test_$(SMOKE).c ../tests/smoke/smoke.c
ENTRY_OBJ := $(BUILD_DIR)/smoke/test_$(SMOKE).o $(BUILD_DIR)/smoke/smoke.o
OUT := $(BUILD_DIR)/smoke/kernel-$(SMOKE).elf
BIN := $(BUILD_DIR)/smoke/kernel-$(SMOKE).bin
endif

all: $(BIN)

$(OUT): $(KERNEL_OBJ) $(ENTRY_OBJ)
	@echo "Linking $@..."
	@mkdir -p $(dir $@)
	$(LD) $(LDFLAGS) -o $@ $^

$(BIN): $(OUT)
	@echo "Creating $@..."
	@mkdir -p $(dir $@)
	$(OBJCOPY) -O binary $< $@
	@ls -lh $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/smoke/%.o: ../tests/smoke/%.c
	@mkdir -p $(dir $@)
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	@echo "Assembling $<..."
	$(AS) $(ASFLAGS) -c -o $@ $<

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean
