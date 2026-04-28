# deskhpsdr Provenance — NereusSDR derived-file inventory

This document catalogs every NereusSDR source file derived from, translated
from, or materially based on deskhpsdr (dl1bz/deskhpsdr). Per-file license
headers live in the source files themselves; this index is the grep-able
summary.

NereusSDR is distributed under GPLv3 (root `LICENSE`). deskhpsdr is also
GPLv3-or-later — fully compatible. See §License below.

## When entries get added

A row is added to the table below — in the **same commit** that introduces
the ported logic — whenever a NereusSDR file:

1. Ports, translates, or materially re-expresses logic from any deskhpsdr
   `.c` or `.h` file in `src/`, AND
2. That logic is not already covered by the Thetis/mi0bot lineage (i.e.
   deskhpsdr is the *primary* source for that logic, not a cross-reference).

The procedure is identical to `THETIS-PROVENANCE.md`:

- Add the verbatim deskhpsdr file header to the NereusSDR file (per
  `HOW-TO-PORT.md` §"Byte-for-byte headers and multi-file attribution").
- Add a `// From deskhpsdr <path>:<line> [@<sha>]` inline cite at every
  ported function/constant (per `HOW-TO-PORT.md` §"Inline cite versioning").
- Add a PROVENANCE row here with the NereusSDR file, deskhpsdr source,
  line ranges, derivation type, and notes.

## Upstream

- **Project:** deskHPSDR
- **Repository:** https://github.com/dl1bz/deskhpsdr
- **Lineage:** fork of piHPSDR by DL1YCF (Christoph van Wüllen); piHPSDR
  was originally authored by John Melton, G0ORX/N6LYT (2015).
- **Current maintainer:** Heiko Amft, DL1BZ (2024–2025)
- **Initial corpus reference SHA:** `@120188f` (HEAD at time of first
  deskhpsdr port, 2026-04-27)
- **Language:** C (`.c` / `.h` only; no C++ in the deskhpsdr source tree)

## License

deskhpsdr is distributed under the **GNU General Public License v3.0 or
later** (GPLv3+). Every `.c` / `.h` file under `src/` carries the header:

```
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
```

NereusSDR is also GPLv3. GPLv3 + GPLv3-or-later → **fully compatible**;
no compatibility note required. No dual-licensing (Samphire-style or
otherwise) exists in the deskhpsdr source tree.

## Legend

Derivation type:
- `port`       — direct reimplementation in C++/Qt6 of a deskhpsdr source file
- `reference`  — consulted for behavior during independent implementation
- `structural` — architectural template with substantive behavioral echo

## Files derived from dl1bz/deskhpsdr

| NereusSDR file | deskhpsdr source | Line ranges | Type | Notes |
| --- | --- | --- | --- | --- |
| `src/core/P2RadioConnection.h` + `P2RadioConnection.cpp` | `src/new_protocol.c:1484-1486` | P2 CmdTx byte 50 mic_boost bit (0x02): `if (mic_boost) { transmit_specific_buffer[50] \|= 0x02; }` | `port` | First deskhpsdr port. 3M-1b G.1. Date: 2026-04-27. |
| `src/core/P2RadioConnection.h` + `P2RadioConnection.cpp` | `src/new_protocol.c:1480-1482` | P2 CmdTx byte 50 line_in bit (0x01): `if (mic_linein) { transmit_specific_buffer[50] \|= 0x01; }` | `port` | 3M-1b G.2. Date: 2026-04-28. |
| `src/core/P2RadioConnection.h` + `P2RadioConnection.cpp` | `src/new_protocol.c:1492-1494` | P2 CmdTx byte 50 mic_tip_ring bit (0x08, INVERTED): `if (mic_ptt_tip_bias_ring) { transmit_specific_buffer[50] \|= 0x08; }`. NOTE: polarity inversion at NereusSDR `setMicTipRing(bool tipHot)` API layer — wire bit is `!tipHot`. | `port` | 3M-1b G.3. Date: 2026-04-28. |
