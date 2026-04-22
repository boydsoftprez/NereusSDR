# Phase 3G — RX Experience Epic

**Status:** Design locked, ready for per-sub-epic plan writing
**Date:** 2026-04-22
**Author:** J.J. Boyd ~ KG4VCF
**Upstream stamps at time of spec:**
- Thetis `v2.10.3.13` @ `501e3f5`
- AetherSDR `main` @ `0cd4559` (2026-04-21, fast-forwarded local fork from `upstream/main` as the first act of this epic)

## Context

NereusSDR's receive-side experience is strong on the core signal chain (Protocol 1 + 2 decode, WDSP integration, spectrum + waterfall GPU rendering, CTUN + zoom) but thin on user-facing polish: no right-edge dBm labels on the spectrum, no noise blanker at all, one placeholder NR toggle, no bandplan overlay, no waterfall history scrollback. All five of these are present in mature form in AetherSDR and/or Thetis. This epic ports them as a coordinated group so their shared concerns (overlay z-order, per-slice persistence, attribution) are decided once.

## Scope (5 sub-epics)

### A — Right-edge dBm scale strip
Port AetherSDR's painted right-edge amplitude scale with click-arrows, hover highlight, drag-to-pan reference, and wheel-to-zoom-range. Currently NereusSDR draws horizontal dBm grid lines but shows no numeric labels on the spectrum face itself.

### B — Noise Blanker family (NB / NB2 / SNB)
Port Thetis's WDSP noise blanker stack. Three user-visible filters sharing a single `NbFamily` wrapper that exposes NB (Whitney, `nob.c`), NB2 (second-gen, `nobII.c`), and SNB (spectral, `snb.c`). `nbp.c` is a general multi-bandpass used internally by NB2 and is not a user-facing toggle. NB and NB2 are mutually exclusive via a cycling toggle; SNB is an independent toggle.

### C — Noise Reduction (7 filters)
Seven client-side NR algorithms, at most one active per slice at a time, following AetherSDR's mutual-exclusion model:

| Slot | Label | Source | Backend |
|---|---|---|---|
| 1 | **NR1 (ANR)** | Thetis `anr.c` port | WDSP-derived LMS leaky |
| 2 | **NR2** | AetherSDR `SpectralNR` | Native C++ port of WDSP `emnr.c` (MMSE-LSA) |
| 3 | **RN2** | AetherSDR `RNNoiseFilter` | Xiph/Mozilla rnnoise (BSD) |
| 4 | **NR4** | AetherSDR `SpecbleachFilter` | libspecbleach (LGPL-2.1) |
| 5 | **DFNR** | AetherSDR `DeepFilterFilter` | DeepFilterNet3 ONNX (MIT/Apache-2.0) |
| 6 | **BNR** *(Windows+NVIDIA only)* | AetherSDR `NvidiaBnrFilter` | NVIDIA Broadcast SDK (EULA) |
| 7 | **MNR** *(macOS only)* | AetherSDR `MacNRFilter` | Apple Accelerate vDSP (system) |

Thetis NR3 and Thetis NR4 are **not** ported because their underlying libraries (rnnoise, libspecbleach) are already in the AetherSDR port via slots 3 and 4 respectively.

### D — Bandplan overlay
Port AetherSDR's `BandPlanManager` with five bundled JSON files (`arrl-us`, `iaru-region1`, `iaru-region2`, `iaru-region3`, `rac-canada`). New `BandplanModel` owns current region + loaded segments; SpectrumWidget gains an overlay layer that paints semi-transparent mode-coloured bands with labels beneath the spectrum trace. Region switcher lives in Setup → Display.

