.PHONY: all clean kernel run qemu-run test help

help:
	@echo "Koreos OS Build Targets"
	@echo "======================="
	@echo "  make kernel    - Build kernel"
	@echo "  make test      - Build and run host-side unit tests"
	@echo "  make clean     - Clean all build artifacts"
	@echo "  make run       - Build and run on QEMU"
	@echo "  make help      - Show this help"

all: kernel

kernel:
	cd kernel && $(MAKE)

# Host-side unit tests, built with the native compiler. Runs the whole suite
# and exits nonzero on any failure. See tests/.
test:
	cd tests && $(MAKE) test

clean:
	cd kernel && $(MAKE) clean
	cd tests && $(MAKE) clean

run: kernel
	./scripts/run-qemu.sh

.PHONY: help all kernel clean run test
