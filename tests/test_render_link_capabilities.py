#!/usr/bin/env python3
# no-port-check: NereusSDR-original.
"""Focused watch-capability rendering validation; Python standard library only.

Run with --renderer pointing explicitly at the candidate or integrated script.
"""
# Modification history (NereusSDR):
#   2026-10-04  J.J. Boyd / KG4VCF  Watch declaration and strict validation
#                                  regression coverage. AI-assisted via
#                                  OpenAI Codex.

import argparse
import importlib.util
from pathlib import Path
import sys
import unittest

sys.dont_write_bytecode = True
RENDERER = None


class CapabilityRenderingTest(unittest.TestCase):
    def renderers(self):
        return (RENDERER.render_capabilities, RENDERER.render_capability_versions)

    def assert_refused(self, entry, message):
        for renderer in self.renderers():
            with self.subTest(renderer=renderer.__name__, entry=entry):
                with self.assertRaisesRegex(RENDERER.SurfaceError, message):
                    renderer([entry])

    def test_absent_watch_value_is_explicit_and_declaration_retained(self):
        watch = {"name": "txWatchPathVersion", "kind": "i64"}
        self.assertIn("| 1 | `txWatchPathVersion` | `i64` |",
                      RENDERER.render_capabilities([watch]))
        versions = RENDERER.render_capability_versions([watch])
        self.assertIn("Value advertised by capture fixture", versions)
        self.assertIn("| `txWatchPathVersion` | not advertised by this fixture |", versions)
        self.assertNotIn("| `txWatchPathVersion` | 1 |", versions)
        self.assertNotIn("value", watch)  # Rendering never fills in a value.

    def test_observed_watch_value_is_rendered_when_supplied(self):
        versions = RENDERER.render_capability_versions([
            {"name": "txWatchPathVersion", "kind": "i64", "value": 1}])
        self.assertIn("| `txWatchPathVersion` | 1 |", versions)
        self.assertNotIn("not advertised by this fixture", versions)

    def test_ordinary_capabilities_still_require_live_values(self):
        for name, kind in (("remoteTxVersion", "i64"), ("radioConnected", "bool"),
                           ("txRefusalReason", "utf8"), ("futureWatchVersion", "i64")):
            with self.subTest(name=name):
                self.assert_refused({"name": name, "kind": kind}, "missing field.*value")

    def test_watch_requires_i64_even_when_value_is_present(self):
        for kind in ("bool", "utf8", "f64", "enum", "unsupported"):
            for has_value in (False, True):
                entry = {"name": "txWatchPathVersion", "kind": kind}
                if has_value:
                    entry["value"] = 1
                self.assert_refused(entry, "requires wire kind i64")

    def test_unknown_fields_are_rejected_for_watch_and_ordinary(self):
        for entry in ({"name": "txWatchPathVersion", "kind": "i64", "error": "missing"},
                      {"name": "txWatchPathVersion", "kind": "i64", "invented": True},
                      {"name": "remoteTxVersion", "kind": "i64", "value": 2, "extra": 1}):
            self.assert_refused(entry, "unknown field")

    def test_name_and_kind_remain_required(self):
        for entry in ({"kind": "i64"}, {"name": "txWatchPathVersion"}, {}):
            self.assert_refused(entry, "missing field")

    def test_nonobject_entries_are_rejected(self):
        for entry in (None, [], "txWatchPathVersion"):
            self.assert_refused(entry, "expected an object")

    def test_ordinary_observed_values_keep_their_rendering(self):
        entries = [{"name": "remoteTxVersion", "kind": "i64", "value": 2},
                   {"name": "txPermitted", "kind": "bool", "value": False}]
        versions = RENDERER.render_capability_versions(entries)
        self.assertIn("| `remoteTxVersion` | 2 |", versions)
        self.assertIn("| `txPermitted` | false |", versions)
        self.assertNotIn("not advertised by this fixture", versions)


def main():
    global RENDERER
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--renderer", type=Path, required=True)
    args = parser.parse_args()
    spec = importlib.util.spec_from_file_location("link_table_renderer", args.renderer)
    RENDERER = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(RENDERER)
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(CapabilityRenderingTest)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    sys.exit(main())
