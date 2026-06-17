.PHONY: all clean kernel run qemu-run help

help:
	@echo "Koreos OS Build Targets"
	@echo "======================="
	@echo "  make kernel    - Build kernel"
	@echo "  make clean     - Clean all build artifacts"
	@echo "  make run       - Build and run on QEMU"
	@echo "  make help      - Show this help"

all: kernel

kernel:
	cd kernel && $(MAKE)

clean:
	cd kernel && $(MAKE) clean

run: kernel
	./scripts/run-qemu.sh

.PHONY: help all kernel clean run
