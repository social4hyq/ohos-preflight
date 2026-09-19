#!/bin/sh
# probes/f2_proc_self_exe.sh
#
# Capability: /proc/self/exe resolves to a real path on disk.
#
# Exit codes:
#   0 = readlink returns an existing path
#   1 = path empty or stale
#   2 = /proc/self/exe missing (precondition)
set -u

if [ ! -e /proc/self/exe ]; then
    echo "/proc/self/exe missing" >&2
    exit 2
fi
target=$(readlink /proc/self/exe 2>/dev/null)
if [ -z "$target" ]; then
    echo "readlink returned empty" >&2
    exit 1
fi
if [ ! -e "$target" ]; then
    echo "target $target does not exist" >&2
    exit 1
fi
exit 0
