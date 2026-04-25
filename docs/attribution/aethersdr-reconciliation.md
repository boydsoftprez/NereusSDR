# AetherSDR Reconciliation — per-file classification

Phase 4 Task 25b output. Consumed by Task 25c to apply corrections.

Scope: every NereusSDR source file that names "AetherSDR" in its
Modification-History block **or** in inline comments (158 src + 18 tests
= 176 files). Each file is classified into exactly one bucket; the
"25c action" column records the mechanical correction to apply.

Input: `aethersdr-contributor-index.md` (Task 25a) and a spot-check of
the AetherSDR clone at `/Users/j.j.boyd/AetherSDR/`.

## Summary

| Bucket | Meaning | Count | 25c action |
|---|---|---:|---|
| **A** | Genuine AetherSDR derivation — add project-level attribution | 48 | Insert AetherSDR copyright line + specify source file(s) in mod-history |
| **B** | False citation (boilerplate "Structural template follows AetherSDR" with no real counterpart) — remove line | 126 | Delete the two "Structural template follows AetherSDR (ten9876/AetherSDR) Qt6 conventions." lines from Modification-History |
| **C** | Mixed lineage — keep both citations, tighten wording | 12 | Keep Thetis block + AetherSDR line, but say what came from where |
| **D** | Uncertain — human review | 5 | Flag for human judgement; do not auto-edit |
| | **Total** | **176** | |

Breakdown of how the 176 files break down by source of citation:
- 135 files carry the Modification-History boilerplate line
  *"Structural template follows AetherSDR (ten9876/AetherSDR) Qt6 conventions."*
  — of those, 126 go to Bucket B (delete), 3 go to Bucket A (replace
  with specific wording), and 6 go to Bucket C (tighten wording).
- 41 files cite AetherSDR only in inline comments (no boilerplate line)
  — of those, 30 go to Bucket A (add attribution), 6 go to Bucket C
  (tighten where mixed), and 5 go to Bucket D (incidental comment, no
  action).

### Guiding test used

A file is in **Bucket A** when one or more of:
- It has *inline* `// From AetherSDR <file>:<line>` comments pointing at
  specific AetherSDR source lines that a compliance reviewer could open
  and compare against.
- Its class structure/Q_PROPERTY shape visibly matches a named AetherSDR
  class (RadioModel, SliceModel, VfoWidget, SpectrumWidget, FilterPassbandWidget,
  ConnectionPanel's SmartSDR-era pattern, AppletPanel, EqApplet, TxApplet,
  PhoneCwApplet, RxApplet, SpectrumOverlayMenu → SpectrumOverlayPanel, the
  GuardedSlider primitives, AudioEngine makeFormat/drain pattern).
- 25a index §"Per-file contributor summary" explicitly maps the file to
  an AetherSDR counterpart.

A file is in **Bucket B** when:
- The only AetherSDR reference is the two-line boilerplate "Structural
  template follows AetherSDR (ten9876/AetherSDR) Qt6 conventions." in the
  Modification-History block,
- AND there is no AetherSDR counterpart in the 25a index (or the index
  explicitly calls out "NOT present in AetherSDR"),
- AND the file body is already attributed to Thetis (explicit "Ported
  from Thetis source" or equivalent) in its Copyright block.

A file is in **Bucket C** when it genuinely derives from both Thetis AND
AetherSDR — Thetis supplies the behaviour (algorithms, state machine,
feature rules), AetherSDR supplies the Qt6 skeleton (class layout,
signal/slot shape, floating-applet mechanics, GPU-pipeline scaffolding).

A file is in **Bucket D** when the header block is missing, or when the
AetherSDR citation isn't formal (inline-only with no explicit mod-history
claim), or when 25a couldn't positively disprove the claim.

---

## Bucket A — Genuine AetherSDR derivations (48 files)

25c action for every file below:
1. Add a project-level attribution line to the Copyright block (after
   the Thetis line if present, before the permission block):
   ```
   //   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
   //       — per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
   //       and About dialog for the live contributor list)
   ```
   Rationale: AetherSDR has no per-file copyright headers, so we cannot
   "copy" what isn't there; we reference the project-level attribution
   that the upstream LICENSE + About dialog establish.
2. Replace the boilerplate Modification-History sentence
   *"Structural template follows AetherSDR (ten9876/AetherSDR) Qt6
   conventions."* with a **specific** sentence naming the AetherSDR
   counterpart file(s) — the column below lists them.

### Models (structural templates — strongest borrows)

