# Koreos — AArch64 OS from Scratch

# Overview

KoreOS is a homemade 64-bit ARM (AArch64) operating system, written from
scratch for educational purposes.

## Toolchain: Docker

The `aarch64-linux-gnu-*` tools and `qemu-system-aarch64` live only in the toolchain
container (`toolchain/Dockerfile`). Always build and run inside Docker. The repo
is mounted at `/workspace` inside the container.

### Build the kernel

```bash
docker compose -f toolchain/docker-compose.yml run --rm toolchain make kernel
```

This produces `build/kernel.elf` and `build/kernel.bin`. **Boot the raw
`kernel.bin`** — `entry.S` carries an arm64 Linux Image header so
QEMU loads it at the 2 MB-aligned RAM base (`0x40000000`) and passes the DTB
pointer in `x0`. Booting the ELF loses the DTB and panics.

### Boot in QEMU (headless, serial to a file)

```bash
docker compose -f toolchain/docker-compose.yml run --rm toolchain bash -c \
  'qemu-system-aarch64 -no-user-config -nodefaults -machine virt -cpu cortex-a57 \
     -m 512M -kernel /workspace/build/kernel.bin -serial file:/tmp/q.log -display none & \
   QPID=$!; sleep 3; kill -9 $QPID; cat /tmp/q.log'
```

(`scripts/run-qemu.sh` wraps this but assumes QEMU is on `PATH`, so run it inside
the container too.)

### Host-side unit tests

Kernel modules are plain, address-agnostic C, so the memory/library code is
tested natively on the host — no QEMU. Tests use the [Unity](tests/unity/)
framework and build with AddressSanitizer + UndefinedBehaviorSanitizer.

```bash
make test        # native host compiler; no Docker needed
```

Suites live in `tests/` (`test_pmm`, `test_memmap`, `test_kmalloc`,
`test_string`, `test_fdt`, `test_kprint`).

## Layout

```
koreos/
├── kernel/
│   ├── arch/arm64/boot/    # entry.S (Image header, BSS zero, stack), vectors.S, linker.ld
│   ├── core/               # kernel_main, exception handlers, panic
│   ├── drivers/serial/     # PL011 UART driver
│   ├── mm/                 # memmap, pmm (frame allocator), kmalloc (heap), mmu
│   ├── lib/                # fdt parser, kprint, string
│   └── include/            # kernel headers
├── tests/                  # Unity host-side unit tests
├── toolchain/              # Docker cross-toolchain + QEMU
├── scripts/                # QEMU runner, toolchain shell
└── tools/
```

## Boot flow

1. QEMU loads `kernel.bin` at `0x40000000` and enters `_start` (`entry.S`).
   `x0` = device tree blob (DTB) pointer.
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

## Implemented

- [x] Direct kernel boot on QEMU virt (arm64 Image header, DTB in `x0`)
- [x] PL011 UART serial + `kprint` (strings, hex, decimal)
- [x] Flattened device tree (FDT) parsing, `/memory` sizing
- [x] Physical memory map (`memmap`) with reservations
- [x] Physical frame allocator (`pmm`)
- [x] Kernel heap allocator (`kmalloc`/`kfree`/`kzalloc`)
- [x] MMU: `MAIR` setup, identity page tables, translation enabled
- [x] EL1 exception vectors + `panic`
- [x] Host-side unit test harness (Unity, ASan/UBSan)

## Next steps

- [ ] Interrupt handling (GIC driver)
- [ ] Task scheduler
- [ ] Higher-half kernel / proper virtual memory layout
- [ ] More drivers (timer, block, network)
- [ ] Userspace, syscalls, init & shell

## Resources

- ARM64 ABI: https://github.com/ARM-software/abi-aa/
- Linux arm64 boot protocol: https://www.kernel.org/doc/html/latest/arm64/booting.html
- QEMU virt: https://qemu.readthedocs.io/

## License

MIT
