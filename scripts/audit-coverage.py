#!/usr/bin/env python3
"""audit-coverage.py — code <-> probe coverage audit for ohos-bun and
ohos-compat-shim.

Two independent checks:

1. bun-tree audit: find every `#[cfg(target_env = "ohos")]` / `__OHOS__`
   site in the ohos-bun source tree that *looks like* a runtime capability
   downgrade (nearby text mentions SIGSYS/seccomp/EACCES/kernel-refuses/
   etc, not just an ABI type difference or build-time path choice), then
   check it's covered by either:
     a. a probes.toml `[[probes.guards]]` anchor found in the same
        neighborhood, or
     b. the comment itself naming ohos-compat-shim and one of the shim's
        own hooked symbols (getcwd, linkat, ...) -- i.e. the code already
        says "the shim handles this", and the shim audit (below)
        independently confirms that hook has its own self-check.
   Uncovered sites are reported as "有降级无探针". probes.toml guards
   whose anchor text can no longer be found in the target file at all are
   reported as "anchor 已消失" (the code moved on; the probe may need
   retiring or updating).

   This is deliberately a heuristic, not a hard classifier -- ABI/
   metadata/build-config sites are expected to fall out naturally because
   they don't use the capability-failure vocabulary below, not because
   they're on a maintained exclude-list. See CAPABILITY_KEYWORDS.

2. shim-tree audit: cross-check the symbol names ohos_compat_shim.c
   actually intercepts (via its `shim_disabled("name")` calls) against
   the check ids ohos_compat_check.c self-tests (via its `add_row("id",
   "A", ...)` calls, group A = actual shim hooks, other groups are
   peripheral info-only probes). This one is exact, not heuristic --
   both sides are simple, stable naming conventions in the same repo.

Usage:
    audit-coverage.py --bun-tree ../../Software/ohos-bun \\
                       --shim-tree ../../Software/ohos-compat-shim
"""
from __future__ import annotations

import argparse
import re
import sys
import tomllib
from pathlib import Path

PROJECT_DIR = Path(__file__).resolve().parent.parent
PROBES_TOML = PROJECT_DIR / "probes.toml"

BUN_SOURCE_EXTS = {".rs", ".cpp", ".cc", ".c", ".h", ".hpp"}
BUN_EXCLUDE_DIRS = {
    "node_modules", "test", "tests", "vendor", "src/node-fallbacks",
    ".git", "target", "build",
}

MARKER_RE = re.compile(r'target_env\s*=\s*"ohos"|__OHOS__|cfg\(.*\bohos\b')

# Vocabulary that shows up, empirically, in every genuine capability-type
# OHOS downgrade comment seen across ohos-bun (SIGSYS/seccomp traps,
# specific wrong-errno kernel behavior, explicit "kernel refuses/denies"
# language) but not in ABI type-selection, metadata, or build-path
# comments. Tuned against this repo's actual comments, not a generic list.
CAPABILITY_KEYWORDS = [
    "sigsys", "seccomp", "eacces", "eperm", "enosys", "ebadf", "einval",
    "kernel refuses", "kernel bug", "kernel denies", "拒绝", "workaround",
    "fallback", "quirk", "hangs", "denies", "blocked", "silently fail",
    "stale", "residue", "ignored", "corrupt",
]

# Symbols ohos-compat-shim hooks, keyed the same way the code/comments
# name them. Used to recognize "the shim already covers this" call-outs
# in ohos-bun's own comments (e.g. sys/lib.rs's getcwd/linkat blocks).
SHIM_HOOK_NAMES = {
    "close_range", "getpwuid_r", "tmpfile", "getcwd", "linkat",
    "symlinkat", "fchmodat2", "splice",
}

# Prefix like "a10_", "k7_", "j13_" in front of a probe's actual symbol name.
PROBE_ID_PREFIX_RE = re.compile(r"^[a-z]+\d+_")
# Components too generic to trust alone as a coverage signal (would match
# almost any comment). Only components NOT in this set count toward the
# match requirement below.
GENERIC_COMPONENTS = {
    "and", "the", "for", "not", "pr", "set", "get", "is", "of", "on",
    "at", "to", "fb", "sym",
}


