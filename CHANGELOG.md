# Changelog

## [Unreleased]

### Added
- New `--profile <name>` / `-p <name>` CLI flag for running multiple NereusSDR instances against different radios concurrently. Each profile gets its own isolated `NereusSDR.settings` + log directory under `profiles/<name>/`; profile names are validated against `[A-Za-z0-9_-]+` to prevent path traversal. FFTW wisdom stays machine-scoped (shared across profiles). Main window title gains a `[<profile>]` suffix. `SupportDialog` / `SupportBundle` now read the same profile-scoped log directory `main.cpp` writes to. Without `--profile`, behavior is byte-for-byte unchanged. Closes #100.

### Fixed
- **Hermes Lite 2 bandpass filter now switches on band/VFO change.** P1RadioConnection was emitting `m_alexHpfBits=0`/`m_alexLpfBits=0` permanently because the filter bits were never recomputed from frequency. P2 had the right code; lifted into shared `src/core/codec/AlexFilterMap` and called from P1's `setReceiverFrequency`/`setTxFrequency`. (Phase 3P-A)
- **Hermes Lite 2 step attenuator now actually attenuates.** P1's bank 11 C4 was using ramdor's 5-bit mask + 0x20 enable for every board; HL2 needs mi0bot's 6-bit mask + 0x40 enable + MOX TX/RX branch. Fixed via per-board codec subclasses (`P1CodecStandard` for Hermes/Orion, `P1CodecHl2` for HL2). RxApplet S-ATT slider range now widens to 0-63 dB on HL2 from `BoardCapabilities::attenuator.maxDb`. (Phase 3P-A)

### Changed
- `P1RadioConnection`'s C&C compose layer refactored into per-board codec subclasses (`P1CodecStandard`, `P1CodecHl2`, `P1CodecAnvelinaPro3`, `P1CodecRedPitaya`) behind a stable `IP1Codec` interface. Behavior byte-identical for every non-HL2 board (regression-frozen via `tst_p1_regression_freeze` against a pre-refactor JSON baseline). Set `NEREUS_USE_LEGACY_P1_CODEC=1` to revert to the pre-refactor compose path for one release cycle as a rollback hatch.
- `P2RadioConnection` now calls the shared `AlexFilterMap::computeHpf/Lpf` helpers instead of its own inline copies; byte output unchanged.
- `BoardCapabilities::Attenuator` extended with `mask`, `enableBit`, and `moxBranchesAtt` fields capturing per-board ATT byte encoding parameters.

## [0.2.1] - 2026-04-19

### Features
- (none)

### Fixes
- fix(radio-model): stop recalling bandstack on VFO-tune crossings
- fix(radio-model): guard per-band save/restore lambdas against reentrancy

### Docs
- (none)

### CI / Build
- (none)

### Other
- (none)

### Tests
- (none)

### Refactors
- (none)


## [0.2.0] - 2026-04-19

### Auto AGC-T (PR #53)

- `NoiseFloorTracker` — lerp-based noise-floor estimator feeding the
  Auto-threshold timer with MOX guard and `agcCalOffset`.
- AUTO button on the AGC-T slider row (VfoWidget + RxApplet) toggles
  auto-mode; periodic NF update keeps the threshold tracking.
- Right-click on the AGC-T slider opens Setup directly on the AGC/ALC
  page; Setup page controls wired to SliceModel.
- Thetis-source tooltips on all four AGC controls + attenuator
  fast-attack.

### Sample-rate wiring (PR #35)

- Per-MAC persistence of hardware sample rate + active-RX count under
  `hardware/<mac>/radioInfo/`.
- Full Thetis-parity rate lists: P1 = 48/96/192 kHz (plus 384 on
  RedPitaya); P2 = 48/96/192/384/768/1536 kHz. Default 192 kHz.
- RX2 sample-rate combo stub (disabled, for Phase 3F).
- Inline reconnect banner on RadioInfoTab when the selected rate
  differs from the active wire rate.

### Fixes

- **RxDspWorker thread-safety:** buffer-size fields now atomic; audio
  thread no longer races the main-thread control path.
- **RxDspWorker accumulator drain** scaled to per-rate `in_size` so
  DDC sample-rate changes don't starve the WDSP feed.
- **Setup dialog** no longer overrides `selectPage()` in `showEvent`,
  so right-click-to-page works as intended.

## [0.1.7] - 2026-04-16

RedPitaya / Hermes P1 protocol fixes driven by pcap analysis in issue #38,
plus Windows D3D11 container lifecycle fixes from issue #42.