### E — Waterfall scrollback (20-min ring buffer)
Port AetherSDR's `ensureWaterfallHistory()` / `historyRowIndexForAge()` / `m_wfHistoryTimestamps[]` machinery from [SpectrumWidget](../../../../AetherSDR/src/gui/SpectrumWidget.cpp). Defaults: 20-minute buffer, capped at 4096 rows, drag-up-to-scrollback gesture, per-row timestamp + center-frequency metadata so history survives pan/zoom. Adapt AetherSDR's QImage history buffer to feed NereusSDR's QRhi waterfall texture via a CPU-side history image + GPU texture update keyed off scroll offset.

## Out of scope (explicitly)

- **Thetis NR3 (rnnr.c)** — redundant with slot 3 (RN2/rnnoise)
- **Thetis NR4 (libspecbleach)** — redundant with slot 4 (NR4/SpecbleachFilter)
- **Thetis ANF** — belongs to a separate notch/ANF epic
- **Disk-spooled waterfall recording** — belongs to Phase 3M (Recording)
- **Server-side Flex NR modes (NR / NRL / NRS / RNN)** — AetherSDR wire-protocol labels that don't apply to OpenHPSDR radios
- **Bandplan data editor** — read-only bundled JSONs only for first pass; user-editable bandplans deferred
- **mi0bot-Thetis additions** — no unique NR/NB value over Thetis proper or AetherSDR for this epic

## Ordering

```
Phase 0: AetherSDR sync  (DONE, not a tracked sub-epic)
         │
         ▼
  A. Right-edge dBm scale strip
         │
         ▼
  B. Noise Blanker (NB / NB2 / SNB)
         │
         ▼
  C. Noise Reduction (4-stage sub-plan: C-1 cross-platform / C-2 BNR / C-3 MNR / C-4 DFNR)
         │
         ▼
  D. Bandplan overlay  ←  (can be parallelised with C by a second session)
         │
         ▼
  E. Waterfall scrollback
```

**Why this order:**

- **A first** — smallest, zero DSP risk, immediate user-visible win, establishes right-edge coordinate math that E reuses for its time-strip
- **B second** — pure WDSP wiring (we already link WDSP), no third-party libs, gives Thetis users parity quickly
- **C third** — largest, highest third-party-lib surface area, deserves sub-staging
- **D parallelisable** with C — pure overlay, no DSP dependency
- **E last** — touches GPU renderer (highest integration risk), benefits from A's settled right-edge geometry

## Per-sub-epic briefs

### A — Right-edge dBm scale strip

**Sources:**
- AetherSDR [SpectrumWidget.cpp:4854-4921](../../../../AetherSDR/src/gui/SpectrumWidget.cpp) (strip paint)
- AetherSDR SpectrumWidget:1712-1741 (click-arrow hit-test)
- AetherSDR SpectrumWidget:2110-2115 (drag-to-pan reference)
- AetherSDR SpectrumWidget:2243 (hover feedback)
- AetherSDR SpectrumWidget:2526 (double-click consumption)
- AetherSDR SpectrumWidget:2630 (wheel zoom range)

**Files touched:**
- `src/gui/SpectrumWidget.{h,cpp}` — add `drawDbmScaleStrip()`, `m_dbmStripRect`, hit-test helpers, drag state (`m_dbmDragStartY`, `m_dbmDragStartRef`), hover state
- `src/gui/setup/DisplaySetupPages.cpp` — new "Show dBm scale strip" toggle (`DisplayDbmScaleVisible`, default true)

**Behaviour contract:**
- Strip is N pixels wide (AetherSDR uses ~40 px; verify at port time) on the right edge of the spectrum rect
- Shows integer dBm labels aligned to the major horizontal grid step
- Top/bottom arrows adjust dynamic range (keep refLevel fixed, adjust floor)
- Click-drag on strip body pans refLevel (both min and max move together)
- Wheel on strip zooms range around cursor dBm
- Double-click consumed (prevents spectrum double-click from triggering here)

**Shipping unit:** 1 PR.

### B — Noise Blanker family (NB / NB2 / SNB)

