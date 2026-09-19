#!/bin/sh
# smoke/09_node_os_userinfo.sh
#
# L2 application smoke: plain `node -e` (not bun-spawned -- compare
# against smoke/06) calling os.userInfo(). This is the direct target of
# the ~/.harmonybrew/bin/node LD_PRELOAD wrapper mentioned in Workspace
# memory (environment_claude_wrapper_ld_preload_source.md /
# project_ohos_bun_node_userinfo_fix.md): it fixes the getpwuid_r ENOENT
# that os.userInfo() otherwise hits in the sandbox.
#
# Exit codes: 0=pass, 1=fail, 2=unsupported (node not installed)
set -u

node=$(command -v node 2>/dev/null)
if [ -z "$node" ]; then
    echo "node not found in PATH" >&2
    exit 2
fi

out=$("$node" -e 'console.log(require("os").userInfo().username)' 2>&1)
rc=$?

if [ "$rc" -ne 0 ]; then
    echo "node os.userInfo() failed rc=$rc: $out" >&2
    exit 1
fi
if [ -z "$out" ]; then
    echo "os.userInfo().username came back empty" >&2
    exit 1
fi
exit 0
