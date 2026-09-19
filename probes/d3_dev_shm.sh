#!/bin/sh
# probes/d3_dev_shm.sh
#
# Capability: /dev/shm is writable. Many runtimes assume this for IPC
# tmpfiles.
#
# Exit codes:
#   0 = create + remove succeeded
#   1 = touch/rm failed
#   2 = /dev/shm absent (precondition)
set -u

if [ ! -d /dev/shm ]; then
    echo "/dev/shm absent" >&2
    exit 2
fi

probe="/dev/shm/.harmonybrew-probe-$$"
if : > "$probe" 2>/dev/null && rm -f "$probe" 2>/dev/null; then
    exit 0
fi
echo "/dev/shm not writable" >&2
rm -f "$probe" 2>/dev/null
exit 1
