#!/bin/sh
# smoke/05_bun_spawn_cwd.sh
#
# L2 application smoke: regression check for the Bun.spawn({cwd})
# getcwd() corruption bug (Workspace memory:
# project_ohos_bun_spawn_cwd_getcwd_bug.md) -- fixed upstream at
# e39db04d6 and verified on real hardware, but a snapshot taken with an
# older bun build, or a devmode-off run that somehow changes sandbox
# chdir+exec behavior, should still be able to catch a regression here.
#
# Exit codes: 0=pass, 1=fail, 2=unsupported (bun missing, or no
#             writable tmpdir)
set -u

bun=$(command -v bun 2>/dev/null)
if [ -z "$bun" ]; then
    echo "bun not found in PATH" >&2
    exit 2
fi

tmpdir="${TMPDIR:-/data/storage/el2/base/tmp}"
if [ ! -d "$tmpdir" ] || [ ! -w "$tmpdir" ]; then
    echo "no writable tmpdir ($tmpdir)" >&2
    exit 2
fi

target_dir="$tmpdir/.ohos-preflight-smoke05-$$"
mkdir -p "$target_dir"
script="$target_dir.js"

cat > "$script" <<EOF
const targetDir = "$target_dir";
const proc = Bun.spawnSync(["sh", "-c", "pwd"], { cwd: targetDir });
const out = proc.stdout.toString().trim();
if (out !== targetDir) {
    console.error("child pwd '" + out + "' does not match cwd '" + targetDir + "'");
    process.exit(1);
}
console.log("cwd-ok");
EOF

out=$("$bun" run "$script" 2>&1)
rc=$?
rm -f "$script"
rmdir "$target_dir" 2>/dev/null

if [ "$rc" -ne 0 ]; then
    echo "Bun.spawnSync({cwd}) regression: $out" >&2
    exit 1
fi
case "$out" in
    *cwd-ok*) exit 0 ;;
    *) echo "unexpected output: $out" >&2; exit 1 ;;
esac
