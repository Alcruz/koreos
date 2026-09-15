# Koreos — AArch64 OS from Scratch

KoreOS is a homemade 64-bit ARM (AArch64) operating system, written from
scratch for educational purposes.

## Quickstart

| Command       | What it does                                                  |
| ------------- | ------------------------------------------------------------- |
| `make kernel` | Cross-compile the kernel. Produces `build/kernel.bin`.        |
| `make run`    | Boot `kernel.bin` on QEMU virt, headless, serial to stdout.   |
| `make test`   | Host-native Unity suites under `tests/unit/` (ASan + UBSan).  |
| `make smoke`  | On-target smoke tests: each `tests/smoke/test_*.c` gets its own kernel image and QEMU run. |

`make kernel`, `make run`, and `make smoke` need the cross-toolchain and
`qemu-system-aarch64` on `PATH`. The easiest way is to run them inside the
toolchain container:

```bash
docker compose -f toolchain/docker-compose.yml run --rm toolchain make kernel
```

`make test` is plain host C and needs no Docker.

## Toolchain

The `aarch64-linux-gnu-*` cross-tools and `qemu-system-aarch64` live only
in `toolchain/Dockerfile`; the repo is mounted at `/workspace` inside the
container. `scripts/run-qemu.sh` is the QEMU wrapper and expects to be
called inside the container (or with QEMU on `PATH`).

## Layout

```
koreos/
├── kernel/
│   ├── arch/arm64/boot/
│   ├── core/
│   ├── drivers/serial/
│   ├── mm/
│   ├── lib/
│   └── include/
├── tests/
│   ├── unit/               # Unity host-side unit tests (one binary per test_*.c)
│   └── smoke/              # On-target QEMU smoke tests
├── toolchain/
├── scripts/
└── tools/
```

## Boot flow

1. QEMU loads `kernel.bin` at `0x40000000` and enters `_start` (`entry.S`).
   `x0` = device tree blob (DTB) pointer. **Boot the raw `kernel.bin`** —
   `entry.S` carries an arm64 Linux Image header so QEMU loads it at the
   2 MB-aligned RAM base and passes the DTB in `x0`. Booting the ELF loses
   the DTB and panics.
2. `_start` zeros BSS, sets up the stack, installs the EL1 exception vector
   table (`vectors.S`), and calls `kernel_main(dtb)`.
3. `kernel_main` (`kernel/core/main.c`):
   - Brings up the PL011 UART and prints over serial.
   - Parses the DTB `/memory` nodes into a `memmap_t`; **panics** if the DTB is
     missing or has no `/memory` node rather than guessing RAM size.
   - Reserves the kernel image and the DTB itself in the map.
   - Bootstraps the physical frame allocator (`pmm`) over usable RAM.
   - Initializes `MAIR_EL1`, builds identity page tables, and **enables the
     MMU** — the first line printed after `mmu_enable` proves code fetch, stack,
     globals, and MMIO all translate correctly.
   - Verifies PMM alloc/free invariants and brings up the kernel heap
     (`kmalloc`) with a smoke check.
   - Enters the idle loop (`wfe`).

## Roadmap

See [`docs/roadmap/`](docs/roadmap/) for the phased roadmap (goals,
deliverables, and current status per phase).

## Resources

- ARM64 ABI: https://github.com/ARM-software/abi-aa/
- Linux arm64 boot protocol: https://www.kernel.org/doc/html/latest/arm64/booting.html
- QEMU virt: https://qemu.readthedocs.io/

## License

MIT
