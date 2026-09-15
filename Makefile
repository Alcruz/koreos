.PHONY: all clean kernel run smoke test help

help:
	@echo "Koreos OS Build Targets"
	@echo "======================="
	@echo "  make kernel    - Build kernel"
	@echo "  make test      - Build and run host-side unit tests"
	@echo "  make smoke     - Build and run on-target smoke tests (QEMU, isolated per test)"
	@echo "  make clean     - Clean all build artifacts"
	@echo "  make run       - Build and run on QEMU"
	@echo "  make help      - Show this help"

all: kernel

kernel:
	cd kernel && $(MAKE)

# Host-side unit tests, built with the native compiler. Runs the whole suite
# and exits nonzero on any failure. See tests/.
test:
	cd tests/unit && $(MAKE) test

# On-target smoke tests: each tests/smoke/test_*.c is linked into its own
# kernel image and booted in its own QEMU invocation. See scripts/run-smoke.sh.
smoke:
	./scripts/run-smoke.sh

clean:
	cd kernel && $(MAKE) clean
	cd tests/unit && $(MAKE) clean

run: kernel
	./scripts/run-qemu.sh
