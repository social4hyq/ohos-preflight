#!/bin/sh
# probes/_env_snapshot.sh
#
# Snapshot the environment facts a devmode on/off comparison needs to be
# trustworthy: is developer mode actually on right now, what OS/kernel
# build, what NDK clang. Emits a single JSON object to stdout.
#
# Not a probe in run.sh's sense (no pass/fail verdict) -- the leading
# `_` already makes run.sh skip it, and it's invoked directly by
# scripts/snapshot.sh, mirroring how run-dual.sh calls _os_version
# directly for the same reason.
#
# Deliberately pure POSIX sh + system `param`/`uname` -- no python3, no
# brew binaries -- so it still produces a meaningful snapshot even in
# the "devmode off broke everything else" scenario this whole tool
# exists to detect.
set -u

json_escape() {
    printf '%s' "$1" | sed 's/\\/\\\\/g; s/"/\\"/g' | tr -d '\n'
}

param_get() {
    param get "$1" 2>/dev/null | sed 's/[[:space:]]*$//'
}

devmode=$(param_get const.security.developermode.state)
debuggable=$(param_get const.debuggable)
secure=$(param_get const.secure)
os_dist_name=$(param_get const.product.os.dist.name)
os_dist_version=$(param_get const.product.os.dist.version)
ohos_fullname=$(param_get const.ohos.fullname)
ohos_cert=$(param_get const.ohos.version.certified)
build_version=$(param_get const.product.software.version)
hardware=$(param_get ohos.boot.hardware)
hdc_secure=$(param_get const.hdc.secure)

kernel_release=$(uname -r 2>/dev/null)
kernel_version=$(uname -v 2>/dev/null)
machine=$(uname -m 2>/dev/null)

ndk_home=$(ls -d "$HOME"/.harmonybrew/Cellar/ohos-sdk/*/native 2>/dev/null | sort -V | tail -1)
clang_version=""
if [ -n "$ndk_home" ] && [ -x "$ndk_home/llvm/bin/clang" ]; then
    clang_version=$("$ndk_home/llvm/bin/clang" --version 2>/dev/null | head -1)
fi

timestamp=$(date -u +%Y-%m-%dT%H:%M:%SZ 2>/dev/null)

printf '{'
printf '"timestamp":"%s"' "$(json_escape "$timestamp")"
printf ',"devmode":"%s"' "$(json_escape "$devmode")"
printf ',"debuggable":"%s"' "$(json_escape "$debuggable")"
printf ',"secure":"%s"' "$(json_escape "$secure")"
printf ',"hdc_secure":"%s"' "$(json_escape "$hdc_secure")"
printf ',"os_dist_name":"%s"' "$(json_escape "$os_dist_name")"
printf ',"os_dist_version":"%s"' "$(json_escape "$os_dist_version")"
printf ',"ohos_fullname":"%s"' "$(json_escape "$ohos_fullname")"
printf ',"ohos_cert":"%s"' "$(json_escape "$ohos_cert")"
printf ',"build_version":"%s"' "$(json_escape "$build_version")"
printf ',"hardware":"%s"' "$(json_escape "$hardware")"
printf ',"kernel_release":"%s"' "$(json_escape "$kernel_release")"
printf ',"kernel_version":"%s"' "$(json_escape "$kernel_version")"
printf ',"machine":"%s"' "$(json_escape "$machine")"
printf ',"clang_version":"%s"' "$(json_escape "$clang_version")"
printf '}\n'
