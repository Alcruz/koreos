# Toolchain container

This folder contains a Dockerfile and docker-compose configuration for a reproducible cross-toolchain environment targeting `aarch64`.

Build and run a shell in the container from the repo root:

```bash
docker compose -f toolchain/docker-compose.yml build
docker compose -f toolchain/docker-compose.yml run --rm toolchain
```

The container provides:

- `aarch64-linux-gnu-gcc`, `aarch64-linux-gnu-g++`, `binutils` for linking
- `qemu-system-arm`, `qemu-user-static` for emulation
- `dtc`, `ccache`, `python3`, `git` and build-essential packages

Mount the repository into `/workspace` (already configured by `docker-compose.yml`).

Use the container for building and testing to ensure reproducibility across hosts.
