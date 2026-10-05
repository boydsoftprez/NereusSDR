#!/usr/bin/env python3
"""Tests for scripts/verify-ios-provenance.py (R-IOS-29).

Each case builds a temporary repository tree with an ios/ directory, runs the
real checker as a subprocess with --root pointing at it, and asserts the exit
code and the message naming the file and the rule.
"""

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from typing import Optional

PROJECT = Path(__file__).resolve().parents[2]
CHECKER = PROJECT / "scripts" / "verify-ios-provenance.py"

HEADER = (
    "// NereusSDR for iOS: a test source file\n"
    "// SPDX-License-Identifier: GPL-3.0-or-later WITH "
    "AdditionRef-NereusSDR-AppStore-permission\n"
)

TABLE_HEAD = (
    "# Third-party code\n\n"
    "| Name | Version or commit | Archive URL | Archive SHA-256 | Licence | Path |\n"
    "| --- | --- | --- | --- | --- | --- |\n"
)

GOOD_SHA = "a" * 64


def table_row(name: str, licence: str, sha: str, path: str) -> str:
    return ("| " + name + " | 1.0 | https://example.invalid/" + name
            + ".zip | " + sha + " | " + licence + " | `" + path + "` |\n")


LICENSES_JSON = "ios/NereusApp/Resources/Licenses.json"


def licenses(*entries: tuple) -> str:
    """Licenses.json text for (name, licence, notice) entries."""
    return json.dumps({"libraries": [
        {"name": name, "version": "1.0", "licence": licence, "notice": notice}
        for name, licence, notice in entries
    ]})


class Tree:
    def __init__(self, rows: str = "") -> None:
        self._temp = tempfile.TemporaryDirectory()
        self.root = Path(self._temp.name)
        self.write("ios/THIRD-PARTY.md", TABLE_HEAD + rows)
        self.write("ios/NereusKit/Sources/NereusModels/Frequency.swift",
                   HEADER + "public struct Frequency {}\n")

    def write(self, relative: str, text: str) -> None:
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")

    def run(self) -> subprocess.CompletedProcess:
        return subprocess.run(
            [sys.executable, str(CHECKER), "--root", str(self.root)],
            capture_output=True, text=True, check=False,
        )

    def close(self) -> None:
        self._temp.cleanup()


