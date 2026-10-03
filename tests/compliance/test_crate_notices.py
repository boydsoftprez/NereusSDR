"""Tests for scripts/collect-crate-notices.py (R-R3-50).

A hand-written `cargo metadata` document and crate directories in tmp_path
stand in for a DeepFilterNet checkout; cargo is not run.
"""
import importlib.util
import json
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent.parent
CRATE_SCRIPT = REPO / "scripts" / "collect-crate-notices.py"

_spec = importlib.util.spec_from_file_location("collect_crate_notices", CRATE_SCRIPT)
cn = importlib.util.module_from_spec(_spec)
sys.modules["collect_crate_notices"] = cn
_spec.loader.exec_module(cn)


APACHE = "                                 Apache License\n   Version 2.0\n"


def _crate(tmp_path: Path, name: str, files: dict[str, bytes], **extra) -> dict:
    crate_dir = tmp_path / "registry" / f"{name}-1.0.0"
    crate_dir.mkdir(parents=True)
    (crate_dir / "Cargo.toml").write_text(f'[package]\nname = "{name}"\n')
    for file_name, data in files.items():
        (crate_dir / file_name).write_bytes(data)
    package = {
        "id": f"{name} 1.0.0",
        "name": name,
        "version": "1.0.0",
        "license": "MIT OR Apache-2.0",
        "source": "registry+https://github.com/rust-lang/crates.io-index",
        "manifest_path": str(crate_dir / "Cargo.toml"),
        "targets": [{"kind": ["lib"]}],
    }
    package.update(extra)
    return package


def _dep(pkg: str, kind=None, target=None) -> dict:
    return {"name": pkg, "pkg": f"{pkg} 1.0.0",
            "dep_kinds": [{"kind": kind, "target": target}]}


def _metadata(tmp_path: Path) -> dict:
    packages = [
        _crate(tmp_path, "deep_filter", {"README.md": b"not a licence\n"},
               source=None, license=None),
        _crate(tmp_path, "linked", {"LICENSE-MIT": b"MIT text\r\nCopyright A\r\n",
                                    "LICENSE-APACHE": APACHE.encode()}),
        _crate(tmp_path, "winonly", {"LICENSE-APACHE": APACHE.encode(),
                                     "NOTICE": b"winonly notice\n"}),
        _crate(tmp_path, "devonly", {"LICENSE": b"dev\n"}),
        _crate(tmp_path, "buildonly", {"LICENSE": b"build\n"}),
        _crate(tmp_path, "derive", {"LICENSE": b"macro\n"},
               targets=[{"kind": ["proc-macro"]}]),
        # A proc-macro crate with a test target, as hdf5-derive has.
        _crate(tmp_path, "derive_tested", {"LICENSE": b"macro2\n"},
               targets=[{"kind": ["proc-macro"]}, {"kind": ["test"]}]),
        # Linked only when another workspace member's features are unified in.
        _crate(tmp_path, "unified", {"LICENSE": b"unified\n"}),
        _crate(tmp_path, "custom", {"legal.txt": b"custom licence file\n"},
               license=None, license_file="legal.txt"),
    ]
    nodes = [
        {"id": "deep_filter 1.0.0", "deps": [
            _dep("linked"), _dep("devonly", "dev"), _dep("buildonly", "build"),
            _dep("derive"), _dep("derive_tested"), _dep("custom"),
            _dep("unified")]},
        {"id": "linked 1.0.0", "deps": [_dep("winonly", None, "cfg(windows)")]},
        {"id": "winonly 1.0.0", "deps": []},
        {"id": "devonly 1.0.0", "deps": []},
        {"id": "buildonly 1.0.0", "deps": []},
        {"id": "derive 1.0.0", "deps": []},
        {"id": "custom 1.0.0", "deps": []},
        {"id": "derive_tested 1.0.0", "deps": []},
        {"id": "unified 1.0.0", "deps": []},
    ]
    return {"packages": packages, "resolve": {"nodes": nodes}}


