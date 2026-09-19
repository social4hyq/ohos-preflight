#!/bin/sh
# smoke/03_bun_run_shebang.sh
#
# L2 application smoke: a `#!/usr/bin/env bun` script, chmod +x'd and
# exec'd directly (./script, not `bun run script`) -- this is exactly
# the shape of an npm package's `bin` field entry after `bun install`
# hardlinks it into node_modules/.bin/. Exercises the same kernel
# binfmt_script path as l0/02 and k2, but through the real bun binary
# instead of /bin/sh.
#
# Exit codes: 0=pass, 1=fail, 2=unsupported (bun not installed, or no
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

script="$tmpdir/.ohos-preflight-smoke03-$$.js"
printf '#!/usr/bin/env bun\nconsole.log("shebang-run-ok");\n' > "$script"
chmod +x "$script"

out=$("$script" 2>&1)
rc=$?
rm -f "$script"

if [ "$rc" -ne 0 ]; then
    echo "direct exec of bun shebang script failed rc=$rc: $out" >&2
    exit 1
fi
case "$out" in
    *shebang-run-ok*) exit 0 ;;
    *) echo "unexpected output: $out" >&2; exit 1 ;;
esac
