#!/usr/bin/env python3
"""
Inject thetis-samphire headers into meter button item header files.
All cite MeterManager.cs (Richard Samphire MW0LGE, 2020-2026).
"""

from pathlib import Path

REPO = Path(__file__).parent.parent

PORT_DATE = "2026-04-16"

HEADER_TEMPLATE = """\
// =================================================================
// {filename}  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   {source}
//
// Original Thetis copyright and license (preserved per GNU GPL):
//
//   Thetis is a C# implementation of a Software Defined Radio.
//   Copyright (C) 2020-2026  Richard Samphire (MW0LGE)
//
//   This program is free software; you can redistribute it and/or
//   modify it under the terms of the GNU General Public License
//   as published by the Free Software Foundation; either version 2
//   of the License, or (at your option) any later version.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
// Dual-Licensing Statement (applies ONLY to Richard Samphire MW0LGE's
// contributions — preserved verbatim from Thetis LICENSE-DUAL-LICENSING):
//
//   For any code originally written by Richard Samphire MW0LGE, or for
//   any modifications made by him, the copyright holder for those
//   portions (Richard Samphire) reserves the right to use, license, and
//   distribute such code under different terms, including closed-source
//   and proprietary licences, in addition to the GNU General Public
//   License granted in LICENCE. Nothing in this statement restricts any
//   rights granted to recipients under the GNU GPL.
//
// =================================================================
// Modification history (NereusSDR):
//   {port_date} — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code. Structural template follows AetherSDR
//                 (ten9876/AetherSDR) Qt6 conventions.
// =================================================================
"""

FILES = [
    ("src/gui/meters/AntennaButtonItem.h",
     "Project Files/Source/Console/MeterManager.cs"),
    ("src/gui/meters/BandButtonItem.h",
     "Project Files/Source/Console/MeterManager.cs"),
    ("src/gui/meters/ButtonBoxItem.h",
     "Project Files/Source/Console/MeterManager.cs"),
    ("src/gui/meters/FilterButtonItem.h",
     "Project Files/Source/Console/MeterManager.cs"),
    ("src/gui/meters/ModeButtonItem.h",
     "Project Files/Source/Console/MeterManager.cs"),
    ("src/gui/meters/OtherButtonItem.h",
     "Project Files/Source/Console/MeterManager.cs"),
    ("src/gui/meters/TuneStepButtonItem.h",
     "Project Files/Source/Console/MeterManager.cs"),
    ("src/gui/meters/VfoDisplayItem.h",
     "Project Files/Source/Console/MeterManager.cs"),
]


def inject(rel_path, source):
    path = REPO / rel_path
    if not path.exists():
        print(f"SKIP {rel_path} — not found")
        return

    content = path.read_text(encoding="utf-8")

    # Check if formal header already present
    if "GNU General Public License" in content[:3000]:
        print(f"SKIP {rel_path} — already has formal header")
        return

    header = HEADER_TEMPLATE.format(
        filename=rel_path,
        source=source,
        port_date=PORT_DATE,
    )

    # Prepend after `#pragma once` and any following blank lines
    lines = content.splitlines(keepends=True)
    insert_at = 0
    if lines and lines[0].strip() == "#pragma once":
        insert_at = 1
        while insert_at < len(lines) and lines[insert_at].strip() == "":
            insert_at += 1

    new_content = (
        "".join(lines[:insert_at])
        + "\n"
        + header
        + "\n"
        + "".join(lines[insert_at:])
    )
    path.write_text(new_content, encoding="utf-8")
    print(f"injected: {rel_path}")


def main():
    for rel_path, source in FILES:
        inject(rel_path, source)


if __name__ == "__main__":
    main()