class VerifyIosProvenanceTest(unittest.TestCase):
    def setUp(self) -> None:
        self.tree: Optional[Tree] = None

    def tearDown(self) -> None:
        if self.tree is not None:
            self.tree.close()

    def make(self, rows: str = "") -> Tree:
        self.tree = Tree(rows)
        return self.tree

    def assertFails(self, tree: Tree, *fragments: str) -> None:
        result = tree.run()
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        for fragment in fragments:
            self.assertIn(fragment, result.stderr)

    def assertPasses(self, tree: Tree) -> None:
        result = tree.run()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    # A clean tree ---------------------------------------------------------

    def test_clean_tree_passes(self) -> None:
        tree = self.make()
        tree.write("ios/NereusKit/Package.swift",
                   "// swift-tools-version:6.0\n" + HEADER + "import PackageDescription\n")
        tree.write("ios/README.md", "Nothing from Thetis goes in here.\n")
        self.assertPasses(tree)

    def test_real_tree_passes(self) -> None:
        result = subprocess.run(
            [sys.executable, str(CHECKER)],
            capture_output=True, text=True, check=False,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    # header ---------------------------------------------------------------

    def test_missing_header_fails_for_each_source_kind(self) -> None:
        for suffix in (".swift", ".c", ".h", ".m", ".metal", ".cpp"):
            with self.subTest(suffix=suffix):
                tree = self.make()
                relative = "ios/NereusKit/Sources/Thing/thing" + suffix
                tree.write(relative, "int x;\n")
                self.assertFails(tree, relative + ": header:")
                tree.close()
                self.tree = None

    def test_wrong_spdx_line_fails(self) -> None:
        tree = self.make()
        tree.write("ios/NereusApp/App.swift",
                   "// NereusSDR for iOS: the app\n"
                   "// SPDX-License-Identifier: GPL-3.0-or-later\n")
        self.assertFails(tree, "ios/NereusApp/App.swift: header: line 2")

    def test_empty_purpose_fails(self) -> None:
        tree = self.make()
        tree.write("ios/NereusApp/App.swift",
                   "// NereusSDR for iOS: \n" + HEADER.splitlines(True)[1])
        self.assertFails(tree, "ios/NereusApp/App.swift: header: line 1")

    def test_tools_version_line_only_allowed_in_package_swift(self) -> None:
        tree = self.make()
        tree.write("ios/NereusApp/App.swift",
                   "// swift-tools-version:6.0\n" + HEADER)
        self.assertFails(tree, "ios/NereusApp/App.swift: header:")

    def test_vendored_directory_is_exempt_from_header(self) -> None:
        tree = self.make(table_row("opus", "BSD-3-Clause", GOOD_SHA,
                                   "NereusKit/Sources/COpus"))
        tree.write(LICENSES_JSON, licenses(("opus", "BSD-3-Clause", "the notice")))
        tree.write("ios/NereusKit/Sources/COpus/src/opus.c",
                   "/* Copyright (C) Xiph.Org */\nint opus;\n")
        self.assertPasses(tree)

    def test_sibling_of_vendored_directory_is_not_exempt(self) -> None:
        tree = self.make(table_row("opus", "BSD-3-Clause", GOOD_SHA,
                                   "ios/NereusKit/Sources/COpus/"))
        tree.write("ios/NereusKit/Sources/COpusShim/shim.c", "int shim;\n")
        self.assertFails(tree, "ios/NereusKit/Sources/COpusShim/shim.c: header:")

    def test_build_output_is_skipped(self) -> None:
        tree = self.make()
        tree.write("ios/NereusKit/.build/debug/generated.swift", "int x;\n")
        tree.write("ios/DerivedData/x/generated.m", "int x;\n")
        self.assertPasses(tree)

    # upstream -------------------------------------------------------------

    def test_upstream_names_fail(self) -> None:
        for name in ("Thetis", "WDSP", "wdsp", "AetherSDR", "FlexRadio",
                     "NR0V", "MW0LGE", "Warren Pratt"):
            with self.subTest(name=name):
                tree = self.make()
                relative = "ios/NereusKit/Sources/NereusBand/Band.swift"
                tree.write(relative, HEADER + "// as " + name + " does it\n")
                self.assertFails(tree, relative + ": upstream: names " + name)
                tree.close()
                self.tree = None

    def test_copyright_line_fails(self) -> None:
        tree = self.make()
        relative = "ios/NereusKit/Sources/NereusMedia/Decoder.swift"
        tree.write(relative, HEADER + "// Copyright (C) 2026 Someone\n")
        self.assertFails(tree, relative + ": upstream: carries a Copyright (C) line")

    # licence and sha256 ----------------------------------------------------

    def test_disallowed_licence_fails(self) -> None:
        tree = self.make(table_row("gplthing", "GPL-2.0-or-later", GOOD_SHA,
                                   "NereusKit/Sources/CGpl"))
        self.assertFails(tree, "ios/THIRD-PARTY.md: row gplthing: licence: GPL-2.0-or-later")

    def test_each_allowed_licence_passes(self) -> None:
        for licence in ("BSD-2-Clause", "BSD-3-Clause", "MIT", "ISC",
                        "Apache-2.0", "MPL-2.0"):
            with self.subTest(licence=licence):
                tree = self.make(table_row("lib", licence, GOOD_SHA,
                                           "NereusKit/Sources/CLib"))
                tree.write(LICENSES_JSON, licenses(("lib", licence, "the notice")))
                self.assertPasses(tree)
                tree.close()
                self.tree = None

    def test_missing_sha256_fails(self) -> None:
        tree = self.make(table_row("sodium", "ISC", "",
                                   "NereusKit/Sources/CSodium"))
        self.assertFails(tree, "ios/THIRD-PARTY.md: row sodium: sha256:")

    def test_malformed_sha256_fails(self) -> None:
        tree = self.make(table_row("sodium", "ISC", "abc123",
                                   "NereusKit/Sources/CSodium"))
        self.assertFails(tree, "ios/THIRD-PARTY.md: row sodium: sha256:")

    # fixtures -------------------------------------------------------------

    def test_fixture_reference_outside_tests_fails(self) -> None:
        tree = self.make()
        relative = "ios/NereusApp/Loader.swift"
        tree.write(relative, HEADER + 'let p = "../tests/data/link/v1"\n')
        self.assertFails(tree, relative + ": fixtures:")

    def test_fixture_reference_in_non_source_file_fails(self) -> None:
        tree = self.make()
        tree.write("ios/project.yml", "resources: [../tests/data/link]\n")
        self.assertFails(tree, "ios/project.yml: fixtures:")

    def test_fixture_reference_in_tests_passes(self) -> None:
        tree = self.make()
        tree.write("ios/NereusKit/Tests/NereusMediaTests/LinkFixtureLoader.swift",
                   HEADER + 'let p = "tests/data/link/v1"\n')
        tree.write("ios/NereusAppTests/Fixtures.swift",
                   HEADER + 'let p = "tests/data/link/v1"\n')
        tree.write("ios/scripts/interop-test.sh", "ls tests/data/link/v1\n")
        self.assertPasses(tree)


    # licences -------------------------------------------------------------

    def two_rows(self) -> Tree:
        return self.make(
            table_row("opus", "BSD-3-Clause", GOOD_SHA, "NereusKit/Sources/COpus")
            + table_row("plog", "MIT", GOOD_SHA, "NereusKit/Sources/CPlog"))

    def test_licenses_json_listing_every_row_passes(self) -> None:
        tree = self.two_rows()
        tree.write("ios/NereusKit/Sources/CPlog/LICENSE", "MIT License\n")
        tree.write(LICENSES_JSON, licenses(("opus", "BSD-3-Clause", "Opus notice"),
                                           ("plog", "MIT", "MIT License\n")))
        self.assertPasses(tree)

    def test_row_missing_from_licenses_json_fails(self) -> None:
        tree = self.two_rows()
        tree.write(LICENSES_JSON, licenses(("opus", "BSD-3-Clause", "Opus notice")))
        self.assertFails(tree, LICENSES_JSON + ": licences: no entry for the "
                         "ios/THIRD-PARTY.md row plog")

    def test_missing_licenses_json_fails_when_rows_exist(self) -> None:
        tree = self.two_rows()
        self.assertFails(tree, LICENSES_JSON + ": licences: file is missing")

    def test_entry_without_row_fails(self) -> None:
        tree = self.two_rows()
        tree.write(LICENSES_JSON, licenses(("opus", "BSD-3-Clause", "n"),
                                           ("plog", "MIT", "n"),
                                           ("sodium", "ISC", "n")))
        self.assertFails(tree, LICENSES_JSON + ": licences: sodium names no "
                         "ios/THIRD-PARTY.md row")

    def test_entry_with_another_licence_fails(self) -> None:
        tree = self.two_rows()
        tree.write(LICENSES_JSON, licenses(("opus", "BSD-3-Clause", "n"),
                                           ("plog", "ISC", "n")))
        self.assertFails(tree, LICENSES_JSON + ": licences: plog gives licence ISC")

    def test_empty_notice_fails(self) -> None:
        tree = self.two_rows()
        tree.write(LICENSES_JSON, licenses(("opus", "BSD-3-Clause", " "),
                                           ("plog", "MIT", "n")))
        self.assertFails(tree, LICENSES_JSON + ": licences: opus has no notice text")

    def test_notice_other_than_licence_file_fails(self) -> None:
        tree = self.two_rows()
        tree.write("ios/NereusKit/Sources/COpus/COPYING", "Opus licence, current\n")
        tree.write(LICENSES_JSON, licenses(("opus", "BSD-3-Clause", "Opus licence, old\n"),
                                           ("plog", "MIT", "n")))
        self.assertFails(tree, LICENSES_JSON + ": licences: opus's notice differs "
                         "from ios/NereusKit/Sources/COpus/COPYING")

    def test_notice_with_its_source_notices_passes(self) -> None:
        tree = self.two_rows()
        tree.write("ios/NereusKit/Sources/COpus/COPYING", "Opus licence\n")
        tree.write("packaging/third-party-licenses/opus-notices.txt", "Opus source notices\n")
        tree.write(LICENSES_JSON, licenses(
            ("opus", "BSD-3-Clause", "Opus licence\n\nOpus source notices\n"),
            ("plog", "MIT", "n")))
        self.assertPasses(tree)

    def test_notice_without_its_source_notices_fails(self) -> None:
        tree = self.two_rows()
        tree.write("ios/NereusKit/Sources/COpus/COPYING", "Opus licence\n")
        tree.write("packaging/third-party-licenses/opus-notices.txt", "Opus source notices\n")
        tree.write(LICENSES_JSON, licenses(("opus", "BSD-3-Clause", "Opus licence\n"),
                                           ("plog", "MIT", "n")))
        self.assertFails(tree, LICENSES_JSON + ": licences: opus's notice differs "
                         "from ios/NereusKit/Sources/COpus/COPYING followed by "
                         "packaging/third-party-licenses/opus-notices.txt")

    def test_notices_file_is_found_by_the_name_with_hyphens(self) -> None:
        tree = self.make(table_row("Mbed TLS", "Apache-2.0", GOOD_SHA, "NereusKit/Sources/CMbedTLS"))
        tree.write("ios/NereusKit/Sources/CMbedTLS/LICENSE", "Apache\n")
        tree.write("packaging/third-party-licenses/mbed-tls-notices.txt", "Mbed notices\n")
        tree.write(LICENSES_JSON, licenses(("Mbed TLS", "Apache-2.0", "Apache\n")))
        self.assertFails(tree, "Mbed TLS's notice differs from ios/NereusKit/Sources/CMbedTLS/LICENSE "
                         "followed by packaging/third-party-licenses/mbed-tls-notices.txt")

    def test_unreadable_licenses_json_fails(self) -> None:
        tree = self.two_rows()
        tree.write(LICENSES_JSON, "{not json")
        self.assertFails(tree, LICENSES_JSON + ": licences: not readable JSON")

    # wordlist -------------------------------------------------------------

    WORD_COPY = "ios/NereusKit/Sources/NereusLink/Resources/pairing-words-v1.txt"

    def test_word_list_copy_equal_to_the_cores_passes(self) -> None:
        tree = self.make()
        tree.write("resources/pairing-words-v1.txt", "anvil\nharbor\n")
        tree.write(self.WORD_COPY, "anvil\nharbor\n")
        self.assertPasses(tree)

    def test_word_list_copy_that_differs_fails(self) -> None:
        tree = self.make()
        tree.write("resources/pairing-words-v1.txt", "anvil\nharbor\n")
        tree.write(self.WORD_COPY, "anvil\nharbour\n")
        self.assertFails(tree, self.WORD_COPY + ": wordlist: differs from the Core's "
                         "resources/pairing-words-v1.txt")

    def test_missing_word_list_copy_fails(self) -> None:
        tree = self.make()
        tree.write("resources/pairing-words-v1.txt", "anvil\nharbor\n")
        self.assertFails(tree, self.WORD_COPY + ": wordlist: missing; the app needs its copy "
                         "of the Core's resources/pairing-words-v1.txt")

    def test_word_list_copy_without_the_cores_fails(self) -> None:
        tree = self.make()
        tree.write(self.WORD_COPY, "anvil\nharbor\n")
        self.assertFails(tree, self.WORD_COPY + ": wordlist: the Core's "
                         "resources/pairing-words-v1.txt is missing")


if __name__ == "__main__":
    unittest.main()
