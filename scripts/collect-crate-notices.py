#!/usr/bin/env python3
"""Write the notices of every Rust crate linked into DeepFilterNet (R-R3-50).

setup-deepfilter.sh and setup-deepfilter.ps1 build the DeepFilterNet
library (libdeepfilter.a, deepfilter.dll) from the pinned DeepFilterNet
commit with cargo. Every crate compiled into it has its own licence. This
script lists them from the checkout's Cargo.lock:

  * `cargo tree -p deep_filter --features ... -e normal,no-proc-macro`
    gives the crates linked into the library: normal dependencies only
    (dev- and build-dependencies, and proc-macro crates and what only they
    use, run on the build machine and are not linked in), with the
    features `cargo cbuild -p deep_filter` selects. `cargo metadata` alone
    is not enough: its resolve unifies features across the whole
    DeepFilterNet workspace, so it also lists crates only pyDF and
    pyDF-data use (hdf5, rayon, jemalloc and others);
  * `--target all` (the default) follows every platform's dependencies,
    so one file covers every package;
  * `cargo metadata` then gives each crate's manifest and source directory;
  * for each crate it writes the name, version and licence expression from
    its manifest, then the licence and notice files from its source
    directory (LICENSE*, LICENCE*, COPYING*, NOTICE*, and the manifest's
    license-file), byte for byte. A text identical to one already written
    is named rather than repeated;
  * a crate that ships no licence or notice file, and has an entry in
    SUPPLEMENTS below (by exact name and version), gets that entry's
    upstream text, byte for byte, marked as not from the crate's source.

Usage (what the setup scripts run after `cargo cbuild`; --offline means it
reads only what the build already fetched):
  python3 scripts/collect-crate-notices.py \\
      --manifest-path <DeepFilterNet>/Cargo.toml --package deep_filter \\
      --features deep_filter/capi --commit <sha> \\
      --output packaging/third-party-licenses/deepfilternet-crates.txt

Or from a saved `cargo metadata --format-version 1` output, with an
optional saved `cargo tree ... --prefix none -f '{p}'` output (without
--tree the metadata's own graph is walked):
  python3 scripts/collect-crate-notices.py --metadata metadata.json \\
      --tree tree.txt ...

The file's header names the DeepFilterNet commit it was generated from;
scripts/check-third-party-licenses.py fails when that commit differs from
third_party/deepfilter/COMMIT.

Exit 0 on success, 1 when cargo fails or the package is missing.
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

_NOTICE_NAME = re.compile(r"^(licen[cs]e|copying|notice)([-_.].*)?$", re.IGNORECASE)


# Licence texts for crates that ship none in their source (R-R3-50). Keyed by
# the exact (name, version) cargo reports, so a new version of either crate
# falls back to "no licence or notice file" until it is looked at again.
# Each text is copied byte for byte from `source` at `pin`; `sha256` is the
# hash of those bytes, checked by tests/compliance/test_crate_notices.py.
# `note` is printed with the text; `authors` asks for the manifest's authors
# after the note (the text itself names no copyright holder).
_CRUNCHY_0_2_2_LICENSE = '''The MIT License (MIT)

Copyright 2017-2019 Vurich.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
'''

_SPDX_MIT = '''MIT License

Copyright (c) <year> <copyright holders>

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
associated documentation files (the "Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the
following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial
portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT
LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO
EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
USE OR OTHER DEALINGS IN THE SOFTWARE.
'''

SUPPLEMENTS: dict[tuple[str, str], dict] = {
    ("crunchy", "0.2.2"): {
        "file": "LICENSE",
        "source": "https://github.com/eira-fransham/crunchy (LICENSE)",
        "pin": "dbc2ec80924bdcc7479f7b5442d9f23c510c9d5b",
        "sha256": "16aff589670d49c45bac12e1dbb9279594d11e5dd9e555541d2e2fcc4b85c5ae",
        "note": "added upstream after 0.2.2 (commit dbc2ec80, 2021); covers 2017-2019",
        "authors": False,
        "text": _CRUNCHY_0_2_2_LICENSE,
    },
    ("realfft", "3.3.0"): {
        "file": "MIT.txt",
        "source": "https://github.com/spdx/license-list-data (text/MIT.txt, tag v3.29.0)",
        "pin": "31ba1a50e5397e00a304dbadc76531740e89ee48",
        "sha256": "b05785f9f18e6716bab63424b11454513b9943a222595b70411009202fc592b5",
        "note": "realfft ships no licence text upstream; its manifest declares MIT; "
                "this is the SPDX list's MIT text",
        "authors": True,
        "text": _SPDX_MIT,
    },
}


def run_cargo_metadata(manifest: Path, features: str | None) -> dict:
    cmd = ["cargo", "metadata", "--format-version", "1", "--locked", "--offline",
           "--manifest-path", str(manifest)]
    if features:
        cmd += ["--features", features]
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        raise SystemExit(f"cargo metadata failed:\n{result.stderr}")
    return json.loads(result.stdout)


def run_cargo_tree(manifest: Path, package: str, features: str | None,
                   target: str) -> str:
    cmd = ["cargo", "tree", "--locked", "--offline",
           "--manifest-path", str(manifest), "-p", package,
           "-e", "normal,no-proc-macro", "--target", target,
           "--prefix", "none", "-f", "{p}"]
    if features:
        cmd += ["--features", features]
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        raise SystemExit(f"cargo tree failed:\n{result.stderr}")
    return result.stdout


def parse_tree(text: str) -> set[tuple[str, str]]:
    """(name, version) of every crate in `cargo tree --prefix none -f '{p}'`."""
    crates: set[tuple[str, str]] = set()
    for line in text.splitlines():
        fields = line.split()
        if len(fields) < 2 or not fields[1].startswith("v"):
            continue
        crates.add((fields[0], fields[1][1:]))
    return crates


def _is_proc_macro(package: dict) -> bool:
    # A proc-macro crate's library target has kind "proc-macro"; its test,
    # bench and example targets have other kinds.
    return any("proc-macro" in t.get("kind", []) for t in package.get("targets", []))


def linked_packages(metadata: dict, root_name: str,
                    tree: set[tuple[str, str]] | None = None) -> list[dict]:
    """Packages linked into root_name's library, root first, then by name.

    With tree (from parse_tree), the crates are exactly those; otherwise
    the metadata's resolve graph is walked.
    """
    packages = {p["id"]: p for p in metadata["packages"]}
    nodes = {n["id"]: n for n in metadata["resolve"]["nodes"]}
    roots = [pid for pid, p in packages.items()
             if p["name"] == root_name and pid in nodes]
    if not roots:
        raise SystemExit(f"package {root_name} is not in the cargo metadata resolve")

    if tree is not None:
        by_key: dict[tuple[str, str], list[str]] = {}
        for pid, p in packages.items():
            by_key.setdefault((p["name"], p["version"]), []).append(pid)
        chosen: list[str] = []
        for key in sorted(tree):
            ids = by_key.get(key)
            if not ids:
                raise SystemExit(
                    f"{key[0]} {key[1]} from cargo tree is not in the cargo metadata")
            if len(ids) > 1:
                raise SystemExit(
                    f"{key[0]} {key[1]} matches more than one cargo metadata package")
            chosen.append(ids[0])
        roots = [pid for pid in roots if pid in chosen]
        if not roots:
            raise SystemExit(f"package {root_name} is not in the cargo tree output")
        root_set = set(roots)
        return [packages[pid] for pid in roots] + sorted(
            (packages[pid] for pid in chosen if pid not in root_set),
            key=lambda p: (p["name"], p["version"]))

    seen: list[str] = []
    stack = list(roots)
    while stack:
        pid = stack.pop()
        if pid in seen:
            continue
        seen.append(pid)
        for dep in nodes[pid].get("deps", []):
            kinds = dep.get("dep_kinds") or [{"kind": None}]
            if not any(k.get("kind") is None for k in kinds):
                continue
            if _is_proc_macro(packages[dep["pkg"]]):
                continue
            stack.append(dep["pkg"])

    root_set = set(roots)
    ordered = [packages[pid] for pid in roots] + sorted(
        (packages[pid] for pid in seen if pid not in root_set),
        key=lambda p: (p["name"], p["version"]))
    return ordered


def notice_files(package: dict) -> list[Path]:
    """Licence and notice files in the crate's source directory."""
    crate_dir = Path(package["manifest_path"]).parent
    found: list[Path] = []
    if crate_dir.is_dir():
        found = [p for p in sorted(crate_dir.iterdir())
                 if p.is_file() and _NOTICE_NAME.match(p.name)]
    extra = package.get("license_file")
    if extra:
        path = (crate_dir / extra).resolve()
        if path.is_file() and path not in [f.resolve() for f in found]:
            found.append(path)
    return found