**Sources:**
- Thetis WDSP `nob.c` / `nob.h` — Whitney blanker (NB)
- Thetis WDSP `nobII.c` / `nobII.h` — second-gen (NB2)
- Thetis WDSP `snb.c` / `snb.h` — Spectral Noise Blanker (SNB)
- Thetis WDSP `nbp.c` / `nbp.h` — internal bandpass (not user-facing)
- Thetis `dsp.cs` — `SetRXANBRun` / `SetRXANB2Run` / `SetRXASNBARun` P/Invoke signatures
- Thetis `console.cs:42475,43525` — cycling-toggle UI logic

**Files touched:**
- `third_party/wdsp/` — ensure `nob.c`, `nobII.c`, `snb.c`, `nbp.c` are in the build (TAPR v1.29 should already have these; verify)
- `src/core/RxChannel.{h,cpp}` — add `setNbMode(NbMode)` + `setSnbEnabled(bool)` + NB tuning params (lag, threshold, hangtime, advtime)
- `src/core/NbFamily.{h,cpp}` *(new)* — small wrapper that owns WDSP NB state and exposes clean Qt-style setters
- `src/models/SliceModel.{h,cpp}` — per-slice NB state (mode + SNB enable + tuning overrides)
- `src/gui/widgets/VfoWidget.cpp` — cycling NB button (NB → NB2 → off) + separate SNB toggle
- `src/gui/applets/RxApplet.cpp` — NB tuning controls (lag slider, threshold slider)
- `src/gui/setup/DspSetupPages.cpp` — global NB defaults page

**Behaviour contract:**
- NB button cycles `off → NB → NB2 → off`; label text changes to match mode (`"NB"` / `"NB2"` / dim)
- SNB toggle is independent; both NB and SNB can run simultaneously (SNB is a spectral stage inside WDSP channel, NB is time-domain pre-filter)
- NB tuning params persist per-slice-per-band (`Slice0NbLag_40m` etc.)
- Thetis default values preserved byte-for-byte from `console.cs` with inline-cite comments

**Shipping unit:** 1 PR.

### C — Noise Reduction (7-filter stack)

**Sub-stages:**

- **C-1 — Cross-platform NR (5 filters)**: NR1(ANR) + NR2(SpectralNR) + RN2(rnnoise) + NR4(specbleach) + DFNR(DeepFilterNet3). Core audio-path pipeline in AudioEngine, mutual-exclusion contract, DSP dialog tabs. Windows/macOS/Linux.
- **C-2 — BNR (NVIDIA Broadcast)**: Windows-only, NVIDIA SDK runtime loaded from user's NVIDIA install. Button hidden on other platforms.
- **C-3 — MNR (Apple Accelerate)**: macOS-only. `#ifdef __APPLE__` gate. Button hidden on other platforms. DSP panel widens 200 → 280 px on Apple builds to fit the extra toggle (AetherSDR pattern).
- **C-4 — UI polish pass**: right-click → DSP Settings on correct tab, tooltip coverage, per-slice-per-band persistence verified, slice-troubleshooting dialog reads current NR state.

Stages C-2 / C-3 / C-4 can land in either order after C-1. C-1 is the blocking foundation.

**Sources (C-1):**
- Thetis WDSP `anr.c` / `anr.h` — LMS leaky
- AetherSDR [SpectralNR.{h,cpp}](../../../../AetherSDR/src/core/SpectralNR.h) — native port of `emnr.c`
- AetherSDR [RNNoiseFilter.{h,cpp}](../../../../AetherSDR/src/core/RNNoiseFilter.h) — rnnoise wrapper
- AetherSDR [SpecbleachFilter.{h,cpp}](../../../../AetherSDR/src/core/SpecbleachFilter.h) — libspecbleach wrapper
- AetherSDR [DeepFilterFilter.{h,cpp}](../../../../AetherSDR/src/core/DeepFilterFilter.h) — DeepFilterNet3 wrapper
- AetherSDR `third_party/libspecbleach/` (bundled)
- AetherSDR `third_party/deep-filter-sys/` (bundled) + model tar.gz
- AetherSDR `third_party/rnnoise/` (bundled)
- AetherSDR [AetherDspDialog.cpp](../../../../AetherSDR/src/gui/AetherDspDialog.cpp) — per-algorithm tabs
- AetherSDR [AudioEngine.cpp:951-1326](../../../../AetherSDR/src/core/AudioEngine.cpp) — filter instantiation + mutual-exclusion wiring

