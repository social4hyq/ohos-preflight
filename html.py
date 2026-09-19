#!/usr/bin/env python3
"""
html.py — four-zone HTML report renderer for ohos-preflight.

Usage:
    python3 html.py openharmony.jsonl harmonyos.jsonl -o report.html [-m meta.json]

Zones:
  1. Cover & executive summary (env cards, conclusion, stat cards)
  2. Release request list P0/P1/P2 (OS team primary audience)
  3. Probe detail tables with filter bar (developer audience)
  4. Both-fail groups & methodology
"""
from __future__ import annotations

import json
import sys
from datetime import datetime
from pathlib import Path

# html.py name collides with stdlib html module — remove script dir from
# sys.path temporarily so we can import the real stdlib html module, then
# restore before importing local modules.
_html_script_dir = str(Path(__file__).resolve().parent)
if _html_script_dir in sys.path:
    sys.path.remove(_html_script_dir)

import html as stdhtml

sys.path.insert(0, _html_script_dir)
from analyze import analyze


PROJECT_DIR = Path(__file__).resolve().parent
PROBES_DIR = PROJECT_DIR / "probes"


def _h(s: str) -> str:
    return stdhtml.escape(str(s))


def find_source(probe_name: str) -> tuple[str, str] | None:
    """Return (lang, escaped_source) or None."""
    for ext, lang in [(".c", "c"), (".sh", "bash")]:
        src = PROBES_DIR / f"{probe_name}{ext}"
        if src.exists():
            return lang, stdhtml.escape(src.read_text(encoding="utf-8"))
    # Cargo-style subdirectory layout: probes/<name>/src/{main,lib}.rs
    for stem in ("main", "lib"):
        rs_src = PROBES_DIR / probe_name / "src" / f"{stem}.rs"
        if rs_src.exists():
            return "rust", stdhtml.escape(rs_src.read_text(encoding="utf-8"))
    return None


import re as _re
SOURCE_REPO_MAP: list[tuple[str, str, str]] = [
    ("bun/",    "https://github.com/oven-sh/bun/blob/main/",              ""),
    ("libuv/",  "https://github.com/libuv/libuv/blob/v1.50.0/",          ""),
]
_SRC_PATH_RE = _re.compile(
    r'\b((?:bun|libuv)/src/[^\s]*?\.(?:rs|c|cpp|zig|h)(?::\d+)?)\b'
)
def _linkify_source_paths(text: str) -> str:
    if not text: return text
    escaped = stdhtml.escape(text)
    def _replace(match: _re.Match[str]) -> str:
        full = match.group(0)
        path_part, line = (full.rsplit(":", 1) + [""])[:2]
        for prefix, base_url, _ in SOURCE_REPO_MAP:
            if path_part.startswith(prefix):
                url = f"{base_url}{path_part[len(prefix):]}"
                if line: url += f"#L{line}"
                return f'<a href="{url}" target="_blank" rel="noopener">{full}</a>'
        return full
    return _SRC_PATH_RE.sub(_replace, escaped)

def reason_hint(reason: str, hints: dict[str, str]) -> str:
    if not reason:
        return ""
    for key, hint in hints.items():
        if key in reason:
            return hint
    return reason[:60] + "\u2026" if len(reason) > 62 else reason


