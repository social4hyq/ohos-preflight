#!/bin/sh
# smoke/04_bun_install_local_tarball.sh
#
# L2 application smoke: `bun install` a locally-built tarball dependency
# (no network) -- exercises the same extraction + hardlink-based
# install fast path that ohos-bun's linkat() workaround
# (ohos-compat-shim's linkat EACCES fallback, see g5/g9 probes) and
# install-time native-binary signing scan (PackageInstaller.rs, see
# k11_dlopen_unsigned_so) both sit on. A registry tarball is used
# rather than a `file:` directory dependency because the latter may
# just get symlinked, bypassing the extract+hardlink path entirely.
#
# Exit codes: 0=pass, 1=fail, 2=unsupported (bun/tar missing, or no
#             writable tmpdir)
set -u

bun=$(command -v bun 2>/dev/null)
if [ -z "$bun" ]; then
    echo "bun not found in PATH" >&2
    exit 2
fi
# `tar` is also one of toybox sh's own bundled applet names, so
# `command -v tar` returns the literal string "alias tar=tar" rather
# than a path here -- same quirk as `timeout` (see
# l0/05_brew_bun_launch.sh for the long version). `type -p tar` isn't
# clean either ("tar is a tracked alias for /path/to/tar"), so just
# test it boolean-style and invoke the bare name below.
if ! type -p tar >/dev/null 2>&1; then
    echo "tar not found in PATH" >&2
    exit 2
fi

tmpdir="${TMPDIR:-/data/storage/el2/base/tmp}"
if [ ! -d "$tmpdir" ] || [ ! -w "$tmpdir" ]; then
    echo "no writable tmpdir ($tmpdir)" >&2
    exit 2
fi

work="$tmpdir/.ohos-preflight-smoke04-$$"
mkdir -p "$work/package" "$work/consumer"

cat > "$work/package/package.json" <<'EOF'
{"name":"ohos-preflight-smoke-pkg","version":"1.0.0","main":"index.js"}
EOF
printf 'module.exports = "smoke-pkg-loaded";\n' > "$work/package/index.js"

( cd "$work" && tar czf pkg.tgz package ) >/dev/null 2>&1
if [ ! -f "$work/pkg.tgz" ]; then
    echo "failed to build local tarball" >&2
    rm -rf "$work"
    exit 2
fi

cat > "$work/consumer/package.json" <<EOF
{"name":"ohos-preflight-smoke-consumer","version":"1.0.0","dependencies":{"ohos-preflight-smoke-pkg":"file:../pkg.tgz"}}
EOF

install_out=$(cd "$work/consumer" && "$bun" install 2>&1)
install_rc=$?
if [ "$install_rc" -ne 0 ]; then
    echo "bun install failed rc=$install_rc: $install_out" >&2
    rm -rf "$work"
    exit 1
fi

verify_out=$(cd "$work/consumer" && "$bun" -e 'console.log(require("ohos-preflight-smoke-pkg"))' 2>&1)
verify_rc=$?
rm -rf "$work"

if [ "$verify_rc" -ne 0 ]; then
    echo "installed package failed to load rc=$verify_rc: $verify_out" >&2
    exit 1
fi
case "$verify_out" in
    *smoke-pkg-loaded*) exit 0 ;;
    *) echo "unexpected output: $verify_out" >&2; exit 1 ;;
esac
