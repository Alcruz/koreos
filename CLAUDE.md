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
# header (text_offset=0, flags=0), but this QEMU (6.2.0) does not honor it for
# a raw -kernel blob: it loads kernel.bin at ram_base + 0x200000 (our link
# address) and synthesizes its own entry stub at ram_base (sets x0=DTB,
# branches to the real load address) instead of jumping to ram_base+text_offset
# directly. Booting the ELF takes QEMU's load_elf path, which does NOT pass a
# DTB (x0=0) — and the kernel now panics rather than guess RAM size.
docker compose -f toolchain/docker-compose.yml run --rm toolchain bash -c \
  'qemu-system-aarch64 -no-user-config -nodefaults -machine virt -cpu cortex-a57 \
     -m 512M -kernel /workspace/build/kernel.bin -serial file:/tmp/q.log -display none & \
   QPID=$!; sleep 3; kill -9 $QPID; cat /tmp/q.log'
```

The repo is mounted at `/workspace` inside the container.

## Boot contract (don't break)

- QEMU (6.2.0, this container) loads a raw `-kernel kernel.bin` at `ram_base + 0x200000`
  (`0x40200000` here), **not** `ram_base + text_offset` as the arm64 Linux Image header
  in `entry.S` requests — it ignores that header for raw blobs and synthesizes its own
  tiny entry stub at `ram_base` (sets `x0`=DTB, `x1`-`x3`=0, branches to the real load
  address) rather than jumping to `ram_base+text_offset` directly. `kernel/arch/arm64/boot/linker.ld`
  links the kernel at `0x40200000` to match. **Do not "fix" this back to `0x40000000`** —
  that was the original (wrong) assumption; verified empirically with `-d in_asm,int` (see
  git history on the GICv2/timer IRQ work). If the link address and QEMU's actual load
  address ever diverge again, everything using PC-relative addressing (branches, most
  code) keeps working by luck, but anything resolving an absolute link-time address
  (`VBAR_EL1`, `_kernel_start`/`_kernel_end`) is silently wrong — the symptom is exceptions
  vectoring into empty RAM and hanging with zero output, not a clean crash.
- The first 64 bytes of `kernel.bin` are still the arm64 Linux Image header (`code0`
  branches past it to `_start_main`); keep it even though this QEMU doesn't honor
  `text_offset` — a different loader might. Booting the bare ELF instead of `kernel.bin`
  takes QEMU's `load_elf` path, which does NOT pass a DTB (`x0=0`) — the kernel panics
  rather than guess RAM size — so always `-kernel kernel.bin`.
- `x0` holds the **device tree blob (DTB) pointer** on entry — preserve it through
  `_start` so it reaches `kernel_main`. Use scratch registers (`x9`+) for setup code. The
  kernel parses the DTB's `/memory` nodes into a `memmap_t` (`fdt_get_memory`); if `x0` is
  null/invalid or carries no `/memory` node, boot **panics** rather than guessing a size.
- `_start` must zero BSS (`_bss_start`..`_bss_end`, 8-byte aligned) before any C code,
  since nothing else does it.
