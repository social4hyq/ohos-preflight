#!/bin/sh
# probes/f1_proc_maps.sh
#
# Capability: /proc/self/maps is readable and returns at least one line.
#
# Exit codes:
#   0 = read >=1 line
#   1 = empty / unreadable
#   2 = /proc absent (precondition)
set -u

if [ ! -d /proc ]; then
    echo "/proc absent" >&2
    exit 2
fi
if [ ! -r /proc/self/maps ]; then
    echo "/proc/self/maps unreadable" >&2
    exit 1
fi
line=$(head -1 /proc/self/maps 2>/dev/null)
if [ -z "$line" ]; then
    echo "/proc/self/maps empty" >&2
    exit 1
fi
exit 0
