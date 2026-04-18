#!/usr/bin/env python3
"""
Repair multi-source headers: replace skeleton placeholders with actual text.

For each file, determine the correct contributor union based on the Thetis
sources it cites, then substitute the real GPL permission block and
(if applicable) the Samphire dual-licensing block.
"""

import re
import sys
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

# Map of NereusSDR file -> (copyright_lines_list, has_samphire)
# copyright_lines_list: lines to put AFTER "Thetis is a C# implementation..."
# but these replace the <additional...> placeholder; the first three standard
# lines (FlexRadio/Wigley/Samphire) may or may not all be present.
#
# Format: list of "//   Copyright (C) ..." strings OR None to omit that line.
# We completely replace lines 17-21 in the skeleton with the correct set.

# Contributor sets (reusable):
# Full set: FlexRadio + Wigley + Samphire
FLEX_WIGLEY_SAMPHIRE = [
    "//   Copyright (C) 2004-2009  FlexRadio Systems",
    "//   Copyright (C) 2010-2020  Doug Wigley (W5WC)",
    "//   Copyright (C) 2019-2026  Richard Samphire (MW0LGE) — heavily modified",
]

# FlexRadio + Wigley only (no Samphire)
FLEX_WIGLEY = [
    "//   Copyright (C) 2004-2009  FlexRadio Systems",
    "//   Copyright (C) 2010-2020  Doug Wigley (W5WC)",
]

# Wigley only (no FlexRadio, no Samphire) — NetworkIO.cs style
WIGLEY_ONLY = [
    "//   Copyright (C) 2010-2018  Doug Wigley (W5WC)",
]

# Samphire only — MeterManager.cs style
SAMPHIRE_ONLY = [
    "//   Copyright (C) 2020-2026  Richard Samphire (MW0LGE)",
]

# Warren Pratt NR0V (dsp.cs / cmaster.c)
NR0V_PRATT = [
    "//   Copyright (C) 2013-2017  Warren Pratt (NR0V)",
]

NR0V_PRATT_CMASTER = [
    "//   Copyright (C) 2014-2019  Warren Pratt (NR0V)",
]

# Wigley LGPL (network.c / network.h / networkproto1.c)
WIGLEY_LGPL = [
    "//   Copyright (C) 2015-2020  Doug Wigley (W5WC)",
]

WIGLEY_LGPL_2020 = [
    "//   Copyright (C) 2020  Doug Wigley (W5WC)",
]

# netInterface.c: Bill Tracey + Wigley
TRACEY_WIGLEY_LGPL = [
    "//   Copyright (C) 2006-2007  Bill Tracey (KD5TFD)",
    "//   Copyright (C) 2010-2020  Doug Wigley (W5WC)",
]

# cmaster.cs: "Original authors" + Samphire
CMASTER_CS = [
    "//   Copyright (C) 2000-2025  Original authors",
    "//   Copyright (C) 2020-2025  Richard Samphire (MW0LGE)",
]

# enums.cs: "Original authors" + Samphire
ENUMS_CS = [
    "//   Copyright (C) 2000-2025  Original authors",
    "//   Copyright (C) 2020-2025  Richard Samphire (MW0LGE)",
]

# xvtr.cs: FlexRadio + Wigley (2010-2013) + Samphire dual-license
XVTR_CS = [
    "//   Copyright (C) 2004-2009  FlexRadio Systems",
    "//   Copyright (C) 2010-2013  Doug Wigley (W5WC)",
    "//   Copyright (C) 2019-2026  Richard Samphire (MW0LGE) — dual-license applies",
]

# Phil Harman additions to display.cs
DISPLAY_CS = [
    "//   Copyright (C) 2004-2009  FlexRadio Systems",
    "//   Copyright (C) 2010-2020  Doug Wigley (W5WC)",
    "//   Waterfall AGC Modifications Copyright (C) 2013  Phil Harman (VK6APH)",
]