def load_all_probes() -> list[dict]:
    if not PROBES_TOML.is_file():
        return []
    raw = tomllib.loads(PROBES_TOML.read_text(encoding="utf-8"))
    return raw.get("probes", [])


def probe_symbol_components(probe_id: str) -> tuple[str, list[str]]:
    """('close_range', ['close', 'range']) from 'a10_close_range' -- the
    prefix-stripped remainder as a whole, plus its meaningful pieces."""
    remainder = PROBE_ID_PREFIX_RE.sub("", probe_id)
    parts = [p for p in remainder.split("_") if len(p) >= 4 and p not in GENERIC_COMPONENTS]
    return remainder, parts


def matches_existing_probe(text: str, probes: list[dict]) -> str | None:
    """Only the pre-existing 94/97 probes without a [[probes.guards]]
    entry rely on this path -- the new K-class probes are matched via
    their explicit guards in is_covered() instead. This exists so the
    audit doesn't have to pretend 97 probes' worth of prior coverage
    doesn't exist just because retrofitting guards onto all of them is
    out of scope; it recognizes coverage the same way a human skimming
    the probe list would -- by the syscall/symbol name each probe is
    named after actually showing up in the flagged comment.

    Plain substring containment, not \\b-bounded regex: these are
    snake_case identifiers (memfd_create, close_range) where the target
    token is often a prefix/component glued to more identifier text by
    underscores, so a word-boundary regex misses exactly the compound
    identifiers this is meant to catch. False-positive risk is low
    because every token here is a distinctive syscall/symbol fragment,
    not a generic English word.

    A probe's id is usually 2-3 meaningful components (e.g.
    "ancestor_stat_eacces"); requiring *every* component to show up
    verbatim is too strict when one component (here, "stat") is
    incidental phrasing that doesn't recur at every call site of the
    same underlying issue. Requiring a majority (at least 2, or all of
    them when there's only 1-2) is a better match for how these
    comments actually vary while still avoiding single-generic-word
    false positives.
    """
    lowered = text.lower()
    for p in probes:
        remainder, parts = probe_symbol_components(p["id"])
        if not parts:
            # Single distinctive token with no underscore (e.g. "openat2")
            # or a compound where every component was filtered as
            # generic (e.g. "memfd" alone after "d4_memfd") -- match the
            # whole remainder as a substring.
            if len(remainder) >= 5 and remainder in lowered:
                return p["id"]
            continue
        required = min(2, len(parts))
        if sum(1 for part in parts if part in lowered) >= required:
            return p["id"]
    return None

WINDOW_BEFORE = 30
WINDOW_AFTER = 6
MERGE_DISTANCE = 4

# Findings this audit genuinely surfaces (real, previously-undocumented
# capability-type OHOS workarounds -- the tool is working correctly by
# flagging these) but that are deliberately out of scope for the current
# 13-probe K-class batch. Each is a future k14+ candidate, not something
# to silently hide. Keyed by "file:line" as reported; kept separate from
# the "0 findings" bar so that bar keeps meaning "no *unacknowledged*
# gap", not "we stopped looking."
KNOWN_GAPS = {
    "src/event_loop/SpawnSyncEventLoop.rs:396":
        "spawnSync 短超时 wrapping_sub 下溢变成事实上的无限 epoll_wait 超时；"
        "未查是否与本机当前内核状态相关，候选未来探针",
    "src/spawn/process.rs:3278":
        "bun 自己的 pidfd+poll+getppid 父死监控回退路径（no_orphans），"
        "和 i10_pr_set_pdeathsig 测的是同一症状在 Playwright 场景下的表现，"
        "但这里是 bun 自身消费者、未被 i10 的 guards 覆盖，候选未来探针",
    "src/runtime/cli/run_command.rs:685":
        "项目根目录整体不可读时的 $HOME 兜底 DirInfo，"
        "与 g8_ancestor_stat_eacces 同一大类（沙箱 EACCES 目录）但代码路径独立，候选未来探针",
    "src/runtime/cli/run_command.rs:716":
        "同上（run_command.rs:685 兜底逻辑的非 OHOS 对照分支）",
    "src/runtime/cli/run_command.rs:800":
        "同上（run_command.rs:685 兜底触发的 npm_package_* 环境变量抑制逻辑）",
    "src/jsc/bindings/wtf-bindings.cpp:197":
        "OHOS 对 PTY master fd 的 tcsetattr(TCSADRAIN/TCSAFLUSH) 拒绝 EACCES，"
        "TCSANOW 可用；未被任何现有探针覆盖，候选未来探针",
    "src/install/lib.rs:599":
        "OHOS tmpfs 对新建目录强制 setgid+group-write，需要 chmod 回 0700 才能通过"
        "后续 EEXIST 权限检查；与 Workspace 记忆 environment_owner_only_path.md "
        "描述的 tmpfs 强制 2771 是同一现象，候选未来探针",
}


