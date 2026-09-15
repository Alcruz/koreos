#!/bin/sh
# PreToolUse(Bash): block `qemu-system-aarch64 -kernel …/kernel.elf`.
# Booting the ELF loses the DTB pointer in x0 and panics — always boot kernel.bin.
input=$(cat)
if printf '%s' "$input" | grep -Eq 'qemu-system-aarch64[^|;&]*-kernel[[:space:]]+[^[:space:]]*kernel\.elf'; then
  echo "Refusing: booting kernel.elf loses the DTB pointer in x0 and panics. Use build/kernel.bin (see README > Boot in QEMU)." >&2
  exit 2
fi
exit 0
