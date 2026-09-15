#!/bin/bash
# Isolated smoke-test harness. For each tests/smoke/test_*.c, build a kernel
# image whose entry is that test, boot it under QEMU, capture serial, and grep
# for the canonical PASS/FAIL marker. Each test runs in its own fresh VM so a
# panic or hang in one cannot mask another.
#
# Tests shut QEMU down via PSCI SYSTEM_OFF once they've printed their marker,
# so a well-behaved run exits within milliseconds. The `timeout` below is a
# safety net for hangs.

set -u

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

QEMU_CMD="qemu-system-aarch64"
TIMEOUT_SEC=10

if ! command -v "$QEMU_CMD" &>/dev/null; then
    echo "Error: $QEMU_CMD not found. Install QEMU for ARM64." >&2
    exit 2
fi

# Portable bounded run: QEMU exits itself via PSCI once the test prints its
# marker, so the timeout is only a hang safety net. `timeout(1)` isn't on
# macOS by default; fall back to a shell watcher.
run_bounded() {
    local secs=$1; shift
    "$@" &
    local pid=$!
    local i=0
    while [ $i -lt $((secs * 10)) ]; do
        kill -0 "$pid" 2>/dev/null || break
        sleep 0.1
        i=$((i + 1))
    done
    if kill -0 "$pid" 2>/dev/null; then
        kill -9 "$pid" 2>/dev/null
        wait "$pid" 2>/dev/null
        return 124
    fi
    wait "$pid" 2>/dev/null
    return $?
}

tests=()
for f in tests/smoke/test_*.c; do
    [ -f "$f" ] || continue
    name="${f##*/test_}"
    name="${name%.c}"
    tests+=("$name")
done

if [ "${#tests[@]}" -eq 0 ]; then
    echo "Error: no tests found under tests/smoke/test_*.c" >&2
    exit 2
fi

pass=0
fail=0
timeout_ct=0
failed_names=()

for name in "${tests[@]}"; do
    bin="build/smoke/kernel-$name.bin"
    log="build/smoke/$name.log"

    printf 'building %-16s ... ' "$name"
    if command -v aarch64-linux-gnu-gcc >/dev/null 2>&1; then
        if ! (cd kernel && make -s "SMOKE=$name") >/dev/null; then
            echo "BUILD FAILED"
            fail=$((fail + 1))
            failed_names+=("$name (build)")
            continue
        fi
        echo "ok"
    elif [ -f "$bin" ]; then
        echo "skipped (prebuilt)"
    else
        echo "BUILD SKIPPED (no cross-gcc, no prebuilt $bin)"
        echo "  hint: run 'make smoke' inside the toolchain container, or prebuild binaries." >&2
        fail=$((fail + 1))
        failed_names+=("$name (no toolchain)")
        continue
    fi

    mkdir -p build/smoke
    rm -f "$log"

    printf 'running  %-16s ... ' "$name"
    run_bounded "$TIMEOUT_SEC" "$QEMU_CMD" \
        -no-user-config -nodefaults \
        -machine virt -cpu cortex-a57 -m 512M \
        -kernel "$bin" \
        -serial "file:$log" \
        -display none >/dev/null 2>&1
    rc=$?

    marker_pass="SMOKE: $name PASS"
    marker_fail="SMOKE: $name FAIL"

    if [ -f "$log" ] && grep -q "$marker_pass" "$log"; then
        echo "[PASS]"
        pass=$((pass + 1))
    elif [ -f "$log" ] && grep -q "$marker_fail" "$log"; then
        echo "[FAIL]"
        fail=$((fail + 1))
        failed_names+=("$name")
        echo "----- $log (tail) -----"
        tail -n 20 "$log" | sed 's/^/  /'
        echo "-----------------------"
    elif [ $rc -eq 124 ]; then
        echo "[TIMEOUT]"
        timeout_ct=$((timeout_ct + 1))
        failed_names+=("$name (timeout)")
        if [ -f "$log" ]; then
            echo "----- $log (tail) -----"
            tail -n 20 "$log" | sed 's/^/  /'
            echo "-----------------------"
        fi
    else
        echo "[NO MARKER]"
        fail=$((fail + 1))
        failed_names+=("$name (no marker)")
        if [ -f "$log" ]; then
            echo "----- $log (tail) -----"
            tail -n 20 "$log" | sed 's/^/  /'
            echo "-----------------------"
        fi
    fi
done

echo
echo "smoke: $pass passed, $fail failed, $timeout_ct timed out"
if [ "$fail" -gt 0 ] || [ "$timeout_ct" -gt 0 ]; then
    echo "failed: ${failed_names[*]}"
    exit 1
fi
exit 0
