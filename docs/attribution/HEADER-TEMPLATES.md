# Header Templates for Thetis-Derived Files

Every NereusSDR source file listed in `THETIS-PROVENANCE.md` must carry
one of the four header blocks below, placed immediately after any
`#pragma once` / include guard and before the first `#include`.

Per spec `docs/architecture/2026-04-16-gpl-compliance-remediation-design.md`
§6.

Substitution rules:
- `<FILENAME>` — relative path from repo root
- `<THETIS_SOURCE_PATHS>` — list of Thetis source paths (newline-separated
  if multiple), each prefixed with `//   ` and beginning with
  `Project Files/Source/...`
- `<YEAR_RANGE>` for each contributor — inherit from the Thetis source
  file's copyright block verbatim (do not recompute)
- `<PORT_DATE>` — ISO date `YYYY-MM-DD` when the port was introduced into
  NereusSDR (use today's date at header-insertion time for existing files;
  for new ports, use the port date)

---

## Variant 1 — `thetis-samphire`

**Use when:** the Thetis source file(s) contain contributions from
Richard Samphire (MW0LGE). His per-file header line reads
`Copyright (C) 2019-2026 Richard Samphire (MW0LGE) – heavily modified`
(or similar).

```cpp
// =================================================================
// <FILENAME>  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
<THETIS_SOURCE_PATHS>
//
// Original Thetis copyright and license (preserved per GNU GPL):
//
//   Thetis is a C# implementation of a Software Defined Radio.
//   Copyright (C) 2004-2009  FlexRadio Systems
//   Copyright (C) 2010-2020  Doug Wigley (W5WC)
//   Copyright (C) 2019-2026  Richard Samphire (MW0LGE) — heavily modified
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
//   <PORT_DATE> — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code. Structural template follows AetherSDR
//                 (ten9876/AetherSDR) Qt6 conventions.
// =================================================================
```

---

## Variant 2 — `thetis-no-samphire`

**Use when:** the Thetis source file(s) contain only FlexRadio / Wigley
contributions (no Samphire heavy-modification line). Drop the
dual-licensing block; keep the two-copyright-line form.

```cpp
// =================================================================
// <FILENAME>  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
<THETIS_SOURCE_PATHS>
//
// Original Thetis copyright and license (preserved per GNU GPL):
//
//   Thetis is a C# implementation of a Software Defined Radio.
//   Copyright (C) 2004-2009  FlexRadio Systems
//   Copyright (C) 2010-2020  Doug Wigley (W5WC)
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
// =================================================================
// Modification history (NereusSDR):
//   <PORT_DATE> — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
// =================================================================
```

---

## Variant 3 — `mi0bot`

**Use when:** the NereusSDR file ports a Thetis source file that BOTH the
mi0bot fork AND ramdor/Thetis upstream carry, but the NereusSDR port
specifically draws on mi0bot's fork-added HL2 modifications. Keeps the
upstream Thetis contributor chain AND credits Reid Campbell (MI0BOT) for
the fork contributions.

Full-name attribution "Reid Campbell (MI0BOT)" is verified from the fork's
own `Project Files/Source/Console/HPSDR/IoBoardHl2.cs` header
(`Copyright (C) 2025 Reid Campbell, MI0BOT, mi0bot@trom.uk`).

For files that are UNIQUE to the mi0bot fork (e.g. `IoBoardHl2.cs` —
solo Reid Campbell authorship, no FlexRadio / Wigley / Samphire
contributions), use **Variant 5** instead.

