#!/usr/bin/env python3
"""markdown.py — render AnalysisResult as a diff-summary Markdown report."""
from __future__ import annotations

import sys
from datetime import datetime
from pathlib import Path

from analyze import analyze


def render(result: dict) -> str:
    """Render AnalysisResult as Markdown string (diff summary only)."""
    lines: list[str] = []
    w = lines.append

    s = result["summary"]
    now = datetime.now().strftime("%Y-%m-%d %H:%M")

    w("# OpenHarmony vs HarmonyOS 系统能力对比报告")
    w(f"> 生成时间：{now}")
    w("")

    w("## 概览")
    w("")
    w("| 指标 | 值 |")
    w("|------|-----|")
    total = s["total"]
    w(f'| 探针总数 | {total} |')
    w(f'| 两端一致通过 | {s["same_pass"]} ({s["same_pass"]/total*100:.1f}%) |' if total else "| 两端一致通过 | 0 |")
    w(f'| 需申请放行 | {s["needs_relax"]} ({s["needs_relax"]/total*100:.1f}%) |' if total else "| 需申请放行 | 0 |")
    w(f'| 两端共同失败 | {s["both_fail"]} ({s["both_fail"]/total*100:.1f}%) |' if total else "| 两端共同失败 | 0 |")
    if s.get("anomalies"):
        w(f'| 异常 | {s["anomalies"]} |')
    if s.get("untestable"):
        w(f'| 无法测定（环境问题） | {s["untestable"]} |')
    w("")

    def status_cn(st: str) -> str:
        labels = result.get("status_labels", {})
        return labels.get(st, st)

    probes_map = {p["id"]: p for p in result["probes"]}

    def gap_section(title: str, probe_ids: list[str], icon: str) -> None:
        w(f"## {icon} {title}")
        w("")
        if not probe_ids:
            w("（无）")
            w("")
            return
        w("| 探针 | 描述 | OH | HM | 建议 |")
        w("|------|------|-----|-----|------|")
        for pid in probe_ids:
            p = probes_map.get(pid, {})
            oh = status_cn(p.get("oh_status", "?"))
            hm = status_cn(p.get("hm_status", "?"))
            desc = p.get("desc", "")
            sol = p.get("solution", "-") or "-"
            # Escape pipes in fields to avoid breaking the markdown table.
            desc_esc = desc.replace("|", "\\|")
            sol_esc = sol.replace("|", "\\|")
            w(f'| `{pid}` | {desc_esc} | {oh} | {hm} | {sol_esc} |')
        w("")

        # Fallback code blocks for probes with solution_code or no_portable_fallback
        for pid in probe_ids:
            p = probes_map.get(pid, {})
            sol_code = p.get("solution_code")
            no_fb = p.get("no_portable_fallback")
            if not (sol_code or no_fb):
                continue
            w(f"#### `{pid}` — 替代方案")
            w("")
            fb_id = p.get("fallback_probe")
            if fb_id:
                fb_oh = p.get("fallback_oh_status")
                fb_hm = p.get("fallback_hm_status")
                if fb_oh == "pass" and fb_hm == "pass":
                    w(f"\u2714 \u53cc\u8f68\u9a8c\u8bc1\u901a\u8fc7（`{fb_id}`）")
                else:
                    w(f"\u2716 \u53cc\u8f68\u9a8c\u8bc1\u5931\u8d25（`{fb_id}`，OH={fb_oh}/HM={fb_hm}）")
                w("")
            elif no_fb:
                w(f"\u26a0 \u65e0 portable \u9a8c\u8bc1\u9879\uff1a{no_fb}")
                w("")
            if sol_code:
                w("```c")
                for line in sol_code.strip().split("\n"):
                    w(line)
                w("```")
                w("")

    # 需申请放行：按 P0/P1/P2 分组并附详情
    relax_ids = result.get("needs_relax", [])
    w("## \U0001f534 需申请放行 — OH 通过但 HM 受限")
    w("")
    if not relax_ids:
        w("（无）")
        w("")
    else:
        priority_titles = {
            "P0": "P0 · 阻断运行（进程崩溃或无法启动）",
            "P1": "P1 · 影响核心功能",
            "P2": "P2 · 影响工具链",
        }
        grouped_by_prio: dict[str, list[str]] = {"P0": [], "P1": [], "P2": [], "_": []}
        for pid in relax_ids:
            prio = probes_map.get(pid, {}).get("priority")
            grouped_by_prio.setdefault(prio if prio in priority_titles else "_", []).append(pid)

        for prio in ("P0", "P1", "P2", "_"):
            ids = grouped_by_prio.get(prio, [])
            if not ids:
                continue
            heading = priority_titles.get(prio, "其他受限项")
            w(f"### {heading}（{len(ids)} 项）")
            w("")
            for pid in ids:
                p = probes_map.get(pid, {})
                desc = p.get("desc", "")
                bun_tag = " **[Bun]**" if p.get("bun") else ""
                w(f"- **`{pid}`**{bun_tag} — {desc}")
                if p.get("impact"):
                    w(f"  - **影响：** {p['impact']}")
                if p.get("trigger_path"):
                    w(f"  - **触发路径：** `{p['trigger_path']}`")
                if p.get("solution"):
                    w(f"  - **建议：** {p['solution']}")
                if p.get("solution_code"):
                    fb_id = p.get("fallback_probe")
                    no_fb = p.get("no_portable_fallback")
                    fb_suffix = ""
                    if fb_id:
                        fb_oh = p.get("fallback_oh_status")
                        fb_hm = p.get("fallback_hm_status")
                        if fb_oh == "pass" and fb_hm == "pass":
                            fb_suffix = f" — \u2714 \u53cc\u8f68\u9a8c\u8bc1\u901a\u8fc7（`{fb_id}`）"
                        else:
                            fb_suffix = f" — \u2716 \u53cc\u8f68\u9a8c\u8bc1\u5931\u8d25（`{fb_id}`，OH={fb_oh}/HM={fb_hm}）"
                    elif no_fb:
                        fb_suffix = f" — \u26a0 \u65e0 portable \u9a8c\u8bc1\u9879"
                    w(f"  - **替代实现**（HarmonyOS 支持前的过渡方案）{fb_suffix}：")
                    if no_fb:
                        w(f"    > \u26a0 {no_fb}")
                    w("")
                    w("    ```c")
                    for line in p["solution_code"].strip().split("\n"):
                        w(f"    {line}")
                    w("    ```")
            w("")

    gap_section("异常 — HM 通过但 OH 失败", result.get("anomalies", []), "\U0001f7e1")
    gap_section("两端共同失败", result.get("both_fail", []), "\u26aa")

    untestable_ids = result.get("untestable", [])
    if untestable_ids:
        gap_section(
            "无法测定 — 前置条件不满足（exit 2），先修环境再下结论",
            untestable_ids, "\U0001f527",
        )

    return "\n".join(lines)


def main(argv: list[str]) -> int:
    if len(argv) < 3:
        print(f"用法: {argv[0]} oh.jsonl hm.jsonl [-m meta.json] [-o report.md]", file=sys.stderr)
        return 2

    out_path = None
    meta_path = None
    args: list[str] = []
    i = 1
    while i < len(argv):
        if argv[i] == "-o" and i + 1 < len(argv):
            out_path = argv[i + 1]; i += 2
        elif argv[i] == "-m" and i + 1 < len(argv):
            meta_path = argv[i + 1]; i += 2
        else:
            args.append(argv[i]); i += 1

    if len(args) != 2:
        print(f"用法: {argv[0]} oh.jsonl hm.jsonl [-m meta.json] [-o report.md]", file=sys.stderr)
        return 2

    result = analyze(args[0], args[1], meta_path)
    output = render(result)

    if out_path:
        Path(out_path).write_text(output, encoding="utf-8")
        print(f"Markdown 报告已保存至 {out_path}", file=sys.stderr)
    else:
        print(output)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
