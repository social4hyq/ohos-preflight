#!/bin/sh
# smoke/07_bun_build_compile.sh
#
# L2 application smoke: `bun build --compile` a trivial script into a
# standalone ELF, then exec the result -- exercises
# StandaloneModuleGraph's .bun-section embedding and (on a fresh binary
# with no prior signature) the exec-time signing path, end to end. This
# is the single most representative smoke test for "can this device
# still produce and run bun-compiled binaries at all".
#
# Exit codes: 0=pass, 1=fail, 2=unsupported (bun missing, or no
#             writable tmpdir)
set -u

bun=$(command -v bun 2>/dev/null)
if [ -z "$bun" ]; then
    echo "bun not found in PATH" >&2
    exit 2
fi

tmpdir="${TMPDIR:-/data/storage/el2/base/tmp}"
if [ ! -d "$tmpdir" ] || [ ! -w "$tmpdir" ]; then
    echo "no writable tmpdir ($tmpdir)" >&2
    exit 2
fi

src="$tmpdir/.ohos-preflight-smoke07-$$.js"
bin="$tmpdir/.ohos-preflight-smoke07-$$.bin"
printf 'console.log("compile-run-ok");\n' > "$src"

build_out=$("$bun" build --compile "$src" --outfile "$bin" 2>&1)
build_rc=$?
rm -f "$src"

if [ "$build_rc" -ne 0 ]; then
    echo "bun build --compile failed rc=$build_rc: $build_out" >&2
    rm -f "$bin"
    exit 1
fi
if [ ! -x "$bin" ]; then
    echo "bun build --compile produced no executable output" >&2
    exit 1
fi

run_out=$("$bin" 2>&1)
run_rc=$?
rm -f "$bin"

if [ "$run_rc" -ne 0 ]; then
    echo "exec of bun-compiled binary failed rc=$run_rc: $run_out" >&2
    exit 1
fi
case "$run_out" in
    *compile-run-ok*) exit 0 ;;
    *) echo "unexpected output: $run_out" >&2; exit 1 ;;
esac
