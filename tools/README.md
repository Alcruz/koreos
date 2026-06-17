# QEMU and tooling

This project uses `qemu-system-aarch64` for development and testing on 64-bit ARM (`aarch64`).

Recommended host packages

- macOS (Homebrew):

```bash
brew install qemu dtc ccache
```

- Debian/Ubuntu:

```bash
sudo apt update
sudo apt install qemu-system-arm qemu-efi-aarch64 dtc ccache build-essential git python3
```

Quick start

1. Build or place a kernel image at `build/kernel/Image` and an initramfs at `build/initramfs.img`.
2. Run the helper script:

```bash
scripts/run-qemu.sh
```

You can also pass explicit paths:

```bash
scripts/run-qemu.sh path/to/Image path/to/initramfs.img path/to/device-tree.dtb
```

Notes

- On Apple Silicon macOS, QEMU may be faster with the `-accel hvf` option if available.
- For full platform emulation (UEFI), provide an EFI firmware like `QEMU_EFI.fd` and add `-bios QEMU_EFI.fd`.
- Adjust `MEM`, `CPU`, and `SMP` environment variables to tune the VM:

```bash
MEM=2048 SMP=4 scripts/run-qemu.sh
```
