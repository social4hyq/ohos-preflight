#!/bin/sh
# smoke/01_bun_version.sh
#
# L2 application smoke: does the installed bun even start and report a
# version? The cheapest possible real-world exercise of bun's own ELF
# signing/exec path (bun.rb formula's bottle, self-signed at build time
# -- a different signing pipeline than this project's raw NDK probes).
#
# Exit codes: 0=pass, 1=fail (exists but broken), 2=unsupported (not installed)
set -u

bun=$(command -v bun 2>/dev/null)
if [ -z "$bun" ]; then
    echo "bun not found in PATH" >&2
    exit 2
fi

out=$("$bun" --version 2>&1)
rc=$?
if [ "$rc" -ne 0 ]; then
    echo "bun --version failed rc=$rc: $out" >&2
    exit 1
fi
case "$out" in
    *[0-9]*) exit 0 ;;
    *) echo "unexpected output: $out" >&2; exit 1 ;;
esac
