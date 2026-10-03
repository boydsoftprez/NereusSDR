"""Tests for scripts/check-third-party-licenses.py (R-R3-50).

Each test builds a small fixture tree (a README with a licence table and a
sources table, a licence folder, a third_party/ directory and a CMake file)
and runs the script on it with --root.
"""
import importlib.util
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent.parent
SCRIPT = REPO / "scripts" / "check-third-party-licenses.py"

README = """\
# Third-Party License Notices

| Dependency | Role | Licence | Notice file | Full licence text |
| --- | --- | --- | --- | --- |
| Foo 1.0 | vendored example | MIT | `foo.txt` | `foo.txt` |
| Bar 2.0 | fetched example | GPL-2.0-or-later | `bar.txt` | `GPLv2.txt` |

| Library | Comes from | Pinned version | Ships in | Notice file |
| --- | --- | --- | --- | --- |
| Foo | `third_party/foo` | 1.0 | desktop packages | `foo.txt` |
| Bar | FetchContent `bar_upstream` | 2.0 | desktop packages | `bar.txt` |
"""

CMAKE = """\
include(FetchContent)
# A comment naming ExternalProject_Add(not_a_real_one) is ignored.
FetchContent_Declare(
    bar_upstream
    GIT_REPOSITORY https://example.invalid/bar.git
    GIT_TAG        v2.0
)
"""


def _make_tree(root: Path) -> Path:
    lic = root / "packaging" / "third-party-licenses"
    lic.mkdir(parents=True)
    (lic / "README.md").write_text(README, encoding="utf-8")
    for name in ("foo.txt", "bar.txt", "GPLv2.txt", "SOURCE-OFFER.txt"):
        (lic / name).write_text(f"{name}\n", encoding="utf-8")
    (root / "third_party" / "foo").mkdir(parents=True)
    (root / "third_party" / "foo" / "LICENSE").write_text("MIT\n", encoding="utf-8")
    (root / "CMakeLists.txt").write_text(CMAKE, encoding="utf-8")
    return root


def _run(root: Path):
    result = subprocess.run(
        [sys.executable, str(SCRIPT), "--root", str(root)],
        capture_output=True, text=True)
    return result.returncode, result.stdout + result.stderr


def test_complete_tree_passes(tmp_path):
    code, out = _run(_make_tree(tmp_path))
    assert code == 0, out
    assert "all present" in out


def test_third_party_dir_without_row_fails(tmp_path):
    root = _make_tree(tmp_path)
    (root / "third_party" / "orphanlib").mkdir()
    code, out = _run(root)
    assert code == 1
    assert "third_party/orphanlib" in out


def test_fetch_name_without_row_fails(tmp_path):
    root = _make_tree(tmp_path)
    cmake_dir = root / "cmake"
    cmake_dir.mkdir()
    (cmake_dir / "Extra.cmake").write_text(
        "ExternalProject_Add(baz_build\n    URL https://example.invalid/baz.zip)\n",
        encoding="utf-8")
    code, out = _run(root)
    assert code == 1
    assert "baz_build" in out
    assert "cmake/Extra.cmake" in out
    # The commented-out name in CMakeLists.txt is not reported.
    assert "not_a_real_one" not in out


def test_multi_line_declaration_without_row_fails(tmp_path):
    # The name on the line after the opening parenthesis, as the zlib,
    # portaudio, libspecbleach and rnnoise declarations are written.
    root = _make_tree(tmp_path)
    cmake_dir = root / "cmake"
    cmake_dir.mkdir()
    (cmake_dir / "Multi.cmake").write_text(
        "FetchContent_Declare(\n"
        "    newlib_upstream\n"
        "    GIT_REPOSITORY https://example.invalid/newlib.git\n"
        ")\n"
        "ExternalProject_Add(   # a comment after the parenthesis\n"
        "\n"
        "    other_build\n"
        "    URL https://example.invalid/other.zip)\n",
        encoding="utf-8")
    code, out = _run(root)
    assert code == 1
    assert "FetchContent_Declare newlib_upstream (cmake/Multi.cmake)" in out
    assert "ExternalProject_Add other_build (cmake/Multi.cmake)" in out


def test_multi_line_declarations_in_the_real_tree_are_found():
    spec = importlib.util.spec_from_file_location("check_licences", SCRIPT)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    names = {name for _, name, _ in module.find_fetch_names(REPO)}
    for multi_line in ("zlib", "portaudio", "libspecbleach_upstream",
                       "rnnoise_upstream"):
        assert multi_line in names


def _write_setup_scripts(root: Path, sh_pin, ps1_pin):
    """The two DeepFilterNet setup scripts, pinning sh_pin and ps1_pin (None
    writes a script that names no pin)."""
    sh = "#!/usr/bin/env bash\nset -euo pipefail\n"
    if sh_pin is not None:
        sh += f'DFNR_COMMIT="{sh_pin}"\n'
    sh += 'echo "Cloning DeepFilterNet at $DFNR_COMMIT..."\n'
    (root / "setup-deepfilter.sh").write_text(sh, encoding="utf-8")
    ps1 = '$ErrorActionPreference = "Stop"\n'
    if ps1_pin is not None:
        ps1 += f'$DfnrCommit = "{ps1_pin}"\n'
    ps1 += '$DfnrCommit | Out-File -Encoding ascii -NoNewline "$OutDir\\COMMIT"\n'
    (root / "setup-deepfilter.ps1").write_text(ps1, encoding="utf-8")


