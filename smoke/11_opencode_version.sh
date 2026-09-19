#!/bin/sh
# smoke/11_opencode_version.sh
#
# L2 application smoke: opencode is a bun-compiled single-binary agent
# (formula opencode.rb, built via the ohos-opencode adaptation tree) --
# exercises the same standalone-binary exec path as smoke/07, but on a
# real production artifact instead of a throwaway compile.
#
# Exit codes: 0=pass, 1=fail, 2=unsupported (not installed)
set -u

opencode=$(command -v opencode 2>/dev/null)
if [ -z "$opencode" ]; then
    echo "opencode not found in PATH" >&2
    exit 2
fi

out=$("$opencode" --version 2>&1)
rc=$?
if [ "$rc" -ne 0 ]; then
    echo "opencode --version failed rc=$rc: $out" >&2
    exit 1
fi
case "$out" in
    *[0-9]*) exit 0 ;;
    *) echo "unexpected output: $out" >&2; exit 1 ;;
esac
