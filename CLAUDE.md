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
# Boot the raw kernel.bin, NOT the ELF. entry.S carries an arm64 Linux Image
# header (text_offset=0, flags=0), so QEMU loads kernel.bin at the 2MB-aligned
# RAM base (0x40000000, our link address) AND loads the device tree into RAM,
# passing its address in x0. Booting the ELF takes QEMU's load_elf path, which
# does NOT pass a DTB (x0=0) — and the kernel now panics rather than guess RAM size.
docker compose -f toolchain/docker-compose.yml run --rm toolchain bash -c \
  'qemu-system-aarch64 -no-user-config -nodefaults -machine virt -cpu cortex-a57 \
     -m 512M -kernel /workspace/build/kernel.bin -serial file:/tmp/q.log -display none & \
   QPID=$!; sleep 3; kill -9 $QPID; cat /tmp/q.log'
```

The repo is mounted at `/workspace` inside the container.

## Boot contract (don't break)

- QEMU loads the kernel at `0x40000000` and enters `_start` (`kernel/arch/arm64/boot/entry.S`).
  This requires booting the raw **`kernel.bin`** via the arm64 Linux Image header at the
  top of `entry.S` (text_offset=0, flags=0 → load at the 2MB-aligned RAM base). The first
  64 bytes of the image are that header; `code0` branches past it to `_start_main`. Booting
  the bare ELF skips the header (QEMU's load_elf path), loses the DTB, and is wrong — always
  `-kernel kernel.bin`.
- `x0` holds the **device tree blob (DTB) pointer** on entry — preserve it through
  `_start` so it reaches `kernel_main`. Use scratch registers (`x9`+) for setup code. The
  kernel parses the DTB's `/memory` nodes into a `memmap_t` (`fdt_get_memory`); if `x0` is
  null/invalid or carries no `/memory` node, boot **panics** rather than guessing a size.
- `_start` must zero BSS (`_bss_start`..`_bss_end`, 8-byte aligned) before any C code,
  since nothing else does it.
