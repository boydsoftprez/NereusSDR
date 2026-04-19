# Changelog

## [Unreleased]

### License compliance (2026-04-18 binary-distribution sweep)

- **Bundled full licence text with the binary.** Added `GPLv2.txt`,
  `GPLv3.txt`, and `LGPLv3.txt` (verbatim upstream copies) under
  `packaging/third-party-licenses/`. Previously the per-dependency
  notice files (`fftw3.txt`, `qt6.txt`, `wdsp.txt`) pointed at gnu.org
  URLs; GPLv2 §1 / LGPLv3 §4 require the text to *accompany* the
  binary, not merely be linked. These three files are installed into
  every release artifact via the existing CMake install rule.
- **Upstream-project notice files.** Added `thetis.txt` (Thetis
  ramdor/Thetis upstream), `mi0bot-thetis.txt` (mi0bot/Thetis-HL2 fork),
  and `aethersdr.txt` (ten9876/AetherSDR template) — so binary-only
  recipients see the upstream copyright chain for code compiled into
  NereusSDR, not just the runtime-linked dependencies. Per-file
  attribution remains in the source-file headers and
  `docs/attribution/`.
- **Written source offer.** Added `SOURCE-OFFER.txt` formalising
  GPLv3 §6(d) + §6(b): primary corresponding source at the tagged
  GitHub release, plus a three-year written offer; plus LGPLv3 §4
  Qt-upstream source pointer.
- **Refreshed per-dependency notices.** `fftw3.txt`, `qt6.txt`,
  `wdsp.txt` now point at the bundled full texts instead of URLs, and
  name the §5(b) upgrade / dynamic-link mechanism explicitly.
- **Expanded README index.** `packaging/third-party-licenses/README.md`
  now enumerates every file, the two dependency categories
  (linked-in vs. ported-from), and where the directory lands in each
  release artifact.

### Fixed
- **Hardware Config → Radio Info sample-rate wiring (#35):** sample-rate combo
  now actually drives the wire rate used on the next connection (previously
  emitted to AppSettings but was never read back; `RadioModel::connectToRadio`
  hardcoded `isP1 ? 192000 : 768000`). Active-RX count spinbox now emits on
  change (its `valueChanged` signal was not connected).

### Changed
- **Sample-rate combo full Thetis-parity rates (#35):** per-protocol selections
  now match Thetis setup.cs:866. P1 = 48/96/192 kHz (plus 384 on RedPitaya),
  P2 = 48/96/192/384/768/1536 kHz. Default is 192 kHz instead of the previous
  "always max rate" UX.
- **Hardware Config RX2 sample-rate stub (#35):** RX2 combo added as a disabled
  control for Phase 3F multi-panadapter. Thetis force-equal rules (setup.cs:7065-7073,
  7155-7156) documented at the combo creation site.
- **RadioInfoTab reconnect banner (#35):** shows a "⚠ Reconnect to apply new
  sample rate" warning when the combo differs from the active wire rate. Disappears
  when live-apply lands in the follow-up PR.

## [0.2.0] - 2026-04-17

### License compliance remediation

- Restored Thetis copyright, GPLv2-or-later permission, per-contributor
  attribution, and Samphire dual-licensing notices across all derived
  source files.
- Added per-file modification history blocks disclosing reimplementation
  date, author, and AI-assisted transformation, per GPL notice-preservation
  requirements.
- Published `docs/attribution/THETIS-PROVENANCE.md` — complete
  file-by-file provenance inventory covering ramdor/Thetis,
  mi0bot/Thetis-HL2, and TAPR WDSP derivations.
- Replaced `resources/meters/ananMM.png` and `cross-needle*.png` with
  original NereusSDR artwork (generators committed under `tools/`);
  audited remaining assets and documented in
  `docs/attribution/ASSETS.md`.
- Re-framed README to accurately describe NereusSDR as a C++20/Qt6 port
  of Thetis within the FlexRadio / OpenHPSDR / Thetis lineage.
- Added non-negotiable license-preservation rule to the source-first
  porting protocol in CLAUDE.md and to CONTRIBUTING.md.
- No functional changes.

**Binaries from v0.1.1–v0.1.7 remain withdrawn and should not be
redistributed.**

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
