---
name: run-qemu
description: Build and boot the koreos kernel on QEMU virt (aarch64) via the Docker toolchain, capturing serial output. Use whenever the user asks to "run the OS", "boot the kernel", "run in QEMU", or "smoke test" the kernel.
---

# Run koreos in QEMU

The cross-toolchain and `qemu-system-aarch64` only exist inside the Docker
container defined in `toolchain/docker-compose.yml`. The host `make run` target
assumes GNU `timeout`, which macOS does not ship — always drive QEMU through
Docker instead.

## Build (if needed)

```bash
docker compose -f toolchain/docker-compose.yml run --rm toolchain make kernel
```

Produces `build/kernel.bin` (the raw image with the arm64 Linux Image header)
and `build/kernel.elf`. **Always boot `kernel.bin`, not the ELF.** QEMU's
`load_elf` path does not pass a DTB (`x0 = 0`), and the kernel panics rather
than guess RAM size — so the ELF boot fails immediately.

## Boot and capture serial

Run QEMU in the background inside the container, kill it after a few seconds,
and print the captured serial log:

```bash
docker compose -f toolchain/docker-compose.yml run --rm toolchain bash -c '
  rm -f /tmp/q.log
  qemu-system-aarch64 -no-user-config -nodefaults -machine virt -cpu cortex-a57 \
    -m 512M -kernel /workspace/build/kernel.bin \
    -serial file:/tmp/q.log -display none &
  QPID=$!
  sleep 3
  kill -9 $QPID 2>/dev/null || true
  wait 2>/dev/null || true
  cat /tmp/q.log
'
```

## Reporting

Show the captured serial output verbatim to the user, then note whether the
boot reached the idle loop (`wfe`) or panicked. If there's no output, the
kernel likely faulted before the UART came up — surface that explicitly.
