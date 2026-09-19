#!/usr/bin/env python3
"""json_report.py — render AnalysisResult as pretty-printed JSON (machine-readable)."""
from __future__ import annotations

import json
import sys
from pathlib import Path

from analyze import analyze


def render(result: dict) -> str:
    """Render AnalysisResult dict as formatted JSON string."""
    return json.dumps(result, ensure_ascii=False, indent=2)


def main(argv: list[str]) -> int:
    if len(argv) < 3:
        print(f"用法: {argv[0]} oh.jsonl hm.jsonl [-m meta.json] [-o result.json]", file=sys.stderr)
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
        print(f"用法: {argv[0]} oh.jsonl hm.jsonl [-m meta.json] [-o result.json]", file=sys.stderr)
        return 2

    result = analyze(args[0], args[1], meta_path)
    output = render(result)

    if out_path:
        Path(out_path).write_text(output, encoding="utf-8")
        print(f"JSON 报告已保存至 {out_path}", file=sys.stderr)
    else:
        print(output)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