# `cargo tree --prefix none -f '{p}'` output for the same graph, without the
# crate only another workspace member's features pull in.
TREE = """\
deep_filter v1.0.0 (/some/checkout/libDF)
custom v1.0.0
linked v1.0.0
winonly v1.0.0
linked v1.0.0 (*)
"""


def test_only_linked_crates_are_listed(tmp_path):
    names = [p["name"] for p in cn.linked_packages(_metadata(tmp_path), "deep_filter")]
    # The metadata walk cannot tell unified features apart; proc-macro
    # crates are left out even when they have test targets.
    assert names == ["deep_filter", "custom", "linked", "unified", "winonly"]


def test_cargo_tree_output_selects_the_crates(tmp_path):
    tree = cn.parse_tree(TREE)
    assert tree == {("deep_filter", "1.0.0"), ("custom", "1.0.0"),
                    ("linked", "1.0.0"), ("winonly", "1.0.0")}
    names = [p["name"] for p in
             cn.linked_packages(_metadata(tmp_path), "deep_filter", tree)]
    assert names == ["deep_filter", "custom", "linked", "winonly"]


def test_crate_from_tree_missing_in_metadata_is_an_error(tmp_path):
    tree = cn.parse_tree(TREE + "ghost v9.9.9\n")
    try:
        cn.linked_packages(_metadata(tmp_path), "deep_filter", tree)
    except SystemExit as exc:
        assert "ghost 9.9.9" in str(exc)
    else:
        raise AssertionError("expected SystemExit")


