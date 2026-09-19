#!/bin/sh
# scripts/snapshot.sh
#
# Single-machine capability snapshot: L0 survival checks (l0/*.sh) -> L1
# capability probes (probes/*.c via run.sh, plus ohos-shim check --json)
# -> L2 application smoke (smoke/*.sh). Everything lands under
# snapshots/<label>/ so two runs -- typically "developer mode on" and
# "developer mode off" -- can be diffed later with compare-snapshots.py.
#
# Why a snapshot-to-disk design instead of "just run both and diff
# inline": toggling developer mode is a manual settings-app action that
# very likely requires a reboot, so the two runs are necessarily
# separate invocations, possibly hours apart or on a different day.
#
# Usage:
#   ./scripts/snapshot.sh                        # auto-labeled full run
#   ./scripts/snapshot.sh --label my-run          # explicit label
#   ./scripts/snapshot.sh --l0-only               # skip L1/L2 entirely
#   ./scripts/snapshot.sh --skip-l2               # L0 + L1 (+ shim check), no app smoke
#
# Deliberately does NOT call python3 anywhere in this script -- see
# README.md's "开发者模式对比" section for why: python3 is itself a
# self-signed brew binary, and if developer mode turns out to gate
# self-signed exec, this collector must still produce a readable
# snapshot without it. JSON is assembled with printf; parsing/diffing
# (which needs python3) is a separate, later step in
# compare-snapshots.py.
set -u

PROJECT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$PROJECT_DIR" || exit 1

LABEL=""
L0_ONLY=0
SKIP_L2=0
L0_TIMEOUT="${L0_TIMEOUT:-15}"
L2_TIMEOUT="${L2_TIMEOUT:-120}"
SHIM_ROUNDS="${SHIM_ROUNDS:-20}"

while [ $# -gt 0 ]; do
    case "$1" in
        --label) LABEL="$2"; shift 2 ;;
        --label=*) LABEL="${1#--label=}"; shift ;;
        --l0-only) L0_ONLY=1; shift ;;
        --skip-l2) SKIP_L2=1; shift ;;
        *)
            echo "usage: $0 [--label NAME] [--l0-only] [--skip-l2]" >&2
            exit 64
            ;;
    esac
done

# `command -v timeout` returns "alias timeout=timeout" (a literal string,
# not a path) on this device's toybox sh for its own bundled applets --
# see l0/05_brew_bun_launch.sh for the long version. `type -p` is the
# reliable test.
HAVE_TIMEOUT=0
if type -p timeout >/dev/null 2>&1; then
    HAVE_TIMEOUT=1
fi

run_with_timeout() {
    # run_with_timeout SECONDS -- CMD...
    secs="$1"; shift
    if [ "$HAVE_TIMEOUT" -eq 1 ]; then
        timeout "$secs" "$@"
    else
        "$@"
    fi
}

# Collect env.json first (needed to auto-derive the label).
ENV_JSON=$(./probes/_env_snapshot.sh)
devmode_raw=$(printf '%s' "$ENV_JSON" | sed -n 's/.*"devmode":"\([^"]*\)".*/\1/p')
os_ver=$(printf '%s' "$ENV_JSON" | sed -n 's/.*"os_dist_version":"\([^"]*\)".*/\1/p')

case "$devmode_raw" in
    true) devmode_tag="on" ;;
    false) devmode_tag="off" ;;
    *) devmode_tag="unknown" ;;
esac

if [ -z "$LABEL" ]; then
    ts=$(date -u +%Y%m%d-%H%M%S 2>/dev/null)
    ver_tag=$(printf '%s' "${os_ver:-unknown}" | tr -c 'A-Za-z0-9._-' '-')
    LABEL="hm-${ver_tag}-devmode-${devmode_tag}-${ts}"
fi

OUT_DIR="$PROJECT_DIR/snapshots/$LABEL"
mkdir -p "$OUT_DIR"
printf '%s\n' "$ENV_JSON" > "$OUT_DIR/env.json"

echo "=== snapshot $LABEL (devmode=$devmode_tag) ===" >&2

