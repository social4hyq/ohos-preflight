#!/usr/bin/env python3
"""compare-snapshots.py — diff two scripts/snapshot.sh runs on the same
machine (typically "developer mode on" vs "developer mode off").

This is deliberately NOT analyze.py: analyze.py diffs two different
*machines* (openharmony container vs a real HarmonyOS device) and asks
"does the sandbox restrict this capability". This tool diffs two runs
on the *same* device across a config flip and asks a different
question -- "did flipping developer mode change this outcome" -- so it
uses its own vocabulary (fixed/regressed/still_broken/still_ok/blocked)
instead of analyze.py's (same/needs_relax/anomaly/both_fail).

Usage:
    compare-snapshots.py OLD_DIR NEW_DIR [-o OUT_DIR] [--format json|md]

OLD_DIR/NEW_DIR are directories produced by scripts/snapshot.sh
(containing l0.jsonl, l1.jsonl, l2.jsonl, shim-check.json, meta.json).
"""
from __future__ import annotations

import json
import re
import sys
import tomllib
from pathlib import Path

PROJECT_DIR = Path(__file__).resolve().parent.parent
PROBES_TOML = PROJECT_DIR / "probes.toml"

# ohos-compat-check's verdict_str() (src/ohos_compat_check.c) emits these
# four literal Chinese strings. V_NEEDED means the symptom the shim works
# around still reproduces (bad -> "fail"); V_DROPPABLE means real
# platform behavior is now fine on its own (good -> "pass"); V_INCONCLUSIVE
# is an intermittent defect that didn't reproduce this run, same
# "couldn't determine" semantics as run.sh's exit-code-2 "unsupported".
# V_INFO rows carry no capability verdict at all (informational context
# only, e.g. seccomp/ptrace/prctl availability) and are excluded entirely.
SHIM_VERDICT_TO_STATUS = {
    "仍需要": "fail",
    "可关闭": "pass",
    "不确定": "unsupported",
}
SHIM_INFO_VERDICT = "信息"


def load_probes_meta() -> dict:
    """Load probes.toml's [[probes]] entries, keyed by id. Returns {} if
    the file is missing/unparseable rather than raising -- this tool must
    still produce a diff even for probe ids probes.toml doesn't know
    about (shim_* synthetic ids, or a brand new probe not yet documented;
    see audit-coverage.py for actually enforcing that coverage)."""
    if not PROBES_TOML.is_file():
        return {}
    try:
        raw = tomllib.loads(PROBES_TOML.read_text(encoding="utf-8"))
    except Exception:
        return {}
    return {p["id"]: p for p in raw.get("probes", []) if "id" in p}


def load_jsonl(path: Path) -> dict[str, dict]:
    out: dict[str, dict] = {}
    if not path.is_file():
        return out
    for raw in path.read_text(encoding="utf-8").splitlines():
        raw = raw.strip()
        if not raw:
            continue
        try:
            obj = json.loads(raw)
        except json.JSONDecodeError:
            continue
        name = obj.get("probe")
        if name:
            out[name] = obj
    return out


def load_shim_check(path: Path) -> dict[str, dict]:
    """Parse an `ohos-shim check --json` payload into the same
    {id: {status, reason}} shape as load_jsonl, namespaced with a
    `shim_` prefix so ids never collide with preflight's own a10_*-style
    probe ids (ohos-shim's own id for close_range is bare "close_range",
    preflight's is "a10_close_range" -- two different probes measuring
    related but not identical things)."""
    if not path.is_file():
        return {}
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (json.JSONDecodeError, OSError):
        return {}
    out: dict[str, dict] = {}
    for row in data.get("rows", []):
        verdict = row.get("verdict")
        if verdict == SHIM_INFO_VERDICT or verdict not in SHIM_VERDICT_TO_STATUS:
            continue
        rid = row.get("id")
        if not rid:
            continue
        out[f"shim_{rid}"] = {
            "status": SHIM_VERDICT_TO_STATUS[verdict],
            "reason": row.get("note", ""),
            "layer": "shim",
        }
    return out


