# NereusSDR — Project Context for Claude

## Project Goal

Port **Thetis** (the OpenHPSDR / Apache Labs SDR console, written in C#) to a
**cross-platform C++20 application** using Qt6. The architectural template is
**AetherSDR** (a FlexRadio SmartSDR client). Target radios: all OpenHPSDR
Protocol 1 and Protocol 2 devices, including the Apache Labs ANAN line and
Hermes Lite 2.

**Critical implication:** The client does ALL signal processing (DSP, FFT,
demodulation). The radio is essentially an ADC/DAC with network transport.

---

## ⚠️ SOURCE-FIRST PORTING PROTOCOL (Read This Before Every Task)

NereusSDR is a **port**, not a reimagination. The Thetis codebase is the
authoritative source for all radio logic, DSP behavior, protocol handling,
constants, state machines, and feature behavior. **Do not guess. Do not
infer. Do not improvise.** Read the source, then translate it.

### The Rule: READ → SHOW → TRANSLATE

For every piece of logic you write that has a Thetis equivalent:

1. **READ** the relevant Thetis source file(s). Use `find`, `grep`, or `rg`
   to locate the C# code. The Thetis repo should be cloned at
   `../Thetis/` (relative to the NereusSDR root). Capture the Thetis
   version tag once at the start of the session — `git -C ../Thetis
   describe --tags` (release) or `git -C ../Thetis rev-parse --short
   HEAD` (between releases) — every inline cite you write in this
   session gets that stamp.
2. **SHOW** the original code before writing anything. State:
   `"Porting from [file]:[function/line range] — original C# logic:"` and
   quote or summarize the relevant section.
3. **TRANSLATE** the C# to C++20/Qt6 faithfully. Use AetherSDR patterns for
   the Qt6 structure (signals/slots, class layout, threading), but the
   **behavior and logic** must come from Thetis.

### License-preservation rule (non-negotiable)

When porting any Thetis file, you MUST — in the same commit that introduces
the port — copy the following from the Thetis source into the NereusSDR
file's header comment:

1. All `Copyright (C)` lines naming contributors (FlexRadio, Wigley,
   Samphire, W2PA, mi0bot, etc.)
2. The GPLv2-or-later permission block verbatim
3. The Samphire dual-licensing statement — ONLY if the Thetis source file
   contains Samphire-authored contributions
4. A trailing "Modification history (NereusSDR)" block with the port date,
   human author, and AI tooling disclosure

Templates live in `docs/attribution/HOW-TO-PORT.md`. Failure to
preserve these notices on a new port is a GPL compliance bug, not a style
nit — reject the PR.

### Byte-for-byte headers and multi-file attribution

Each Thetis source file has its own distinct header with different copyright
holders and modification credits (e.g. `console.cs` credits W2PA and
Samphire; `display.cs` credits VK6APH and Samphire). These headers are NOT
interchangeable.

- Copy each source file's header **byte-for-byte** — do not paraphrase,
  summarize, or merge headers from different files.
- If a NereusSDR file ports from **multiple** Thetis files, include **every**
  relevant header, separated by `// --- From [filename] ---` markers.
- Include the Thetis version (`v2.10.3.13`) and commit (`501e3f5`) in the
  "Ported from" line.

### Inline comment preservation — SHIP-BLOCKING

**This is a GPL attribution rule, not a style preference. Dropping a
developer-attribution tag during porting is a compliance bug that
blocks release.** A real incident (2026-04-21) shipped with a
`//DH1KLM` tag silently dropped during a `computeAlexFwdPower` port
— caught only because someone eyeballed the PR. The fix is
mechanical:

All inline comments from Thetis source code within ported logic **must be
preserved verbatim** in the C++ translation. This includes:

- **Developer attribution tags** — `//DH1KLM`, `//MW0LGE`, `//W2PA`,
  `//G8NJJ`, `//MI0BOT`, etc. Canonical list of recognized authors
  lives in `docs/attribution/thetis-author-tags.json`, built
  mechanically by `scripts/discover-thetis-author-tags.py`.
- **Dash-prefix attribution** — `//-W2PA`, `// -W2PA`
- **Version-tagged attribution** — `//[2.10.3.13]MW0LGE`, `//MW0LGE [2.9.0.7]`
- **Underscored variants** — `//MW0LGE_21k5 change to rx2`
- **Behavioral notes** — `// only cleared by getAndResetADC_Overload()`
- **TODO / FIXME / XXX / HACK** annotations
- Any `//` comment on or above a ported line of logic

When the C++ translation restructures the code so that the comment no longer
sits on the same line, place it on the nearest equivalent line with a note:
```cpp
// MW0LGE_21k5 change to rx2  [original inline comment from display.cs:10079]
```

**Mechanical enforcement:** `scripts/verify-inline-tag-preservation.py`
runs in the pre-commit hook chain and in CI. For every
`// From Thetis X:N [@sha]` cite in the diff, it opens `../Thetis/X`
(or `../mi0bot-Thetis/X`) at line N, extracts any author tag within
±5 source lines, and fails the commit if a corresponding tag is not
present within ±10 port lines. No way to land a port with a dropped
tag. If the check fires, re-insert the verbatim tag exactly as it
appears upstream.

**Corpus drift:** when you re-sync Thetis (`git -C ../Thetis pull`),
also run:
```
python3 scripts/discover-thetis-author-tags.py
```
to refresh the corpus. CI's `--drift` check fails the PR if new
upstream contributors aren't in the committed corpus.

### Pre-port checklist (Ring 1 — authoring-time)

Before reading any Thetis source file (`../Thetis/...`), state out loud:

1. **Thetis file** you're about to read.
2. **NereusSDR file(s)** the port will touch (new or existing).
3. **Provenance status** of each NereusSDR file — run:
    ```
    grep -l "<nereussdr-path>" docs/attribution/THETIS-PROVENANCE.md
    ```
   If the file is not registered, the port is a **new attribution event**.
4. **Plan**: if (3) returned nothing, you will add the verbatim upstream
   header AND a PROVENANCE row in the same commit that introduces the
   ported logic. Use `docs/attribution/HOW-TO-PORT.md` for the format.

If you cannot answer (3) confidently, **stop and grep** before continuing.
The cost of asking is one shell command; the cost of skipping is a
merge-blocking CI failure (or worse, a missed gap that ships to main).

This applies equally to:
- New files that port Thetis logic.
- Edits to NereusSDR-original files that **add** new ported logic
  (e.g. wiring in a new Thetis-derived constant or formula).
- Ports from non-Thetis upstreams (`../mi0bot-Thetis/`, `../AetherSDR/`,
  WDSP) — same protocol, different PROVENANCE table / variant.

Verifier scripts (`scripts/verify-thetis-headers.py`,
`scripts/check-new-ports.py`) are the safety net (Ring 3, in CI). The
local pre-commit hook installed via `scripts/install-hooks.sh` runs the
same scripts pre-push (Ring 2). The primary control is this checklist.

### What Counts As "Guessing" (NEVER Do These)

- Writing a function body without first reading the Thetis equivalent
- Assuming what WDSP function signatures, parameters, or return types look like
- Inventing enum values, constants, magic numbers, thresholds, or defaults
- Paraphrasing what a Thetis feature "probably does" based on its name
- Writing placeholder/stub logic with TODOs for things that exist in Thetis
- Assuming protocol message formats or byte layouts without reading the code
- "Improving" or "simplifying" Thetis logic without being asked to
- Using general DSP knowledge instead of the actual WDSP API calls Thetis makes
- Porting a Thetis file without copying its license header and appending a modification note

### Constants and Magic Numbers

Preserve ALL constants, thresholds, scaling factors, and magic numbers exactly
as they appear in Thetis. If Thetis uses `0.98f`, NereusSDR uses `0.98f`. If
Thetis uses `2048` as a buffer size, document where it came from and keep it.
Give constants a `constexpr` name but note the Thetis origin — with a
version stamp — in a comment:

```cpp
// From Thetis console.cs:4821 [v2.10.3.13] — original value 0.98f
static constexpr float kAgcDecayFactor = 0.98f;
```

The `[v2.10.3.13]` tag records the Thetis release the value was verified
against. Use `[@shortsha]` when no tagged release applies, and refresh the
stamp whenever you re-port from a newer upstream. Full grammar:
`docs/attribution/HOW-TO-PORT.md` §Inline cite versioning.

### WDSP Calls — Extra Caution

- Every WDSP function call must match the exact name, parameter order, and
  types from `Project Files/Source/wdsp/` in the Thetis repo
- Cross-reference against `Project Files/Source/Console/dsp.cs` (the C#
  P/Invoke declarations) for the managed-side signatures
- DSP parameter ranges, defaults, and scaling come from Thetis code, not
  from general knowledge or WDSP documentation
- When in doubt, read both the WDSP C source AND the Thetis C# callsite

### If You Can't Find the Source

**STOP AND ASK.** Say: "I cannot locate the Thetis source for [X]. Which
file or class should I look in?" Do NOT fabricate an implementation. It is
always better to ask than to guess wrong.

### The Two-Source Rule

| Question | Source |
| --- | --- |
| **What** does the code do? | Thetis (C# source) |
| **How** do we structure it in Qt6? | AetherSDR (C++20/Qt6 patterns) |

AetherSDR provides the **skeleton** (class structure, signals/slots, threading,
state management patterns). Thetis provides the **organs** (logic, algorithms,
constants, protocol handling, DSP flow, feature behavior).

### Thetis Source Layout Quick Reference

```
../Thetis/
├── Project Files/
│   └── Source/
│       ├── Console/          ← Main UI, radio logic, state management
│       │   ├── console.cs    ← Monster file: VFO, band, mode, DSP, display
│       │   ├── setup.cs      ← Setup dialog (hardware config, DSP params)
│       │   ├── display.cs    ← Spectrum/waterfall rendering
│       │   ├── audio.cs      ← Audio engine, VAC, portaudio
│       │   ├── cmaster.cs    ← Channel master (WDSP channel management)
│       │   ├── dsp.cs        ← WDSP P/Invoke declarations
│       │   ├── NetworkIO.cs  ← Protocol 1/2 network I/O
│       │   ├── protocol2.cs  ← Protocol 2 specific handling
│       │   └── ...
│       └── wdsp/             ← WDSP C source (DSP engine)
│           ├── channel.c     ← Channel create/destroy/exchange
│           ├── RXA.c         ← RX channel pipeline
│           ├── TXA.c         ← TX channel pipeline
│           └── ...
```

---

## AI Agent Guidelines

When helping with NereusSDR:

* Prefer C++20 / Qt6 idioms (std::ranges, concepts if clean, Qt signals/slots)
* Keep classes small and single-responsibility
* Use RAII everywhere (no naked new/delete)
* Comment non-obvious protocol decisions with protocol version (P1 vs P2)
* Never suggest Wine/Crossover workarounds — goal is native cross-platform
* Flag any proposal that would break the core RX path (I/Q → WDSP → audio)
* If unsure about protocol behavior → ask for pcap captures first
* **Use `AppSettings`, never `QSettings`** — see "Settings Persistence" below
* **Read `CONTRIBUTING.md`** for full contributor guidelines and coding conventions
* Reference OpenHPSDR protocol specs, not SmartSDR protocol

### Autonomous Agent Boundaries

AI agents may autonomously fix:

* **Bugs with clear root cause** — persistence missing, guard missing, crash fix
* **Protocol compliance** — matching OpenHPSDR protocol spec behavior
* **Build/CI fixes** — missing dependencies, platform compatibility

AI agents must **NOT** autonomously change:

* **Visual design** — colors, fonts, layout, theme
* **UX behavior** — how controls work, what clicks do, keyboard shortcuts
* **Architecture** — adding new threads, changing signal routing, new dependencies
* **Feature scope** — adding features beyond what the issue describes
* **Default values** — changing defaults that affect all users
* **DSP parameters or constants** — unless directly porting from Thetis source

When in doubt, implement the fix and note in the PR that design decisions need
maintainer review.

---

## C++ Style Guide

* **No `goto`** — use early returns, break, or restructure the logic
* **No raw `new`/`delete`** — use `std::unique_ptr`, `std::make_unique`, or Qt parent ownership
* **No `#define` macros for constants** — use `constexpr` or `static constexpr`
* **Braces on all control flow** — even single-line `if`/`else`/`for`/`while`
* **`auto` sparingly** — use explicit types unless the type is obvious from context
* **Naming**: classes `PascalCase`, methods/variables `camelCase`, constants `kPascalCase`, member variables `m_camelCase`
* **Platform guards**: use `#ifdef Q_OS_WIN` / `Q_OS_MAC` / `Q_OS_LINUX`, not `_WIN32` or `__APPLE__`
* **Don't remove code you didn't add** — review the diff before submitting
* **Atomic parameters for cross-thread DSP** — main thread writes via `std::atomic`, audio thread reads. Never hold a mutex in the audio callback.
* **Error handling**: log with `qCWarning(lcCategory)`, don't throw exceptions
* **Thetis origin comments**: when porting logic, add `// From Thetis [file]:[line or function] [v<version>|@<shortsha>]` comments. The bracketed stamp records the upstream release or commit the port was verified against; grab it from `git -C ../Thetis describe --tags` (or `rev-parse --short HEAD`) at the moment of porting. Full grammar and placement rules: `docs/attribution/HOW-TO-PORT.md` §Inline cite versioning

---

## Build

```
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j$(nproc)
./build/NereusSDR
```

Dependencies: `qt6-base qt6-multimedia cmake ninja pkgconf fftw`
Optional (Linux only): `libpipewire-0.3-dev` ≥ 0.3.50 — enables the
PipeWire-native audio bridge (Phase 3O). Without it the build still
works and the Linux audio path falls back to the existing pactl /
LinuxPipeBus FIFO route.

WDSP source is in `third_party/wdsp/` (TAPR v1.29 + linux_port.h for cross-platform).
FFTW3: system package on Linux/macOS, pre-built DLL on Windows (`third_party/fftw3/`).
First run generates FFTW wisdom (~15 min). Cached in `~/.config/NereusSDR/` for subsequent launches.

Current version: **0.2.0** (set in `CMakeLists.txt`).

---

## Architecture Quick Reference

Key source directories: `src/core/` (protocol, audio, DSP), `src/models/`
(RadioModel, SliceModel, etc.), `src/gui/` (MainWindow, SpectrumWidget, applets).

**Key classes:**

* `RadioModel` — central state, owns connection + all sub-models + WdspEngine
* `SliceModel` — per-receiver VFO state (freq, mode, filter, AGC, gains, antenna). Single source of truth.
* `PanadapterModel` — per-panadapter display state (center freq, bandwidth, dBm range). As of 3G-8, also owns the per-band grid storage (`BandGridSettings {dbMax, dbMin}` × 14 bands, global `gridStep`) and the current `band()` derived from `setCenterFrequency()` via `Band::bandFromFrequency()`. Emits `bandChanged(Band)` on boundary crossings and pushes the stored slot into `dBmFloor`/`dBmCeiling` automatically. `RadioModel::spectrumWidget()` / `fftEngine()` non-owning view hooks are set here by `MainWindow` so setup pages reach the renderer.
* `Band` (`src/models/Band.h`) — first-class 14-band enum (160m–6m + GEN + WWV + XVTR) with `bandLabel()`, `bandKeyName()` (AppSettings key suffix), `bandFromFrequency()` (IARU Region 2 lookup with WWV discrete centers), `bandFromUiIndex()` / `uiIndexFromBand()`. Added in 3G-8.
* `ReceiverManager` — DDC-aware receiver lifecycle, maps logical receivers to hardware DDCs; exposes DDC center frequency for CTUN pan positioning
* `RadioDiscovery` — OpenHPSDR radio discovery on UDP port 1024
* `RadioConnection` — Protocol 1 (UDP) and Protocol 2 (UDP multi-port) connections
* `WdspEngine` — WDSP lifecycle manager (wisdom, channels, impulse cache)
* `RxChannel` — per-receiver WDSP channel wrapper (fexchange2, NB, mode/filter/AGC, shift offset for CTUN demodulation)
* `AudioEngine` — QAudioSink output (Int16 stereo, timer-based drain)
* `FFTEngine` — FFTW3 spectrum computation (worker thread, I/Q → dBm bins)
* `SpectrumWidget` — GPU spectrum trace + waterfall display (QRhiWidget — Metal/Vulkan/D3D12); zoom via visibleBinRange() bin subsetting with m_ddcCenterHz/m_sampleRateHz
* `VfoWidget` — floating VFO flag (AetherSDR pattern): freq display, mode/filter/AGC tabs, antenna buttons
* `ContainerWidget` — dock/float/resize/axis-lock container shell (Thetis ucMeter equivalent)
* `FloatingContainer` — top-level window wrapper for floating containers (Thetis frmMeterDisplay equivalent)
* `ContainerManager` — singleton container lifecycle: 3 dock modes (panel/overlay/floating), axis-lock reposition, QSplitter, persistence
* `MeterWidget` — GPU meter renderer (QRhiWidget — 3 pipelines: background texture, vertex geometry, QPainter overlay); one per container, renders all MeterItems in single draw pass
* `MeterItem` — base class for composable meter elements (normalized 0-1 positioning, data binding, z-order); concrete types: BarItem (+ Edge mode), TextItem, ScaleItem, SolidColourItem, ImageItem, NeedleItem (+ ANANMM/CrossNeedle calibration extensions), SpacerItem, FadeCoverItem, LEDItem, HistoryGraphItem, MagicEyeItem, NeedleScalePwrItem, SignalTextItem, DialItem, TextOverlayItem, WebImageItem, FilterDisplayItem, RotatorItem, ButtonBoxItem (shared grid base), BandButtonItem, ModeButtonItem, FilterButtonItem, AntennaButtonItem, TuneStepButtonItem, OtherButtonItem, VoiceRecordPlayItem, DiscordButtonItem, VfoDisplayItem, ClockItem, ClickBoxItem, DataOutItem
* `ItemGroup` — composites N MeterItems into named presets; 35+ factory methods including S-Meter, Power/SWR, ANANMM (7-needle), CrossNeedle (dual fwd/rev), MagicEye, History, SignalText, and all TX bar meters
* `MeterPoller` — QTimer-based WDSP meter polling (100ms/10fps); calls RxChannel::getMeter(), pushes to bound MeterWidgets
* `AppSettings` — custom XML settings persistence (NOT QSettings)
* `MainWindow` — wires everything together, signal routing hub; uses QSplitter for spectrum + container panel
* `SpectrumOverlayPanel` — 10-button overlay panel on SpectrumWidget with 5 flyout sub-panels (display/filter/noise/spots/tools), auto-close
* `SetupDialog` — 47-page setup dialog across 10 categories with real controls
* `AppletPanelWidget` — fixed S-Meter header + scrollable applet body for Container #0
* `applets/` — 12 applets: RxApplet, TxApplet, PhoneCwApplet, EqApplet, FmApplet, DigitalApplet, PureSignalApplet, DiversityApplet, CwxApplet, DvkApplet, CatApplet, TunerApplet
* `StyleConstants.h` — shared color palette, fonts, widget style constants
* `HGauge` — horizontal bar gauge widget
* `ComboStyle` — styled combo box shared across applets
* `ColorSwatchButton` (`src/gui/ColorSwatchButton.h`) — reusable color picker button: QPushButton subclass, QColorDialog with alpha, `colorChanged(QColor)` signal, static `colorToHex` / `colorFromHex` helpers for AppSettings `"#RRGGBBAA"` round-trip. Added in 3G-8; used by 9 call sites across the Display setup pages (S11/S13 trace colours, W10 waterfall low colour, G6 band edge, G9–G13 grid/text/zero-line colours).

**Thread Architecture:**

| Thread | Components |
| --- | --- |
| **Main** | GUI rendering, RadioModel, all sub-models, user input |
| **Connection** | RadioConnection (UDP I/O, protocol framing) |
| **Audio** | AudioEngine + WdspEngine (I/Q processing, DSP, audio output) |
| **Spectrum** | FFT computation, waterfall data generation |

Cross-thread communication uses auto-queued signals exclusively.
RadioModel owns all sub-models on the main thread. Never hold a mutex in the
audio callback.

### Data Flow (Phase 3E + CTUN + Zoom — VERIFIED WORKING)

```
Radio (ADC) → UDP port 1037 (DDC2) → P2RadioConnection
    ↓ iqDataReceived(ddcIndex=2, interleaved float I/Q)
ReceiverManager::feedIqData(2) → maps DDC2 → receiver 0
    ↓ iqDataForReceiver(0, samples)
RadioModel lambda:
    ├── emit rawIqData(samples) → FFTEngine → SpectrumWidget
    ├── Deinterleave I/Q, accumulate 238 → 1024 samples
    └── RxChannel::processIq() → fexchange2() → decoded audio
        ↓
    AudioEngine::feedAudio() → float→int16 → m_rxBuffer
        ↓ 10ms timer drain
    QAudioSink (48kHz stereo Int16) → Speakers

FFT → Display (with zoom):
    FFTEngine emits N bins (full DDC bandwidth)
    → SpectrumWidget::updateSpectrum() stores in m_smoothed
    → visibleBinRange(N) maps m_centerHz ± m_bandwidthHz/2 to bin indices
      using m_ddcCenterHz + m_sampleRateHz for bin-to-frequency mapping
    → GPU/CPU renderer iterates only [firstBin..lastBin], stretched to full display
    → pushWaterfallRow() writes only visible bin subset to waterfall texture

User zooms (freq scale drag or Ctrl+scroll):
    m_bandwidthHz changes → visibleBinRange() narrows → immediate visual zoom
    On mouse release → bandwidthChangeRequested → MainWindow replans FFT size
    → FFTEngine delivers more bins → sharper resolution at new zoom level

User tunes VFO:
    VfoWidget (wheel/click/edit) → emit frequencyChanged(hz)
    → SliceModel::setFrequency(hz)
    → ReceiverManager::setReceiverFrequency(0, hz)
      → hardwareFrequencyChanged(DDC2, hz)
      → P2RadioConnection::setReceiverFrequency(2, hz) + Alex HPF/LPF update
      → sendCmdHighPriority() → radio retunes DDC NCO
```

---

## Key Implementation Patterns

### Settings Persistence (AppSettings — NOT QSettings)

**IMPORTANT:** Do NOT use `QSettings` anywhere in NereusSDR. All client-side
settings are stored via `AppSettings` (`src/core/AppSettings.h`), which writes
an XML file at `~/.config/NereusSDR/NereusSDR.settings`. Key names use
PascalCase (e.g. `LastConnectedRadioMac`, `DisplayFftAverage`). Boolean
values are stored as `"True"` / `"False"` strings.

```
auto& s = AppSettings::instance();
s.setValue("MyFeatureEnabled", "True");
bool on = s.value("MyFeatureEnabled", "False").toString() == "True";
```

### Radio-Authoritative Settings Policy

**Radio-authoritative (do NOT persist):** ADC attenuation, preamp, TX power,
antenna selection.

**Hardware sample rate and active RX count:** persisted per-MAC in AppSettings
under `hardware/<mac>/radioInfo/sampleRate` and `.../activeRxCount`. Applied
on next connect. This matches Thetis, which persists rate globally via
`DB.SaveVarsDictionary("Options", ...)` (setup.cs:1627). NereusSDR scopes
per-MAC so users with multiple radios retain per-radio selections. Live-apply
of rate changes to a running connection is deferred to the follow-up PR that
adds WDSP channel teardown/rebuild infrastructure.

**Client-authoritative (persist in AppSettings):** VFO frequency, mode, filter,
DSP settings (AGC, NR, NB, ANF), layout arrangement, UI preferences, display
preferences. OpenHPSDR radios don't store per-slice state.

### GUI↔Model Sync (No Feedback Loops)

* Model setters emit signals → RadioConnection sends protocol commands
* Protocol responses update models via `applyStatus()` or equivalent
* Use `m_updatingFromModel` guard or `QSignalBlocker` to prevent echo loops
* Follow AetherSDR's proven pattern exactly

---

## Documentation Index

### Master Plan & Progress

| Document | Description |
| --- | --- |
| [docs/MASTER-PLAN.md](docs/MASTER-PLAN.md) | Full phased roadmap, menu bar layout, GUI container mapping (Thetis → NereusSDR), skin system design, progress tracking |
| [CONTRIBUTING.md](CONTRIBUTING.md) | Contributor guidelines, coding conventions, PR process |
| [STYLEGUIDE.md](STYLEGUIDE.md) | Applet color palette, button states, gauge zones, slider/combo styling |
| [CHANGELOG.md](CHANGELOG.md) | Version history and per-phase feature additions |

### Architecture Design Docs (`docs/architecture/`)

| Document | Scope |
| --- | --- |
| [overview.md](docs/architecture/overview.md) | Layer diagram, thread architecture, RX/TX data flow overview |
| [radio-abstraction.md](docs/architecture/radio-abstraction.md) | P1/P2 connections, MetisFrameParser, ReceiverManager, C&C register map, protocol details |
| [multi-panadapter.md](docs/architecture/multi-panadapter.md) | PanadapterStack (5 layouts), PanadapterApplet, wirePanadapter(), FFTRouter |
| [gpu-waterfall.md](docs/architecture/gpu-waterfall.md) | FFTEngine, SpectrumWidget, QRhi shaders, overlay system, color schemes |
| [wdsp-integration.md](docs/architecture/wdsp-integration.md) | RxChannel/TxChannel wrappers, PureSignal, thread safety, WDSP channel lifecycle |
| [skin-compatibility.md](docs/architecture/skin-compatibility.md) | SkinParser, extended skin format, Thetis import, 4-pan support |
| [adc-ddc-panadapter-mapping.md](docs/architecture/adc-ddc-panadapter-mapping.md) | ADC->DDC->Receiver->FFT->Pan signal chain, Thetis UpdateDDCs() analysis, per-board DDC assignment, bandwidth limits |
| [ctun-zoom-design.md](docs/architecture/ctun-zoom-design.md) | CTUN zoom bin subsetting: visibleBinRange(), hybrid FFT replan, DDC center tracking |

### Implementation Plans (`docs/architecture/phase*-plan.md`)

| Plan | Phase | Status |
| --- | --- | --- |
| [phase3d-spectrum-waterfall-plan.md](docs/architecture/phase3d-spectrum-waterfall-plan.md) | 3D: GPU Spectrum & Waterfall | **Complete** |
| [ctun-zoom-plan.md](docs/architecture/ctun-zoom-plan.md) | 3E: CTUN Zoom Bin Subsetting | **Complete** |
| [phase3g1-container-infrastructure-plan.md](docs/architecture/phase3g1-container-infrastructure-plan.md) | 3G-1: Container Infrastructure | **Complete** |
| [phase3g2-meter-widget.md](docs/superpowers/plans/2026-04-10-phase3g2-meter-widget.md) | 3G-2: MeterWidget GPU Renderer | **Complete** |
| [phase3g3-core-meter-groups.md](docs/superpowers/plans/2026-04-10-phase3g3-core-meter-groups.md) | 3G-3: Core Meter Groups | **Complete** |
| [phase3-ui-skeleton-plan-v2.md](docs/architecture/phase3-ui-skeleton-plan-v2.md) | 3-UI: Full UI Skeleton | **Complete** |
| [phase3g4-g6-advanced-meters-design.md](docs/architecture/phase3g4-g6-advanced-meters-design.md) | 3G-4/5/6: Advanced Meters Design Spec | **Approved** |
| [phase3g4-advanced-meters-plan.md](docs/architecture/phase3g4-advanced-meters-plan.md) | 3G-4: Advanced Meter Items | **Complete** |
| [phase3g5-interactive-meters-plan.md](docs/architecture/phase3g5-interactive-meters-plan.md) | 3G-5: Interactive Meter Items | **Complete** |
| [phase3g6a-plan.md](docs/architecture/phase3g6a-plan.md) | 3G-6 (one-shot): Full Thetis Parity + MMIO | **Complete** |
| [phase3g8-rx1-display-parity-plan.md](docs/architecture/phase3g8-rx1-display-parity-plan.md) | 3G-8: RX1 Display Parity (47 Spectrum/Waterfall/Grid controls, Band enum, per-band grid, GPU polish) | **Complete** |
| [phase3g8-verification/README.md](docs/architecture/phase3g8-verification/README.md) | 3G-8: 47-control manual verification matrix | Matrix drafted |
| [phase3f-multi-panadapter-plan.md](docs/architecture/phase3f-multi-panadapter-plan.md) | 3F: Multi-Panadapter + DDC Assignment | Planning (after 3I-4) |

### Protocol Reference (`docs/protocols/`)

| Document | Scope |
| --- | --- |
| [openhpsdr-protocol1.md](docs/protocols/openhpsdr-protocol1.md) | P1 summary + pointer to capture reference; Thetis P1 source map |
| [openhpsdr-protocol1-capture-reference.md](docs/protocols/openhpsdr-protocol1-capture-reference.md) | Annotated HL2↔Thetis capture: discovery, start/stop, EP6/EP2 frames, C0 maps, cadence, band/TX traces, HL2 quirks, Phase 3L checklist |
| [openhpsdr-protocol2.md](docs/protocols/openhpsdr-protocol2.md) | P2 UDP multi-port, command packets, per-DDC I/Q streams |

### Phase 1 Analysis Docs (`docs/phase1/`)

| Document | Key Findings |
| --- | --- |
| 1A: AetherSDR Analysis | RadioModel hub, auto-queued signals, worker threads, AppSettings XML, GPU rendering via QRhi |
| 1B: Thetis Analysis | Dual-thread DSP (RX1/RX2), pre-allocated receivers, one-way protocol, skin system |
| 1C: WDSP Analysis | 256 API functions, channel-based DSP, fexchange2() for I/Q, PureSignal feedback loop |

### Current Phase: v0.2.3 shipped 2026-04-24 — RX epic + Alex antennas complete; nothing in flight; **3M-1 SSB TX is the next major epic**

| Phase | Goal | Status |
| --- | --- | --- |
| 3A: Radio Connection | Connect to ANAN-G2 via P2, receive I/Q | **Complete** |
| 3B: WDSP Integration | Process I/Q through WDSP, audio output | **Complete** |
| 3C: macOS Build | Cross-platform WDSP build + wisdom crash fix | **Complete** |
| 3D: Spectrum Display | GPU spectrum + waterfall (QRhi Metal/Vulkan/D3D12) | **Complete** |
| 3E: VFO + Multi-RX Foundation | VFO controls, CTUN panadapter, rewired I/Q pipeline | **Complete** |
| **3G-1: Container Infrastructure** | **Dock/float/resize/persist container shells** | **Complete** |
| **3G-2: MeterWidget GPU Renderer** | **QRhi-based meter rendering engine** | **Complete** |
| **3G-3: Core Meter Groups** | **S-Meter, Power/SWR, ALC presets** | **Complete** |
| **3-UI: Full UI Skeleton** | **12 applets, 9-menu bar, SetupDialog (47pp), SpectrumOverlayPanel** | **Complete** |
| **3G-4: Advanced Meter Items** | **12 item types + ANANMM/CrossNeedle presets + Edge mode** | **Complete** |
| **3G-5: Interactive Meter Items** | **14 interactive items + mouse forwarding + ButtonBoxItem base** | **Complete** |
| **3G-6: Container Settings Dialog (one-shot)** | **3-column dialog, 31 per-item editors, MMIO subsystem (4 transports + JSON/XML/RAW), Edit Container submenu** | **Complete** |
| **3G-7: Polish** | **MMIO clone-path bug fix + 5 subclass accessor gap fills + NeedleItemEditor QGroupBox grouping** | **Complete** |
| **3G-8: RX1 Display Parity** | **47 Spectrum/Waterfall/Grid controls wired (Setup → Display), `Band` enum + per-band grid on PanadapterModel, `BandButtonItem` 12→14, GPU polish: overlay cache invalidation, waterfall chrome in overlay texture, peak hold VBO, fill/gradient/cal-offset in vertex gen** | **Complete (PR #8)** |
| **3I: Radio Connector & Radio-Model Port** | **P1 full family (Atlas/Hermes/HermesII/Angelia/Orion/HL2), BoardCapabilities registry, ConnectionPanel, HardwarePage 9-tab capability-gated, per-MAC persistence, mi0bot RadioDiscovery port, RadioConnectionError taxonomy** | **Complete** |
| **3G-9: Display Refactor** | **3G-9a source-first audit + Thetis-first tooltips + slider/spinbox refactor (PR #25, v0.1.5); 3G-9b smooth defaults + Clarity Blue palette + Reset-to-Smooth-Defaults button (v0.1.5); 3G-9c ClarityController adaptive tuning + NoiseFloorEstimator + Re-tune button + per-band Clarity memory (v0.1.5)** | **Complete (v0.1.5)** |
| **3G-10: RX DSP Parity + AetherSDR Flag Port** | **Stage 1: widget library + SliceModel stubs + VfoWidget S-meter (PR #28), tab rewrite + mode containers + tooltip coverage (PR #30). Stage 2: 10 WDSP feature slices wired (AGC-adv/EMNR/SNB/APF/squelch/mute-pan-bin/NB2/RIT-XIT/lock/mode-containers), per-slice-per-band persistence, Thetis-first tooltips.** | **Complete** |
| **3G-13: Step Attenuator & ADC Overload** | **StepAttenuatorController (Classic + Adaptive auto-att), P1/P2 adcOverflow emission, ADC OVL status badge, Setup→General→Options page, RxApplet ATT/S-ATT row, per-model preamp items from Thetis SetComboPreampForHPSDR, stepAttMaxDb (31/61), per-MAC persistence, 9 tests. PR #34.** | **Complete (v0.1.5)** |
| **3P-I-a: Alex Antenna Integration (Core)** | **AlexController → `RadioConnection::setAntennaRouting` pump (3 triggers: antennaChanged / bandChanged / Connected); VFO Flag, RxApplet, Setup-grid, SpectrumOverlayPanel combos, AntennaButtonItem all route through AlexController; kPopupMenu stylesheet fixes Ubuntu 25.10 dark-on-dark menus; HL2/Atlas hide all antenna UI on `!caps.hasAlex \|\| antennaInputCount < 3`; byte-for-byte P1/P2 wire-lock tests + 10 new test cases + manual verification matrix; closes #98. PR #116.** | **Complete** |
| **3P-I-b: RX-Only Antennas + SKU Labels + XVTR** | **`SkuUiProfile` 14-SKU overlay drives per-product labels (RX1/RX2/XVTR vs EXT2/EXT1/XVTR vs BYPS/EXT1/XVTR). `AlexController` +6 flags (Ext1/Ext2/RxOutOnTx mutual-exclusion trio + rxOutOverride + useTxAntForRx + xvtrActive), 5 persisted per-MAC. P1 bank0 C3 bits 5-7 + P2 Alex0 bits **8-11** wired (design-doc correction: plan said 27-30 — bit 27 is `_TR_Relay`; Thetis network.h:263-307 authoritative). `applyAlexAntennaForBand` now full Alex.cs:310-413 port minus MOX/Aries (→ 3M-1): isTx branch + Ext-on-TX mapping + xvtrActive-from-band derivation + rx_out_override clamp. Setup → Antenna Control gains RX-only grid column + 5 TX-bypass checkboxes; Alex-2 Filters sub-tab gates on `caps.hasAlex2`; VFO Flag gains BYPS 3rd button (double-gated: hasRxBypassRelay + hasRxOutOnTx). 32 new test cases + verification matrix §7-§8. PR #117.** | **Complete (v0.2.3)** |
| **3G-14: 💡 AI-Assisted Issue Reporter** | **💡 menu bar corner widget + version check gate + AI-assisted issue dialog (ported from AetherSDR TitleBar). Provider buttons, structured prompt, feature_request.yml / bug_report.yml integration. PR #36.** | **Complete (v0.1.6)** |
| **3G-RX-Epic (v0.2.3): dBm Strip + NB Family + 7-Filter NR + PipeWire** | **Sub-A AetherSDR-style dBm scale strip (wheel-zoom range, hover crosshair, calibrated). Sub-B full Thetis NB/NB2/SNB family port via `NbFamily` wrapper, per-slice-per-band persistence. Sub-C-1 7-filter NR stack on VFO flag DSP grid (NR1 ANR / NR2 EMNR / NR3 RNNR rnnoise / NR4 SBNR libspecbleach / DFNR DeepFilterNet3 / MNR Apple Accelerate MMSE-Wiener / ANF), `DspParamPopup` right-click quick controls, mutual-exclusion via `setActiveNr`, bundled rnnoise + DFNR models in every release artifact. Phase 3O Linux PipeWire-native audio bridge supersedes 0.2.2 pactl fallback.** | **Complete (v0.2.3)** |
| 3M-1: Basic SSB TX (was 3I-TX) | TxChannel, mic input, MOX state machine, I/Q output | **Next** |
| 3M-2: CW TX (was 3I-CW) | Sidetone, firmware keyer, QSK/break-in | Planned |
| 3M-3: TX Processing | 18-stage TXA chain + TX-side RX DSP additions | Planned |
| 3M-4: PureSignal (was 3I-PS) | Feedback DDC, calcc/IQC engine, PSForm, AmpView | Planned |
| 3F: Multi-Panadapter | DDC assignment (incl. PS states), FFTRouter, PanadapterStack, enable RX2 | Planned |
| 3H: Skins | Thetis-inspired skin format, 4-pan, legacy import | Planned |
| 3J: TCI + Spots | TCI server, DX Cluster/RBN clients, spot overlay | Planned |
| 3K: CAT/rigctld | 4-channel rigctld, TCP CAT server | Planned |
| 3L: HL2 ChannelMaster.dll port | HL2 IoBoardHl2 I2C-over-ep2 wire encoding, bandwidth monitor full port | Planned |
| 3M: Recording | WAV record/playback, I/Q record, scheduled | Planned |
| **3N: Packaging** | **Consolidated `release.yml` (prepare → build×3 → sign-and-publish), `/release` skill, GPG-signed alpha builds: Linux AppImage ×2 archs, macOS Apple Silicon DMG, Windows portable ZIP + NSIS installer** | **Complete** |

---

## Reference Repositories

1. **AetherSDR** — `https://github.com/ten9876/AetherSDR`
   * Architectural template: radio abstraction, state management, signal/slot patterns, GPU rendering, multi-pan layout
2. **Thetis** — `https://github.com/ramdor/Thetis`
   * Feature source: every Thetis capability must be accounted for and ported
   * **Clone to `../Thetis/` relative to NereusSDR root**
3. **WDSP** — `https://github.com/TAPR/OpenHPSDR-wdsp`
   * DSP engine: all signal processing functions
