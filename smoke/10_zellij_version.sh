#!/bin/sh
# smoke/10_zellij_version.sh
#
# L2 application smoke: zellij exercises its own LD_PRELOAD
# ohos-compat-shim wrapper (PR #210, project_zellij_ohos_ld_preload_fix.md)
# to work around intermittent startup crashes -- a --version run is
# cheap and still goes through that wrapper.
#
# Exit codes: 0=pass, 1=fail, 2=unsupported (not installed)
set -u

zellij=$(command -v zellij 2>/dev/null)
if [ -z "$zellij" ]; then
    echo "zellij not found in PATH" >&2
    exit 2
fi

out=$("$zellij" --version 2>&1)
rc=$?
if [ "$rc" -ne 0 ]; then
    echo "zellij --version failed rc=$rc: $out" >&2
    exit 1
fi
case "$out" in
    *[0-9]*) exit 0 ;;
    *) echo "unexpected output: $out" >&2; exit 1 ;;
esac
