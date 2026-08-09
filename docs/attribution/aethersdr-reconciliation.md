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
| `src/models/NotchModel.h` | `src/models/TnfModel.{h,cpp}` | Header attribution block names TnfModel [@c6481cbf]. `Notch::id` is the AetherSDR `TnfEntry::id` addition (`TnfModel.h:9`). TNF (tunable notch filter) Task 3. | "Stable-id notch-store shape ported from AetherSDR `src/models/TnfModel.{h,cpp}` [@c6481cbf]. `TnfEntry::depthDb` and `::permanent` dropped (SmartSDR capabilities with no WDSP equivalent, design section 1.2); `m_globalEnabled` defaults false rather than AetherSDR's true (`TnfModel.h:52`) because ours is the source of truth, not a mirror of radio state; notch geometry, guards and spatial helpers are Thetis, see Copyright block." |
| `src/models/NotchModel.cpp` | `src/models/TnfModel.cpp` | Same as NotchModel.h. | Same as NotchModel.h above. |
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

### Phase 3J-1 — TCI server skeleton

| NereusSDR file | AetherSDR counterpart | Evidence | Specific mod-history wording |
|---|---|---|---|
| `src/core/TciServer.h` | `src/core/TciServer.{h,cpp}` | `no-port-check:` escape + inline cites: `// From AetherSDR src/core/TciServer.cpp:247-273 [@0cd4559]` (onNewConnection slot), `// From AetherSDR src/core/TciServer.cpp:275+ [@0cd4559]` (onClientDisconnected slot). NereusSDR diverges in bind address (LocalHost vs Any), double-start contract (false vs idempotent), signal set (clientConnected/clientDisconnected carrying QWebSocket* vs clientCountChanged(int)), and client table type (QHash vs QList). | "Transport lifecycle (start/stop/onNewConnection/onClientDisconnected) adapted from AetherSDR `src/core/TciServer.{h,cpp}` [@0cd4559]. Bind address LocalHost (AetherSDR: Any) per design doc Q7." |
| `src/core/TciServer.cpp` | `src/core/TciServer.{h,cpp}` | `no-port-check:` escape + inline cites: start() listen block from `TciServer.cpp:159-181 [@0cd4559]`; stop() cleanup loop from `TciServer.cpp:184-207 [@0cd4559]`; isRunning/port from `TciServer.cpp:209-217 [@0cd4559]`; onNewConnection from `TciServer.cpp:247-273 [@0cd4559]`; onClientDisconnected from `TciServer.cpp:275+ [@0cd4559]`. | Same as `.h`. |

### Phase 3J-2 Task B1 — Spot-ingest clients (DXLab SpotCollector)

Added 2026-05-10. Three files port the DXLab SpotCollector UDP listener
from AetherSDR. `DxSpot` is extracted to its own header so all six
spot-ingest clients (Cluster, RBN, SpotCollector, WSJT-X, POTA, FreeDV,
PSK) can share the value type without pulling in DX-cluster code. Files
carry a NereusSDR port-citation header (HOW-TO-PORT.md rule 6 form -
AetherSDR has no per-file GPL header to copy verbatim) naming the
upstream source.