### Fixed
- **P1 RedPitaya / Hermes family (#38):** restore DDC3 NCO command on the wire.
  The `kRxC0Address` lookup table was missing the `0x0A` address entry,
  which caused bank 6 (RX4 / DDC3 frequency) to be dropped and bank 9
  (RX7 / DDC6) to alias onto bank 10's `0x12` TX-drive slot. Verified
  byte-for-byte against Thetis `networkproto1.c` cases 6 and 9.
- **P1 EP2 send cadence (#38):** host→radio packets are now paced by a
  dedicated 2 ms `Qt::PreciseTimer` with a `QElapsedTimer`-driven catch-up
  loop, yielding ~200-400 pps (target: Thetis' 380.95 pps from its 48 kHz
  audio clock). Previously we ran ~40 pps, starving the radio's audio DAC
  and stretching the 17-bank C&C round-robin to ~213 ms per cycle.
- **Windows container float/dock rendering** — five interlocking issues in
  the meter container lifecycle on Windows D3D11 QRhi (#42):
  - HWND collision on reparent — `E_ACCESSDENIED` →
    `DXGI_ERROR_DEVICE_REMOVED` cascade from the old `MeterWidget`
    lingering under the parent HWND during `setParent()`. Old widget now
    detaches synchronously (`hide()` + `setParent(nullptr)`) before
    `deleteLater`; `ContainerManager` swaps the meter around each reparent
    so no `WA_NativeWindow` child is reparented.
  - First float landed at `(0,0)` behind the main window —
    `FloatingContainer::ensureVisiblePosition()` centers the form on the
    anchor's screen when saved geometry is missing, at origin, or
    off every connected screen.
  - Use-after-free in `MeterPoller` — raw `MeterWidget*` targets dangled
    when the reparent-swap destroyed them. Switched to
    `QVector<QPointer<MeterWidget>>` with null-guarded `poll()`.
  - Progressive stack compression across reparent cycles —
    `inferStackFromGeometry` merged touching row intervals into one
    cluster, collapsing N bar rows onto stack slot 0. Require strict
    overlap > 0.002 before merging.
  - Empty band below the meter stack on resizable containers — Thetis's
    fixed `kNormalRowHNorm = 0.05` assumes fixed-aspect containers; stack
    now shares `(1 − bandTop)` equally among rows. 24 px floor preserved
    for small widgets.

### Known issues
- **ANAN MM preset** still shows empty space below the needle panel when
  no bar rows are added. Thetis-faithful fix (per-container `AutoHeight`)
  is scoped in
  [`docs/architecture/meter-autoheight-plan.md`](docs/architecture/meter-autoheight-plan.md).
- Exit-time segfault (exit 139) reproducible on close; root cause still
  unknown, not implicated by the #42 changes.
- One `QRhiWidget: No QRhi` warning per meter install cycle; benign,
  under investigation.

## [0.1.7-rc1] - 2026-04-16

Radio model selector and P1 protocol completion for RedPitaya and non-standard
Hermes devices.

### Added
- **Radio model selector** — per-MAC model override in ConnectionPanel detail panel;
  users can select their actual radio model (e.g. "Red Pitaya") when the discovery
  board byte is ambiguous (e.g. reports "Hermes")
- **HardwareProfile engine** — port of Thetis clsHardwareSpecific.cs; maps 16
  HPSDRModel variants to correct ADC count, BPF, supply voltage, and board capabilities
- **P1 C&C full 17-bank round-robin** — port of Thetis networkproto1.c WriteMainLoop
  cases 0-17; was only sending 3 of 17 banks with zeros for dither/random/preamp/Alex filters
- **Radio Setup menu item** wired (was disabled NYI)
- Auto-reconnect loads persisted model override from AppSettings

### Fixed
- P1 bank 0 C3/C4 sent all zeros — dither, random, preamp, duplex now populated
- P1 reconnect log spam (30-80 duplicate "Reconnected" lines per cycle → 1 line)
- SaturnMKII board byte falls back to Saturn-family model instead of Hermes
- Null-guard on HardwareProfile caps pointer in P1/P2 connection setup

### Docs
- Phase 3I-RP design spec and implementation plan


## [0.1.6] - 2026-04-16

About dialog and built-in issue reporter.

### Added
- **About dialog** (Help → About NereusSDR) — version, build info, Qt/WDSP
  versions, heritage credits, license text, GPG fingerprint. Accessible from
  the menu bar and wired into the build.
- **AI-assisted issue reporter** — click the lightbulb (💡) in the menu bar
  corner to file a bug report or feature request directly from NereusSDR.
  Structured prompts guide you through the fields; submits to the GitHub
  issue tracker using the `bug_report.yml` and `feature_request.yml`
  templates.

### Tests
- AboutDialog content verification tests


## [0.1.5] - 2026-04-16

Major feature release: RX DSP parity, step attenuator, Clarity adaptive display.

### Phase 3G-10: RX DSP Parity + AetherSDR Flag Port (Complete)

- **10 WDSP feature slices wired** end-to-end through SliceModel → RadioModel → RxChannel → WDSP: AGC advanced (threshold/hang/slope/attack/decay), EMNR (NR2), SNB, APF (SPCW module), squelch (SSB/AM/FM 3-variant), mute/audio pan/binaural, NB2 advanced params, RIT/XIT client offset, frequency lock, mode containers (FM OPT/DIG/RTTY)
- **Per-slice-per-band persistence** via AppSettings (`Slice<N>/Band<key>/*` namespace) with legacy key migration and band-change save/restore cycle
- **VfoWidget rewrite** — 4-tab layout (Audio/DSP/Mode/X-RIT), 4×2 DSP toggle grid, AGC 5-button row, S-meter level bar with dBm readout and cyan→green gradient, mode containers (FM/DIG/RTTY), tooltip coverage test
- **AGC-T ↔ RF Gain bidirectional sync** — both control the same WDSP max_gain; `GetRXAAGCTop`/`GetRXAAGCThresh` readback prevents audio-breaking gain runaway
- **S-meter wired** to VfoWidget via MeterPoller `smeterUpdated` signal
- **Thetis-first tooltip sweep** — 18 controls updated with verbatim Thetis text and source-line citations
- 17 new test files, widget library (VfoLevelBar, ScrollableLabel, GuardedComboBox, TriBtn, ResetSlider, CenterMarkSlider)

### Phase 3G-13: Step Attenuator & ADC Overload (PR #34)

- **StepAttenuatorController** with Classic + Adaptive auto-attenuation modes, hysteresis, and per-MAC persistence
- **P1/P2 ADC overflow detection** — `adcOverflow` signal from frame parsers, OVL status badge in RxApplet
- **Setup → General → Options** page for step ATT configuration
- **RxApplet ATT/S-ATT row** with per-model preamp items from Thetis `SetComboPreampForHPSDR`
- 9 controller tests

### Phase 3G-9: Display Refactor

- **Clarity Blue waterfall palette** — full-spectrum rainbow with deep-black noise floor
- **ClarityController** with cadence, EWMA, deadband + NoiseFloorEstimator percentile estimator
- **Reset to Smooth Defaults** button on Setup → Display → Spectrum Defaults
- **Re-tune button + Clarity status badge** in spectrum overlay panel
- **Per-band Clarity memory** in BandGridSettings
- **Zoom persistence** — visible bandwidth saved/restored across restarts
- Thetis-cited tooltips on 47 Spectrum/Waterfall/Grid setup controls
- SliderRow/SliderRowD TDD factory helpers for setup pages

### Phase 3G-11: P1 Field Fixes

- **P1 VFO frequency encoding** — encode as raw Hz, not NCO phase word

### Fixes

- Bidirectional AGC-T ↔ RF Gain sync (prevents audio runaway)
- AGC-T slider direction inverted to match Thetis (right = more gain)
- RF Gain slider removed (redundant with AGC-T)
- S-meter continuous gradient cyan→green at S9 boundary
- S-meter dBm readout with clipping fix
- NYI overlays removed on live-wired FM/DIG/RTTY containers
- Mode tab layout matches AetherSDR (combo fills row)
- DSP grid equal-column stretch eliminates gap
- AGC-T right-click context menu → Setup dialog
- Windows linker ODR violation resolved (TriBtn consolidation)
- Waterfall AGC margin widened 3→12 dB

## [0.1.4] - 2026-04-14

Bug-fix release. Improves Hermes (Protocol 1) startup reliability and
fixes two Windows-only crashes seen on cold launch / shutdown. Also
relaxes an over-aggressive board firmware-version gate that rejected
some legitimate radios.

### Fixes
- **P1 Hermes DDC priming** — prime the DDC before sending `metis-start`
  on Hermes-class boards, declare `nddc=2`, and alternate the TX/RX1
  command banks. Resolves silent RX after connect on Hermes.
- **Windows startup/shutdown crashes** — fix two latent crashes that
  surfaced on Windows clean installs (cold-start init order and
  shutdown teardown ordering).
- **Connect: drop unattested firmware floors** — remove per-board
  firmware-version minimums that were never verified against
  real-world firmware ranges and were blocking valid radios.


## [0.1.3] - 2026-04-13

Hotfix for the v0.1.2 Windows artifacts. Linux and macOS builds are
functionally identical to v0.1.2 — only the Windows installer and
portable ZIP have changed.

### Fixes
- **Windows NSIS installer** — fixed missing Qt6 DLL packaging that
  caused startup crash on clean Windows installs.
- **Windows portable ZIP** — same DLL fix as installer.


## [0.1.2] - 2026-04-13

First full cross-platform alpha release with all 6 build artifacts.

### Added
- Linux AppImage (x86_64 + aarch64)
- macOS Apple Silicon DMG
- Windows portable ZIP + NSIS installer
- Consolidated `release.yml` CI pipeline
- `/release` skill for Claude Code

### Fixes
- Linux: added `qtshadertools` build dependency
- Linux aarch64: disabled GPU spectrum (no Vulkan in CI container)
- release.yml: fixed artifact upload paths


## [0.1.1] - 2026-04-12

Internal milestone — Phase 3I Radio Connector complete.

## [0.1.0] - 2026-04-10

Initial alpha — RX-only, Protocol 2 (ANAN-G2), spectrum + waterfall,
container meter system, 12 applets, 47-page setup dialog.
