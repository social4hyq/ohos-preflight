#!/bin/sh
# scripts/ohos-preflight/run.sh
#
# Discover every executable probe under probes/ and emit one JSON object per
# probe to stdout. Output is line-delimited JSON; compare.py consumes it.
#
#   {"probe":"a1_seccomp_unotify","track":"<TRACK>","status":"pass|fail|unsupported","reason":"..."}
#
# Env:
#   TRACK   tag for the run; typically "openharmony" or "harmonyos".
#
# Exit codes from the probes:
#   0 = pass, 1 = fail (capability unavailable), 2 = unsupported (precondition unmet).
#
# Missing-binary handling: silently skip non-executable files (a .c source
# with no compiled output, a cargo subdir, etc.). The Makefile decides what
# gets built; run.sh just reports what's runnable.

set -u
TRACK="${TRACK:-unknown}"
ERRFILE="$(mktemp -t probe.err.XXXXXX)"
trap 'rm -f "$ERRFILE"' EXIT

for probe in probes/*; do
    # Skip directories (b4_naked_asm/, c2_sys_execveat/) and non-executables.
    [ -f "$probe" ] || continue
    [ -x "$probe" ] || continue

    # Strip .sh/.c/.rs extension if present so the probe id is canonical.
    name=$(basename "$probe" | sed -E 's/\.(sh|c|rs)$//')

    # Hook .so artifacts and helper binaries are not probes; filter by file shape.
    case "$name" in
        libb1hook*) continue ;;
        _*)         continue ;;
    esac

    if "./$probe" 2>"$ERRFILE" >/dev/null; then
        status="pass"
        reason=""
    else
        ec=$?
        if [ "$ec" = "2" ]; then
            status="unsupported"
        else
            status="fail"
        fi
        # First line of stderr -> reason. Escape inner double-quotes.
        reason=$(head -1 "$ERRFILE" 2>/dev/null | sed 's/"/\\"/g' | tr -d '\n')
    fi

    printf '{"probe":"%s","track":"%s","status":"%s","reason":"%s"}\n' \
        "$name" "$TRACK" "$status" "$reason"
done