**Sources (C-2):** AetherSDR [NvidiaBnrFilter.{h,cpp}](../../../../AetherSDR/src/core/NvidiaBnrFilter.h) + AetherSDR CMakeLists BNR block.

**Sources (C-3):** AetherSDR `MacNRFilter.{h,cpp}` @ commit 71c0396 + AetherSDR CMakeLists MNR block + AetherSDR AudioEngine `#ifdef __APPLE__` block.

**Sources (C-4):** AetherSDR DSP dialog open-to-tab + right-click flow + sliceTroubleshootingDialog DSP readout.

**Files touched (across C-1 through C-4):**
- `third_party/libspecbleach/` *(new, bundled)*
- `third_party/deep-filter-sys/` + model tar.gz *(new, bundled)*
- `third_party/rnnoise/` *(new, bundled)*
- `src/core/wdsp_anr.{h,cpp}` *(new — Thetis anr.c native C++ port)*
- `src/core/SpectralNR.{h,cpp}` *(new, from AetherSDR)*
- `src/core/RNNoiseFilter.{h,cpp}` *(new, from AetherSDR)*
- `src/core/SpecbleachFilter.{h,cpp}` *(new, from AetherSDR)*
- `src/core/DeepFilterFilter.{h,cpp}` *(new, from AetherSDR)*
- `src/core/NvidiaBnrFilter.{h,cpp}` *(new, from AetherSDR, Windows-gated)*
- `src/core/MacNRFilter.{h,cpp}` *(new, from AetherSDR, macOS-gated)*
- `src/core/AudioEngine.{h,cpp}` — filter ownership + mutual-exclusion dispatch + FFTW wisdom generation for SpectralNR
- `src/models/SliceModel.{h,cpp}` — `activeNr` enum + per-algorithm-per-band parameter storage
- `src/gui/AetherDspDialog.{h,cpp}` *(new — renamed to `NrDspDialog` or similar)* from AetherSDR
- `src/gui/widgets/VfoWidget.cpp` — NR button + right-click → DSP dialog
- `src/gui/SpectrumOverlayPanel.cpp` — NR row in overlay panel (AetherSDR pattern)
- `CMakeLists.txt` — `ENABLE_NR4` / `ENABLE_DFNR` / `ENABLE_BNR` / `ENABLE_MNR` options + third-party build integration

**Behaviour contract:**
- Exactly one of {NR1, NR2, RN2, NR4, DFNR, BNR, MNR} active per slice at any time
- Turning on any NR slot turns all others off for that slice
- Per-slice-per-band parameter persistence (`Slice0Nr2ReductionAmount_40m`, `Slice0Nr4SmoothingFactor_20m`, etc.) — follows existing 3G-10 per-slice-per-band pattern
- `Slice<N>NrActive` is the single source of truth for which NR slot is enabled on a slice. Per-algorithm *parameters* persist independently of active state so the user's last-used tuning survives a slot switch and a restart.
- NR runs on demodulated 24 kHz stereo Int16 audio inside AudioEngine (post-WDSP demod)
- BNR button hidden on non-Windows / non-NVIDIA; MNR button hidden on non-macOS — same platform-gating pattern as AetherSDR
- DFNR model file (`DeepFilterNet3_onnx.tar.gz`) bundled in macOS Resources, Linux AppImage payload, Windows portable ZIP
- Right-click any NR button → opens DSP settings dialog scrolled to that algorithm's tab