# File → (contributor_lines, has_samphire)
# has_samphire = True → include dual-licensing block
FILE_MAP = {
    # clsHardwareSpecific (Samphire) + setup (Samphire) + console (no Samphire)
    # + enums (Samphire) + NetworkIO (no Samphire) + clsDiscoveredRadioPicker (Samphire)
    # + network.h (Wigley LGPL) → union: FlexRadio, Wigley, Samphire + Wigley-LGPL
    "src/core/BoardCapabilities.cpp": (
        [
            "//   Copyright (C) 2004-2009  FlexRadio Systems",
            "//   Copyright (C) 2010-2020  Doug Wigley (W5WC)",
            "//   Copyright (C) 2015-2020  Doug Wigley (W5WC) [ChannelMaster — LGPL]",
            "//   Copyright (C) 2019-2026  Richard Samphire (MW0LGE) — heavily modified",
        ],
        True,
    ),
    # clsHardwareSpecific (Samphire) + setup (Samphire) + console (no Samphire)
    # + specHPSDR (Wigley only) + network.h (Wigley LGPL)
    "src/core/BoardCapabilities.h": (
        [
            "//   Copyright (C) 2004-2009  FlexRadio Systems",
            "//   Copyright (C) 2010-2020  Doug Wigley (W5WC)",
            "//   Copyright (C) 2015-2020  Doug Wigley (W5WC) [ChannelMaster — LGPL]",
            "//   Copyright (C) 2019-2026  Richard Samphire (MW0LGE) — heavily modified",
        ],
        True,
    ),
    # enums.cs (Samphire) + network.h (Wigley LGPL)
    "src/core/HpsdrModel.h": (
        [
            "//   Copyright (C) 2000-2025  Original authors",
            "//   Copyright (C) 2015-2020  Doug Wigley (W5WC) [ChannelMaster — LGPL]",
            "//   Copyright (C) 2020-2025  Richard Samphire (MW0LGE)",
        ],
        True,
    ),
    # networkproto1.c (Wigley LGPL) + NetworkIO.cs (Wigley) + cmaster.cs (Samphire) + console.cs (FlexRadio/Wigley)
    "src/core/P1RadioConnection.cpp": (
        [
            "//   Copyright (C) 2004-2009  FlexRadio Systems",
            "//   Copyright (C) 2010-2020  Doug Wigley (W5WC)",
            "//   Copyright (C) 2020  Doug Wigley (W5WC) [ChannelMaster networkproto1.c — LGPL]",
            "//   Copyright (C) 2000-2025  Original authors [cmaster.cs]",
            "//   Copyright (C) 2020-2025  Richard Samphire (MW0LGE)",
        ],
        True,
    ),
    # networkproto1.c (Wigley LGPL) + NetworkIO.cs (Wigley)
    "src/core/P1RadioConnection.h": (
        [
            "//   Copyright (C) 2010-2020  Doug Wigley (W5WC)",
            "//   Copyright (C) 2020  Doug Wigley (W5WC) [ChannelMaster networkproto1.c — LGPL]",
        ],
        False,
    ),
    # network.c (Wigley LGPL) + network.h (Wigley LGPL) + netInterface.c (Tracey+Wigley LGPL) + console.cs (FlexRadio/Wigley)
    "src/core/P2RadioConnection.cpp": (
        [
            "//   Copyright (C) 2004-2009  FlexRadio Systems",
            "//   Copyright (C) 2006-2007  Bill Tracey (KD5TFD) [ChannelMaster netInterface.c — LGPL]",
            "//   Copyright (C) 2010-2020  Doug Wigley (W5WC)",
            "//   Copyright (C) 2015-2020  Doug Wigley (W5WC) [ChannelMaster — LGPL]",
        ],
        False,
    ),
    # Same as .cpp
    "src/core/P2RadioConnection.h": (
        [
            "//   Copyright (C) 2004-2009  FlexRadio Systems",
            "//   Copyright (C) 2006-2007  Bill Tracey (KD5TFD) [ChannelMaster netInterface.c — LGPL]",
            "//   Copyright (C) 2010-2020  Doug Wigley (W5WC)",
            "//   Copyright (C) 2015-2020  Doug Wigley (W5WC) [ChannelMaster — LGPL]",
        ],
        False,
    ),
    # radio.cs (Flex/Wigley/Samphire) + console.cs (Flex/Wigley) + dsp.cs (NR0V+Samphire)
    # + rxa.cs (no header) + specHPSDR (Wigley) + setup.cs (Samphire) + cmaster.c (NR0V)
    "src/core/RxChannel.cpp": (
        [
            "//   Copyright (C) 2004-2009  FlexRadio Systems",
            "//   Copyright (C) 2010-2020  Doug Wigley (W5WC)",
            "//   Copyright (C) 2013-2017  Warren Pratt (NR0V) [dsp.cs / cmaster.c]",
            "//   Copyright (C) 2019-2026  Richard Samphire (MW0LGE) — heavily modified",
        ],
        True,
    ),
    # dsp.cs (NR0V+Samphire) + radio.cs (Samphire) + specHPSDR (Wigley)
    "src/core/wdsp_api.h": (
        [
            "//   Copyright (C) 2010-2018  Doug Wigley (W5WC)",
            "//   Copyright (C) 2013-2017  Warren Pratt (NR0V) [dsp.cs]",
            "//   Copyright (C) 2019-2026  Richard Samphire (MW0LGE) — heavily modified",
        ],
        True,
    ),
    # dsp.cs (NR0V+Samphire) + setup.cs (Samphire) + console.cs (Flex/Wigley)
    "src/core/WdspTypes.h": (
        [
            "//   Copyright (C) 2004-2009  FlexRadio Systems",
            "//   Copyright (C) 2010-2020  Doug Wigley (W5WC)",
            "//   Copyright (C) 2013-2017  Warren Pratt (NR0V) [dsp.cs]",
            "//   Copyright (C) 2019-2026  Richard Samphire (MW0LGE) — heavily modified",
        ],
        True,
    ),
    # MeterManager.cs (Samphire) + dsp.cs (NR0V+Samphire) + console.cs (Flex/Wigley)
    # + setup.cs (Samphire) + radio.cs (Samphire)
    "src/gui/MainWindow.cpp": (
        [
            "//   Copyright (C) 2004-2009  FlexRadio Systems",
            "//   Copyright (C) 2010-2020  Doug Wigley (W5WC)",
            "//   Copyright (C) 2013-2017  Warren Pratt (NR0V) [dsp.cs]",
            "//   Copyright (C) 2019-2026  Richard Samphire (MW0LGE) — heavily modified",
            "//   Copyright (C) 2020-2026  Richard Samphire (MW0LGE) [MeterManager.cs]",
        ],
        True,
    ),
    # MeterManager.cs (Samphire 2020-2026) + console.cs (Flex/Wigley)
    "src/gui/meters/MeterItem.cpp": (
        [
            "//   Copyright (C) 2004-2009  FlexRadio Systems",
            "//   Copyright (C) 2010-2020  Doug Wigley (W5WC)",
            "//   Copyright (C) 2020-2026  Richard Samphire (MW0LGE) [MeterManager.cs]",
        ],
        True,
    ),
    "src/gui/meters/MeterItem.h": (
        [
            "//   Copyright (C) 2004-2009  FlexRadio Systems",
            "//   Copyright (C) 2010-2020  Doug Wigley (W5WC)",
            "//   Copyright (C) 2020-2026  Richard Samphire (MW0LGE) [MeterManager.cs]",
        ],
        True,
    ),
    # MeterManager.cs (Samphire) + console.cs (Flex/Wigley)
    "src/gui/meters/SignalTextItem.cpp": (
        [
            "//   Copyright (C) 2004-2009  FlexRadio Systems",
            "//   Copyright (C) 2010-2020  Doug Wigley (W5WC)",
            "//   Copyright (C) 2020-2026  Richard Samphire (MW0LGE) [MeterManager.cs]",
        ],
        True,
    ),
    "src/gui/meters/SignalTextItem.h": (
        [
            "//   Copyright (C) 2004-2009  FlexRadio Systems",
            "//   Copyright (C) 2010-2020  Doug Wigley (W5WC)",
            "//   Copyright (C) 2020-2026  Richard Samphire (MW0LGE) [MeterManager.cs]",
        ],
        True,
    ),
    # xvtr.cs (FlexRadio + Wigley 2010-2013 + Samphire dual-license)
    "src/gui/setup/hardware/XvtrTab.cpp": (
        [
            "//   Copyright (C) 2004-2009  FlexRadio Systems",
            "//   Copyright (C) 2010-2013  Doug Wigley (W5WC)",
            "//   Copyright (C) 2019-2026  Richard Samphire (MW0LGE) — heavily modified",
        ],
        True,
    ),
    "src/gui/setup/hardware/XvtrTab.h": (
        [
            "//   Copyright (C) 2004-2009  FlexRadio Systems",
            "//   Copyright (C) 2010-2013  Doug Wigley (W5WC)",
            "//   Copyright (C) 2019-2026  Richard Samphire (MW0LGE) — heavily modified",
        ],
        True,
    ),
    # display.cs (Flex/Wigley/VK6APH) + console.cs (Flex/Wigley)
    "src/gui/SpectrumWidget.cpp": (
        [
            "//   Copyright (C) 2004-2009  FlexRadio Systems",
            "//   Copyright (C) 2010-2020  Doug Wigley (W5WC)",
            "//   Waterfall AGC Modifications Copyright (C) 2013  Phil Harman (VK6APH)",
        ],
        False,
    ),
    # enums.cs (Samphire) + setup.cs (Samphire) + display.cs (Flex/Wigley/VK6APH)
    "src/gui/SpectrumWidget.h": (
        [
            "//   Copyright (C) 2004-2009  FlexRadio Systems",
            "//   Copyright (C) 2010-2020  Doug Wigley (W5WC)",
            "//   Copyright (C) 2019-2026  Richard Samphire (MW0LGE) — heavily modified",
            "//   Waterfall AGC Modifications Copyright (C) 2013  Phil Harman (VK6APH)",
        ],
        True,
    ),
    # console.cs (Flex/Wigley) + setup.designer.cs (auto-gen, no copyright) + radio.cs (Samphire)
    "src/gui/widgets/VfoModeContainers.cpp": (
        [
            "//   Copyright (C) 2004-2009  FlexRadio Systems",
            "//   Copyright (C) 2010-2020  Doug Wigley (W5WC)",
            "//   Copyright (C) 2019-2026  Richard Samphire (MW0LGE) — heavily modified",
        ],
        True,
    ),
    # console.cs (Flex/Wigley) + dsp.cs (NR0V+Samphire)
    "src/gui/widgets/VfoModeContainers.h": (
        [
            "//   Copyright (C) 2004-2009  FlexRadio Systems",
            "//   Copyright (C) 2010-2020  Doug Wigley (W5WC)",
            "//   Copyright (C) 2013-2017  Warren Pratt (NR0V) [dsp.cs]",
            "//   Copyright (C) 2019-2026  Richard Samphire (MW0LGE) — heavily modified",
        ],
        True,
    ),
    # console.cs + console.resx + display.cs + enums.cs + radio.cs + dsp.cs + specHPSDR.cs
    # Union: FlexRadio, Wigley, Samphire (radio.cs), NR0V (dsp.cs), VK6APH (display.cs)
    "src/gui/widgets/VfoWidget.cpp": (
        [
            "//   Copyright (C) 2004-2009  FlexRadio Systems",
            "//   Copyright (C) 2010-2020  Doug Wigley (W5WC)",
            "//   Copyright (C) 2013-2017  Warren Pratt (NR0V) [dsp.cs]",
            "//   Copyright (C) 2019-2026  Richard Samphire (MW0LGE) — heavily modified",
            "//   Waterfall AGC Modifications Copyright (C) 2013  Phil Harman (VK6APH)",
        ],
        True,
    ),
    # console.cs + setup.cs + radio.cs + dsp.cs + NetworkIO.cs + cmaster.c
    "src/models/RadioModel.cpp": (
        [
            "//   Copyright (C) 2004-2009  FlexRadio Systems",
            "//   Copyright (C) 2010-2020  Doug Wigley (W5WC)",
            "//   Copyright (C) 2014-2019  Warren Pratt (NR0V) [cmaster.c]",
            "//   Copyright (C) 2019-2026  Richard Samphire (MW0LGE) — heavily modified",
        ],
        True,
    ),
    # console.cs + display.cs
    "src/models/SliceModel.cpp": (
        [
            "//   Copyright (C) 2004-2009  FlexRadio Systems",
            "//   Copyright (C) 2010-2020  Doug Wigley (W5WC)",
        ],
        False,
    ),
    # console.cs + radio.cs (Samphire) + setup.designer.cs (auto-gen)
    "src/models/SliceModel.h": (
        [
            "//   Copyright (C) 2004-2009  FlexRadio Systems",
            "//   Copyright (C) 2010-2020  Doug Wigley (W5WC)",
            "//   Copyright (C) 2019-2026  Richard Samphire (MW0LGE) — heavily modified",
        ],
        True,
    ),
    # console.cs + enums.cs (Samphire) + radio.cs (Samphire) + setup.designer.cs
    "tests/tst_dig_rtty_wire.cpp": (
        [
            "//   Copyright (C) 2004-2009  FlexRadio Systems",
            "//   Copyright (C) 2010-2020  Doug Wigley (W5WC)",
            "//   Copyright (C) 2019-2026  Richard Samphire (MW0LGE) — heavily modified",
        ],
        True,
    ),
    # enums.cs (Samphire) + network.h (Wigley LGPL)
    "tests/tst_hpsdr_enums.cpp": (
        [
            "//   Copyright (C) 2000-2025  Original authors",
            "//   Copyright (C) 2015-2020  Doug Wigley (W5WC) [ChannelMaster — LGPL]",
            "//   Copyright (C) 2020-2025  Richard Samphire (MW0LGE)",
        ],
        True,
    ),
    # radio.cs (Samphire) + console.cs (Flex/Wigley) + dsp.cs (NR0V+Samphire)
    "tests/tst_rxchannel_agc_advanced.cpp": (
        [
            "//   Copyright (C) 2004-2009  FlexRadio Systems",
            "//   Copyright (C) 2010-2020  Doug Wigley (W5WC)",
            "//   Copyright (C) 2013-2017  Warren Pratt (NR0V) [dsp.cs]",
            "//   Copyright (C) 2019-2026  Richard Samphire (MW0LGE) — heavily modified",
        ],
        True,
    ),
    # dsp.cs (NR0V+Samphire) + radio.cs (Samphire)
    "tests/tst_rxchannel_audio_panel.cpp": (
        [
            "//   Copyright (C) 2004-2009  FlexRadio Systems",
            "//   Copyright (C) 2010-2020  Doug Wigley (W5WC)",
            "//   Copyright (C) 2013-2017  Warren Pratt (NR0V) [dsp.cs]",
            "//   Copyright (C) 2019-2026  Richard Samphire (MW0LGE) — heavily modified",
        ],
        True,
    ),
    # console.cs (Flex/Wigley) + dsp.cs (NR0V+Samphire)
    "tests/tst_rxchannel_snb.cpp": (
        [
            "//   Copyright (C) 2004-2009  FlexRadio Systems",
            "//   Copyright (C) 2010-2020  Doug Wigley (W5WC)",
            "//   Copyright (C) 2013-2017  Warren Pratt (NR0V) [dsp.cs]",
            "//   Copyright (C) 2019-2026  Richard Samphire (MW0LGE) — heavily modified",
        ],
        True,
    ),
    # Same as tst_rxchannel_audio_panel.cpp
    "tests/tst_slice_audio_panel.cpp": (
        [
            "//   Copyright (C) 2004-2009  FlexRadio Systems",
            "//   Copyright (C) 2010-2020  Doug Wigley (W5WC)",
            "//   Copyright (C) 2013-2017  Warren Pratt (NR0V) [dsp.cs]",
            "//   Copyright (C) 2019-2026  Richard Samphire (MW0LGE) — heavily modified",
        ],
        True,
    ),
    # Same as tst_rxchannel_snb.cpp
    "tests/tst_slice_snb.cpp": (
        [
            "//   Copyright (C) 2004-2009  FlexRadio Systems",
            "//   Copyright (C) 2010-2020  Doug Wigley (W5WC)",
            "//   Copyright (C) 2013-2017  Warren Pratt (NR0V) [dsp.cs]",
            "//   Copyright (C) 2019-2026  Richard Samphire (MW0LGE) — heavily modified",
        ],
        True,
    ),
}

