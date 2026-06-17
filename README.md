# Koreos OS – AArch64 from Scratch

A modern 64-bit ARM OS built from scratch for QEMU virt machine.

## Quick Start

### Prerequisites
- AArch64 cross-toolchain: `aarch64-linux-gnu-gcc`, `aarch64-linux-gnu-as`, `aarch64-linux-gnu-ld`
- QEMU: `qemu-system-aarch64`

### Build & Run
```bash
# Build kernel
make kernel

# Run on QEMU
make run
```

## Architecture

```
koreos/
├── kernel/                  # Kernel sources
│   ├── arch/arm64/boot/    # AArch64 entry point & linker script
│   ├── core/               # Scheduler, MM, IPC, VFS
│   ├── drivers/serial/     # PL011 UART driver
│   └── include/            # Kernel headers
├── userspace/              # User-mode programs, shells
├── libs/                   # libc, libm, etc.
├── scripts/                # Build, QEMU runner scripts
├── tools/                  # Development tools
├── docs/                   # Documentation
├── config/                 # Configuration & device trees
└── tests/                  # Unit & integration tests
```

## Boot Flow (Direct Kernel on QEMU virt)

1. QEMU loads kernel binary at 0x40000000
2. CPU enters AArch64 mode at `_start` (entry.S)
3. Entry code clears BSS, sets up stack
4. Calls `kernel_main()` → prints hello message to serial
5. Halts in idle loop

## Next Steps

- [ ] Implement basic memory management (page tables, buddy allocator)
- [ ] Add interrupt handling (GIC driver)
- [ ] Implement task scheduler
- [ ] Add more device drivers (block, network)
- [ ] Userspace & libc
- [ ] Init process & shell

## Resources

- ARM64 ABI: https://github.com/ARM-software/abi-aa/
- QEMU virt: https://qemu.readthedocs.io/
- Linux kernel (reference): https://github.com/torvalds/linux

## License

MIT (or your preferred license)
