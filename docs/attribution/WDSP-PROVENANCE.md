# WDSP Provenance & License

WDSP (Warren Pratt NR0V's DSP library) is vendored in `third_party/wdsp/`.

## Upstream

- **Author:** Warren Pratt (NR0V, W5WJ)
- **Canonical repository:** https://github.com/TAPR/OpenHPSDR-wdsp
- **Version in NereusSDR:** v1.29 (as of 2025-02-XX), with **partial Thetis
  v2.10.3.13 sync** for `cfcomp.c` + `cfcomp.h` (see "Partial sync record"
  below).

## Partial sync record

| File | Status | Source | Date |
| --- | --- | --- | --- |
| `third_party/wdsp/src/cfcomp.c` | Partial sync to Thetis v2.10.3.13 (commit `501e3f5`) | `../Thetis/Project Files/Source/wdsp/cfcomp.c` | 2026-04-30 |
| `third_party/wdsp/src/cfcomp.h` | Partial sync to Thetis v2.10.3.13 (commit `501e3f5`) | `../Thetis/Project Files/Source/wdsp/cfcomp.h` | 2026-04-30 |

**Reason:** Phase 3M-3a-ii needs per-band Qg (gain skirt Q) and Qe (ceiling
skirt Q) on the SetTXACFCOMPprofile setter so the CFC dialog can ship with
full Thetis userland parity (nudCFC_q + nudCFC_cq per-band Q controls).
TAPR v1.29's 5-arg signature has no Qg/Qe.

**License delta:** Thetis v2.10.3.13 cfcomp headers carry a Richard Samphire
(MW0LGE) dual-licensing block — Copyright (c) 2026 — which lands verbatim in
the bundled `cfcomp.{c,h}` headers as part of this sync.  This is upstream's
own dual-license (GPLv2-or-later WDSP carries on as the floor — Samphire
reserves additional rights only over his own contributions).  The license
delta does NOT restrict any rights granted to NereusSDR under the GPL.

All other 140 WDSP source files remain at TAPR v1.29.  Full WDSP upstream
re-sync is out of scope for 3M-3a-ii — see `UPSTREAM-SYNC-PROTOCOL.md` §6
for the full-sync procedure.

## NereusSDR-original glue (not from upstream)

| File | Status | Reason | Date |
| --- | --- | --- | --- |
| `third_party/wdsp/src/txgain_stub.c` | NereusSDR-original glue stub (GPLv2-or-later, J.J. Boyd KG4VCF) | Provides `SetTXFixedGain` / `SetTXFixedGainRun` symbols so the bundled `wdsp_static` library exposes the API surface that Thetis's ChannelMaster module exports (`Project Files/Source/ChannelMaster/txgain.c [v2.10.3.13]`). NereusSDR has not yet ported the wider ChannelMaster module (only individual primitives like `cmbuffs.c` have landed in `src/core/audio/TxMicSource.{cpp,h}`); the stub stores per-channel `(Igain, Qgain)` plus a run flag in a flat static so the linker resolves and the C++ wrapper at `src/core/TxChannel.cpp::setTxFixedGain` (issue #167 Phase 1 Agent 1C) can be unit-tested. A future ChannelMaster-port phase can replace this stub with the byte-for-byte port of `txgain.c`. | 2026-05-03 |

The glue stub is GPLv2-or-later (compatible with the rest of `wdsp_static`)
and carries a verbatim NereusSDR-authored GPL header so
`scripts/verify-thetis-headers.py --kind=wdsp` passes.  The file's
`no-port-check:` marker exempts it from the Thetis-tells heuristic in
`scripts/check-new-ports.py` because it is NereusSDR-original code, not a
port.

## License Analysis

### Survey of Vendored Sources

All 143 source files in `third_party/wdsp/src/` were examined:

- **133 files** carry the full GPLv2-or-later permission block:
  - `"either version 2 of the License, or (at your option) any later version"`
  - All signal processing core: `channel.c`, `RXA.c`, `TXA.c`, `bandpass.c`, `amd.c`, `anf.c`, `anr.c`, and 125 others
  - `rnnr.c` + `rnnr.h` + `sbnr.c` + `sbnr.h` ported in Sub-epic C-1 (NR3/NR4 backends), carry verbatim Thetis GPLv2-or-later + MW0LGE dual-license headers.
  - `txgain_stub.c` (issue #167 Phase 1 Agent 1C, NereusSDR-original glue stub authored by J.J. Boyd KG4VCF, GPLv2-or-later — see row at top of "Vendored Source Files" table).
  - The other 132 files are Warren Pratt, NR0V, Copyright 2012–2025.
  - **Conclusion: GPLv2-or-later**

- **2 files** are the POSIX portability shim — authored jointly with John Melton:
  - `linux_port.c` / `linux_port.h`
  - Header: `Copyright (C) 2013 Warren Pratt, NR0V and John Melton, G0ORX/N6LYT`
  - These files translate Win32 synchronization primitives (`CRITICAL_SECTION`,
    `CreateSemaphore`, `WaitForSingleObject`, `InterlockedIncrement`, etc.) to
    POSIX equivalents so WDSP builds on Linux and macOS. They are co-authored
    by Warren Pratt (NR0V) and John Melton (G0ORX/N6LYT), with a subsequent
    macOS recursive-mutex fix contributed by Christoph van Wullen (DL1YCF) —
    see inline `// DL1YCF:` comment at `linux_port.c:49`.
  - **Conclusion: GPLv2-or-later** (same permission block as the rest of WDSP)

- **10 files** have no license headers (non-copyrightable or build infrastructure):
  - Data tables: `calculus.c`, `FDnoiseIQ.c` (noise lookup tables)
  - Generated files: `resource.h`, `resource1.h` (MSVC IDE artifacts)
  - Minimal wrappers: `version.c`, `version.h`, `fastmath.h` (empty), `calculus.h` (empty)
  - Third-party library: `fftw3.h` (FFTW3 header, GPLv2-or-later — same terms as the FFTW3 source upstream; the "BSD license" label in earlier versions of this doc was incorrect)
  - **Conclusion: Non-GPL sources or utility stubs; not blocking GPL compatibility**

- **No files** carry GPLv2-only language (`"version 2 only"`, `"GPLv2-only"`)

### Representative Files Spot-Checked

Five canonical files confirm uniform GPLv2-or-later:

1. **channel.c** (core channel management)
   - Copyright 2013 Warren Pratt NR0V
   - Permission: "either version 2 of the License, or (at your option) any later version"

2. **RXA.c** (RX audio processing pipeline)
   - Copyright 2013–2025 Warren Pratt NR0V
   - Permission: "either version 2 of the License, or (at your option) any later version"

3. **TXA.c** (TX audio processing pipeline)
   - Copyright 2013–2023 Warren Pratt NR0V
   - Permission: "either version 2 of the License, or (at your option) any later version"

4. **bandpass.c** (bandpass filter implementation)
   - Copyright 2013–2017 Warren Pratt NR0V
   - Permission: "either version 2 of the License, or (at your option) any later version"

5. **amd.c** (AM demodulator)
   - Copyright 2012–2013 Warren Pratt NR0V
   - Permission: "either version 2 of the License, or (at your option) any later version"

## Compatibility with NereusSDR

**NereusSDR is distributed under GPLv3** (see `/LICENSE` at the root).

GPLv3 permits aggregation of GPLv2-or-later code:
- GPLv3 §5(b) explicitly allows combining GPLv3 code with code "under the terms of version 2 or any later version of the GNU General Public License"
- WDSP's "or any later version" language satisfies this condition unambiguously

**Result: WDSP v1.29 (GPLv2-or-later) is fully compatible with NereusSDR's GPLv3 distribution.**

## NereusSDR Modifications

The vendored WDSP tree is unmodified from upstream and retains all upstream copyright notices and license headers. NereusSDR wraps WDSP via:

- `src/core/WdspEngine.cpp` — WDSP lifecycle manager
- `src/core/RxChannel.cpp` — RX channel DSP wrapper
- `src/core/TxChannel.cpp` — TX channel DSP wrapper

These wrappers are authored by NereusSDR contributors and carry NereusSDR (GPLv3) headers with Thetis/WDSP attribution in comments (see `docs/attribution/HEADER-TEMPLATES.md` for full attribution chain).

## Attribution Chain

1. **WDSP original** ← Warren Pratt NR0V, TAPR, GPLv2-or-later
2. **WDSP POSIX shim** (`linux_port.{c,h}`) ← Warren Pratt NR0V + John Melton G0ORX/N6LYT (co-authors, 2013), with macOS mutex fix from Christoph van Wullen DL1YCF; GPLv2-or-later; distributed via both `TAPR/OpenHPSDR-wdsp` and `g0orx/wdsp`
3. **Thetis port** ← ramdor + contributors, GPLv2-or-later (Thetis itself includes WDSP)
4. **NereusSDR port** ← JJ Boyd KG4VCF + contributors, GPLv3 (with Thetis upstream attribution preserved in source headers)

Per compliance notice §6.1, the WDSP attribution is already preserved in the source tree headers and in NereusSDR wrapper source comments pointing to the Thetis contributor chain.