def iter_source_files(root: Path):
    for path in root.rglob("*"):
        if not path.is_file() or path.suffix not in BUN_SOURCE_EXTS:
            continue
        rel = path.relative_to(root)
        if any(part in BUN_EXCLUDE_DIRS for part in rel.parts):
            continue
        yield path


def load_bun_guards() -> list[dict]:
    """All probes.toml [[probes.guards]] entries with repo == 'ohos-bun'."""
    if not PROBES_TOML.is_file():
        return []
    raw = tomllib.loads(PROBES_TOML.read_text(encoding="utf-8"))
    out = []
    for p in raw.get("probes", []):
        for g in p.get("guards", []):
            if g.get("repo") == "ohos-bun":
                out.append({**g, "probe_id": p["id"]})
    return out


def find_marker_sites(bun_tree: Path) -> list[tuple[Path, int]]:
    """Every line matching MARKER_RE, merged into sites when consecutive
    hits are within MERGE_DISTANCE lines of each other (an if/else OHOS
    pair, or a multi-line cfg attribute, shouldn't count as two sites)."""
    sites: list[tuple[Path, int]] = []
    for path in iter_source_files(bun_tree):
        try:
            lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
        except OSError:
            continue
        hits = [i for i, line in enumerate(lines) if MARKER_RE.search(line)]
        last = -999
        for h in hits:
            if h - last > MERGE_DISTANCE:
                sites.append((path, h))
            last = h
    return sites


def window_text(lines: list[str], line_no: int) -> str:
    lo = max(0, line_no - WINDOW_BEFORE)
    hi = min(len(lines), line_no + WINDOW_AFTER)
    return "\n".join(lines[lo:hi])


def is_capability_candidate(text: str) -> bool:
    lowered = text.lower()
    return any(kw in lowered for kw in CAPABILITY_KEYWORDS)


def is_covered(text: str, guards_in_file: list[dict]) -> str | None:
    """Returns a human-readable reason if covered, else None."""
    for g in guards_in_file:
        anchor = g.get("anchor", "")
        if anchor and anchor in text:
            return f"guarded by probe {g['probe_id']} (anchor: {anchor[:50]!r})"
    lowered = text.lower()
    if "ohos-compat-shim" in lowered or "ohos_compat_shim" in lowered:
        for name in SHIM_HOOK_NAMES:
            if name in lowered:
                return f"self-documented as handled by ohos-compat-shim's {name} hook"
    return None


def audit_bun_tree(bun_tree: Path) -> tuple[list[dict], list[dict]]:
    """Returns (uncovered_findings, stale_guard_findings)."""
    guards = load_bun_guards()
    all_probes = load_all_probes()
    guards_by_file: dict[Path, list[dict]] = {}
    for g in guards:
        resolved = (bun_tree / g["path"]).resolve()
        guards_by_file.setdefault(resolved, []).append(g)

    uncovered = []
    file_cache: dict[Path, list[str]] = {}

    for path, line_no in find_marker_sites(bun_tree):
        if path not in file_cache:
            file_cache[path] = path.read_text(encoding="utf-8", errors="replace").splitlines()
        lines = file_cache[path]
        text = window_text(lines, line_no)
        if not is_capability_candidate(text):
            continue
        reason = is_covered(text, guards_by_file.get(path.resolve(), []))
        if reason is None:
            reason = matches_existing_probe(text, all_probes)
        if reason is None:
            rel = path.relative_to(bun_tree)
            key = f"{rel}:{line_no + 1}"
            uncovered.append({
                "file": str(rel),
                "line": line_no + 1,
                "snippet": lines[line_no].strip(),
                "known_gap": KNOWN_GAPS.get(key),
            })

    stale = []
    for g in guards:
        resolved = (bun_tree / g["path"]).resolve()
        if not resolved.is_file():
            stale.append({**g, "reason": f"file does not exist: {g['path']}"})
            continue
        content = resolved.read_text(encoding="utf-8", errors="replace")
        if g.get("anchor", "") not in content:
            stale.append({**g, "reason": "anchor text not found in file"})

    return uncovered, stale


