---
name: run-tests
description: Build and run the host-side Unity unit test suites for koreos kernel modules. Use whenever the user asks to "run tests", "run the test suite", or verify a change to code under kernel/mm, kernel/lib, or kernel/core.
---

# Run koreos host-side tests

Kernel modules under test are address-agnostic C — they build with the native
host compiler and run directly on the host under AddressSanitizer +
UndefinedBehaviorSanitizer. No Docker, no QEMU.

## Run the full suite

```bash
make test
```

This delegates to `tests/Makefile`, which builds one binary per `test_*.c`
into `tests/build/<uname>-<arch>/` and runs each. The target exits non-zero if
any suite fails.

## Run a single suite

```bash
cd tests && make build/$(uname -s)-$(uname -m)/<suite> && ./build/$(uname -s)-$(uname -m)/<suite>
```

Replace `<suite>` with the name of any `test_<suite>.c` in `tests/`.

## When adding a suite

- Drop `test_<name>.c` into `tests/` — the Makefile picks it up via the glob.
- If the module under test has unresolved externals (e.g. calls into a driver
  the test can't link), add its sources to `EXTRA_<name>` in `tests/Makefile`
  rather than to the shared `KERNEL_SRC`.
- Write tests against **observable behavior**, not internals — see the
  guideline in `CLAUDE.md`.

## Reporting

If tests pass, say so and name the suites that ran. If any fail, quote the
first failing `TEST_ASSERT_*` line and the file:line it came from — that's
what the user needs to debug.
