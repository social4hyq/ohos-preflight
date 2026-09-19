"""Tests for compare-snapshots.py — same-machine devmode on/off diffing.

Unlike analyze.py (which diffs two different *machines*, container vs
real device, with a same/needs_relax/anomaly/both_fail vocabulary),
compare-snapshots.py diffs two runs on the *same* machine across a
config change (developer mode on -> off). The vocabulary here answers
a different question ("did this regress when we flipped a switch?"),
so it's deliberately a separate module with its own classify().
"""
import importlib.util
import json
import shutil
import sys
import tempfile
from pathlib import Path

import pytest

MODULE_PATH = Path(__file__).resolve().parents[1] / "scripts" / "compare-snapshots.py"
spec = importlib.util.spec_from_file_location("compare_snapshots", MODULE_PATH)
compare_snapshots = importlib.util.module_from_spec(spec)
sys.modules["compare_snapshots"] = compare_snapshots
spec.loader.exec_module(compare_snapshots)


@pytest.fixture
def tmp_path():
    # Same override as tests/test_analyze.py: pytest's builtin tmp_path
    # fixture does an st_uid ownership check that fails on HarmonyOS's
    # forced-setgid tmpfs; tempfile.mkdtemp does no such check.
    d = Path(tempfile.mkdtemp(prefix="ohpf-cmp-test-"))
    yield d
    shutil.rmtree(d, ignore_errors=True)


def write_jsonl(path: Path, entries: list[dict]) -> None:
    path.write_text(
        "\n".join(json.dumps(e) for e in entries) + ("\n" if entries else ""),
        encoding="utf-8",
    )


def write_snapshot(
    root: Path,
    label: str,
    l0=(), l1=(), l2=(),
    shim_rows=None,
    blocked=False,
    block_reason="",
    devmode="true",
) -> Path:
    d = root / label
    d.mkdir(parents=True)
    write_jsonl(d / "l0.jsonl", list(l0))
    write_jsonl(d / "l1.jsonl", list(l1))
    write_jsonl(d / "l2.jsonl", list(l2))
    if shim_rows is None:
        (d / "shim-check.json").write_text("{}", encoding="utf-8")
    else:
        (d / "shim-check.json").write_text(
            json.dumps({"meta": {}, "rows": shim_rows}), encoding="utf-8"
        )
    (d / "env.json").write_text(json.dumps({"devmode": devmode}), encoding="utf-8")
    (d / "meta.json").write_text(
        json.dumps({"label": label, "devmode": devmode, "blocked": blocked, "block_reason": block_reason}),
        encoding="utf-8",
    )
    return d


def row(probe, status, reason=""):
    return {"probe": probe, "track": "x", "status": status, "reason": reason}


# ---------------------------------------------------------------- classify

@pytest.mark.parametrize(
    "old_status, new_status, expected",
    [
        ("pass", "pass", "still_ok"),
        ("fail", "fail", "still_broken"),
        ("fail", "pass", "fixed"),
        ("pass", "fail", "regressed"),
        ("pass", "unsupported", "untestable"),
        ("unsupported", "pass", "untestable"),
        ("unsupported", "unsupported", "untestable"),
        (None, "pass", "untestable"),
        ("fail", None, "untestable"),
        ("blocked", "pass", "blocked"),
        ("pass", "blocked", "blocked"),
        ("blocked", "blocked", "blocked"),
    ],
)
def test_classify(old_status, new_status, expected):
    assert compare_snapshots.classify(old_status, new_status) == expected


# --------------------------------------------------------- load_shim_check

def test_load_shim_check_maps_verdicts(tmp_path):
    path = tmp_path / "shim-check.json"
    path.write_text(json.dumps({
        "meta": {},
        "rows": [
            {"id": "close_range", "group": "A", "verdict": "仍需要", "note": "still crashes"},
            {"id": "getcwd", "group": "G", "verdict": "可关闭", "note": "works natively now"},
            {"id": "splice_poll", "group": "G", "verdict": "不确定", "note": "not reproduced this run"},
            {"id": "fyi_only", "group": "Z", "verdict": "信息", "note": "reference only"},
        ],
    }), encoding="utf-8")
    result = compare_snapshots.load_shim_check(path)
    assert result["shim_close_range"]["status"] == "fail"
    assert result["shim_getcwd"]["status"] == "pass"
    assert result["shim_splice_poll"]["status"] == "unsupported"
    # V_INFO rows carry no verdict about whether a symptom still
    # reproduces -- they're not comparable across runs, so they must not
    # show up as a scored probe at all.
    assert "shim_fyi_only" not in result


def test_load_shim_check_missing_file_returns_empty(tmp_path):
    assert compare_snapshots.load_shim_check(tmp_path / "does-not-exist.json") == {}


# ------------------------------------------------------------ load_snapshot

def test_load_snapshot_merges_layers_and_shim(tmp_path):
    d = write_snapshot(
        tmp_path, "run-a",
        l0=[row("01_tmpdir_writable", "pass")],
        l1=[row("a10_close_range", "pass")],
        l2=[row("bun_version", "pass")],
        shim_rows=[{"id": "tmpfile", "group": "G", "verdict": "仍需要", "note": "EPERM"}],
    )
    probes, meta = compare_snapshots.load_snapshot(d)
    assert probes["01_tmpdir_writable"]["status"] == "pass"
    assert probes["01_tmpdir_writable"]["layer"] == "l0"
    assert probes["a10_close_range"]["layer"] == "l1"
    assert probes["bun_version"]["layer"] == "l2"
    assert probes["shim_tmpfile"]["status"] == "fail"
    assert meta["label"] == "run-a"
    assert meta["blocked"] is False