def extract_shim_hooks(shim_tree: Path) -> set[str]:
    src = shim_tree / "src" / "ohos_compat_shim.c"
    if not src.is_file():
        return set()
    text = src.read_text(encoding="utf-8", errors="replace")
    names = set(re.findall(r'shim_disabled\("([a-z0-9_]+)"\)', text))
    names |= set(re.findall(r'env_list_has\("OHOS_COMPAT_SHIM_DISABLE",\s*"([a-z0-9_]+)"\)', text))
    return names


def extract_check_row_ids(shim_tree: Path) -> set[str]:
    src = shim_tree / "src" / "ohos_compat_check.c"
    if not src.is_file():
        return set()
    text = src.read_text(encoding="utf-8", errors="replace")
    # Only group "A" rows are actual shim-hook self-checks; other groups
    # (B: dynamic linker, etc.) are peripheral info-only probes.
    return set(re.findall(r'add_row\(\s*"([a-z0-9_]+)"\s*,\s*"A"', text))


def audit_shim_tree(shim_tree: Path) -> list[str]:
    hooks = extract_shim_hooks(shim_tree)
    checks = extract_check_row_ids(shim_tree)
    missing = sorted(hooks - checks)
    return [f"shim hooks '{name}' but ohos_compat_check.c has no matching self-check row" for name in missing]


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--bun-tree", type=Path, help="path to the ohos-bun repo (ohos-aarch64 branch)")
    ap.add_argument("--shim-tree", type=Path, help="path to the ohos-compat-shim repo")
    args = ap.parse_args()

    exit_code = 0

    if args.bun_tree:
        bun_tree = args.bun_tree.resolve()
        uncovered, stale = audit_bun_tree(bun_tree)
        unacknowledged = [f for f in uncovered if not f["known_gap"]]
        acknowledged = [f for f in uncovered if f["known_gap"]]
        print(f"=== ohos-bun coverage audit ({bun_tree}) ===")
        if unacknowledged:
            exit_code = 1
            print(f"\n有降级无探针，未登记 ({len(unacknowledged)}):")
            for f in unacknowledged:
                print(f"  {f['file']}:{f['line']}: {f['snippet']}")
        else:
            print("\n有降级无探针，未登记: 0 条")
        if acknowledged:
            print(f"\n已知但暂缓（未来 k14+ 候选，共 {len(acknowledged)} 条，不计入上面的 0）:")
            for f in acknowledged:
                print(f"  {f['file']}:{f['line']}: {f['known_gap']}")
        if stale:
            exit_code = 1
            print(f"\nanchor 已消失 ({len(stale)}):")
            for s in stale:
                print(f"  probe {s['probe_id']} -> {s['path']}: {s['reason']} (anchor: {s.get('anchor','')[:60]!r})")
        else:
            print("anchor 已消失: 0 条")
    else:
        print("(--bun-tree not given, skipping ohos-bun audit)")

    if args.shim_tree:
        shim_tree = args.shim_tree.resolve()
        print(f"\n=== ohos-compat-shim self-coverage audit ({shim_tree}) ===")
        issues = audit_shim_tree(shim_tree)
        if issues:
            exit_code = 1
            for i in issues:
                print(f"  {i}")
        else:
            print("  0 条问题")
    else:
        print("\n(--shim-tree not given, skipping ohos-compat-shim audit)")

    return exit_code


if __name__ == "__main__":
    sys.exit(main())
