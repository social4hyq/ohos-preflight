#!/bin/sh
# smoke/12_git_version.sh
#
# L2 application smoke: git is a plain dynamically-linked bottled
# binary with no OHOS-specific patching -- a useful baseline "is the
# generic brew-bottle exec path fine at all" control alongside the
# more OHOS-specific checks above.
#
# Exit codes: 0=pass, 1=fail, 2=unsupported (not installed)
set -u

git=$(command -v git 2>/dev/null)
if [ -z "$git" ]; then
    echo "git not found in PATH" >&2
    exit 2
fi

out=$("$git" --version 2>&1)
rc=$?
if [ "$rc" -ne 0 ]; then
    echo "git --version failed rc=$rc: $out" >&2
    exit 1
fi
case "$out" in
    *[0-9]*) exit 0 ;;
    *) echo "unexpected output: $out" >&2; exit 1 ;;
esac
