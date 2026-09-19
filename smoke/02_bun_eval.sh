#!/bin/sh
# smoke/02_bun_eval.sh
#
# L2 application smoke: `bun -e` runs a trivial script through the full
# JSC + Bun runtime bring-up (not just the CLI arg parser that
# --version exercises).
#
# Exit codes: 0=pass, 1=fail, 2=unsupported (bun not installed)
set -u

bun=$(command -v bun 2>/dev/null)
if [ -z "$bun" ]; then
    echo "bun not found in PATH" >&2
    exit 2
fi

out=$("$bun" -e 'console.log(1 + 1)' 2>&1)
rc=$?
if [ "$rc" -ne 0 ]; then
    echo "bun -e failed rc=$rc: $out" >&2
    exit 1
fi
if [ "$out" != "2" ]; then
    echo "unexpected output: $out" >&2
    exit 1
fi
exit 0
