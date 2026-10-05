#!/usr/bin/env python3
"""The phone's logs keep a Core's reason sentences private (R-IOS-28).

A Core's reason is its own sentence, and some name another device's
address: an older Core's take-over sentence says "Another app at
<address>:<port> connected to the Core and took over."
(src/core/session/SessionEndReasons.cpp:65-69). The app's log goes into the
support bundle, so no os Logger line in the app, its widget, the shared
code or NereusKit may interpolate a reason with `privacy: .public`; a
reason's code, a fixed word, may stay public. An enum's `rawValue` named
reason is the app's own word and is not a Core sentence.

Runs under pytest or alone:
`python3 tests/compliance/test_ios_log_privacy.py`.
"""

from __future__ import annotations

import re
import tempfile
import unittest
from pathlib import Path
from typing import List

REPO = Path(__file__).resolve().parents[2]
IOS = REPO / "ios"

SCANNED_DIRS = ("NereusApp", "NereusActivity", "Shared", "NereusKit/Sources")
SKIPPED_PARTS = {"Tests", "UITests", ".build"}

# One interpolation: \( <expression> , privacy: .public ). The expression
# stops at the first comma outside quotes, which is enough for log lines.
INTERPOLATION = re.compile(r'\\\(((?:[^,()"]|"[^"]*"|\([^()]*\))+),\s*privacy:\s*\.public\b')
# A reason: the identifier reason, or any value's .reason, optionally
# optional-chained or defaulted; never .rawValue of one.
REASON = re.compile(r'(?:^|[.\s?!])reason\b(?!\s*\.\s*rawValue)')


def find_public_reasons(ios: Path) -> List[str]:
    """Every log interpolation of a reason with privacy .public."""
    findings: List[str] = []
    for name in SCANNED_DIRS:
        base = ios / name
        if not base.is_dir():
            continue
        for path in sorted(base.rglob("*.swift")):
            if SKIPPED_PARTS.intersection(path.relative_to(ios).parts):
                continue
            for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
                for match in INTERPOLATION.finditer(line):
                    expression = match.group(1).strip()
                    if REASON.search(expression):
                        shown = "ios/" + path.relative_to(ios).as_posix()
                        findings.append("%s:%d: %s" % (shown, number, expression))
    return findings


class LogPrivacy(unittest.TestCase):
    def test_no_core_reason_is_logged_public(self) -> None:
        self.assertEqual(find_public_reasons(IOS), [])

    def test_the_scan_finds_each_shape(self) -> None:
        lines = {
            "NereusApp/A.swift": 'log.info("ended: \\(end.reason, privacy: .public)")\n',
            "NereusApp/B.swift": 'log.info("kept: \\(pin?.reason ?? "", privacy: .public)")\n',
            "NereusApp/C.swift": 'log.info("refused: \\(reason, privacy: .public)")\n',
            "NereusKit/Sources/X/D.swift":
                'log.info("refused \\(verb, privacy: .public): \\(result.reason, privacy: .public)")\n',
        }
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            for relative, text in lines.items():
                path = root / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(text, encoding="utf-8")
            found = "\n".join(find_public_reasons(root))
        for expected in ("A.swift:1: end.reason", 'B.swift:1: pin?.reason ?? ""', "C.swift:1: reason",
                         "D.swift:1: result.reason"):
            self.assertIn(expected, found)
        self.assertNotIn("verb", found)

    def test_private_reasons_codes_and_raw_values_pass(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / "NereusApp").mkdir()
            (root / "NereusApp" / "A.swift").write_text(
                'log.info("ended (\\(end.code ?? "none", privacy: .public)): \\(end.reason, privacy: .private)")\n'
                'log.notice("Media restarted: \\(reason.rawValue, privacy: .public)")\n'
                'log.info("\\(reasonCount, privacy: .public)")\n', encoding="utf-8")
            (root / "NereusApp" / "Tests").mkdir()
            (root / "NereusApp" / "Tests" / "T.swift").write_text(
                'log.info("\\(end.reason, privacy: .public)")\n', encoding="utf-8")
            self.assertEqual(find_public_reasons(root), [])


if __name__ == "__main__":
    unittest.main()
