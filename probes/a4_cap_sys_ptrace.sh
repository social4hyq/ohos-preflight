#!/bin/sh
# probes/a4_cap_sys_ptrace.sh
#
# Capability: CAP_SYS_PTRACE bit set in this process's effective capabilities.
# Reads /proc/self/status, finds the CapEff hex line, and tests bit 0x80000
# (CAP_SYS_PTRACE = 19).
#
# Exit codes:
#   0 = CAP_SYS_PTRACE present
#   1 = capability absent
#   2 = /proc/self/status unreadable / unparseable
set -u

if [ ! -r /proc/self/status ]; then
    echo "/proc/self/status unreadable" >&2
    exit 2
fi

cap_eff=$(awk '/^CapEff:/ { print $2 }' /proc/self/status)
if [ -z "$cap_eff" ]; then
    echo "CapEff line missing" >&2
    exit 2
fi

# busybox sh lacks $(()) hex; pipe through printf + awk for portability.
val=$(printf '%d' "0x$cap_eff" 2>/dev/null || echo "")
if [ -z "$val" ]; then
    echo "could not parse CapEff=$cap_eff" >&2
    exit 2
fi

# CAP_SYS_PTRACE = bit 19 = 0x80000 = 524288
mask=524288
if [ "$((val & mask))" -ne 0 ]; then
    exit 0
fi
echo "CAP_SYS_PTRACE not set (CapEff=$cap_eff)" >&2
exit 1
