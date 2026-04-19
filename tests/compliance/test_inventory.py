"""Tests for scripts/compliance-inventory.py — tree-wide classifier."""
import json
import subprocess
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent.parent


def _run_inventory(*extra_args):
    """Invoke the inventory script and return (exit_code, stdout)."""
    cmd = ["python3", "scripts/compliance-inventory.py", *extra_args]
    result = subprocess.run(cmd, cwd=REPO, capture_output=True, text=True)
    return result.returncode, result.stdout, result.stderr


def _inventory_json():
    code, stdout, stderr = _run_inventory("--format=json")
    assert code == 0, f"script failed (exit {code}): {stderr}"
    return json.loads(stdout)


def test_every_tracked_file_in_inventory():
    """The inventory must cover every git-tracked file. No silent drops."""
    tracked = subprocess.check_output(
        ["git", "-C", str(REPO), "ls-files"], text=True
    ).splitlines()
    # Exclude directories (ls-files returns files only, but be defensive)
    tracked = {t for t in tracked if t and not (REPO / t).is_dir()}
    inventory = _inventory_json()
    inventoried = {row["path"] for row in inventory}
    missing = tracked - inventoried
    assert not missing, (
        f"{len(missing)} tracked files missing from inventory: "
        f"{sorted(missing)[:10]}"
    )


def test_wdsp_sources_classified_as_vendored():
    inventory = _inventory_json()
    wdsp = [r for r in inventory
            if r["path"].startswith("third_party/wdsp/src/")
            and r["path"].endswith((".c", ".h"))]
    assert wdsp, "no wdsp source files found — fixture broken"
    for row in wdsp:
        assert row["classification"] == "wdsp-vendored", (
            f"{row['path']} classified {row['classification']!r}, "
            f"expected 'wdsp-vendored'"
        )


def test_fftw3_files_classified_as_vendored():
    inventory = _inventory_json()
    fftw = [r for r in inventory if r["path"].startswith("third_party/fftw3/")]
    assert fftw, "no fftw3 files found — fixture broken"
    for row in fftw:
        assert row["classification"] == "fftw3-vendored"


def test_thetis_ports_classified_from_provenance():
    """Any file listed in THETIS-PROVENANCE.md must be classified 'thetis-port'
    (or a derivative label that includes 'thetis' — implementations may split
    Thetis vs mi0bot vs multi-source; check substring for flexibility)."""
    inventory = _inventory_json()
    by_path = {r["path"]: r for r in inventory}
    # Spot-check a well-known ported file
    known = "src/core/AppSettings.cpp"
    assert known in by_path, f"{known} not in inventory"
    assert "thetis" in by_path[known]["classification"], (
        f"{known} classified {by_path[known]['classification']!r}, "
        f"expected something containing 'thetis'"
    )


def test_plan_doc_classified_as_docs():
    """Sanity: a markdown file under docs/ is 'docs' or 'attribution-doc'."""
    inventory = _inventory_json()
    by_path = {r["path"]: r for r in inventory}
    known = "docs/architecture/2026-04-18-gpl-compliance-full-audit-plan.md"
    if known in by_path:
        assert by_path[known]["classification"] in ("docs", "attribution-doc")


def test_classifications_are_from_known_set():
    ALLOWED = {
        "thetis-port", "aethersdr-port", "mi0bot-port", "wdsp-vendored",
        "fftw3-vendored", "nereussdr-original", "attribution-doc", "docs",
        "test", "resource", "packaging",
    }
    inventory = _inventory_json()
    unknown = {r["classification"] for r in inventory} - ALLOWED
    assert not unknown, f"unknown classifications emitted: {unknown}"


def test_json_schema_stable():
    """Every inventory row must carry the three canonical fields."""
    inventory = _inventory_json()
    for row in inventory:
        assert set(row.keys()) >= {"path", "classification", "missing_markers"}
        assert isinstance(row["missing_markers"], list)
