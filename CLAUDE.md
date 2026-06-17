# Koreos — AArch64 OS from scratch

## Toolchain & running: always use Docker

There is **no local cross-toolchain or QEMU** on the host. The `aarch64-linux-gnu-*`
tools and `qemu-system-aarch64` live only in the toolchain container
(`toolchain/Dockerfile`). Always build and run inside Docker.

- Build: `make kernel` runs the host `make`, which expects the cross-compiler —
  run it through the container instead.
- The wrapper `scripts/toolchain-shell.sh` word-splits its arguments (`${@:-bash}`
  is unquoted), so multi-word commands get mangled. For anything beyond a single
  word, invoke docker compose directly:

```bash
# Build
docker compose -f toolchain/docker-compose.yml run --rm toolchain make kernel

# Boot in QEMU (serial captured to a file, headless)
# Boot the ELF, NOT kernel.bin: QEMU's -kernel loads a raw arm64 image at
# RAM base + 0x80000, which does not match our 0x40000000 link address and
# silently breaks absolute-addressed code (BSS clear, page tables, VBAR_EL1).
docker compose -f toolchain/docker-compose.yml run --rm toolchain bash -c \
  'qemu-system-aarch64 -no-user-config -nodefaults -machine virt -cpu cortex-a57 \
     -m 512M -kernel /workspace/build/kernel.elf -serial file:/tmp/q.log -display none & \
   QPID=$!; sleep 3; kill -9 $QPID; cat /tmp/q.log'
```

The repo is mounted at `/workspace` inside the container.

## Boot contract (don't break)

- QEMU loads the kernel at `0x40000000` and enters `_start` (`kernel/arch/arm64/boot/entry.S`).
  This holds **only when booting `kernel.elf`** — booting the raw `kernel.bin` loads at
  `0x40080000` instead and breaks all absolute-addressed code. Always `-kernel kernel.elf`.
- `x0` holds the **device tree blob (DTB) pointer** on entry — preserve it through
  `_start` so it reaches `kernel_main`. Use scratch registers (`x9`+) for setup code.
- `_start` must zero BSS (`_bss_start`..`_bss_end`, 8-byte aligned) before any C code,
  since nothing else does it.
