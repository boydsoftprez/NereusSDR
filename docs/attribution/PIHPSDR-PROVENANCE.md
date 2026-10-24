# piHPSDR Provenance: NereusSDR derived-file inventory

This document lists every NereusSDR source file that ports from, or cites
facts from, piHPSDR (dl1ycf/pihpsdr). Per-file license headers live in the
source files themselves; this index is the grep-able summary.

NereusSDR is distributed under GPLv3 (root `LICENSE`). piHPSDR is
GPLv3-or-later, which is fully compatible. See §License below.

## When entries get added

A row is added to the table below, in the **same commit** that introduces
the code, whenever a NereusSDR file:

1. Ports, translates, or materially re-expresses logic from any piHPSDR
   `.c` or `.h` file in `src/` (kind `port`), or
2. Takes a fact or constant from piHPSDR (a timeout, a bit name, a default
   value) and cites it inline (kind `reference`).

For a `port`, follow `HOW-TO-PORT.md` §"piHPSDR and deskHPSDR": copy the
upstream file's header byte-for-byte, add the Modification history block,
and carry inline comments verbatim. For a `reference`, an inline cite of the
form `// From piHPSDR <path>:<line> [@<sha>]` and a row here are enough.

## Upstream

- **Project:** piHPSDR
- **Repository:** https://github.com/dl1ycf/pihpsdr
- **Authors:** John Melton, G0ORX/N6LYT (2015); Christoph van Wüllen,
  DL1YCF (2025), per the header of each `src/` file.
- **Pinned SHA:** `@4aa95c5` (2026-08-06), cloned to `../pihpsdr/`. Do not
  pull; re-pinning is deliberate and re-verifies every cite.
- **Language:** C with GTK.

## License

piHPSDR is distributed under the **GNU General Public License v3.0 or
later** (GPLv3+, `COPYING`). Its `src/` files carry the header:

```
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
```

NereusSDR is also GPLv3, so the two are fully compatible.

## Legend

Derivation type:
- `port`       direct reimplementation in C++/Qt6 of a piHPSDR source file
- `reference`  a fact or constant cited inline, or behavior consulted during
               independent implementation
- `structural` architectural template with substantive behavioral echo

## Files citing dl1ycf/pihpsdr

| NereusSDR file | piHPSDR source | Line ranges | Type | Notes |
| --- | --- | --- | --- | --- |
| `src/core/PaGainProfile.cpp` | `src/band.c` | 320-330 (@4aa95c5) | `reference` | ANAN-G2E PA gain row: piHPSDR's default `pa_calibration` of 53.0 dB on 160 m through 6 m, used in place of the Thetis ANAN7000D row that overdrives the G2E. Drive formula equivalence checked against `src/radio.c:2537-2547` (`calcLevel`). Date: 2026-10-18. |
| `src/core/SpeakerAmplifier.h` | `src/new_protocol.c` | 868-882 (@4aa95c5) | `reference` | When the radio speaker amplifier is muted: the condition, in substance (side tone expected in CW and while tuning). R-SPK-09. |
| `src/core/codec/P2CodecOrionMkII.cpp` | `src/alex.h` | 129 (@4aa95c5) | `reference` | Name of P2 high-priority byte 1400 bit 1, `ANAN7000_HIPRIO1400_SPKR_MUTE`. R-SPK-09. |
| `src/core/session/StationServer.h` | `src/server_thread.c` | 925-934 (@4aa95c5) | `reference` | Remote session heartbeat: 15 s heartbeat against a 30 s receive timeout, the ratio of two behind the missed-pong count. |
| `src/gui/SpectrumWidget.cpp` | `src/receiver.c` | 571-582, 596 (@4aa95c5) | `reference` | Waterfall draws a row only when the analyzer has given new pixels; consulted for the key-edge row hold. |