| NereusSDR file | AetherSDR counterpart | Evidence | Specific mod-history wording |
|---|---|---|---|
| `src/core/DxSpot.h` | `src/core/DxClusterClient.h:13-23` | Port-citation header names AetherSDR `src/core/DxClusterClient.h:13-23`. Phase 3J-2 Task B1. Standalone value type extracted from AetherSDR's `DxClusterClient.h` so future spot-ingest clients (RBN, WSJT-X, POTA, FreeDV, PSK) can share the type without pulling in DX-cluster code. Source-label list expanded from "Cluster, RBN, WSJT-X" to also include "SpotCollector, POTA, FreeDV, PSK". | "DxSpot value type extracted byte-for-byte from AetherSDR `src/core/DxClusterClient.h:13-23` [@0cd4559] so multiple spot-ingest clients can share the type without pulling in DX-cluster code. Source-label list expanded to include the additional NereusSDR ingest sources (SpotCollector, POTA, FreeDV, PSK)." |
| `src/core/SpotCollectorClient.h` | `src/core/SpotCollectorClient.h` | Port-citation header names AetherSDR `src/core/SpotCollectorClient.h`. Phase 3J-2 Task B1. Class skeleton ported byte-for-byte; namespace AetherSDR -> NereusSDR; added public `parseDxSpotLineForTest()` seam for the parser unit test. `DxSpot` include moved to the extracted `DxSpot.h`. | "DXLab SpotCollector UDP listener ported from AetherSDR `src/core/SpotCollectorClient.{h,cpp}` [@0cd4559]. Namespace AetherSDR -> NereusSDR; added public `parseDxSpotLineForTest()` seam for the parser unit test; `DxSpot` include moved to extracted `DxSpot.h`." |
| `src/core/SpotCollectorClient.cpp` | `src/core/SpotCollectorClient.cpp` | Port-citation header names AetherSDR `src/core/SpotCollectorClient.cpp`. Phase 3J-2 Task B1. Inline cites at the constructor (`:13-19`), destructor (`:21-25`), `logFilePath` (`:27-31`), `startListening` (`:33-61`), `stopListening` (`:63-69`), `onReadyRead` (`:73-100`), and `parseDxSpotLine` (`:104-126`). NereusSDR divergences: `qCDebug(lcDxCluster)` -> `qCDebug(lcSpots)` (NereusSDR's `nereus.spots` category); log file path uses `AppConfigLocation` (already lands under `NereusSDR/`) instead of AetherSDR's `GenericConfigLocation + "AetherSDR/spotcollector.log"`; source-label assignment moved from `onReadyRead()` into `parseDxSpotLine()` so unit tests see a fully-populated `DxSpot`; added "RBN" promotion when the spotter callsign carries a `-#` suffix or `RBN-` prefix (Reverse Beacon Network spots pushed via SpotCollector). | "Same as `SpotCollectorClient.h` above. Source-label assignment moved into the parser so unit tests see a fully-populated `DxSpot`; default label `SpotCollector`, promoted to `RBN` when the spotter callsign carries a `-#` suffix or `RBN-` prefix. Logging routed through NereusSDR's `lcSpots` category; log file path under `AppConfigLocation` instead of AetherSDR's `GenericConfigLocation`." |

### Phase 3J-2 Task B2 - POTA HTTPS spot poller

Added 2026-05-10. Two files port the POTA (Parks On The Air) HTTPS spot
poller from AetherSDR. Polls `https://api.pota.app/spot/activator` on a
configurable interval (default 30 sec) and emits one DxSpot per new
activation. Dedup is by integer `spotId` set membership across
consecutive polls. Files carry a NereusSDR port-citation header
(HOW-TO-PORT.md rule 6 form - AetherSDR has no per-file GPL header to
copy verbatim) naming the upstream source.

| NereusSDR file | AetherSDR counterpart | Evidence | Specific mod-history wording |
|---|---|---|---|
| `src/core/PotaClient.h` | `src/core/PotaClient.h` | Port-citation header names AetherSDR `src/core/PotaClient.h`. Phase 3J-2 Task B2. Class skeleton ported byte-for-byte; namespace AetherSDR -> NereusSDR; `DxSpot` include moved to the extracted `DxSpot.h` (B1) instead of AetherSDR's `DxClusterClient.h`. Added public `parseJsonForTest()` seam returning the vector of NEW (post-dedup) spots from a poll, so unit tests can exercise the JSON parser and dedup set without instantiating a `QNetworkAccessManager` or simulating an HTTPS round-trip. | "POTA (Parks On The Air) HTTPS spot poller ported from AetherSDR `src/core/PotaClient.{h,cpp}` [@0cd4559]. Namespace AetherSDR -> NereusSDR; `DxSpot` include moved to extracted `DxSpot.h`; added public `parseJsonForTest()` seam returning the vector of NEW (post-dedup) spots so unit tests can validate the parser without an HTTPS round-trip. Logging routed through NereusSDR's `lcSpots` category; log file path under `AppConfigLocation` (lands under `NereusSDR/pota.log`) instead of AetherSDR's `GenericConfigLocation + AetherSDR/pota.log`." |
| `src/core/PotaClient.cpp` | `src/core/PotaClient.cpp` | Port-citation header names AetherSDR `src/core/PotaClient.cpp`. Phase 3J-2 Task B2. Inline cites at the constructor (`:16-23`), destructor (`:25-29`), `logFilePath` (`:31-35`), `startPolling` (`:37-59`), `stopPolling` (`:61-67`), `parseAndCollect` (`:90-155`, parse + dedup body extracted from upstream's `onPollTimer` lambda), and `onPollTimer` (`:69-159`, HTTP / logging / signal-emission shell). NereusSDR divergences: `qCDebug(lcDxCluster)` / `qCWarning(lcDxCluster)` -> `qCDebug(lcSpots)` / `qCWarning(lcSpots)`; log file path uses `AppConfigLocation` instead of AetherSDR's `GenericConfigLocation + "AetherSDR/pota.log"`; parse + dedup body extracted into pure `parseAndCollect()` helper so the public `parseJsonForTest()` seam can exercise the parser without an HTTPS round-trip; User-Agent header changes from `"AetherSDR"` to `"NereusSDR"`. Field mapping (activator -> dxCall, spotter -> spotterCall, frequency kHz -> freqMhz / 1000, expire fallback `> 0 ? expire : 600`, color `#RRGGBB -> #FFRRGGBB`, comment composition `ref + park + mode`, ISODate `spotTime` parse with current-UTC fallback, `freqMhz <= 0 \|\| dxCall.isEmpty()` reject filter) preserved verbatim. | "Same as `PotaClient.h` above. Parse + dedup body extracted from upstream's `onPollTimer` lambda into a pure `parseAndCollect()` helper. Field mapping, lifetime fallback (`expire > 0 ? expire : 600`), color formatting (`#RRGGBB -> #FFRRGGBB`), comment composition (`ref + park + mode`), spotTime parse, and the `freqMhz <= 0 \|\| dxCall.isEmpty()` reject filter are byte-for-byte from upstream. Logging routed through NereusSDR's `lcSpots` category; User-Agent header `AetherSDR` -> `NereusSDR`." |

### Phase 3J-2 Task B3 - DX cluster telnet client

Added 2026-05-10. Two files port the DX cluster telnet client from
AetherSDR. The client speaks the standard DX Spider / AR-Cluster /
CC-Cluster telnet dialect: TCP connect, multi-flavor login prompt
detection (login: / call: / callsign: / "Please enter your call" /
"your call"), telnet IAC byte stripping (0xFF + 2 command bytes per
sequence), "DX de" line regex parser, auto-reconnect with exponential
backoff (5s initial, 60s max). NereusSDR uses ONE DxClusterClient
class instantiated twice in RadioModel - once for the DX cluster
connection, once for the RBN connection - rather than a separate
RbnClient class; RBN-tagging happens at the spotter-suffix level
inside `parseDxSpotLine()`. Files carry a NereusSDR port-citation
header (HOW-TO-PORT.md rule 6 form - AetherSDR has no per-file GPL
header to copy verbatim) naming the upstream source.

| NereusSDR file | AetherSDR counterpart | Evidence | Specific mod-history wording |
|---|---|---|---|
| `src/core/DxClusterClient.h` | `src/core/DxClusterClient.h` | Port-citation header names AetherSDR `src/core/DxClusterClient.h`. Phase 3J-2 Task B3. Class skeleton ported byte-for-byte; namespace AetherSDR -> NereusSDR; `DxSpot` include moved to the extracted `DxSpot.h` (B1) instead of redefining `DxSpot` inline (upstream defined it at `DxClusterClient.h:13-23`). Added public test seams `parseDxSpotLineForTest()`, `isLoginPromptForTest()`, `stripTelnetIACForTest()` so unit tests can validate the parser, login-prompt detector, and telnet IAC stripper without instantiating a `QTcpSocket` or simulating a telnet server. Added `stripTelnetIACBuffer(QByteArray&)` static helper alongside the existing `stripTelnetIAC()` member so the IAC stripper algorithm is unit-testable as a pure function. | "DX cluster telnet client ported from AetherSDR `src/core/DxClusterClient.{h,cpp}` [@0cd4559]. Namespace AetherSDR -> NereusSDR; `DxSpot` include moved to extracted `DxSpot.h` (B1) instead of upstream's inline redefinition; added three public test seams (`parseDxSpotLineForTest`, `isLoginPromptForTest`, `stripTelnetIACForTest`) so unit tests can validate the parser, login-prompt detector, and IAC stripper without a live telnet socket. Logging routed through NereusSDR's `lcSpots` category; log file path under `AppConfigLocation` (lands under `NereusSDR/dxcluster.log`) instead of AetherSDR's `GenericConfigLocation + AetherSDR/dxcluster.log`. NereusSDR addition: source-label assignment moved into `parseDxSpotLine()` so a single class instance can serve both the DX cluster and RBN connections (RadioModel will instantiate two `DxClusterClient` objects); default label `Cluster`, promoted to `RBN` when the spotter callsign starts with `RBN-` (case-insensitive) or ends with `-#`." |
| `src/core/DxClusterClient.cpp` | `src/core/DxClusterClient.cpp` | Port-citation header names AetherSDR `src/core/DxClusterClient.cpp`. Phase 3J-2 Task B3. Inline cites at the constructor (`:12-25`), destructor (`:27-34`), `logFilePath` (`:36-40`), `connectToCluster` (`:42-67`), `disconnect` (`:69-78`), `sendCommand` (`:80-89`), `onConnected` (`:93-112`), `onDisconnected` (`:114-131`), `onSocketError` (`:133-138`), `onReconnectTimer` (`:140-145`), `stripTelnetIACBuffer` / `stripTelnetIAC` (`:149-160`, refactored from upstream's monolithic instance method into a pure static helper plus a thin instance wrapper so the test seam can exercise the algorithm), `onReadyRead` (`:162-198`), `handleLine` (`:200-218`), `isLoginPrompt` (`:222-232`), `parseDxSpotLine` (`:236-260`). NereusSDR divergences: `qCDebug(lcDxCluster)` / `qCWarning(lcDxCluster)` -> `qCDebug(lcSpots)` / `qCWarning(lcSpots)`; log file path uses `AppConfigLocation` instead of AetherSDR's `GenericConfigLocation + "AetherSDR/dxcluster.log"`; `stripTelnetIAC()` body extracted into pure `stripTelnetIACBuffer(QByteArray&)` static helper so the public `stripTelnetIACForTest()` seam can exercise the algorithm without a live socket; source-label assignment added to `parseDxSpotLine()` (default `Cluster`, promoted to `RBN` when spotter starts with `RBN-` case-insensitive or ends with `-#`). Spot regex (`^DX\s+de\s+(\S+?):\s+(\d+\.?\d*)\s+(\S+)\s+(.*?)\s+(\d{4})Z`), reconnect backoff math (`min(InitialReconnectDelayMs * (1 << attempts), MaxReconnectDelayMs)`), login-prompt detection logic (`endsWith("login:") \|\| endsWith("call:") \|\| endsWith("callsign:") \|\| contains("enter your call") \|\| contains("your call")`), IAC stripping algorithm (skip 0xFF + 2 command bytes per sequence), and field mapping preserved verbatim. | "Same as `DxClusterClient.h` above. `stripTelnetIAC()` body extracted into pure `stripTelnetIACBuffer(QByteArray&)` static helper so the public test seam can exercise the algorithm without a live socket. Source-label assignment added to `parseDxSpotLine()`: default `Cluster`, promoted to `RBN` when the spotter callsign starts with `RBN-` (case-insensitive) or ends with `-#`. Spot regex, reconnect backoff math, login-prompt detection list, and IAC stripping algorithm preserved verbatim from upstream. Logging routed through NereusSDR's `lcSpots` category; log file path under `AppConfigLocation` instead of AetherSDR's `GenericConfigLocation`." |

### Phase 3J-2 Task B4 - WSJT-X UDP binary protocol client

Added 2026-05-10. Two files port the WSJT-X UDP multicast client
from AetherSDR. The client speaks the WSJT-X binary UDP protocol:
big-endian QDataStream framing, magic `0xADBCCBDA`, schema number,
message type tag, then per-type fields. Two message types are
consumed - Status (type 1) updates internal dial-freq + mode state;
Decode (type 2) emits one `DxSpot` per parsed decode after gating on
isNew (skip replayed) and not lowConfidence (skip likely-false). The
`extractCallsign()` helper dispatches across the WSJT-X message
families (`CQ <call>`, `CQ DX <call>`, `CQ <directive> <call>` for
POTA / NA / EU / SA / AS / AF / OC / TEST etc., and directed
`<my> <their> <report>` / `R-report` / `RR73` exchanges) and always
returns the OTHER station (not the local operator's call). Spot
frequency is `(dialFreqHz + deltaFreqHz) / 1e6`. `DxSpot` is also
extended with `Q_DECLARE_METATYPE` (`DxSpot.h` mod-history entry) so
`QSignalSpy` can capture `spotReceived(DxSpot)` in the B4 unit test.
Files carry a NereusSDR port-citation header (HOW-TO-PORT.md rule 6
form - AetherSDR has no per-file GPL header to copy verbatim) naming
the upstream source.

| NereusSDR file | AetherSDR counterpart | Evidence | Specific mod-history wording |
|---|---|---|---|
| `src/core/DxSpot.h` (B4 amend) | `src/core/DxClusterClient.h:13-23` | Port-citation header (extended). Phase 3J-2 Task B4. Adds `Q_DECLARE_METATYPE(NereusSDR::DxSpot)` so the type can flow through `QSignalSpy` in the B4 unit test. No fields or layout changes; the existing `DxSpot` struct extracted in B1 is unchanged. | "Added `Q_DECLARE_METATYPE` so `DxSpot` can flow through `QSignalSpy` in the `tst_wsjtx_decoder` test (the WSJT-X parser tests are the first ones that spy on `spotReceived(DxSpot)` rather than calling a parser seam synchronously)." |
| `src/core/WsjtxClient.h` | `src/core/WsjtxClient.h` | Port-citation header names AetherSDR `src/core/WsjtxClient.h`. Phase 3J-2 Task B4. Class skeleton ported byte-for-byte; namespace AetherSDR -> NereusSDR; `DxSpot` include moved to the extracted `DxSpot.h` (B1) instead of upstream's transitive include from `DxClusterClient.h`. Added three public test seams `processPacketForTest(QByteArray)`, `setDialFreqForTest(double, QString)`, `extractCallsignForTest(QString)` so unit tests can drive the binary parser, seed dial-freq state, and validate the WSJT-X callsign extractor without instantiating a `QUdpSocket` or simulating a multicast sender. | "WSJT-X UDP multicast client ported from AetherSDR `src/core/WsjtxClient.{h,cpp}` [@0cd4559]. Namespace AetherSDR -> NereusSDR; `DxSpot` include moved to extracted `DxSpot.h` (B1) instead of upstream's transitive include via `DxClusterClient.h`; added three public test seams (`processPacketForTest`, `setDialFreqForTest`, `extractCallsignForTest`) so unit tests can drive the binary parser, seed dial-freq state, and validate the WSJT-X callsign extractor without a live UDP socket. Logging routed through NereusSDR's `lcSpots` category; log file path under `AppConfigLocation` (lands under `NereusSDR/wsjtx.log`) instead of AetherSDR's `GenericConfigLocation + AetherSDR/wsjtx.log`." |
| `src/core/WsjtxClient.cpp` | `src/core/WsjtxClient.cpp` | Port-citation header names AetherSDR `src/core/WsjtxClient.cpp`. Phase 3J-2 Task B4. Inline cites at the constructor (`:14-19`), destructor (`:21-25`), `logFilePath` (`:27-31`), `startListening` (`:33-72`), `stopListening` (`:74-82`), `onReadyRead` (`:86-94`), `parseMessage` (`:96-111`), `parseStatus` (`:115-130`), `parseDecode` (`:134-195`), `extractCallsign` (`:199-236`), `readQString` (`:240-255`), `readBool` (`:257-264`). NereusSDR divergences: `qCDebug(lcDxCluster)` / `qCWarning(lcDxCluster)` -> `qCDebug(lcSpots)` / `qCWarning(lcSpots)`; log file path uses `AppConfigLocation` instead of AetherSDR's `GenericConfigLocation + "AetherSDR/wsjtx.log"`. Magic check (`0xADBCCBDA`), big-endian `QDataStream` framing, Status (type 1) field order (id / dialFreqHz / mode), Decode (type 2) field order (id / isNew / timeMs / snr / deltaTime / deltaFreqHz / mode / message / lowConfidence / offAir), skip-non-new + skip-low-confidence gates, `freqHz = m_dialFreqHz + deltaFreqHz` math, DxSpot field mapping (dxCall = extracted, freqMhz = freqHz/1e6, spotterCall = "WSJT-X", source = "WSJT-X", comment = `message.trimmed()`, utcTime = `QTime::fromMSecsSinceStartOfDay(timeMs)`, snr = parsed), `extractCallsign()` callsign regex (`\b([A-Z0-9]{1,3}[0-9][A-Z0-9]{0,3}[A-Z])\b`), CQ-vs-directed dispatch (CQ skips modifier words and returns first match; directed takes parts[1]), and `readQString` UTF-8 framing (quint32 length prefix with 0xFFFFFFFF null sentinel and 10000-byte sanity cap) preserved verbatim. | "Same as `WsjtxClient.h` above. Magic check, big-endian `QDataStream` framing, Status / Decode field orders, skip-non-new and skip-low-confidence gates, `freqHz = dial + delta` math, `DxSpot` field mapping, `extractCallsign()` regex and CQ-vs-directed dispatch, and `readQString` UTF-8 framing are byte-for-byte from upstream. Logging routed through NereusSDR's `lcSpots` category; log file path under `AppConfigLocation` instead of AetherSDR's `GenericConfigLocation`." |

### Phase 3J-2 Task B5 - FreeDV Reporter Engine.IO/Socket.IO client (HYBRID)

Added 2026-05-10. Two files port the FreeDV Reporter client. Unlike
B1-B4, this is a **hybrid port** with two upstreams — the wire-protocol
authority is freedv-gui (`src/reporting/FreeDVReporter.{h,cpp}`
[@77e793a]; tracked in `FREEDV-GUI-PROVENANCE.md`), while the Qt6
structural pattern (QWebSocket + slot wiring, exponential-backoff
reconnect, dual-feed spot synthesis) follows AetherSDR's earlier
FreeDvClient port that already targeted the same Engine.IO v4 / Socket.IO
v4 wire format on top of QWebSocket. The rows below cover the
AetherSDR-originating bits; the wire-protocol bits are documented in the
freedv-gui registry. Files carry a NereusSDR port-citation header that
names BOTH upstreams, with `// --- From freedv-gui ... ---` markers
separating the verbatim-copied freedv-gui file headers from the
AetherSDR-attributed Qt6 scaffolding.

| NereusSDR file | AetherSDR counterpart | Evidence | Specific mod-history wording |
|---|---|---|---|
| `src/core/FreeDVReporterClient.h` | `src/core/FreeDvClient.h` | Port-citation header names AetherSDR `src/core/FreeDvClient.h` (alongside the freedv-gui authority). Phase 3J-2 Task B5 (HYBRID port). QWebSocket + QTimer member layout, public surface (startConnection / stopConnection / isConnected / setIdentity / setServerUrl / logFilePath), Qt slot signatures (onWsConnected / onWsDisconnected / onWsTextMessage / onWsError / onReconnectTimer), and the static reconnect constants (`InitialReconnectDelayMs = 5000`, `MaxReconnectDelayMs = 60000`, the wss URL constant) follow AetherSDR's FreeDvClient.h:67-84 verbatim. Replaces upstream's `StationInfo` private struct with the freedv-gui-faithful 14-field `FreeDVStation` (B5 new file) so the same struct can be reused by FreeDVStationModel (Task D3). NereusSDR additions: stationAdded / stationUpdated / stationRemoved signals and 4 test-seam wrappers (handleEngineIOForTest / handleSocketIOForTest / pingIntervalMsForTest / lastSentMessageForTest). | "FreeDV Reporter Engine.IO/Socket.IO client. HYBRID port: wire-protocol logic from freedv-gui `src/reporting/FreeDVReporter.{h,cpp}` [@77e793a]; Qt6 structural pattern (QWebSocket + QTimer + slot wiring, exponential-backoff reconnect, dual-feed spot synthesis) from AetherSDR `src/core/FreeDvClient.{h,cpp}` [@0cd4559]. Replaces AetherSDR's lossy 5-field StationInfo with the freedv-gui-faithful 14-field FreeDVStation so the rich Reporter dialog (Task D3) can be built. Test seam wrappers exposed so tests can drive the wire-protocol layer without an actual WebSocket round-trip." |
| `src/core/FreeDVReporterClient.cpp` | `src/core/FreeDvClient.cpp` | Port-citation header names AetherSDR `src/core/FreeDvClient.cpp` (alongside the freedv-gui authority). Phase 3J-2 Task B5 (HYBRID port). Inline cites against AetherSDR for: constructor (`:14-39`, QWebSocket + QTimer wiring + ping-keepalive lambda), `onWsConnected` (`:92-96`), `onWsDisconnected` (`:98-115`, exponential-backoff reconnect math), `onWsError` (`:117-123`), `onReconnectTimer` (`:125-131`), `onWsTextMessage` (`:135-139`), `handleEngineIO` (`:141-179`, the Engine.IO Open / Ping / Pong / Message switch), `handleSocketIO` (`:181-219`, Socket.IO Connect ACK / Event / Disconnect switch + bulk_update fan-out wrapping), `emitSpotFromFreqChange` (`:243-295`, DxSpot field mapping + AppSettings FreeDvSpotColor lookup with `#RRGGBB` -> `#FFRRGGBB` promotion + log-line composition), `emitSpotFromRxReport` (`:322-370`, same DxSpot mapping for the rx-report case). Per-event handler bodies (onNewConnection / onFreqChange / onRxReport / onTxReport / onRemoveConnection / onMessageUpdate / onConnectionSuccessful / onBulkUpdate) ported from freedv-gui FreeDVReporter.cpp [@77e793a] (cited inline; see FREEDV-GUI-PROVENANCE.md). NereusSDR divergences: `qCDebug(lcDxCluster)` / `qCWarning(lcDxCluster)` -> `qCDebug(lcSpots)` / `qCWarning(lcSpots)`; log file path uses `AppConfigLocation` (lands under `NereusSDR/freedv.log`) instead of AetherSDR's `GenericConfigLocation + AetherSDR/freedv.log`; QWebSocket member guarded by `#ifdef HAVE_WEBSOCKETS` so the class still compiles when Qt6::WebSockets is absent (test-seam parsers remain functional). | "Same as `FreeDVReporterClient.h` above. AetherSDR contributes the QWebSocket lifecycle, the Engine.IO / Socket.IO state machine (handleEngineIO / handleSocketIO), the exponential-backoff reconnect math, and the DxSpot field-mapping bodies (emitSpotFromFreqChange / emitSpotFromRxReport). freedv-gui contributes the per-event handler bodies (onNewConnection / onFreqChange / onRxReport / onTxReport / onRemoveConnection / onMessageUpdate / onConnectionSuccessful / onBulkUpdate) which translate the upstream yyjson lookups into Qt JSON lookups field-for-field. NereusSDR-architectural addition: dual-feed (every freq_change / rx_report drives BOTH stationUpdated and spotReceived) per design doc Section 4 Flow 2." |

### Phase 3J-2 Task C1 - CtyDatParser (AD1C / K1EA cty.dat lookup)

Added 2026-05-10. First task of Phase C (DXCC stack). Two source
files port the `CtyDatParser` byte-for-byte from AetherSDR plus a
`cty.dat` data-file copy. The data file is the AD1C-maintained K1EA
country file (community-maintained at `country-files.com`, no
upstream license header in the file itself). Subsequent C2-C4 will
build the worked-status overlay on top of this lookup.

| NereusSDR file | AetherSDR counterpart | Evidence | Specific mod-history wording |
|---|---|---|---|
| `src/core/CtyDatParser.h` | `src/core/CtyDatParser.h` | Port-citation header names AetherSDR `src/core/CtyDatParser.h`. Phase 3J-2 Task C1. Class skeleton ported byte-for-byte; namespace AetherSDR -> NereusSDR. `DxccEntity` field layout (`primaryPrefix`, `name`, `continent`, `cqZone`, `ituZone`) and the public surface (`loadFromFile`, `loadFromResource`, `resolvePrimaryPrefix`, `entityByPrefix`, `entityCount`, `isLoaded`) preserved verbatim from upstream. Inline cites at `DxccEntity` struct (`:9-15`) and `CtyDatParser` class (`:17-49`). | "AD1C / K1EA cty.dat country-file parser ported from AetherSDR `src/core/CtyDatParser.{h,cpp}` [@0cd4559]. Namespace AetherSDR -> NereusSDR. `DxccEntity` field layout (`primaryPrefix`, `name`, `continent`, `cqZone`, `ituZone`) and the public surface (`loadFromFile`, `loadFromResource`, `resolvePrimaryPrefix`, `entityByPrefix`, `entityCount`, `isLoaded`) preserved verbatim. First task of Phase C DXCC stack." |
| `src/core/CtyDatParser.cpp` | `src/core/CtyDatParser.cpp` | Port-citation header names AetherSDR `src/core/CtyDatParser.cpp`. Phase 3J-2 Task C1. Inline cites at `cleanPrefix` helper (`:10-23`), `loadFromFile` / `loadFromResource` (`:25-53`), `parse` (`:55-149`, header regex + commit-entity lambda + alias-token splitter + exact-match `=` token handling), `resolvePrimaryPrefix` (`:151-195`, exact-match-then-longest-prefix iteration + `/P /M /MM /AM /QRP` portable-suffix stripper + `/country` prefix-override fallback), and `entityByPrefix` (`:197-202`). Header regex (`^([^:]+):\s*(\d+):\s*(\d+):\s*(\w+):\s*[\d\.\-]+:\s*[\d\.\-]+:\s*[\d\.\-]+:\s*([^:]+):)`), zone-override stripper (`\([^)]*\)\|\[[^\]]*\]`), portable-suffix list (`P / M / MM / AM / QRP`), `length() <= 4` country-suffix gate, and longest-to-shortest prefix iteration preserved verbatim. No NereusSDR-side logic divergences; the parser is pure data transformation with no logging or settings interactions. | "Same as `CtyDatParser.h` above. `cleanPrefix` helper, header regex, commit-entity lambda, alias-token splitter, `=` exact-match token handling, `resolvePrimaryPrefix` (exact-match-then-longest-prefix iteration + portable-suffix stripper + country-suffix prefix-override fallback), and `entityByPrefix` are byte-for-byte from upstream. No logging or settings interactions." |
| `cty.dat` (data file) | `cty.dat` (data file) | Copy of AetherSDR's vendored AD1C cty.dat (community-maintained K1EA country file from country-files.com, no per-file license header). 1589 lines, 100K. Same content used by AetherSDR; both projects vendor the file at the repo root. | (No source mod-history — this is a data file with no header.) |

### Phase 3J-2 Task C2 - AdifParser (.adi / .adif amateur-radio log parser)

Added 2026-05-10. Second task of Phase C (DXCC stack). Two source
files port the `AdifParser` byte-for-byte from AetherSDR plus a
small in-tree fixture log used by the unit test. The fixture
captures 10 QSOs across 5 callsigns / 5 DXCC entities (W1AW USA,
JA1ABC Japan, VK6APH Australia, G3OCA England, DL1ABCD Germany),
3 bands (20m / 40m / 15m), and 2 modes (SSB normalised to PHONE,
CW kept as CW). The parser feeds Task C3's worked-status tracker.

| NereusSDR file | AetherSDR counterpart | Evidence | Specific mod-history wording |
|---|---|---|---|
| `src/core/AdifParser.h` | `src/core/AdifParser.h` | Port-citation header names AetherSDR `src/core/AdifParser.h`. Phase 3J-2 Task C2. Class skeleton ported byte-for-byte; namespace AetherSDR -> NereusSDR. `QsoRecord` field layout (`callsign`, `band`, `modeGroup`, `dxccPrefix`) and the public surface (static `parseFile`, `Q_INVOKABLE parseFileAsync`, `finished(QVector<QsoRecord>)` and `openFailed(QString)` signals) preserved verbatim from upstream. Inline cites at `QsoRecord` struct (`:9-14`) and `AdifParser` class (`:16-46`). NereusSDR addition: one public test seam `parseBytesForTest(const QByteArray&)` that delegates to the private `parse()` so unit tests can drive the parser against an in-memory buffer without a `QFile` round-trip (precedent: B1 / B2 / B3 / B5 introduced equivalent `*ForTest()` seams against their respective parsers). `Q_DECLARE_METATYPE(NereusSDR::QsoRecord)` added so `QSignalSpy` can serialise the `QVector<QsoRecord>` payload of `finished()` from the unit test. | "ADIF (.adi / .adif) amateur-radio log parser ported from AetherSDR `src/core/AdifParser.{h,cpp}` [@0cd4559]. Namespace AetherSDR -> NereusSDR. `QsoRecord` field layout (`callsign`, `band`, `modeGroup`, `dxccPrefix`) and the public surface (static `parseFile`, `Q_INVOKABLE parseFileAsync`, `finished` / `openFailed` signals) preserved verbatim. Added `parseBytesForTest()` test seam (precedent: B1 / B2 / B3 / B5) and `Q_DECLARE_METATYPE` for `QSignalSpy` serialisation. Second task of Phase C DXCC stack." |
| `src/core/AdifParser.cpp` | `src/core/AdifParser.cpp` | Port-citation header names AetherSDR `src/core/AdifParser.cpp`. Phase 3J-2 Task C2. Inline cites at `extractField` helper (`:10-29`, ADIF `<FIELDNAME:length>value` regex with optional `:TYPE` segment, case-insensitive), `normaliseMode` (`:31-54`, submode-aware mode-group classifier mapping FT8 / FT4 / JS8 / JT65 / JT9 / WSPR / PSK31 / PSK63 / RTTY -> DATA, CW -> CW, SSB / USB / LSB / AM / FM -> PHONE, MFSK / OLIVIA / CONTESTIA / SSTV / PACKET / HELL / ATV -> DATA, unknown-non-empty -> DATA, empty -> PHONE default), `freqToBand` (`:56-73`, IARU MHz -> band-label table 160m..70cm), `parse` (`:75-138`, `<EOH>` header skip + `<EOR>`-delimited record iteration + bare-number band-label normaliser ("10" -> "10m") + BAND-then-FREQ fallback), `parseFile` (`:140-150`), and `parseFileAsync` (`:152-173`, 3-attempt open retry with 500 ms backoff + `openFailed` emission on terminal failure). Field-extraction regex (`<FIELDNAME(?::\d+(?::[A-Z])?)?:(\d+)>`), header-skip sentinel (`<EOH>` length 5 + `Qt::CaseInsensitive`), record splitter (`<EOR>` `QRegularExpression::CaseInsensitiveOption`), bare-number band map (`160 / 80 / 60 / 40 / 30 / 20 / 17 / 15 / 12 / 10 / 6 / 4 / 2 / 70`), retry constants (`kMaxAttempts = 3`, `kRetryDelayMs = 500`), and the entire `normaliseMode` / `freqToBand` decision tables preserved verbatim. NereusSDR addition: pass-through `parseBytesForTest()` body delegating to private `parse()` (zero logic divergence, just exposes the existing parser). | "Same as `AdifParser.h` above. `extractField` helper, `normaliseMode` (submode-aware classifier with full DATA / PHONE / CW token table), `freqToBand` (IARU 160m..70cm table), `parse` (header skip + EOR-delimited iteration + bare-number band-label normaliser + BAND-then-FREQ fallback), `parseFile`, and `parseFileAsync` (3-attempt open retry with 500 ms backoff) are byte-for-byte from upstream. `parseBytesForTest()` is a pass-through to the private `parse()` so unit tests can exercise the parser against an in-memory buffer." |
| `tests/fixtures/adif/sample.adi` (test fixture) | (no upstream counterpart) | NereusSDR-original 10-QSO ADIF log used by `tst_adif_parser` for the `parsesSampleAdif`, `emitsFinishedSignal`, and `skipsHeaderSection` cases. Five callsigns (W1AW USA, JA1ABC Japan, VK6APH Australia, G3OCA England, DL1ABCD Germany), three bands (20m / 40m / 15m), and two modes (SSB / CW). Header section (`<adif_ver>`, `<created_timestamp>`, `<programid>`) ahead of `<eoh>` exercises the header-skip path. | (No source mod-history. This is an in-tree test fixture with no upstream provenance.) |

### Phase 3J-2 Task C3 - DxccWorkedStatus (per-entity / per-band / per-modeGroup worked tracker)

Added 2026-05-11. Third task of Phase C (DXCC stack). Two source
files port the `DxccWorkedStatus` byte-for-byte from AetherSDR.
The tracker consumes the `QVector<QsoRecord>` produced by Task C2's
`AdifParser` and answers (entity, band, modeGroup) worked-status
queries with the 4-tier `DxccStatus` enum (NewDxcc / NewBand /
NewMode / Worked) plus an `Unknown` sentinel for empty primary
prefixes (the cty.dat resolver returned no match). Task C4
(`DxccColorProvider`) will combine this tracker with C1's
`CtyDatParser` to drive the panadapter spot-color overlay.

| NereusSDR file | AetherSDR counterpart | Evidence | Specific mod-history wording |
|---|---|---|---|
| `src/core/DxccWorkedStatus.h` | `src/core/DxccWorkedStatus.h` | Port-citation header names AetherSDR `src/core/DxccWorkedStatus.h`. Phase 3J-2 Task C3. Class skeleton ported byte-for-byte; namespace AetherSDR -> NereusSDR. `DxccStatus` enum (`NewDxcc`, `NewBand`, `NewMode`, `Worked`, `Unknown`) and the public surface (`load(QVector<QsoRecord>)`, `clear`, `query(primaryPrefix, band, modeGroup)`, `entityCount`, `totalQsos`) preserved verbatim from upstream. Internal data layout `QHash<QString, QHash<QString, QSet<QString>>>` keyed by `primaryPrefix -> band -> set<modeGroup>` plus the `int m_totalQsos{0}` counter preserved verbatim. Forward declaration of `QsoRecord` matches upstream; `AdifParser.h` supplies the type when the `.cpp` includes it. Inline cites at `DxccStatus` enum (`:12-18`) and `DxccWorkedStatus` class (`:20-44`). | "DxccWorkedStatus per-entity / per-band / per-modeGroup worked-status tracker ported from AetherSDR `src/core/DxccWorkedStatus.{h,cpp}` [@0cd4559]. Namespace AetherSDR -> NereusSDR. `DxccStatus` enum (`NewDxcc`, `NewBand`, `NewMode`, `Worked`, `Unknown`) and the public surface (`load`, `clear`, `query`, `entityCount`, `totalQsos`) preserved verbatim. Internal `QHash<QString, QHash<QString, QSet<QString>>>` layout keyed by `primaryPrefix -> band -> set<modeGroup>` preserved verbatim. Third task of Phase C DXCC stack." |
| `src/core/DxccWorkedStatus.cpp` | `src/core/DxccWorkedStatus.cpp` | Port-citation header names AetherSDR `src/core/DxccWorkedStatus.cpp`. Phase 3J-2 Task C3. Inline cites at `load` (`:6-16`, iterates input vector and inserts into nested `QHash` skipping rows where `dxccPrefix` / `band` / `modeGroup` is empty, increments `m_totalQsos` per accepted row), `clear` (`:18-22`, resets nested `QHash` and `m_totalQsos`), and `query` (`:24-42`, empty-`primaryPrefix` early-out to `Unknown` then walks entity / band / mode in order returning `NewDxcc` / `NewBand` / `NewMode` / `Worked`). Skip-on-empty gate, totalQsos increment placement (after the gate, only on accepted rows), and the early-`Unknown` branch preserved verbatim. No NereusSDR-side logic divergences; the tracker is pure data transformation with no logging or settings interactions. | "Same as `DxccWorkedStatus.h` above. `load` (skip-on-empty gate + totalQsos increment after gate), `clear`, and `query` (empty-prefix early-`Unknown` branch + entity / band / mode walk returning `NewDxcc` / `NewBand` / `NewMode` / `Worked`) are byte-for-byte from upstream. No logging or settings interactions." |

### Phase 3J-2 Task C4 - DxccColorProvider (DXCC stack integrator)

Added 2026-05-11. Fourth and final task of Phase C (DXCC stack). Two
source files port the `DxccColorProvider` byte-for-byte from
AetherSDR. The provider owns a `CtyDatParser` (C1) + a
`DxccWorkedStatus` (C3) + a worker-thread `AdifParser` (C2) and
exposes a single GUI-thread entry point `colorForSpot(callsign,
freqMhz, mode)` that returns one of four configurable `QColor`
members (NewDxcc bright red `#FF3030`, NewBand orange `#FF8C00`,
NewMode gold `#FFD700`, Worked dim grey `#606060`) or a
default-constructed `QColor` for the Unknown case. ADIF
auto-reload is wired through a `QFileSystemWatcher` plus a
2-second `QTimer` debounce (atomic-rename re-arming and
delete-then-recreate handling preserved verbatim). Reads are
lock-free after `importFinished()` fires. Completes the
Phase C DXCC stack; Phase D (spot models) will call
`colorForSpot()` at spot-insert time to drive the panadapter
overlay tint.

| NereusSDR file | AetherSDR counterpart | Evidence | Specific mod-history wording |
|---|---|---|---|
| `src/core/DxccColorProvider.h` | `src/core/DxccColorProvider.h` | Port-citation header names AetherSDR `src/core/DxccColorProvider.h`. Phase 3J-2 Task C4. Class skeleton ported byte-for-byte; namespace AetherSDR -> NereusSDR. Public surface (`loadCtyDat`, `importAdifFile`, `setAutoReload`, `colorForSpot`, `statusForSpot`, `isEnabled` / `setEnabled`, `qsoCount`, `entityCount`, `importStarted` / `importFinished` signals) and the four configurable `QColor` members (`colorNewDxcc{0xFF, 0x30, 0x30}` bright red, `colorNewBand{0xFF, 0x8C, 0x00}` orange, `colorNewMode{0xFF, 0xD7, 0x00}` gold, `colorWorked{0x60, 0x60, 0x60}` dim grey) preserved verbatim from upstream. Member layout (`CtyDatParser` + `DxccWorkedStatus` + `m_enabled` + worker-thread `QThread` + heap-allocated `AdifParser*` + `QFileSystemWatcher` + 2-second `QTimer` debounce + `m_watchedPath`) preserved verbatim. Forward declarations of `QsoRecord` and `AdifParser` match upstream. Inline cite at `DxccColorProvider` class (`:18-89`). | "DxccColorProvider integrator ported from AetherSDR `src/core/DxccColorProvider.{h,cpp}` [@0cd4559]. Namespace AetherSDR -> NereusSDR. Public surface (`loadCtyDat`, `importAdifFile`, `setAutoReload`, `colorForSpot`, `statusForSpot`, `isEnabled` / `setEnabled`, `qsoCount`, `entityCount`, `importStarted` / `importFinished`) and the four configurable `QColor` members (NewDxcc `#FF3030` bright red, NewBand `#FF8C00` orange, NewMode `#FFD700` gold, Worked `#606060` dim grey) preserved verbatim. Owns `CtyDatParser` (C1) + `DxccWorkedStatus` (C3) + worker-thread `AdifParser` (C2). Fourth and final task of Phase C DXCC stack." |
| `src/core/DxccColorProvider.cpp` | `src/core/DxccColorProvider.cpp` | Port-citation header names AetherSDR `src/core/DxccColorProvider.cpp`. Phase 3J-2 Task C4. Inline cites at the constructor (`:9-65`, heap-allocates the `AdifParser`, moves it onto `m_parseThread`, wires queued `finished` / `openFailed` signals, starts the thread, sets up the 2-second single-shot debounce timer with the lambda that re-adds the file path before invoking `importAdifFile` to handle atomic-rename inode changes, plus the directory-changed lambda that re-arms the file watcher when the target file reappears after a delete-then-recreate), destructor (`:67-72`, `m_parseThread.quit()` + `wait()` + `delete m_parser`), `loadCtyDat` (`:74-77`, delegates to `CtyDatParser::loadFromResource`), `importAdifFile` (`:79-85`, emits `importStarted` then queues `parseFileAsync` via `QMetaObject::invokeMethod`), `setAutoReload` (`:87-107`, clears existing watched paths and stops the debounce timer; when on, registers the file path plus the parent directory with the watcher), `onParseFinished` (`:109-117`, resolves DXCC primary prefixes for every record on the GUI thread after the queued signal, loads the worked status, emits `importFinished(qsoCount, entityCount)`), `onParseFailed` (`:119-135`, re-arms the debounce timer only when the file still exists (locked case) and emits `importFinished` so the "Updating..." UI label clears), `freqToBand` (`:141-158`, IARU MHz -> band-label table 160m..70cm), the file-scope `inferModeFromFreq` helper (`:160-198`, IARU band-plan CW / DATA / PHONE segments per-band with `PHONE` default for anything outside the table), `normaliseMode` (`:200-208`, CW -> CW, SSB / USB / LSB / AM / FM / PHONE -> PHONE, everything else -> DATA), `statusForSpot` (`:210-224`, resolves primary prefix and Unknown-early-outs on empty, maps freq -> band and Unknown-early-outs on empty, then either calls `normaliseMode` on the explicit mode or `inferModeFromFreq` when the spot carries no mode), and `colorForSpot` (`:226-237`, dispatches the `DxccStatus` enum to the four `QColor` members with default-constructed `QColor()` for the Unknown case). Atomic-rename re-arming, delete-then-recreate directory-watcher handling, the 2-second debounce interval, the IARU band-plan table, the freq-to-band table, the mode-group classifier, and the default-`QColor()` Unknown sentinel preserved verbatim. No NereusSDR-side logic divergences; the integrator wires existing C1-C3 components with no behavioural changes. | "Same as `DxccColorProvider.h` above. Constructor wiring (AdifParser worker-thread move + queued signal connections + 2-second debounce timer + atomic-rename re-arming + directory-watcher re-arm on delete-then-recreate), destructor (quit + wait + delete), `loadCtyDat`, `importAdifFile` (queued `parseFileAsync`), `setAutoReload` (file + parent-directory registration), `onParseFinished` (DXCC primary-prefix resolution + worked-status load + `importFinished` emission), `onParseFailed` (re-arm only when file still exists + `importFinished` emission to clear UI), `freqToBand` (IARU 160m..70cm table), `inferModeFromFreq` (band-plan CW / DATA / PHONE segments + PHONE default), `normaliseMode` (CW / PHONE / DATA classifier), `statusForSpot` (empty-prefix and empty-band Unknown-early-outs + explicit-mode vs band-plan inference fork), and `colorForSpot` (DxccStatus dispatch to four QColor members + default-constructed QColor for Unknown) are byte-for-byte from upstream. No logging or settings interactions." |

### Phase 3J-2 Task D1 - SpotModel (TCI-keyed spot sink)

Added 2026-05-11. First task of Phase D (Models). Two source files
port the `SpotModel` byte-for-byte from AetherSDR. `SpotModel` is a
`QMap<int, SpotData>` sink keyed by monotonic spot index. The
TCI-keyed `applySpotStatus(int index, const QMap<QString,QString>&
kvs)` update API recognises 12 keys (`callsign`, `rx_freq`,
`tx_freq`, `mode`, `color`, `background_color`, `source`,
`spotter_callsign`, `comment`, `timestamp`, `lifetime_seconds`,
`priority`) and decodes the TCI 0x7F (DEL) wire-format quirk to a
single ASCII space in `callsign` and `comment`. Six signals
(`spotAdded`, `spotUpdated`, `spotRemoved`, `spotsCleared`,
`spotsRefreshed`, `spotTriggered`). The TCI-keyed contract is the
seam the 3J-1 TCI worktree's `TciServer` will hook into when it
lands. Subsequent Phase D tasks (D2 `SpotTableModel` +
`BandFilterProxy`, D3 `FreeDVStationModel`, D4 `RxDecodeModel`,
D5 `SliceModel::snrDb`) build on this foundation.

| NereusSDR file | AetherSDR counterpart | Evidence | Specific mod-history wording |
|---|---|---|---|
| `src/models/SpotModel.h` | `src/models/SpotModel.h` | Port-citation header names AetherSDR `src/models/SpotModel.h`. Phase 3J-2 Task D1. Class skeleton ported byte-for-byte; namespace AetherSDR -> NereusSDR. `SpotData` struct (14 fields: `index`, `callsign`, `rxFreqMhz`, `txFreqMhz`, `mode`, `color`, `backgroundColor`, `source`, `spotterCallsign`, `comment`, `timestamp`, `lifetimeSeconds`, `priority`, `addedMs`) preserved verbatim from upstream with their default initialisers (`lifetimeSeconds{1800}`, `priority{0}`, `addedMs{0}`, `index{-1}`). Public surface (`applySpotStatus(int, QMap<QString,QString>)`, `removeSpot(int)`, `clear()`, `refresh()`, `spots() const`) and the six signals (`spotAdded(SpotData)`, `spotUpdated(SpotData)`, `spotRemoved(int)`, `spotsCleared()`, `spotsRefreshed()`, `spotTriggered(int, QString panId)`) preserved verbatim. `Q_DECLARE_METATYPE(NereusSDR::SpotData)` added so `QSignalSpy` can capture spot signals in the test harness; upstream omits this because the AetherSDR test suite never spies on `SpotData`-carrying signals. Inline cites at `SpotData` struct (`:10-25`) and `SpotModel` class (`:27-49`). | "SpotModel TCI-keyed spot sink ported from AetherSDR `src/models/SpotModel.{h,cpp}` [@0cd4559]. Namespace AetherSDR -> NereusSDR. `SpotData` struct (14 fields) and the public surface (`applySpotStatus`, `removeSpot`, `clear`, `refresh`, `spots`) plus six signals (`spotAdded`, `spotUpdated`, `spotRemoved`, `spotsCleared`, `spotsRefreshed`, `spotTriggered`) preserved verbatim. `Q_DECLARE_METATYPE(NereusSDR::SpotData)` added so `QSignalSpy` can capture spot signals in the test harness. First task of Phase D (Models). The TCI-keyed contract is the seam the 3J-1 TCI worktree's `TciServer` will hook into." |
| `src/models/SpotModel.cpp` | `src/models/SpotModel.cpp` | Port-citation header names AetherSDR `src/models/SpotModel.cpp`. Phase 3J-2 Task D1. Inline cites at `applySpotStatus` (`:6-52`, isNew detection on first call for a given index sets `addedMs = QDateTime::currentMSecsSinceEpoch()` and emits `spotAdded`; subsequent calls emit `spotUpdated`. All 12 TCI keys dispatched verbatim. 0x7F (DEL) -> space replacement on `callsign` and `comment` via `QString(val).replace(QChar(0x7f), ' ')` preserved verbatim. `timestamp` key parsed as seconds-since-epoch via `QDateTime::fromSecsSinceEpoch(ts, Qt::UTC)` only when `toLongLong(&ok)` succeeds, matching upstream's silent-skip-on-parse-failure contract.), `removeSpot` (`:54-58`, calls `m_spots.remove(index)` and emits `spotRemoved(index)` only when the removal returned non-zero), `clear` (`:60-64`, empties the map and emits `spotsCleared` unconditionally), and `refresh` (`:66-69`, emits `spotsRefreshed` without touching the map). isNew detection, 0x7F decoding for callsign + comment, timestamp parse-then-guard, removeSpot's gated emission, and clear's unconditional emission preserved verbatim. No NereusSDR-side logic divergences; the model is pure data transformation with no logging or settings interactions. | "Same as `SpotModel.h` above. `applySpotStatus` (isNew detection sets `addedMs` and dispatches to `spotAdded` vs `spotUpdated`; all 12 TCI keys dispatched; 0x7F -> space replacement on callsign and comment; timestamp parse-then-guard), `removeSpot` (gated `spotRemoved` emission only when the remove returned non-zero), `clear` (unconditional `spotsCleared` emission), and `refresh` (`spotsRefreshed` only, no map mutation) are byte-for-byte from upstream. No logging or settings interactions." |

### Phase 3J-2 Task D2 - SpotTableModel + BandFilterProxy

Added 2026-05-11. Second task of Phase D (Models). Four source files
extract two classes byte-for-byte from AetherSDR. Both classes lived
inline in AetherSDR's `src/gui/DxClusterDialog.h:33-75` (with
implementations in `src/gui/DxClusterDialog.cpp:75-226`); we extract
them to standalone `src/models/` files because the SpotHubDialog
(Phase F) needs to reuse the same table model and band filter for
all seven spot-ingest sources (Cluster, RBN, WSJT-X, SpotCollector,
POTA, FreeDV, PSK), not just the DX cluster dialog. `SpotTableModel`
is a `QAbstractTableModel` over a bounded `QVector<DxSpot>` with 8
columns (`ColTime`, `ColFreq`, `ColDxCall`, `ColComment`,
`ColSpotter`, `ColBand`, `ColMode`, `ColSource`); newest spot at
row 0; default cap of 500 spots. `BandFilterProxy` is a
`QSortFilterProxyModel` that hides spots whose band (read via
`SpotTableModel::ColBand` `DisplayRole`) is in the `m_hiddenBands`
`QSet<QString>`. Empty / unknown bands always show.

| NereusSDR file | AetherSDR counterpart | Evidence | Specific mod-history wording |
|---|---|---|---|
| `src/models/SpotTableModel.h` | `src/gui/DxClusterDialog.h:33-58` | Port-citation header names AetherSDR `src/gui/DxClusterDialog.h:33-58`. Phase 3J-2 Task D2. Class skeleton ported byte-for-byte; namespace AetherSDR -> NereusSDR. 8-column `Column` enum (`ColTime`, `ColFreq`, `ColDxCall`, `ColComment`, `ColSpotter`, `ColBand`, `ColMode`, `ColSource`, `ColCount`) preserved verbatim. Public surface (`extractMode`, `addSpot`, `addSpots`, `clear`, `setMaxSpots`, `freqAtRow`, `rowCount`, `columnCount`, `data`, `headerData`) preserved verbatim. Default cap `m_maxSpots{500}` preserved verbatim. `DxSpot` include from extracted `DxSpot.h` (B1) instead of upstream's transitive include via `DxClusterClient.h`. NereusSDR divergence: extracted from inline-in-dialog-header to standalone `src/models/` file so the SpotHubDialog (Phase F) can reuse the same table model across all seven spot-ingest sources. Inline cite at `SpotTableModel` class (`:33-58`). | "SpotTableModel ported from AetherSDR `src/gui/DxClusterDialog.h:33-58` + `src/gui/DxClusterDialog.cpp:75-204` [@0cd4559]. Class lived inline in AetherSDR's dialog header upstream; extracted to a standalone `src/models/` file so the SpotHubDialog (Phase F) can reuse it. 8-column enum, public surface, and default 500-spot cap preserved verbatim." |
| `src/models/SpotTableModel.cpp` | `src/gui/DxClusterDialog.cpp:75-204` | Port-citation header names AetherSDR `src/gui/DxClusterDialog.cpp:75-204`. Phase 3J-2 Task D2. Inline cites at `extractMode` (`:75-88`, known mode-token set of 20 entries with first-or-last-word match against the comment, returns empty string when no match), `data` (`:90-126`, DisplayRole switch over the 8 columns + TextAlignmentRole right-aligned-VCenter for ColFreq + center-aligned for ColTime + ForegroundRole accent `#00B4D8` cyan for ColDxCall + `#E0D060` yellow-ish for ColFreq + UserRole returning `freqMhz` on ColFreq for sortable freq), `headerData` (`:128-143`, 8 fixed labels: "Time", "Freq (kHz)", "DX Call", "Mode", "Comment", "Spotter", "Band", "Source"), `addSpot` (`:145-156`, `beginInsertRows({}, 0, 0)` + `prepend` + `endInsertRows`, then trim to `m_maxSpots` if exceeded with `beginRemoveRows`/`resize`/`endRemoveRows`), `addSpots` (`:158-173`, batch-prepend in reverse so newest stays at index 0, then trim), `freqAtRow` (`:175-180`, bounds-checked accessor returning 0.0 on out-of-range), `clear` (`:182-187`, `beginResetModel`/`clear`/`endResetModel`), and `bandForFreq` (`:189-204`, IARU 160m..2m closed-interval lookup table returning empty string for VHF/UHF outside 144-148 MHz). Foreground colours, sort-key UserRole on ColFreq, batch-prepend reverse iteration to preserve newest-first ordering, and the IARU band table preserved verbatim. No NereusSDR-side logic divergences; the model is pure data transformation with no logging or settings interactions. | "Same as `SpotTableModel.h` above. `extractMode` (20-token mode set, first-or-last-word match), `data` (DisplayRole + TextAlignmentRole + ForegroundRole + UserRole-on-ColFreq), `headerData` (8 fixed labels), `addSpot` / `addSpots` (prepend + cap-trim, reverse iteration in `addSpots` so newest stays at row 0), `freqAtRow` (bounds-checked), `clear` (model-reset), and `bandForFreq` (IARU 160m..2m lookup) are byte-for-byte from upstream. Foreground colours (DxCall accent `#00B4D8`, Freq `#E0D060`) preserved verbatim. No logging or settings interactions." |
| `src/models/BandFilterProxy.h` | `src/gui/DxClusterDialog.h:62-75` | Port-citation header names AetherSDR `src/gui/DxClusterDialog.h:62-75`. Phase 3J-2 Task D2. Class skeleton ported byte-for-byte; namespace AetherSDR -> NereusSDR. Public surface (`setBandVisible(QString, bool)`, `isBandVisible(QString) const`) and protected `filterAcceptsRow` override preserved verbatim. Internal `QSet<QString> m_hiddenBands` membership semantics preserved verbatim (band added to set -> hidden; absent -> visible). NereusSDR divergence: extracted from inline-in-dialog-header to standalone `src/models/` file so the SpotHubDialog (Phase F) can reuse it. Inline cite at `BandFilterProxy` class (`:62-75`). | "BandFilterProxy ported from AetherSDR `src/gui/DxClusterDialog.h:62-75` + `src/gui/DxClusterDialog.cpp:208-226` [@0cd4559]. Class lived inline in AetherSDR's dialog header upstream; extracted to a standalone `src/models/` file so the SpotHubDialog (Phase F) can reuse it. Public surface, protected `filterAcceptsRow` override, and `QSet<QString>` membership semantics preserved verbatim." |
| `src/models/BandFilterProxy.cpp` | `src/gui/DxClusterDialog.cpp:208-226` | Port-citation header names AetherSDR `src/gui/DxClusterDialog.cpp:208-226`. Phase 3J-2 Task D2. Inline cites at `setBandVisible` (`:208-215`, toggles `m_hiddenBands.insert(band)` / `.remove(band)` based on visibility flag and calls `invalidateFilter()`), and `filterAcceptsRow` (`:217-226`, empty `m_hiddenBands` -> always accept fast path; otherwise looks up `SpotTableModel::ColBand` `DisplayRole` for the source row; empty band string -> always show; otherwise membership check `!m_hiddenBands.contains(band)`). Empty-set fast path, empty-band always-show fall-through, and the membership-check semantics preserved verbatim. No NereusSDR-side logic divergences; the proxy is pure filter logic with no logging or settings interactions. | "Same as `BandFilterProxy.h` above. `setBandVisible` (toggles `QSet` membership and reinvalidates the filter) and `filterAcceptsRow` (empty-set fast path, empty-band always-show, otherwise membership check against `SpotTableModel::ColBand` `DisplayRole`) are byte-for-byte from upstream. No logging or settings interactions." |

### Phase 3J-2 Task E1 - SpectrumWidget drawSpotMarkers + click hit-test

Added 2026-05-11. First task of Phase E (panadapter spot overlay).
Extends the existing `src/gui/SpectrumWidget.{h,cpp}` (NereusSDR's
multi-source Thetis + AetherSDR widget; see Bucket C entries) with the
upstream spot-overlay subsystem: three nested structs (`SpotMarker`,
`SpotCluster`, `SpotHitRect`), the public setter / config API
(`setSpotMarkers`, `setShowSpots`, `setSpotFontSize`, `setSpotMaxLevels`,
`setSpotStartPct`, `setSpotOverrideColors`, `setSpotOverrideBg`,
`setSpotColor`, `setSpotBgColor`, `setSpotBgOpacity`), the new
`spotTriggered(int)` signal, the `drawSpotMarkers` render method, and
the `showSpotClusterPopup` cluster-badge popup. The widget's existing
`paintEvent` (CPU path) and GPU overlay rebuild block both gain a
`drawSpotMarkers` call between the spectrum / waterfall layers and the
VFO marker, mirroring AetherSDR's ordering. `mousePressEvent` gains a
spot-label + cluster-badge hit-test before the existing dBm-strip /
divider / freq-scale / filter-edge / pan-drag chain.

| NereusSDR file | AetherSDR counterpart | Evidence | Specific mod-history wording |
|---|---|---|---|
| `src/gui/SpectrumWidget.h` (E1 extension) | `src/gui/SpectrumWidget.h:283-294, 297-300, 302-311, 327, 387-388, 635-651` | Three nested structs preserved verbatim: `SpotMarker` (10 fields: `index`, `callsign`, `freqMhz`, `color`, `mode`, `dxccColor`, `source`, `spotterCallsign`, `comment`, `timestampMs`), `SpotCluster` (`rect`, `spots`), `SpotHitRect` (`rect`, `freqMhz`, `markerIndex`). Public setter / config surface (`setSpotMarkers`, `setShowSpots`, `setSpotFontSize`, `setSpotMaxLevels`, `setSpotStartPct`, `setSpotOverrideColors`, `setSpotOverrideBg`, `setSpotColor`, `setSpotBgColor`, `setSpotBgOpacity`) preserved verbatim. New `spotTriggered(int)` signal added beside the existing `frequencyClicked(double hz)` signal. Private member defaults preserved verbatim: `m_showSpots{true}`, `m_spotFontSize{16}`, `m_spotMaxLevels{3}`, `m_spotStartPct{50}`, `m_spotOverrideColors{false}`, `m_spotOverrideBg{true}`, `m_spotColor{Qt::yellow}`, `m_spotBgColor{Qt::black}`, `m_spotBgOpacity{48}`. Private method declarations `drawSpotMarkers(QPainter&, QRect)` and `showSpotClusterPopup(SpotCluster, QPoint)` mirror upstream. NereusSDR-only additions: three `*ForTest()` const accessors (`spotMarkersForTest`, `spotClickRectsForTest`, `spotClustersForTest`) plus a `drawSpotMarkersForTest` shim so `tests/tst_spot_overlay_render` can assert geometry without granting friend access. Inline cites at the three struct definitions, the public-surface block, the signal, the private method declarations, and the private member variable declarations. | "Spot-overlay subsystem added to the existing `SpectrumWidget.{h,cpp}` per Phase 3J-2 Task E1, porting AetherSDR `src/gui/SpectrumWidget.{h,cpp}` [@0cd4559] (drawSpotMarkers + showSpotClusterPopup at `:4497-4633` and `:4635-4672`; struct definitions at `:283-300, 635-639`; public surface at `:302-311`; signal at `:327`). Three structs, public surface, signal, and private member defaults preserved verbatim. NereusSDR additions: three `*ForTest()` const accessors and one `drawSpotMarkersForTest` shim for unit tests; the `frequencyClicked(double hz)` signal stays in Hz units (the cluster popup and click hit-test multiply `freqMhz * 1e6` to match the existing Hz wire signature)." |
| `src/gui/SpectrumWidget.cpp` (E1 extension) | `src/gui/SpectrumWidget.cpp:1623-1644, 3787, 4303-4307, 4497-4633, 4635-4672` | Five new method bodies: `setSpotMarkers` (`:4303-4307`, replaces `m_spotMarkers` and triggers `update()` instead of upstream's `markOverlayDirty()` because NereusSDR's overlay cache invalidation hooks aren't wired in for the spot path on initial port), `drawSpotMarkers` (`:4497-4633`, full collision-avoiding multi-level stacker + overflow cluster + tick + pill + label algorithm), and `showSpotClusterPopup` (`:4635-4672`, popup menu with formatted spot lines). Per-method coordinate-helper substitution: AetherSDR `mhzToX(spot.freqMhz)` becomes NereusSDR `hzToX(spot.freqMhz * 1.0e6, specRect)` to match NereusSDR's Hz-based coordinate helper. Click handler `emit frequencyClicked(spot.freqMhz)` becomes `emit frequencyClicked(spot.freqMhz * 1.0e6)` to match NereusSDR's Hz-based signal signature. Default cyan colour (`#00b4d8`), DXCC color priority chain, override-color fallback, override-background pill alpha math (`m_spotBgOpacity * 255 / 100`), vertical dotted-tick pen (1px dotted, RGB from spot color + alpha 120), maxBottom = startY + th * m_spotMaxLevels overflow threshold, 40-px `ClusterBinWidth` for overflow grouping, cluster badge styling (`QColor(0x30,0x50,0x70,200)` filled, `QColor(0xff,0xc0,0x40)` amber text), and popup menu stylesheet (`#0f0f1a` background, `#305070` border, `#c8d8e8` text, `#1a3a5a` hover) preserved verbatim from upstream. CPU `paintEvent` paint sequence gains `drawSpotMarkers(p, specRect)` between `drawWaterfall` and `drawVfoMarker` (mirrors upstream paint ordering at `:3787` placing the spot overlay between TNF and slice markers). GPU overlay rebuild block gains the same call site. `mousePressEvent` gains the spot click hit-test loop (`:1623-1644`) right after the `event->button() != Qt::LeftButton` guard, before the dBm-strip / divider / freq-scale / filter-edge / pan-drag chain. New `<QMap>` + `<QMenu>` includes added alongside existing Qt headers. | "Same as `SpectrumWidget.h` above. Five new method bodies (`setSpotMarkers`, `drawSpotMarkers`, `showSpotClusterPopup` + their call-site wires in `paintEvent` / GPU overlay rebuild / `mousePressEvent`). Color-priority chain, multi-level vertical stacker with re-scan-from-top on collision, overflow into `+N` cluster badges at `maxBottom + 2`, 40-px ClusterBinWidth, vertical dotted tick, optional background pill, cluster badge styling, and popup menu stylesheet preserved verbatim from AetherSDR `src/gui/SpectrumWidget.cpp:4497-4633` [@0cd4559]. NereusSDR coordinate-helper substitution: `mhzToX(freqMhz)` becomes `hzToX(freqMhz * 1e6, specRect)`; the `frequencyClicked` click handler multiplies `freqMhz * 1e6` so the existing Hz-based signal contract continues to work. `setSpotMarkers` uses `update()` instead of upstream's `markOverlayDirty()` because NereusSDR's overlay-cache invalidation hooks aren't wired for the spot path on initial port (full wire-up lands when SpotHub setup wires `setShowSpots` + the per-source colour toggles)." |

Companion test file `tests/tst_spot_overlay_render.cpp` (NOT listed in
Bucket A, mirroring the precedent of every other Phase 3J-2 test:
`tst_spot_model`, `tst_spot_table_model`, `tst_dx_cluster_client`,
`tst_pota_client`, `tst_wsjtx_decoder`, `tst_freedv_reporter_client`,
`tst_psk_reporter_client`, `tst_cty_dat_parser`, `tst_adif_parser`,
`tst_dxcc_worked_status`, `tst_dxcc_color_provider`,
`tst_freedv_station_model`, `tst_rx_decode_model`, `tst_slice_model_snr`).
Six tests pinning the upstream algorithm contract:
`emptyOverlayDrawsNothing` (empty `m_spotMarkers` -> zero click rects +
zero clusters), `singleSpotDrawsOneLabel` (one in-range spot -> one
click rect with correct `freqMhz` + `markerIndex`),
`overlappingSpotsStackVertically` (3 collisions at the same Hz -> 3
click rects with strictly increasing `top()`),
`overflowSpotsBecomeClusterBadge` (8 spots with `setSpotMaxLevels(2)`
-> 2 click rects + 1 cluster of 6), `clickRectAtSpotXTuneable` (rect
center hit-test contains the marker x), `rejectsBeyondVisibleRange`
(spots outside `m_centerHz ± bandwidthHz/2` dropped, only in-range
spot produces a click rect). Test seam is the public
`drawSpotMarkersForTest(QPainter&, QRect)` shim + the three `*ForTest()`
const accessors so the tests can inspect post-render state without
granting `friend` access. Render path renders into an offscreen
`QImage` via a `QPainter`. Fixture callsigns (`TEST1..TEST9`,
`OVR1..OVR8`, `TESTLO`, `TESTHI`) are fabricated so the collision and
overflow paths can be exercised deterministically without real amateur
callsigns; `no-port-check` header set with the precedent list (B2-B6,
C1-C4, D1-D5).

### Phase 3J-2 Task F1 - SpotHubDialog shell with 9-tab strip

Added 2026-05-11. First task of Phase F (Hub dialog). Ports the
shell of AetherSDR's `src/gui/DxClusterDialog.{h,cpp}` [@0cd4559] -
the constructor, the top-level `QTabWidget` with 9 tabs in upstream
order (Cluster / RBN / WSJT-X / SpotCollector / POTA / FreeDV /
PSK Reporter / Spot List / Display), and the seventeen signal
declarations forwarded from the per-source sub-tabs. Each
`build<Source>Tab()` is a stub adding a placeholder QLabel. F2
(per-source tabs), F3 (Spot List), and F4 (Display) build the
content. Renamed `DxClusterDialog` -> `SpotHubDialog` to match the
expanded scope (Cluster + RBN + 5 other ingest sources + Spot List
+ Display).

| NereusSDR file | AetherSDR counterpart | Evidence | Specific mod-history wording |
|---|---|---|---|
| `src/gui/SpotHubDialog.h` | `src/gui/DxClusterDialog.h:79-215` | Port-citation header names AetherSDR `src/gui/DxClusterDialog.{h,cpp}` (shell only). Phase 3J-2 Task F1. Constructor signature mirrors upstream's six-client + DxccColorProvider arg layout. NereusSDR divergences: (1) replaces upstream's trailing `RadioModel* radioModel` argument with `SpotModel* spots` (the TCI-keyed spot sink from Task D1); routing of `tuneRequested(double)` into the active RadioModel happens in MainWindow when the dialog is instantiated. (2) Replaces upstream's `HAVE_WEBSOCKETS`-gated `FreeDvClient` with the always-built `FreeDVReporterClient` (Task B5; same Engine.IO / Socket.IO contract, no compile-time gating). (3) Adds a `PskReporterClient* pskClient` argument and a PSK Reporter tab between FreeDV and Spot List (NereusSDR Task B6; upstream has no PSK Reporter tab). Seventeen signals declared: `settingsChanged`, `connectRequested(QString,quint16,QString)`, `disconnectRequested`, `rbnConnectRequested(QString,quint16,QString)`, `rbnDisconnectRequested`, `wsjtxStartRequested(QString,quint16)`, `wsjtxStopRequested`, `spotCollectorStartRequested(quint16)`, `spotCollectorStopRequested`, `potaStartRequested(int)`, `potaStopRequested`, `freedvStartRequested`, `freedvStopRequested`, `pskStartRequested`, `pskStopRequested`, `tuneRequested(double)`, `spotsClearedAll`. Inline cite at the class definition. | "SpotHub dialog ported from AetherSDR `src/gui/DxClusterDialog.{h,cpp}` [@0cd4559]. Class renamed to `SpotHubDialog` to match expanded scope. Constructor mirrors upstream's six-client + DxccColorProvider arg layout but replaces upstream's trailing `RadioModel*` with `SpotModel*` (Task D1) and adds a `PskReporterClient*` argument plus a corresponding PSK Reporter tab between FreeDV and Spot List. FreeDV tab is always built (no `HAVE_WEBSOCKETS` gate). Tab order: Cluster / RBN / WSJT-X / SpotCollector / POTA / FreeDV / PSK Reporter / Spot List / Display. F1 ships the shell only; each `build*Tab()` is a stub adding a placeholder QLabel. F2-F4 fill in content." |
| `src/gui/SpotHubDialog.cpp` | `src/gui/DxClusterDialog.cpp:230-273` | Port-citation header names AetherSDR `src/gui/DxClusterDialog.cpp` (shell only). Phase 3J-2 Task F1. Constructor body (`SpotHubDialog::SpotHubDialog`) follows upstream `:230-273`: `setWindowTitle("SpotHub")`, `setMinimumSize(680, 560)`, `resize(760, 640)`, root `QVBoxLayout` with `setSpacing(0)` + `setContentsMargins(4, 4, 4, 4)`, `QTabWidget` with verbatim panel-border + tab-color stylesheet (`QTabWidget::pane { border: 1px solid #203040; }`, `QTabBar::tab { background: #1a1a2e; color: #808890; border: 1px solid #203040; padding: 6px 16px; margin-right: 2px; }`, `QTabBar::tab:selected { background: #0f0f1a; color: #00b4d8; border-bottom: none; }`), then nine `build*Tab(tabs)` calls in upstream order with the PSK tab inserted between FreeDV and Spot List. F1 ships nine stub bodies; each `build*Tab` creates a `QWidget` page with a `QVBoxLayout` containing one `QLabel` placeholder, then calls `tabs->addTab(page, "<Source>")`. NereusSDR divergences: spot-batch timer, per-source connection / spot / log wiring, log-file tailing, and the 17 inline lambdas that forward `rawLineReceived` / `spotReceived` / `connected` / `disconnected` / `connectionError` signals into UI state are deferred to F2 (per-source tabs), F3 (Spot List), and F4 (Display). FreeDV tab is unconditional in NereusSDR (upstream gated on `HAVE_WEBSOCKETS`). | "Same as `SpotHubDialog.h` above. Constructor body, window-title / sizing, root `QVBoxLayout` + content-margin / spacing, `QTabWidget` panel-border + tab-color stylesheet (background `#1a1a2e`, selected `#0f0f1a` + accent `#00b4d8`, panel border `#203040`), and the nine `build*Tab(tabs)` invocations are byte-for-byte from upstream. F1 stub bodies add a placeholder QLabel per tab; F2-F4 replace them. Per-source connect / disconnect / spot / log lambdas, spot-batch timer, and log-file tailing are deferred to F2 / F3 / F4." |

Companion test file `tests/tst_spothub_dialog_smoke.cpp` (not listed in
Bucket A, mirroring the precedent of every Phase 3J-2 test).
Three tests pinning the F1 contract: `dialogConstructs` (all six
clients + SpotModel + DxccColorProvider supplied, dialog non-null),
`hasNineTabs` (`findChild<QTabWidget*>` returns a tab strip with
exactly 9 tabs), `tabOrderMatchesAetherSdr` (tab labels at indices
0-8 match Cluster / RBN / WSJT / SpotCollector / POTA / FreeDV /
PSK / Spot List / Display in upstream order). `no-port-check`
header set because the test constructs fixture clients only and has
no callsign or wire-payload fixtures.

### Phase 3J-2 Task F2 - SpotHubDialog per-source tab content (uniform template)

Builds on F1 by fleshing out the seven per-source tab builders with
the uniform template (connection-control grid + auto-start toggle +
start/stop button + status label + raw-event console). Cluster,
RBN, WSJT-X, SpotCollector, POTA, and FreeDV port verbatim from
upstream `src/gui/DxClusterDialog.cpp:637-1596 [@0cd4559]`. PSK
Reporter is NereusSDR-native (no AetherSDR equivalent) and uses the
same uniform shape with `pskCallEdit` and `pskGridEdit` identity
inputs.

| NereusSDR file | AetherSDR file | Status: what was preserved verbatim and what differs | Modification-History line shape |
|---|---|---|---|
| `src/gui/SpotHubDialog.h` (F2 extension) | `src/gui/DxClusterDialog.h:141-199` | F2 expands the F1 shell. Forward-declares `QLineEdit`, `QSpinBox`, `QPushButton`, `QLabel`, `QCheckBox`, `QPlainTextEdit` so the private member block can hold the per-source widget pointers. Private section grows by 41 member pointers ported verbatim from upstream `DxClusterDialog.h:141-199` and grouped by source tab (`Cluster`, `RBN`, `WSJT-X`, `SpotCollector`, `POTA`, `FreeDV`, PSK Reporter). PSK Reporter sub-block (six pointers: `m_pskCallEdit`, `m_pskGridEdit`, `m_pskStartBtn`, `m_pskAutoStartBtn`, `m_pskStatusLabel`, `m_pskConsole`) is NereusSDR-native. NereusSDR omits upstream's `m_radioModel` because Task F1 already replaced the trailing `RadioModel*` constructor argument with the TCI-keyed `SpotModel*`; routing of `tuneRequested(double)` into the active radio happens in MainWindow. Spot-batch timer, `m_spotBatch` vector, `flushSpotBatch()`, log-file tailing helpers, `m_totalSpotsLabel`, `m_dxccStatsLabel`, `m_spotModel` (the dialog-local `SpotTableModel`), `m_spotTable`, and `m_proxyModel` are deferred to F3 (Spot List) and F4 (Display). | "Per-source tab content (uniform template) ported from AetherSDR `src/gui/DxClusterDialog.h:141-199` [@0cd4559]. Header expanded with 41 widget-pointer members grouped by source tab (Cluster, RBN, WSJT-X, SpotCollector, POTA, FreeDV). PSK Reporter sub-block (six pointers) is NereusSDR-native. Spot-batch timer, log-file tailing, and the Spot List + Display member pointers stay deferred to F3 and F4." |
| `src/gui/SpotHubDialog.cpp` (F2 extension) | `src/gui/DxClusterDialog.cpp:637-1596` | F2 replaces the seven F1 placeholder stubs with the actual tab content. Each builder ports the upstream verbatim with three NereusSDR-side conventions: (a) `objectName()` is set on every test-relevant widget (`clusterHostEdit`, `wsjtxFilterCQ`, `pskStartBtn`, etc.) so the smoke-test harness can locate widgets via `findChild()`; (b) every client-pointer dereference is nullptr-guarded so a dialog constructed with `nullptr` clients (the test fixture path) does not crash; (c) the seven inline stylesheets that upstream repeats per tab are factored to file-scope `constexpr` strings (`kLineEditStyle`, `kSpinBoxStyle`, `kAutoToggleStyle`, `kStartBtnStyle`, `kStatusIdleStyle`, `kConsoleStyle`, `kCmdEditStyle`) at the top of the .cpp, plus a `swatchStyle(QColor)` helper for the 11 inline color-swatch buttons. Per-tab citation breakdown: `buildClusterTab()` ports `DxClusterDialog.cpp:637-803` (Server / Port / Callsign grid + Auto-Connect toggle + Connect/Disconnect button + status + console + command-input row; AppSettings keys `DxClusterHost` default `"dxc.nc7j.com"`, `DxClusterPort` default `7300`, `DxClusterCallsign`, `DxClusterAutoConnect`, `DxClusterSpotColor` default `"#D2B48C"`, all preserved verbatim). `buildRbnTab()` ports `:805-992` (same skeleton + Rate Limit spinbox row; keys `RbnHost` default `"telnet.reversebeacon.net"`, `RbnPort` default `7000`, `RbnCallsign` falling back to `DxClusterCallsign`, `RbnRateLimit` default `10`, `RbnAutoConnect`, `RbnSpotColor` default `"#4488FF"`, all preserved verbatim). `buildWsjtxTab()` ports `:994-1237` (Address / Port grid + Auto-Start toggle + Start/Stop button + three filter checkboxes `CQ` / `CQ POTA` / `Calling Me` with inline color pickers + Default color picker + spot-life slider 30-300s + console; keys `WsjtxAddress` default `"224.0.0.1"`, `WsjtxPort` default `2237`, `WsjtxAutoStart`, `WsjtxFilterCQ` / `WsjtxFilterPOTA` / `WsjtxFilterCallingMe` defaults `"True"`, `WsjtxColorCQ` `"#00FF00"`, `WsjtxColorPOTA` `"#00FFFF"`, `WsjtxColorCallingMe` `"#FF0000"`, `WsjtxColorDefault` `"#FFFFFF"`, `WsjtxSpotLifetime` default 120, all preserved verbatim; mutex between `wsjtxFilterCQ` and `wsjtxFilterPOTA` preserved). `buildSpotCollectorTab()` ports `:1239-1345` (UDP port spinbox + help text + Auto-Start toggle + Start/Stop button + status + console; keys `SpotCollectorPort` default `9999`, `SpotCollectorAutoStart`, preserved verbatim). `buildPotaTab()` ports `:1347-1479` (poll-interval spinbox 15-300 sec + Auto-Start toggle + Start/Stop button + status + console + spot-color picker; keys `PotaPollInterval` default `30`, `PotaAutoStart`, `PotaSpotColor` default `"#FFFF00"`, preserved verbatim). `buildFreeDvTab()` ports `:1482-1596` (fixed server label `qso.freedv.org (WebSocket)` + Auto-Start toggle + Start/Stop button + status + console + spot-color picker; keys `FreeDvAutoStart`, `FreeDvSpotColor` default `"#FF8C00"`, preserved verbatim). NereusSDR-side deviation from upstream FreeDV: built unconditionally instead of behind `HAVE_WEBSOCKETS` because `FreeDVReporterClient` (Task B5) is a native QWebSocket + nlohmann::json port rather than an optional dependency. `buildPskTab()` is NereusSDR-native (no AetherSDR equivalent) and uses the F2 uniform template: Callsign + Grid identity grid + help text + Auto-Start toggle + Start/Stop button + status + console; keys `PskReporterCallsign` (falling back to `DxClusterCallsign`), `PskReporterGrid`, `PskReporterAutoStart`. `GuardedSlider` substituted with plain `QSlider` for the WSJT-X spot-life slider; upstream uses `GuardedSlider` but the slider here does not need wheel-event guarding (it lives in a modal-style hub dialog, not in the spectrum overlay). Spot-batch timer, log-file tailing, per-source `rawLineReceived` / `spotReceived` / `connected` / `disconnected` signal forwarding into UI state, and the 17 inline lambdas that update status labels in real time are deferred to a future F2b polish task or to F3 (Spot List) / F4 (Display). | "Same as `SpotHubDialog.h` above. Seven per-source tab builders fleshed out with the uniform template. Cluster / RBN / WSJT-X / SpotCollector / POTA / FreeDV port verbatim from upstream `DxClusterDialog.cpp:637-1596`. PSK Reporter is NereusSDR-native. AppSettings key names, default values, stylesheets, color-swatch buttons, and the WSJT-X filter mutex preserved verbatim. NereusSDR-side conventions: `objectName()` on every test-relevant widget so smoke tests can find them; nullptr-guarded client-pointer dereferences for the test-fixture path; shared `constexpr` stylesheet fragments at file scope to reduce duplication; FreeDV tab built unconditionally (no `HAVE_WEBSOCKETS` gate); `QSlider` substituted for `GuardedSlider` on the WSJT-X spot-life slider. Spot-batch timer, log-file tailing, and the real-time per-source status-label lambdas are deferred." |

Companion test file `tests/tst_spothub_dialog_smoke.cpp` extension
(not listed in Bucket A, same precedent as F1). Twelve new tests
pinning the F2 contract: `clusterTabHasHostPortCall` (host/port/call
edits + cmd edit + send btn discoverable by objectName),
`rbnTabHasHostPortCallRate` (host/port/call/rate spin + cmd edit +
send btn), `wsjtxTabHasAddrPort` (multicast addr / port / spot-life
slider), `wsjtxTabHasFilterCheckboxes` (CQ / POTA / CallingMe three
checkboxes), `wsjtxTabHasFourColorPickers` (CQ / POTA / CallingMe /
Default four color buttons), `spotCollectorTabHasPortSpin` (UDP
port spin), `potaTabHasIntervalSpin` (poll-interval spin + color
btn), `freedvTabHasAutoStartAndConsole` (auto-start btn + console +
color btn), `pskTabHasCallsignField` (callsign + grid edits +
auto-start + start/stop + status + console), and
`everySourceTabHasUniformTemplate` (cross-cutting: every per-source
tab carries auto-toggle + start/stop + status + console, accepting
either the `*AutoConnect*` (cluster/rbn) or `*AutoStart*` (other)
button naming convention, and either the `*Connect*` (cluster/rbn)
or `*Start*` (other) primary action button name). All 15 tests pass
(3 F1 + 12 F2).

### Phase 3J-2 Task F3 - SpotHubDialog Spot List tab

Builds on F2 by replacing the F1 stub for `buildSpotListTab` with the
merged 8-column QTableView bound to a `BandFilterProxy` wrapped
around a dialog-owned `SpotTableModel`. The tab adds a row of 12
band-filter pills (160m..2m), a row of 7 source-filter pills (DX /
RBN / JT / COL / POT / FDR / PSK), a spot-count label, and a Clear
button. Double-click on any row emits `tuneRequested(double)` for
MainWindow to route to the active slice. Every ingest client's
`spotReceived(DxSpot)` signal is wired to `m_spotTableModel->addSpot`
so the table shows the cross-source merge.

Ported from AetherSDR `src/gui/DxClusterDialog.cpp:1599-1717
[@0cd4559]` (the `buildSpotListTab` function) with three NereusSDR-
side divergences. Also extends `src/models/BandFilterProxy.{h,cpp}`
with a `setSourceVisible(source, visible)` / `isSourceVisible`
API mirroring the band filter; `filterAcceptsRow` now applies
both band and source predicates with AND semantics.

| NereusSDR file | AetherSDR counterpart | Evidence | Specific mod-history wording |
|---|---|---|---|
| `src/gui/SpotHubDialog.h` (F3 extension) | `src/gui/DxClusterDialog.h:200-209` | Adds three private member pointers (`m_spotTableModel`, `m_spotProxyModel`, `m_spotTable`) for the Spot List tab. Adds forward declarations for `QTableView`, `SpotTableModel`, `BandFilterProxy`. The existing `tuneRequested(double)` signal from F1 is reused. NereusSDR divergence: upstream pinned `m_spotModel` / `m_proxyModel` / `m_spotTable` to the Cluster tab only; in NereusSDR they aggregate all seven ingest clients. | "Spot List tab content (F3): three new member pointers (`m_spotTableModel`, `m_spotProxyModel`, `m_spotTable`) and forward declarations for `QTableView`, `SpotTableModel`, `BandFilterProxy`. NereusSDR-specific: the table model is fed by every ingest client (cross-source merge), not just Cluster as in upstream." |
| `src/gui/SpotHubDialog.cpp` (F3 extension) | `src/gui/DxClusterDialog.cpp:1599-1717` | Replaces the F1 placeholder stub with the merged 8-column QTableView bound to a `BandFilterProxy(SpotTableModel)`. Column widths preserved verbatim from upstream `:1675-1682` (Time 50, Freq 80, DxCall 90, Mode 45, Comment 200, Spotter 80, Band 45, Source 55). Table stylesheet (`kSpotTableStyle` file-scope constant) preserved verbatim from upstream `:1652-1672`: dark `#0a0a14` background, cyan `#00b4d8` header text, `#1a3a5a` selection background, gridline `#1a2a3a`. Double-click handler at `:1688-1693` ported verbatim: `mapToSource` -> `freqAtRow(srcIdx.row())` -> `emit tuneRequested(freq)` if `freq > 0.0`. Bottom row (`:1697-1714`): spot count label connected to `rowsInserted` + Clear button calling `m_spotTableModel->clear()`. NereusSDR divergences: (1) band filter row uses 12 checkable `QPushButton` pills (`kFilterPillStyle`) instead of upstream's 11 `QCheckBox` controls; the extra pill is `2m` (upstream stopped at 6m even though SpotTableModel's `bandForFreq` already produces "2m"); (2) new source filter row with 7 pills (DX/RBN/JT/COL/POT/FDR/PSK) driving the new `BandFilterProxy::setSourceVisible` (NereusSDR-native); (3) every ingest client's `spotReceived(DxSpot)` is wired through a `wireClient` template-lambda into `m_spotTableModel->addSpot` so the table shows the cross-source merge (upstream only fed Cluster spots into it). AppSettings keys: `SpotBandFilter_<band>` preserved verbatim (defaults to `"True"`); `SpotSourceFilter_<label>` added per pill label (NereusSDR-native; defaults to `"True"`). Source-pill labels (DX/RBN/JT/COL/POT/FDR/PSK) map to the upstream source strings emitted by each client (`Cluster`/`RBN`/`WSJT-X`/`SpotCollector`/`POTA`/`FreeDV`/`PSK`) via the local `SourcePill { label, source }` struct. Adds new `kFilterPillStyle` and `kSpotTableStyle` file-scope `constexpr` stylesheet fragments. | "Spot List tab content (F3): replaces the F1 placeholder stub with the merged 8-column QTableView bound to BandFilterProxy(SpotTableModel). Column widths, table stylesheet, double-click handler, and bottom-row spot count + Clear button preserved verbatim from upstream. Three NereusSDR divergences: (1) band filters become 12 checkable QPushButton pills instead of 11 QCheckBoxes (adds 2m); (2) new source-filter pill row drives BandFilterProxy::setSourceVisible (NereusSDR-native); (3) all seven ingest clients feed spots into the table (upstream only fed Cluster). AppSettings keys: SpotBandFilter_<band> verbatim; SpotSourceFilter_<label> added (native)." |
| `src/models/BandFilterProxy.h` (F3 extension) | n/a (NereusSDR-native extension) | Adds `setSourceVisible(source, visible)` / `isSourceVisible(source)` public API mirroring the band filter, plus the private `QSet<QString> m_hiddenSources`. AetherSDR upstream's `BandFilterProxy` filters bands only (`DxClusterDialog.h:62-75 [@0cd4559]`); source filtering is NereusSDR-specific because the F3 Spot List tab adds a source-filter pill row that upstream does not have. | "F3 extension: new `setSourceVisible` / `isSourceVisible` API mirrors band filter. NereusSDR-native, no AetherSDR equivalent (upstream BandFilterProxy filters bands only)." |
| `src/models/BandFilterProxy.cpp` (F3 extension) | n/a (NereusSDR-native extension) | Implements `setSourceVisible` (toggles `m_hiddenSources` QSet membership + calls `invalidateFilter`), updates `filterAcceptsRow` to apply both band and source predicates with AND semantics. Empty band / source strings always show (matches upstream convention for unknown band values). Source column lookup uses `SpotTableModel::ColSource` DisplayRole. | "F3 extension: `setSourceVisible` toggles m_hiddenSources + invalidateFilter; filterAcceptsRow applies band AND source predicates with AND semantics. Empty source always shows. NereusSDR-native, no AetherSDR equivalent." |

Companion test file `tests/tst_spothub_dialog_smoke.cpp` extension
(not listed in Bucket A, same precedent as F1 / F2). Six new tests
pinning the F3 contract: `spotListTabHasTableView` (table + Clear
button + count label findable by objectName),
`spotListTabHasTwelveBandPills` (12 band pills 160m..2m findable by
`spotListBandPill_<band>`), `spotListTabHasSevenSourcePills` (7
source pills DX/RBN/JT/COL/POT/FDR/PSK findable by
`spotListSourcePill_<label>`), `spotListBandPillTogglesProxyFilter`
(toggling the 20m pill toggles `BandFilterProxy::isBandVisible("20m")`),
`spotListSourcePillTogglesProxyFilter` (toggling the DX pill toggles
`isSourceVisible("Cluster")`), and
`doubleClickOnSpotRowEmitsTuneRequested` (adds a DxSpot at 14.025
MHz, double-clicks the proxy row, and asserts `tuneRequested(14.025)`
emits once via QSignalSpy). All 21 tests pass (3 F1 + 12 F2 + 6 F3).

### Phase 3J-2 Task F4 - SpotHubDialog Display tab

Folds AetherSDR's standalone `src/gui/SpotSettingsDialog.{h,cpp}`
into the Display tab of `SpotHubDialog` (the upstream standalone
dialog is retired; the F4 Display tab is the single consolidated
control surface). Two-column layout: LEFT column carries 8 live
stat blocks (Total Spots / Unique Callsigns / Active Sources /
cty.dat entries / ADIF QSOs / DXCC entities / New DXCC in feed /
New bands in feed) plus a red "Clear All Spots" button at the
bottom; RIGHT column ports every knob from upstream
`SpotSettingsDialog.cpp:38-270 [@0cd4559]` (Spots toggle +
Memories toggle + Levels slider + Position slider + Font Size
slider + Spot Lifetime slider + Override Colors toggle and
swatch + Override Background two toggles and swatch + Background
Opacity slider). Each knob change writes to the same AppSettings
key the upstream standalone dialog used (`IsSpotsEnabled`,
`SpotsMaxLevel`, `SpotsStartingHeightPercentage`, `SpotFontSize`,
`DxClusterSpotLifetimeSec`, `IsSpotsOverrideColorsEnabled`,
`IsSpotsOverrideBackgroundColorsEnabled`,
`IsSpotsOverrideToAutoBackgroundColorEnabled`,
`SpotsOverrideColor`, `SpotsOverrideBgColor`,
`SpotsBackgroundOpacity`, `IsMemorySpotsEnabled`) and emits
`settingsChanged()` so MainWindow can refresh the live spectrum
spot overlay (matches upstream `:50-55, :289` live-preview
contract). The red "Clear All Spots" button calls
`SpotModel::clear()` + `SpotTableModel::clear()` and emits
`spotsClearedAll()` for MainWindow to propagate to the spectrum
overlay (upstream had a `Clear All Spots` button at
`SpotSettingsDialog.cpp:281-292 [@0cd4559]` that sent
`spot clear` over the SmartSDR command channel; NereusSDR
substitutes the in-process clear + signal emission since it does
not use the SmartSDR wire). `GuardedSlider` widget reused from
`src/gui/widgets/GuardedSlider.h` (already ported from upstream
`src/gui/GuardedSlider.h [@0cd4559]`).

| NereusSDR file | AetherSDR counterpart | Evidence | Specific mod-history wording |
|---|---|---|---|
| `src/gui/SpotHubDialog.h` (F4 extension) | `src/gui/SpotSettingsDialog.h:23-52 [@0cd4559]` | Adds eight private `QLabel*` member pointers (`m_statTotalSpots`, `m_statUniqueCallsigns`, `m_statActiveSources`, `m_statCtyDatEntries`, `m_statAdifQsos`, `m_statDxccEntities`, `m_statNewDxcc`, `m_statNewBands`) so the Display tab can refresh stat blocks on table-model row changes and DxccColorProvider import finishes. The knob widgets are NOT held as members (created and forgotten inside `buildDisplayTab`); upstream held them as members for the constant `setTotalSpots(int)` public setter, but that surface is replaced by the NereusSDR live-refresh wiring. NereusSDR divergence: upstream had a single `QLabel* m_totalSpotsLabel` (line 43); NereusSDR has eight stat labels (Total / Unique / Active Sources / cty.dat / ADIF / Entities / NewDxcc / NewBands). | "Display tab content (F4): eight new private `QLabel*` member pointers for the stat-block refresh path. NereusSDR-native: upstream `SpotSettingsDialog.h` held only one `m_totalSpotsLabel`; the Display tab grows that surface to eight live counts read from SpotTableModel + DxccColorProvider." |
| `src/gui/SpotHubDialog.cpp` (F4 extension) | `src/gui/SpotSettingsDialog.cpp:38-292 [@0cd4559]` | Replaces the F1 placeholder stub for `buildDisplayTab` with the two-column Display tab. LEFT column: 8 stat blocks built via the `makeStatRow` helper, each with a fixed `objectName()` and a member-pointer assignment. Red "Clear All Spots" button at the bottom emits `spotsClearedAll()` and resets the count labels to 0. RIGHT column: every knob from upstream `SpotSettingsDialog.cpp:38-270 [@0cd4559]` ported verbatim: the load-persisted block at `:21-37` (with the lifetime-key migration `:34-37`); the `save` lambda contract `:50-55`; the green/red `kToggleStyle` `:63-65`; Spots toggle `:57-71`; Memories toggle `:74-89`; Levels slider `:91-106`; Position slider `:108-123`; Font Size slider `:125-140`; non-linear lifetime step table `:146-178` (10..55s in 5s, 5..55min in 5min, 1..24hr in 1hr, 45 indices); Override Colors toggle + swatch `:180-210`; Override Background two toggles + swatch `:212-252`; Background Opacity slider `:254-270`. AppSettings keys preserved verbatim. NereusSDR divergence: (1) eight stat blocks replace upstream's single Total Spots label at `:272-276`; (2) the Clear All Spots button at `:281-292` substitutes `SpotModel::clear()` + `SpotTableModel::clear()` for upstream's `m_model->sendCommand("spot clear")` (NereusSDR is in-process; no SmartSDR wire); (3) GuardedSlider is reused from the existing `src/gui/widgets/GuardedSlider.h` port. Live-refresh wiring hooks the SpotTableModel's `rowsInserted` / `rowsRemoved` / `modelReset` signals + the DxccColorProvider's `importFinished` signal to a `refreshStats` lambda that walks the table model for unique callsigns / New DXCC / New bands counts and polls the seven ingest clients for the Active Sources count. cty.dat entry count uses `DxccColorProvider::entityCount()` (the worked-status entity count, which is the closest publicly exposed proxy for cty.dat rows); if a dedicated cty.dat row count is needed later the integrator can grow a `ctyEntityCount()` accessor. | "Display tab content (F4): two-column layout. LEFT column has eight stat blocks driven by SpotTableModel + DxccColorProvider (NereusSDR-native, replaces upstream's single Total Spots label) plus a red Clear All Spots button that calls `SpotModel::clear` + `SpotTableModel::clear` + emits `spotsClearedAll()`. RIGHT column ports every knob from upstream SpotSettingsDialog.cpp:38-270 verbatim with the same AppSettings keys + `settingsChanged()` emission contract + non-linear lifetime step table + GuardedSlider widget. The standalone upstream SpotSettingsDialog is retired in favour of this folded Display tab." |

Companion test file `tests/tst_spothub_dialog_smoke.cpp` extension
(not listed in Bucket A, same precedent as F1 / F2 / F3). Seven
new tests pinning the F4 contract: `displayTabHasStatBlocks` (all
8 stat labels findable by objectName), `displayTabHasLevelsSlider`
(Levels / Position / Font Size / BG Opacity sliders findable +
range check), `displayTabHasLifetimeSlider` (lifetime slider
findable + range matches the 45-step non-linear table),
`displayTabHasOverrideColorButton` (Spots / Memories / Override
Colors / Override BG / Auto toggle buttons + both color swatches
findable), `displayTabHasClearAllSpotsButton` (the red Clear All
Spots button is present and labeled "Clear"),
`clearAllButtonEmitsSignal` (clicking it emits
`spotsClearedAll()` once via QSignalSpy), and
`knobChangeEmitsSettingsChanged` (changing the Levels slider
emits `settingsChanged()` via QSignalSpy). All 28 tests pass (3
F1 + 12 F2 + 6 F3 + 7 F4).

### Phase 3R Task I1 - RADE codec wrapper skeleton

NereusSDR's RADE (Radio Autoencoder) v1 codec wrapper is a hybrid
port. Class layout, Q_OBJECT shape, signal/slot surface, member
ownership pattern, and out-of-line destructor placement (so the
forward-declared `std::unique_ptr<Resampler>` / `<RadeText>` members
can resolve their destructors) follow AetherSDR's `RADEEngine`. The
DSP API surface that Tasks I2 / I3 / I4 will plug in (rade_open call
shape, LPCNet feature extractor lifecycle, FARGAN vocoder warm-up,
embedded rade_text aux channel) follows freedv-gui's `RADEReceiveStep`
and `RADETransmitStep` pipeline classes; that lineage is recorded
separately in `docs/attribution/FREEDV-GUI-PROVENANCE.md`. The
NereusSDR file's header carries the verbatim freedv-gui
BSD-2-Clause-style header per the freedv-gui PROVENANCE rule + the
AetherSDR project-level attribution per HOW-TO-PORT.md rule 6
(AetherSDR has no per-file copyright headers; the project URL and
primary author are referenced at NereusSDR-block level instead of
copying a verbatim block that does not exist upstream).

| NereusSDR file | AetherSDR counterpart | Evidence | Specific mod-history wording |
|---|---|---|---|
| `src/core/RadeChannel.h` | `src/core/RADEEngine.h` [@0cd4559] | Class layout (Q_OBJECT subclass, `explicit RadeChannel(QObject* parent)` ctor, public `start` / `stop` / `isActive` / `isSynced` lifecycle, three slots `processIq` / `txEncode` / `resetTx`, signals `rxSpeechReady` / `txModemReady` / `syncChanged` / `snrChanged` / `freqOffsetChanged`), opaque-pointer ownership of the rade / LPCNet / FARGAN handles (`struct rade*`, `LPCNetEncState*`, `void* m_fargan`), member-name set (`m_rade`, `m_lpcnetEnc`, `m_fargan`, `m_farganWarmedUp`, `m_synced`, the four `std::unique_ptr<Resampler>` members `m_down24to8` / `m_up8to24` / `m_down24to16` / `m_up16to24`, and the four `QByteArray` accumulators `m_txAccum` / `m_txFeatAccum` / `m_rxAccum` / `m_rxFeatAccum`) match AetherSDR's `RADEEngine.h` line-for-line. NereusSDR-architectural additions: `start()` takes a model-path argument (OpenHPSDR requires runtime model selection rather than AetherSDR's hard-coded `"dummy"` first arg to `rade_open`); the `processIq` slot accepts I/Q from the receiver instead of `feedRxAudio` accepting DAX audio; `txEncode` accepts 16 kHz mono mic samples; new signal `rxTextDecoded(callsign, grid)` exposes the embedded rade_text aux channel that AetherSDR does not surface; new `m_active` flag separates "wrapper started" from "rade handle non-null" so the lifecycle pins (`!isActive()` after construction, `isActive()` after `start()`, `!isActive()` after `stop()`) hold even before I2 wires in the real RADE handle. | "Structural template (class layout, Q_OBJECT shape, signal/slot surface, member ownership, out-of-line dtor placement so forward-declared unique_ptr types resolve) ported from AetherSDR `src/core/RADEEngine.{h,cpp}` [@0cd4559]. DSP API surface that I2/I3/I4 will plug in is freedv-gui's `RADEReceiveStep` / `RADETransmitStep`; see `docs/attribution/FREEDV-GUI-PROVENANCE.md`. NereusSDR divergences vs AetherSDR: `start()` takes a model-path argument (OpenHPSDR runtime model selection vs AetherSDR's hard-coded `"dummy"`); slot surface re-shaped for receiver I/Q + mic-bus mono input; `rxTextDecoded` signal added for the I4 embedded text channel; `m_active` flag separates wrapper lifecycle from rade-handle existence so the I1 skeleton tests pin a deterministic contract even before I2 wires in the real RADE handle." |
| `src/core/RadeChannel.cpp` | `src/core/RADEEngine.cpp` [@0cd4559] | Ctor / dtor pair (default constructor, dtor calls `stop()` to unwind RADE handles on destruction without an explicit `stop()`) follows `RADEEngine.cpp:18-25`. `start()` skeleton mirrors `RADEEngine.cpp:27-78` (idempotent guard on the active flag, early return on bad input, success path flips the active flag and returns true; the real rade_open / lpcnet_encoder_create / fargan_init / resampler construction lands at I2). `stop()` skeleton mirrors `RADEEngine.cpp:80-106` (idempotent guard on the active flag, the real destroy / close / finalize unwind lands at I2). `isActive()` / `isSynced()` accessors mirror `RADEEngine.cpp:108-124` verbatim. `processIq` / `txEncode` / `resetTx` slot bodies are TODO-marked for I2 / I3 with explicit cites to the AetherSDR + freedv-gui call sequences they will follow. NereusSDR divergences: `start()` validates the model-path file exists rather than ignoring it; the active-flag check inside the early-return guard replaces AetherSDR's `if (m_rade) return true` check (the rade handle is null in I1 because the wrapper has no I2 DSP body to allocate it). | "Skeleton port. Lifecycle bodies (ctor/dtor pair, `start()` path-exists check + active-flag flip, `stop()` active-flag unflip + state-vars clear, accessors) ported from AetherSDR `src/core/RADEEngine.cpp:18-124` [@0cd4559]. DSP-bearing slot bodies (`processIq` / `txEncode` / `resetTx`) are TODO-marked for Phase 3R Tasks I2 / I3 with inline cites to the AetherSDR `feedRxAudio` / `feedTxAudio` / `resetTx` bodies and the freedv-gui `RADEReceiveStep::execute` / `RADETransmitStep::execute` / `RADETransmitStep::reset` they will follow. NereusSDR divergences vs AetherSDR: `start()` validates the model-path file exists rather than ignoring the argument the way AetherSDR's hard-coded `\"dummy\"` does; the active-flag guard replaces AetherSDR's `if (m_rade) return true` check because the rade handle stays null until I2." |

Two NereusSDR-native helper headers also land in I1 to keep
`std::unique_ptr<Resampler>` / `<RadeText>` destruction well-defined
in `RadeChannel.cpp`:

- `src/core/Resampler.h` - forward stub for the resampler class
  (full implementation lands at Phase 3R Task I2/I3; will wrap
  r8brain or the freedv-gui equivalent and mirror AetherSDR
  `src/core/Resampler.{h,cpp}` [@0cd4559]).
- `src/core/RadeText.h` - forward stub for the embedded text channel
  wrapper (full implementation lands at Phase 3R Task I4; will wrap
  `third_party/rade/src/rade_text.c` [@77e793a] and follow freedv-gui
  `src/pipeline/rade_text.{c,h}`).

Both stubs are NereusSDR-native scaffolding (no upstream counterpart),
carry the `no-port-check:` opt-out marker in their first comment so
the new-ports detector does not flag them, and are deliberately
NOT listed in this Bucket A table - the table is for genuine
AetherSDR derivations, and these two files are pre-port placeholders
for code that will become AetherSDR derivations in I2/I3/I4. They
will be added to Bucket A in those tasks.

Companion test file `tests/tst_rade_channel.cpp` (not listed in the
Bucket A core table; matches the F1/F2/F3/F4 test-as-companion
precedent). Three tests pin the I1 lifecycle skeleton:

- `initialState`: a fresh `RadeChannel` reports `!isActive()` and
  `!isSynced()`.
- `startStop`: `start(<temp-file-path>)` returns `true`, flips
  `isActive()` true; `stop()` flips it back. Uses a `QTemporaryFile`
  fixture so the path-exists precondition holds without depending on
  a checked-in test fixture file.
- `modelLoadFailureDisablesChannel`: `start("/nonexistent/path.f32")`
  returns `false` without mutating `isActive()`.

I2 will extend this suite with the real RX-path tests (DSP body,
sync indicator, snrChanged emission); I3 with TX-path tests; I4
with embedded-text-channel tests.

### Phase 3R Task I2 - RadeChannel RX path

The I1 skeleton `start()` / `stop()` bodies and the empty `processIq`
slot are now filled in with the RX-path port from AetherSDR
`RADEEngine.cpp:27-78` (start), `:80-106` (stop), and `:200-303`
(`feedRxAudio` body) [@0cd4559]. Each block is cross-checked against
freedv-gui `RADEReceiveStep.cpp:175-310` [@77e793a]; the divergences
between the two upstreams (mono short + freq_shift_coh in freedv-gui
vs stereo + L+R-averaged in AetherSDR) and the NereusSDR-architectural
divergence (true I/Q complex baseband via two parallel resamplers)
are called out inline in the .cpp comments at each step.

| NereusSDR file | AetherSDR counterpart | Evidence | Specific mod-history wording |
|---|---|---|---|
| `src/core/RadeChannel.cpp` (I2 delta) | `src/core/RADEEngine.cpp:27-78, 80-106, 200-303` [@0cd4559] | The `start()` body now mirrors `RADEEngine.cpp:27-78` step-for-step: `rade_initialize`; `rade_open(model_path, RADE_USE_C_ENCODER \| RADE_USE_C_DECODER \| RADE_VERBOSE_0)`; cleanup-on-failure (`rade_finalize`); `lpcnet_encoder_create`; cleanup-on-failure (`rade_close` + `rade_finalize`); `new FARGANState`; `fargan_init`; resampler chain construction (`m_down24to8`, `m_up8to24`, `m_down24to16`, `m_up16to24` matching AetherSDR plus the NereusSDR-added `m_down24to8Q`); accumulator clear; `rade_n_features_in_out` / `rade_n_tx_out` / `rade_nin` log line. `stop()` mirrors `RADEEngine.cpp:80-106` in reverse: `lpcnet_encoder_destroy`; `delete static_cast<FARGANState*>`; `rade_close` + `rade_finalize`; resampler `reset()` calls; accumulator clears; m_active / m_synced / m_farganWarmedUp flag clears. `processIq()` mirrors the `feedRxAudio` body at `RADEEngine.cpp:200-303`: I/Q deinterleave (NereusSDR divergence: AetherSDR averages L+R), parallel I+Q downsample to 8 kHz, RADE_COMP assembly (NereusSDR divergence: imag=Q instead of imag=0), `rade_nin`-bounded drain loop calling `rade_rx`, `n_features_in_out`-bounded feature accumulator, FARGAN warmup with `float zeros[320]` + `float warmup_features[5 * NB_TOTAL_FEATURES]` on first frame, FARGAN synthesise per LPCNET_FRAME_SIZE chunk, 16 kHz mono -> 24 kHz stereo upsample, `rxSpeechReady` emit pacing (NereusSDR divergence: no silence-pad on the no-sync branch), `rade_sync`/`rade_snrdB_3k_est`/`rade_freq_offset` sample-and-emit. The test seam `radeRxCallCountForTest()` returns `m_radeRxCallCount`, incremented each time `rade_rx` runs. | "Phase 3R Task I2. RX path body lands. start() now calls rade_initialize + rade_open + lpcnet_encoder_create + fargan_init + builds the five-resampler chain (24<->8 with the second m_down24to8Q for the Q leg, 24<->16); ported from AetherSDR `src/core/RADEEngine.cpp:27-78` [@0cd4559]. stop() unwinds in reverse, ported from RADEEngine.cpp:80-106. processIq() ports the feedRxAudio body at RADEEngine.cpp:200-303 with the NereusSDR-architectural divergence noted in RadeChannel.h's mod-history: AetherSDR's processStereoToMono(L,R)+imag=0 path is replaced with parallel processing of the I leg through m_down24to8 and the Q leg through m_down24to8Q, because NereusSDR's input is already complex baseband from the OpenHPSDR DDC. Cross-checked against freedv-gui `RADEReceiveStep::execute` (src/pipeline/RADEReceiveStep.cpp:175-310 [@77e793a]); freedv-gui's freq_shift_coh step is not needed because NereusSDR's DDC delivers baseband directly." |

Companion `RadeChannel.h` delta: adds `m_down24to8Q` (the second
`std::unique_ptr<Resampler>` for the I/Q-parallel downsample),
`m_radeRxCallCount` (test seam counter), and the
`radeRxCallCountForTest()` public accessor. No AetherSDR-side
equivalent; NereusSDR-architectural additions tied to the
I/Q-baseband divergence and the I2 test suite.

Companion test file `tests/tst_rade_channel.cpp` (the I1 file is
extended in-place with 4 new RX-path tests): `startInitializesRade`
pins the `"dummy"` sentinel + idempotent start + nonexistent-path
rejection; `processIqEmitsSyncFalseOnNoise` feeds 8 chunks x 4096
samples of deterministic noise and verifies no sustained sync, no
spurious `syncChanged(true)`, no `rxSpeechReady` emission, and at
least one `rade_rx` invocation; `processIqAccumulatesAcrossMultipleChunks`
pins the accumulator threshold (no `rade_rx` calls until the
`rade_nin`-bounded buffer fills); `stopReleasesResources` exercises
multiple start/stop cycles and a destructor-only teardown.

A synthetic in-test fixture (deterministic noise via `std::mt19937`
seed `0xC0DEC0DE`) is used because a real RADE I/Q capture requires
the I3 TX path to encode known speech (blocked at I2 time). The
fixture-generation rationale lives at `tests/fixtures/rade/README.md`.
The synced + decoded-speech contract is bench-verified at the Phase 3R
N2 matrix.

### Phase 3R Task I2a - Resampler port (r8brain wrapper)

The 17-line I1 stub `src/core/Resampler.h` is replaced with a
byte-for-byte port of AetherSDR's `Resampler.{h,cpp}` [@0cd4559].
The class wraps `r8b::CDSPResampler24` (from the vendored
`third_party/r8brain/` MIT library, SHA `5c44beb`) and exposes
mono / stereo convenience helpers (`process`, `processStereoToMono`,
`processMonoToStereo`, `processStereoToStereo`) over `float`-format
`QByteArray` buffers. AetherSDR has no per-file copyright header,
so per HOW-TO-PORT.md rule 6 the NereusSDR header carries a
project-level attribution block instead of copying a verbatim header
that does not exist upstream.

| NereusSDR file | AetherSDR counterpart | Evidence | Specific mod-history wording |
|---|---|---|---|
| `src/core/Resampler.h` | `src/core/Resampler.h` [@0cd4559] | Byte-for-byte port aside from namespace rename `AetherSDR` -> `NereusSDR`. Class shape (ctor accepts `srcRate` / `dstRate` / `maxBlockSamples=4096`; four public methods `process` / `processStereoToMono` / `processMonoToStereo` / `processStereoToStereo`; two public accessors `srcRate()` / `dstRate()`), member set (`m_srcRate` / `m_dstRate` / `std::unique_ptr<r8b::CDSPResampler24> m_resampler` / `std::vector<double> m_inBuf`), the `r8b::CDSPResampler24` forward declaration, and the thread-safety note all match AetherSDR line-for-line. | "Phase 3R Task I2a. Full port of AetherSDR `src/core/Resampler.{h,cpp}` [@0cd4559]. Replaces the 17-line I1 stub. Namespace renamed AetherSDR -> NereusSDR; otherwise byte-for-byte." |
| `src/core/Resampler.cpp` | `src/core/Resampler.cpp` [@0cd4559] | Byte-for-byte port aside from namespace rename. Each public method body carries an inline `// From AetherSDR src/core/Resampler.cpp:N-M [@0cd4559]` cite at the function head: ctor (`:7-13`), dtor (`:15`), `process` (`:17-38`), `processStereoToMono` (`:40-61`), `processMonoToStereo` (`:63-87`), `processStereoToStereo` (`:89-111`). The float32 <-> double conversion loops, the 0.5 stereo-downmix factor, the `Qt::Uninitialized` allocation, and the `int outLen <= 0 \|\| !outPtr` failure-path return are reproduced verbatim. | "Phase 3R Task I2a. Full port of AetherSDR `src/core/Resampler.cpp` [@0cd4559]. Namespace renamed AetherSDR -> NereusSDR; the r8brain header is now resolved against `third_party/r8brain/` (added in the same commit)." |

Companion test file `tests/tst_resampler.cpp` (not listed in the
Bucket A core table; matches the test-as-companion precedent).
Five tests pin the I2a Resampler port:

- `identity24kTo24k`: a 1:1 rate wrapper feeding 96000 mono samples
  produces ~96000 output samples within 5% (r8brain's polyphase
  FIR has a small startup latency that washes out at 4-second
  block sizes).
- `downsample24kTo8k`: a 3:1 ratio feeding 96000 input samples
  produces ~32000 output samples within 10% (the FIR-kernel
  startup latency at this ratio is ~2400 samples at the output
  rate, about 7-8%).
- `monoToStereoDoubles`: `processMonoToStereo` produces exactly
  `2 * monoFloats` floats for the same input fed through
  `process` (the stereo helper duplicates each output sample to
  L+R).
- `stereoToMonoHalves`: `processStereoToMono` produces the same
  sample count as a `process()` run with mono-equivalent input
  (the helper downmixes stereo to mono *before* resampling).
- `stereoToStereoRoundTrips`: `processStereoToStereo` produces
  exactly `2 *` the float count of `processStereoToMono` for
  the same input (the helper downmixes, resamples, and
  duplicates).

The companion file `third_party/r8brain/` carries its own MIT
license at `third_party/r8brain/LICENSE.txt` and a `VERSION.txt`
pinning the upstream URL, SHA, and vendoring date. A separate
provenance registry at `docs/attribution/R8BRAIN-PROVENANCE.md`
mirrors the FREEDV-GUI / RNNOISE / DESKHPSDR pattern.

### Phase 3R Task I3 - RadeChannel TX path

The I1/I2 skeleton + RX-only `txEncode` no-op is replaced with the
TX-path port from AetherSDR `RADEEngine.cpp:134-198` (the
`feedTxAudio` body) [@0cd4559]. The companion `resetTx` body at
AetherSDR `RADEEngine.cpp:126-132` lands at the same time. Each
block is cross-checked against freedv-gui `RADETransmitStep.cpp`
:216-247 (the `restartVocoder` EOO-queue and `reset()` FIFO-flush
paths) [@77e793a]; the divergence between AetherSDR's QByteArray
accumulator model and freedv-gui's pre-allocated FIFO + worker
thread model is called out inline in the .cpp comments at each
step, as is the one NereusSDR-architectural divergence: input is
already 16 kHz mono int16 from the WdspEngine TX pump per the plan,
so AetherSDR's 24 kHz stereo float -> 16 kHz mono int16 conversion
at `:139-152` is dropped and the input bytes append straight into
`m_txAccum`. The LPCNet feature extractor, the 12-frame feature
accumulator drained at `rade_n_features_in_out`-byte boundaries,
the `rade_tx` call, the RADE_COMP real-leg take, and the
8 kHz mono -> 24 kHz stereo float32 upsample via `m_up8to24` all
follow AetherSDR line-for-line.

| NereusSDR file | AetherSDR counterpart | Evidence | Specific mod-history wording |
|---|---|---|---|
| `src/core/RadeChannel.cpp` (I3 delta) | `src/core/RADEEngine.cpp:126-132, 134-198` [@0cd4559] | The `txEncode()` body now mirrors `feedTxAudio` at `RADEEngine.cpp:134-198`: input append into `m_txAccum` (NereusSDR divergence: AetherSDR's 24 kHz stereo float -> 16 kHz mono int16 conversion at :139-152 is dropped because the input is already in the LPCNet-ready format); LPCNET_FRAME_SIZE drain loop calling `lpcnet_compute_single_frame_features(m_lpcnetEnc, samples, features, 0)` with the `const_cast<int16_t*>(samples)` cast mirrored byte-for-byte; NB_TOTAL_FEATURES feature accumulator; `rade_n_features_in_out`-bounded drain loop calling `rade_tx(m_rade, tx_out.data(), reinterpret_cast<float*>(m_txFeatAccum.data()))`; RADE_COMP `.real`-component take into an `n_tx_out`-byte float32 mono buffer; `m_up8to24->processMonoToStereo` upsample to 24 kHz stereo float32; `emit txModemReady(stereo24k)` unconditional (the empty-buffer-during-warm-up case is upstream behaviour). `resetTx()` mirrors `RADEEngine.cpp:126-132`: clear `m_txAccum`, clear `m_txFeatAccum`, additionally zero `m_radeTxCallCount` (NereusSDR-added test-seam counter). The test seam `radeTxCallCountForTest()` returns `m_radeTxCallCount`, incremented each time `rade_tx` runs. The test seam `txFeatureAccumSizeForTest()` returns `m_txFeatAccum.size()`. | "Phase 3R Task I3. TX path body lands. txEncode() ports the feedTxAudio body at AetherSDR `src/core/RADEEngine.cpp:134-198` [@0cd4559] with one NereusSDR-architectural divergence: the input is already 16 kHz mono int16 (the WdspEngine TX pump feeds mic samples at that rate per the plan), so AetherSDR's 24 kHz stereo float -> 16 kHz mono int16 conversion at :139-152 is dropped and the input bytes append straight into m_txAccum. The LPCNet feature extraction (lpcnet_compute_single_frame_features over LPCNET_FRAME_SIZE chunks), the NB_TOTAL_FEATURES feature accumulator, the rade_n_features_in_out-bounded drain to rade_tx, the RADE_COMP real-leg take, and the 8 kHz mono -> 24 kHz stereo upsample via m_up8to24 all follow AetherSDR line-for-line. resetTx() flushes m_txAccum + m_txFeatAccum + m_radeTxCallCount per AetherSDR :126-132 [@0cd4559] cross-checked against freedv-gui `RADETransmitStep::reset` (:242-247 [@77e793a])." |

Companion `RadeChannel.h` delta: adds `m_radeTxCallCount` (test
seam counter) and the public accessors `radeTxCallCountForTest()`
and `txFeatureAccumSizeForTest()`. No AetherSDR-side equivalent;
NereusSDR-architectural additions tied to the I3 test suite.

Companion test file `tests/tst_rade_channel.cpp` (the I1/I2 file
is extended in-place with 4 new TX-path tests):
`txEncodeAcceptsAndAccumulates` pins that 8 chunks x 2000 samples
of synthetic 16 kHz mono speech drives at least one `rade_tx`
call; `txEncodeEmitsModemSamples` pins that at least one emitted
`txModemReady` chunk carries a non-empty payload (r8brain's
upsampler has nontrivial startup latency so the very first emit
may be empty - upstream behaviour at AetherSDR
`RADEEngine.cpp:189-192` [@0cd4559] is "emit unconditionally");
`resetTxClearsAccumulators` pins that `resetTx()` flushes the TX
feature accumulator and the speech accumulator (verified via the
`txFeatureAccumSizeForTest` seam plus the "post-reset partial
chunk does not bridge to a full LPCNet frame" pin);
`txWhileInactiveIsNoOp` pins that pre-start and post-stop
`txEncode` calls are no-ops (no signals, no crash).

A synthetic in-test fixture (`makeSyntheticSpeech16k`: 1 second
of a Hanning-windowed 300 Hz sine at 16 kHz mono int16) drives
the TX path tests; the on-disk mirror at
`tests/fixtures/rade/tx_test_speech.bin` documents the input
bytewise for external pipeline replay. The fixture-generation
rationale lives at `tests/fixtures/rade/README.md`. The "encode
then decode round-trip" contract is bench-verified at the Phase
3R N2 matrix because it requires both ends of the codec working
end-to-end on real radio hardware.

### Phase 3R Task I4 - RadeText (callsign-over-EOO wrapper)

The Phase 3R plan originally called for I4 to port `freedv-gui`
`src/pipeline/rade_text.{h,c}` verbatim and wrap it with a Qt6
shell, mirroring how I1/I2/I3 ported `RADEReceiveStep` /
`RADETransmitStep`. The review that opened I4 surfaced two
problems with that path:

1. **freedv-gui's `rade_text.c` is not self-contained.** It
   transitively pulls in roughly 1500 lines of codec2
   dependencies (`gp_interleaver.{h,c}`, `ldpc_codes.{h,c}` +
   12 LDPC tables, `mpdecode_core.{h,c}`, `ofdm_internal.h` and
   transitive headers, `ulog.h`) that are not currently in the
   NereusSDR tree. Pulling them in would expand the third-party
   surface meaningfully and require a separate vendoring +
   provenance pass.
2. **The actual call surface is the EOO (End-Of-Over) soft-bit
   channel, not a free-form feature stream.** freedv-gui's
   `RADEReceiveStep.cpp:225` calls
   `rade_text_rx(textPtr_, eooOut_, rade_n_eoo_bits(dv_) / 2)`
   and `freedv_interface.cpp:709` calls
   `rade_text_generate_tx_string(...)` followed by
   `rade_tx_set_eoo_bits(rade, eooSyms)`. Both sides sit on the
   EOO channel, not the feature stream the plan's narrative
   implied.

Given that the already-vendored `third_party/rade` library
exposes a working callsign-over-EOO channel via
`rade_tx_set_eoo_callsign` /
`rade_rx_get_eoo_callsign` (declared at
`third_party/rade/src/rade_api.h:120-145` [@b289102] and
implemented at `third_party/rade/src/rade_api_nopy.c:159-201`
[@b289102]) with no extra dependencies, NereusSDR uses that
surface instead of porting freedv-gui's `rade_text.c`.

Consequently `src/core/RadeText.{h,cpp}` is **NereusSDR-native**:
no AetherSDR derivation (AetherSDR does not surface the EOO text
channel; the `rxTextDecoded` signal on AetherSDR's `RADEEngine`
is a stub) and **no freedv-gui derivation** (the underlying RADE
C library is what does the encode / decode work, and the wrapper
on top is original NereusSDR code). The file is therefore NOT a
Bucket A entry here and NOT a row in
`docs/attribution/FREEDV-GUI-PROVENANCE.md`. The underlying
RADE library is BSD-2-Clause and is already attributed via
`third_party/rade/LICENSE` and the `third_party/rade/` vendor
block in the root `CMakeLists.txt`.

Trade-offs vs the freedv-gui `rade_text.c` path the plan
originally targeted:

- **No LDPC FEC** on the callsign bits. The RADE EOO channel is
  bare 7-bit ASCII; the freedv-gui channel runs an LDPC code
  over the bits, which is more robust at low SNR.
- **No CRC** on the message; per-message integrity is a future
  enhancement.
- **No grid square.** The RADE EOO channel exposes at most
  `RADE_EOO_CALLSIGN_MAX = 8` characters via the convenience
  API. The full 180-bit EOO frame has room for more, but a
  grid-square channel would need a separate format-design pass
  over the leftover bits and is deferred to a future task.

**Important deviation from the I1 prose at lines 681-687 above:**
the I1 forward stub prose anticipated that I4 would add
`src/core/RadeText.h` (and a sibling `.cpp`) to **this Bucket A
table**. That expectation no longer holds: the I4 review (above)
concluded the realistic implementation is a NereusSDR-native
wrapper around `third_party/rade`'s native API, not a port of
freedv-gui's `rade_text.c` (or any AetherSDR surface). The two
files are therefore deliberately **NOT listed in the Bucket A
table** for any task. They have no AetherSDR counterpart and
no upstream-license posture beyond the BSD-2-Clause of the
RADE library they call.

Files added in this task (NereusSDR-native; no Bucket A row):

- `src/core/RadeText.h`: public API surface (`setOurCallsign`
  / `ourCallsign` / `pushTxCallsign(struct rade*)` /
  `processRxEooBits(const float*, int)` / `textDecoded(QString)`
  signal). NereusSDR-native shape designed around the
  third_party/rade callsign-over-EOO API. The class owns no
  rade state; callers pass the active `struct rade*` from
  `RadeChannel` when encoding, and pass raw EOO soft-decision
  bits when decoding.
- `src/core/RadeText.cpp`: two short forwarders to
  `rade_tx_set_eoo_callsign` (with upper-case + truncation to
  `RADE_EOO_CALLSIGN_MAX`) and `rade_rx_get_eoo_callsign` (with
  empty-string filter on the decoded value). Inline cites
  point at the third_party/rade API at `rade_api.h:120-145`
  [@b289102] and `rade_api_nopy.c:159-201` [@b289102].

The companion `RadeChannel.h` is unchanged: `m_textChannel`
remains a forward-declared `std::unique_ptr<RadeText>` member
whose destructor is well-defined now that `RadeText` is a
real class. Wire-up into `RadeChannel::processIq` (the
`has_eoo_out` branch of `rade_rx`) and
`RadeChannel::txEncode` (the MOX-on branch that calls
`rade_tx_set_eoo_callsign` followed by `rade_tx_eoo`) is
deferred to **Phase L** per the plan.

Companion test file `tests/tst_rade_text.cpp` pins five
contracts: `constructsAndIsEmpty`,
`setOurCallsignStoresValue`, `pushTxNullRadeIsNoOp`,
`pushTxEmptyCallsignIsNoOp`,
`roundTripDecodesEncodedCallsign`. The headline test does a
bit-level round-trip (no OFDM mod/demod) because the wrapper
only sits on `set_eoo_callsign` /
`get_eoo_callsign`; the full OFDM round-trip is exercised
separately by the upstream
`third_party/rade/src/rade_callsign_test.c` executable.

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

---

## Phase 3P-II PGXL/TGXL Accessories

| NereusSDR file | AetherSDR counterpart | Port notes | Mod-history wording |
|---|---|---|---|
| `src/core/PgxlConnection.h` | `src/core/PgxlConnection.h` [@0cd4559] | Direct port: namespace AetherSDR -> NereusSDR; `onError` signature changed from `onError(QAbstractSocket::SocketError)` to `onError()` (no-arg, uses `m_socket.errorString()` internally) to match the `connectionFailed(QString)` signal that NereusSDR adds; `connectionFailed(QString)` signal added (AetherSDR only emits `connected`/`disconnected`/`statusUpdated`); `connectToPgxl` and `sendCommand` promoted to `public slots` for QML/signal wiring convenience; `injectLineForTesting` test-seam added in the same commit as the .cpp port (Tasks 4+5). | "PGXL amp TCP telemetry ported from AetherSDR `src/core/PgxlConnection.{h,cpp}` [@0cd4559]. Namespace AetherSDR -> NereusSDR. onError no-arg (was socket-error-code param). connectionFailed(QString) signal added. processLine() stubbed; V/R/S frame parsing lands in Tasks 6+7." |
| `src/core/PgxlConnection.cpp` | `src/core/PgxlConnection.cpp` [@0cd4559] | Direct port: namespace + logging-category renamed (`lcTuner` -> `lcPgxl`); `onError` body uses `m_socket.errorString()` + emits `connectionFailed`; `processLine()` stubbed (V/R/S frame parsing deferred to Tasks 6+7 per plan); `pollStatus()` preserves upstream `if (m_connected)` guard; debug-log prefix strings updated to remove "PgxlConnection:" redundancy. | "Same as .h." |
| `src/core/TgxlConnection.h` | `src/core/TgxlConnection.h` [@0cd4559] | Direct port: namespace AetherSDR -> NereusSDR; `onError` signature changed from `onError(QAbstractSocket::SocketError)` to `onError()` (no-arg, uses `m_socket.errorString()` internally); `connectionFailed(QString)` signal added (AetherSDR does not have this); `connectToTgxl`, `disconnect`, `adjustRelay`, `sendCommand` promoted to `public slots`; `injectLineForTesting` test-seam added under `public:` (not `public slots:`) per PgxlConnection precedent; `stateUpdated(QMap<QString,QString>)` and `statusUpdated(QMap<QString,QString>)` signals preserved verbatim. | "TGXL tuner TCP telemetry ported from AetherSDR `src/core/TgxlConnection.{h,cpp}` [@0cd4559]. Namespace AetherSDR -> NereusSDR. onError no-arg. connectionFailed(QString) signal added. Full V/R/S frame parsing with state/status object discrimination landed in same commit as Tasks 8+9." |
| `src/core/TgxlConnection.cpp` | `src/core/TgxlConnection.cpp` [@0cd4559] | Direct port: namespace + logging-category renamed (`lcTuner` -> `lcTgxl`); `onError` body uses `m_socket.errorString()` + emits `connectionFailed`; full `processLine()` ported including V-frame version handshake, R-frame KV body extraction emitting `statusUpdated`, and S-frame object discriminator (`state` -> `stateUpdated`, `status` -> `statusUpdated`) with strict `if (lastSpaceBeforeEq < 0) return;` per design §6.1; `m_pollTimer.setInterval(1000)` (1 Hz, matches upstream); `adjustRelay` clamp (relay 0-2, move +/-1) matches upstream verbatim; `pollStatus` guard `if (m_connected)` preserved. | "Same as .h." |
| `src/models/TunerModel.h` | `src/models/TunerModel.h` [@0cd4559] | Port with NereusSDR divergences: `bindConnection(TgxlConnection*)` replaces `setDirectConnection` (no SmartSDR handle routing in NereusSDR); `isPresent()` driven by `m_present` bool (set when `model`/`serial_num` keys appear in `applyStatus`) vs upstream handle-based `!m_handle.isEmpty()`; `commandReady(QString)` signal dropped (commands forward directly via `m_conn->sendCommand()`); `directConnectionChanged()` has no bool arg (upstream has `bool`); `relayChanged()` signal added (plan addition over upstream); `fwd`/`swr` parsed in `applyStatus` as raw floats (upstream parses these only in `stateUpdated`/`statusUpdated` lambdas inside `setDirectConnection`). `setHandle()` slot dropped (SmartSDR-only mechanism). | "TGXL tuner state model ported from AetherSDR `src/models/TunerModel.{h,cpp}` [@0cd4559]. Namespace AetherSDR -> NereusSDR. bindConnection replaces setDirectConnection. isPresent is m_present bool (model/serial_num trigger). commandReady dropped. directConnectionChanged no-arg. relayChanged() added. fwd/swr in applyStatus." |
| `src/models/TunerModel.cpp` | `src/models/TunerModel.cpp` [@0cd4559] | Full port of applyStatus KV loop (13 keys: serial_num/model/operate/bypass/tuning/relayC1/relayC2/relayL/antA/one_by_three/ip/fwd/swr); bindConnection wires TgxlConnection::connected/disconnected/stateUpdated/statusUpdated with full stateUpdated/statusUpdated lambda bodies (dBm->watts/return-loss->SWR conversion preserved verbatim from upstream); hasDirectConnection() checks `m_conn && m_conn->isConnected()`; slot bodies (setOperate/setBypass/autoTune/setAntennaA/adjustRelay) guard on `m_conn->isConnected()` instead of `m_handle.isEmpty()`; logging category `lcTunerModel`/`nereus.tuner.model`. | "Same as .h." |
| `src/gui/RelayBar.h` | `src/gui/HGauge.h` (inner class) [@0cd4559] | Extraction of AetherSDR inner `class RelayBar : public QWidget` (lines 199-288 in HGauge.h); promoted to standalone NereusSDR file. Constructor body (lines 206-212), setValue (214-218), setScrollEnabled (220-223), wheelEvent (229-242), paintEvent (244-281) all ported verbatim with namespace AetherSDR -> NereusSDR; colors mapped to StyleConstants (kPanelBg, kTextPrimary, kAccent); angleAccum accumulator logic (KDE/Cinnamon 960-per-notch clamping) preserved exactly. | "Extracted RelayBar widget from AetherSDR src/gui/HGauge.h (inner class) into standalone NereusSDR widget. Supports mousewheel step adjustment. Style colors mapped to NereusSDR StyleConstants." |
| `src/gui/RelayBar.cpp` | `src/gui/HGauge.h` (inner class) [@0cd4559] | Same as .h. | "Same as .h." |
| `src/gui/applets/AmpApplet.h` | `src/gui/AmpApplet.h` [@0cd4559] | Direct port: namespace AetherSDR -> NereusSDR; base class QWidget -> AppletWidget (adds title bar + helpers); constructor adds RadioModel* param; public methods promoted to public slots; appletId/appletTitle/syncFromModel pure-virtual overrides added; HGauge uses NereusSDR setter API (setRange/setYellowStart/setRedStart/setTitle/setUnit/setTickLabels) instead of upstream 7-arg positional constructor. | "PGXL amp telemetry/control applet ported from AetherSDR src/gui/AmpApplet.{h,cpp} [@0cd4559]. AppletWidget base, RadioModel* ctor, NereusSDR HGauge setter API." |
| `src/gui/applets/AmpApplet.cpp` | `src/gui/AmpApplet.cpp` [@0cd4559] | Same as .h. Full UI layout preserved: 3 HGauge bars (FwdPwr 0-2000W, SWR 1-3, Temp 0-100C), stacked power/meff labels, OPERATE/STANDBY toggle button with green/blue state styling. | "Same as .h." |
| `src/gui/SMeterWidget.h` | `src/gui/SMeterWidget.h` [@0cd4559] | Direct port: namespace AetherSDR -> NereusSDR; RxMode expanded to 4 entries (SMeter/SignalAverage/SMeterPeak/MaxBin) per design doc ss5.4.1; contextMenuEvent declared, stub body (Task 38 deferred); test accessors testPowerScaleMax/testPowerRedStart added. Phase 3P-II Phase 2 Tasks 34/35/36. | "Analog S-Meter gauge ported from AetherSDR src/gui/SMeterWidget.{h,cpp} [@0cd4559]. Namespace AetherSDR -> NereusSDR. RxMode 4-entry (SignalAverage + MaxBin added). contextMenuEvent stubbed (Task 38). Test accessors added. Peak hold config + arc rendering verbatim." |
| `src/gui/SMeterWidget.cpp` | `src/gui/SMeterWidget.cpp` [@0cd4559] | Same as .h. Full constructor + setPowerScale + paintEvent arc/scale/tick/needle/label rendering. Peak hold config slots. contextMenuEvent stub. setRxMode extended to dispatch SignalAverage and MaxBin. | "Same as .h." |

## Phase 3F Sub-Epic D - Pan layout skeletons

Added 2026-05-27. AetherSDR-derived widgets land the pan-layout foundation
for Phase 3F multi-panadapter. `PanadapterApplet` is the per-pan container that
hosts a `SpectrumWidget` and tracks associated slices for overlay rendering
(AetherSDR overlay model: a pan picks one DDC for FFT, any slice whose freq
falls within visible range overlays as a flag). `PanadapterStack` (Task 3
below) is the layout manager skeleton that constructs with one `pan-0` in a
vertical `QSplitter` (the default "1" template); `applyLayout` /
`floatPanadapter` / `rebuildSplitters` are stubbed for Tasks 4-8 of the
Sub-Epic D plan. Files carry a `no-port-check:` head marker (HOW-TO-PORT.md
rule 6 form - AetherSDR has no per-file GPL header to copy verbatim) naming
the upstream source.

| NereusSDR file | AetherSDR counterpart | Port notes | Mod-history wording |
|---|---|---|---|
| `src/gui/PanadapterApplet.h` | `src/gui/PanadapterApplet.{h,cpp}` [@0cd4559] | Structural port: namespace AetherSDR -> NereusSDR; per-pan container hosting one `SpectrumWidget` (default-construct, no extra params); slice association API (`addSlice` / `removeSlice` / `associatedSlices`); active-slice promotion semantics (first added slice auto-promotes, remove of active promotes from `QSet<int>::begin()`, remove of last sets to -1); display state `centerMhz` / `bandwidthMhz` with persistence deferred to AppSettings wiring in later tasks. Phase 3F Sub-Epic D Task 1. | "Per-pan container ported structurally from AetherSDR `src/gui/PanadapterApplet.{h,cpp}` [@0cd4559]. Hosts one `SpectrumWidget` + tracks associated slices for overlay rendering. NereusSDR preserves the existing single-output-device + per-slice pan from its own audio model. Phase 3F Sub-Epic D Task 1." |
| `src/gui/PanadapterApplet.cpp` | `src/gui/PanadapterApplet.{h,cpp}` [@0cd4559] | Same as `.h`. Implementation body: `QVBoxLayout` with zero margin/spacing wrapping the `SpectrumWidget`; `setActiveSliceIndex` idempotency guard + signal emission; `removeSlice` promotes another slice from the set when removing the active one. | "Same as `.h`." |
| `src/gui/PanadapterStack.h` | `src/gui/PanadapterStack.{h,cpp}` [@0cd4559] | Structural port: namespace AetherSDR -> NereusSDR; pan layout manager skeleton (5 templates planned: "1" / "2v" / "2h" / "12h" / "2x2"; only "1" wired in this task); `addPanadapter` / `removePanadapter` / `panadapter` / `allApplets` / `count` / `currentLayoutId` / `activePanId` / `setActivePan` live; `applyLayout` / `floatPanadapter` / `removeAll` / `rebuildSplitters` / `clearSplitters` stubbed for Tasks 4-8. Forward-declares `PanFloatingWindow` (lands in Task 7). Phase 3F Sub-Epic D Task 3. | "Pan layout manager skeleton ported structurally from AetherSDR `src/gui/PanadapterStack.{h,cpp}` [@0cd4559]. Default 'Single' layout constructed in ctor; 4 remaining templates land in Tasks 4-6. Phase 3F Sub-Epic D Task 3." |
| `src/gui/PanadapterStack.cpp` | `src/gui/PanadapterStack.{h,cpp}` [@0cd4559] | Same as `.h`. Implementation body: `QVBoxLayout` wrapping vertical `QSplitter`; ctor auto-adds `pan-0` to seed the default `"1"` layout; `addPanadapter` idempotency on duplicate pan-id; `removePanadapter` uses `deleteLater()` for safe child cleanup; `setActivePan` change-only signal emission. | "Same as `.h`." |

## 3D Stacked-Trace Spectrum Plan

Added 2026-08-08. Task 1 of the 3D stacked-trace spectrum plan (design doc
`docs/architecture/2026-08-08-3d-stacked-trace-spectrum-design.md`, plan
`docs/architecture/2026-08-08-3d-stacked-trace-spectrum-plan.md`) lifts the
DSS perspective projection math verbatim from AetherSDR's `DssRenderer.h`,
with the three shape constants promoted to a runtime `DssShape` so a 3D
Angle control can drive them later in the plan.

| NereusSDR file | AetherSDR counterpart | Port notes | Mod-history wording |
|---|---|---|---|
| `src/gui/DssGeometry.h` | `src/gui/DssRenderer.h` | Verbatim lift of the perspective constants (`:33-64`) and projection functions (`:84-187`) at `[@1872028c]`: depthScale, projectPerspective, projectSurface, rowSpanFactorFor, rowSpanFactorForOverhang, wedgeFreeDepth, rowScreenCoverage, rowFrequencyUnit, maxRowSpanFactor. Excludes `:66-82` (`kMeshCols` + its `static_assert`), which Task 3 owns under a different design. Two marked NereusSDR deviations (`//-KG4VCF [v0.5.3]`): the three shape constants are promoted to a runtime `DssShape` for the 3D Angle control, and the transition-row reserve is documented as always one row here. | "3DSS perspective geometry constants and projection functions ported verbatim from AetherSDR `src/gui/DssRenderer.h`; the shape constants are parameterised at runtime for the NereusSDR-original 3D Angle control." |
| `src/gui/DssMeshGeometry.h` | `src/gui/SpectrumWidget.cpp` + `src/gui/DssRenderer.h` | Verbatim comment lift (mechanically renamed) at `[@1872028c]` from three upstream sites: `SpectrumWidget.cpp:148-172` (`dssFillVerticesPerRow`/`dssLineVerticesPerRow`/`appendDssVertex`, incl. the "33.7 MiB at kMeshCols (1280) columns" size note), `SpectrumWidget.cpp:13136-13200` (the mesh-build loop: row/layer/column iteration, fill and ribbon vertex tag values, back-to-front row order), and `DssRenderer.h:66-82` (the `kMeshCols` sizing formula and its `static_assert`, claimed here per Task 1's row, which explicitly excluded it). Renames: `DssRenderer::kMeshCols` (compile-time member) -> `dssMeshColsFor(shape)` (runtime function) / `meshCols` (parameter); `kCols` -> `kDssCols`; the `static_assert` -> runtime `dssMeshDensityHolds`, re-checked after every angle change rather than only at construction. `dssMeshBytesFor` is a NereusSDR-original convenience wrapper (upstream computes VBO byte sizes inline at its two buffer-creation call sites, not via a named function). The six-vertex visitation order within each triangle pair is reordered from upstream (both windings draw the same two triangles per quad); confirmed cosmetic, not behavioural, because neither `m_dssMeshFillPipeline` nor `m_dssMeshLinePipeline` sets a QRhi cull mode upstream. | "GPU mesh vertex generation ported from AetherSDR `src/gui/SpectrumWidget.cpp:148-172,13136-13200` and `src/gui/DssRenderer.h:66-82`; the mesh column count is a function of the runtime DssShape instead of a compile-time constant, so the widest-angle mesh is only paid for when the shape demands it (design doc §5.5)." |
| `src/gui/DssRenderer.h` | `src/gui/DssRenderer.h` | Task 4: the class declaration for the row ring store subset only (`[@1872028c]`), excluding the QImage CPU-cache/palette rendering path (`image()`/`rebuild()`/`m_cache*`, deferred to Task 10) and the retained scrollback store (`m_historyRows` + friends, `setHistoryCapacityRows`/`appendHistoryRow`/`rebuildVisibleFromHistory`/`reprojectFrequencyFrame`, deferred to a later task if reached). Member comments preserved verbatim where the underlying concept is unchanged (`m_rows`, `m_rowCoverage`, the "Data-model accessors" section intro, the `m_head`/`m_count`/`m_generation`/`m_rowGeneration` trailing comments, the `m_rawPrev1`/`m_rawPrev2` comment, the one-shot skip-flag comment, and the `m_rowCenterMhz` frame-stamp comment, all restored after an initial pass silently dropped them). Upstream's `supplemental` concept is renamed `wide` throughout per the plan's explicit divergence (off-screen bins of the SAME FFT at the SAME calibration, not a separately-calibrated native waterfall tile), so the `m_rowSupplemental`/`m_wideRawPrev1` member comments and the `newestSupplementalBandwidthMhz` doc comment are rewritten rather than lifted verbatim, and upstream's `DssSupplementalCoverage.h` is not ported. NereusSDR-original: a private `ringAtAge(int)` helper consolidates the four upstream `*AtAge` accessors' individual `if (age < 0 \|\| age >= m_count) return 0.0;` guards into one clamped ring-index lookup; `clear()` restores upstream's full `m_rowCenterMhz`/`m_rowBandwidthMhz`/`m_rowWideCenterMhz`/`m_rowWideBandwidthMhz`/`m_rowWideCoverage` reset (an initial pass dropped it, which combined with the clamp-based `ringAtAge` would have leaked stale frequency stamps past a `clear()`); `newestWideBandwidthMhz` restores upstream's `newestSupplementalBandwidthMhz` non-finite/non-positive `targetBandwidthMhz` guard and the `isfinite(candidate)` check, both present in upstream and in the sibling `dssRowSpanFactorFor` (Task 1) but missing from an initial pass. | "3DSS row ring store declaration ported from AetherSDR `src/gui/DssRenderer.h` [@1872028c], ring-store subset only (CPU QImage cache and scrollback history deferred). supplemental renamed wide (off-screen same-FFT bins, not a separately-calibrated tile)." |
| `src/gui/DssRenderer.cpp` | `src/gui/DssRenderer.cpp` | Task 4: `median3` (`:28-30`... precisely `:30-33`), `frequencyFramesMatch` (`:49-61`... precisely `:50-60`), `resampledRawRow` (`:182-215`... precisely `:183-216`) and `smoothDssRow` (`:217-252`... precisely `:218-250`) lifted verbatim at `[@1872028c]`, substituting `DssRenderer::kCols` -> `kDssCols`; none of the four carry internal `//` comments upstream so no comment loss is possible there. `pushRow`/`pushRowWithSupplemental` (`:389-475`) ported to `pushRow`/`pushRowWithWide` verbatim including the one inline comment ("Adjacent bin indices no longer represent the same frequencies..."). `resetInputSmoothing()` (`:254-267`) ported with its explanatory comment restored (an initial pass dropped it); the comment's `pushRow/appendHistoryRow` upstream identifier pair is reduced to `pushRow` (with a matching singular verb) because `appendHistoryRow` is not ported in this task, and the `m_skipHistoryTemporalBlendOnce`/`resetHistorySmoothing()` lines are dropped for the same reason. `clear()` (`:269-284`) keeps the brief's given flag-reset shape but restores upstream's four frequency-stamp-array `.fill(0.0)` calls plus the `m_rowWideCoverage` reset loop (an initial pass omitted them; see the `.h` row for why that matters given the `ringAtAge` clamp). `newestSupplementalBandwidthMhz` (`:323-337`) -> `newestWideBandwidthMhz` restores upstream's leading guard clause and `isfinite(candidate)` check dropped by an initial pass. `rowAt(int)` (`:316-321`) is not ported; the four upstream `*AtAge` accessor bodies (`:721-751`, each with its own `if (age < 0 \|\| age >= m_count) return 0.0;` guard) are replaced by the shared `ringAtAge` helper (NereusSDR-original, ordering/signature given by the task brief) plus direct ring indexing. | "3DSS row ring store ported from AetherSDR `src/gui/DssRenderer.cpp` [@1872028c]: peak-preserving downsample, median-of-3 impulse rejection, 1-2-1 spatial blur, temporal IIR blend, and the ring write, all verbatim. supplemental renamed wide throughout." |
| `src/gui/SpectrumWidget.h` (Task 6 extension) | `src/gui/SpectrumWidget.h:78-83, 442-443, 516-534, 1473, 1477, 1481, 1484, 1493, 1764` [@1872028c] | Task 6 (render mode + row tee). `SpectrumRenderMode` (`:78-83`) is a byte-for-byte match including the `Mode2D`/`Mode3D`/`Count` inline comments, confirmed by diff against the pinned SHA. Public accessor NAMES/signatures match upstream 1:1 -- `setSpectrumRenderMode`/`spectrumRenderMode`, `setDssFloorDepth`/`dssFloorDepth`, `setDssGain`/`dssGain`, `setDssRowSpan`/`dssRowSpan` (`:516-534`), `setThreeDSliceDepth`/`threeDSliceDepth` (`:442-443`) -- but the bodies (see `.cpp` row) are NereusSDR-original single-source simplifications of a materially larger upstream surface, so the signatures are ported while the semantics are not claimed as verbatim. `DssRenderer m_dss` (`:1477`) matches upstream's field name and type but drops its GUI-thread-affinity comment ("pushRow() ... and the renderGpuFrame/paint reads all run on the GUI thread, so m_dss needs no lock"); the same constraint holds in NereusSDR (`pushWaterfallRow()` always runs on the main thread via the WaterfallTicker's queued connection) but the comment itself was not carried over. `dssFloorDepth()`'s storage representation diverges: upstream keeps a negative offset `float m_dssFloorOffsetDb{-6.0f}` (`:1481`) and derives the shown value via `std::lround(-m_dssFloorOffsetDb)`; NereusSDR stores the shown value directly as `int m_dssFloorDepth{6}`, dropping the per-source (`m_flexDssFloorDepth`/`m_kiwiDssFloorDepth`) split entirely -- one FFT-derived source per the Task 4 divergence, so there is nothing to split. `dssRowSpan`'s backing field is renamed `m_dssRowSpanPct` (`:1493`) -> `m_dssRowSpan`; default 100 preserved but upstream's explanatory comment on why 100 (not the safer-looking 0) is the shipped default is not reproduced. `m_dssGain{70}` (`:1484`) matches upstream's name and default verbatim. NereusSDR-original, no upstream counterpart at this SHA (confirmed absent by grep for "Angle" across the whole file): `setDssAngle`/`dssAngle`/`dssShape()` and the backing `m_dssAngle` -- upstream renders at one fixed viewing angle (Task 1's DssGeometry.h divergence note). Also NereusSDR-original: the three `*ForTest()` seams (`pushWaterfallRowForTest`, `setTxActiveForTest`, `dssRowsPushedForTest`), following this file's established convention (33 prior `*ForTest()` accessors predate this task; see Phase 3J-2 Task E1 above for the precedent), and `m_dssMeshNeedsResize` (scaffolding consumed by Task 7). `#include "gui/DssRenderer.h"` added alongside the existing `spectrum/*.h` includes. | "SpectrumRenderMode enum and the 3DSS control accessor names/signatures ported from AetherSDR `src/gui/SpectrumWidget.h` [@1872028c]; setter bodies are NereusSDR-original single-source simplifications of upstream's multi-source (Kiwi/Flex/per-profile) implementation, detailed in the `.cpp` row. The 3D Angle control has no upstream counterpart -- AetherSDR renders at one fixed angle." |
| `src/gui/SpectrumWidget.cpp` (Task 6 extension) | `src/gui/SpectrumWidget.cpp:4129-4156, 4652-4711` [@1872028c] | Same as `.h` above for what is and is not ported. `setThreeDSliceDepth` (`:4129-4156`): upstream walks every `QApplication::topLevelWidgets()` so floating pans agree live and calls a `DisplaySettings` facade; NereusSDR is single-pan today (no sibling windows, no `DisplaySettings` facade) so the port is state-plumbing only (`scheduleSettingsSave()` + `update()`) pending a later multi-pan task -- the flag itself is wired but nothing yet reads it to render (Task 12, "slice shadow decals", is expected to consume it). `setDssFloorDepth` (`:4652-4654`) drops upstream's `setDssFloorDepthForSource`/`restoreDssFloorDepthForCurrentSource` indirection (`:4599-4650`, per-source dispatch + a `dssFloorDepthResolved` signal) entirely; NereusSDR clamps and stores directly. `setDssGain`/`setDssRowSpan` (`:4656-4680`) keep upstream's `std::clamp` shape but drop the `AETHER_GPU_SPECTRUM` LUT re-bake token (`m_dssLutToken = ~0ull`), deferred to Task 8 ("Palette LUT decoupled from waterfall knobs"). `setSpectrumRenderMode` (`:4682-4711`) drops `commitFrequencyPreview()` (no frequency-preview concept in NereusSDR), the inline `AppSettings::setValue`/`save()` pair (uses `scheduleSettingsSave()` instead, this file's existing convention), and the three-way retained-scrollback-capacity toggle across `m_dss`/`m_nativeWaterfallState.dss`/`m_kiwiWaterfallState.dss` (NereusSDR has one `m_dss`, no retained scrollback yet); keeps the shape of "invalidate + markOverlayDirty on every real mode change" and adds the NereusSDR-original leaving-3D ring clear (`m_dss.clear()` + `m_dssRowsPushed = 0`) so re-entering 3D does not show a stack captured at an abandoned frequency, which upstream does not need because its retained-scrollback capacity toggle already discards the old data on mode exit. The row-tee itself, `pushDssRow()` called from inside `pushWaterfallRow()` immediately after the stop-on-TX early return, has no literal upstream call site to cite: upstream's equivalent, `pushDssLiveRow` / `dss.pushRowWithSupplemental` (`SpectrumWidget.cpp:5192-5216` [@1872028c]), is fed by a much larger frequency-preview-aware, multi-source (native/Kiwi/per-profile) pipeline with no TX-stop concept anywhere in the file (grepped absent at this SHA -- AetherSDR is a FlexRadio client with no local TX-blank feature in this widget). The tee's specific placement downstream of `m_wfStopOnTx` is therefore NereusSDR-original engineering that satisfies the plan's own lockstep requirement (design doc `2026-08-08-3d-stacked-trace-spectrum-design.md` section 4.1, "Row ingest"), not a port. The `m_wfStopOnTx` gate condition gains `\|\| m_txActiveForTest` (NereusSDR-original test seam, decoupled from `ActivePeakHoldTrace::txActive()` so the test does not need to drive the peak-hold subsystem to prove the gate). | "Setter bodies for the render-mode enum and 3DSS controls simplified from AetherSDR `src/gui/SpectrumWidget.cpp` [@1872028c] to NereusSDR's single-source, single-pan shape (no Kiwi/Flex split, no sibling-pan sync, no retained scrollback, no GPU LUT token -- deferred to later tasks in this plan). The stop-on-TX row-tee placement inside pushWaterfallRow() is NereusSDR-original: AetherSDR's equivalent row-feed path has no TX-stop concept to place it against. Leaving 3D clears the ring so re-entering does not show a stack captured at an abandoned frequency." |
| `src/gui/SpectrumWidget.h` (Task 7 extension) | `src/gui/SpectrumWidget.cpp:12786-12920, 14834-14848` [@1872028c] | Task 7 (GPU resources and mesh pass). Adds the "3DSS mesh GPU resources" block to the existing `NEREUS_GPU_SPECTRUM` private section: `initDssMeshPipeline()` (returns `bool`, upstream's returns `void`, per the graceful-degradation requirement that a failed init must be observable to the caller without inspecting `m_dssMeshReady` directly), `rebuildDssMeshIfNeeded`, `uploadDssHeightRows`, `uploadDssPaletteLut`, `writeDssMeshUbo`, `dssMeshReady()`, and (as of the post-Step-6b-review fix below, which adds `m_dssMeshUploaded`) seventeen member variables -- corrected here from an original miscount of eleven, caught by direct line-by-line recount while re-verifying this row for the current fix. `m_dssHeightTex`/`m_dssPaletteTex`/`m_dssHeightSampler`/`m_dssPaletteSampler`/`m_dssMeshReady`/`m_dssLutToken` match upstream's own field names verbatim (confirmed against the `releaseResources()` teardown at `:14834-14848`). The remaining upstream fields are renamed by dropping the redundant "Mesh" infix that upstream needs (upstream ALSO keeps a separate, not-yet-ported `m_dssSrb`/`m_dssGpuTex` pair for its cached-image CPU-fallback quad, so its GPU-mesh fields carry a `Mesh` qualifier to stay distinct: `m_dssMeshFillPipeline`->`m_dssFillPipeline`, `m_dssMeshLinePipeline`->`m_dssLinePipeline`, `m_dssMeshSrb`->`m_dssSrb`, `m_dssMeshUbo`->`m_dssUbo`; `m_dssMeshVbo`/`m_dssMeshLineVbo` keep the qualifier verbatim because there is no competing non-mesh VBO). Since Task 7 does not yet build that cached-image fallback layer (design doc defers it to Task 10, "CPU fallback surface"), reusing the unqualified names now means Task 10 must pick different names for the fallback quad's own SRB/texture when it lands, rather than the `m_dssSrb`/`m_dssGpuTex` names upstream uses. `m_dssMeshHeadUploaded`/`m_dssMeshRowGenUploaded` (int/quint64) are renamed `m_dssLastUploadedHead`/`m_dssUploadedRowGeneration` (same types, same sentinels `-1`/`~0ull`) per the task brief's own naming, not upstream's. `m_dssMeshCols` (`int`) and the `uploadDssPaletteLut`/`writeDssMeshUbo` stub declarations are NereusSDR-original with no upstream counterpart: `m_dssMeshCols` exists only because Task 3's `dssMeshColsFor(shape)` makes the column count a runtime quantity (upstream's `DssRenderer::kMeshCols` is a compile-time constant, so upstream has no equivalent field). **Step 6b addition** (closing a gap flagged and confirmed after the first pass of this task, not part of the original brief): `DssOutlinePipelineMode m_dssOutlinePipelineMode{DssOutlinePipelineMode::DedicatedRibbonPipeline}` -- upstream's own field name verbatim (`SpectrumWidget.cpp:12786-12920` region, field declared alongside the other GPU-mesh members it teards down at `:14834-14848`); the `DssOutlinePipelineMode` type itself is not declared in this file but in `src/gui/DssMeshGeometry.h` (see that file's new row below, sourced from a different upstream file, `SpectrumPreviewLogic.h`). NereusSDR's default-member-initializer value (`DedicatedRibbonPipeline`) has no upstream counterpart to match against -- upstream's own field has no default initializer at its declaration, only the runtime assignment at the top of `initDssMeshPipeline()`; NereusSDR's default exists only to give the member a defined value between construction and that first `initDssMeshPipeline()` call, and is overwritten before anything reads it. `#include "gui/DssMeshGeometry.h"` added inside the `NEREUS_GPU_SPECTRUM` guard (this member's only consumer) rather than unconditionally alongside `DssRenderer.h`. | "3DSS GPU mesh resource declarations added to the existing `SpectrumWidget.h` `NEREUS_GPU_SPECTRUM` block, following AetherSDR `src/gui/SpectrumWidget.cpp` [@1872028c] field-for-field where upstream has a direct counterpart (height/palette textures and samplers, mesh-ready flag, LUT token verbatim); pipeline/SRB/UBO/upload-tracking fields drop upstream's redundant 'Mesh' infix because this task does not yet carry upstream's separate cached-image fallback fields of the same un-prefixed name. `m_dssMeshCols` is NereusSDR-original, required by the runtime-angle mesh sizing introduced in Task 3. Step 6b adds `m_dssOutlinePipelineMode` (upstream's field name verbatim), whose type is declared in the new `DssMeshGeometry.h` row, sourced from AetherSDR `src/gui/SpectrumPreviewLogic.h`." |
| `src/gui/SpectrumWidget.cpp` (Task 7 extension) | `src/gui/SpectrumWidget.cpp:12786-12920, 13304-13307, 14834-14848` [@1872028c] | Task 7. `initDssMeshPipeline()` follows upstream's structure end to end -- the `RGBA16F` support probe and its exact log wording including the upstream em dash (`:12793-12798`), shader load, buffer/texture/sampler/SRB creation each with upstream's own failure-guard-and-log shape, the vertex layout (`Float3`, stride 3), the `TargetBlend` (`SrcAlpha`/`OneMinusSrcAlpha`/`One`/`OneMinusSrcAlpha`), and the binding numbers (0 UBO both stages, 1 height vertex-stage, 2 palette fragment-stage, load-bearing against `dss_mesh.vert:63` and `dss_mesh.frag:47,50`). Two intentional exclusions from the cited range: (1) buffer sizing is `dssMeshColsFor(dssShape())` (Task 3) rather than upstream's compile-time `kMeshCols`, the task's headline divergence, reallocated only on an actual column-count change (see `rebuildDssMeshIfNeeded`); (2) **Step 6b** (added after the coordinator confirmed the gap flagged by the first pass of this task): `m_dssOutlinePipelineMode = dssOutlinePipelineModeForBackend(r->backend() == QRhi::OpenGLES2)` is set at the top of `initDssMeshPipeline()`, after the existing `if (!r) { return false; }` guard (upstream has no such guard and calls `r->backend()` unconditionally; NereusSDR's added null-check moves the assignment a few lines later than upstream's literal position so `r->backend()` is never called on a null `r`). `m_dssLinePipeline`'s creation -- not just its later `create()` call -- is skipped entirely when the mode is `SharedFillPipeline`, by wrapping `m_dssLinePipeline = makePipeline();` in `if (m_dssOutlinePipelineMode == DssOutlinePipelineMode::DedicatedRibbonPipeline)`; the combined `create()` failure check is updated to `!m_dssFillPipeline->create() \|\| (m_dssLinePipeline && !m_dssLinePipeline->create())` so a null (never-created) line pipeline cannot be mistaken for a creation failure. At the outline draw site inside `renderGpuFrame()`'s `is3D` branch, `cb->setGraphicsPipeline(m_dssLinePipeline)` becomes `cb->setGraphicsPipeline(dssOutlinePipelineFor(m_dssOutlinePipelineMode, m_dssFillPipeline, m_dssLinePipeline))` -- the `dssOutlinePipelineFor` selector (declared in `DssMeshGeometry.h`, see that file's new row) rather than a ternary at the call site, matching upstream's own reason for factoring it out (keeps the selection unit-testable without a QRhi device). `releaseResources()` needs no change: `delete m_dssLinePipeline` on a null pointer (the OpenGL case) is a no-op in C++, and upstream itself does not reset `m_dssOutlinePipelineMode` in its teardown either -- confirmed absent by grepping the pinned SHA's `releaseResources()` -- because it is a backend property, correctly re-derived from `r->backend()` at the next `initDssMeshPipeline()` regardless of teardown. Upstream's own comment records why any of this exists at all: "QRhi's OpenGLES2 backend, which covers desktop GL as well as GLES, reuses the fill program for ribbon outlines: live probes showed flat/stale outlines with a separate, identically configured program." (`SpectrumPreviewLogic.h:16-18 [@1872028c]`, moved to sit directly above `dssOutlinePipelineModeForBackend` in the port because the excluded `dssRowSpanSupported` no longer sits between them -- see the new `DssMeshGeometry.h` row). NereusSDR's `SpectrumWidget` constructor calls `setApi(QRhiWidget::Api::Metal)` on macOS and `setApi(QRhiWidget::Api::Direct3D11)` on Windows but has no `Q_OS_LINUX` branch, so a Linux build takes Qt's default `QRhiWidget` backend selection for that platform -- typically OpenGL absent an explicit override -- which is exactly the case this port makes correct. `rebuildDssMeshIfNeeded()` is NereusSDR-original: upstream has no equivalent function because it never resizes (fixed compile-time `kMeshCols`); this task's resize-only-on-change guard and its 57.8/33.8 MiB accounting are new. **Post-Step-6b-review fix**: the guard originally shipped keyed on `!m_dssMeshNeedsResize && wanted == m_dssMeshCols && m_dssMeshVbo->size() > 0` (consuming the `m_dssMeshNeedsResize` flag Task 6 introduced and left unread until this task). The coordinator's own review caught that this guard is satisfied on the very first call after `initDssMeshPipeline()` -- which sets `m_dssMeshCols` and creates a correctly SIZED but EMPTY buffer -- so `dssBuildMeshVertices`/`uploadStaticBuffer` never ran at all and the mesh would draw undefined GPU memory permanently. Fixed (still NereusSDR-original, no upstream counterpart to diverge from) by rekeying on a new member, `bool m_dssMeshUploaded{false}`, so the guard asks "is the geometry currently on the GPU correct" (`m_dssMeshUploaded && wanted == m_dssMeshCols`) rather than "did somebody tell me it changed". `m_dssMeshNeedsResize` became fully redundant and was removed from `SpectrumWidget.h`, along with its lone assignment site in `setDssAngle()`. `m_dssMeshUploaded` is reset to `false` in `releaseResources()` alongside the pointer nulling so a rebuilt pipeline uploads again rather than trusting a stale `true`. Verified empirically (not by inspection): a temporary log line in a locally-built, temporarily-3D-forced run showed `uploaded(before)=false` on the first call, `dssBuildMeshVertices` producing the exact expected float count (4,423,680 = 96 visible rows x 15360 vertices/row x 3 floats for a 1281-column mesh), then `SKIP` on every subsequent unchanged-angle call; both temporary changes were reverted before commit. `uploadDssHeightRows()` is the task brief's given implementation; its tracking fields correspond conceptually to upstream's `m_dssMeshHeadUploaded`/`m_dssMeshRowGenUploaded` (both present in the cited teardown range), but the upstream upload function's own body was not read for this task, so no line-range citation is claimed for its internals -- only the field-name correspondence is asserted. `uploadDssPaletteLut()` and `writeDssMeshUbo()` are declared per this task's interface contract and stubbed with an empty body plus a one-line comment naming the task that fills them in (Task 8 and Task 9 respectively); calling them from a not-yet-implemented state is intentional scaffolding, not a defect. The render-loop dispatch in `renderGpuFrame()` replaces the unconditional `if (m_fftFillPipeline && ...)` FFT draw block with `if (is3D && m_dss.rowCount() > 0) { <mesh> } else if (!is3D && m_fftFillPipeline && ...) { <verbatim original body, reindented not rewritten> }`; the lead comment quotes upstream verbatim at `:13304-13307` ("3DSS replaces only the spectrum trace: the surface fills specRect and the waterfall, divider, freq scale, and all overlays keep their normal 2D positions. Everything below is identical to 2D except the FFT trace is swapped for the 3DSS surface quad inside specRect."). NereusSDR's dispatch is structurally simpler than upstream's own draw-call site (`:14607-14650`, not cited as a source here beyond the comment) because NereusSDR does not yet have upstream's cached-image CPU-fallback quad branch (Task 10) or its row-frequency waterfall-pipeline-mode concept (not part of this plan); `is3D` here additionally requires `m_dssMeshReady`, which upstream's top-of-function `is3D` does not, because upstream's cached-image fallback branch handles the not-ready case where NereusSDR's `else if (!is3D ...)` currently would not draw anything for one frame until the next resource-update cycle catches up -- acceptable because `initDssMeshPipeline()` runs once at `initialize()` time, before the first paint. The three-plus-one resource-update calls (`rebuildDssMeshIfNeeded`/`uploadDssPaletteLut`/`uploadDssHeightRows`/`writeDssMeshUbo`) are queued into the existing `batch` immediately before the pre-existing `cb->resourceUpdate(batch)` call, gated on `m_spectrumRenderMode == SpectrumRenderMode::Mode3D` alone (not `m_dssMeshReady`, so a failed-init retry path stays available to a later task); `writeDssMeshUbo` needs a `dpr` value and the file's single canonical `dpr` (`outputSize.width() / w`) is not computed until after `cb->beginPass()`, so this call site computes its own local `dssDpr` from the same `renderTarget()->pixelSize()` formula rather than moving or duplicating the existing post-beginPass declaration -- the existing declaration and every line that reads it are untouched. `releaseResources()` gains an eleven-line teardown block mirroring upstream's `:14834-14848` (same field-name renames as the `.h` row), deleting every resource this task created and resetting `m_dssMeshReady`/`m_dssMeshCols`/`m_dssLutToken`/`m_dssUploadedRowGeneration`/`m_dssLastUploadedHead` so a device-loss re-`initialize()` starts clean. `initialize()` gains one line, `initDssMeshPipeline();`, after the existing `initSpectrumPipeline();` call. `#include "gui/DssMeshGeometry.h"` added for `dssMeshColsFor`/`dssFillVerticesPerRow`/`dssLineVerticesPerRow`/`dssBuildMeshVertices`/`kDssMeshUboFloats`, absent from the file before this task. The pre-existing 2D FFT draw block's body (fill pass / peak-hold pass / line pass, `:8644-8679` pre-task line numbers, confirmed against the `git diff` hunk header `@@ -8641,8 +8895,38 @@`) is reindented into the `else if` branch with no other change; diffed against the pre-task revision it is character-identical apart from the guard line. | "GPU pipeline/SRB/texture creation ported from AetherSDR `src/gui/SpectrumWidget.cpp:12786-12920` [@1872028c] field-for-field, including the RGBA16F format probe's graceful CPU-fallback path and its exact upstream log wording. Buffer sizing is the task's headline divergence: the runtime `DssShape` drives column count instead of a compile-time constant, reallocated only when the shape crosses a column boundary. Step 6b ports upstream's OpenGL-specific dedicated-vs-shared outline pipeline dispatch (`dssOutlinePipelineModeForBackend`/`dssOutlinePipelineFor`, declared in the new `DssMeshGeometry.h` row and sourced from AetherSDR `src/gui/SpectrumPreviewLogic.h`), closing the gap flagged and confirmed after this task's first pass -- NereusSDR's Linux build has no explicit QRhi backend override and defaults to OpenGL, where upstream's own comment records a real driver bug (flat/stale ribbon outlines from a separate, identically configured pipeline). The render-loop swap-in at `:13304-13307` quotes upstream's own comment verbatim and leaves the existing 2D FFT draw block's body untouched, reindented only." |
| `src/gui/DssMeshGeometry.h` (Task 7 Step 6b extension) | `src/gui/SpectrumPreviewLogic.h:11-56` [@1872028c] | New upstream source file in this project's provenance (not cited anywhere else in NereusSDR before this task). Added after the coordinator confirmed a gap flagged by the first pass of Task 7: this file's `initDssMeshPipeline()` always built two independent QRhi pipeline objects (fill + dedicated outline) regardless of backend, while upstream conditionally shares the fill pipeline on `QRhi::OpenGLES2` to work around a real driver bug ("live probes showed flat/stale outlines"); `SpectrumWidget`'s constructor sets an explicit QRhi backend on macOS/Windows only, so a Linux build (NereusSDR ships Linux AppImages for two architectures) takes Qt's OpenGL default and would have hit exactly that bug. Ports three of the four declarations at `:11-56`: the `DssOutlinePipelineMode` enum (`:11-14`, verbatim, two enumerators `DedicatedRibbonPipeline`/`SharedFillPipeline`), `dssOutlinePipelineModeForBackend` (`:33-39`, verbatim, plus its explanatory comment at `:16-18` reattached to sit directly above it -- see below), and `dssOutlinePipelineFor` (`:48-56`, verbatim including the em dashes in its trailing comment, with one substitution: upstream's comment names its own unit test binary, `spectrum_preview_logic_test`; NereusSDR has no file of that name, so the brief substitutes the generic "the unit test", used as given rather than naming `tst_dss_mesh_geometry.cpp` directly). Deliberately NOT ported: `dssRowSpanSupported` (`:19-31`, doc comment + function), upstream's declaration textually sandwiched between the outline-pipeline comment and `dssOutlinePipelineModeForBackend` with no blank line separating the two topics in the original file -- confirmed by reading the pinned SHA directly, not assumed from the brief's transcription. The brief's task-7-brief.md explicitly scopes this out ("that file also contains `dssRowSpanSupported`, which a later task needs; do NOT port that one now, it belongs with the control wiring"), so the 3-line outline-pipeline comment at `:16-18` is reattached directly above `dssOutlinePipelineModeForBackend` in this port (its logical target) rather than left orphaned above a function this task does not add. Namespace: upstream's `AetherSDR` becomes `NereusSDR`, matching this file's existing convention (all of `DssGeometry.h`/`DssMeshGeometry.h`/`DssRenderer.h` already live in `namespace NereusSDR`). Both new functions are `constexpr` (implicitly inline per the C++ standard, matching upstream, unlike this file's other `inline`-qualified non-constexpr helpers) and header-only/QRhi-free, matching upstream's own stated reason for the shape: the backend-selection logic stays unit-testable without a graphics device. Two new test functions added to `tests/tst_dss_mesh_geometry.cpp` (`outlinePipeline_sharesFillOnOpenGlOnly`, `outlinePipeline_selectsWithoutTouchingTheNullPipeline`); the second was proven to fail (not just reasoned about) by temporarily inverting the ternary in `dssOutlinePipelineFor` and confirming `outlinePipeline_selectsWithoutTouchingTheNullPipeline` goes red (`0x0` returned in place of the expected fill-pipeline address, mismatch caught by `QCOMPARE` without a crash -- proving the null-safety property empirically, not just by inspection), then reverting and reconfirming 13/13 green. | "Outline-pipeline backend-selection dispatch ported from AetherSDR `src/gui/SpectrumPreviewLogic.h:11-56` [@1872028c] -- the enum and both `constexpr` functions verbatim, including the driver-bug explanatory comment and the null-safety comment on the selector. `dssRowSpanSupported`, upstream's other declaration in the same cited range, is explicitly deferred to a later task per the brief and not ported here. One wording substitution: upstream's comment names its own unit test binary; substituted with a generic reference since no NereusSDR file shares that name." |
| `src/gui/SpectrumWidget.h` (Task 8 extension) | `src/gui/SpectrumWidget.cpp:11710-11719, 12727-12728` [@1872028c] | Task 8 (palette LUT decoupled from waterfall knobs). Adds the free function `interpolateWfGradient(float t, const WfGradientStop* stops, int count)` beside the existing `wfSchemeStops` declaration (`:292`) -- NereusSDR-original, no upstream counterpart claimed: it is an in-file extraction of `dbmToRgb()`'s own pre-existing inline stop-interpolation loop (itself Thetis-derived, `display.cs:6889-6891`), not a port of anything at this SHA (upstream's own equivalent, `waterfallLevelToRgb`, was read for context but is structurally different -- see the `.cpp` row -- and is not cited as a source for this function). Adds two public members beside the existing `dssShape()` accessor: `QRgb dssStrengthToRgb(float s) const` (upstream `:11710-11719`, verbatim gamma formula and comment, ported public rather than upstream's own access level so `tests/tst_dss_palette.cpp` can call it directly and so `uploadDssPaletteLut()` has a CPU-callable sibling) and `quint64 dssPaletteToken() const` (upstream `:12727-12728` -- `uploadDssPaletteLut()`'s own inline 2-field token there, promoted to a named method; see the `.cpp` row for why this is the correct upstream counterpart and not the differently-scoped 5-field member upstream separately names `dssPaletteToken()` at `:11721-11733`). | "`interpolateWfGradient` is a NereusSDR-original extraction of dbmToRgb()'s own pre-existing loop, not an AetherSDR port. `dssStrengthToRgb`/`dssPaletteToken` accessor declarations ported public from AetherSDR `src/gui/SpectrumWidget.cpp` [@1872028c] so the 3DSS palette LUT is directly testable and reusable outside the GPU upload path." |
| `src/gui/SpectrumWidget.cpp` (Task 8 extension) | `src/gui/SpectrumWidget.cpp:11710-11733, 12713-12741` [@1872028c] | Task 8. **`interpolateWfGradient`** (added beside `wfSchemeStops`'s definition, `:306` pre-task): a byte-for-byte behavioural extraction of the loop `dbmToRgb()` inlined before this task (own pre-existing NereusSDR code, Thetis-derived per the `// From Thetis display.cs:6889-6891` cite immediately above it in `dbmToRgb`, itself untouched by this task) -- `stopCount`/`t` renamed `count`/`f` to read as a general-purpose free function, the caller-side `qBound` clamp duplicated defensively inside the function (harmless: `dbmToRgb` already clamps `adjusted` before calling it, so the clamp is idempotent there, and it is what makes the function safe for `dssStrengthToRgb`'s own out-of-range gamma output). `dbmToRgb()` itself shrinks to a `return interpolateWfGradient(adjusted, stops, stopCount);` one-liner; everything above that line (the `m_wfActiveLowThreshold`/`m_wfActiveHighThreshold`/`m_wfBlackLevel`/`m_wfColorGain` effective-threshold computation, `// From Thetis display.cs:6889-6891`, the `wfSchemeStops` call) is untouched -- confirmed by `git diff` showing only the 15-line loop+return replaced by the single delegating line, nothing above it in the hunk. Proven behaviour-preserving empirically, not by inspection: a standalone scratch harness (copy-pasted the pre-task inline loop as `oldPath` and the post-task `interpolateWfGradient` as `newPath`, both compiled against real `QRgb`/`qRgb`/`qBound` from Qt headers, no NereusSDR object code involved) compared both across all 8 real gradient-stop tables (`kDefaultStops` through `kClarityBlueStops`) over a `t` sweep from -0.5 to 1.5 in 10,000ths plus every exact stop boundary and its `nextafterf` neighbours: 160,182 `(scheme, t)` pairs, 0 mismatches. **`dssStrengthToRgb`** (`:11710-11719` verbatim, including the two-line gamma comment word for word) is placed directly after `dbmToRgb()` in the existing "dBm to waterfall color" section, sharing `interpolateWfGradient`/`wfSchemeStops` with it but reading none of `dbmToRgb`'s effective-threshold fields. **`dssPaletteToken`**: upstream has two functions of this concept at this SHA, not one -- the separately-declared 5-field `SpectrumWidget::dssPaletteToken()` member (`:11721-11733`, comment preserved in full via direct quotation in this port's comment since the fields it folds are not ported: "Fold the inputs that define the 3DSS surface colour so the cached image recolours when any change. The surface now maps strength through the scheme + '3D Gain' (dssStrengthToRgb); the waterfall gain/black/min are kept here too since they still affect the 2D/waterfall colour path."), whose one call site is `buildDssImage()` (`:11880-11894`), the CPU-fallback cached-image path Task 4's row explicitly deferred to Task 10; and `uploadDssPaletteLut()`'s own separate inline 2-field token (`:12727-12728`, `static_cast<quint64>(m_wfColorScheme) * 131 + static_cast<quint64>(m_dssGain)`), which upstream computes directly rather than by calling its own `dssPaletteToken()` member -- confirmed by grep, upstream's `dssPaletteToken()` has exactly one caller and it is not `uploadDssPaletteLut`. NereusSDR's `dssPaletteToken()` ports the latter (the actual GPU-LUT-gating computation) under the same name upstream gives the former, since NereusSDR has not yet ported the CPU-fallback path that would need the 5-field version; the port comment records this precisely (both citations, both line ranges, the call-site evidence) so Task 10 does not silently shadow the name with different semantics when `buildDssImage()` is eventually ported. **`uploadDssPaletteLut`** (`:12713-12741`, replacing Task 7's stub): the 6-line design comment ("The 3D surface maps its stable colour aperture across the FULL colormap...") and the `// unchanged` / `// owns its data` inline comments are preserved verbatim; the `floorDbm`/`rangeDb` parameters and their `Q_UNUSED` lines are dropped because Task 7 already fixed this file's stub to the simpler single-argument signature (`dssStrengthToRgb` needs no per-frame dBm floor/range at all, unlike upstream's cached-image path) -- no comment is lost by the drop, upstream attaches no comment to either `Q_UNUSED` line. Empirical decoupling proof (not argued, run): `dssStrengthToRgb` was temporarily mutated twice. Mutation 1 (naive), `return dbmToRgb(s);` -- passes the raw 0..1 strength straight through as if it were a dBm value -- turned 3 of the 7 `tst_dss_palette` cases red (`gain50_isLinear`, `gain100_liftsTowardTheFloor`, `gain0_coloursOnlyTheStrongest`) but left `waterfallKnobs_doNotMove3DColours` green: `dbmToRgb`'s effective-high threshold tops out at -62 dBm for any reachable slider position, so any `s` in [0,1] saturates its `adjusted` clamp to 1.0 regardless of `m_wfColorGain`/`m_wfBlackLevel`, an unintended blind spot in the single-probe test as originally adapted from the brief (confirmed to persist even after widening to four probes -- 0.1/0.4/0.7/0.9 -- since the whole [0,1] domain saturates identically, not just the one brief-specified point). Mutation 2 (realistic), `return dbmToRgb(-140.0f + s * 140.0f);` -- maps the strength across a representative dBm span before the same hand-off, matching what an actual wrong port of `uploadDssPaletteLut` would plausibly do -- turned `waterfallKnobs_doNotMove3DColours` red too (`0xff000000` vs the un-mutated `0xfff5f5f5` at one probe), alongside the same 3 gamma cases. Reverting restored all 7/7 green both times. `tests/tst_dss_palette.cpp`'s `waterfallKnobs_doNotMove3DColours` was permanently strengthened from the brief's single 0.4 probe to four probes (0.1/0.4/0.7/0.9) as a result, closing the saturation blind spot for future regressions even though it did not change which of the two mutations this task's own verification needed to reach red. Gamma identities confirmed exact (not approximate) against the shipped `std::pow(4.0f, (50.0f - m_dssGain) / 50.0f)` expression: gain 50 -> 1.000000000, gain 100 -> 0.250000000, gain 0 -> 4.000000000, bit-exact float equality (all three are exact powers of 2, no rounding). | "dbmToRgb()'s inline gradient loop is extracted to a new free function, `interpolateWfGradient`, proven byte-identical to the pre-extraction loop across 160,182 sampled (scheme, t) pairs spanning all 8 real gradient tables. `dssStrengthToRgb`/`dssPaletteToken`/`uploadDssPaletteLut` ported from AetherSDR `src/gui/SpectrumWidget.cpp` [@1872028c]; the 3D surface maps a stable colour aperture across the full colormap, gamma-shaped only by '3D Gain', independent of the 2D waterfall's gain/black-level knobs, proven by two mutation runs (one revealing and then closing a test blind spot at saturation, one directly reproducing the decoupling failure and reverting it). `dssPaletteToken()` ports upstream's `uploadDssPaletteLut`-local 2-field token rather than the differently-scoped 5-field member of the same name, which belongs to the still-unported CPU-fallback path (Task 10)." |
| `src/gui/SpectrumWidget.h` (Task 9 extension) | `src/gui/SpectrumWidget.cpp:11735-11810, 11871-11879, 12743-12784, 14195` [@1872028c] | Task 9 (floor anchoring, wide channel feed, UBO writer). Adds five public accessors beside the existing 3DSS block. **Review-round correction**: an earlier pass of this row claimed `dssFloorDbm()`/`dssSpanDb()` were NereusSDR-original with "nothing to cite" -- false, caught by review. Upstream has functions of the SAME NAMES: `SpectrumWidget::dssFloorDbm()` at `src/gui/SpectrumWidget.cpp:11735-11810` [@1872028c] and `SpectrumWidget::dssSpanDb() const` at `:11871-11879` [@1872028c], both re-read directly to confirm. NereusSDR's bodies are restructured/simplified from these, not independent inventions -- see the `.cpp` row for exactly what each upstream function does and why NereusSDR does not port the machinery wholesale. Also adds `dssColorRangeDb()` (review-round addition, Finding 1: `writeDssMeshUbo`'s `colorRangeDb` field was written as the bare `kDssColorSpanDb` constant, uncapped by the actual dBm span -- see the `.cpp` row). `buildDssWideRow()` remains genuinely NereusSDR-original with no upstream analog of this shape -- design doc section 4.2 records why: upstream's wide-equivalent channel is fed from a second, independently-calibrated measurement (a native FLEX/Kiwi waterfall tile) that must be quantile-matched against the exact FFT before use, so upstream carries a `DssSupplementalCoverage.h` calibration layer NereusSDR has no counterpart for and does not port; NereusSDR has one measurement, so the whole problem collapses to windowing the same FFT frame's off-screen bins, a computation upstream never needed and therefore never wrote. `dssRowSpanTarget()`'s declaration is ported public (upstream: `src/gui/SpectrumWidget.cpp:12743-12784` -- see the `.cpp` row for the exact-vs-excluded content of that range). New test seams: `setMeasuredNoiseFloorForTest()` and `setFullBinsForTest()`, both NereusSDR-original following this file's established `*ForTest()` convention (see the Task 6 row above for the precedent); `setFullBinsForTest()` was added beyond the task brief's own listed seam set -- the brief's given `rowSpanTarget_scalesTheAvailableOverhang` test drives the production `pushDssRow()` -> `buildDssWideRow(m_lastFullBinsDbm, ...)` path, but nothing in the brief populates `m_lastFullBinsDbm` (production only fills it from `updateSpectrumLinear()`, an FFT-arrival callback the test never drives), so without this seam the wide channel never receives data and the test's `w.setDssRowSpan(100); QVERIFY(w.dssRowSpanTarget(0.096) > 1.0f);` assertion is unreachable regardless of implementation correctness -- confirmed by mutation (see the `.cpp`/test row below). New private members: `m_lastFullBinsDbm` (NereusSDR-original, full-DDC dBm cache feeding `buildDssWideRow`; see `.cpp` row for the conversion formula) and `m_dssScrollProgressRows`/`m_dssLastTickMs` (NereusSDR-original continuous scroll-progress accumulator; upstream drives the equivalent `scrollProgressRows` uniform from a `QElapsedTimer` restarted per push burst, `SpectrumWidget.cpp:6031-6064` [@1872028c] -- not ported verbatim because NereusSDR's producer always advances by exactly one row, so an accumulate-on-tick/reset-on-push pair on the already-existing `m_displayTimer` needs no new clock object; see the `.cpp` row for the tick-side logic). | "`dssFloorDbm`/`dssSpanDb` restructure AetherSDR functions of the same names (`SpectrumWidget.cpp:11735`/`:11871` [@1872028c]), reusing existing simpler NereusSDR state instead of porting their anchor/clamp machinery -- corrected from an earlier pass of this row that wrongly claimed nothing upstream to cite. `buildDssWideRow` remains genuinely NereusSDR-original: NereusSDR has one calibrated FFT measurement where AetherSDR arbitrates between an exact FFT and a separately-calibrated native waterfall tile, so the whole DssSupplementalCoverage.h calibration layer AetherSDR needs has no NereusSDR counterpart. `dssColorRangeDb` (review-round addition) ports upstream's `std::min(rangeDb, kColorSpanDb)` cap verbatim as a named, directly testable accessor. `dssRowSpanTarget` declaration ported public from AetherSDR `src/gui/SpectrumWidget.cpp` [@1872028c]. The scroll-progress accumulator is NereusSDR-original; AetherSDR drives the equivalent uniform from a QElapsedTimer restarted per push burst, not applicable here because this producer always advances by exactly one row." |
| `src/gui/SpectrumWidget.cpp` (Task 9 extension) | `src/gui/SpectrumWidget.cpp:11735-11810, 11871-11890, 12743-12784, 14148-14408, 14195, 6031-6064` [@1872028c] | Task 9. **`dssFloorDbm`** (`src/gui/SpectrumWidget.cpp:11735-11810` [@1872028c], review-round citation correction -- an earlier pass claimed NereusSDR-original with nothing to cite, which is false and was caught by review): upstream's version is a much larger Kiwi/Flex-branching function. Its Flex path (the branch comparable to NereusSDR, since NereusSDR has no Kiwi concept) maintains its own smoothed anchor state machine: `m_dssFloorAnchorDbm`/`m_dssFloorAnchorValid`, a `kAnchorAlpha = 0.04f` IIR blend toward a target floor drawn from `m_noiseFloorBaselineDbm`/`m_measuredNoiseFloorDbm`/an FFT-bin estimate (in that fallback order), an 18 dB hard re-anchor guard (`std::abs(targetFloor - m_dssFloorAnchorDbm) > 18.0f` skips the IIR blend and snaps straight to the new target instead of drifting slowly toward a large jump), and a final `std::round(floor * 2.0f) / 2.0f + m_dssFloorOffsetDb` (0.5 dB rounding step, then the floor-depth offset -- NereusSDR's `m_dssFloorDepth` subtraction is the direct analog of this last term only). NereusSDR's `dssFloorDbm()` reuses the already-existing, independently Thetis-sourced `m_nfLerpAverage` (display.cs:4628 lerp average, itself already a smoothed value) directly, offset by `m_dssFloorDepth`, with no anchor state machine, no 18 dB guard, and no 0.5 dB rounding -- a deliberate simplification (this reuse is the right call: porting a second, parallel smoothing/anchor layer on top of an already-smoothed Thetis-sourced value would be redundant state, not missing behavior), but the divergence itself needs to be visible to a future maintainer evaluating whether the 18 dB anti-jump guard or the rounding step are worth adding later, which is why this row now names them instead of claiming there is nothing upstream to compare against. **`dssSpanDb`** (`src/gui/SpectrumWidget.cpp:11871-11879` [@1872028c], same citation correction): upstream delegates to `DbmRangeTransition::displaySpanDb(m_dynamicRange)`, defined as `std::clamp(dynamicRangeDb, 10.0f, 120.0f)` (`DbmRangeTransition.h:125-130` [@1872028c]) -- part of a whole unported dBm-range-transition-animation subsystem (`beginDbmRangeTransition`/`Range`/`Evaluation`, smooth interpolation when the operator changes the dBm scale) that NereusSDR has no counterpart for. NereusSDR's `dssSpanDb()` reads `m_dynamicRange` directly with a bare `std::max(1.0f, ...)` floor: a 1 dB lower bound rather than upstream's 10 dB, and no upper bound at all versus upstream's 120 dB ceiling. Not changed as part of this fix (out of scope -- the review round asked for accurate provenance, not a behavior change to a control the operator can already reach), but recorded here so the bounds difference is visible rather than silently absent. **`dssColorRangeDb`** (review-round addition, Finding 1): upstream computes this same cap in two places, both re-read directly -- inline in the UBO-assembly block at `SpectrumWidget.cpp:14195` [@1872028c] (`std::min(rangeDb, DssRenderer::kColorSpanDb)`, `rangeDb` there being upstream's per-frame `dssSpanDb()` equivalent) and again in `buildDssImage()`'s own local at `:11889-11890` [@1872028c] (`std::min(rangeDb, DssRenderer::kColorSpanDb)`, identical expression, CPU-fallback path). The first pass of this task wrote `writeDssMeshUbo`'s `colorRangeDb` field as the bare `kDssColorSpanDb` constant with no cap against the actual dBm span, dropping this `std::min` entirely -- a real visible bug (any operator narrowing the dBm range below 45 dB gets the 45 dB colour aperture stretched across a wider span than the display shows, washing the surface out relative to upstream), caught in review, not by any test (nothing pinned the aperture before this fix). Fixed by adding a named `dssColorRangeDb()` accessor computing `std::min(dssSpanDb(), kDssColorSpanDb)`, used at the UBO call site instead of the bare constant, and pinned by a new test (`colorRangeDb_capsAtTheStableApertureButNeverExceedsANarrowSpan`, proven to fail under the exact same mutation -- constant restored unconditionally -- that shipped the bug; see `tests/tst_dss_floor_and_span.cpp`, same file, and the mutation-proof paragraph below for the captured red/green transcript). **`buildDssWideRow`**: NereusSDR-original per the `.h` row's design-doc citation; sizes its window from `dssMaxRowSpanFactor(dssShapeForAngle(0))` (Task 1/2 free functions) so the window covers the WIDEST angle the 3D Angle slider allows rather than the current one, per design doc section 4.2 ("moving the angle slider never requires re-ingesting history") -- angle-independence verified directly by `wideRowWindow_isAngleIndependent` (bit-identical `c`/`b` at angle 0 vs 100, `QCOMPARE` not a tolerance check). One bug found and fixed during implementation, not present in the upstream analog because there is none to compare against: the task brief's given `std::clamp(int, int, qsizetype)` calls (`fullBins.size()` returns `QVector`'s `qsizetype`, not `int`) fail template argument deduction and do not compile; fixed by narrowing to an explicit `const int binCount = static_cast<int>(fullBins.size());` used in both clamp calls. **`dssRowSpanTarget`** (`src/gui/SpectrumWidget.cpp:12743-12784` [@1872028c], corrected from the task brief's given `:12703-12742`, which is actually `uploadDssPaletteLut`'s range from the immediately preceding Task 8 row -- verified against the pinned SHA directly before citing): ports the upstream function's overall shape (delegate to a row-span-factor helper fed by "the newest row that actually carries a tile/slice", per upstream's own comment reasoning quoted in this port's comment) while dropping two upstream sections per the brief's own "minus" framing -- the `AETHER_DSS_ROW_SPAN` environment-variable A/B override (upstream `:12752-12764`) and the "age 0 is not authoritative" Flex/Kiwi multi-producer caveat block (upstream `:12766-12779`, which explains why upstream cannot trust its OWN newest row and must search back through the visible ring) -- because NereusSDR has exactly one producer (`pushDssRow`) and it always attaches a wide slice whenever `buildDssWideRow` finds one available, so there is no alternate producer that could append without one while zoomed in the way upstream's Kiwi/native-tile-less path can. **`pushDssRow`** extended per the task brief to fill both channels via `buildDssWideRow`+`m_dss.pushRowWithWide`, falling back to the exact-only `pushRow` when the wide channel is empty; gains `m_dssScrollProgressRows = 0.0f;` at the end (brief-specified reset, see `.h` row and the display-timer tick below). **`updateSpectrumLinear()`** (NereusSDR-original addition, no upstream counterpart claimed -- upstream's exact/wide arbitration is a bin-range test against a single frame, not a second cached conversion pass): caches `m_fullLinearBins` (already-existing full-DDC linear-power member, unsliced -- confirmed by reading its own doc comment and every call site before writing to it) converted to dBm into `m_lastFullBinsDbm`, gated on `Mode3D` (nothing reads it in 2D). The per-bin conversion (`10*log10(dbmScale * linear + 1.0e-60)`) is not invented: it mirrors `SpectrumAvenger::apply()`'s `avMode==0` "no averaging" case byte-for-byte (`SpectrumAvenger.cpp:166-172`, itself `WDSP analyzer.c:495-501 [v2.10.3.13]` case 0 of the `avenger()` function at `analyzer.c:464-554` verbatim) including the same `1.0e-60` log floor (`SpectrumAvenger.cpp:65`) and the SAME `dbmScale` local variable the exact channel's own detector/avenger call already uses two lines above -- calibration-consistent with the exact channel by construction, not a separately-derived constant. Detector reduction and averaging are deliberately NOT replicated: both are display-pixel/view-width concerns, and `DssRenderer::pushRowWithWide` (Task 4) already applies its own independent temporal smoothing (median-of-3 + 1-2-1 blur + IIR blend, `smoothDssRow()`) to whatever it receives, so a second averaging pass here would only blur what that smoothing already handles. **`writeDssMeshUbo`** (`src/gui/SpectrumWidget.cpp:14148-14408` [@1872028c], replacing the Task 7 stub): upstream assembles the equivalent UBO inline inside `renderGpuFrame()`, not as a separate function, so this port restructures rather than lifts; field order matches `dss_mesh.vert:14-58` exactly (independently re-verified twice against the live shader file, byte for byte, both before and after writing this function -- not assumed from the brief -- plus empirically at runtime, see below) but two upstream sub-ranges are NOT ported: the row-span-factor ease-toward-target animation (`:14164-14183`, `m_dssRowSpanFactor += kRowSpanAlpha * (target - m_dssRowSpanFactor)`) -- this task writes `dssRowSpanTarget()`'s result straight through with no per-frame easing, a possible follow-up and not a correctness requirement -- and the slice-shadow descriptor computation (`:14216-14366`, `writeShadowSlot`/`appendShadow` against `m_sliceOverlays`), left zeroed per the task brief's own comments and explicitly deferred to Task 12. The `rowFrames` loop (`:14373-14401`) is the closest thing to a verbatim carry-over in shape (four floats per row, ages `0..kDssRows-1`, oldest first) but diverges in one respect verified by hand against `dss_mesh.vert:112-154` before porting this way: upstream pre-subtracts each row's target-relative delta at CPU write time (`rowCenterMhz - dssTargetCenterMhz`) and always writes a literal `0.0f` for `targetCenterOffsetMhz`; this port writes both sides absolute (the row's own captured centre via `rowCenterMhzAtAge`/`rowWideCenterMhzAtAge`, and the CURRENT centre via `targetCenterMhz`) and lets the shader's own `(targetCenterOffsetMhz - frame.x)` subtraction (`dss_mesh.vert:121`, `:139`) do the identical arithmetic -- algebraically identical either way (`target - row` both ways), and cheaper: NereusSDR's `DssRenderer::rowCenterMhzAtAge`/`rowWideCenterMhzAtAge` (Task 4) already return absolute MHz with no upstream-style pre-subtraction hook, so writing them straight through avoids `kDssRows` CPU subtractions every frame whether or not that row is ever sampled. `zCurve`'s `0.6f` literal matches the task brief and the master plan identically (both authored 2026-08-08); upstream's own `m_dssZCurve` runtime field defaults to `0.70f` (`SpectrumWidget.h:1492` [@1872028c]) but is exposed as an operator control there, so the two defaults are not directly comparable -- NereusSDR has no Z-Curve control yet (not in this plan's scope through Task 15) and the brief's `0.6f` is taken as given rather than second-guessed toward upstream's default. **Display-timer tick** (constructor, `m_displayTimer`'s existing `QTimer::timeout` lambda): gains the `m_dssScrollProgressRows` accumulate/reset-on-mode-exit logic per the brief's instruction ("advance it in the display-timer tick from wall clock between waterfall pushes, clamped to [0, 1]"); NereusSDR-original shape (accumulate a per-tick wall-clock delta as a fraction of `m_wfUpdatePeriodMs`, clamped, vs. upstream's `QElapsedTimer::elapsed()` queried fresh each read) -- see `.h` row for why. Empirical proof the UBO writer fills exactly `kDssMeshUboFloats` (512) at runtime, not just by counting: `Q_ASSERT` compiles to a no-op under this build's `-DQT_NO_DEBUG`, so counting on the assert alone would prove nothing here. Followed the Task 7 row's own precedent exactly -- temporarily forced `m_spectrumRenderMode`'s default to `Mode3D` (tagged `// TEMP-VERIFY-T9UBO`) and added one temporary `qWarning()` printing `i` and `kDssMeshUboFloats` immediately before the assert, built and ran the real app for ~6 seconds, `SIGTERM`, and captured six real log lines across both `SpectrumWidget` instances this app creates, all reading `i= 512 kDssMeshUboFloats= 512`. Reverted both temporary changes immediately after capturing the log; `grep -rn "TEMP-VERIFY-T9UBO" src/` returned nothing before rebuilding and committing. Test-completeness fixes to the task brief's own given `tests/tst_dss_floor_and_span.cpp` (not a change to any production file, recorded here because both were proven necessary by mutation rather than assumed): `rowSpanTarget_scalesTheAvailableOverhang` gained `resize(400,200)`/`show()`/`qWaitForWindowExposed()` (brief's literal test omits these; `pushWaterfallRow()`'s first guard returns immediately while `m_waterfall` is null, the exact trap `tst_dss_row_tee.cpp`'s `stopOnTx_freezesBothPanesTogether` already documents for this same file) and `setFullBinsForTest(QVector<float>(4096, -130.0f))` (the new `.h`-row seam) before the push loop, plus a `QCOMPARE(w.dssRowsPushedForTest(), 3)` precondition assertion. Both proven load-bearing by mutation, not argued: with `setFullBinsForTest` commented out, `rowSpanTarget_scalesTheAvailableOverhang` fails at the `dssRowSpanTarget(0.096) > 1.0f` assertion (returns `1.0f` at span 100, same as span 0, because the wide channel never receives data so `newestWideBandwidthMhz` never exceeds `targetBandwidthMhz`); with the resize/show/expose block commented out instead, the added `dssRowsPushedForTest()==3` precondition fails first (actual `0`), pinpointing the real failure mode precisely instead of leaving the later row-span assertion to fail for an unrelated-looking reason. Reverted both mutations and reconfirmed all 6 test functions green (8 pass counting init/cleanup) before committing. **Review-round mutation proof (colour-range cap)**: `dssColorRangeDb()`'s `std::min(dssSpanDb(), kDssColorSpanDb)` temporarily replaced with the bare `kDssColorSpanDb` constant (the exact bug Finding 1 reported). Wide-span assertion (100 dB, `QVERIFY(abs(dssColorRangeDb() - 45.0f) < 0.01f)`) stayed green even under the mutation, because 45 is what the bug ALSO returns at any span above the cap -- the mutation is invisible from that assertion alone. Narrow-span assertion (20 dB, `QVERIFY(abs(dssColorRangeDb() - 20.0f) < 0.01f)`) went red (`'std::abs(w.dssColorRangeDb() - 20.0f) < 0.01f' returned FALSE`), because the mutated function still returns 45 instead of tracking the narrower 20 dB span. Reverted; reconfirmed 7/7 test functions green (9 pass counting init/cleanup) before committing. | "`dssFloorDbm`/`dssSpanDb` restructure AetherSDR functions of the same names (`:11735`/`:11871` [@1872028c]) -- corrected in review from an earlier claim that nothing upstream existed to cite; NereusSDR reuses simpler existing state (the already-smoothed, independently Thetis-sourced `m_nfLerpAverage`; a bare `max(1.0f, ...)` floor) instead of porting upstream's IIR-anchor/18dB-guard/rounding machinery or its unported `DbmRangeTransition` clamp(10,120) subsystem -- both real, described divergences, not omissions. `buildDssWideRow` remains genuinely NereusSDR-original (design doc section 4.2: one calibrated measurement here, vs. AetherSDR's exact-FFT-plus-separately-calibrated-native-tile arbitration). `dssColorRangeDb` (review-round Finding 1 fix) ports upstream's `std::min(rangeDb, kColorSpanDb)` cap (`:14195`, `:11889-11890` [@1872028c]) as a named, tested accessor, closing a real bug where the first pass wrote the bare constant with no cap against a narrow dBm range. `dssRowSpanTarget` restructured from AetherSDR `src/gui/SpectrumWidget.cpp:12743-12784` [@1872028c] (brief's cited range corrected from `:12703-12742`, which is Task 8's `uploadDssPaletteLut`), dropping the env-var A/B override and the multi-producer 'age 0 is not authoritative' caveat that does not apply to NereusSDR's single producer. `writeDssMeshUbo` restructures AetherSDR's inline UBO assembly (`:14148-14408`) into a standalone function matching `dss_mesh.vert`'s std140 layout field-for-field, verified twice by direct read and once empirically at runtime (`i=512` logged across both live SpectrumWidget instances, temporary instrumentation reverted before commit); the row-span ease animation and slice-shadow descriptors are excluded (deferred/Task 12). The dBm conversion feeding the wide channel reuses the exact `SpectrumAvenger`/WDSP avenger() no-averaging formula and calibration scale already used two lines above for the exact channel, not an invented one." |
| `src/gui/DssRenderer.h` (Task 10 extension) | `src/gui/DssRenderer.h:15-29, 234-251, 388-403` [@1872028c] | Task 10 (CPU fallback surface). Restores two class-doc sentences Task 4's row silently dropped along with the functionality they describe ("Fill colour follows amplitude via an injected palette and dims with depth for atmospheric perspective; a bright per-amplitude line tops each ridge." and the caching/QRhi-compositing paragraph) -- caught by this task's own "sweep both directions" comment check against upstream `:15-29`, not carried forward from any earlier review. The restored caching paragraph's file reference is adapted (upstream: "cached ... and composite through the existing QRhi overlay pipeline", written against upstream's single-file `DssRenderer.h`; NereusSDR's shared geometry constants the same paragraph implicitly assumes already live in the separate `DssGeometry.h` Task 1 split out -- a factual correction to match this file's real structure, not a wording preference). Adds the public `image()` method (`:234-251` [@1872028c], doc comment ported near-verbatim) with one signature addition and one behavioural addition beyond upstream, both marked `//-KG4VCF [v0.5.3]`: a `const DssShape& shape` trailing parameter (upstream needs none -- its perspective is the three `static constexpr float` members `kBackWidthFrac`/`kDepthSpanFrac`/`kFrontMaxRidgeFrac`; NereusSDR promoted these to a runtime `DssShape` back in Task 1 for the 3D Angle control), and the three shape floats becoming part of the cache-invalidation gate (`image()`'s `changed` computation) and the cache-key state (`m_cacheShape`, new private member) -- required verification #1 below. Adds the matching private `rebuild()` declaration (adds the same `shape` parameter, otherwise upstream's `:308-310` signature verbatim) and the six pre-existing cache-field members plus the new `m_cacheShape` field, using the task brief's own given snippet verbatim including its `//-KG4VCF [v0.5.3]` comment. Adds the free function `dssDepthVisibleSegments()` (`:388-403` [@1872028c], doc comment AND declaration both verbatim -- no adaptation needed, upstream's own comment does not reference any upstream-specific file structure) inside `namespace NereusSDR` after the class (matching upstream's own placement outside its class, upstream having no namespace wrapper at all at this SHA). Not ported from this cited range: `rowAt(int)` (`:312-313` [@1872028c]) -- `rebuild()`'s single call site is satisfied by the existing Task 4 private helper `ringAtAge(int)` plus direct `m_rows`/`m_rowCoverage` indexing, avoiding a redundant parallel accessor; see the `.cpp` row for why this substitution is safe (`rebuild()`'s loop bound never exceeds `ringAtAge`'s clamp range, so the clamp is a no-op in this call context). | "Restores two AetherSDR `DssRenderer.h:15-29` [@1872028c] class-doc sentences an earlier task silently dropped along with the `image()`/`rebuild()` functionality they describe (caught by this task's own comment sweep, not a prior review). Adds `image()` (`:234-251`) and `rebuild()` (declaration only) with a `DssShape` parameter upstream does not need -- its perspective is compile-time, ours is a runtime 3D Angle control -- and folds the three shape floats into the cache key so an angle change actually invalidates the cached surface. Adds `dssDepthVisibleSegments()` (`:388-403`) verbatim, doc comment included." |
| `src/gui/DssRenderer.cpp` (Task 10 extension) | `src/gui/DssRenderer.cpp:12-16, 17, 18-20, 28, 35-39, 41-48, 753-779, 781-888, 890-903` [@1872028c] | Task 10. Adds four CPU-only tunables to the existing anonymous namespace: `kMinDim` (`:16` [@1872028c], inserted before the pre-existing `kTemporalAlpha` to match upstream's declaration order) and `kSlopeGain`/`kShadeLo`/`kShadeHi` (`:18-20` [@1872028c], inserted after `kTemporalAlpha`). The shared "CPU-only tunables" comment block (`:12-15` [@1872028c]) is reproduced above `kMinDim` rather than moved to sit above the pre-existing `kTemporalAlpha` (Task 4 already gave that constant its own single-line cite to `:17`) -- the block comment covers all five constants collectively upstream, so placing it beside the newly-added four rather than disturbing Task 4's already-committed, independently-cited line is the non-restructuring choice; the file's actual upstream file reference inside that comment ("DssRenderer.h") is corrected to "DssGeometry.h" for the same file-structure reason as the `.h` row's class-doc restoration. Adds three colour helpers verbatim: `chan` (`:28`), `scaled` (`:35-39`), `lerpColor` (`:41-48`, including its one-line "Linear blend" comment) -- all new to this file; `median3` and `frequencyFramesMatch` were already ported by Task 4 and are untouched. Adds `image()` (`:753-779` [@1872028c]) and `rebuild()` (`:781-888` [@1872028c]) verbatim apart from the two changes the task brief specifies: every `kCols`/`kRows`/`kVisibleRows`/`kHaze` reference becomes `kDssCols`/`kDssRows`/`kDssVisibleRows`/`kDssHaze` (Task 1/4 naming, unchanged from those tasks), every `kBackWidthFrac`/`kDepthSpanFrac`/`kFrontMaxRidgeFrac` reference becomes `shape.backWidthFrac`/`shape.depthSpanFrac`/`shape.frontMaxRidgeFrac`, and the cache-key `changed` computation in `image()` gains the three shape-float comparisons (see `.h` row). `rebuild()`'s row lookup substitutes upstream's `rowAt(age)` (which internally computes `(m_head + age) % kRows` a second time for the row array, alongside a SEPARATE explicit `ring` computation for the coverage array two lines later -- upstream computes the ring index twice) with a single `const int ring = ringAtAge(age);` reused for both `m_rows[ring]` and `m_rowCoverage[ring]`; safe because `rebuild()`'s loop runs `age` from `visibleRowCount() - 1` down to `0`, always `< kDssVisibleRows <= kDssRows`, so `ringAtAge`'s `std::clamp(age, 0, kDssRows - 1)` is a no-op over this call's entire domain and the two lookups are provably equivalent, not merely assumed so. Adds `dssDepthVisibleSegments()` (`:890-903` [@1872028c]) verbatim, placed at the end of the file matching upstream's own placement. Adds `#include <QPainter>`/`#include <QPolygonF>` (needed by `rebuild()`) and `#include <limits>` (needed by `dssDepthVisibleSegments()`'s `std::numeric_limits<qreal>::max()`), none present in this file before this task. **Brief test-bug found and fixed, not silently worked around**: the task brief's given `tests/tst_dss_cpu_surface.cpp` asserted `dssDepthVisibleSegments({10.0, 50.0, 90.0}).at(0) == false`. Hand-tracing the byte-for-byte-verbatim ported algorithm against this input gives `true` (the leading segment's `silhouetteY` comparand is still the initial `std::numeric_limits<qreal>::max()` sentinel on the first loop iteration, so nothing has been established yet to occlude it against -- the very first segment can never be culled, by construction, since nothing is drawn in front of it). Independently confirmed against upstream's OWN dedicated unit test at this SHA, `tests/dss_renderer_test.cpp:363-374` [@1872028c] (`testDepthShadowOcclusion`'s `behind` case, `dssDepthVisibleSegments({50.0, 120.0, 130.0, 140.0})`, asserts `!behind.at(0)` -- i.e. `behind.at(0)` must be `true` -- `\|\| behind.at(1) \|\| behind.at(2)`, the identical leading-segment-is-never-culled shape), not merely reasoned about in isolation. The implementation was left untouched (it is upstream's real, verbatim, tested behaviour); the test's `QCOMPARE(h.at(0), false)` was corrected to `QCOMPARE(h.at(0), true)` and its lead comment rewritten to describe why. This is the class of error CLAUDE.md's "My briefs have carried wrong facts before" warning anticipates -- trust the (upstream-verified) code, not the brief, and say so. | "Adds `image()`/`rebuild()` (AetherSDR `src/gui/DssRenderer.cpp:753-888` [@1872028c]) and `dssDepthVisibleSegments()` (`:890-903`) verbatim apart from the runtime-`DssShape` substitution the `.h` row describes, plus four new CPU-only tunables (`kMinDim`/`kSlopeGain`/`kShadeLo`/`kShadeHi`, `:16`/`:18-20`) and three colour helpers (`chan`/`scaled`/`lerpColor`, `:28`/`:35-39`/`:41-48`). `rebuild()`'s row lookup is consolidated onto the existing Task 4 `ringAtAge()` helper instead of porting upstream's separate `rowAt()` accessor, proven equivalent over this call's domain rather than assumed. Fixed one wrong assertion in the task brief's own given test (`h.at(0)` should be `true`, not `false` -- the leading segment can never be occluded), confirmed against upstream's own `tests/dss_renderer_test.cpp` at the pinned SHA, not merely against a hand trace." |
| `src/gui/SpectrumWidget.h` (Task 10 extension) | `src/gui/SpectrumWidget.cpp:11880-11894`, `SpectrumWidget.h:1510-1511` [@1872028c] | Task 10. **Beyond the task brief's own file list** (which names only `SpectrumWidget.cpp`): the brief's Step 3 instruction ("blit it through the existing overlay pipeline") is deliberately structural, not literal code -- the plan document's own "Known plan-level gaps" section says so explicitly ("Task 10's fallback blit ... described structurally ... because it depends on the exact overlay-compositing call shape at the Task 7 integration point, which the implementer will have in front of them"). Reading that Task 7 integration point directly (this file, `NEREUS_GPU_SPECTRUM` block) shows Task 7 already claimed the unqualified `m_dssSrb`/`m_dssGpuTex`-shaped names for its OWN mesh SRB (`m_dssSrb` specifically -- upstream reserves that exact unqualified pair for ITS OWN cached-image fallback, keeping a `Mesh` infix on its mesh fields instead; NereusSDR's Task 7 took the shorter names for the mesh side), and the Task 7 row of this document says so too: "Task 10 must pick different names for the fallback quad's own SRB/texture when it lands, rather than the `m_dssSrb`/`m_dssGpuTex` names upstream uses." Implementing the brief's own instruction correctly therefore requires new `.h` members Task 7 anticipated but could not add itself. Adds `buildDssImage(const QSize& px, int scaleStripPx)` (declared here so both `drawSpectrum()`'s QPainter path and the new GPU-path upload function below can share it) -- see the `.cpp` row for why its signature drops upstream's `floorDbm` parameter. Adds two constants, `kDssFallbackMaxW`/`kDssFallbackMaxH` (upstream `SpectrumWidget.h:1510-1511` [@1872028c], values `1024`/`512` unchanged), deliberately placed OUTSIDE the `NEREUS_GPU_SPECTRUM` block for the same reason upstream places its own `kDssMaxW`/`kDssMaxH` outside `AETHER_GPU_SPECTRUM`: `buildDssImage()`'s QPainter caller needs them even when GPU spectrum rendering is compiled out entirely (upstream's own comment at this site, reused near-verbatim: "Consumed by BOTH the GPU mesh and the CPU fallback surface ... the CPU paint path needs them even when GPU spectrum rendering is disabled"). Inside the `NEREUS_GPU_SPECTRUM` block, adds the fallback quad's own GPU resources -- `m_dssFallbackTex`/`m_dssFallbackSrb` (`QRhiTexture*`/`QRhiShaderResourceBindings*`, the renamed pair Task 7's row above already flagged), `m_dssFallbackTexW`/`m_dssFallbackTexH` (upstream's own `m_dssTexW`/`m_dssTexH`, renamed for the same reason), and `m_dssFallbackUploadedGen` (upstream's own `m_dssLastUploadedGen`, renamed) -- plus the `uploadDssFallbackImage(QRhiResourceUpdateBatch*, const QRect&, float)` method declaration (see `.cpp` row for the upstream citation of its body). | "Adds `buildDssImage()` (shared CPU-surface builder) and `kDssFallbackMaxW`/`kDssFallbackMaxH` (AetherSDR `SpectrumWidget.h:1510-1511` [@1872028c]) outside the GPU-only block, since the CPU-only build path needs them too. Adds the GPU-only cached-image-quad resources (`m_dssFallbackTex`/`m_dssFallbackSrb`/etc) under names distinct from Task 7's mesh resources of similar shape, per that task's own forward note that Task 10 would need to. Goes beyond the task brief's literal file list (SpectrumWidget.cpp only) because implementing its own 'blit through the existing overlay pipeline' instruction correctly requires these declarations; the brief's own plan document flags this exact call shape as deliberately left for the implementer." |
| `src/gui/SpectrumWidget.cpp` (Task 10 extension) | `src/gui/SpectrumWidget.cpp:11880-11894, 14409-14447, 14643-14654, 14915-14933, 12619-12630` [@1872028c] | Task 10. **`buildDssImage(const QSize& px, int scaleStripPx)`** (`:11880-11894` [@1872028c]): drops upstream's `floorDbm` parameter -- upstream's two call sites pass different values (`m_lastDetectDssFloor` in 2D, `dssFloorDbm()` in 3D) because upstream's `buildDssImage()` can theoretically be reached with either mode active; NereusSDR's helper is only ever called while `m_spectrumRenderMode == Mode3D` (both of ITS call sites are inside `if (m_spectrumRenderMode == Mode3D)` guards), so `dssFloorDbm()` unconditionally is the correct and only value, and threading a redundant parameter through would just restate what the function already knows. Uses `dssColorRangeDb()` (Task 9's named accessor) rather than upstream's inline `std::min(rangeDb, DssRenderer::kColorSpanDb)` recomputed from the locally-rounded `rangeDb` -- a deliberate, deviation-worth-naming substitution: it keeps this CPU path reading the exact same colour-aperture accessor `writeDssMeshUbo()` (Task 9) already reads for the GPU mesh, rather than a second, separately-rounded near-duplicate of the same cap that could in principle disagree with it at the sub-0.5-dB level. `zCurve` (`0.6f`) and `bgFill` (`QColor(0x0a, 0x0a, 0x14)`) are hardcoded literals matching `writeDssMeshUbo()`'s own hardcoded values (Task 9, same file) rather than new named `SpectrumWidget` members -- upstream's `m_dssZCurve`/`m_bgFillColor` are real operator-facing state NereusSDR has not ported (no Z-Curve or background-image control exists in this plan through Task 15); matching Task 9's own already-shipped precedent of a bare literal, not introducing new unrequested surface area, is the conservative choice pending whichever later task (if any) exposes these as controls. **`drawSpectrum()`** QPainter-path addition (`:14915-14933` [@1872028c], upstream's `paintEvent`'s `is3D` branch): the ONLY 3DSS surface a `-DNEREUS_GPU_SPECTRUM=OFF` build ever draws, since that configuration never compiles `renderGpuFrame()` at all (confirmed by reading the file's own `#ifdef NEREUS_GPU_SPECTRUM` / `#endif // NEREUS_GPU_SPECTRUM` bracketing, `:7914`-`:9423` pre-task, which encloses `renderGpuFrame()` and everything `m_dssMeshReady`-related in full). Not explicitly named in the task brief's Step 3 (which only describes the `renderGpuFrame()` wiring) but required by the task's own stated goal ("Two independent ways to land here, and both must work" -- a `-DNEREUS_GPU_SPECTRUM=OFF` build and an RGBA16F-unsupported runtime); confirmed as upstream's own actual architecture, not inferred, by reading upstream's `SpectrumWidget.h:33-39` (`AETHER_GPU_SPECTRUM` toggle, same shape as NereusSDR's) and finding upstream's OWN second `buildDssImage()` call site inside ITS `paintEvent`, at `:14928` [@1872028c] -- upstream wires the identical CPU surface into both its QRhi-mesh-unready path AND its pure-QPainter path, confirming this is not scope creep but the two-call-site pattern the source itself uses. Skips the grid entirely in 3D mode by early-returning before the classic-trace code, matching upstream's `is3D`-vs-`else` branch split; does not touch the separate `drawGrid(p, specRect)` call already made earlier in `paintEvent()` (untouched, unmoved) because the opaque plot region this draws (scale strip `0`, so no transparent strip at all) fully covers whatever grid lines were drawn moments before -- net-identical z-order to upstream's grid-skip, achieved by opaque overpaint instead of not drawing the grid in the first place, a smaller, lower-risk diff than restructuring `paintEvent()`'s call sequence. **`uploadDssFallbackImage()`** (`:14409-14447` [@1872028c], upstream's `renderGpuFrame()`-inline `} else if (is3D) { ... }` cached-image-fallback block): ported as a standalone method (matching this file's own Task 7-established convention of factoring GPU resource-update blocks into named `uploadDss*`/`writeDss*` functions, none of which upstream itself factors out this way). Two exclusions: `dpr` is a parameter here instead of being recomputed from `renderTarget()->pixelSize()` locally, matching `writeDssMeshUbo()`'s own existing `dpr` parameter convention in this same file, and the `perfEnabled`-gated `PerfTelemetry::instance().recordGpuUpload(...)` call is dropped -- NereusSDR's `PerfMonitor` has no per-texture-upload-kind equivalent and adding one is out of this task's scope. **Draw-call branch** (`:14643-14654` [@1872028c], upstream's `} else if (is3D && m_ovPipeline && m_dssSrb && m_dssTexW > 0) { ... }`): inserted as a new middle arm between the existing mesh branch and the existing classic-FFT-trace branch in `renderGpuFrame()`'s spectrum-region if/else-if chain, guarded on `m_spectrumRenderMode == Mode3D` (not `is3D`, which is already false by construction whenever this branch's condition needs evaluating -- `is3D` requires `m_dssMeshReady`) rather than upstream's `is3D` (upstream's local is only ever `m_spectrumRenderMode == Mode3D`, with no mesh-readiness component, so upstream's condition and NereusSDR's read identically at their respective call sites despite the different local names) -- MUST win over the classic-FFT-trace `else if` or Mode3D would silently render the 2D trace whenever the mesh is unavailable, defeating the whole task. Reuses `m_ovPipeline`/`m_ovVbo` (the shared textured-quad pipeline every overlay draw call already uses) with the new `m_dssFallbackSrb`, viewport-restricted to `specRect` exactly like the mesh draw immediately above it in the same chain -- confirmed as the correct upstream pattern (share the PIPELINE object across draws via a fresh `cb->setShaderResources()` call per draw, not a new pipeline) by finding `m_ovDynSrb` already doing exactly this in NereusSDR's own pre-existing overlay-dynamic-layer draw call, a pattern this task's addition is structurally identical to, not a new one. **`initOverlayPipeline()`** gains the fallback quad's texture/SRB creation (`:12619-12630` [@1872028c], upstream's own `m_dssGpuTex`/`m_dssSrb` init inside its equivalent function), initially sized to the full window like the pre-existing `m_ovGpuTex`/`m_ovDynGpuTex` siblings, matching upstream's own initial sizing (`QSize(pw, ph)`, not the eventual capped `dssW`/`dssH`) -- `uploadDssFallbackImage()` resizes it down to the actual (capped) surface size on first real use. **`releaseResources()`** gains the matching teardown (delete + null both pointers, reset the three tracking fields), mirroring the existing mesh-teardown block immediately above it. **Resource-update wiring**: `uploadDssFallbackImage(batch, specRect, dssDpr)` is called from the existing `if (m_spectrumRenderMode == Mode3D) { ... }` resource-update block (already home to `rebuildDssMeshIfNeeded`/`uploadDssPaletteLut`/`uploadDssHeightRows`/`writeDssMeshUbo`), additionally gated on `!m_dssMeshReady` so the CPU image is never rebuilt on a frame the mesh branch will actually draw. **Required verification #1 (cache-shape mutation)**: the `.h`/`.cpp` DssRenderer row's `image()` cache-key shape comparison was temporarily deleted (replaced with a one-line marker comment) and `tst_dss_cpu_surface` rerun -- `cache_dropsOnShapeChange` went red (`'r.generation() > g' returned FALSE`), all seven other cases in the same binary stayed green, isolating the mutation's effect to exactly the property it claims to cover; the comparison was restored and the full 8/8 reconfirmed green. Real captured output for both runs is in `task-10-report.md`. **Required verification #2 (fallback actually composites)**: `renderGpuFrame()`'s new branch cannot be reached on this dev machine's real GPU (Metal always supports RGBA16F, so `initDssMeshPipeline()` never naturally fails), so this was verified empirically rather than assumed, following the same temporary-instrumentation methodology the Task 7/9 rows above already established as this plan's own precedent for GPU-only paths with no other way to exercise them: two temporary, clearly-marked hooks (env-var-gated, tagged `TEMP TASK-10 VERIFICATION HOOK`) were added -- one forcing `initDssMeshPipeline()` to fail immediately (simulating real RGBA16F-unsupported hardware) and one pushing 40 rows of synthetic sine-wave dBm data into `m_dss` via the pre-existing `pushWaterfallRowForTest()` seam and forcing `setSpectrumRenderMode(1)` shortly after construction. Built and launched the real app with the env var set; the log showed the forced-failure warning firing exactly once per `SpectrumWidget` instance, confirming `m_dssMeshReady` stayed false; a screenshot of the running app (captured via `screencapture`, read back and visually inspected, not just logged) shows a fully opaque, correctly-perspective-shaped dark-red trapezoid filling the spectrum region -- a clear diagonal side-wall where the back rows narrow inward, a lighter ridge band near the top, composited correctly beneath the pre-existing VFO marker/band-plan-label/S-meter chrome exactly as the mesh path already does -- not a blank region, not the classic 2D trace. No errors, crashes, or asserts appeared in the captured log. Both temporary hooks were fully reverted immediately after capturing this evidence; `grep -rn "TEMP TASK-10\|NEREUS_DSS_FORCE_CPU_FALLBACK_DEBUG" src/` returned nothing before rebuilding and committing, and the full `tests_gui` suite (192/192, +1 over the pre-task 191/191 baseline) was reconfirmed green against the reverted tree. | "`buildDssImage()` ports AetherSDR `SpectrumWidget.cpp:11880-11894` [@1872028c] with `floorDbm` dropped (NereusSDR has one caller-independent floor source) and `dssColorRangeDb()` substituted for upstream's inline recomputation, so the CPU fallback and the GPU mesh (Task 9) read the exact same colour aperture. Wires the fallback into BOTH places upstream itself does -- `drawSpectrum()`'s QPainter path (`:14915-14933`, the ONLY 3DSS surface a `-DNEREUS_GPU_SPECTRUM=OFF` build ever draws) and `renderGpuFrame()`'s mesh-unready branch (`:14409-14447` build/upload, `:14643-14654` draw) -- beyond the task brief's own Step 3, which only describes the second; upstream's own `paintEvent` has a second `buildDssImage()` call site confirming this is upstream's real architecture, not scope creep. New GPU resources (`m_dssFallbackTex`/`m_dssFallbackSrb`/etc, `SpectrumWidget.cpp:12619-12630` init) are named distinctly from Task 7's mesh resources per that task's own forward note. Required verification #1 (cache-shape mutation) and #2 (empirical fallback compositing, screenshot-verified via a temporary, fully-reverted RGBA16F-failure simulation) both performed and recorded in task-10-report.md." |
