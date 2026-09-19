#!/bin/sh
# smoke/06_bun_spawn_node_userinfo.sh
#
# L2 application smoke: regression check for the bun-spawned-node
# os.userInfo() ENOENT bug (Workspace memory:
# project_ohos_bun_node_userinfo_fix.md) -- fixed in bun r49/r50, and
# the current r56 is well past that, but this is exactly the kind of
# thing a devmode-off run could plausibly reintroduce (it depends on
# ohos_node_userinfo.rs's NODE_OPTIONS preload injection working, which
# in turn depends on exec/signing behavior this whole snapshot tool
# exists to interrogate).
#
# Exit codes: 0=pass, 1=fail, 2=unsupported (bun/node missing)
set -u

bun=$(command -v bun 2>/dev/null)
if [ -z "$bun" ]; then
    echo "bun not found in PATH" >&2
    exit 2
fi
if ! command -v node >/dev/null 2>&1; then
    echo "node not found in PATH" >&2
    exit 2
fi

out=$("$bun" -e '
const proc = Bun.spawnSync(["node", "-e", "console.log(require(\"os\").userInfo().username)"]);
if (proc.exitCode !== 0) {
    console.error("node child exited " + proc.exitCode + ": " + proc.stderr.toString());
    process.exit(1);
}
const username = proc.stdout.toString().trim();
if (!username) {
    console.error("os.userInfo().username came back empty");
    process.exit(1);
}
console.log("userinfo-ok:" + username);
' 2>&1)
rc=$?

if [ "$rc" -ne 0 ]; then
    echo "bun-spawned node os.userInfo() regression: $out" >&2
    exit 1
fi
case "$out" in
    *userinfo-ok:*) exit 0 ;;
    *) echo "unexpected output: $out" >&2; exit 1 ;;
esac