**Shipping unit:** 4 PRs (one per sub-stage). Sub-stage plans written separately via `writing-plans`.

### D — Bandplan overlay

**Sources:**
- AetherSDR [BandPlan.h](../../../../AetherSDR/src/models/BandPlan.h) — `BandSegment` struct + inline ARRL-US table
- AetherSDR [BandPlanManager.{h,cpp}](../../../../AetherSDR/src/models/BandPlanManager.h) — region switcher + JSON loader
- AetherSDR `resources/bandplans/*.json` — 5 bundled JSONs (`arrl-us`, `iaru-region1`, `iaru-region2`, `iaru-region3`, `rac-canada`)
- AetherSDR `resources.qrc` — bandplan resource registration

**Files touched:**
- `resources/bandplans/*.json` *(new, copied from AetherSDR)*
- `resources/resources.qrc` — bandplan registration
- `src/models/BandPlan.{h,cpp}` *(new)* — BandSegment + loader
- `src/models/BandplanModel.{h,cpp}` *(new)* — active region + segment list + signals
- `src/gui/SpectrumWidget.{h,cpp}` — `drawBandplanOverlay(specRect)` painted beneath the spectrum trace
- `src/gui/setup/DisplaySetupPages.cpp` — region dropdown + overlay-enable toggle
- `src/models/RadioModel.{h,cpp}` — wire BandplanModel

**Behaviour contract:**
- One active region globally (AppSettings `BandplanRegion` = `"arrl-us"` | `"iaru-r1"` | `"iaru-r2"` | `"iaru-r3"` | `"rac"`). Default selected from user's locale at first run.
- Segments render as semi-transparent coloured bands (alpha ~0.25) beneath the spectrum trace
- Mode labels (CW / SSB / DATA / BCN / etc.) painted at segment top when width > 40 px, abbreviated or hidden when narrow
- Toggleable via overlay panel button (`BandplanOverlayEnabled`, default on)
- Segments outside the current panadapter window are clipped cheaply

**Shipping unit:** 1 PR.

### E — Waterfall scrollback

**Sources:**
- AetherSDR [SpectrumWidget.cpp](../../../../AetherSDR/src/gui/SpectrumWidget.cpp) — `ensureWaterfallHistory()` @ line ~594, `historyRowIndexForAge()` @ ~570, `maxWaterfallHistoryOffsetRows()` @ ~565, `waterfallHistoryCapacityRows()` @ ~559
- AetherSDR commits `98b57ab` (initial scrollback), `74f7e98` (scrub direction), `2bb3b5c` (4096-row cap + resize debounce), `c5ad5c1` (pan/zoom sync), `8890d6f` (width-change preservation)

**Files touched:**
- `src/gui/SpectrumWidget.{h,cpp}` — ring buffer (`m_waterfallHistory` QImage + `m_wfHistoryTimestamps` qint64 array + `m_wfHistoryWriteRow` + `m_wfHistoryRowCount` + `m_wfHistoryOffset` for scrub position)
- GPU renderer path — when `m_wfHistoryOffset > 0`, upload the past-N-rows window of the history image to the waterfall texture instead of the live row. When offset = 0, resume normal live appends.
- Scroll-gesture handler in SpectrumWidget event path — drag up = scroll back, drag down = scroll forward; released at offset 0 resumes live
- Timestamp + center-freq overlay at cursor when `m_wfHistoryOffset > 0`
- `src/gui/setup/DisplaySetupPages.cpp` — history-depth selector (60 s / 5 min / 15 min / 20 min, persisted as `DisplayWaterfallHistoryMs`)

