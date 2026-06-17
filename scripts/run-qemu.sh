#!/usr/bin/env bash
set -euo pipefail

# Simple helper to run qemu-system-aarch64 with common defaults.
# Usage: scripts/run-qemu.sh [kernel-image] [initramfs.img] [optional.dtb]

KERNEL=${1:-build/kernel/Image}
INITRD=${2:-build/initramfs.img}
DTB=${3:-}
MEM=${MEM:-1024}
CPU=${CPU:-cortex-a57}
SMP=${SMP:-2}

if [ ! -f "$KERNEL" ]; then
  echo "Kernel not found: $KERNEL"
  exit 1
fi

if [ ! -f "$INITRD" ]; then
  echo "Initramfs not found: $INITRD"
  exit 1
fi

CMD=(qemu-system-aarch64)
CMD+=( -machine virt )
CMD+=( -cpu "$CPU" )
CMD+=( -smp "$SMP" )
CMD+=( -m "$MEM" )
CMD+=( -nographic )
CMD+=( -kernel "$KERNEL" )
CMD+=( -initrd "$INITRD" )
CMD+=( -append "console=ttyAMA0 root=/dev/ram rw" )
CMD+=( -netdev user,id=net0 )
CMD+=( -device virtio-net-device,netdev=net0 )

if [ -n "$DTB" ] && [ -f "$DTB" ]; then
  CMD+=( -dtb "$DTB" )
fi

echo "Running: ${CMD[*]}"
exec "${CMD[@]}"
