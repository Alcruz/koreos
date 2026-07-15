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

CFLAGS := -Wall -O2 -ffreestanding -nostdinc -nostdlib -fno-builtin -isystem $(GCC_INCLUDE)
ASFLAGS := -march=armv8-a
LDFLAGS := -T arch/$(ARCH)/boot/linker.ld -nostdlib

BUILD_DIR ?= ../build
OUT := $(BUILD_DIR)/kernel.elf
BIN := $(BUILD_DIR)/kernel.bin

KERNEL_SRC := $(wildcard core/*.c drivers/*/*.c mm/*.c lib/*.c)
KERNEL_ASM := $(wildcard arch/$(ARCH)/boot/*.S)

KERNEL_OBJ := $(KERNEL_SRC:%.c=$(BUILD_DIR)/%.o)
KERNEL_OBJ += $(KERNEL_ASM:%.S=$(BUILD_DIR)/%.o)

all: $(BIN)

$(OUT): $(KERNEL_OBJ)
	@echo "Linking kernel..."
	@mkdir -p $(BUILD_DIR)
	$(LD) $(LDFLAGS) -o $@ $^

$(BIN): $(OUT)
	@echo "Creating kernel binary..."
	$(OBJCOPY) -O binary $< $@
	@ls -lh $@

$(BUILD_DIR)/%.o: %.c
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
