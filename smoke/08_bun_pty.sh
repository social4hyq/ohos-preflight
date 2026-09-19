#!/bin/sh
# smoke/08_bun_pty.sh
#
# L2 application smoke: can @ohos-ports/bun-pty (../Software/ohos-bun-pty)
# actually open a pty and spawn a shell in it? Best-effort: this package
# is not always globally installed on every machine that runs
# snapshot.sh, so an absent install is "unsupported", not "fail".
#
# Exit codes: 0=pass, 1=fail, 2=unsupported (bun missing, package not found)
set -u

bun=$(command -v bun 2>/dev/null)
if [ -z "$bun" ]; then
    echo "bun not found in PATH" >&2
    exit 2
fi

resolve_script='
let mod;
try {
    mod = require("@ohos-ports/bun-pty");
} catch {
    try { mod = require("bun-pty"); } catch { mod = null; }
}
if (!mod) {
    console.error("UNSUPPORTED: @ohos-ports/bun-pty not resolvable from " + process.cwd());
    process.exit(2);
}
const { spawn } = mod;
const p = spawn("/bin/sh", ["-c", "echo pty-ok"], { name: "xterm", cols: 80, rows: 24 });
let out = "";
p.onData((d) => { out += d; });
p.onExit(() => {
    if (out.includes("pty-ok")) {
        console.log("pty-ok-confirmed");
        process.exit(0);
    }
    console.error("pty spawned but output missing: " + JSON.stringify(out));
    process.exit(1);
});
setTimeout(() => { console.error("pty smoke test timed out"); process.exit(1); }, 8000);
'

out=$("$bun" -e "$resolve_script" 2>&1)
rc=$?

case "$out" in
    *UNSUPPORTED:*)
        echo "$out" | head -1 >&2
        exit 2
        ;;
esac
if [ "$rc" -ne 0 ]; then
    echo "bun-pty smoke failed rc=$rc: $out" >&2
    exit 1
fi
case "$out" in
    *pty-ok-confirmed*) exit 0 ;;
    *) echo "unexpected output: $out" >&2; exit 1 ;;
esac