PLACEHOLDER_CONTRIBUTOR = "//   <additional per-file contributor lines as present in the sources>"
PLACEHOLDER_GPL = "//   [GPLv2-or-later permission block verbatim — see Variant 1]"
PLACEHOLDER_SAMPHIRE = "// [Samphire dual-licensing block if any cited source had Samphire content]"
SKELETON_SAMPHIRE_LINE = "//   Copyright (C) 2019-2026  Richard Samphire (MW0LGE) — heavily modified"
SKELETON_FLEX_LINE = "//   Copyright (C) 2004-2009  FlexRadio Systems"
SKELETON_WIGLEY_LINE = "//   Copyright (C) 2010-2020  Doug Wigley (W5WC)"


def repair_file(rel_path: str, contributors: list, has_samphire: bool) -> bool:
    path = REPO / rel_path
    if not path.exists():
        print(f"SKIP {rel_path} — file not found")
        return False

    text = path.read_text(encoding="utf-8")

    # Check if the skeleton placeholders are present
    if PLACEHOLDER_GPL not in text:
        print(f"SKIP {rel_path} — GPL placeholder not found (already repaired?)")
        return False

    # Step 1: Replace the three standard skeleton copyright lines + placeholder contributor
    # with the correct contributor union.
    # The skeleton has these lines in this order:
    #   //   Copyright (C) 2004-2009  FlexRadio Systems
    #   //   Copyright (C) 2010-2020  Doug Wigley (W5WC)
    #   //   Copyright (C) 2019-2026  Richard Samphire (MW0LGE) — heavily modified
    #   <additional per-file contributor lines as present in the sources>
    #
    # We need to replace all four with the correct contributor list.

    # Build the old block (skeleton) and new block
    old_contrib_block = (
        "//   Copyright (C) 2004-2009  FlexRadio Systems\n"
        "//   Copyright (C) 2010-2020  Doug Wigley (W5WC)\n"
        "//   Copyright (C) 2019-2026  Richard Samphire (MW0LGE) — heavily modified\n"
        "//   <additional per-file contributor lines as present in the sources>"
    )
    new_contrib_block = "\n".join(contributors)

    if old_contrib_block not in text:
        print(f"WARN {rel_path} — contributor skeleton block not found verbatim; trying partial replacement")
        # Try just removing the placeholder contributor line
        text = text.replace(
            "\n" + PLACEHOLDER_CONTRIBUTOR,
            ""
        )
    else:
        text = text.replace(old_contrib_block, new_contrib_block)

    # Step 2: Replace the GPL placeholder with actual GPL text
    text = text.replace(
        "//   [GPLv2-or-later permission block verbatim — see Variant 1]",
        GPL_BLOCK
    )

    # Step 3: Replace the Samphire block placeholder
    if has_samphire:
        text = text.replace(
            "// [Samphire dual-licensing block if any cited source had Samphire content]",
            SAMPHIRE_BLOCK
        )
    else:
        # Remove the placeholder line entirely (including surrounding blank comment lines)
        text = text.replace(
            "//\n// [Samphire dual-licensing block if any cited source had Samphire content]\n//\n",
            ""
        )
        # Also try without surrounding blank lines
        text = text.replace(
            "\n// [Samphire dual-licensing block if any cited source had Samphire content]\n",
            "\n"
        )

    path.write_text(text, encoding="utf-8")
    print(f"repaired: {rel_path}")
    return True


def main():
    repaired = 0
    skipped = 0
    for rel_path, (contributors, has_samphire) in FILE_MAP.items():
        if repair_file(rel_path, contributors, has_samphire):
            repaired += 1
        else:
            skipped += 1

    print(f"\n{repaired} files repaired, {skipped} skipped")


if __name__ == "__main__":
    main()