def load_snapshot(directory: Path) -> tuple[dict[str, dict], dict]:
    """Merge l0/l1/l2 jsonl + shim-check.json into one {id: {status,
    reason, layer}} map, and return the run's meta.json alongside it."""
    merged: dict[str, dict] = {}
    for layer, fname in (("l0", "l0.jsonl"), ("l1", "l1.jsonl"), ("l2", "l2.jsonl")):
        for pid, obj in load_jsonl(directory / fname).items():
            merged[pid] = {
                "status": obj.get("status"),
                "reason": obj.get("reason", ""),
                "layer": layer,
            }
    merged.update(load_shim_check(directory / "shim-check.json"))

    meta_path = directory / "meta.json"
    if meta_path.is_file():
        try:
            meta = json.loads(meta_path.read_text(encoding="utf-8"))
        except (json.JSONDecodeError, OSError):
            meta = {}
    else:
        meta = {}
    meta.setdefault("blocked", False)
    meta.setdefault("block_reason", "")
    meta.setdefault("label", directory.name)
    return merged, meta


def classify(old_status: str | None, new_status: str | None) -> str:
    """Six-way verdict for a same-machine, cross-config diff.

    blocked takes priority over everything -- a snapshot.sh run that hit
    its blocking gate (raw ELF exec refused) writes literal "blocked"
    rows instead of leaving probes absent, specifically so this always
    wins over the untestable/missing-data path below and a devmode-off
    wipeout reads as "blocked", not as a pile of shrugs.
    """
    if old_status == "blocked" or new_status == "blocked":
        return "blocked"
    if old_status is None or new_status is None:
        return "untestable"
    if old_status == "unsupported" or new_status == "unsupported":
        return "untestable"
    if old_status == "pass" and new_status == "pass":
        return "still_ok"
    if old_status == "pass" and new_status == "fail":
        return "regressed"
    if old_status == "fail" and new_status == "pass":
        return "fixed"
    if old_status == "fail" and new_status == "fail":
        return "still_broken"
    return "untestable"


def _sort_key(name: str) -> tuple:
    m = re.match(r"^([a-z]*)(\d*)_?(.*)", name)
    if m:
        num = m.group(2)
        return (m.group(1), int(num) if num else -1, m.group(3))
    return (name, -1, "")


def compare(old_dir: Path, new_dir: Path) -> dict:
    old_map, old_meta = load_snapshot(old_dir)
    new_map, new_meta = load_snapshot(new_dir)
    probes_meta = load_probes_meta()

    all_ids = sorted(set(old_map) | set(new_map), key=_sort_key)

    results = []
    for pid in all_ids:
        o = old_map.get(pid, {})
        n = new_map.get(pid, {})
        gap = classify(o.get("status"), n.get("status"))
        meta_p = probes_meta.get(pid, {})
        results.append({
            "id": pid,
            "layer": n.get("layer") or o.get("layer"),
            "old_status": o.get("status"),
            "new_status": n.get("status"),
            "old_reason": o.get("reason", ""),
            "new_reason": n.get("reason", ""),
            "gap": gap,
            "desc": meta_p.get("desc", pid),
            "priority": meta_p.get("priority"),
            "request_id": meta_p.get("request_id"),
            "expected_version": meta_p.get("expected_version"),
            "retire": meta_p.get("retire"),
        })

    buckets: dict[str, list[dict]] = {
        "fixed": [], "regressed": [], "still_broken": [],
        "still_ok": [], "blocked": [], "untestable": [],
    }
    for r in results:
        buckets[r["gap"]].append(r)

    by_request_id: dict[str, dict[str, list[dict]]] = {}
    for r in results:
        rid = r.get("request_id")
        if not rid:
            continue
        group = by_request_id.setdefault(rid, {
            "fixed": [], "regressed": [], "still_broken": [],
            "still_ok": [], "blocked": [], "untestable": [],
        })
        group[r["gap"]].append(r)

    return {
        "old_meta": old_meta,
        "new_meta": new_meta,
        "summary": {k: len(v) for k, v in buckets.items()},
        "probes": results,
        **buckets,
        "by_request_id": by_request_id,
    }