def _read(path: Path) -> str:
    with open(path, encoding="utf-8", errors="surrogateescape", newline="") as fh:
        return fh.read()


def _source_label(package: dict) -> str:
    source = package.get("source")
    if source is None:
        return "DeepFilterNet workspace"
    if source.startswith("registry+"):
        return "crates.io"
    return source


def supplement_lines(package: dict) -> list[str]:
    """The SUPPLEMENTS entry for a crate that ships no licence text, as lines
    of its entry: a marked heading, the note (and the manifest's authors when
    asked for), then the upstream text byte for byte. Empty when the crate
    has no entry."""
    entry = SUPPLEMENTS.get((package["name"], package["version"]))
    if entry is None:
        return []
    lines = [
        "",
        f"---- {entry['file']}, from upstream, not from the crate's source ----",
        f"Note: {entry['note']}.",
    ]
    if entry["authors"]:
        authors = package.get("authors") or []
        lines.append("Authors (as its Cargo.toml lists them): "
                     + (", ".join(authors) if authors else "(none listed)"))
    lines.append(f"From: {entry['source']} at {entry['pin']}")
    lines.append("")
    lines.append(entry["text"].rstrip("\n"))
    return lines


def render(packages: list[dict], commit: str, command: str,
           target: str = "all") -> str:
    rule = "=" * 72
    lines = [
        "DeepFilterNet Rust crate notices",
        "",
        "The DeepFilterNet library NereusSDR links (libdeepfilter.a on Linux and",
        "macOS, deepfilter.dll on Windows) is compiled from the Rust crates",
        "listed here. Each entry gives the crate, its version, the licence",
        "expression in its manifest, and the licence and notice files from its",
        "source, copied byte for byte. A text identical to one written earlier",
        "in this file is named instead of repeated. Where a crate's source has",
        "no licence text, a text from upstream is added, marked as such, with",
        "its source and commit.",
        "",
        f"DeepFilterNet commit: {commit}",
        "The crates are those locked by that commit's Cargo.lock.",
    ]
    if target == "all":
        lines += [
            "Every platform's dependencies are included, so some crates listed are",
            "compiled only into another platform's package.",
        ]
    else:
        lines.append(f"Only the dependencies of target {target} are included.")
    lines += [
        f"Generated by: {command}",
        f"Crates: {len(packages)}",
        "",
    ]
    written: dict[str, str] = {}
    for package in packages:
        lines.append(rule)
        lines.append(f"{package['name']} {package['version']}")
        lines.append(f"Licence: {package.get('license') or '(none given in its manifest)'}")
        lines.append(f"Source: {_source_label(package)}")
        files = notice_files(package)
        if not files:
            lines.append("")
            lines.append("(no licence or notice file in the crate's source directory)")
            lines += supplement_lines(package)
        for path in files:
            text = _read(path)
            lines.append("")
            first = written.get(text)
            if first is not None:
                lines.append(f"---- {path.name}: identical to {first} above ----")
                continue
            written[text] = f"{path.name} of {package['name']} {package['version']}"
            lines.append(f"---- {path.name} ----")
            lines.append(text.rstrip("\n"))
        lines.append("")
    return "\n".join(lines) + "\n"


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--manifest-path", type=Path)
    source.add_argument("--metadata", type=Path,
                        help="saved `cargo metadata --format-version 1` output")
    parser.add_argument("--package", default="deep_filter")
    parser.add_argument("--features")
    parser.add_argument("--tree", type=Path,
                        help="saved `cargo tree --prefix none -f '{p}'` output "
                             "(with --metadata)")
    parser.add_argument("--target", default="all",
                        help="cargo tree target triple, or all (the default)")
    parser.add_argument("--commit", required=True)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args(argv)

    tree: set[tuple[str, str]] | None = None
    if args.metadata is not None:
        metadata = json.loads(args.metadata.read_text(encoding="utf-8"))
        if args.tree is not None:
            tree = parse_tree(args.tree.read_text(encoding="utf-8"))
    else:
        metadata = run_cargo_metadata(args.manifest_path, args.features)
        tree = parse_tree(run_cargo_tree(args.manifest_path, args.package,
                                         args.features, args.target))
    packages = linked_packages(metadata, args.package, tree)
    command = ("python3 scripts/collect-crate-notices.py --package "
               f"{args.package}" + (f" --features {args.features}" if args.features else "")
               + f" --target {args.target}")
    output = render(packages, args.commit, command, args.target)
    if args.output is None:
        sys.stdout.write(output)
    else:
        with open(args.output, "w", encoding="utf-8", errors="surrogateescape",
                  newline="") as fh:
            fh.write(output)
        print(f"{args.output}: {len(packages)} crates", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
