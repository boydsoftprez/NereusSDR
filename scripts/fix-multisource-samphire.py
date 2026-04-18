#!/usr/bin/env python3
"""
Fix multi-source files that are missing the Dual-Licensing Statement.

The verifier requires it for all multi-source files. Add it to any
multi-source file that doesn't have it yet.
Also fix the missing blank-line separator before the === fence.
"""

from pathlib import Path

REPO = Path(__file__).parent.parent

SAMPHIRE_BLOCK = """\
// Dual-Licensing Statement (applies ONLY to Richard Samphire MW0LGE's
// contributions — preserved verbatim from Thetis LICENSE-DUAL-LICENSING):
//
//   For any code originally written by Richard Samphire MW0LGE, or for
//   any modifications made by him, the copyright holder for those
//   portions (Richard Samphire) reserves the right to use, license, and
//   distribute such code under different terms, including closed-source
//   and proprietary licences, in addition to the GNU General Public
//   License granted in LICENCE. Nothing in this statement restricts any
//   rights granted to recipients under the GNU GPL."""

# Files that need the Dual-Licensing Statement added
FILES_NEED_SAMPHIRE = [
    "src/core/P1RadioConnection.h",
    "src/core/P2RadioConnection.cpp",
    "src/core/P2RadioConnection.h",
    "src/gui/SpectrumWidget.cpp",
    "src/models/SliceModel.cpp",
]


def fix_file(rel_path: str) -> bool:
    path = REPO / rel_path
    if not path.exists():
        print(f"SKIP {rel_path} — file not found")
        return False

    text = path.read_text(encoding="utf-8")

    if "Dual-Licensing Statement" in text:
        print(f"SKIP {rel_path} — already has Dual-Licensing Statement")
        return False

    # Insert Samphire block before the Modification history line.
    # The block should go right before:
    # // =================================================================
    # // Modification history (NereusSDR):
    mod_marker = "// =================================================================\n// Modification history (NereusSDR):"

    if mod_marker not in text:
        print(f"WARN {rel_path} — modification history marker not found")
        return False

    insert_block = "\n" + SAMPHIRE_BLOCK + "\n//\n"
    text = text.replace(mod_marker, insert_block + mod_marker, 1)

    path.write_text(text, encoding="utf-8")
    print(f"fixed: {rel_path}")
    return True


def main():
    for rel_path in FILES_NEED_SAMPHIRE:
        fix_file(rel_path)


if __name__ == "__main__":
    main()
