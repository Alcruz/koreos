#!/bin/bash
set -e

# Boot the raw image, not the ELF: only kernel.bin (with its arm64 Image
# header) loads at 0x40000000 and gets a DTB in x0. See CLAUDE.md.
KERNEL_BIN="./build/kernel.bin"
QEMU_CMD="qemu-system-aarch64"

if [ ! -f "$KERNEL_BIN" ]; then
    echo "Error: $KERNEL_BIN not found. Run 'make kernel' first."
    exit 1
fi

if ! command -v "$QEMU_CMD" &> /dev/null; then
    echo "Error: $QEMU_CMD not found. Install QEMU for ARM64."
    exit 1
fi

echo "Booting Koreos on QEMU virt..."
echo "========================================"

# Run QEMU in background with serial output to file
rm -f /tmp/qemu-serial.log
timeout 5 $QEMU_CMD \
    -no-user-config -nodefaults \
    -machine virt \
    -cpu cortex-a57 \
    -m 512M \
    -kernel "$KERNEL_BIN" \
    -serial file:/tmp/qemu-serial.log \
    "$@" 2>&1 &

QEMU_PID=$!
sleep 2

# Check if process still running
if ps -p $QEMU_PID > /dev/null 2>&1; then
    kill -9 $QEMU_PID 2>/dev/null || true
fi

echo ""
echo "Serial output:"
echo "========================================"
if [ -f /tmp/qemu-serial.log ] && [ -s /tmp/qemu-serial.log ]; then
    cat /tmp/qemu-serial.log
else
    echo "(No serial output written)"
fi