# ---------------------------------------------------------------------
# Generic runner: iterate *.sh probes in a directory, emit JSONL lines
# shaped like run.sh's ({"probe":...,"track":...,"status":...,"reason":...}),
# so compare-snapshots.py and the existing analyze.py/html.py/markdown.py
# tooling can all consume the same shape.
# ---------------------------------------------------------------------
run_probe_dir() {
    dir="$1"; outfile="$2"; timeout_secs="$3"
    errfile="${TMPDIR:-/data/storage/el2/base/tmp}/.ohos-preflight-snapshot-err-$$"
    : > "$outfile"
    for probe in "$dir"/*.sh; do
        [ -f "$probe" ] || continue
        [ -x "$probe" ] || continue
        name=$(basename "$probe" .sh)
        run_with_timeout "$timeout_secs" "$probe" >/dev/null 2>"$errfile"
        ec=$?
        if [ "$ec" -eq 0 ]; then
            status="pass"; reason=""
        elif [ "$ec" -eq 124 ] || [ "$ec" -eq 137 ]; then
            status="fail"; reason="timed out after ${timeout_secs}s"
        elif [ "$ec" -eq 2 ]; then
            status="unsupported"; reason=$(head -1 "$errfile" 2>/dev/null | sed 's/"/\\"/g' | tr -d '\n')
        else
            status="fail"; reason=$(head -1 "$errfile" 2>/dev/null | sed 's/"/\\"/g' | tr -d '\n')
        fi
        printf '{"probe":"%s","track":"%s","status":"%s","reason":"%s"}\n' \
            "$name" "$LABEL" "$status" "$reason" >> "$outfile"
    done
    rm -f "$errfile"
}

run_probe_dir "$PROJECT_DIR/l0" "$OUT_DIR/l0.jsonl" "$L0_TIMEOUT"
l0_total=$(wc -l < "$OUT_DIR/l0.jsonl" | tr -d ' ')
l0_fail=$(grep -c '"status":"fail"' "$OUT_DIR/l0.jsonl")
echo "L0: $l0_total checks, $l0_fail failed" >&2

# Blocking gate: if the kernel refuses to exec even the raw, unsigned,
# NDK-built probe binaries, every L1 probe and every L2 smoke script is
# unusable -- both are exactly that kind of binary. Record it as data
# (status "blocked" everywhere) rather than aborting with a bare error,
# so a fully-blocked run is still a valid, diffable snapshot.
BLOCKED=0
BLOCK_REASON=""
if ! grep -q '"probe":"03_raw_elf_exec","track":"[^"]*","status":"pass"' "$OUT_DIR/l0.jsonl"; then
    BLOCKED=1
    BLOCK_REASON="l0/03_raw_elf_exec did not pass -- raw NDK-built ELFs cannot exec, so L1 probes (identical binary shape) and L2 smoke (brew binaries) are assumed unusable"
fi

if [ "$L0_ONLY" -eq 1 ]; then
    : > "$OUT_DIR/l1.jsonl"
    : > "$OUT_DIR/l2.jsonl"
    echo "{}" > "$OUT_DIR/shim-check.json"
    echo "--l0-only: skipping L1/L2/shim-check" >&2
elif [ "$BLOCKED" -eq 1 ]; then
    echo "BLOCKED: $BLOCK_REASON" >&2
    : > "$OUT_DIR/l1.jsonl"
    : > "$OUT_DIR/l2.jsonl"
    echo "{}" > "$OUT_DIR/shim-check.json"
    escaped_reason=$(printf '%s' "$BLOCK_REASON" | sed 's/"/\\"/g')
    # Emit an explicit "blocked" row per probe/smoke script that would
    # have run, rather than leaving them absent. compare-snapshots.py
    # treats "blocked" (a higher-level gate stopped this from running)
    # differently from "untestable" (it ran but its precondition wasn't
    # met) -- collapsing them would misreport a devmode-off wipeout as a
    # pile of inconclusive results instead of the blocking event it is.
    for probe in "$PROJECT_DIR"/probes/*.c; do
        [ -f "$probe" ] || continue
        name=$(basename "$probe" .c)
        printf '{"probe":"%s","track":"%s","status":"blocked","reason":"%s"}\n' \
            "$name" "$LABEL" "$escaped_reason" >> "$OUT_DIR/l1.jsonl"
    done
    for probe in "$PROJECT_DIR"/smoke/*.sh; do
        [ -f "$probe" ] || continue
        name=$(basename "$probe" .sh)
        printf '{"probe":"%s","track":"%s","status":"blocked","reason":"%s"}\n' \
            "$name" "$LABEL" "$escaped_reason" >> "$OUT_DIR/l2.jsonl"
    done
else
    echo "[L1] building probes..." >&2
    if make -C "$PROJECT_DIR" >"$OUT_DIR/.make.log" 2>&1; then
        echo "[L1] running run.sh..." >&2
        (cd "$PROJECT_DIR" && TRACK="$LABEL" ./run.sh) > "$OUT_DIR/.l1.raw" 2>>"$OUT_DIR/.make.log"
        grep '{"probe"' "$OUT_DIR/.l1.raw" > "$OUT_DIR/l1.jsonl"
        rm -f "$OUT_DIR/.l1.raw"
        l1_total=$(wc -l < "$OUT_DIR/l1.jsonl" | tr -d ' ')
        echo "L1: $l1_total probes" >&2
    else
        echo "[L1] make failed, see $OUT_DIR/.make.log -- treating as blocked" >&2
        BLOCKED=1
        BLOCK_REASON="make failed to build L1 probes, see .make.log"
        escaped_reason=$(printf '%s' "$BLOCK_REASON" | sed 's/"/\\"/g')
        : > "$OUT_DIR/l1.jsonl"
        for probe in "$PROJECT_DIR"/probes/*.c; do
            [ -f "$probe" ] || continue
            name=$(basename "$probe" .c)
            printf '{"probe":"%s","track":"%s","status":"blocked","reason":"%s"}\n' \
                "$name" "$LABEL" "$escaped_reason" >> "$OUT_DIR/l1.jsonl"
        done
    fi

    if command -v ohos-shim >/dev/null 2>&1; then
        run_with_timeout 60 ohos-shim check --json --rounds "$SHIM_ROUNDS" > "$OUT_DIR/shim-check.json" 2>"$OUT_DIR/.shim-check.err"
        if [ ! -s "$OUT_DIR/shim-check.json" ]; then
            echo "{}" > "$OUT_DIR/shim-check.json"
        fi
        rm -f "$OUT_DIR/.shim-check.err"
    else
        echo "ohos-shim not found in PATH, skipping shim-check.json" >&2
        echo "{}" > "$OUT_DIR/shim-check.json"
    fi

    if [ "$SKIP_L2" -eq 1 ]; then
        : > "$OUT_DIR/l2.jsonl"
        echo "--skip-l2: skipping application smoke" >&2
    else
        echo "[L2] running application smoke (this can take a few minutes)..." >&2
        run_probe_dir "$PROJECT_DIR/smoke" "$OUT_DIR/l2.jsonl" "$L2_TIMEOUT"
        l2_total=$(wc -l < "$OUT_DIR/l2.jsonl" | tr -d ' ')
        l2_fail=$(grep -c '"status":"fail"' "$OUT_DIR/l2.jsonl")
        echo "L2: $l2_total checks, $l2_fail failed" >&2
    fi
fi

end_ts=$(date -u +%Y-%m-%dT%H:%M:%SZ 2>/dev/null)
printf '{"label":"%s","devmode":"%s","blocked":%s,"block_reason":"%s","completed_at":"%s"}\n' \
    "$LABEL" "$devmode_raw" \
    "$([ "$BLOCKED" -eq 1 ] && echo true || echo false)" \
    "$(printf '%s' "$BLOCK_REASON" | sed 's/"/\\"/g')" \
    "$end_ts" > "$OUT_DIR/meta.json"

echo "=== done: $OUT_DIR ===" >&2