**Behaviour contract:**
- Default buffer depth: 20 minutes (`kWaterfallHistoryMs = 1200000`)
- Cap: 4096 rows regardless of configured depth
- Drag-up gesture on waterfall = scroll back in time (AetherSDR's ergonomic direction)
- Auto-resume live when scrolled back to offset 0 (cursor released at newest row)
- Width resize preserves history via horizontal QImage::scaled (AetherSDR pattern)
- Pan/zoom during scrollback: per-row center-freq metadata lets us detect and either blank or reproject (match AetherSDR)
- Timestamp of cursor row + age ("-3m 42s") shown in overlay while scrolling
- Pure in-memory (no disk); lost on panadapter close or app restart

**Shipping unit:** 1 PR.

## Shared concerns

### AppSettings key namespace

| Prefix | Scope | Examples |
|---|---|---|
| `Display*` | per-SpectrumWidget | `DisplayDbmScaleVisible`, `DisplayWaterfallHistoryMs`, `DisplayDbmStripWidthPx` |
| `Bandplan*` | global | `BandplanRegion`, `BandplanOverlayEnabled`, `BandplanSegmentAlpha` |
| `Slice<N>Nr*` | per-slice | `Slice0NrActive` = `"NR2"`\|`"NR4"`\|…\|`""`, `Slice0Nr2GainMax`, `Slice0Nr4ReductionAmount_40m` |
| `Slice<N>Nb*` | per-slice | `Slice0NbMode` (`"NB"`/`"NB2"`/`""`), `Slice0SnbEnabled`, `Slice0NbLag_40m` |

Per-slice-per-band keys use `_<band>` suffix following the existing 3G-10 convention (`Slice0*_40m`, `Slice0*_20m`, etc.).

### Panadapter overlay z-order (bottom → top)

```
spectrum fill / gradient
horizontal dB grid lines
vertical freq grid lines
bandplan segments (semi-transparent)       ← NEW (D)
spectrum trace + peak hold
filter passband shading
TNF / band-edge markers
VFO cursor lines
right-edge dBm scale strip                 ← NEW (A)  — painted OVER everything in its rect
scrollback time cursor + age tooltip       ← NEW (E)  — painted OVER everything during scrub
```

The right-edge dBm strip (A) and the waterfall scrollback cursor (E) paint on top of every other layer in their respective rects. Bandplan (D) paints beneath the trace so signals aren't visually obscured.

### Mutual exclusion & per-slice policy

- **NR:** exactly one of `{NR1, NR2, RN2, NR4, DFNR, BNR, MNR}` active per slice at a time; enabling one disables others for that slice (enforced by `SliceModel::setActiveNr(NrSlot)`)
- **NB:** `NB` and `NB2` mutually exclusive via cycling toggle on a single `NbMode` enum; `SNB` independent
- **Scrollback:** per-panadapter (each `SpectrumWidget` owns its own ring buffer)
- **Bandplan region:** global (one region app-wide, not per-slice)
- **dBm scale strip:** per-panadapter (each `SpectrumWidget` paints its own)

### Attribution

Each new file carries its upstream file-header byte-for-byte per `docs/attribution/HOW-TO-PORT.md`, plus the NereusSDR modification block. The pre-commit hook `scripts/verify-inline-tag-preservation.py` enforces inline-tag preservation on every port. License-level bundle:

| Source | License | Distribution |
|---|---|---|
| Thetis `anr.c` / `nob.c` / `nobII.c` / `snb.c` / `nbp.c` | GPLv2+ | Source only, per-file header preserved |
| AetherSDR `SpectralNR` (native C++ port of WDSP `emnr.c`) | GPLv2+ (AetherSDR-maintained chain) | AetherSDR file-header copied byte-for-byte, modification block appended |
| AetherSDR `RNNoiseFilter` / `SpecbleachFilter` / `DeepFilterFilter` / `NvidiaBnrFilter` / `MacNRFilter` | GPLv3 (AetherSDR wrapper) + upstream license | Wrapper headers copied; third-party licenses bundled in `third_party/*/LICENSE` |
| libspecbleach | LGPL-2.1 | `third_party/libspecbleach/` bundled; LICENSE preserved |
| DeepFilterNet3 model + df-sys | MIT / Apache-2.0 | `third_party/deep-filter-sys/` bundled; model tar.gz in per-platform Resources |
| rnnoise | BSD | `third_party/rnnoise/` bundled |
| NVIDIA Broadcast SDK | NVIDIA EULA | **Not redistributed.** Runtime-loaded from user's NVIDIA install on Windows; absent elsewhere |
| Apple Accelerate (vDSP) | Apple system lib | No distribution concern (macOS-only, system-provided) |
| AetherSDR `BandPlanManager` + JSONs | GPLv3 (AetherSDR) | AetherSDR header preserved; ARRL data is factual (not copyrightable) |
| AetherSDR SpectrumWidget scrollback + dBm strip paint paths | GPLv3 (AetherSDR) | AetherSDR header + per-function cite comments (`// From AetherSDR SpectrumWidget.cpp:<N> [@0cd4559]`) |

**GPLv2 vs GPLv3:** Thetis's GPLv2-or-later clause makes it upward-compatible with GPLv3. AetherSDR is GPLv3. NereusSDR's combined work under GPLv3 is license-compatible. No conflict.

## Acceptance heuristic (per sub-epic)

Each sub-epic PR merges when:
1. All new code passes `verify-inline-tag-preservation.py` and `verify-thetis-headers.py` (existing Ring 3 CI checks)
2. Unit / integration tests added (examples: NR filter bypass correctness, NB click-reduction on a seeded impulse sample, bandplan JSON round-trip, scrollback ring-buffer wrap-around, dBm strip hit-test)
3. Manual verification matrix written in `docs/architecture/<sub-epic>-verification/README.md` following the 3G-8 model
4. Settings round-trip verified (quit → relaunch preserves every user-visible state change)
5. CHANGELOG entry + screenshot in PR body

## Open questions deferred to per-sub-epic plans

- Exact dBm strip pixel width (AetherSDR uses ~40 px; measure and port)
- FFTW wisdom generation UX for SpectralNR (already present for the main spectrum FFT — does SpectralNR reuse the wisdom file or generate its own?)
- DeepFilterNet3 model file size + bundle impact per platform (measure at C-4 time)
- Bandplan overlay default colour alpha (AetherSDR uses ARRL colours at full saturation for label backgrounds; we may want lower alpha to keep the trace readable)
- Scrollback buffer memory cap when the configured depth × panadapter-width would exceed the 4096-row cap (AetherSDR just clamps rows; verify that's the right behaviour)

These are research tasks, not design decisions — they get answered during `writing-plans` for their sub-epic.

## References

**Upstream roots:**
- Thetis: https://github.com/ramdor/Thetis — `v2.10.3.13` @ `501e3f5`
- AetherSDR: https://github.com/ten9876/AetherSDR — `main` @ `0cd4559` (2026-04-21)
- WDSP: https://github.com/TAPR/OpenHPSDR-wdsp

**Key upstream commits:**
- AetherSDR `98b57ab` — Add waterfall history scrollback with 20-minute ring buffer (#1432)
- AetherSDR `74f7e98` — Invert waterfall scrub direction: pull up to scroll back in time
- AetherSDR `2bb3b5c` — Cap waterfall history buffer at 4096 rows and debounce resize re-allocation (#1478)
- AetherSDR `c5ad5c1` — Sync waterfall pan and zoom history (#1314)
- AetherSDR `8890d6f` — Preserve waterfall history on width change instead of blanking (#1608)
- AetherSDR `71c0396` — macOS MMSE-Wiener spectral noise reduction (MNR) (#1672)

**Internal:**
- `docs/attribution/HOW-TO-PORT.md` — porting + attribution protocol
- `docs/architecture/phase3g8-rx1-display-parity-plan.md` — 3G-8 pattern reference for per-slice-per-band persistence + verification matrix
