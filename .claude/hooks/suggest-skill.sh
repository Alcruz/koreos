#!/bin/sh
# PostToolUse(Edit|Write): data-driven nudge based on what the tree says, not
# hardcoded paths. Looks at tests/unit and tests/smoke to decide whether the
# edited module has host or on-target coverage, and falls back to a run-qemu
# hint for kernel code that has neither.
#
# Exit 2 sends stderr back to Claude as non-blocking feedback.

set -u

input=$(cat)
path=$(printf '%s' "$input" | python3 -c 'import json,sys
try:
    d=json.load(sys.stdin); t=d.get("tool_input",{})
    print(t.get("file_path") or t.get("notebook_path") or "")
except Exception:
    print("")' 2>/dev/null)
[ -z "$path" ] && exit 0

root="${CLAUDE_PROJECT_DIR:-$(pwd)}"

# Only care about kernel edits and edits inside tests/ themselves.
case "$path" in
  "$root"/kernel/*|"$root"/tests/*) ;;
  *) exit 0 ;;
esac

base="${path##*/}"
stem="${base%.*}"
[ -z "$stem" ] && exit 0

# Look for any test source (unit or smoke) that either is named after the
# stem (test_<stem>.c) or references the edited file by name. This picks up
# new modules and new tests automatically — no list to maintain.
has_ref() {
  dir="$1"
  [ -d "$dir" ] || return 1
  [ -f "$dir/test_${stem}.c" ] && return 0
  grep -rlqE "(\"|/)${stem}\.(c|h)(\"|$|[[:space:]])" "$dir" 2>/dev/null && return 0
  return 1
}

unit_hit=0
smoke_hit=0
has_ref "$root/tests/unit"  && unit_hit=1
has_ref "$root/tests/smoke" && smoke_hit=1

msgs=""
[ $unit_hit  -eq 1 ] && msgs="Run \`make test\` (run-tests skill) — tests/unit covers ${stem}."
if [ $smoke_hit -eq 1 ]; then
  [ -n "$msgs" ] && msgs="$msgs "
  msgs="${msgs}Run \`make smoke\` — tests/smoke covers ${stem}."
fi

case "$path" in
  "$root"/kernel/*)
    if [ $unit_hit -eq 0 ] && [ $smoke_hit -eq 0 ]; then
      msgs="No test under tests/unit or tests/smoke references ${stem}. Boot the kernel (run-qemu skill) or add coverage before claiming done."
    fi
    ;;
esac

if [ -n "$msgs" ]; then
  echo "$msgs" >&2
  exit 2
fi
exit 0