| NereusSDR file | AetherSDR counterpart | Evidence | Specific mod-history wording |
|---|---|---|---|
| `src/models/RadioModel.h` | `src/models/RadioModel.{h,cpp}` | 25a index §Per-file (RadioModel is the "Main structural template" for NereusSDR's hub pattern). | "Structural template (state-hub pattern, sub-model ownership, main-thread signal routing) ported from AetherSDR `src/models/RadioModel.{h,cpp}`." |
| `src/models/SliceModel.h` | `src/models/SliceModel.{h,cpp}` | Inline comment line 84: "From AetherSDR SliceModel pattern: Q_PROPERTY + signals for each state." Line 184: rxAntenna/txAntenna pattern. 25a index §Per-file (slice-template match). | "Per-slice `Q_PROPERTY` + signal shape ported from AetherSDR `src/models/SliceModel.{h,cpp}` (NereusSDR swaps SmartSDR slice/pan fields for OpenHPSDR DDC/receiver fields; DSP behaviour is Thetis, see Copyright block)." |
| `src/models/BandPlan.h` | `src/models/BandPlanManager.h` | File header line 5: "Ported from AetherSDR src/models/BandPlanManager.h [@0cd4559]." Phase 3G RX Epic sub-epic D Task 2. BandSegment/BandSpot value types extracted from AetherSDR BandPlanManager. | "BandSegment + BandSpot value types ported from AetherSDR `src/models/BandPlanManager.h` [@0cd4559]. Loader class BandPlanManager ported separately in Task 3." |
| `src/models/BandPlanManager.h` | `src/models/BandPlanManager.h` | File header line 5: "Ported from AetherSDR src/models/BandPlanManager.h [@0cd4559]." Phase 3G RX Epic sub-epic D Task 3. BandPlanManager loader class verbatim port. | "Loader class ported verbatim from AetherSDR `src/models/BandPlanManager.{h,cpp}` [@0cd4559]. NereusSDR namespace; AppSettings key 'BandPlanName' kept for upstream parity; value types in BandPlan.h." |
| `src/models/BandPlanManager.cpp` | `src/models/BandPlanManager.cpp` | File cites "See BandPlanManager.h for license, attribution, and modification history." Inline cites at loadPlans (line 30), setActivePlan (line 57), loadPlanFromJson (line 82). Phase 3G RX Epic sub-epic D Task 3. | Same as BandPlanManager.h above. |

### VFO widget tree (AetherSDR floating-flag pattern)

| NereusSDR file | AetherSDR counterpart | Evidence | Specific mod-history wording |
|---|---|---|---|
| `src/gui/widgets/VfoWidget.h` | `src/gui/VfoWidget.h` | Line 20 "Floating VFO flag widget — AetherSDR pattern." Line 31 "From AetherSDR VfoWidget.h pattern." 25a: "Floating VFO flag pattern." | "Floating VFO-flag widget ported from AetherSDR `src/gui/VfoWidget.{h,cpp}`. DSP field values (frequency, mode, filter, AGC) come from Thetis `console.cs`; see Copyright block." |
| `src/gui/widgets/VfoWidget.cpp` | `src/gui/VfoWidget.cpp` | 30+ inline `// From AetherSDR VfoWidget.cpp:<line>` citations (98, 142, 154, 164, 352, 374, 387, 1479, 1625, 1671, 1799, …). Already has a proper Thetis Copyright block, so 25c only needs the AetherSDR line added. | Same as VfoWidget.h. |
| `src/gui/widgets/VfoStyles.h` | `src/gui/VfoWidget.cpp:134-177` | File header line 9: "Ported verbatim from AetherSDR src/gui/VfoWidget.cpp:134-177." Ten inline citations point at exact line numbers. | "Stylesheet constants ported verbatim from AetherSDR `src/gui/VfoWidget.cpp:134-177`." |
| `src/gui/widgets/VfoModeContainers.h` | `src/gui/VfoWidget.cpp:996-1300` | File header line 56/132 cites "mode sub-widget blocks", "Step constants from AetherSDR VfoWidget.cpp". | "Mode sub-widget containers lifted from AetherSDR `src/gui/VfoWidget.cpp:996-1300`." |
| `src/gui/widgets/VfoModeContainers.cpp` | Same | Inline: "AetherSDR VfoWidget.cpp uses the same list" (line 76), "AetherSDR VfoWidget.cpp uses 25 Hz step for Mark and 5 Hz step for Shift" (line 92). | Same as .h. |
| `src/gui/widgets/VfoLevelBar.cpp` | `src/gui/VfoWidget.cpp:38-64` (LevelBar) | Line 7 "From AetherSDR src/gui/VfoWidget.cpp:38-64 — LevelBar port". Line 47 "ported from AetherSDR LevelBar::paintEvent". | "LevelBar widget ported from AetherSDR `src/gui/VfoWidget.cpp:38-64`." |

### AetherSDR widget-library primitives

| NereusSDR file | AetherSDR counterpart | Evidence | Specific mod-history wording |
|---|---|---|---|
| `src/gui/widgets/GuardedSlider.h` | `src/gui/GuardedSlider.h:13-19, 22-47` | Inline lines 9, 21: "ControlsLock / GuardedSlider — ported from AetherSDR src/gui/GuardedSlider.h:13-19 / 22-47". | "`ControlsLock` and `GuardedSlider` ported from AetherSDR `src/gui/GuardedSlider.h:13-47`." |
| `src/gui/widgets/GuardedComboBox.h` | `src/gui/GuardedSlider.h:56` | File header line 2 "NereusSDR native widget; Qt skeleton pattern informed by AetherSDR." AetherSDR has `class GuardedComboBox` at line 56 of GuardedSlider.h. | "Qt6 pattern informed by AetherSDR `src/gui/GuardedSlider.h:56-79`." |
| `src/gui/widgets/ResetSlider.h` | `src/gui/VfoWidget.cpp:68-76` (and `RxApplet.cpp.bak:17-24`) | Inline line 8: "ResetSlider — ported from AetherSDR src/gui/VfoWidget.cpp:68-76". | "`ResetSlider` ported from AetherSDR `src/gui/VfoWidget.cpp:68-76`." |
| `src/gui/widgets/CenterMarkSlider.h` | `src/gui/VfoWidget.cpp:79-94` | Inline line 9: "CenterMarkSlider — ported from AetherSDR src/gui/VfoWidget.cpp:79-94". | "`CenterMarkSlider` ported from AetherSDR `src/gui/VfoWidget.cpp:79-94`." |
| `src/gui/widgets/TriBtn.h` | `src/gui/VfoWidget.cpp:97-129` (and `RxApplet.cpp.bak:28-40`) | Inline line 8: "Ported from AetherSDR src/gui/VfoWidget.cpp:97-129". | "`TriBtn` ported from AetherSDR `src/gui/VfoWidget.cpp:97-129`." |
| `src/gui/widgets/ScrollableLabel.h` | `src/gui/GuardedSlider.h:81-100` | Inline lines 2-4: "Qt skeleton patterns informed by AetherSDR's `ScrollableLabel`". | "Qt6 pattern informed by AetherSDR `src/gui/GuardedSlider.h:81-100`." |
| `src/gui/widgets/ScrollableLabel.cpp` | Same | Inline lines 1-2 cite the same AetherSDR file/lines. | Same. |
| `src/gui/widgets/FilterPassbandWidget.h` | `src/gui/FilterPassbandWidget.h` | File header line 3: "Ported from AetherSDR src/gui/FilterPassbandWidget.h". | "Ported from AetherSDR `src/gui/FilterPassbandWidget.h`." |
| `src/gui/widgets/FilterPassbandWidget.cpp` | `src/gui/FilterPassbandWidget.cpp` | File header line 2: "Ported from AetherSDR src/gui/FilterPassbandWidget.cpp". 14 inline `// From AetherSDR FilterPassbandWidget.cpp lines X-Y` citations. | "Ported from AetherSDR `src/gui/FilterPassbandWidget.cpp` (filter low/high drag + shift-band visualisation)." |
| `src/gui/widgets/DspParamPopup.h` | `src/gui/DspParamPopup.h` | File header port-citation: "Ported from AetherSDR src/gui/DspParamPopup.h @ 0cd4559". Byte-for-byte import except namespace (AetherSDR → NereusSDR). Floating right-click popup for DSP parameter quick-access. Phase 3G-RX Task 13. | "Floating popup widget for DSP parameter controls, ported byte-for-byte from AetherSDR `src/gui/DspParamPopup.h` [@0cd4559]. Used by NR buttons (Task 14) and other DSP controls. Namespace changed AetherSDR → NereusSDR." |
| `src/gui/widgets/DspParamPopup.cpp` | `src/gui/DspParamPopup.cpp` | File header port-citation: "Ported from AetherSDR src/gui/DspParamPopup.cpp @ 0cd4559". Byte-for-byte import except namespace (AetherSDR → NereusSDR). Depends on GuardedSlider (already ported to src/gui/widgets/GuardedSlider.h). | Same as `DspParamPopup.h`. |
| `src/gui/widgets/MeterSlider.h` | `src/gui/MeterSlider.h` | File header port-citation block: "Ported from AetherSDR source: src/gui/MeterSlider.h". Composite horizontal level-meter + gain-slider; logic is header-inline, `.cpp` is the MOC trigger only. Phase 3O Sub-Phase 9 Task 9.1; dependency of VaxApplet. | "Composite level-meter + gain-slider widget ported from AetherSDR `src/gui/MeterSlider.{h,cpp}` (pure paint + mouse; logic header-inline, `.cpp` is MOC trigger). Used by `VaxApplet`." |
| `src/gui/widgets/MeterSlider.cpp` | `src/gui/MeterSlider.cpp` | File header port-citation: "Ported from AetherSDR src/gui/MeterSlider.cpp". MOC trigger only — matches the AetherSDR shape (single `#include "MeterSlider.h"`). | Same as `MeterSlider.h`. |
| `src/gui/applets/VaxApplet.h` | `src/gui/DaxApplet.h` + `src/gui/DaxApplet.cpp` | File header port-citation block: "Ported from AetherSDR source: src/gui/DaxApplet.h / src/gui/DaxApplet.cpp". Rename DAX → VAX across class name, members, signals, AppSettings keys. Phase 3O Sub-Phase 9 Task 9.2b; docs/architecture/2026-04-19-vax-design.md §6.4. | "Ported from AetherSDR `src/gui/DaxApplet.{h,cpp}` — per-VAX-channel gain/mute applet with TX row. Renamed DAX → VAX throughout; adapted to NereusSDR's `AppletWidget` base (AetherSDR's DaxApplet inherits QWidget directly). Wires to `AudioEngine::setVaxRxGain`/`setVaxMuted`/`setVaxTxGain`." |
| `src/gui/applets/VaxApplet.cpp` | `src/gui/DaxApplet.cpp` | File header port-citation: "Ported from AetherSDR src/gui/DaxApplet.{h,cpp}". Adapted buildUI layout (4 RX channel strips + divider + TX row) to NereusSDR's AppletWidget base + StyleConstants palette. Added 50 ms QTimer level poll (Pattern A per Sub-Phase 9 handoff) reading `AudioEngine::vaxRxLevel`/`vaxTxLevel`. | Same as `VaxApplet.h`. |

### Applet layouts (style/geometry borrowed, DSP wiring native)

| NereusSDR file | AetherSDR counterpart | Evidence | Specific mod-history wording |
|---|---|---|---|
| `src/gui/applets/AppletPanelWidget.h` | `src/gui/AppletPanel.{h,cpp}` | Lines 15, 19, 31: "AetherSDR AppletPanel styling", constants from AetherSDR AppletPanel.cpp, "2.0 = AetherSDR's 280:140". | "Scrollable applet-panel pattern (260px fixed width, 16px title bars) ported from AetherSDR `src/gui/AppletPanel.{h,cpp}`." |
| `src/gui/applets/AppletPanelWidget.cpp` | Same | Inline lines 18, 172, 182, 189 reference AetherSDR AppletPanel / AppletTitleBar styling. | Same. |
| `src/gui/applets/AppletWidget.h` | `src/gui/AppletPanel.{h,cpp}` | Shared base for all NereusSDR applets; hoists AetherSDR AppletPanel title-bar gradient + slider-row + toggle-button styling up so every applet inherits identical visuals. Same styling pattern already covered by AppletPanelWidget Bucket A entry above. | "Shared applet base class; title-bar gradient, slider-row, and toggle-button helpers extracted from the AetherSDR `src/gui/AppletPanel.{h,cpp}` styling pattern." |
| `src/gui/applets/AppletWidget.cpp` | Same | Same — inherits the title-bar and toggle-button styles from Style::titleBarStyle() / buttonBaseStyle() which themselves come from AetherSDR. | Same. |
| `src/gui/applets/CatApplet.h` | `src/gui/CatApplet.{h,cpp}` | NereusSDR applet mirrors AetherSDR CatApplet layout (serial CAT / rigctl / TCI enable rows + PTT LEDs). AetherSDR counterpart exists at `~/AetherSDR/src/gui/CatApplet.{h,cpp}`. | "Layout mirrors AetherSDR `src/gui/CatApplet.{h,cpp}` (serial CAT / rigctl / TCI enable rows + PTT LEDs). All controls NYI." |
| `src/gui/applets/CatApplet.cpp` | Same | Same. | Same. |
| `src/gui/applets/CwxApplet.h` | `src/gui/CwxPanel.{h,cpp}` | NereusSDR port of AetherSDR CwxPanel (CW text entry + WPM slider + message-slot buttons). Renamed from CwxPanel → CwxApplet for NereusSDR's applet naming convention. | "Port of AetherSDR `src/gui/CwxPanel.{h,cpp}` (CW text entry + WPM + message-slot buttons); renamed to CwxApplet in NereusSDR. All controls NYI." |
| `src/gui/applets/CwxApplet.cpp` | Same | Same. | Same. |
| `src/gui/applets/DvkApplet.h` | `src/gui/DvkPanel.{h,cpp}` | NereusSDR port of AetherSDR DvkPanel (DVK F-key slot grid + record/play controls). Renamed from DvkPanel → DvkApplet for naming convention. | "Port of AetherSDR `src/gui/DvkPanel.{h,cpp}` (DVK F-key slot grid + record/play controls); renamed to DvkApplet in NereusSDR. All controls NYI." |
| `src/gui/applets/DvkApplet.cpp` | Same | Same. | Same. |
| `src/gui/applets/TunerApplet.h` | `src/gui/TunerApplet.{h,cpp}` | Layout mirrors AetherSDR TunerApplet (ATU/tune controls + SWR progress bar). | "Layout from AetherSDR `src/gui/TunerApplet.{h,cpp}` (ATU/tune controls + SWR progress bar). All controls NYI." |
| `src/gui/applets/TunerApplet.cpp` | Same | Same. | Same. |
| `src/gui/applets/RxApplet.h` | `src/gui/RxApplet.{h,cpp}` | Line 5 "Layout adapted from AetherSDR RxApplet.cpp." Line 39 "FilterPassband widget (ported from AetherSDR, Tier 1 wired)". | "Layout adapted from AetherSDR `src/gui/RxApplet.{h,cpp}` (18-control RX panel). Tier-1 SliceModel wiring follows AetherSDR GUI↔model pattern; DSP behaviour is Thetis." |
| `src/gui/applets/RxApplet.cpp` | Same | 15 inline `// From AetherSDR RxApplet.cpp lines X-Y` citations (117, 156, 183, 226, 251, 293, 304, 323, 453, 502, 561, 637, 656, 677). | Same. |
| `src/gui/applets/TxApplet.h` | `src/gui/TxApplet.{h,cpp}` | Line 13 "Layout (AetherSDR TxApplet.cpp pattern)". | "Layout pattern from AetherSDR `src/gui/TxApplet.{h,cpp}`. Wiring deferred to Phase 3M." |
| `src/gui/applets/TxApplet.cpp` | Same | 12 inline `AetherSDR TxApplet.cpp` line references (48, 61, 71, 77, 87-104, 107-128, 131-153, 155-203, 224-253). | Same. |
| `src/gui/applets/EqApplet.h` | `src/gui/EqApplet.{h,cpp}` | Line 13 "Layout mirrors AetherSDR EqApplet.cpp exactly". | "Layout mirrors AetherSDR `src/gui/EqApplet.{h,cpp}`." |
| `src/gui/applets/EqApplet.cpp` | Same | Lines 3, 23, 67, 84, 109 cite AetherSDR EqApplet.cpp. | Same. |
| `src/gui/applets/PhoneCwApplet.cpp` | `src/gui/PhoneCwApplet.{h,cpp}` (and `PhoneApplet`) | Lines 74, 109, 215, 566 cite AetherSDR PhoneCwApplet.cpp `buildPhonePanel()` / `buildCwPanel()` constants. | "Phone + CW applet layout ported from AetherSDR `src/gui/PhoneCwApplet.{h,cpp}`." |

### Spectrum overlay + audio-engine patterns

| NereusSDR file | AetherSDR counterpart | Evidence | Specific mod-history wording |
|---|---|---|---|
| `src/gui/SpectrumOverlayMenu.h` | `src/gui/SpectrumOverlayMenu.{h,cpp}` | Line 18 "From plan Step 9 / AetherSDR SpectrumOverlayMenu pattern". | "Overlay-menu pattern from AetherSDR `src/gui/SpectrumOverlayMenu.{h,cpp}`." |
| `src/gui/SpectrumOverlayPanel.h` | Same | Line 3 "Ported from AetherSDR SpectrumOverlayMenu — same visual style". | "Ported from AetherSDR `src/gui/SpectrumOverlayMenu.{h,cpp}` (left button strip + 5 flyout panels)." |
| `src/gui/SpectrumOverlayPanel.cpp` | Same | 12 inline citations ("constants from", "stylesheets verbatim", "band table from AetherSDR SpectrumOverlayMenu.cpp", flyout positioning logic). | Same. |
| `src/gui/StyleConstants.h` | `src/gui/ComboStyle.h` / `HGauge.h` / `SliceColors.h` / misc inline palette | Line 7 "Core Theme (from AetherSDR source + STYLEGUIDE.md)". | "Theme palette imported from AetherSDR `src/gui/ComboStyle.h` / `HGauge.h` / `SliceColors.h` and inline QColor calls in `MainWindow.cpp` / `VfoWidget.cpp`." |
| `src/core/AudioEngine.h` | `src/core/AudioEngine.{h,cpp}` | Line 23 "Pattern from AetherSDR AudioEngine:" (four specific bullets on feedAudio/drain/Int16 format/buffer cap). Line 73 "Buffer + timer drain pattern (from AetherSDR)". | "QAudioSink feed-and-drain pattern ported from AetherSDR `src/core/AudioEngine.{h,cpp}` (48 kHz Int16 stereo, 10 ms timer drain, 200 ms buffer cap)." |
| `src/core/AudioEngine.cpp` | Same | Lines 14, 24, 91, 274 inline "From AetherSDR AudioEngine::makeFormat()", "RX timer pattern", "on Windows, don't trust isFormatSupported()". | Same. |
| `src/core/DeepFilterFilter.h` | `src/core/DeepFilterFilter.h` | Port-citation header lines 5-6 name `src/core/DeepFilterFilter.h @0cd4559`. Modification-history block records removal of r8brain 24↔48 resampler pair, new 48 kHz stereo float in-place process() signature, namespace AetherSDR → NereusSDR. | "Ported from AetherSDR `src/core/DeepFilterFilter.h` [@0cd4559]. Modified for NereusSDR's 48 kHz stereo float audio pipeline: removed r8brain resampler pair; new process(outL, outR, sampleCount) operates in-place on separated channel arrays at 48 kHz native (DeepFilterNet3's native rate). Tuning surface (attenLimit, postFilterBeta) and df_create / df_process_frame / df_set_* calls unchanged." |
| `src/core/DeepFilterFilter.cpp` | `src/core/DeepFilterFilter.cpp` | Same port-citation header. findModelPath() updated: XDG/system search paths rebrand AetherSDR → NereusSDR. process() body rewritten: stereo L+R → mono mix at 48 kHz (no upsample), frame-accumulate, df_process_frame, drain back to outL/outR. | Same as `.h`. |
| `src/core/MacNRFilter.h` | `src/core/MacNRFilter.h` | Port-citation header (lines 5-6) names `src/core/MacNRFilter.{h,cpp} @0cd4559`. Modification-history block records rate retune: LOG2N 9→10, FFT 512→1024, hop 256→512 to preserve 46.9 Hz bin resolution at 48 kHz; new in-place process(outL, outR, sampleCount) signature; namespace AetherSDR→NereusSDR; QByteArray dependency removed. | "Ported from AetherSDR `src/core/MacNRFilter.{h,cpp}` [@0cd4559]. Retuned for NereusSDR's 48 kHz post-fexchange2 audio path: LOG2N 9→10 (FFT 512→1024, hop 256→512) to preserve 46.9 Hz/bin resolution. Algorithm (MMSE-Wiener + minimum-statistics + GSMOOTH) preserved verbatim. New process(outL, outR, sampleCount) in-place signature; QByteArray dependency removed." |
| `src/core/MacNRFilter.cpp` | `src/core/MacNRFilter.cpp` | Same port-citation header. process() body rewritten from QByteArray round-trip to in-place on outL/outR arrays. Constructor/reset/processFrame algorithm verbatim from AetherSDR. | Same as `.h`. |

### Phase 3O VAX — CoreAudio HAL bridge

Added 2026-04-19 (Sub-Phase 5 Tasks 5.1 / 5.3 / 5.4). All three files
carry a NereusSDR port-citation header (HOW-TO-PORT.md rule 6 form —
AetherSDR has no per-file GPL header to copy verbatim) naming the
upstream source file.

| NereusSDR file | AetherSDR counterpart | Evidence | Specific mod-history wording |
|---|---|---|---|
| `src/core/audio/CoreAudioHalBus.h` | `src/core/VirtualAudioBridge.h` | Port-citation header lines 5-6 name `src/core/VirtualAudioBridge.h`. Modification-history lines 18-27 record the decomposition (monolithic → per-endpoint Role), QObject drop, silence-fill/TX-poll-timer deferral, shm-path rebrand, and 24 kHz → 48 kHz rate bump. | "Ported from AetherSDR `src/core/VirtualAudioBridge.{h,cpp}` with the monolithic 4-RX + 1-TX bridge decomposed into per-endpoint IAudioBus instances (Role enum for Vax1..4 / TxInput). QObject/signals dropped in favour of atomic metering; silence-fill and TX-poll timers deferred to Phase 3M. Shm paths rebranded /aethersdr-dax-\* → /nereussdr-vax-\*; native rate lifted 24 kHz → 48 kHz per spec §8.1." |
| `src/core/audio/CoreAudioHalBus.cpp` | `src/core/VirtualAudioBridge.cpp` | Same header + TX drain loop (`pull()`) is a line-for-line port of `VirtualAudioBridge::readTxAudio` (acquire loads, writer-lapped skip, MAX/TARGET backlog guards, strided RMS meter). | Same. |
| `hal-plugin/NereusSDRVAX.cpp` | `hal-plugin/AetherSDRDAX.cpp` | Port-citation header lines 5-6 name `hal-plugin/AetherSDRDAX.cpp`. Modification-history lines 15-22 record the DAX → VAX rebrand: device UIDs `com.aethersdr.dax.*` → `com.nereussdr.vax.*`, shm paths `/aethersdr-dax-*` → `/nereussdr-vax-*`, device names `AetherSDR DAX N` → `NereusSDR VAX N`, factory UUID regenerated. 48 kHz stereo float32 format preserved. Shared-memory layout block (lines 57–) mirrored byte-for-byte against `src/core/audio/CoreAudioHalBus.h`. | "Ported from AetherSDR `hal-plugin/AetherSDRDAX.cpp` — libASPL-based Core Audio HAL Audio Server Plug-In creating 4 virtual RX outputs + 1 virtual TX input. DAX → VAX rebrand across device UIDs, shm paths, device names, and factory UUID; wire format (48 kHz stereo float32) and ring-buffer layout preserved." |
| `src/core/audio/LinuxPipeBus.h` | `src/core/PipeWireAudioBridge.h` | Port-citation header lines 5-6 name `src/core/PipeWireAudioBridge.{h,cpp}`. Modification-history lines 18-27 record the decomposition (monolithic → per-endpoint Role), QObject/signals drop, silence-fill + TX poll timer + per-channel/TX gain deferral to Phase 3M, pipe-path rebrand `/tmp/aethersdr-dax-*` → `/tmp/nereussdr-vax-*`, and 24 kHz mono int16 → 48 kHz stereo float32 format lift. | "Ported from AetherSDR `src/core/PipeWireAudioBridge.{h,cpp}` with the monolithic 4-RX + 1-TX bridge decomposed into per-endpoint IAudioBus instances (Role enum for Vax1..4 / TxInput). QObject/signals dropped in favour of atomic metering; silence-fill, TX poll timer, and per-channel/TX gain deferred to Phase 3M. Pipe paths rebranded /tmp/aethersdr-dax-\* → /tmp/nereussdr-vax-\*; format lifted 24 kHz mono int16 → 48 kHz stereo float32 per spec §8.1. std::once_flag stale-cleanup replaces per-open scan." |
| `src/core/audio/LinuxPipeBus.cpp` | `src/core/PipeWireAudioBridge.cpp` | Same header + pactl lifecycle (mkfifo, load-module, open fd, unload-module, unlink) adapted from `PipeWireAudioBridge::loadPipeSource()` / `loadPipeSink()` / `unloadModules()`. Stale-module scan ported from `cleanupStaleModules()` with prefix rebrand. push()/pull() RMS metering ported from feedDaxAudio()/readTxPipe() meter blocks. | Same. |

> **Classification note (Task 5.4):** `hal-plugin/NereusSDRVAX.cpp` is
> registered here for provenance completeness, but
> `scripts/compliance-inventory.py::_aethersdr_bucket_a_paths` currently
> filters Bucket-A paths to those starting with `src/` or `tests/` (see
> script line 87). The hal-plugin path therefore still classifies as
> `nereussdr-original` in the inventory output. Widening that prefix
> filter is a one-line script change deferred out of Task 5.4's "CI +
> docs only" scope. The file carries the full AetherSDR-port header
> block regardless, so the `--fail-on-unclassified` gate passes
> cleanly either way.

### Setup shared-style

| NereusSDR file | AetherSDR counterpart | Evidence | Specific mod-history wording |
|---|---|---|---|
| `src/gui/SetupPage.cpp` | `src/gui/RadioSetupDialog.{h,cpp}` | Line 8 "Shared style strings — mirror AetherSDR RadioSetupDialog constants". | "Shared setup-page style constants mirror AetherSDR `src/gui/RadioSetupDialog.{h,cpp}`." |

### Phase 3O Sub-Phase 10 Task 10c — TitleBar host strip

Added 2026-04-20 (Sub-Phase 10 Task 10c). Scoped-down port of AetherSDR's
monolithic TitleBar — keeps the 32 px host-strip + `setMenuBar()` re-
parent pattern + app-name label, drops everything else. Files carry a
NereusSDR port-citation header (HOW-TO-PORT.md rule 6 — AetherSDR has no
per-file GPL header to copy) naming AetherSDR `src/gui/TitleBar.{h,cpp}`.

| NereusSDR file | AetherSDR counterpart | Evidence | Specific mod-history wording |
|---|---|---|---|
| `src/gui/TitleBar.h` | `src/gui/TitleBar.h` + `TitleBar.cpp:27-34, 94-104, 282-295` | Port-citation header names both AetherSDR files. Structural pattern (fixed 32 px host strip, `[menu][stretch][app-name][stretch][right-cluster]` hbox, `setMenuBar()` re-parent at position 0) comes directly from AetherSDR. Scope intentionally reduced: only the master-output right cluster is included; AetherSDR's heartbeat / multiFLEX / PC-audio / headphone / minimal-mode / feature-request widgets are deferred to separate NereusSDR phases. | "Scoped-down port of AetherSDR `src/gui/TitleBar.{h,cpp}` — master-output strip only; heartbeat / multiFLEX / PC-audio / headphone / minimal-mode / feature-request widgets intentionally omitted (deferred to separate phases — 3G-14 plans the 💡 feature-request widget; headphone devices land in Sub-Phase 12). Hosts `MasterOutputWidget` (Task 10b) on the right; `setMenuBar()` copied from AetherSDR `TitleBar.cpp:282-295`." |
| `src/gui/TitleBar.cpp` | Same | Same — constructor background (#0a0a14) + bottom border (#203040) + `setFixedHeight(32)` from AetherSDR `TitleBar.cpp:30-31`; app-name label from `TitleBar.cpp:101-104` with "AetherSDR" → "NereusSDR". `setMenuBar()` is a line-for-line port of `TitleBar.cpp:282-295`. | Same as `.h`. |

---

## Bucket B — False AetherSDR citations (126 files)

Every file below carries the mod-history boilerplate
> *"Structural template follows AetherSDR (ten9876/AetherSDR) Qt6 conventions."*

…but has **no AetherSDR counterpart** that a compliance reviewer could
open. Each file's Copyright block already cites the correct Thetis
source(s) (MeterManager.cs, frmMeterDisplay.cs, setup.cs, console.cs,
display.cs, ucRadioList.cs, etc.), so the file stands on its own merit
after removal.

**25c action for every file below:** delete the two lines starting with
*"Claude Code. Structural template follows AetherSDR"* and
*"(ten9876/AetherSDR) Qt6 conventions."* from the Modification-History
block. Leave the Thetis Copyright block untouched. If the third
Modification-History sentence is the AetherSDR one only, the resulting
entry should end with "…via Anthropic Claude Code."

Cross-reference with 25a §"NereusSDR-original files with NO AetherSDR
counterpart" and Flags #3–#6 (WDSP, meters, containers, MMIO are
Thetis, not AetherSDR).

### B.1 — Meter subsystem (Thetis MeterManager/ucMeter, AetherSDR has no per-item tree)

`src/gui/meters/MeterWidget.h`, `src/gui/meters/MeterWidget.cpp`,
`src/gui/meters/MeterPoller.h`, `src/gui/meters/MeterPoller.cpp`,
`src/gui/meters/ItemGroup.h`, `src/gui/meters/ItemGroup.cpp`,
`src/gui/meters/SpacerItem.h`, `src/gui/meters/SpacerItem.cpp`,
`src/gui/meters/FadeCoverItem.h`, `src/gui/meters/FadeCoverItem.cpp`,
`src/gui/meters/LEDItem.h`, `src/gui/meters/LEDItem.cpp`,
`src/gui/meters/HistoryGraphItem.h`, `src/gui/meters/HistoryGraphItem.cpp`,
`src/gui/meters/MagicEyeItem.h`, `src/gui/meters/MagicEyeItem.cpp`,
`src/gui/meters/NeedleScalePwrItem.h`, `src/gui/meters/NeedleScalePwrItem.cpp`,
`src/gui/meters/FilterDisplayItem.h`, `src/gui/meters/FilterDisplayItem.cpp`,
`src/gui/meters/TextOverlayItem.h`, `src/gui/meters/TextOverlayItem.cpp`,
`src/gui/meters/RotatorItem.h`, `src/gui/meters/RotatorItem.cpp`,
`src/gui/meters/ButtonBoxItem.h`, `src/gui/meters/ButtonBoxItem.cpp`,
`src/gui/meters/BandButtonItem.h`, `src/gui/meters/BandButtonItem.cpp`,
`src/gui/meters/ModeButtonItem.h`, `src/gui/meters/ModeButtonItem.cpp`,
`src/gui/meters/FilterButtonItem.h`, `src/gui/meters/FilterButtonItem.cpp`,
`src/gui/meters/AntennaButtonItem.h`, `src/gui/meters/AntennaButtonItem.cpp`,
`src/gui/meters/TuneStepButtonItem.h`, `src/gui/meters/TuneStepButtonItem.cpp`,
`src/gui/meters/OtherButtonItem.h`, `src/gui/meters/OtherButtonItem.cpp`,
`src/gui/meters/VfoDisplayItem.h`, `src/gui/meters/VfoDisplayItem.cpp`,
`src/gui/meters/ClockItem.h`,
`src/gui/meters/ClickBoxItem.h`,
`src/gui/meters/DataOutItem.h`,
`src/gui/meters/DialItem.h`, `src/gui/meters/DialItem.cpp`,
`src/gui/meters/DiscordButtonItem.h`,
`src/gui/meters/VoiceRecordPlayItem.h`,
`src/gui/meters/WebImageItem.h`, `src/gui/meters/WebImageItem.cpp`

Notes:
- `MeterItem.h` / `MeterItem.cpp` are **NOT** in this list — those files
  never had the AetherSDR mod-history line; their AetherSDR mentions
  are inline, tied to the NeedleItem (S-meter) port from AetherSDR
  SMeterWidget. See Bucket C.
- Per 25a Flag #4: AetherSDR's `MeterApplet` is a single applet, not a
  tree. None of the per-item classes listed above have an AetherSDR
  counterpart.

### B.2 — Container subsystem (Thetis ucMeter/frmMeterDisplay — AetherSDR supplies only the float-shell pattern)

These files' Copyright blocks cite `frmMeterDisplay.cs` /
`MeterManager.cs`. Per 25a Flag #5: AetherSDR's `FloatingAppletWindow`
is the *structural* starting point but NereusSDR's container system has
diverged significantly (dock modes, axis-lock, MMIO hooks). The
boilerplate AetherSDR line overclaims; the real debt is to Thetis. They
DO have a weak AetherSDR structural pattern debt, but the standing
Bucket B fix is correct: remove the boilerplate and rely on the
AetherSDR inline note that ContainerWidget already is conceptually
AetherSDR-inspired. (If future 3G-14 work adds more AetherSDR
FloatingAppletWindow code, add Bucket A attribution then.)

`src/gui/containers/ContainerWidget.h`, `src/gui/containers/ContainerWidget.cpp`,
`src/gui/containers/ContainerManager.h`, `src/gui/containers/ContainerManager.cpp`,
`src/gui/containers/FloatingContainer.h`, `src/gui/containers/FloatingContainer.cpp`,
`src/gui/containers/ContainerSettingsDialog.h`, `src/gui/containers/ContainerSettingsDialog.cpp`,
`src/gui/containers/MmioVariablePickerPopup.h`,
`src/gui/containers/meter_property_editors/ScaleItemEditor.h`,
`src/gui/containers/meter_property_editors/ScaleItemEditor.cpp`,
`src/gui/containers/meter_property_editors/NeedleItemEditor.h`,
`src/gui/containers/meter_property_editors/NeedleScalePwrItemEditor.h`

### B.3 — MMIO subsystem (Thetis ONLY, per 25a Flag #6)

`src/core/mmio/MmioEndpoint.h`,
`src/core/mmio/FormatParser.h`, `src/core/mmio/FormatParser.cpp`,
`src/core/mmio/ExternalVariableEngine.h`,
`src/core/mmio/UdpEndpointWorker.h`, `src/core/mmio/UdpEndpointWorker.cpp`,
`src/core/mmio/TcpListenerEndpointWorker.h`, `src/core/mmio/TcpListenerEndpointWorker.cpp`,
`src/core/mmio/TcpClientEndpointWorker.h`, `src/core/mmio/TcpClientEndpointWorker.cpp`,
`src/core/mmio/SerialEndpointWorker.h`, `src/core/mmio/SerialEndpointWorker.cpp`

### B.4 — WDSP / hardware / receiver / FFT / DSP controllers (per 25a Flag #3, these have NO AetherSDR ancestry)

`src/core/ReceiverManager.h`, `src/core/ReceiverManager.cpp`,
`src/core/HardwareProfile.h`, `src/core/HardwareProfile.cpp`,
`src/core/StepAttenuatorController.h`, `src/core/StepAttenuatorController.cpp`,
`src/core/NoiseFloorEstimator.h`,
`src/core/ClarityController.h`,
`src/core/FFTEngine.h`, `src/core/FFTEngine.cpp`

### B.5 — Models that are Thetis-ported, not AetherSDR-ported

`src/models/Band.h` — Thetis console.cs 14-band enum; AetherSDR has
`BandDefs.h` but the NereusSDR enum is explicitly Thetis-shaped with
IARU Region 2 lookup and WWV discrete centers.

`src/models/RxDspWorker.h` — NereusSDR-original worker thread; no
AetherSDR counterpart.

### B.6 — Setup pages (Thetis Setup.cs; AetherSDR RadioSetupDialog is SmartSDR-license-only)

`src/gui/setup/HardwarePage.h`, `src/gui/setup/HardwarePage.cpp`,
`src/gui/setup/DisplaySetupPages.h`, `src/gui/setup/DisplaySetupPages.cpp`,
`src/gui/setup/TransmitSetupPages.h`, `src/gui/setup/TransmitSetupPages.cpp`,
`src/gui/setup/DspSetupPages.cpp`,
`src/gui/setup/GeneralOptionsPage.h`, `src/gui/setup/GeneralOptionsPage.cpp`,
`src/gui/setup/hardware/RadioInfoTab.h`, `src/gui/setup/hardware/RadioInfoTab.cpp`,
`src/gui/setup/hardware/PureSignalTab.h`, `src/gui/setup/hardware/PureSignalTab.cpp`,
`src/gui/setup/hardware/PaCalibrationTab.h`, `src/gui/setup/hardware/PaCalibrationTab.cpp`,
`src/gui/setup/hardware/OcOutputsTab.h`, `src/gui/setup/hardware/OcOutputsTab.cpp`,
`src/gui/setup/hardware/DiversityTab.h`, `src/gui/setup/hardware/DiversityTab.cpp`,
`src/gui/setup/hardware/AntennaAlexTab.h`, `src/gui/setup/hardware/AntennaAlexTab.cpp`,
`src/gui/setup/hardware/Hl2IoBoardTab.h`, `src/gui/setup/hardware/Hl2IoBoardTab.cpp`

### B.7 — Dialogs with no AetherSDR counterpart

`src/gui/AddCustomRadioDialog.h`, `src/gui/AddCustomRadioDialog.cpp` —
Thetis `frmAddCustomRadio` port (per 25a explicitly listed).

### B.8 — Tests with boilerplate AetherSDR line (15 files; no AetherSDR test counterpart exists; tests are NereusSDR-native)

`tests/tst_radio_discovery_parse.cpp`,
`tests/tst_step_attenuator_controller.cpp`,
`tests/tst_slice_squelch.cpp`,
`tests/tst_slice_rit_xit.cpp`,
`tests/tst_slice_emnr.cpp`,
`tests/tst_slice_apf.cpp`,
`tests/tst_slice_agc_advanced.cpp`,
`tests/tst_rxchannel_squelch.cpp`,
`tests/tst_rxchannel_emnr.cpp`,
`tests/tst_rxchannel_apf.cpp`,
`tests/tst_reading_name.cpp`,
`tests/tst_meter_item_scale.cpp`,
`tests/tst_meter_item_bar.cpp`,
`tests/tst_meter_presets.cpp`,
`tests/tst_fm_opt_container_wire.cpp`

(Subtotal for Bucket B: src 111 + tests 15 = **126**.)

---

## Bucket C — Mixed lineage (12 files)

The file's behaviour is Thetis but its Qt6 skeleton / GPU pipeline /
widget shell is demonstrably from AetherSDR. 25c keeps **both**
citations but tightens the third Modification-History sentence so it
specifies what came from each source.

### C.1 — Models + discovery with both lineages (4 files)

| NereusSDR file | Thetis piece | AetherSDR piece | Suggested mod-history wording |
|---|---|---|---|
| `src/models/PanadapterModel.h` | per-band grid (`BandGridSettings`, Phase 3G-8) is Thetis console.cs / display.cs | display-state template is AetherSDR `src/models/PanadapterModel.{h,cpp}` | "Per-panadapter display-state template from AetherSDR `src/models/PanadapterModel.{h,cpp}`; per-band grid storage added in Phase 3G-8 ports Thetis console.cs / display.cs." |
| `src/models/PanadapterModel.cpp` | Same | Same | Same. |
| `src/core/RadioDiscovery.h` | mi0bot/Thetis-HL2 `clsRadioDiscovery.cs` discovery parsing | AetherSDR UDP listener shell (bind retry, stale timer, re-bind) — per 25a note on RadioDiscovery | "Discovery parsing ported from mi0bot/Thetis-HL2 `HPSDR/clsRadioDiscovery.cs`; UDP listener shell (rebind-on-error, stale-entry timer) follows AetherSDR `src/core/RadioDiscovery.{h,cpp}`." |
| `src/core/RadioDiscovery.cpp` | Same | Same | Same. |

Note: `RadioModel.h` is classified **Bucket A**, not C. Although
RadioModel has mixed lineage (Thetis `console.cs` hub logic + AetherSDR
hub pattern), the Copyright block is already a proper Thetis block
naming the correct contributors — so the only 25c work on RadioModel.h
is to replace the boilerplate AetherSDR line with a specific AetherSDR
attribution (Bucket A action), which happens to also be what Bucket C
prescribes. One bucket, one action.

### C.2 — GUI files with Thetis Copyright block + heavy inline AetherSDR porting (8 files)

| NereusSDR file | Thetis piece | AetherSDR piece | Suggested mod-history wording |
|---|---|---|---|
| `src/gui/SpectrumWidget.h` | enums.cs / setup.cs / display.cs (band logic, FFT sizes, color enums) | `src/gui/SpectrumWidget.{h,cpp}` (QRhi pipeline, tile layout, overlay caching, SMOOTH_ALPHA, kMaxFftBins, kFftVertStride) | Add AetherSDR attribution line + retain existing Thetis block. Mod-history: "Combines Thetis display/enums/setup logic with AetherSDR `src/gui/SpectrumWidget.{h,cpp}` QRhi pipeline architecture and drag/hit-test model." |
| `src/gui/SpectrumWidget.cpp` | Same | 30+ inline `// From AetherSDR SpectrumWidget.cpp:<line>` citations (bin indexing, VFO triangle, slice colors, waterfall gradient, pan model §1815). | Same. |
| `src/gui/MainWindow.h` | `console.cs` command dispatch, menu bar, wisdom-generation flow | AetherSDR `MainWindow.{h,cpp}` signal-routing hub + double-height status bar + TitleBar feature-request dialog | Add AetherSDR attribution line + "Signal-routing hub, status bar layout, and feature-request dialog ported from AetherSDR `src/gui/MainWindow.{h,cpp}` and `src/gui/TitleBar.{h,cpp}`." (Note: MainWindow.h/.cpp do NOT currently carry formal port headers; see Bucket D #4.) |
| `src/gui/MainWindow.cpp` | Same | Same + 8 inline citations (MainWindow:160, 243, 603, 1342, 1420, 1790, 2048, 2475) | Same. |
| `src/gui/ConnectionPanel.h` | Thetis `ucRadioList.cs` | AetherSDR `src/gui/ConnectionPanel.{h,cpp}` | "Connection-panel layout from AetherSDR `src/gui/ConnectionPanel.{h,cpp}`; discovery/connection logic ported from Thetis `ucRadioList.cs`." |
| `src/gui/ConnectionPanel.cpp` | Same | Same | Same. |
| `src/gui/meters/MeterItem.h` | `MeterManager.cs` + `console.cs` (all item types except NeedleItem) | AetherSDR `SMeterWidget.{h,cpp}` for the `NeedleItem` class (40+ inline citations on arc geometry, SMOOTH_ALPHA, peak-hold preset, dbmToFraction, sUnitsText, tick drawing, needle geometry) | MeterItem.h has NO "Structural template follows AetherSDR" line — the header is already correctly Thetis. Add AetherSDR attribution line + amend mod-history: "`NeedleItem` S-meter is a direct port of AetherSDR `src/gui/SMeterWidget.{h,cpp}` (see inline citations lines 673-793 / 1178-1712)." |
| `src/gui/meters/MeterItem.cpp` | Same | Same | Same. |

(Subtotal for Bucket C: models/discovery 4 + GUI 8 = **12**.)

---

The 5 files below cite AetherSDR only in incidental inline comments
(single-line notes, phase-naming comments, contributor-list literals).
None of them carry a formal "Structural template follows AetherSDR"
line in their Modification-History block, and none are a definite
AetherSDR port. 25c should NOT auto-edit them; J.J. should glance and
decide case-by-case whether to leave them untouched.

| File | Inline AetherSDR mention | Default recommendation |
|---|---|---|
| `src/core/WdspEngine.cpp` | Line 59: `// From AetherSDR AudioEngine::needsWisdomGeneration() pattern.` — AetherSDR has no WDSP; note documents a UI-pattern borrow for FFTW wisdom generation only. | Leave as-is. The inline comment correctly limits the scope of the AetherSDR debt to a single method pattern. The file's Copyright block is Thetis-only (WDSP is TAPR/Thetis); no formal AetherSDR attribution needed. |
| `src/gui/AboutDialog.cpp` | ~~Lines 106-107: contributor-table data~~ | **Resolved 2026-04-17 (Compliance Plan T11):** see Bucket D.1 below for the full resolution note. AboutDialog.{h,cpp} now carry NereusSDR port-citation headers naming AetherSDR `MainWindow.cpp` about-box section + `TitleBar.{h,cpp}` at project level. |
| `tests/tst_about_dialog.cpp` | Line 41: `QStringLiteral("ten9876/AetherSDR")` — test literal that round-trips the About-dialog contributor string. | Leave as-is. String is asserted correctness of data, not a file-origin claim. |
| `tests/tst_container_persistence.cpp` | Line 219: `// NeedleItem::setValue historically clamped to the AetherSDR…` — inline test commentary explaining clamp-range history. | Leave as-is. Single-line note inside a test body; no header claim. |
| `tests/CMakeLists.txt` | Line 83: `# ── Phase 3G-10 VFO DSP parity + AetherSDR flag port ─────────` — internal phase-naming separator. | Leave as-is. Section banner, not a copyright claim. |

Note: two files that originally looked like candidates for Bucket D —
`src/gui/MainWindow.h` and `src/gui/MainWindow.cpp` — are instead
classified as **Bucket C** (see C.2) because the inline citations are
load-bearing enough (double-height status bar, signal routing hub,
feature-request dialog port) to merit formal attribution. The
MainWindow files do *not* currently carry any port header, so 25c
should skip them too; a follow-up task should add dual Thetis +
AetherSDR port headers outside the narrow 25b → 25c remediation loop.

---

## Appendix — master manifest

Produced by `grep -rl "AetherSDR" src/ tests/` executed at
HEAD of `compliance/v0.2.0-remediation` on 2026-04-16.

Total: 176 files (158 src + 18 tests). See the per-bucket lists above
for exact membership. Anyone sanity-checking 25b's classification
should replay that grep and match each hit to one of the four buckets.

---

## Judgement calls 25c should sanity-check

1. **VfoWidget.cpp** Bucket A over Bucket C: the file has a Thetis
   Copyright block AND 30+ inline AetherSDR references. I classed it
   as A because the dominant code debt is AetherSDR (VfoWidget is an
   AetherSDR invention — Thetis has no floating-flag widget). If you
   disagree, reclass as C — the only wording change is "Structural
   template follows AetherSDR" → "AetherSDR for the flag shell, Thetis
   for per-field radio behaviour".
2. **Container subsystem in Bucket B (not C):** I removed the AetherSDR
   boilerplate rather than keep-and-tighten because 25a Flag #5 says
   the container subsystem has diverged substantially and the Thetis
   ucMeter/frmMeterDisplay attribution is the load-bearing one.
   Defensible the other way; if you want to preserve the
   structural-pattern credit, reclass the six container files
   (ContainerWidget, ContainerManager, FloatingContainer,
   ContainerSettingsDialog, MmioVariablePickerPopup, and the two
   property-editor files) from B to C.
3. **Setup pages in Bucket B:** AetherSDR's `RadioSetupDialog` does
   have a tabbed setup-dialog shape that NereusSDR's
   HardwarePage/DspSetupPages broadly follow. I classed as B because
   25a explicitly says "Pattern only; content is Thetis-feature-driven."
   and the per-field setup code is 100% Thetis Setup.cs. If compliance
   reviewers want a fig-leaf pattern credit, reclass to C.
4. **MainWindow.h/.cpp in Bucket C:** listed as C because the inline
   AetherSDR debt is load-bearing (signal-routing hub, double-height
   status bar, feature-request dialog port). These files do not
   currently carry a formal Modification-History block, so 25c cannot
   mechanically "tighten" it — 25c should flag the pair as out-of-scope
   for the narrow reconciliation pass and defer to a follow-up task
   that adds full dual Thetis + AetherSDR port headers. If you'd
   rather treat them as pure D (incidental), reclass both.
5. **RadioModel.h bucket:** classified as A (boilerplate replaced with
   specific wording). Mixed-lineage in reality (Thetis `console.cs` +
   AetherSDR `RadioModel.{h,cpp}`), but the file's Copyright block is
   already Thetis-correct, so the 25c action is a Bucket-A-style
   replace + add-attribution.
6. **Tests in Bucket B:** 15 test files carry the boilerplate but
   there is no AetherSDR test counterpart at all — tests are entirely
   NereusSDR-native. Defensible to leave the boilerplate (argue it
   documents the DSP-feature-under-test's origin in Thetis with
   AetherSDR-style Qt6 test scaffolding), but I classed B because
   *test* files should not be carrying a copyright-style attribution
   line for an unrelated runtime dependency. 25c should remove.

End of Task 25b.

---

## Bucket D — Deferred files (Task 25c deferral note)

Added by Task 25c. These 7 files were NOT auto-edited. Each is flagged
for human review.

### D.1 — Original Bucket D: incidental inline AetherSDR mentions (5 files)

These 5 files were classified Bucket D by Task 25b because they cite
AetherSDR only in incidental inline comments — not in a formal
Modification-History attribution claim.  25c did NOT touch them.

| File | AetherSDR mention | Reason deferred |
|---|---|---|
| `src/core/WdspEngine.cpp` | Line 59: `// From AetherSDR AudioEngine::needsWisdomGeneration() pattern.` | Single method UI-pattern note. File Copyright block is Thetis-only (WDSP is TAPR/Thetis); no formal AetherSDR attribution needed. Leave as-is unless J.J. determines otherwise. |
| `src/gui/AboutDialog.cpp` | ~~Lines 106-107: contributor-table data~~ | **Resolved 2026-04-17 (Compliance Plan T11):** Both `AboutDialog.{h,cpp}` were bare on origin (single-line `// path` comment only). Added a NereusSDR port-citation header to each citing AetherSDR `src/gui/MainWindow.cpp` (about-box section) + `src/gui/TitleBar.{h,cpp}` at project level per HOW-TO-PORT.md rule 6 (AetherSDR has no per-file headers to copy verbatim). The contributor-table data at AboutDialog.cpp:106-107 was already correct as data; the new file-level header captures the design-origin claim that 25a Flag #11 identified. No Thetis derivation, so no PROVENANCE row owed. |
| `tests/tst_about_dialog.cpp` | Line 41: `QStringLiteral("ten9876/AetherSDR")` | Test literal asserting correctness of the About-dialog data. Not a file-origin claim. Leave as-is. |
| `tests/tst_container_persistence.cpp` | Line 219: `// NeedleItem::setValue historically clamped to the AetherSDR…` | Single-line test-body comment explaining clamp-range history. Not a header claim. Leave as-is. |
| `tests/CMakeLists.txt` | Line 83: `# ── Phase 3G-10 VFO DSP parity + AetherSDR flag port ─────────` | Section banner / phase-naming separator. Not a copyright claim. Leave as-is. |

### D.2 — Reclassified from Bucket C: MainWindow files (2 files)

Task 25b classified `src/gui/MainWindow.h` and `src/gui/MainWindow.cpp`
as **Bucket C** (mixed Thetis + AetherSDR lineage) because the
AetherSDR debt is load-bearing (signal-routing hub, double-height status
bar, feature-request dialog port from AetherSDR `src/gui/MainWindow.{h,cpp}`
and `src/gui/TitleBar.{h,cpp}`).

Task 25c moved them to Bucket D because **neither file currently carries
a formal Modification-History block** — they open with `#pragma once`
and Qt/std includes, with no Copyright or mod-history comment.  The
mechanical Bucket C action ("tighten the Modification-History AetherSDR
line") cannot be applied to a file with no such block.

Recommended follow-up task:
1. Add a full dual-attribution header to both files (Thetis `console.cs`
   command dispatch + AetherSDR `MainWindow.{h,cpp}` signal-routing hub).
2. Register both in `THETIS-PROVENANCE.md` under a new "gui/MainWindow"
   subsection.
3. Apply the Bucket C copyright-block form at that time.

Until then, the files are compliant with the verifier (they are not
listed in `THETIS-PROVENANCE.md`) but carry no formal attribution.

End of Task 25c deferral note.

---

**Resolution (2026-04-17, GPL Compliance Plan Task 1):**

All three follow-up actions are complete:

1. `src/gui/MainWindow.cpp` already carried the multi-source Thetis
   verbatim header (MeterManager.cs / dsp.cs / console.cs / setup.cs /
   radio.cs); the Modification-History block was extended with an
   AetherSDR project-level citation naming the signal-routing hub,
   double-height status-bar layout, and TitleBar feature-request dialog
   ports.
2. `src/gui/MainWindow.h` received a full new header: NereusSDR
   port-citation block + Thetis `console.cs` verbatim block (byte-identical
   to the `.cpp` block, trailing whitespace preserved) + AetherSDR
   project-level citation in the Modification-History block.
3. Both files are now registered in `docs/attribution/THETIS-PROVENANCE.md`.

The AetherSDR citation form follows `docs/attribution/HOW-TO-PORT.md`
rule 6 (project-level reference, since AetherSDR has no per-file headers
to copy verbatim). Bucket D.2 closed.
