FROM ubuntu:22.04
ENV DEBIAN_FRONTEND=noninteractive

# Ensure universe is enabled so packages like qemu-efi are available
RUN apt-get update && apt-get install -y --no-install-recommends software-properties-common ca-certificates && \
    add-apt-repository universe && \
    apt-get update && apt-get install -y --no-install-recommends \
      gcc-aarch64-linux-gnu g++-aarch64-linux-gnu binutils-aarch64-linux-gnu \
      qemu-user-static qemu-system-aarch64 qemu-efi device-tree-compiler ccache build-essential python3 git sudo \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace

CMD ["/bin/bash"]