def render(result: dict) -> str:
    """Render the AnalysisResult dict into a complete HTML document."""
    lines: list[str] = []

    def w(s: str = "") -> None:
        lines.append(s)

    summary = result["summary"]
    meta = result.get("meta", {})
    categories = result["categories"]
    probes = result["probes"]
    needs_relax = result["needs_relax"]
    anomalies = result["anomalies"]
    both_fail = result["both_fail"]
    status_labels = result["status_labels"]
    hints = result["reason_hints"]
    total = summary["total"]
    same_pass = summary["same_pass"]
    now = datetime.now().strftime("%Y-%m-%d %H:%M")

    probe_map: dict[str, dict] = {p["id"]: p for p in probes}

    grouped: dict[str, list[dict]] = {}
    for p in probes:
        cat = p["category"]
        grouped.setdefault(cat, []).append(p)

    # -- HTML head --
    w("<!DOCTYPE html>")
    w('<html lang="zh-CN"><head><meta charset="utf-8">')
    w('<meta name="viewport" content="width=device-width,initial-scale=1">')
    w("<title>OpenHarmony vs HarmonyOS \u7cfb\u7edf\u80fd\u529b\u5bf9\u6bd4\u62a5\u544a</title>")
    w("<style>")
    w(r"""
/* ── 颜色变量 ───────────────────────────────────────────── */
:root {
  --pass: #2da44e; --fail: #cf222e; --na: #9a6700;
  --bg: #ffffff; --bg2: #f6f8fa; --border: #d0d7de; --text: #1f2328;
  --code-bg: #f6f8fa; --link: #0969da;
  --p0: #cf222e; --p1: #e36209; --p2: #0969da;
  --badge-relax: #fff0f0; --badge-relax-text: #cf222e; --badge-relax-border: #ffcdd2;
  --badge-fail: #f6f8fa;  --badge-fail-text: #656d76;  --badge-fail-border: #d0d7de;
  --badge-pass: #f0fff4;  --badge-pass-text: #2da44e;  --badge-pass-border: #a8e6bf;
  --badge-anom: #fffbdd;  --badge-anom-text: #9a6700;  --badge-anom-border: #f0c000;
}
@media (prefers-color-scheme: dark) {
  :root {
    --bg:#0d1117; --bg2:#161b22; --border:#30363d; --text:#c9d1d9; --code-bg:#161b22;
    --badge-relax:#3d0000; --badge-relax-text:#ff7b72; --badge-relax-border:#6d1010;
    --badge-fail:#21262d;  --badge-fail-text:#8b949e;  --badge-fail-border:#30363d;
    --badge-pass:#0d2818;  --badge-pass-text:#56d364;  --badge-pass-border:#1a4f2a;
    --badge-anom:#2d2000;  --badge-anom-text:#d29922;  --badge-anom-border:#5a3e00;
  }
}
body.dark {
  --bg:#0d1117; --bg2:#161b22; --border:#30363d; --text:#c9d1d9; --code-bg:#161b22;
  --badge-relax:#3d0000; --badge-relax-text:#ff7b72; --badge-relax-border:#6d1010;
  --badge-fail:#21262d;  --badge-fail-text:#8b949e;  --badge-fail-border:#30363d;
  --badge-pass:#0d2818;  --badge-pass-text:#56d364;  --badge-pass-border:#1a4f2a;
  --badge-anom:#2d2000;  --badge-anom-text:#d29922;  --badge-anom-border:#5a3e00;
}

/* ── 重置 & 基础 ─────────────────────────────────────────── */
*{box-sizing:border-box;margin:0;padding:0;}
body{font:15px/1.65 -apple-system,BlinkMacSystemFont,"Segoe UI","Noto Sans","Helvetica Neue",sans-serif;
     color:var(--text);background:var(--bg);max-width:1200px;margin:0 auto;padding:28px 20px;}

/* ── 标题 ────────────────────────────────────────────────── */
.report-title{font-size:26px;font-weight:700;margin-bottom:4px;}
.report-subtitle{font-size:14px;color:#656d76;margin-bottom:4px;}
.report-time{font-size:13px;color:#656d76;margin-bottom:24px;}

/* ── 区块分隔 ────────────────────────────────────────────── */
.section{border-top:2px solid var(--border);padding-top:28px;margin-top:32px;}
.section-title{font-size:20px;font-weight:700;margin-bottom:16px;}

/* ── 环境卡片 ────────────────────────────────────────────── */
.env-cards{display:grid;grid-template-columns:1fr 1fr;gap:16px;margin-bottom:24px;}
.env-card{border:1px solid var(--border);border-radius:10px;padding:16px 20px;background:var(--bg2);}
.env-card .role-tag{display:inline-block;font-size:11px;font-weight:700;padding:2px 8px;
  border-radius:10px;margin-bottom:8px;letter-spacing:.3px;}
.env-card.has-sandbox .role-tag{background:#fff0f0;color:#cf222e;border:1px solid #ffcdd2;}
.env-card.no-sandbox  .role-tag{background:#f0fff4;color:#2da44e;border:1px solid #a8e6bf;}
body.dark .env-card.has-sandbox .role-tag{background:#3d0000;color:#ff7b72;border-color:#6d1010;}
body.dark .env-card.no-sandbox  .role-tag{background:#0d2818;color:#56d364;border-color:#1a4f2a;}
.env-card .env-name{font-size:16px;font-weight:700;margin-bottom:10px;}
.env-card table{width:100%;font-size:13px;border-collapse:collapse;}
.env-card td{padding:3px 0;vertical-align:top;}
.env-card td:first-child{color:#656d76;width:80px;}

/* ── 结论文字 ────────────────────────────────────────────── */
.conclusion{background:var(--bg2);border:1px solid var(--border);border-radius:8px;
  padding:14px 18px;margin-bottom:20px;font-size:14px;line-height:1.7;}

/* ── 数字卡片 ────────────────────────────────────────────── */
.stat-cards{display:grid;grid-template-columns:repeat(4,1fr);gap:12px;margin-bottom:32px;}
.stat-card{text-align:center;padding:16px 8px;border-radius:8px;
  background:var(--bg2);border:1px solid var(--border);}
.stat-card .num{font-size:30px;font-weight:700;line-height:1.1;}
.stat-card .label{font-size:12px;color:#656d76;margin-top:4px;}
.stat-card .sublabel{font-size:11px;color:#656d76;margin-top:2px;}
.stat-card.pass-rate .num{color:var(--pass);}
.stat-card.relax-count .num{color:var(--fail);}
.stat-card.bun-block .num{color:var(--p0);}
@media(max-width:640px){.stat-cards{grid-template-columns:repeat(2,1fr);}
  .env-cards{grid-template-columns:1fr;}}

/* ── 申请放行卡片（第二区）────────────────────────────────── */
.priority-group{margin-bottom:28px;}
.priority-group-title{font-size:15px;font-weight:700;margin-bottom:12px;
  display:flex;align-items:center;gap:8px;}
.p-badge{display:inline-block;font-size:11px;font-weight:700;padding:2px 8px;
  border-radius:4px;letter-spacing:.5px;}
.p-badge.P0{background:#fff0f0;color:var(--p0);border:1px solid #ffcdd2;}
.p-badge.P1{background:#fff8f0;color:var(--p1);border:1px solid #ffd9b3;}
.p-badge.P2{background:#f0f6ff;color:var(--p2);border:1px solid #b6d4fe;}
body.dark .p-badge.P0{background:#3d0000;color:#ff7b72;border-color:#6d1010;}
body.dark .p-badge.P1{background:#2d1500;color:#ffa657;border-color:#5a2d00;}
body.dark .p-badge.P2{background:#0a1628;color:#58a6ff;border-color:#1f4080;}

.relax-card{border-radius:8px;border:1px solid var(--border);
  margin-bottom:12px;overflow:hidden;}
.relax-card.P0{border-left:4px solid var(--p0);}
.relax-card.P1{border-left:4px solid var(--p1);}
.relax-card.P2{border-left:4px solid var(--p2);}
.relax-card-head{padding:12px 16px;background:var(--bg2);
  display:flex;align-items:baseline;gap:10px;flex-wrap:wrap;}
.relax-card-head code{font-size:14px;font-weight:700;}
.relax-card-head .probe-desc{font-size:13px;color:#656d76;}
.relax-card-body{padding:12px 16px;display:grid;gap:10px;font-size:13px;}
.relax-field-label{font-size:11px;font-weight:700;text-transform:uppercase;
  letter-spacing:.5px;color:#656d76;margin-bottom:2px;}
.relax-evidence{background:var(--code-bg);border:1px solid var(--border);
  border-radius:6px;padding:8px 12px;font-family:monospace;font-size:12px;}
.relax-evidence .ev-row{display:flex;gap:8px;margin:2px 0;}
.relax-evidence .ev-env{font-weight:700;min-width:120px;color:#656d76;}
.relax-evidence .ev-pass{color:var(--pass);}
.relax-evidence .ev-fail{color:var(--fail);}
.relax-solution{color:var(--link);}
details{margin-top:6px;}
details summary{cursor:pointer;font-size:12px;color:#656d76;user-select:none;
  padding:4px 0;}
details summary:hover{color:var(--link);}
details[open] summary{margin-bottom:8px;}
.trigger-path{font-family:monospace;font-size:12px;background:var(--code-bg);
  border:1px solid var(--border);border-radius:6px;padding:8px 12px;
  word-break:break-all;margin-bottom:8px;}

/* ── TOC（第三区）────────────────────────────────────────── */
.toc-bar{position:sticky;top:0;background:var(--bg);z-index:10;
  padding:8px 0;border-bottom:1px solid var(--border);margin-bottom:20px;
  display:flex;flex-wrap:wrap;gap:6px;}
.toc-btn{display:inline-flex;align-items:center;gap:6px;
  font-size:12px;padding:3px 10px;border-radius:6px;text-decoration:none;
  border:1px solid var(--border);color:var(--text);background:var(--bg2);}
.toc-btn:hover{border-color:var(--link);color:var(--link);}
.toc-prog{display:inline-block;width:40px;height:6px;border-radius:3px;
  background:var(--border);overflow:hidden;vertical-align:middle;}
.toc-prog-fill{height:100%;background:var(--pass);}

/* ── 筛选栏 ──────────────────────────────────────────────── */
.filter-bar{display:flex;gap:8px;align-items:center;flex-wrap:wrap;margin-bottom:16px;}
.filter-input{flex:1;min-width:200px;padding:7px 12px;border:1px solid var(--border);
  border-radius:6px;font-size:13px;background:var(--bg);color:var(--text);}
.filter-input:focus{outline:none;border-color:var(--link);
  box-shadow:0 0 0 2px rgba(9,105,218,.2);}
.filter-btn{padding:5px 12px;border:1px solid var(--border);border-radius:6px;
  background:var(--bg);color:var(--text);font-size:12px;cursor:pointer;user-select:none;}
.filter-btn:hover{background:var(--bg2);}
.filter-btn.active{font-weight:600;}
.filter-btn[data-filter="relax"].active{background:var(--badge-relax);
  color:var(--badge-relax-text);border-color:var(--badge-relax-border);}
.filter-btn[data-filter="fail"].active{background:var(--badge-fail);
  color:var(--badge-fail-text);border-color:var(--badge-fail-border);}
.filter-btn[data-filter="anom"].active{background:var(--badge-anom);
  color:var(--badge-anom-text);border-color:var(--badge-anom-border);}
.filter-btn[data-filter="bun"].active{background:#f0f6ff;color:var(--p2);border-color:#b6d4fe;}
.filter-count{font-size:12px;color:#656d76;margin-left:auto;white-space:nowrap;}

/* ── 探针表格 ────────────────────────────────────────────── */
.cat-section{margin-bottom:32px;}
.cat-header{display:flex;align-items:center;gap:12px;margin-bottom:6px;}
.cat-header h2{font-size:17px;font-weight:700;margin:0;}
.cat-prog-wrap{flex:1;max-width:200px;}
.cat-prog-bar{height:8px;border-radius:4px;background:var(--border);overflow:hidden;
  display:flex;}
.cat-prog-pass{background:var(--pass);}
.cat-prog-relax{background:var(--fail);}
.cat-prog-fail{background:#bbb;}
.cat-stats{font-size:12px;color:#656d76;white-space:nowrap;}
.cat-detail-text{font-size:13px;color:#656d76;margin-bottom:10px;}

table{width:100%;border-collapse:collapse;font-size:13px;margin-bottom:4px;}
thead{position:sticky;top:42px;}
th{background:var(--bg2);text-align:left;padding:8px 10px;
   border-bottom:2px solid var(--border);font-weight:600;white-space:nowrap;}
td{padding:7px 10px;border-bottom:1px solid var(--border);vertical-align:top;}
tr.probe-row{cursor:pointer;transition:background .12s;}
tr.probe-row:hover{background:var(--bg2);}
.bun-badge{display:inline-block;font-size:10px;font-weight:700;color:var(--p2);
  background:#f0f6ff;border:1px solid #b6d4fe;border-radius:10px;
  padding:1px 6px;margin-left:4px;vertical-align:middle;}
body.dark .bun-badge{background:#0a1628;border-color:#1f4080;}

.webkit-badge{display:inline-block;font-size:10px;font-weight:700;color:#8250df;
  background:#f5f0ff;border:1px solid #d4c5f9;border-radius:10px;
  padding:1px 6px;margin-left:3px;vertical-align:middle;}
body.dark .webkit-badge{background:#1a1028;border-color:#4a2f80;}
.bun-run-badge{display:inline-block;font-size:10px;font-weight:700;color:#1a7f37;
  background:#dafbe1;border:1px solid #8cd49a;border-radius:10px;
  padding:1px 6px;margin-left:3px;vertical-align:middle;}
body.dark .bun-run-badge{background:#0d2818;border-color:#1a4f2a;}
.nodejs-badge{display:inline-block;font-size:10px;font-weight:700;color:#0f6e31;
  background:#dcffe4;border:1px solid #8bc49a;border-radius:10px;
  padding:1px 6px;margin-left:3px;vertical-align:middle;}
body.dark .nodejs-badge{background:#0d2818;border-color:#1a5c2a;}
.vite-plus-badge{display:inline-block;font-size:10px;font-weight:700;color:#b08800;
  background:#fff8e8;border:1px solid #e8d080;border-radius:10px;
  padding:1px 6px;margin-left:3px;vertical-align:middle;}
body.dark .vite-plus-badge{background:#2d2000;border-color:#5a3e00;}
.playwright-badge{display:inline-block;font-size:10px;font-weight:700;color:#0550ae;
  background:#ddf4ff;border:1px solid #80c8ff;border-radius:10px;
  padding:1px 6px;margin-left:3px;vertical-align:middle;}
body.dark .playwright-badge{background:#0a1628;border-color:#1f5a80;}

/* ── 状态徽章（4色）─────────────────────────────────────── */
.gap-badge{display:inline-block;font-size:11px;font-weight:600;padding:2px 8px;
  border-radius:10px;white-space:nowrap;}
.gap-badge.needs_relax{background:var(--badge-relax);color:var(--badge-relax-text);
  border:1px solid var(--badge-relax-border);}
.gap-badge.both_fail{background:var(--badge-fail);color:var(--badge-fail-text);
  border:1px solid var(--badge-fail-border);}
.gap-badge.same{background:var(--badge-pass);color:var(--badge-pass-text);
  border:1px solid var(--badge-pass-border);}
.gap-badge.anomaly{background:var(--badge-anom);color:var(--badge-anom-text);
  border:1px solid var(--badge-anom-border);}
.gap-badge.untestable{background:var(--badge-anom);color:var(--badge-anom-text);
  border:1px dashed var(--badge-anom-border);}

.st-pass{color:var(--pass);font-weight:600;cursor:help;}
.st-fail{color:var(--fail);font-weight:600;cursor:help;}
.st-na{color:var(--na);font-weight:600;cursor:help;}
.st-miss{color:#656d76;font-style:italic;}
.reason-text{font-size:11px;color:#656d76;margin-top:2px;word-break:break-all;}

/* ── 展开源码行 ──────────────────────────────────────────── */
.src-row{display:none;}
.src-row.open{display:table-row;}
.src-row td{padding:0 10px 12px;border-bottom:2px solid var(--border);background:var(--code-bg);}
.src-wrap{max-height:400px;overflow:auto;border-radius:6px;
  border:1px solid var(--border);margin-top:4px;}
.src-wrap pre{font:12px/1.45 "JetBrains Mono","Cascadia Code","SF Mono","Fira Code",monospace;
  padding:12px;margin:0;tab-size:4;}
.src-lang{display:inline-block;font-size:11px;color:#656d76;padding:2px 6px;
  background:#eaeef2;border-radius:4px;margin-bottom:4px;}
body.dark .src-lang{background:#21262d;}

/* ── 共同失败（第四区）──────────────────────────────────── */
.fail-group{margin-bottom:20px;}
.fail-group-title{font-size:14px;font-weight:700;color:#656d76;
  margin-bottom:8px;padding-bottom:4px;border-bottom:1px solid var(--border);}
.fail-item{padding:8px 12px;border:1px solid var(--border);border-radius:6px;
  margin-bottom:6px;font-size:13px;}
.fail-item code{font-weight:700;}
.fail-item .alt{margin-top:4px;font-size:12px;color:var(--link);}

/* ── 方法论 ──────────────────────────────────────────────── */
.method-grid{display:grid;grid-template-columns:1fr 1fr;gap:16px;margin-top:12px;}
.method-card{background:var(--bg2);border:1px solid var(--border);
  border-radius:8px;padding:14px 16px;font-size:13px;}
.method-card h4{font-size:13px;font-weight:700;margin-bottom:8px;}
.method-card ul{padding-left:16px;}
.method-card li{margin:3px 0;}
@media(max-width:640px){.method-grid{grid-template-columns:1fr;}}

/* ── 主题切换 ────────────────────────────────────────────── */
.theme-toggle{position:fixed;top:12px;right:12px;z-index:100;padding:6px 10px;
  border:1px solid var(--border);border-radius:6px;background:var(--bg2);
  color:var(--text);font-size:16px;cursor:pointer;opacity:.8;}
.theme-toggle:hover{opacity:1;}

/* ── 打印 ────────────────────────────────────────────────── */
@media print{
  .theme-toggle,.filter-bar,.toc-bar{display:none!important;}
  thead{position:static;}
  .src-row,.src-row.open{display:none!important;}
  details{display:block;}
  details summary{display:none;}
  .relax-card{page-break-inside:avoid;}
  body{padding:0;font-size:12px;}
  .section{border-top:1px solid #ccc;}
}
""")
    w("</style></head><body>")

    # -- 主题切换按钮 --
    w('<button class="theme-toggle" onclick="toggleDark()" title="\u5207\u6362\u4eae\u8272/\u6697\u8272\u6a21\u5f0f">\U0001f313</button>')

    # ── 第一区：封面 & 执行摘要 ──
    w('<div>')
    w('<h1 class="report-title">OpenHarmony vs HarmonyOS \u7cfb\u7edf\u80fd\u529b\u5bf9\u6bd4\u62a5\u544a</h1>')
    w('<p class="report-subtitle">\u8bc4\u4f30 Bun / vite-plus / Playwright / NodeJS \u524d\u7aef\u5f00\u53d1\u5de5\u5177\u94fe\u5728 HarmonyOS \u4e0a\u7684\u9002\u914d\u53ef\u884c\u6027\uff0c\u91cf\u5316\u6c99\u7bb1\u9650\u5236\u8303\u56f4</p>')
    w(f'<p class="report-time">\u751f\u6210\u65f6\u95f4\uff1a{now}</p>')

    # 环境卡片
    hm_meta = meta.get("hm", {})
    ci_meta = meta.get("ci", {})
    if hm_meta or ci_meta:
        w('<div class="env-cards">')

        def env_card(m: dict, css_cls: str, role: str, icon: str) -> None:
            label  = _h(m.get("label",    "\u2014"))
            kernel = _h(m.get("kernel",   "\u2014"))
            ohos   = _h(m.get("ohos",     "\u2014"))
            build  = _h(m.get("build",    "\u2014"))
            hw     = _h(m.get("hardware", "\u2014"))
            clang  = _h(m.get("clang",    "\u2014"))
            w(f'<div class="env-card {css_cls}">')
            w(f'<div class="role-tag">{icon} {_h(role)}</div>')
            w(f'<div class="env-name">{label}</div>')
            w('<table>')
            for k, v in [("OHOS", ohos), ("\u5185\u6838", kernel), ("\u6784\u5efa", build), ("\u786c\u4ef6", hw), ("\u7f16\u8bd1\u5668", clang)]:
                w(f'<tr><td>{k}</td><td>{v}</td></tr>')
            w('</table>')
            w('</div>')

        env_card(hm_meta, "has-sandbox", "\u6709\u6c99\u7bb1 \u00b7 \u771f\u673a\u5b9e\u9645\u53ef\u7528\u80fd\u529b", "\U0001f5a5")
        env_card(ci_meta, "no-sandbox",  "\u65e0\u6c99\u7bb1 \u00b7 \u7528\u6237\u6001\u80fd\u529b\u4e0a\u9650",   "\U0001f4e6")
        w('</div>')

    # 一句话结论（动态生成）
    same_pass_rate = summary.get("same_pass_rate", 0)
    conclusion_parts = [
        f"{total} \u9879\u63a2\u9488\u4e2d\uff0c{same_pass} \u9879\u4e24\u7aef\u4e00\u81f4\u901a\u8fc7"
        f"\uff08{same_pass_rate:.1f}%\uff09\uff1b"
        f"{len(needs_relax)} \u9879 HarmonyOS \u53d7\u6c99\u7bb1\u9650\u5236\u9700\u7533\u8bf7\u653e\u884c"
        f"\uff08\u8986\u76d6 Bun / vite-plus / Playwright / NodeJS\uff09\uff1b"
        f"{len(both_fail)} \u9879\u4e24\u7aef\u5171\u540c\u5931\u8d25\uff08\u5e73\u53f0\u7279\u6027\u6216\u5185\u6838\u672a\u5b9e\u73b0\uff09"
    ]
    if anomalies:
        conclusion_parts.append(
            f"\uff1b{len(anomalies)} \u9879\u5f02\u5e38\uff08HarmonyOS \u901a\u8fc7\u4f46 OpenHarmony \u5bb9\u5668\u672a\u8986\u76d6\uff09"
        )
    untestable = result.get("untestable", [])
    if untestable:
        conclusion_parts.append(
            f"\uff1b{len(untestable)} \u9879\u65e0\u6cd5\u6d4b\u5b9a\uff08\u524d\u7f6e\u6761\u4ef6\u4e0d\u6ee1\u8db3\uff0c\u9700\u5148\u4fee\u590d\u73af\u5883\uff09"
        )
    conclusion_text = "".join(conclusion_parts) + "\u3002"
    w(f'<div class="conclusion">{_h(conclusion_text)}</div>')

    # 数字卡片
    cat_count = len(set(p["category"] for p in probes))
    hm_pass_count = same_pass + len(anomalies)
    w('<div class="stat-cards" style="grid-template-columns:repeat(3,1fr)">')
    w(f'<div class="stat-card pass-rate">'
      f'<div class="num">{summary["pass_rate_hm"]:.1f}%</div>'
      f'<div class="label">HarmonyOS \u901a\u8fc7\u7387</div>'
      f'<div class="sublabel">{hm_pass_count}/{total} \u9879\u901a\u8fc7</div>'
      f'</div>')
    w(f'<div class="stat-card relax-count">'
      f'<div class="num">{len(needs_relax)}</div>'
      f'<div class="label">\u9700\u7533\u8bf7\u653e\u884c</div>'
      f'<div class="sublabel">\u8986\u76d6 Bun / vite-plus / Playwright / NodeJS</div>'
      f'</div>')
    w(f'<div class="stat-card">'
      f'<div class="num">{total}</div>'
      f'<div class="label">\u63a2\u9488\u603b\u6570</div>'
      f'<div class="sublabel">\u8986\u76d6 {cat_count} \u5927\u7cfb\u7edf\u80fd\u529b\u7c7b\u522b</div>'
      f'</div>')
    w('</div>')
    w('</div>')  # 第一区结束

    # ── 第二区：申请放行清单 ──
    w('<div class="section">')
    w('<h2 class="section-title" style="cursor:pointer" onclick="toggleSection(\'relax\')">\U0001f534 \u9700\u7533\u8bf7\u653e\u884c <span id="relax-arrow">\u25bc</span></h2>')
    w('<div id="relax-body">')
    w('<p style="font-size:13px;color:#656d76;margin-bottom:20px">'
      '\u4ee5\u4e0b\u7cfb\u7edf\u8c03\u7528\u6216\u80fd\u529b\u5728 OpenHarmony \u5bb9\u5668\uff08\u65e0\u6c99\u7bb1\uff09\u4e2d\u53ef\u6b63\u5e38\u4f7f\u7528\uff0c'
      '\u4f46\u5728 HarmonyOS \u771f\u673a\u4e0a\u88ab seccomp \u6c99\u7bb1\u62e6\u622a\u6216\u6743\u9650\u62d2\u7edd\u3002'
      '\u5efa\u8bae\u9e3f\u8499 OS \u56e2\u961f\u5c06\u6807\u6ce8\u9879\u52a0\u5165\u5e94\u7528\u80fd\u529b\u767d\u540d\u5355\u3002</p>')

    priority_order = ["P0", "P1", "P2"]
    priority_labels = {
        "P0": ("\u963b\u65ad\u8fd0\u884c", "\u7f3a\u5931\u5bfc\u81f4\u8fdb\u7a0b\u5d29\u6e83\u6216\u65e0\u6cd5\u542f\u52a8"),
        "P1": ("\u5f71\u54cd\u6838\u5fc3\u529f\u80fd", "\u7f3a\u5931\u5bfc\u81f4\u67d0\u7c7b\u529f\u80fd\u4e0d\u53ef\u7528"),
        "P2": ("\u5f71\u54cd\u5de5\u5177\u94fe", "\u7f3a\u5931\u5f71\u54cd\u8c03\u8bd5/profiler\uff0c\u4e0d\u5f71\u54cd\u751f\u4ea7\u8fd0\u884c"),
    }
    relax_probes = [p for p in probes if p["gap"] == "needs_relax"]

    for prio in priority_order:
        group = [p for p in relax_probes if p.get("priority") == prio]
        if not group:
            continue
        ptitle, pdesc = priority_labels[prio]
        w('<div class="priority-group">')
        w(f'<div class="priority-group-title">'
          f'<span class="p-badge {prio}">{prio}</span>'
          f'{_h(ptitle)}\uff08{len(group)} \u9879\uff09'
          f'<span style="font-size:12px;color:#656d76;font-weight:400"> \u2014 {_h(pdesc)}</span>'
          f'</div>')

        for p in group:
            pid      = p["id"]
            desc     = p["desc"]
            is_bun   = p.get("bun", False)
            impact   = p.get("impact", "")
            trigger  = p.get("trigger_path", "")
            solution = p.get("solution", "")
            sol_code = p.get("solution_code", "")
            oh_r     = p.get("oh_reason", "") or ""
            hm_r     = p.get("hm_reason", "") or ""
            oh_s     = status_labels.get(p.get("oh_status"), "\u2014")
            hm_s     = status_labels.get(p.get("hm_status"), "\u2014")
            src      = find_source(pid)

            webkit_tag = '<span class="webkit-badge">WebKit</span>' if p.get("webkit") else ''
            bun_run_tag = '<span class="bun-run-badge">Bun运行</span>' if p.get("bun_run") else ''
            nodejs_tag = '<span class="nodejs-badge">NodeJS</span>' if p.get("nodejs") else ''
            vite_plus_tag = '<span class="vite-plus-badge">vite-plus</span>' if p.get("vite_plus") else ''
            playwright_tag = '<span class="playwright-badge">Playwright</span>' if p.get("playwright") else ''
            bun_tag = '<span class="bun-badge">Bun</span>' if is_bun else ''
            w(f'<div class="relax-card {prio}">')

            # 卡片头
            w(f'<div class="relax-card-head">'
              f'<code>{_h(pid)}</code>{bun_tag}{webkit_tag}{bun_run_tag}{nodejs_tag}{vite_plus_tag}{playwright_tag}'
              f'<span class="probe-desc">{_h(desc)}</span>'
              f'</div>')

            w('<div class="relax-card-body">')

            # 影响
            if impact:
                w(f'<div><div class="relax-field-label">\u5f71\u54cd</div>'
                  f'<div>{_h(impact)}</div></div>')

            # 实测结果
            oh_cls = "ev-pass" if p.get("oh_status") == "pass" else "ev-fail"
            hm_cls = "ev-pass" if p.get("hm_status") == "pass" else "ev-fail"
            oh_hint = reason_hint(oh_r, hints)
            hm_hint = reason_hint(hm_r, hints)
            w(f'<div><div class="relax-field-label">\u5b9e\u6d4b\u7ed3\u679c</div>'
              f'<div class="relax-evidence">'
              f'<div class="ev-row"><span class="ev-env">OpenHarmony</span>'
              f'<span class="{oh_cls}">{_h(oh_s)}</span>'
              f'{("&nbsp;&nbsp;" + _h(oh_hint)) if oh_hint and p.get("oh_status") != "pass" else ""}'
              f'</div>'
              f'<div class="ev-row"><span class="ev-env">HarmonyOS</span>'
              f'<span class="{hm_cls}">{_h(hm_s)}</span>'
              f'{("&nbsp;&nbsp;" + _h(hm_hint)) if hm_hint else ""}'
              f'</div>'
              f'</div></div>')

            # 建议
            if solution:
                w(f'<div><div class="relax-field-label">\u5efa\u8bae</div>'
                  f'<div class="relax-solution">\U0001f4a1 {_h(solution)}</div></div>')

            # 替代实现（HarmonyOS 支持前的过渡方案）
            if sol_code:
                fb_id = p.get("fallback_probe")
                no_fb = p.get("no_portable_fallback")
                fb_badge = ""
                if fb_id:
                    fb_oh = p.get("fallback_oh_status")
                    fb_hm = p.get("fallback_hm_status")
                    fb_ok = (fb_oh == "pass" and fb_hm == "pass")
                    cls = "ev-pass" if fb_ok else "ev-fail"
                    icon = "\u2714" if fb_ok else "\u2716"
                    label = "\u53cc\u8f68\u9a8c\u8bc1\u901a\u8fc7" if fb_ok else "\u53cc\u8f68\u9a8c\u8bc1\u5931\u8d25"
                    fb_badge = (f'&nbsp;&nbsp;<span class="{cls}" '
                                f'title="OH={fb_oh} / HM={fb_hm}">'
                                f'{icon} {_h(label)} ({_h(fb_id)})</span>')
                elif no_fb:
                    fb_badge = (f'&nbsp;&nbsp;<span class="ev-fail" '
                                f'title="{_h(no_fb)}">'
                                f'\u26a0 \u65e0 portable \u9a8c\u8bc1\u9879</span>')
                w('<details class="fallback-details">')
                w(f'<summary>\U0001f527 \u67e5\u770b\u66ff\u4ee3\u5b9e\u73b0\uff08HarmonyOS \u652f\u6301\u524d\u7684\u8fc7\u6e21\u65b9\u6848\uff09{fb_badge}</summary>')
                if no_fb:
                    w(f'<div style="font-size:12px;color:#656d76;margin:8px 0">'
                      f'\u26a0 {_h(no_fb)}</div>')
                w(f'<span class="src-lang">c</span>')
                w(f'<div class="src-wrap"><pre>{_h(sol_code.strip())}</pre></div>')
                w('</details>')
            elif p.get("no_portable_fallback"):
                w(f'<div><div class="relax-field-label">\u66ff\u4ee3\u5b9e\u73b0</div>'
                  f'<div style="font-size:13px;color:#656d76">'
                  f'\u26a0 \u65e0 portable \u9a8c\u8bc1\u9879\uff1a{_h(p["no_portable_fallback"])}'
                  f'</div></div>')

            # 技术依据（折叠）
            if trigger or src:
                w('<details>')
                w('<summary>\u25b6 \u67e5\u770b\u6280\u672f\u4f9d\u636e\u4e0e\u63a2\u9488\u6e90\u7801</summary>')
                if trigger:
                    w(f'<div class="relax-field-label">\u89e6\u53d1\u8def\u5f84</div>'
                      f'<div class="trigger-path">{_h(trigger)}</div>')
                if src:
                    lang, code = src
                    w(f'<span class="src-lang">{lang}</span>')
                    w(f'<div class="src-wrap"><pre>{code}</pre></div>')
                w('</details>')

            w('</div>')  # relax-card-body
            w('</div>')  # relax-card

        w('</div>')  # priority-group

    # 无 priority 字段的 needs_relax 探针（兜底）
    no_prio = [p for p in relax_probes if not p.get("priority")]
    if no_prio:
        w('<div class="priority-group">')
        w('<div class="priority-group-title">\u5176\u4ed6\u53d7\u9650\u9879</div>')
        for p in no_prio:
            w(f'<div class="relax-card" style="border-left:4px solid var(--border)">'
              f'<div class="relax-card-head"><code>{_h(p["id"])}</code>'
              f'<span class="probe-desc">{_h(p["desc"])}</span></div>'
              f'</div>')
        w('</div>')

    w('</div>')  # relax-body
    w('</div>')  # 第二区结束

    # ── 第三区：探针详情表 ──
    w('<div class="section">')
    w('<h2 class="section-title">\u8be6\u7ec6\u6d4b\u8bd5\u6570\u636e</h2>')

    # TOC（带迷你进度条）
    w('<nav class="toc-bar">')
    for cat_id in sorted(grouped):
        cat_info  = categories.get(cat_id, {})
        cat_label = cat_info.get("label", f"\u7c7b\u522b {cat_id}")
        c_total   = cat_info.get("total", 0)
        c_pass    = cat_info.get("pass_both", 0)
        pct       = round(c_pass / c_total * 100) if c_total else 0
        w(f'<a class="toc-btn" href="#cat-{cat_id}">'
          f'{cat_id} \u2014 {_h(cat_label)} {c_pass}/{c_total}'
          f'<span class="toc-prog"><span class="toc-prog-fill" style="width:{pct}%"></span></span>'
          f'</a>')
    w('</nav>')

    # 筛选栏
    w('<div class="filter-bar">')
    w('<input type="text" class="filter-input" id="filter-input" '
      'oninput="applyFilters()" placeholder="\u641c\u7d22\u63a2\u9488\u540d\u79f0\u6216\u63cf\u8ff0\u2026">')
    w('<button class="filter-btn" data-filter="relax" '
      'onclick="toggleFilter(this,\'relax\')">\U0001f534 \u9700\u7533\u8bf7\u653e\u884c</button>')
    w('<button class="filter-btn" data-filter="fail" '
      'onclick="toggleFilter(this,\'fail\')">\u26ab \u4e24\u7aef\u5931\u8d25</button>')
    w('<button class="filter-btn" data-filter="anom" '
      'onclick="toggleFilter(this,\'anom\')">\U0001f7e1 \u5f02\u5e38</button>')
    w('<button class="filter-btn" data-filter="webkit" onclick="toggleFilter(this,\'webkit\')">WebKit</button>')
    w('<button class="filter-btn" data-filter="bun-run" onclick="toggleFilter(this,\'bun-run\')">Bun运行</button>')
    w('<button class="filter-btn" data-filter="nodejs" onclick="toggleFilter(this,\'nodejs\')">NodeJS</button>')
    w('<button class="filter-btn" data-filter="vite-plus" onclick="toggleFilter(this,\'vite-plus\')">vite-plus</button>')
    w('<button class="filter-btn" data-filter="playwright" onclick="toggleFilter(this,\'playwright\')">Playwright</button>')
    w('<button class="filter-btn" data-filter="bun" '
      'onclick="toggleFilter(this,\'bun\')">Bun</button>')
    w('<span class="filter-count" id="filter-count"></span>')
    w('</div>')

    # 各分类表格
    gap_badge_map = {
        "needs_relax": ("needs_relax", "\U0001f534 \u9700\u7533\u8bf7\u653e\u884c"),
        "both_fail":   ("both_fail",   "\u26ab \u4e24\u7aef\u5747\u5931\u8d25"),
        "same":        ("same",        "\U0001f7e2 \u4e00\u81f4\u901a\u8fc7"),
        "anomaly":     ("anomaly",     "\U0001f7e1 \u5f02\u5e38"),
        "untestable":  ("untestable",  "\U0001f527 \u65e0\u6cd5\u6d4b\u5b9a"),
    }

    for cat_id in sorted(grouped):
        cat_info   = categories.get(cat_id, {})
        cat_label  = cat_info.get("label", f"\u7c7b\u522b {cat_id}")
        cat_detail = cat_info.get("detail", "")
        cat_probes = grouped[cat_id]
        c_total    = cat_info.get("total", len(cat_probes))
        c_pass     = cat_info.get("pass_both", 0)
        c_relax    = cat_info.get("needs_relax", 0)
        c_fail     = cat_info.get("both_fail", 0)

        pass_pct  = round(c_pass  / c_total * 100) if c_total else 0
        relax_pct = round(c_relax / c_total * 100) if c_total else 0
        fail_pct  = round(c_fail  / c_total * 100) if c_total else 0

        w(f'<div class="cat-section" id="cat-{cat_id}">')
        w('<div class="cat-header">')
        w(f'<h2>{cat_id} \u7c7b \u2014 {_h(cat_label)}</h2>')
        w(f'<div class="cat-prog-wrap">'
          f'<div class="cat-prog-bar">'
          f'<div class="cat-prog-pass" style="width:{pass_pct}%"></div>'
          f'<div class="cat-prog-relax" style="width:{relax_pct}%"></div>'
          f'<div class="cat-prog-fail" style="width:{fail_pct}%"></div>'
          f'</div></div>')
        relax_part = f' \u00b7 {c_relax} \u9700\u653e\u884c' if c_relax else ''
        fail_part  = f' \u00b7 {c_fail} \u5171\u540c\u5931\u8d25' if c_fail else ''
        w(f'<span class="cat-stats">{c_pass}/{c_total} \u901a\u8fc7{relax_part}{fail_part}</span>')
        w('</div>')
        if cat_detail:
            w(f'<p class="cat-detail-text">{_h(cat_detail)}</p>')

        w('<table><thead><tr>'
          '<th>\u63a2\u9488</th><th>\u63cf\u8ff0</th>'
          '<th>OpenHarmony</th><th>HarmonyOS</th>'
          '<th>\u72b6\u6001</th>'
          '</tr></thead><tbody>')

        for p in cat_probes:
            pid       = p["id"]
            desc      = p["desc"]
            gap       = p.get("gap", "same")
            is_bun    = p.get("bun", False)
            oh_status = p.get("oh_status")
            hm_status = p.get("hm_status")
            oh_reason = p.get("oh_reason", "")
            hm_reason = p.get("hm_reason", "")

            badge_cls, badge_txt = gap_badge_map.get(gap, ("same", "\U0001f7e2 \u4e00\u81f4\u901a\u8fc7"))
            src    = find_source(pid)
            src_id = f"src-{pid}"
            webkit_tag = '<span class="webkit-badge">WebKit</span>' if p.get("webkit") else ''
            bun_run_tag = '<span class="bun-run-badge">Bun运行</span>' if p.get("bun_run") else ''
            nodejs_tag = '<span class="nodejs-badge">NodeJS</span>' if p.get("nodejs") else ''
            vite_plus_tag = '<span class="vite-plus-badge">vite-plus</span>' if p.get("vite_plus") else ''
            playwright_tag = '<span class="playwright-badge">Playwright</span>' if p.get("playwright") else ''
            bun_tag = '<span class="bun-badge">Bun</span>' if is_bun else ''

            oh_hint = reason_hint(oh_reason, hints)
            hm_hint = reason_hint(hm_reason, hints)

            def status_cell(st: str | None, hint: str) -> str:
                if st is None:
                    return '<td class="st-miss">\u2014</td>'
                lbl = status_labels.get(st, st)
                cls = {"pass": "st-pass", "fail": "st-fail",
                       "unsupported": "st-na"}.get(st, "")
                if st == "pass":
                    return f'<td><span class="{cls}">{_h(lbl)}</span></td>'
                hint_attr = f' title="{_h(hint)}"' if hint else ''
                hint_html = f'<div class="reason-text">{_h(hint)}</div>' if hint else ''
                return f'<td><span class="{cls}"{hint_attr}>{_h(lbl)}</span>{hint_html}</td>'

            w(f'<tr class="probe-row" '
              f'data-name="{_h(pid)}" data-desc="{_h(desc)}" '
              f'data-gap="{gap}" data-bun="{"true" if is_bun else "false"}" data-webkit="{"true" if p.get("webkit") else "false"}" data-bun-run="{"true" if p.get("bun_run") else "false"}" data-nodejs="{"true" if p.get("nodejs") else "false"}" data-vite-plus="{"true" if p.get("vite_plus") else "false"}" data-playwright="{"true" if p.get("playwright") else "false"}" '
              f'onclick="toggleSrc(\'{src_id}\')">')
            w(f'<td><code>{_h(pid)}</code>{bun_tag}{webkit_tag}{bun_run_tag}{nodejs_tag}{vite_plus_tag}{playwright_tag}</td>')
            w(f'<td>{_h(desc)}</td>')
            w(status_cell(oh_status, oh_hint))
            w(status_cell(hm_status, hm_hint))
            w(f'<td><span class="gap-badge {badge_cls}">{badge_txt}</span></td>')
            w('</tr>')

            if src:
                lang, code = src
                w(f'<tr class="src-row" id="{src_id}"><td colspan="5">'
                  f'<span class="src-lang">{lang}</span>'
                  f'<div class="src-wrap"><pre>{code}</pre></div>'
                  f'</td></tr>')

        w('</tbody></table>')
        w('</div>')  # cat-section

    w('</div>')  # 第三区结束

    # ── 第四区：共同失败 & 测试方法 ──
    w('<div class="section">')
    w('<h2 class="section-title">\u26ab \u4e24\u7aef\u5171\u540c\u5931\u8d25</h2>')
    w('<p style="font-size:13px;color:#656d76;margin-bottom:16px">'
      '\u4ee5\u4e0b\u63a2\u9488\u5728 OpenHarmony \u5bb9\u5668\u548c HarmonyOS \u771f\u673a\u4e0a\u5747\u5931\u8d25\uff0c\u5c5e\u4e8e\u5e73\u53f0\u5c42\u9762\u7684\u5171\u540c\u9650\u5236\uff0c'
      '\u4e0d\u6d89\u53ca HarmonyOS \u6c99\u7bb1\u5dee\u5f02\u3002</p>')

    fail_groups: dict[str, list[str]] = {
        "\u5185\u6838\u672a\u5b9e\u73b0": [],
        "HarmonyOS \u6c99\u7bb1\u62e6\u622a": [],
        "\u5e73\u53f0\u7279\u6027\u5dee\u5f02": [],
    }
    for n in both_fail:
        p = probe_map.get(n, {})
        oh_r = p.get("oh_reason", "")
        hm_r = p.get("hm_reason", "")
        # OH 容器无沙箱，OH 侧 ENOSYS 或 "not implemented" 说明内核未实现
        if ("not implemented" in oh_r or "ENOSYS" in oh_r
                or "Function not implemented" in oh_r):
            fail_groups["\u5185\u6838\u672a\u5b9e\u73b0"].append(n)
        # 仅 HM 侧 SIGSYS / core dump 才是沙箱拦截（OH 容器无沙箱不可能有 SIGSYS）
        elif "SIGSYS" in hm_r or "core dump" in hm_r:
            fail_groups["HarmonyOS \u6c99\u7bb1\u62e6\u622a"].append(n)
        else:
            fail_groups["\u5e73\u53f0\u7279\u6027\u5dee\u5f02"].append(n)

    for group_name, probe_names in fail_groups.items():
        if not probe_names:
            continue
        w('<div class="fail-group">')
        w(f'<div class="fail-group-title">{_h(group_name)}\uff08{len(probe_names)} \u9879\uff09</div>')
        for n in probe_names:
            p    = probe_map.get(n, {})
            desc = p.get("desc", n)
            sol  = p.get("solution", "")
            hm_hint = reason_hint(p.get("hm_reason", ""), hints)
            oh_hint = reason_hint(p.get("oh_reason", ""), hints)
            w(f'<div class="fail-item">'
              f'<code>{_h(n)}</code> \u2014 {_h(desc)}')
            details_parts = []
            if oh_hint:
                details_parts.append(f'OH: {oh_hint}')
            if hm_hint:
                details_parts.append(f'HM: {hm_hint}')
            if details_parts:
                w(f'<span style="font-size:12px;color:#656d76;margin-left:8px">'
                  f'\uff08{" | ".join(_h(d) for d in details_parts)}\uff09</span>')
            if sol:
                w(f'<div class="alt">\U0001f4a1 {_h(sol)}</div>')

            trigger = p.get("trigger_path", "")
            if trigger:
                w('<details>')
                w('<summary>\u25b6 \u67e5\u770b\u6280\u672f\u4f9d\u636e</summary>')
                w(f'<div class="trigger-path" style="margin-top:8px">{_linkify_source_paths(trigger)}</div>')
                w('</details>')

            sol_code = p.get("solution_code", "")
            if sol_code:
                fb_id = p.get("fallback_probe")
                no_fb = p.get("no_portable_fallback")
                fb_badge = ""
                if fb_id:
                    fb_oh = p.get("fallback_oh_status")
                    fb_hm = p.get("fallback_hm_status")
                    fb_ok = (fb_oh == "pass" and fb_hm == "pass")
                    cls = "ev-pass" if fb_ok else "ev-fail"
                    icon = "\u2714" if fb_ok else "\u2716"
                    label = "\u53cc\u8f68\u9a8c\u8bc1\u901a\u8fc7" if fb_ok else "\u53cc\u8f68\u9a8c\u8bc1\u5931\u8d25"
                    fb_badge = (f'&nbsp;&nbsp;<span class="{cls}" '
                                f'title="OH={fb_oh} / HM={fb_hm}">'
                                f'{icon} {_h(label)} ({_h(fb_id)})</span>')
                elif no_fb:
                    fb_badge = (f'&nbsp;&nbsp;<span class="ev-fail" '
                                f'title="{_h(no_fb)}">'
                                f'\u26a0 \u65e0 portable \u9a8c\u8bc1\u9879</span>')
                w('<details class="fallback-details">')
                w(f'<summary>\U0001f527 \u67e5\u770b\u66ff\u4ee3\u5b9e\u73b0{fb_badge}</summary>')
                if no_fb:
                    w(f'<div style="font-size:12px;color:#656d76;margin:8px 0">'
                      f'\u26a0 {_h(no_fb)}</div>')
                w(f'<span class="src-lang">c</span>')
                w(f'<div class="src-wrap"><pre>{_h(sol_code.strip())}</pre></div>')
                w('</details>')
            elif p.get("no_portable_fallback"):
                w(f'<div style="font-size:12px;color:#656d76;margin:6px 0 0 0">'
                  f'\u26a0 \u65e0 portable \u9a8c\u8bc1\u9879\uff1a{_h(p["no_portable_fallback"])}</div>')
            w('</div>')
        w('</div>')

    # 方法论
    w('<h2 class="section-title" style="margin-top:32px">\u6d4b\u8bd5\u65b9\u6cd5</h2>')
    w('<div class="method-grid">')
    w('<div class="method-card"><h4>\u63a2\u9488\u8bbe\u8ba1</h4><ul>'
      '<li>\u6bcf\u4e2a\u63a2\u9488\u662f\u72ec\u7acb\u53ef\u6267\u884c\u6587\u4ef6</li>'
      '<li>exit 0 = \u901a\u8fc7\uff08\u80fd\u529b\u53ef\u7528\uff09</li>'
      '<li>exit 1 = \u5931\u8d25\uff08\u80fd\u529b\u4e0d\u53ef\u7528\uff09</li>'
      '<li>exit 2 = \u524d\u7f6e\u6761\u4ef6\u4e0d\u6ee1\u8db3</li>'
      '<li>stderr \u7b2c\u4e00\u884c\u4f5c\u4e3a\u539f\u56e0\u5b57\u6bb5</li>'
      '</ul></div>')
    w('<div class="method-card"><h4>\u5bf9\u6bd4\u65b9\u6cd5\u8bba</h4><ul>'
      '<li>OpenHarmony \u5bb9\u5668\uff1a\u65e0 init/SELinux/\u6c99\u7bb1\uff0c\u4ee3\u8868\u7528\u6237\u6001\u80fd\u529b\u4e0a\u9650</li>'
      '<li>HarmonyOS \u771f\u673a\uff1a\u6709\u5e94\u7528\u6c99\u7bb1\uff0c\u4ee3\u8868\u7528\u6237\u5b9e\u9645\u53ef\u7528\u80fd\u529b</li>'
      '<li>\u4e24\u7aef\u4f7f\u7528\u540c\u4e00 OHOS NDK clang \u4ea4\u53c9\u7f16\u8bd1\uff08aarch64-linux-ohos\uff09\uff0c\u786e\u4fdd ABI \u4e00\u81f4</li>'
      '</ul></div>')
    w(f'<div class="method-card"><h4>\u7f16\u8bd1\u73af\u5883</h4><ul>'
      f'<li>\u7f16\u8bd1\u5668\uff1aOHOS NDK clang</li>'
      f'<li>\u76ee\u6807\u67b6\u6784\uff1aaarch64-linux-ohos</li>'
      f'<li>\u63a2\u9488\u603b\u6570\uff1a{total} \u9879\uff0c\u8986\u76d6 {cat_count} \u7c7b\u7cfb\u7edf\u80fd\u529b</li>'
      f'</ul></div>')
    w('<div class="method-card"><h4>\u5dee\u5f02\u5206\u7c7b</h4><ul>'
      '<li>\U0001f534 \u9700\u7533\u8bf7\u653e\u884c\uff1aOH \u901a\u8fc7 / HM \u53d7\u9650</li>'
      '<li>\u26ab \u4e24\u7aef\u5747\u5931\u8d25\uff1a\u5e73\u53f0\u5171\u540c\u9650\u5236</li>'
      '<li>\U0001f7e1 \u5f02\u5e38\uff1aHM \u901a\u8fc7 / OH \u5931\u8d25\uff08\u7f55\u89c1\uff09</li>'
      '<li>\U0001f7e2 \u4e00\u81f4\u901a\u8fc7\uff1a\u4e24\u7aef\u5747\u53ef\u7528</li>'
      '</ul></div>')
    w('</div>')
    w('</div>')  # 第四区结束

    # -- JavaScript --
    w('<script>')
    w(r"""
function toggleSrc(id) {
  var r = document.getElementById(id);
  if (r) r.classList.toggle('open');
}

function toggleSection(name) {
  var body = document.getElementById(name + '-body');
  var arrow = document.getElementById(name + '-arrow');
  if (body && arrow) {
    if (body.style.display === 'none') {
      body.style.display = '';
      arrow.textContent = '\u25bc';
    } else {
      body.style.display = 'none';
      arrow.textContent = '\u25b6';
    }
  }
}

function toggleDark() {
  document.body.classList.toggle('dark');
  localStorage.setItem('ohos-pf-dark', document.body.classList.contains('dark') ? '1' : '0');
}

var _activeFilters = {};

function toggleFilter(btn, name) {
  btn.classList.toggle('active');
  if (_activeFilters[name]) { delete _activeFilters[name]; }
  else { _activeFilters[name] = true; }
  applyFilters();
}

function applyFilters() {
  var q = (document.getElementById('filter-input').value || '').toLowerCase();
  var rows = document.querySelectorAll('tr.probe-row');
  var visible = 0;

  rows.forEach(function(row) {
    var name = row.getAttribute('data-name') || '';
    var desc = row.getAttribute('data-desc') || '';
    var gap  = row.getAttribute('data-gap')  || '';
    var bun  = row.getAttribute('data-bun')  === 'true';

    var match = !q || name.indexOf(q) !== -1 || desc.toLowerCase().indexOf(q) !== -1;
    if (match && _activeFilters['webkit'] && row.getAttribute('data-webkit') !== 'true') match = false;
    if (match && _activeFilters['bun-run'] && row.getAttribute('data-bun-run') !== 'true') match = false;
    if (match && _activeFilters['nodejs'] && row.getAttribute('data-nodejs') !== 'true') match = false;
    if (match && _activeFilters['vite-plus'] && row.getAttribute('data-vite-plus') !== 'true') match = false;
    if (match && _activeFilters['playwright'] && row.getAttribute('data-playwright') !== 'true') match = false;
    if (match && _activeFilters['relax'] && gap !== 'needs_relax') match = false;
    if (match && _activeFilters['fail']  && gap !== 'both_fail')   match = false;
    if (match && _activeFilters['anom']  && gap !== 'anomaly')     match = false;
    if (match && _activeFilters['bun']   && !bun)                  match = false;

    row.style.display = match ? '' : 'none';
    var onclickVal = row.getAttribute('onclick') || '';
    var m = onclickVal.match(/'([^']+)'/);
    if (m) {
      var sr = document.getElementById(m[1]);
      if (sr) {
        if (!match) { sr.classList.remove('open'); sr.style.display = 'none'; }
        else { sr.style.display = ''; }
      }
    }
    if (match) visible++;
  });

  var ce = document.getElementById('filter-count');
  if (ce) ce.textContent = visible + '/' + rows.length + ' \u6761';

  document.querySelectorAll('.cat-section').forEach(function(sec) {
    var anyVisible = false;
    sec.querySelectorAll('tr.probe-row').forEach(function(r) {
      if (r.style.display !== 'none') anyVisible = true;
    });
    sec.style.display = anyVisible ? '' : 'none';
  });
}

(function() {
  if (localStorage.getItem('ohos-pf-dark') === '1') {
    document.body.classList.add('dark');
  }
  var rows = document.querySelectorAll('tr.probe-row');
  var ce = document.getElementById('filter-count');
  if (ce) ce.textContent = rows.length + '/' + rows.length + ' \u6761';
})();
""")
    w('</script>')
    w('</body></html>')

    return "\n".join(lines)


def main(argv: list[str]) -> int:
    out_path: str | None = None
    meta_path: str | None = None
    args: list[str] = []
    i = 1
    while i < len(argv):
        if argv[i] == "-o" and i + 1 < len(argv):
            out_path = argv[i + 1]
            i += 2
        elif argv[i] == "-m" and i + 1 < len(argv):
            meta_path = argv[i + 1]
            i += 2
        else:
            args.append(argv[i])
            i += 1

    if len(args) != 2:
        print(
            f"\u7528\u6cd5: {argv[0]} openharmony.jsonl harmonyos.jsonl [-o report.html] [-m meta.json]",
            file=sys.stderr,
        )
        return 2

    result = analyze(args[0], args[1], meta_path)
    html_output = render(result)

    if out_path:
        Path(out_path).write_text(html_output, encoding="utf-8")
        print(f"\u62a5\u544a\u5df2\u4fdd\u5b58\u81f3 {out_path}", file=sys.stderr)
    else:
        print(html_output)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