def test_load_snapshot_blocked_meta_propagates(tmp_path):
    d = write_snapshot(tmp_path, "run-b", blocked=True, block_reason="raw ELF exec refused")
    _, meta = compare_snapshots.load_snapshot(d)
    assert meta["blocked"] is True
    assert "refused" in meta["block_reason"]


# -------------------------------------------------------------------- compare

def test_compare_end_to_end_all_states(tmp_path):
    old = write_snapshot(
        tmp_path, "devmode-on",
        l1=[
            row("k1_exec_selfsigned_elf", "fail", "EACCES"),   # -> fixed
            row("a10_close_range", "pass"),                    # -> regressed
            row("a1_seccomp_unotify", "fail", "EINVAL"),        # -> still_broken
            row("g4_getcwd", "pass"),                           # -> still_ok
        ],
    )
    new = write_snapshot(
        tmp_path, "devmode-off",
        l1=[
            row("k1_exec_selfsigned_elf", "pass"),
            row("a10_close_range", "fail", "SIGSYS"),
            row("a1_seccomp_unotify", "fail", "EINVAL"),
            row("g4_getcwd", "pass"),
        ],
    )
    result = compare_snapshots.compare(old, new)
    gaps = {p["id"]: p["gap"] for p in result["probes"]}
    assert gaps["k1_exec_selfsigned_elf"] == "fixed"
    assert gaps["a10_close_range"] == "regressed"
    assert gaps["a1_seccomp_unotify"] == "still_broken"
    assert gaps["g4_getcwd"] == "still_ok"
    assert result["summary"]["fixed"] == 1
    assert result["summary"]["regressed"] == 1
    assert result["summary"]["still_broken"] == 1
    assert result["summary"]["still_ok"] == 1
    assert [p["id"] for p in result["regressed"]] == ["a10_close_range"]
    assert [p["id"] for p in result["fixed"]] == ["k1_exec_selfsigned_elf"]


def test_compare_new_snapshot_blocked_marks_everything_blocked(tmp_path):
    old = write_snapshot(
        tmp_path, "devmode-on",
        l1=[row("a10_close_range", "pass")],
        l2=[row("bun_version", "pass")],
    )
    new = write_snapshot(
        tmp_path, "devmode-off",
        l1=[row("a10_close_range", "blocked", "raw ELF exec refused")],
        l2=[row("bun_version", "blocked", "raw ELF exec refused")],
        blocked=True,
        block_reason="raw ELF exec refused",
    )
    result = compare_snapshots.compare(old, new)
    gaps = {p["id"]: p["gap"] for p in result["probes"]}
    assert gaps["a10_close_range"] == "blocked"
    assert gaps["bun_version"] == "blocked"
    assert result["new_meta"]["blocked"] is True
    assert result["summary"]["blocked"] == 2
    # Nothing should be miscounted as fixed/regressed when the whole run
    # was blocked -- that would misreport a wipeout as real capability
    # movement.
    assert result["summary"]["fixed"] == 0
    assert result["summary"]["regressed"] == 0


def test_compare_groups_by_request_id(tmp_path, monkeypatch):
    old = write_snapshot(tmp_path, "on", l1=[row("k1_x", "fail"), row("k2_y", "fail")])
    new = write_snapshot(tmp_path, "off", l1=[row("k1_x", "pass"), row("k2_y", "fail")])

    fake_probes = {
        "k1_x": {"id": "k1_x", "request_id": "HMOS-100", "desc": "x"},
        "k2_y": {"id": "k2_y", "request_id": "HMOS-100", "desc": "y"},
    }
    monkeypatch.setattr(compare_snapshots, "load_probes_meta", lambda: fake_probes)

    result = compare_snapshots.compare(old, new)
    assert "HMOS-100" in result["by_request_id"]
    group = result["by_request_id"]["HMOS-100"]
    assert {p["id"] for p in group["fixed"]} == {"k1_x"}
    assert {p["id"] for p in group["still_broken"]} == {"k2_y"}


def test_compare_probe_missing_from_toml_still_reported(tmp_path, monkeypatch):
    """A probe with no probes.toml entry (e.g. a shim_* synthetic id, or a
    freshly-added probe nobody documented yet) must still show up in the
    diff -- unlike analyze.py's tag-based filtering, this is a completeness
    tool and silently dropping undocumented probes would hide exactly the
    kind of coverage gap audit-coverage.py is meant to catch."""
    monkeypatch.setattr(compare_snapshots, "load_probes_meta", lambda: {})
    old = write_snapshot(tmp_path, "on", l1=[row("mystery_probe", "fail")])
    new = write_snapshot(tmp_path, "off", l1=[row("mystery_probe", "pass")])
    result = compare_snapshots.compare(old, new)
    assert any(p["id"] == "mystery_probe" for p in result["probes"])