```cpp
// =================================================================
// <FILENAME>  (NereusSDR)
// =================================================================
//
// Ported from mi0bot/Thetis-HL2 source:
<THETIS_SOURCE_PATHS>
//
// Original copyright and license (preserved per GNU GPL):
//
//   Thetis / Thetis-HL2 is a C# implementation of a Software Defined Radio.
//   Copyright (C) 2004-2009  FlexRadio Systems
//   Copyright (C) 2010-2020  Doug Wigley (W5WC)
//   Copyright (C) 2019-2026  Richard Samphire (MW0LGE) — Thetis heavy modifications
//   Copyright (C) <YEAR_RANGE>  Reid Campbell (MI0BOT) — Hermes-Lite fork contributions
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
// contributions in the upstream Thetis source — preserved verbatim from
// Thetis LICENSE-DUAL-LICENSING):
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
//   <PORT_DATE> — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code. Structural template follows AetherSDR
//                 (ten9876/AetherSDR) Qt6 conventions.
// =================================================================
```

---

## Variant 4 — `multi-source`

**Use when:** the NereusSDR file synthesizes logic from multiple Thetis
source files with different contributor sets. Enumerate each source; list
the union of contributors. If any cited source includes Samphire
contributions, include the dual-licensing block.

Skeleton (adapt copyright lines to match union of cited sources):

```cpp
// =================================================================
// <FILENAME>  (NereusSDR)
// =================================================================
//
// Ported from multiple Thetis sources:
<THETIS_SOURCE_PATHS>
//
// Original Thetis copyright and license (preserved per GNU GPL,
// representing the union of contributors across all cited sources):
//
//   Thetis is a C# implementation of a Software Defined Radio.
//   Copyright (C) 2004-2009  FlexRadio Systems
//   Copyright (C) 2010-2020  Doug Wigley (W5WC)
//   Copyright (C) 2019-2026  Richard Samphire (MW0LGE) — heavily modified
//   <additional per-file contributor lines as present in the sources>
//
//   [GPLv2-or-later permission block verbatim — see Variant 1]
//
// [Samphire dual-licensing block if any cited source had Samphire content]
//
// =================================================================
// Modification history (NereusSDR):
//   <PORT_DATE> — Synthesized in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code. Combines logic from the Thetis sources
//                 listed above.
// =================================================================
```

---

## Variant 5 — `mi0bot-solo`

**Use when:** the NereusSDR file ports a file that exists ONLY in the
mi0bot/OpenHPSDR-Thetis fork (not in ramdor/Thetis upstream) and whose
header names Reid Campbell (MI0BOT) as the sole author with no
FlexRadio / Wigley / Samphire contributions. Currently applies to
derivatives of `Project Files/Source/Console/HPSDR/IoBoardHl2.cs`.

```cpp
// =================================================================
// <FILENAME>  (NereusSDR)
// =================================================================
//
// Ported from mi0bot/OpenHPSDR-Thetis source:
<THETIS_SOURCE_PATHS>
//
// This file is a mi0bot-fork-unique source authored solely by Reid
// Campbell (MI0BOT) — it does not exist in ramdor/Thetis upstream and
// carries no FlexRadio / Wigley / Samphire contributions. Its header
// attribution is reproduced verbatim below, per GNU GPL preservation
// requirements.
//
// Original copyright and license (preserved from upstream header):
//
//   Copyright (C) <YEAR>  Reid Campbell (MI0BOT), mi0bot@trom.uk
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation; either version 2 of the License, or
//   (at your option) any later version.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
//   You should have received a copy of the GNU General Public License
//   along with this program; if not, write to the Free Software
//   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307, USA.
//
// =================================================================
// Modification history (NereusSDR):
//   <PORT_DATE> — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                (KG4VCF), with AI-assisted transformation via Anthropic
//                Claude Code.
// =================================================================
```

---

## Non-applicability

Files that do NOT require a Thetis header:

- Pure NereusSDR-native Qt widgets (`VfoWidget`, `HGauge`, `ColorSwatchButton`,
  applet panel widgets, etc.)
- `src/models/Band.h` (IARU Region 2 spec data — no Thetis source consulted)
- Build files, CI config, shell scripts, markdown docs
- `third_party/wdsp/` (already vendored with upstream TAPR attribution)
- `third_party/fftw3/` (out of scope)

Files in these categories are listed in `THETIS-PROVENANCE.md` under the
"Independently implemented — Thetis-like but not derived" section with a
short justification.
