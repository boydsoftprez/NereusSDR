#!/usr/bin/env python3
"""
Manually inject headers into the 3 files the inserter missed because they
already had inline 'Ported from' references in class comments.
"""

from pathlib import Path

REPO = Path(__file__).parent.parent

GPL_BLOCK = """\
//   This program is free software; you can redistribute it and/or
//   modify it under the terms of the GNU General Public License
//   as published by the Free Software Foundation; either version 2
//   of the License, or (at your option) any later version.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details."""

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

PORT_DATE = "2026-04-16"


def make_header(filename, sources, contributors, has_samphire):
    src_lines = "\n".join(f"//   {s}" for s in sources)
    contrib_lines = "\n".join(contributors)
    samphire_section = f"\n{SAMPHIRE_BLOCK}\n//" if has_samphire else ""
    return (
        f"// =================================================================\n"
        f"// {filename}  (NereusSDR)\n"
        f"// =================================================================\n"
        f"//\n"
        f"// Ported from multiple Thetis sources:\n"
        f"{src_lines}\n"
        f"//\n"
        f"// Original Thetis copyright and license (preserved per GNU GPL,\n"
        f"// representing the union of contributors across all cited sources):\n"
        f"//\n"
        f"//   Thetis is a C# implementation of a Software Defined Radio.\n"
        f"{contrib_lines}\n"
        f"//\n"
        f"{GPL_BLOCK}\n"
        f"//{samphire_section}\n"
        f"// =================================================================\n"
        f"// Modification history (NereusSDR):\n"
        f"//   {PORT_DATE} — Synthesized in C++20/Qt6 for NereusSDR by J.J. Boyd\n"
        f"//                 (KG4VCF), with AI-assisted transformation via Anthropic\n"
        f"//                 Claude Code. Combines logic from the Thetis sources\n"
        f"//                 listed above.\n"
        f"// =================================================================\n"
    )


HEADERS = {
    "src/core/RxChannel.h": make_header(
        "src/core/RxChannel.h",
        [
            "Project Files/Source/Console/radio.cs",
            "Project Files/Source/Console/console.cs",
            "Project Files/Source/Console/dsp.cs",
            "Project Files/Source/Console/HPSDR/specHPSDR.cs",
            "Project Files/Source/ChannelMaster/cmaster.c",
        ],
        [
            "//   Copyright (C) 2004-2009  FlexRadio Systems",
            "//   Copyright (C) 2010-2020  Doug Wigley (W5WC)",
            "//   Copyright (C) 2013-2017  Warren Pratt (NR0V) [dsp.cs / cmaster.c]",
            "//   Copyright (C) 2019-2026  Richard Samphire (MW0LGE) — heavily modified",
        ],
        has_samphire=True,
    ),
    "src/core/WdspEngine.h": make_header(
        "src/core/WdspEngine.h",
        [
            "Project Files/Source/Console/cmaster.cs",
            "Project Files/Source/ChannelMaster/cmaster.c",
        ],
        [
            "//   Copyright (C) 2000-2025  Original authors [cmaster.cs]",
            "//   Copyright (C) 2014-2019  Warren Pratt (NR0V) [cmaster.c]",
            "//   Copyright (C) 2020-2025  Richard Samphire (MW0LGE) [cmaster.cs]",
        ],
        has_samphire=True,
    ),
    "src/gui/SpectrumWidget.h": make_header(
        "src/gui/SpectrumWidget.h",
        [
            "Project Files/Source/Console/enums.cs",
            "Project Files/Source/Console/setup.cs",
            "Project Files/Source/Console/display.cs",
        ],
        [
            "//   Copyright (C) 2004-2009  FlexRadio Systems",
            "//   Copyright (C) 2010-2020  Doug Wigley (W5WC)",
            "//   Copyright (C) 2019-2026  Richard Samphire (MW0LGE) — heavily modified",
            "//   Waterfall AGC Modifications Copyright (C) 2013  Phil Harman (VK6APH)",
        ],
        has_samphire=True,
    ),
}


def inject(rel_path, header_text):
    path = REPO / rel_path
    if not path.exists():
        print(f"SKIP {rel_path} — not found")
        return

    content = path.read_text(encoding="utf-8")

    # Prepend after the first `#pragma once` line (if present)
    lines = content.splitlines(keepends=True)
    insert_at = 0
    if lines and lines[0].strip() == "#pragma once":
        insert_at = 1
        # Skip blank lines after pragma
        while insert_at < len(lines) and lines[insert_at].strip() == "":
            insert_at += 1

    new_content = (
        "".join(lines[:insert_at])
        + "\n"
        + header_text
        + "\n"
        + "".join(lines[insert_at:])
    )
    path.write_text(new_content, encoding="utf-8")
    print(f"injected: {rel_path}")


def main():
    for rel_path, header_text in HEADERS.items():
        inject(rel_path, header_text)


if __name__ == "__main__":
    main()