def render_markdown(result: dict) -> str:
    old_meta = result["old_meta"]
    new_meta = result["new_meta"]
    s = result["summary"]
    lines = [
        f"# 快照对比：{old_meta.get('label')} → {new_meta.get('label')}",
        "",
        f"- 旧快照 devmode: `{old_meta.get('devmode')}`{'（BLOCKED: ' + old_meta['block_reason'] + '）' if old_meta.get('blocked') else ''}",
        f"- 新快照 devmode: `{new_meta.get('devmode')}`{'（BLOCKED: ' + new_meta['block_reason'] + '）' if new_meta.get('blocked') else ''}",
        "",
        f"| fixed | regressed | still_broken | still_ok | blocked | untestable |",
        f"|---|---|---|---|---|---|",
        f"| {s['fixed']} | {s['regressed']} | {s['still_broken']} | {s['still_ok']} | {s['blocked']} | {s['untestable']} |",
        "",
    ]

    def render_group(title: str, rows: list[dict]) -> list[str]:
        if not rows:
            return [f"## {title}", "", "（无）", ""]
        out = [f"## {title}", "", "| id | layer | old → new | reason | request_id | retire |", "|---|---|---|---|---|---|"]
        for r in rows:
            out.append(
                f"| `{r['id']}` | {r['layer']} | {r['old_status']} → {r['new_status']} | "
                f"{r['new_reason'] or r['old_reason']} | {r.get('request_id') or ''} | {r.get('retire') or ''} |"
            )
        out.append("")
        return out

    l1_regressed = [r for r in result["regressed"] if r["layer"] in ("l0", "l1", "shim")]
    l2_regressed = [r for r in result["regressed"] if r["layer"] == "l2"]

    lines += render_group("① 关闭后丢失的能力（L0/L1/shim regressed）", l1_regressed)
    lines += render_group("② 关闭后挂掉的应用（L2 regressed）", l2_regressed)
    lines += render_group("③ 与本次变更无关的既有降级（still_broken）", result["still_broken"])
    lines += render_group("已修复（fixed）", result["fixed"])
    lines += render_group("被阻断（blocked）", result["blocked"])

    if result["by_request_id"]:
        lines.append("## 按诉求编号分组")
        lines.append("")
        for rid, group in sorted(result["by_request_id"].items()):
            counts = ", ".join(f"{k}={len(v)}" for k, v in group.items() if v)
            lines.append(f"- **{rid}**: {counts}")
        lines.append("")

    return "\n".join(lines)


def main(argv: list[str]) -> int:
    out_dir: Path | None = None
    fmt = "md"
    args: list[str] = []
    i = 1
    while i < len(argv):
        if argv[i] == "-o" and i + 1 < len(argv):
            out_dir = Path(argv[i + 1]); i += 2
        elif argv[i] == "--format" and i + 1 < len(argv):
            fmt = argv[i + 1]; i += 2
        else:
            args.append(argv[i]); i += 1

    if len(args) != 2:
        print(f"usage: {argv[0]} OLD_DIR NEW_DIR [-o OUT_DIR] [--format json|md]", file=sys.stderr)
        return 2

    old_dir, new_dir = Path(args[0]), Path(args[1])
    result = compare(old_dir, new_dir)

    if fmt == "json":
        output = json.dumps(result, ensure_ascii=False, indent=2)
        suffix = "json"
    else:
        output = render_markdown(result)
        suffix = "md"

    if out_dir:
        out_dir.mkdir(parents=True, exist_ok=True)
        name = f"compare-{old_dir.name}-vs-{new_dir.name}.{suffix}"
        (out_dir / name).write_text(output, encoding="utf-8")
        print(f"written to {out_dir / name}", file=sys.stderr)
    else:
        print(output)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