def test_crate_file_lists_versions_licences_and_texts(tmp_path):
    metadata_file = tmp_path / "metadata.json"
    metadata_file.write_text(json.dumps(_metadata(tmp_path)))
    tree_file = tmp_path / "tree.txt"
    tree_file.write_text(TREE)
    out = tmp_path / "crates.txt"
    result = subprocess.run(
        [sys.executable, str(CRATE_SCRIPT), "--metadata", str(metadata_file),
         "--tree", str(tree_file), "--commit", "d375b2d8", "--output", str(out)],
        capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    data = out.read_bytes()
    text = data.decode("utf-8")
    assert "Crates: 4" in text
    assert "\nDeepFilterNet commit: d375b2d8\n" in text
    assert "Every platform's dependencies are included" in text
    assert "--target all" in text
    # No local path from the machine that generated it.
    assert str(tmp_path) not in text
    assert "linked 1.0.0\nLicence: MIT OR Apache-2.0\nSource: crates.io" in text
    # Byte for byte, CRLF included.
    assert b"---- LICENSE-MIT ----\nMIT text\r\nCopyright A\r\n" in data
    # A text already written is named, not repeated.
    assert text.count("Apache License") == 1
    assert "---- LICENSE-APACHE: identical to LICENSE-APACHE of linked 1.0.0 above ----" in text
    assert "---- NOTICE ----\nwinonly notice" in text
    assert "---- legal.txt ----\ncustom licence file" in text
    assert "Licence: (none given in its manifest)" in text
    assert "deep_filter 1.0.0\nLicence: (none given in its manifest)\n" \
           "Source: DeepFilterNet workspace\n\n" \
           "(no licence or notice file in the crate's source directory)" in text
    for absent in ("devonly", "buildonly", "derive", "unified"):
        assert absent not in text


def test_missing_package_is_an_error(tmp_path):
    metadata_file = tmp_path / "metadata.json"
    metadata_file.write_text(json.dumps(_metadata(tmp_path)))
    result = subprocess.run(
        [sys.executable, str(CRATE_SCRIPT), "--metadata", str(metadata_file),
         "--package", "nope", "--commit", "x"], capture_output=True, text=True)
    assert result.returncode == 1
    assert "nope" in result.stderr


# ── Upstream texts for crates that ship none (R-R3-50) ──────────────────

def test_supplement_texts_are_the_pinned_bytes():
    import hashlib
    assert set(cn.SUPPLEMENTS) == {("crunchy", "0.2.2"), ("realfft", "3.3.0")}
    for key, entry in cn.SUPPLEMENTS.items():
        digest = hashlib.sha256(entry["text"].encode("utf-8")).hexdigest()
        assert digest == entry["sha256"], key
        assert len(entry["pin"]) == 40, key
        assert entry["source"].startswith("https://github.com/"), key


def _no_licence_crate(tmp_path, name, version, authors=None):
    package = _crate(tmp_path, f"{name}-{version}", {"README.md": b"readme\n"},
                     license="MIT")
    package["name"] = name
    package["version"] = version
    package["id"] = f"{name} {version}"
    if authors is not None:
        package["authors"] = authors
    return package


def test_supplements_are_written_marked_for_crates_without_a_text(tmp_path):
    packages = [
        _no_licence_crate(tmp_path, "crunchy", "0.2.2", ["Vurich <jackefransham@hotmail.co.uk>"]),
        _no_licence_crate(tmp_path, "realfft", "3.3.0", ["HEnquist <henrik.enquist@gmail.com>"]),
        # Another version of a supplemented crate gets no supplement.
        _no_licence_crate(tmp_path, "realfft", "3.4.0", ["HEnquist <henrik.enquist@gmail.com>"]),
    ]
    text = cn.render(packages, "d375b2d8", "cmd")
    crunchy = cn.SUPPLEMENTS[("crunchy", "0.2.2")]
    mit = cn.SUPPLEMENTS[("realfft", "3.3.0")]
    assert ("crunchy 0.2.2\nLicence: MIT\nSource: crates.io\n\n"
            "(no licence or notice file in the crate's source directory)\n\n"
            "---- LICENSE, from upstream, not from the crate's source ----\n"
            "Note: added upstream after 0.2.2 (commit dbc2ec80, 2021); covers 2017-2019.\n"
            "From: https://github.com/eira-fransham/crunchy (LICENSE) at "
            "dbc2ec80924bdcc7479f7b5442d9f23c510c9d5b\n\n"
            + crunchy["text"]) in text
    assert "Copyright 2017-2019 Vurich." in text
    assert ("realfft 3.3.0\nLicence: MIT\nSource: crates.io\n\n"
            "(no licence or notice file in the crate's source directory)\n\n"
            "---- MIT.txt, from upstream, not from the crate's source ----\n"
            "Note: realfft ships no licence text upstream; its manifest declares MIT; "
            "this is the SPDX list's MIT text.\n"
            "Authors (as its Cargo.toml lists them): HEnquist <henrik.enquist@gmail.com>\n"
            "From: https://github.com/spdx/license-list-data (text/MIT.txt, tag v3.29.0) at "
            "31ba1a50e5397e00a304dbadc76531740e89ee48\n\n"
            + mit["text"]) in text
    assert text.endswith("realfft 3.4.0\nLicence: MIT\nSource: crates.io\n\n"
                         "(no licence or notice file in the crate's source directory)\n\n")
    assert text.count("---- MIT.txt, from upstream") == 1


def test_supplement_is_not_added_when_the_crate_ships_a_text(tmp_path):
    package = _crate(tmp_path, "crunchy-shipped", {"LICENSE": b"its own\n"}, license="MIT")
    package["name"], package["version"] = "crunchy", "0.2.2"
    text = cn.render([package], "d375b2d8", "cmd")
    assert "---- LICENSE ----\nits own" in text
    assert "from upstream, not from the crate's source" not in text


def test_committed_crate_file_holds_the_supplements_as_generated():
    # The committed file is what the generator writes for these two crates,
    # so a regeneration keeps them.
    committed = (REPO / "packaging" / "third-party-licenses"
                 / "deepfilternet-crates.txt").read_text(encoding="utf-8")
    for name, version, authors in (
            ("crunchy", "0.2.2", ["Vurich <jackefransham@hotmail.co.uk>"]),
            ("realfft", "3.3.0", ["HEnquist <henrik.enquist@gmail.com>"])):
        package = {"name": name, "version": version, "authors": authors}
        block = "\n".join(
            ["(no licence or notice file in the crate's source directory)"]
            + cn.supplement_lines(package))
        assert block + "\n\n" in committed, name
