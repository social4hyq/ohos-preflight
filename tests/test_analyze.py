"""Tests for analyze.py — gap classification and statistics pipeline."""
import json
import shutil
import sys
import tempfile
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import analyze


@pytest.fixture
def tmp_path():
    # Override pytest's builtin: its st_uid ownership check fails on
    # HarmonyOS tmpfs (forced setgid mode); tempfile does no such check.
    d = Path(tempfile.mkdtemp(prefix="ohpf-test-"))
    yield d
    shutil.rmtree(d, ignore_errors=True)


def fake_meta(probes: dict) -> dict:
    return {
        "categories": {"A": {"name": "内核能力", "detail": ""}},
        "probes": probes,
        "status_labels": {},
        "reason_hints": {},
    }


def write_jsonl(path: Path, entries: list[dict]) -> Path:
    path.write_text("\n".join(json.dumps(e) for e in entries) + "\n", encoding="utf-8")
    return path


def run_analyze(monkeypatch, tmp_path, probes_meta, ci_entries, hm_entries):
    monkeypatch.setattr(analyze, "load_probes_toml", lambda: fake_meta(probes_meta))
    ci = write_jsonl(tmp_path / "openharmony.jsonl", ci_entries)
    hm = write_jsonl(tmp_path / "harmonyos.jsonl", hm_entries)
    return analyze.analyze(str(ci), str(hm))


def tagged(probe_id: str, **extra) -> dict:
    return {"id": probe_id, "category": "A", "desc": probe_id, "bun": True, **extra}


# ---------------------------------------------------------------- classify

@pytest.mark.parametrize(
    "oh_status, hm_status, expected_gap",
    [
        ("pass", "pass", "same"),
        ("fail", "pass", "anomaly"),
        ("unsupported", "pass", "anomaly"),
        (None, "pass", "anomaly"),
        ("unsupported", "fail", "untestable"),
        ("fail", "unsupported", "untestable"),
        ("pass", "unsupported", "untestable"),
        ("unsupported", "unsupported", "untestable"),
        (None, "fail", "untestable"),
        ("fail", None, "untestable"),
        ("pass", "fail", "needs_relax"),
        ("fail", "fail", "both_fail"),
    ],
)
def test_classify(monkeypatch, tmp_path, oh_status, hm_status, expected_gap):
    ci = [{"probe": "a1_x", "status": oh_status, "reason": ""}] if oh_status else []
    hm = [{"probe": "a1_x", "status": hm_status, "reason": ""}] if hm_status else []
    result = run_analyze(monkeypatch, tmp_path, {"a1_x": tagged("a1_x")}, ci, hm)
    assert result["probes"][0]["gap"] == expected_gap


def test_unsupported_not_folded_into_fail(monkeypatch, tmp_path):
    """Regression for 50312c8: container unsupported (env unmet) must yield
    untestable, not both_fail/needs_relax."""
    result = run_analyze(
        monkeypatch, tmp_path,
        {"g1_tmpfile": tagged("g1_tmpfile")},
        [{"probe": "g1_tmpfile", "status": "unsupported", "reason": "ENOENT"}],
        [{"probe": "g1_tmpfile", "status": "fail", "reason": "EPERM"}],
    )
    assert result["probes"][0]["gap"] == "untestable"
    assert result["both_fail"] == []
    assert result["needs_relax"] == []
    assert result["untestable"] == ["g1_tmpfile"]


# ------------------------------------------------------- statistics pipeline