def _add_crate_notices(root: Path, generated_from, pinned: str,
                       sh_pin="same", ps1_pin="same"):
    _write_setup_scripts(root,
                         pinned if sh_pin == "same" else sh_pin,
                         pinned if ps1_pin == "same" else ps1_pin)
    lic = root / "packaging" / "third-party-licenses"
    readme = lic / "README.md"
    readme.write_text(readme.read_text(encoding="utf-8").replace(
        "| `foo.txt` | `foo.txt` |", "| `foo.txt`, `deepfilternet-crates.txt` | `foo.txt` |"),
        encoding="utf-8")
    header = "DeepFilterNet Rust crate notices\n\n"
    if generated_from is not None:
        header += f"DeepFilterNet commit: {generated_from}\n"
    (lic / "deepfilternet-crates.txt").write_text(header, encoding="utf-8")
    pin = root / "third_party" / "deepfilter"
    pin.mkdir(parents=True, exist_ok=True)
    (pin / "COMMIT").write_text(pinned + "\n", encoding="utf-8")
    readme.write_text(readme.read_text(encoding="utf-8") +
                      "| DeepFilterNet | `third_party/deepfilter` | x | desktop | "
                      "`deepfilternet-crates.txt` |\n", encoding="utf-8")


def test_crate_notices_from_the_pinned_commit_pass(tmp_path):
    root = _make_tree(tmp_path)
    _add_crate_notices(root, "d375b2d8aaaa", "d375b2d8aaaa")
    code, out = _run(root)
    assert code == 0, out


def test_crate_notices_from_another_commit_fail(tmp_path):
    root = _make_tree(tmp_path)
    _add_crate_notices(root, "d375b2d8aaaa", "0123456789ab")
    code, out = _run(root)
    assert code == 1
    assert "generated from DeepFilterNet d375b2d8aaaa" in out
    assert "pins 0123456789ab" in out


def test_crate_notices_naming_no_commit_fail(tmp_path):
    root = _make_tree(tmp_path)
    _add_crate_notices(root, None, "0123456789ab")
    code, out = _run(root)
    assert code == 1
    assert "names no DeepFilterNet commit" in out


def test_setup_sh_pinning_another_commit_fails(tmp_path):
    root = _make_tree(tmp_path)
    _add_crate_notices(root, "d375b2d8aaaa", "d375b2d8aaaa", sh_pin="0123456789ab")
    code, out = _run(root)
    assert code == 1
    assert "setup-deepfilter.sh pins DeepFilterNet 0123456789ab" in out
    assert "pins d375b2d8aaaa" in out
    assert "setup-deepfilter.ps1" not in out


def test_setup_ps1_pinning_another_commit_fails(tmp_path):
    root = _make_tree(tmp_path)
    _add_crate_notices(root, "d375b2d8aaaa", "d375b2d8aaaa", ps1_pin="0123456789ab")
    code, out = _run(root)
    assert code == 1
    assert "setup-deepfilter.ps1 pins DeepFilterNet 0123456789ab" in out
    assert "setup-deepfilter.sh" not in out


def test_setup_script_naming_no_pin_fails(tmp_path):
    root = _make_tree(tmp_path)
    _add_crate_notices(root, "d375b2d8aaaa", "d375b2d8aaaa", sh_pin=None, ps1_pin=None)
    code, out = _run(root)
    assert code == 1
    assert "setup-deepfilter.sh names no DeepFilterNet commit" in out
    assert "setup-deepfilter.ps1 names no DeepFilterNet commit" in out


def test_setup_script_missing_fails(tmp_path):
    root = _make_tree(tmp_path)
    _add_crate_notices(root, "d375b2d8aaaa", "d375b2d8aaaa")
    (root / "setup-deepfilter.ps1").unlink()
    code, out = _run(root)
    assert code == 1
    assert "setup-deepfilter.ps1 is missing" in out


def test_named_file_missing_fails(tmp_path):
    root = _make_tree(tmp_path)
    (root / "packaging" / "third-party-licenses" / "GPLv2.txt").unlink()
    code, out = _run(root)
    assert code == 1
    assert "GPLv2.txt" in out
    assert "missing" in out


def test_notice_file_missing_fails(tmp_path):
    root = _make_tree(tmp_path)
    (root / "packaging" / "third-party-licenses" / "foo.txt").unlink()
    code, out = _run(root)
    assert code == 1
    assert "foo.txt" in out


def test_stale_text_fails(tmp_path):
    root = _make_tree(tmp_path)
    (root / "packaging" / "third-party-licenses" / "oldlib.txt").write_text(
        "old\n", encoding="utf-8")
    code, out = _run(root)
    assert code == 1
    assert "oldlib.txt" in out
    assert "named by no row" in out


def test_build_and_deps_dirs_are_not_scanned(tmp_path):
    root = _make_tree(tmp_path)
    for skipped in ("build-x/_deps/qux-src", ".git/modules"):
        d = root / skipped
        d.mkdir(parents=True)
        (d / "CMakeLists.txt").write_text(
            "FetchContent_Declare(qux_upstream URL x)\n", encoding="utf-8")
    code, out = _run(root)
    assert code == 0, out


def test_real_repository_passes():
    code, out = _run(REPO)
    assert code == 0, out


def test_script_runs_from_its_default_root(tmp_path):
    # With no --root the script checks its own repository.
    result = subprocess.run([sys.executable, str(SCRIPT)],
                            capture_output=True, text=True, cwd=tmp_path)
    assert result.returncode == 0, result.stdout + result.stderr
