#!/usr/bin/env node
// scripts/commercial-device-koffi-check.js
//
// 【用途】独立于 bun/opencode，直接验证 koffi（N-API 版 FFI）在当前设备上
// 是否可用——不依赖任何 JIT 相关环境变量，是"这台设备真实内核策略"的
// 第一手证据，而不是 devmode=on 机器上用 BUN_JSC_useJIT=0 模拟出的症状。
//
// 【背景】atomgit issue social4hyq/homebrew-core#1：opencode TUI 在商用
// HarmonyOS 设备上因 bun:ffi 需要 JIT（匿名 RWX mmap 被内核拒绝）而崩溃。
// 本脚本验证候选修法方向——把 opentui-core 的 bun:ffi 加载层换成 koffi——
// 在真实商用机上是否站得住脚。用 plain node 跑（不是 bun），koffi 是
// N-API addon，其调用/回调 trampoline 是构建期就已签名的 file-backed
// 静态代码池，不需要运行时匿名可执行内存，理论上不受商用机 JIT 限制影响。
//
// 【用法】
//   brew install deepseek-harness   # 本 tap/Harmonybrew 官方 core 均可，
//                                    # 附带已编译好、已签名的 koffi
//   node commercial-device-koffi-check.js
//
// 输出 JSON 到 stdout；exit 0=三项全过，1=任一项失败（附 reason）。

"use strict";

const fs = require("fs");
const os = require("os");
const path = require("path");

function findKoffi() {
  const envOverride = process.env.KOFFI_PATH;
  if (envOverride) return envOverride;

  const home = os.homedir();
  const roots = [
    path.join(home, ".harmonybrew/Cellar/deepseek-harness"),
  ];
  for (const root of roots) {
    if (!fs.existsSync(root)) continue;
    for (const version of fs.readdirSync(root)) {
      const candidate = path.join(
        root, version,
        "libexec/lib/node_modules/@deepseek-ai/dsh/node_modules/koffi"
      );
      if (fs.existsSync(candidate)) return candidate;
    }
  }
  return null;
}

function countAnonExecMaps() {
  try {
    const maps = fs.readFileSync("/proc/self/maps", "utf8").split("\n");
    return maps.filter((l) => {
      const parts = l.trim().split(/\s+/);
      const perms = parts[1] || "";
      const pathname = parts[5] || "";
      return perms[2] === "x" && (pathname === "" || pathname.startsWith("[anon"));
    }).length;
  } catch (e) {
    return -1; // /proc/self/maps unreadable -- not fatal, just can't audit
  }
}

function main() {
  const result = { checks: {}, ok: true };

  // Baseline BEFORE touching koffi: the host runtime's own JIT (V8 under
  // node, JSC under bun) may already hold an anon-exec region at process
  // start -- that would be the runtime's engine, not koffi, and must not
  // be misattributed. If the runtime itself couldn't start on a device
  // that truly blocks anonymous RWX, this script would never get this far
  // in the first place, which is itself informative (note it if asked).
  result.runtime = typeof process.versions.bun === "string" ? `bun ${process.versions.bun}` : `node ${process.versions.node}`;
  result.anon_exec_mmap_regions_baseline = countAnonExecMaps();

  const koffiPath = findKoffi();
  if (!koffiPath) {
    result.ok = false;
    result.error = "koffi not found -- run `brew install deepseek-harness` first, or set KOFFI_PATH";
    console.log(JSON.stringify(result, null, 2));
    process.exit(1);
  }
  result.koffi_path = koffiPath;

  let koffi;
  try {
    koffi = require(koffiPath);
  } catch (e) {
    result.ok = false;
    result.error = `require(koffi) failed: ${e.message}`;
    console.log(JSON.stringify(result, null, 2));
    process.exit(1);
  }

  // 1. load: does dlopen even work for this N-API addon itself.
  let lib;
  try {
    lib = koffi.load("libc.so");
    result.checks.load = { pass: true };
  } catch (e) {
    result.checks.load = { pass: false, reason: e.message };
    result.ok = false;
  }

  // 2. call: plain FFI call through koffi's dispatcher (no callback).
  if (lib) {
    try {
      const getpid = lib.func("int getpid()");
      const pid = getpid();
      result.checks.call = { pass: typeof pid === "number" && pid > 0, pid };
      if (!result.checks.call.pass) result.ok = false;
    } catch (e) {
      result.checks.call = { pass: false, reason: e.message };
      result.ok = false;
    }
  }

  // 3. callback: native -> JS callback (this is the piece libffi-based
  //    node:ffi fails on with "ffi_closure_alloc failed"; opentui-core's
  //    yoga measure/dirtied callbacks are exactly this shape).
  if (lib) {
    try {
      const CmpCb = koffi.proto("int Cmp(const void *a, const void *b)");
      const qsort = lib.func("void qsort(void *base, unsigned long n, unsigned long sz, Cmp *cmp)");
      const buf = new Int32Array([5, 3, 9, 1, 7]);
      let calls = 0;
      const cb = koffi.register((a, b) => {
        calls++;
        return koffi.decode(a, "int") - koffi.decode(b, "int");
      }, koffi.pointer(CmpCb));
      qsort(buf, buf.length, 4, cb);
      koffi.unregister(cb);
      const sorted = Array.from(buf);
      const isSorted = sorted.every((v, i) => i === 0 || sorted[i - 1] <= v);
      result.checks.callback = { pass: isSorted && calls > 0, sorted, invocations: calls };
      if (!result.checks.callback.pass) result.ok = false;
    } catch (e) {
      result.checks.callback = { pass: false, reason: e.message };
      result.ok = false;
    }
  }

  result.anon_exec_mmap_regions_final = countAnonExecMaps();
  result.koffi_added_anon_exec_regions =
    result.anon_exec_mmap_regions_baseline >= 0 && result.anon_exec_mmap_regions_final >= 0
      ? result.anon_exec_mmap_regions_final - result.anon_exec_mmap_regions_baseline
      : null;

  console.log(JSON.stringify(result, null, 2));
  process.exit(result.ok ? 0 : 1);
}

main();