def test_summary_counts(monkeypatch, tmp_path):
    probes = {p: tagged(p) for p in ["a1_s", "a2_r", "a3_b", "a4_u", "a5_a"]}
    ci = [
        {"probe": "a1_s", "status": "pass"},
        {"probe": "a2_r", "status": "pass"},
        {"probe": "a3_b", "status": "fail"},
        {"probe": "a4_u", "status": "unsupported"},
        {"probe": "a5_a", "status": "fail"},
    ]
    hm = [
        {"probe": "a1_s", "status": "pass"},
        {"probe": "a2_r", "status": "fail"},
        {"probe": "a3_b", "status": "fail"},
        {"probe": "a4_u", "status": "fail"},
        {"probe": "a5_a", "status": "pass"},
    ]
    result = run_analyze(monkeypatch, tmp_path, probes, ci, hm)
    s = result["summary"]
    assert s["total"] == 5
    assert s["same_pass"] == 1
    assert s["needs_relax"] == 1
    assert s["both_fail"] == 1
    assert s["untestable"] == 1
    assert s["anomalies"] == 1
    assert s["pass_rate_hm"] == 40.0  # a1_s + a5_a
    assert s["pass_rate_oh"] == 40.0  # a1_s + a2_r
    assert s["same_pass_rate"] == 20.0
    cat = result["categories"]["A"]
    assert cat["total"] == 5
    assert cat["pass_both"] == 1
    assert cat["needs_relax"] == 1
    assert cat["both_fail"] == 1
    assert cat["untestable"] == 1


def test_untagged_probes_filtered(monkeypatch, tmp_path):
    probes = {
        "a1_x": tagged("a1_x"),
        "a2_helper": {"id": "a2_helper", "category": "A", "desc": "no tags"},
    }
    entries = [
        {"probe": "a1_x", "status": "pass"},
        {"probe": "a2_helper", "status": "fail"},
    ]
    result = run_analyze(monkeypatch, tmp_path, probes, entries, entries)
    assert [p["id"] for p in result["probes"]] == ["a1_x"]
    assert result["summary"]["total"] == 1
    assert result["both_fail"] == []
    assert result["categories"]["A"]["probes"] == ["a1_x"]


def test_empty_input_no_zero_division(monkeypatch, tmp_path):
    result = run_analyze(monkeypatch, tmp_path, {}, [], [])
    s = result["summary"]
    assert s["total"] == 0
    assert s["pass_rate_hm"] == 0
    assert s["pass_rate_oh"] == 0
    assert s["same_pass_rate"] == 0


def test_fallback_status_resolved_from_unfiltered_results(monkeypatch, tmp_path):
    """J-category fallback probes carry no tags and are filtered out of
    probe_results, but their status must still resolve for the main probe."""
    probes = {
        "a1_x": tagged("a1_x", fallback_probe="j1_fb"),
        "j1_fb": {"id": "j1_fb", "category": "J", "desc": "fallback"},
    }
    ci = [
        {"probe": "a1_x", "status": "pass"},
        {"probe": "j1_fb", "status": "pass"},
    ]
    hm = [
        {"probe": "a1_x", "status": "fail"},
        {"probe": "j1_fb", "status": "pass"},
    ]
    result = run_analyze(monkeypatch, tmp_path, probes, ci, hm)
    (p,) = result["probes"]
    assert p["fallback_probe"] == "j1_fb"
    assert p["fallback_oh_status"] == "pass"
    assert p["fallback_hm_status"] == "pass"


# ---------------------------------------------------------------- helpers

def test_load_jsonl_skips_garbage(tmp_path):
    path = tmp_path / "in.jsonl"
    path.write_text(
        '{"probe": "a1_x", "status": "pass"}\n'
        "\n"
        "not json at all\n"
        '{"status": "fail"}\n'
        '{"probe": "a2_y", "status": "fail"}\n',
        encoding="utf-8",
    )
    out = analyze.load_jsonl(str(path))
    assert set(out) == {"a1_x", "a2_y"}


def test_reason_hint():
    hints = {"EPERM": "权限被拒"}
    assert analyze.reason_hint("", hints) == ""
    assert analyze.reason_hint("open failed: EPERM", hints) == "权限被拒"
    short = "no match here"
    assert analyze.reason_hint(short, hints) == short
    long = "x" * 70
    assert analyze.reason_hint(long, hints) == "x" * 60 + "\u2026"


def test_sort_key_numeric_order():
    names = ["a10_z", "a2_y", "c1_x", "b1_w", "a1_v"]
    assert sorted(names, key=analyze._sort_key) == [
        "a1_v", "a2_y", "a10_z", "b1_w", "c1_x",
    ]
