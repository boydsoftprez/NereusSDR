# Longpath Provenance: NereusSDR derived-file inventory

This document catalogs every NereusSDR source file derived from, translated
from, or materially based on Longpath (oe5sos/Longpath). Per-file headers
live in the source files themselves; this index is the grep-able summary.

NereusSDR is distributed under GPLv3 (root `LICENSE`). Longpath is a fork of
NereusSDR and is distributed under the same licence. See the License section
below.

## When entries get added

A row is added to the table below, in the **same commit** that introduces
the ported logic, whenever a NereusSDR file ports, translates, or materially
re-expresses logic from a Longpath source file. The procedure is the one in
`HOW-TO-PORT.md`:

- Copy the Longpath file's header block (its `// ===` banner,
  "Longpath-original" note and "Modification history (Longpath)" block, or
  the leading comment block where a file has no banner) byte for byte under
  `// --- From <file> ---`, below a NereusSDR port-citation block and a
  "Modification history (NereusSDR)" block.
- Add a `// From Longpath <path>:<line> [@<sha>]` inline cite at every
  ported function, type and constant.
- Keep every inline comment inside ported logic verbatim, German ones
  included.
- User-facing strings are reworded in plain English operator words, with no
  dashes and no cites; NereusSDR's palette replaces Longpath's.

## Upstream

- **Project:** Longpath
- **Repository:** https://github.com/oe5sos/Longpath
- **Lineage:** a fork of NereusSDR, renamed; rotor work since 2026-08-07.
- **Maintainer:** Martin Fischer, OE5SOS
- **Local reference clone:** `../Longpath/`, pinned at `551576e`
  (v0.6.7-1). Never pulled mid-task; re-pinning is deliberate and re-verifies
  every cite.
- **Language:** C++20 / Qt6

## License

Longpath ships the GNU General Public License version 3 as its root
`LICENSE`, with NereusSDR's `LICENSE-NOTICE` and the Samphire
`LICENSE-DUAL-LICENSING` statement carried over from NereusSDR. The rotor
files ported so far are Longpath-original (no Thetis code, no Samphire
contribution) and carry no per-file GPL header; the project-level `LICENSE`
applies, and each NereusSDR file says so in its port-citation block.
NereusSDR is also GPLv3, so no compatibility note is needed.

## Legend

Derivation type:
- `port`       : direct reimplementation in NereusSDR of a Longpath source file
- `reference`  : consulted for behavior during independent implementation
- `structural` : architectural template with substantive behavioral echo

## Files derived from oe5sos/Longpath

| NereusSDR file | Longpath source | Line ranges | Type | Notes |
| --- | --- | --- | --- | --- |
| `src/core/RotorHeading.h` | `src/core/RotorPeilung.h` | 1-52 (`kHoechstens`, `lies()`) | `port` | Strict heading checks: empty, not-a-number, infinite, below 0 and above 360 refused, never wrapped; 360 is north and is sent as 0. `lies()` becomes `parse()`; `accept()` applies the same checks to a number. German comments kept verbatim. Rotor control plan, Task 3a. Date: 2026-10-08. |
| `src/core/RotorRoute.h` | `src/core/BeamHeading.h` | 1-91 (`Stop`, `Move`, `plan()`, `wrap360()`) | `port` | `Stop` becomes `EndStop` (the wire's `endStop` enum); `Move` gains `routeKnown` and `targetSpanDeg`. Extended to rotors with overlap (span from the counter-clockwise stop, up to 450 degrees, nearer of two span positions) and `SpanTracker`, NereusSDR's own, which follows modulo-360 replies by continuity. `greatCircle()`, `longPath()` and `advice()` not taken. Rotor control plan, Task 3a. Date: 2026-10-08. |
| `src/core/RotorRoute.cpp` | `src/core/BeamHeading.cpp` | 1-89 (`wrap360()`, `plan()`) | `port` | `plan()` split into `planFree` (no stop, shorter way) and `planOnSpan` (from the rotor's span position, overlap aware); the "long way round" note kept above 270 degrees of travel, reworded. Rotor control plan, Task 3a. Date: 2026-10-08. |
| `src/core/RotorModels.h` | `src/core/RotorModels.h` | 1-117 (`RotorModel`, `commonRotorModels()`, `commonRotorBauds()`) | `port` | The curated Hamlib model list and baud rates. Every model number checked again against Hamlib master `include/hamlib/rotlist.h` (`@50fc454`, read 2026-10-08), cited per entry; names and notes reworded in plain English. Rotor control plan, Task 3a. Date: 2026-10-08. |
