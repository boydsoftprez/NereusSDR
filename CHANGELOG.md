# Changelog

## [Unreleased]

### Added (Phase 3G RX Epic Sub-epic E — Waterfall rewind / scrollback)

- **Waterfall rewind / scrollback.** Drag up on the right-edge time-scale
  strip to pause the waterfall and scroll backward through up to ~8 minutes
  of history at default refresh, or 20+ minutes at any period ≥ 73 ms. A
  bright-red "LIVE" button appears when paused; one click returns to
  real-time. Tick labels switch from elapsed seconds (live) to absolute
  UTC timestamps (paused). Configurable depth via Setup → Display →
  Waterfall (60 s / 5 min / 15 min / 20 min, default 20 min, persisted as
  `DisplayWaterfallHistoryMs`).

  Ported from AetherSDR `main@0cd4559` for the live machinery and the
  unmerged PR `aetherclaude/issue-1478@2bb3b5c` (#1478) for the row cap
  and 250 ms resize debounce. NereusSDR raises the cap from upstream's
  4 096 rows to 16 384 rows (~128 MB at 2 000 px wide) so the default
  30 ms refresh delivers ~8 minutes of effective rewind instead of ~2.
  A disk-spool tier (to extend beyond the in-memory cap at the fastest
  refresh rates) is deferred to Phase 3M (Recording).

  Diverges from upstream by clearing history on band/zoom largeShift,
  keeping the rewind window coherent with the current band — see
  [`docs/architecture/phase3g-rx-epic-e-waterfall-scrollback-plan.md`](docs/architecture/phase3g-rx-epic-e-waterfall-scrollback-plan.md)
  §authoring-time #3.

  Closes the last item in the Phase 3G RX Experience Epic.

## [0.2.3] - 2026-04-24

This release rounds out the **Phase 3G RX experience epic** (AetherSDR-style
dBm scale strip, full Thetis Noise Blanker family port, cross-platform
7-filter Noise Reduction stack), brings **Alex antenna integration** to
feature-complete (3P-I-a + 3P-I-b — full `Alex.cs:310-413` composition,
RX-only antennas with SKU-driven labels, P1 bank0 / P2 Alex0 wire-locked),
and adds a **Linux PipeWire-native audio bridge** that supersedes the 0.2.2
pactl/PulseAudio fallback on PipeWire systems. v0.2.3 also addresses three
ANAN-G2 bench bugs caught after 0.2.2 shipped (P2 first-packet HPF/LPF
mismatch, band-crossing antenna-label desync, AlexController per-band
persistence).

### Added (Phase 3G RX Epic Sub-epic A — dBm scale strip)
- New AetherSDR-style **dBm scale strip** rendered on the right edge of the
  spectrum widget. Wheel inside the strip zooms the dynamic range live;
  arrow clicks at top/bottom nudge the floor or ceiling and persist back to
  `PanadapterModel`. Hover shows a cursor crosshair with the dBm value at
  the pointer. The strip honors the calibration offset, so on-screen labels
  reflect the actual signal level the user reads, not raw FFT bins. New
  `DisplayDbmScaleVisible` toggle in Setup → Display lets users hide the
  strip entirely. 17-row manual verification matrix at
  `docs/architecture/phase3g-rx-epic-a-verification.md`.
- Pure dBm-strip geometry helpers split out of the renderer with their own
  unit tests so layout changes are caught at build time.

### Added (Phase 3G RX Epic Sub-epic B — Noise Blanker family)
- Full port of Thetis's three-filter NB stack: **NB** (`nob.c`, Whitney),
  **NB2** (`nobII.c`, second-gen), and **SNB** (`snb.c`, spectral). New
  `NbFamily` wrapper on `RxChannel` owns the WDSP create/destroy lifecycle
  and tuning. The VFO Flag gets a cycling NB button (`Off → NB → NB2 → Off`)
  mirroring Thetis `chkNB` tri-state (`console.cs:43513-43560 [v2.10.3.13]`).
- RxApplet gains Threshold (0-100) + Lag (0-20 ms) sliders, scaled to match
  `setup.cs:8572, 16236 [v2.10.3.13]`. Setup → DSP → NB/SNB page is now
  interactive (previously greyed) and wires global defaults via AppSettings.
- `NbMode` + full `NbTuning` struct persist per-slice-per-band under
  `Slice<N>/Band<key>/Nb{Mode,Threshold,TauMs,LeadMs,LagMs}`; `SnbEnabled`
  session-level. Defaults pinned to Thetis `cmaster.c:43-68 [v2.10.3.13]`
  byte-for-byte (`nbThreshold=30.0`, times=0.1 ms, `backtau=0.05 s`,
  `nb2MaxImpMs=25.0`).
- New tests: `tst_nb_family`, `tst_slice_nb_persistence`, plus a rewritten
  `tst_rxchannel_nb2_polish` cover mode cycling, default parity, and
  per-band round-trip.

### Added (Phase 3G RX Epic Sub-epic C-1 — 7-filter NR stack)
- Full noise-reduction stack on the VFO flag DSP grid: **NR1** (ANR, WDSP),
  **NR2** (EMNR including post2 cascade), **NR3** (RNNR/rnnoise with
  user-loadable .bin models), **NR4** (SBNR/libspecbleach), **DFNR**
  (DeepFilterNet3 — cross-platform neural NR), **MNR** (Apple Accelerate
  MMSE-Wiener — macOS-only native), and **ANF**. BNR (NVIDIA Broadcast) is
  intentionally deferred (gRPC/NIM transport out of scope; tracked for
  follow-up).
- Setup → DSP → NR/ANF page with sub-tabs for NR1, NR2, NR3, NR4, DFNR,
  MNR, and ANF. NR1-NR4 sub-tabs mirror the Thetis `setup.designer.cs`
  layout byte-for-byte. The MNR sub-tab gains all 6 tuning knobs (Strength /
  Aggressiveness / Floor / Alpha / Bias / Gsmooth) with factory-default
  Reset buttons. The DFNR sub-tab uses AetherSDR-verbatim defaults
  (AttenLimit 100 dB, PostFilterBeta 0).
- Right-clicking any NR button on the VFO flag opens a `DspParamPopup` (port
  of AetherSDR's pattern) with 3-6 quick-control sliders plus a "More
  Settings…" entry point that deep-links into the matching Setup sub-tab.
- Per-slice NR independence via `SliceModel::setActiveNr` mutual exclusion
  (only one of NR1/NR2/NR3/NR4/DFNR/MNR active per slice at a time; ANF is
  independent and can stack).
- Vendored Xiph rnnoise (BSD-3) and libspecbleach (LGPL-2.1) into the WDSP
  build tree. Ported Thetis `rnnr.c` + `sbnr.c` (GPLv2+ + MW0LGE
  dual-license) into `third_party/wdsp/src/`.
- Bundled `Default_large.bin` (3.4 MB) and `Default_small.bin` (1.5 MB)
  rnnoise models plus `DeepFilterNet3_onnx.tar.gz` (7.6 MB) into every
  release artifact (Linux AppImage × 2 archs / macOS DMG / Windows ZIP +
  NSIS).
- New `ModelPaths` helper (`src/core/ModelPaths.{h,cpp}`) resolves rnnoise
  + DFNR model paths per platform install layout (Resources/ on macOS,
  share/NereusSDR/ on Linux, adjacent on Windows, plus dev-build fallbacks).
- New `setup-deepfilter.{sh,ps1}` scripts at the repo root build the
  DeepFilterNet3 Rust libdf for local development. CI workflows install
  Rust + invoke the appropriate script before `cmake -B build` so release
  artifacts always carry the runtime library.
- The 4×2 DSP toggle grid on the VFO Flag was rewritten as a 3×4 grid that
  mirrors AetherSDR's pattern exactly: NR mutex (NR1-4 + DFNR + MNR
  alternating off the same row) + NB mutex + ANF/SNB independent toggles.
  Buttons are 63×26 and fully fill the flag width.

### Added (Phase 3G RX Epic — band-button auto-mode, #118)
- New `RadioModel::onBandButtonClicked` handler funnels every band-button
  entry point (VFO Flag, RxApplet bands row, container `BandButtonItem`,
  spectrum overlay Band flyout) through one routing path. Closes #118.
- New `BandDefaults` seed table provides the per-band default mode + filter
  preset on the user's first visit to each band; subsequent visits load
  whatever the user last set there.
- `SliceModel::hasSettingsFor(Band)` probe + `Band::bandFromName` helper
  back the seed-vs-restore decision and round-trip with case-sensitivity
  pinning tests.

### Added (Phase 3P-I-a — Alex antenna integration core, #116)
- `AlexController`'s per-band antenna state now reaches the radio via
  `RadioConnection::setAntennaRouting`. Three triggers fire the pump:
  `AlexController::antennaChanged`, `PanadapterModel::bandChanged`, and
  `onConnectionStateChanged(Connected)`. All 5 writeable antenna surfaces
  (VFO Flag, RxApplet, Setup-grid, SpectrumOverlayPanel combos,
  AntennaButtonItem) funnel through `AlexController` as the single source
  of truth; `SliceModel` caches from the controller via
  `refreshAntennasFromAlex` so VFO/applet labels stay coherent on band
  changes. **Closes [#98](https://github.com/boydsoftprez/NereusSDR/issues/98).**
- `BoardCapabilities` gains `hasAlex2`, `hasRxBypassRelay`, and
  `rxOnlyAntennaCount` fields with cites to Thetis `setup.cs:6228` and
  `HPSDR/Alex.cs:377` (`//DH1KLM` / `//G8NJJ` author tags preserved
  verbatim).
- New `AntennaLabels` helper (`src/core/AntennaLabels.{h,cpp}`) is the
  single source for the ANT1/ANT2/ANT3 label list; returns empty on boards
  without Alex so UI sites can `setVisible(!empty)`. Replaces ~10
  hardcoded `QStringList{"ANT1","ANT2","ANT3"}` sites.
- New `PopupMenuStyle.h` defines the universal `kPopupMenu` dark-palette
  `QMenu` stylesheet. Every antenna popup (VFO Flag + RxApplet) now
  applies it, fixing Ubuntu 25.10 GNOME dark-on-dark menu rendering.
- `RadioConnection::setAntennaRouting(AntennaRouting)` pure-virtual
  replaces the deprecated `setAntenna(int)`. `AntennaRouting` carries
  RX/TX antenna numbers + `caps.hasAlex` so the protocol layer can zero
  the antenna bits on HL2/Atlas. Both P1 and P2 implementations updated
  with byte-for-byte wire-lock tests.
- VFO Flag, RxApplet, and SpectrumOverlayPanel antenna UI hidden on
  HL2 / Atlas (`!caps.hasAlex || antennaInputCount < 3`); these SKUs
  previously saw zombie ANT2/ANT3 buttons that wrote nothing. Matches
  Thetis's behavior of hiding antenna controls for boards with no
  Alex relay. `AntennaButtonItem` (meter) click is a silent no-op on
  `!caps.hasAlex`.
- New manual verification matrix at
  [`docs/architecture/antenna-routing-verification.md`](docs/architecture/antenna-routing-verification.md)
  covers per-SKU VFO Flag / RxApplet / Setup grid / spectrum overlay /
  AntennaButtonItem / band-change reapply / pcap verification.
- 17 new tests: `tst_antenna_routing_model` (4), `tst_alex_controller`
  (+4), `tst_ui_capability_gating` (5), `tst_popup_style_coverage` (1),
  plus expanded byte-lock cases on `tst_p1_codec_standard` and
  `tst_p2_codec_orionmkii`.

### Added (Phase 3P-I-b — RX-only antennas + SKU labels + XVTR, #117)
- New `SkuUiProfile` (`src/core/SkuUiProfile.{h,cpp}`) — per-`HPSDRModel`
  UI overlay describing RX-only labels + checkbox visibility. 14-case
  switch ports Thetis `setup.cs:19832-20375` exactly: Hermes/ANAN10 →
  "RX1/RX2/XVTR", ANAN100-class → "EXT2/EXT1/XVTR", 7000D/G2/etc. →
  "BYPS/EXT1/XVTR". Pure UI overlay; doesn't touch the wire.
- `AlexController` gains 6 flags (`rxOutOnTx` / `ext1OutOnTx` /
  `ext2OutOnTx` mutual-exclusion trio + `rxOutOverride` +
  `useTxAntForRx` + `xvtrActive`), ported from Thetis `Alex.cs:61-66`
  static fields. 5 persisted per-MAC; `xvtrActive` session-scoped.
  Mutual-exclusion matches Thetis `setup.cs:15420-16505` handlers.
- P1 bank0 C3 bits 5-7 now encode `AntennaRouting.rxOnlyAnt` + `rxOut`,
  byte-locked against Thetis `networkproto1.c:453-468` +
  `netInterface.c:479-481`. Both `P1CodecStandard` and `P1CodecHl2` (which
  has its own bank0, not inherited) updated.
- P2 Alex0 bits **8-11** (not 27-30 as the original plan said — bit 27 is
  `_TR_Relay`; corrected against authoritative Thetis `network.h:263-307`)
  encode rxOnlyAnt (bits 8/9/10 for XVTR_Rx_In / Rx_2_In / Rx_1_In) and
  rxOut (bit 11 = K36 RL17 RX-Bypass-Out relay).
- `RadioModel::applyAlexAntennaForBand(Band, bool isTx=false)` now ports
  the full Thetis `Alex.cs:310-413 UpdateAlexAntSelection` composition
  (minus MOX coupling + Aries clamp, both deferred to Phase 3M-1): isTx
  branch with Ext1/Ext2OnTx mapping, xvtrActive gating (derived from
  `band == Band::XVTR`), rx_out_override clamp. 6 new signal triggers
  wire flag changes to reapply composition.
- Setup → Antenna Control tab gains 5 new TX-bypass checkboxes (RX
  Bypass on TX, Ext 1 on TX, Ext 2 on TX, Disable RX Bypass relay,
  Use TX antenna for RX). SKU-driven visibility + per-SKU Ext2-on-TX
  tooltip variants. RX-only column sub-headers retargeted per SKU.
- Setup → Antenna → Alex-2 Filters sub-tab now gates on `caps.hasAlex2`
  (replaces the 3P-F hardcoded board check + hides the tab outright on
  non-BPF2 boards instead of leaving it gray).
- The VFO Flag gains an optional grey **BYPS** 3rd button between blue RX
  and red TX antenna buttons, double-gated on `caps.hasRxBypassRelay &&
  SkuUiProfile.hasRxOutOnTx`. Toggles `AlexController::rxOutOnTx` with
  bidirectional sync to the Setup checkbox.
- `AntennaButtonItem` meter gains `setHpsdrSku(HPSDRModel)` — button
  indices 3-5 (Thetis "Aux1/Aux2/XVTR" slots) now show SKU-specific
  labels from `SkuUiProfile.rxOnlyLabels`.
- 32 new tests across `tst_sku_ui_profile`, `tst_antenna_labels`,
  `tst_alex_controller` (flag mutual-exclusion), the codec byte-lock
  suites, and `tst_antenna_routing_model` integration cases.
- Verification doc: appended §7 per-SKU matrix (RX-only / XVTR /
  Ext-on-TX) + §8 authoritative P1 bank0 C3 and P2 Alex0 bit-layout
  reference.

### Added (Phase 3O — Linux PipeWire-native audio bridge)
- Native PipeWire audio bridge on Linux with live latency telemetry,
  separate sidetone / MON sinks, and per-slice output routing. Falls
  back to pactl / PulseAudio where PipeWire isn't present.
- New `Setup → Audio → Output` page (Primary, Sidetone, Monitor cards
  + per-slice routing section). Real PipeWire terminology throughout:
  `quantum`, `node.name`, `media.role`, `xruns`, `process-cb CPU`,
  `stream state` (no cosmetic relabeling).
- Setup → Audio → VAX page rebuilt: VAX is now a virtual source the
  system sees, not a device the user picks. Per-channel Rename + Copy
  node-name buttons.
- New `AudioBackendStrip` header on every Setup → Audio sub-page —
  shows the detected backend (PipeWire / Pactl / None) and Rescan +
  Open-logs buttons.
- `Help → Diagnose audio backend…` menu item opens the same
  detection-state dialog used at first launch when no audio backend
  is detected. The first-launch `VaxLinuxFirstRunDialog` walks the
  user through installing PipeWire / Pactl with copy-pasteable
  package commands.
- New optional build dependency on Linux: `libpipewire-0.3-dev`
  ≥ 0.3.50 (auto-detected via `pkg-config`). Without it the build
  still succeeds and the legacy LinuxPipeBus FIFO path is used.
- Lock-free SPSC byte ring (`AudioRingSpsc`) for cross-thread audio
  delivery, plus an RAII `PipeWireThreadLoop` wrapper over
  `pw_thread_loop` and a `QAudioSinkAdapter` that lets the unified
  `IAudioBus` interface front the QAudioSink fallback path.
- Manual verification matrix:
  `docs/architecture/linux-audio-verification/README.md`.

### Added — UI / theme polish
- App-wide dark `QPalette` + baseline QSS bootstrap installed at
  startup so every window — not just the spectrum / VFO chrome —
  renders against the correct palette on Linux. Fixes the gray-on-gray
  Setup dialog that some Ubuntu users saw on first launch.
- Setup → DSP → NR/ANF page: spinboxes replaced with sliders per the
  user-facing directive that NR controls should feel continuous.
  DFNR + MNR sliders use the shared `addSliderRow` /
  `addDoubleSliderRow` helpers so all NR sub-tabs render identically.

### Build
- Rust + cargo are now required for DFNR builds. CI installs
  `dtolnay/rust-toolchain@stable`. Local devs run
  `./setup-deepfilter.sh` (or `./setup-deepfilter.ps1` on Windows)
  before `cmake -B build`. `ENABLE_DFNR` auto-OFFs if the libdf is
  absent at configure time, so the build still succeeds without Rust
  — just without DFNR.
- `qt6-shadertools-dev` added to the Linux build prerequisites for
  Ubuntu 25.10 (Qt 6.8).
- CI installs ALSA + JACK + PipeWire dev headers on Linux so
  PortAudio's ALSA / JACK backends are compiled in (`PA_USE_ALSA` +
  `PA_USE_JACK` forced on).
- `NEREUS_HAVE_PIPEWIRE` is now propagated to `NereusSDRObjs` consumers
  so unit tests under `build/tests/` see the same compile-time gates as
  the app.
- Release artifacts grow ~12 MB per platform (DFNet3 model + rnnoise
  large/small models bundled).

### Fixed
- **Initial `CmdHighPriority` packet on P2 connect sent a DDC frequency
  that didn't match the HPF/LPF bits.** `P2RadioConnection::connectToRadio`
  unconditionally reset `m_rx[2].frequency` to 3865000 (80m LSB) after
  `RadioModel` had queued `setReceiverFrequency` with the persisted VFO.
  Worker-thread FIFO order made the hardcoded seed overwrite the real
  value, so the first packet told the radio to tune DDC2 to 80m while
  enabling 13 MHz HPF. Audio stayed silent until the user moved the
  panadapter. Fixed by only seeding the default when
  `m_rx[2].frequency == 0`. Caught on ANAN-G2 (Saturn) bench testing
  by KG4VCF.
- **Band-crossing reapplied the wire antenna but kept the old UI label.**
  `SliceModel::frequencyChanged` → `applyAlexAntennaForBand` was missing
  the `refreshAntennasFromAlex` call that the click-driven path had —
  so the relay switched but the VFO Flag / RxApplet buttons showed the
  previous band's antenna. Fixed + regression test
  `band_crossing_refreshes_slice_labels`.
- **`AlexController` per-band antenna selection didn't persist across
  app restart.** `AlexController::save()` had zero production call
  sites — only tests invoked it. User would pick ANT2 on 20m, quit,
  relaunch, and see ANT1 again. Hooked `antennaChanged` +
  `blockTxChanged` into the existing `scheduleSettingsSave()` coalescer
  via an `m_alexControllerDirty` flag so the 14-per-band emit burst
  during `load()` collapses to a single write. Also flushes on
  `teardownConnection()`.
- **SpectrumOverlayPanel RX Ant / TX Ant combos in the ANT flyout now
  actually change the antenna.** Previously the combos rendered but
  had no `currentTextChanged` handler — zombie controls. Wired through
  slice 0 via the same pattern as the VAX Ch combo with
  `m_updatingFromModel` echo guard.
- **Latent 3P-I-a bug: `AlexController::setRxOnlyAnt(band, 0)` was
  clamped to 1** by the shared `clampAnt(v)` helper, but Thetis
  `Alex.cs:58` uses 0 as "none selected" (required by the RX
  composition logic). New `clampRxOnlyAnt(0..3)` helper allows the 0
  state; constructor default changed from 1 to 0; `load()` defaults +
  clamp updated. 3P-I-b's full composition now works as Thetis intended.
- **Latent 3P-F bug: `setAntennasTo1` no longer touches rxOnlyAnt**
  (Thetis `Alex.cs:72-77` is explicit: "the various RX bypass
  unaffected"). 3P-F wrote all 3 arrays to 1; combined with 3P-I-b's
  new 0 semantic this would have silently activated the RX-bypass
  relay in external-ATU compat mode.
- **MNR's MMSE prior-SNR computation had a dimensional bug** in the
  post-filter application (caught while wiring the 6-knob tuning
  surface). Fixed in `src/core/MnrFilter.cpp`. Worth flagging upstream
  to AetherSDR — maintainers may want to backport.
- **`VfoWidget` removed raw `delete` calls** that bypassed Qt parent
  ownership. Closes #113.
- **AudioEngine fallback now honors the requested `deviceName`** and
  falls back through the `QMediaDevices` enumeration in order, instead
  of jumping straight to the system default. Closes #112.
- Three antenna/BPF bench bugs caught on ANAN-G2 during 3P-I-a
  shakedown (double-connect post-sweep, container `BandButtonItem`
  click routing, lock-state short-circuit on `onBandButtonClicked`).
- Several PipeWire shakedown bugs surfaced during Ubuntu 25.10 testing:
  non-blocking ring write on the PW data thread (was blocking and
  triggering Mutter's RT-thread SIGKILL); RT-thread `qCInfo` removed
  from `probeSchedOnce` (same Mutter SIGKILL path); null-guard on
  `on_process` buffer pointers (PipeWire PAUSED-state crash); a
  pre-existing SEGFAULT in `stop()` that skipped bus teardown when not
  running.
- Restored `//G8NJJ` author tag in the VfoWidget port from
  `setup.cs:6277` and preserved `//G8NJJ //MW0LGE //N1GP //DH1KLM`
  tags across the 3P-I-b ports — caught by the
  `verify-inline-tag-preservation.py` corpus drift gate.

### Changed
- `RadioModel` pushes the active slice's NR tuning state to its
  `RxChannel` whenever any of the 7 NR knobs changes (Task 19) — keeps
  WDSP/DFNR/MNR in sync with the model without requiring callers to
  reach into `RxChannel` directly.
- `RxChannel` consumes the `NbFamily` facade rather than juggling
  WDSP NB lifecycle directly. `WdspEngine` is no longer involved in
  per-channel NB create/destroy (it never should have been —
  duplicated lifecycle was a refactor leftover from 3-UI).

### Deprecated
- `RadioConnection::setAntenna(int)` — use
  `setAntennaRouting(AntennaRouting)`. Kept as a thin wrapper for one
  release cycle; scheduled for removal in 0.3.x.

### Internal
- New attribution corpus refresh (`thetis-author-tags.json` regenerated
  from upstream walk) + the standard `verify-inline-cites.py` baseline
  regression gate, `compliance-inventory.py --fail-on-unclassified`,
  and the cross-platform CI workflows that now build PipeWire +
  DeepFilterNet on Linux/macOS/Windows.

## [Unreleased]

## [0.2.2] - 2026-04-22

### Added
- New `--profile <name>` / `-p <name>` CLI flag for running multiple NereusSDR instances against different radios concurrently. Each profile gets its own isolated `NereusSDR.settings` + log directory under `profiles/<name>/`; profile names are validated against `[A-Za-z0-9_-]+` to prevent path traversal. FFTW wisdom stays machine-scoped (shared across profiles). Main window title gains a `[<profile>]` suffix. `SupportDialog` / `SupportBundle` now read the same profile-scoped log directory `main.cpp` writes to. Without `--profile`, behavior is byte-for-byte unchanged. Closes #100.
- New **Alex-1 Filters** per-row live-LED column mirroring Alex-2 — lights the matched HPF + LPF band for the current RX VFO. Ports Thetis `console.cs:setAlexHPF` / `setAlexLPF` first-match range logic including master-bypass and per-row bypass fallback. (Phase 3P-H)
- **ADC Overload** status-bar label on MainWindow (left of STATION). Text format matches Thetis verbatim: `"ADCi Overload"` with three-space separator, trimmed. Yellow at levels 1-3, red at level > 3, 2 s single-shot auto-hide timer restarts on each event (matches `ucInfoBar._warningTimer`). RxApplet's per-ADC OVL badges removed from the visible layout — the status-bar indicator is now authoritative. Ports `console.cs:21323, 21359-21389` + `ucInfoBar.cs:911-933`. (Phase 3P-H)
- New **attribution enforcement pipeline**: `scripts/discover-thetis-author-tags.py` walks upstream Thetis + mi0bot source, mechanically builds `docs/attribution/thetis-author-tags.json` (19 contributors discovered, with human-curated name/role fields). `scripts/verify-inline-tag-preservation.py` reads the corpus and fails commits that drop any developer-attribution tag (e.g. `//DH1KLM`, `//MW0LGE`) within ±10 port lines of a cite. `scripts/generate-contributor-indexes.py` regenerates `thetis-contributor-index.md` + `thetis-inline-mods-index.md` from the corpus + upstream walk — indexes now cover **151 upstream files, 2947 inline markers** (up from 30 files / 1458 markers). Old hand-curated indexes preserved as `-v020-snapshot.md`. CI runs `--drift` check so new upstream contributors block the PR until named. Pre-commit hook enforces strict mode locally. (Phase 3P-H attribution infra)
- **Attribution sweep commits** restore 74 historical `//DH1KLM` / `//MW0LGE` / `//G8NJJ` / `//W2PA` tags across 22 files (src + tests) that prior porting work had silently dropped. (Phase 3P-H sweep)
- New **Diagnostics → Radio Status** dashboard consolidating Thetis's piecemeal readouts (Front Console, PA Settings, main meter) into a single 5-card layout: PA Status (temp/current/voltage with bar meters), Forward/Reflected/SWR, PTT Source (pill row for MOX/VOX/CAT/Mic/CW/Tune/2-Tone + 8-event history), Connection Quality summary, Settings Hygiene actions. (Phase 3P-H)
- New **Diagnostics → Connection Quality**, **Settings Validation**, **Export / Import**, **Logs** sibling sub-tabs. Connection Quality shows live EP6/EP2 byte rates + throttle + sequence-gap counter from `HermesLiteBandwidthMonitor`. Settings Validation lists `SettingsHygiene::issues()` with per-row Reset / Forget / Re-validate; auto-refreshes on `issuesChanged()`. Export / Import round-trips the full AppSettings XML via QFile + QFileDialog. (Phase 3P-H)
- `RadioStatus` aggregator (`src/core/RadioStatus.{h,cpp}`) — PA temperature/current/forward-reflected/SWR/active PttSource + 8-event PTT history. Variant `multi-source`; data shapes from Thetis `console.cs` status handlers `[@501e3f5]`. Owned by `RadioModel`. (Phase 3P-H)
- `SettingsHygiene` (`src/core/SettingsHygiene.{h,cpp}`) — NereusSDR-original. Validates AppSettings against `BoardCapabilities` on connect and emits per-MAC mismatch records (severity Critical / Warning / Info). Powers the dashboard Reset / Forget actions. (Phase 3P-H)
- `PttSource` enum (`src/core/PttSource.h`) — NereusSDR-original. Tracks the trigger that asserted MOX so the Radio Status dashboard highlights the active source. (Phase 3P-H)
- `RadioModel` wires PA telemetry from both protocols into `RadioStatus` and runs `SettingsHygiene::validate()` on every `Connected` transition. P1 extracts PA fwd/rev/exciter/userADC0/userADC1/supply raws from `parseEp6Frame`'s C0 telemetry cases 0x08/0x10/0x18; P2 reads the same six fields from `processHighPriorityStatus` at documented offsets. Thetis per-board scaling (`computeAlexFwdPower` / `computeRefPower` / `convertToVolts` / `convertToAmps` `[@501e3f5]`) applied verbatim. (Phase 3P-H)
- **Live LED wire-up across earlier-phase pages**: Alex-2 Filters HPF + LPF LED rows highlight the active filter for the current RX frequency (mirrors Thetis `setAlex2HPF` / `setAlex2LPF` first-match ranges); OC Outputs pin-state LED row reflects `OcMatrix::maskFor(band, isTx)` and flips on MOX; HL2 I/O register state table polls `IoBoardHl2::registerValue(idx)` at 40 ms + bandwidth meter lit from `HermesLiteBandwidthMonitor`. (Phase 3P-H)
- Tests: `tst_ptt_source` (10), `tst_radio_status` (18), `tst_settings_hygiene` (11), `tst_p1_status_telemetry` (4), `tst_p2_status_telemetry` (3), `tst_radio_model_status_wiring` (4), `tst_alex2_live_leds` (5), `tst_oc_outputs_live_pins` (5), `tst_hl2_live_polling` (4). (Phase 3P-H)
- New **Hardware → Calibration** Setup page (renamed from PA Calibration). Hosts 5 Thetis-1:1 group boxes: **Freq Cal** (frequency spinbox + Start button + accuracy helptext), **Level Cal** (reference freq/level + Rx1/Rx2 6m LNA offsets + Start/Reset), **HPSDR Freq Cal Diagnostic** (correction factor with 9 decimal places + Using external 10 MHz ref toggle + 10 MHz factor), **TX Display Cal** (dB offset), and the existing **PA Current (A) calculation** group (preserved exactly). Per-MAC persistence under `hardware/<mac>/cal/`. (Phase 3P-G)
- `CalibrationController` model holds frequency correction factor (default 1.0), separate 10 MHz factor, using10MHzRef toggle, level offset, Rx1/Rx2 6m LNA offsets, TX display offset, PA current sensitivity/offset. `effectiveFreqCorrectionFactor()` returns factor10M when using10MHzRef, else factor. Per-MAC persistence. Ports `setup.cs:5137-5144; 14036-14050; 22690-22706; 14325; 17243; 18315` + `console.cs:9764-9844; 21022-21086` `[@501e3f5]`. (Phase 3P-G)
- New **Hardware → HL2 I/O** Setup page replacing the Phase 3I empty placeholder. Diagnostic surface for HL2 owners: connection status (LED + I2C address + last-probe timestamp), N2ADR Filter enable, register state table (8 principal registers from the IoBoardHl2 33-register set), 12-step state machine visualizer (current step highlighted), I2C transaction log (monospace listing of recent enqueue/dequeue with timestamps), bandwidth monitor mini (EP6/EP2 byte-rate progress bars + LAN PHY throttle indicator). Auto-hides for non-HL2 boards. (Phase 3P-E)
- `IoBoardHl2` model — 33-register enum + I2C TLV circular queue (32 slots per `network.h:MAX_I2C_QUEUE`) + 12-step UpdateIOBoard state machine with human-readable step descriptors. Closes the long-deferred Phase 3I-T12 work. (Phase 3P-E)
- `HermesLiteBandwidthMonitor` — HL2 LAN PHY throttle detection layered on a faithful port of mi0bot's `bandwidth_monitor.{c,h}` two-pointer byte-rate compute. Tracks ep6 ingress + ep2 egress, flags throttle when ep6 stays silent for 3 consecutive ticks while ep2 has traffic. (Phase 3P-E)
- New **Hardware → OC Outputs** Setup page with HF + SWL sub-sub-tabs (SWL placeholder). Hosts the full OC Outputs surface modeled 1:1 on Thetis: master toggles (Penny Ext Control, N2ADR Filter, Allow hot switching, Reset OC defaults), per-band RX matrix (14 × 7), per-band TX matrix (14 × 7), TX Pin Action mapping (7 pins × 7 actions per `enums.cs:443-457` TXPinActions), USB BCD output config, External PA control, and live OC pin state LED stubs (Phase H wires them to ep6 status). (Phase 3P-D)
- New `OcMatrix` model (per-band × per-pin × per-mode bit storage + TX pin action mapping) backs the OC Outputs page. Per-MAC persistence under `hardware/<mac>/oc/{rx,tx}/<band>/pin<n>` and `.../actions/pin<n>/<action>`. Owned by `RadioModel`. (Phase 3P-D)
- **RxApplet preamp combo now populates per board** from `BoardCapabilities::preampItemsForBoard()` at construction time instead of hardcoded items. HL2 / OrionMKII / Saturn / Angelia-no-Alex show the 4-item anan100d set (Off / -10 / -20 / -30 dB); Alex-equipped boards (Hermes, Angelia, Orion) show the 7-item on_off+alex set; Atlas no-Alex shows the 2-item on_off set. On connect the combo was already repopulated (PR #34); this closes the pre-connect gap. Matches Thetis `SetComboPreampForHPSDR` (`console.cs:40755-40825 [@501e3f5]`). (Phase 3P-C)
- **Hermes Lite 2 preamp combo corrected to 4 items** (anan100d set: 0dB / -10dB / -20dB / -30dB). Previously returned a 1-item table ("0dB" only). HL2 is not in Thetis's `SetComboPreampForHPSDR` switch (postdates it), but its LNA supports the same 4-level control as the anan100d set per mi0bot HL2 LNA design `[@c26a8a4]`. (Phase 3P-C)
- New `tst_preamp_combo` (17 assertions) locks per-board combo contents 1:1 with Thetis and verifies the 7-mode `PreampMode` enum has correct integer indices (0=Off, 1=On, 2–6=Minus10–50). Also verifies `RxApplet.preampComboItemCountForTest()` reflects board caps at construction. (Phase 3P-C)
- Hardware → Antenna / ALEX page split into three sub-sub-tabs — Antenna Control (placeholder for Phase F), **Alex-1 Filters**, and **Alex-2 Filters** — matching Thetis's General → Alex IA. Alex-1 page exposes per-band HPF/LPF bypass + edge editors and the **Saturn BPF1 panel** (gated on ANAN-G2 / G2-1K). Alex-2 page exposes per-band HPF/LPF for the RX2 board with live-LED indicator stubs (Phase H wires them). Per-MAC persistence under `hardware/<mac>/alex/...` and `.../alex2/...`. (Phase 3P-B)
- ADC OVL badge in RxApplet now splits into OVL₀ + OVL₁ for dual-ADC boards (Orion-MKII family — boards with `BoardCapabilities::p2PreampPerAdc=true`). Single-ADC boards (HL2, Hermes, Angelia) keep a single badge. (Phase 3P-B)
- Per-ADC RX1 preamp toggle exposed in RxApplet for OrionMKII-family boards; routes to byte 1403 bit 1 in P2 CmdHighPriority via the new `P2RadioConnection::setRx1Preamp(bool)`. (Phase 3P-B)
- ANAN-G2 / G2-1K can now use user-configured Saturn BPF1 band edges instead of Alex defaults via the new Hardware → Antenna/ALEX → Alex-1 Filters page; codec layer (`P2CodecSaturn`) reads the configured edges from `CodecContext.p2SaturnBpfHpfBits`. (Phase 3P-B)
- New **Hardware → Antenna / ALEX → Antenna Control** sub-sub-tab — third sub-sub-tab alongside Phase B's Alex-1/Alex-2 Filters. Per-band antenna assignment grid (14 bands × TX/RX1/RX-only ports 1-3) with Block-TX safety toggles. Backed by AlexController model. (Phase 3P-F)
- `AlexController` — per-band TX/RX/RX-only antenna arrays + Block-TX safety + SetAntennasTo1 force mode. Per-MAC persistence. Replaces Phase 3I Alex stubs. Ports `HPSDR/Alex.cs:30-106`. (Phase 3P-F)
- `ApolloController` — Apollo PA + ATU + LPF accessory state model (present/filterEnabled/tunerEnabled). Per-MAC persistence. Capability-gated on `BoardCapabilities::hasApollo` (only HPSDR-kit per Thetis source). Ports `setup.cs:15566-15590`. (Phase 3P-F)
- `PennyLaneController` — Penny external control master enable (companion to Phase D's OcMatrix which holds the per-pin/per-band masks). Per-MAC persistence. Ports `Penny.cs` + `console.cs:14899`. (Phase 3P-F)
- `BoardCapabilities` extended with `hasApollo` / `hasAlex` / `hasPennyLane` per-board enable rules per Thetis `setup.cs:19834-20205` board-model if-ladder. Source-first correction caught: only HPSDR-kit enables Apollo (NOT all ANAN family — every ANAN board explicitly disables it in Thetis). (Phase 3P-F)
- **VAX audio routing subsystem** — full port of AetherSDR's virtual-cable audio bus architecture as a NereusSDR-native multi-platform stack. Replaces the single-device QAudioSink output with `IAudioBus` abstract interface backed by 3 platform implementations: `PortAudioBus` (render + TX capture + device enumeration), `CoreAudioHalBus` (macOS HAL plugin bridge), `LinuxPipeBus` (pipewire/pulse module-pipe-source × 4 + module-pipe-sink × 1 under `nereussdr-vax-*` namespace). Per-slice VAX channel routing with up to 4 output channels + 1 TX input channel. (Phase 3O)
- `IAudioBus` abstract interface (`src/audio/IAudioBus.h`) — unified render / capture / enumerate API across platform backends. `AudioEngine` now holds `IAudioBus` instances rather than `QAudioSink` directly; platform selection at startup via `Q_OS_*`. (Phase 3O Sub-Phase 1 + 4)
- `MasterMixer` (`src/audio/MasterMixer.{h,cpp}`) — per-slice mute / volume / pan accumulation feeding the bus render path. Sits between slice audio output and the bus; master-mute API added to `AudioEngine`. (Phase 3O Sub-Phase 2)
- `PortAudioBus` (`src/audio/PortAudioBus.{h,cpp}`) — PortAudio v19.7.0 vendored via CMake FetchContent. Windows audio backend (the one that owns virtual-cable driver interop) plus fallback on macOS/Linux. Render-only minimal scaffold → host-API + device enumeration → TX capture (input stream + `pull()`). (Phase 3O Sub-Phase 3)
- `LinuxPipeBus` (`src/audio/LinuxPipeBus.{h,cpp}`) — PulseAudio / PipeWire named-pipe bus. Loads `module-pipe-source × 4` (RX destinations) + `module-pipe-sink × 1` (TX source) at startup under a `nereussdr-vax-*` namespace; render writes raw PCM to the pipes, capture reads from the sink's monitor. Auto-unloads modules on shutdown. (Phase 3O Sub-Phase 6)
- **macOS VAX HAL plugin** (`hal-plugin/`) — full port of AetherSDR's `NereusSDRVAX.driver` CoreAudio HAL plugin. Exposes 4 virtual output devices (`NereusSDR VAX 1..4`) + 1 TX input device to the macOS audio system; app process communicates with the plugin via a shared-memory ring (`VaxShmBlock`) for low-latency sample handoff. `CoreAudioHalBus` wraps the plugin on the app side. Includes `packaging/macos/hal-installer.sh` that builds + signs + packages the driver into a `productbuild` `.pkg` with a postinstall restart of `coreaudiod` (with macOS 14.4+ `killall` fallback when `launchctl kickstart` returns EPERM). **Note:** the release pipeline cannot produce a redistributable notarized installer until Apple Developer ID credentials are in place, so **v0.2.2 does NOT attach the HAL plugin `.pkg` to the GitHub Release**. macOS users who want VAX routing today can self-sign locally — see [docs/debugging/alpha-tester-hl2-smoke-test.md](docs/debugging/alpha-tester-hl2-smoke-test.md) §"macOS notes" for step-by-step instructions. (Phase 3O Sub-Phase 5)
- `VirtualCableDetector` (`src/audio/VirtualCableDetector.{h,cpp}`) — Windows-focused auto-detect of VB-Audio Virtual Cable, VAC, Voicemeeter, Dante Virtual Soundcard, and FlexRadio DAX virtual-cable families. Enumerates PortAudio devices against a family-signature table; reports detected cable pairs with suggested channel bindings. `rescan()` + rescan-diff helpers feed the VAX first-run wizard. (Phase 3O Sub-Phase 7)
- `VaxChannelSelector` widget (`src/widgets/VaxChannelSelector.{h,cpp}`) — compact per-slice VAX channel button row (1-4 + Off) embedded in the VFO flag. `setSlice()` rebinds cleanly on slice reshuffle (e.g. RX2 enable/disable); prior bindings are disconnected before new ones attach. Tooltips explain each button's destination. (Phase 3O Sub-Phase 8)
- `VaxFirstRunDialog` (`src/gui/VaxFirstRunDialog.{h,cpp}`) — launched on MainWindow startup when no VAX configuration has been persisted. Platform-specific: Windows runs `VirtualCableDetector` and offers to pre-fill channel bindings from detected virtual-cable pairs; macOS prompts for HAL-plugin install if absent; Linux checks for PulseAudio/PipeWire presence. "Apply suggested" maps detected pairs onto the first unassigned VAX slots. (Phase 3O Sub-Phase 10)
- `MasterOutputWidget` (`src/widgets/MasterOutputWidget.{h,cpp}`) hosted in the new `TitleBar` strip at the top of the main window — global speaker-volume slider + master-mute button + right-click → device picker + scroll-wheel fine-tune. One source of truth for output routing across the app. (Phase 3O Sub-Phase 11)
- `TitleBar` widget — thin strip above the main menu that hosts the menu bar, `MasterOutputWidget`, and the 💡 feature-request button (moved from menu corner). Keeps top chrome clean at narrow window widths. (Phase 3O Sub-Phase 11)
- **Setup → Audio sub-tabs** — `AudioDevicesPage` (per-device driver API / buffer / format with live-reconfig), `AudioVaxPage` (4 channel strips with meter + gain + mute + device picker + Auto-detect QMenu wiring `VirtualCableDetector`), `AudioTciPage` (placeholder for Phase 3J TCI), `AudioAdvancedPage` (reset-all-audio-settings with confirmation dialog + IVAC feedback parity controls). Full Audio-nav refactor from the legacy single-page Setup → Audio. (Phase 3O Sub-Phase 12 Tasks 12.1–12.5)
- `VaxApplet` (`src/gui/applets/VaxApplet.{h,cpp}`) — container-applet port from AetherSDR with per-channel RX gain slider + mute button + TX gain slider + level meter for all 4 VAX slots. `MeterSlider` widget (ported from AetherSDR) drives the sliders. (Phase 3O)
- `SliceModel::vaxChannel` property (`VaxChannel` enum: `Off` / 1-4) — per-slice VAX routing persisted under `slice<N>/vaxChannel`. New `SliceModel::slicePrefix()` helper unifies settings-key composition for VAX + future slice-scoped fields. Clamped on load so invalid persisted values don't poison the engine. (Phase 3O)
- `TransmitModel::txOwnerSlot` + `VaxSlot` enum — TX arbitration field so only the slice that armed TX actually transmits; `vaxSlotFromString` / `vaxSlotToString` round-trip with explicit `MicDirect` arm. Prevents ambiguous multi-slice MOX. (Phase 3O)
- `AppSettings` VAX schema migration helper — `main.cpp` runs a one-shot migration from pre-3O `daxChannel` keys to `vaxChannel` keys (covers the Phase 3O DAX→VAX rebrand). Idempotent; subsequent launches are no-ops. (Phase 3O)
- `SpectrumOverlayPanel` VAX Ch combo now wires to slice 0 directly; changes propagate through `SliceModel::vaxChannel` (was: unwired stub). (Phase 3O)
- Tests: `MasterOutputWidgetSignalRefreshTest` (with 50 ms timing assertion), audio-engine IAudioBus refactor coverage, VirtualCableDetector rescan helpers, `VaxFirstRunDialog` guard tests, `vaxSlotFromString` round-trip, SliceModel `VaxChannel` clamp-on-load. (Phase 3O)
- **macOS alpha-tester guidance for code signing + VAX HAL plugin** — `docs/debugging/alpha-tester-hl2-smoke-test.md` now documents the v0.2.x signing state (ad-hoc codesign, no Developer ID, no notarization), the Gatekeeper right-click → Open workflow, the `xattr -dr com.apple.quarantine` fallback, and a step-by-step self-sign procedure for the VAX HAL plugin so testers can enable VAX routing on macOS today without the notarized `.pkg`.

### Fixed
- **Setup-dialog checkboxes and radio buttons now visible on the dark theme.** `SetupPage` base class applies `kCheckBoxStyle` + `kRadioButtonStyle` at the page root; previously system defaults rendered black-on-dark and were invisible. (Phase 3P-H)
- **Alex-1 / Alex-2 Filters live LED now tracks VFO through CTUN-mode edge crossings.** Tabs were subscribing to `PanadapterModel::centerFrequencyChanged` but in CTUN mode the pan centre stays put while the slice tunes, so edge crossings were missed. Switched to `SliceModel::frequencyChanged` on every current + future slice (handles post-connect `addSlice` events). Added 250 ms polling fallback as a signal-path belt-and-suspenders. (Phase 3P-H)
- **ADC Overload status-bar label no longer shifts STATION** when firing. Fixed 180 px width + 12 px gap + empty-text-when-idle holds layout stable. (Phase 3P-H)
- **Silent shutdown crash on Windows after HL2 session, plus downstream Thetis startup hang.** `RadioConnectionTeardown` posted the worker-thread disconnect via a plain queued `invokeMethod` and immediately called `quit()`; on Windows' event dispatcher the quit could race the posted event out of the loop, leaving metis-stop unsent, the UDP socket open, and the `P1RadioConnection` + its three timers orphaned on a dead thread. Later Qt teardown then tripped a fast-fail abort and left the HL2 Winsock endpoint in a state that blocked Thetis from binding port 51188 until `netsh winsock reset` / reboot. Helper now uses a `QSemaphore` + `tryAcquire(1, 3000ms)` to ensure `disconnect()` completes on the worker before quit while keeping a bounded shutdown latency if the worker is wedged; `P1RadioConnection::disconnect()` now also closes its UDP socket explicitly, matching P2. Closes #83.
- **Hermes Lite 2 bandpass filter now switches on band/VFO change.** P1RadioConnection was emitting `m_alexHpfBits=0`/`m_alexLpfBits=0` permanently because the filter bits were never recomputed from frequency. P2 had the right code; lifted into shared `src/core/codec/AlexFilterMap` and called from P1's `setReceiverFrequency`/`setTxFrequency`. (Phase 3P-A)
- **Hermes Lite 2 step attenuator now actually attenuates.** P1's bank 11 C4 was using ramdor's 5-bit mask + 0x20 enable for every board; HL2 needs mi0bot's 6-bit mask + 0x40 enable + MOX TX/RX branch. Fixed via per-board codec subclasses (`P1CodecStandard` for Hermes/Orion, `P1CodecHl2` for HL2). RxApplet S-ATT slider range now widens to 0-63 dB on HL2 from `BoardCapabilities::attenuator.maxDb`. (Phase 3P-A)
- **VFO frequency entry no longer rejects valid thousand-grouped input.** The prior parser treated `7,200` as two tokens and silently dropped the comma group; full rewrite now handles decimal / comma-grouped thousands / unit-suffix (`k`, `M`, `G`, `Hz`, `kHz`, `MHz`, `GHz`) inputs consistently, including the single-comma + unit + 3-digit tail case (`7,200kHz`) which is treated as thousands. Closes #73.
- **RX-applet STEP ↑/↓ arrows now actually step the tuning ladder.** The arrows were rendering but their clicks were unwired; both now advance/retreat through the tuning-step ladder, and **500 Hz was added** between 100 Hz and 1 kHz to match Thetis's default ladder. Closes #69.
- **Receivers no longer leak across disconnect/reconnect cycles on the same radio.** `ReceiverManager` is now `reset()` on disconnect so the DDC-to-receiver map is clean when a second connect attempt runs; previously reconnecting the same rig would accumulate phantom receivers and crash WDSP channel allocation on the second attempt. Closes #75.
- **HL2 I2C read responses now persist into the `IoBoardHl2` model.** The HL2 I/O Setup page's register table was reading a model whose register slots were never updated from ep6 I2C read-response frames; the parse path now writes the received byte into the model and emits a change signal so the page stays in sync with the radio's live register state.
- **`FreqCorrectionFactor` now actually shifts the radio.** The Phase 3P-G Calibration page wrote the factor into the model correctly, but `P2CodecOrionMkII::hzToPhaseWord` never consulted it — the UI changed and the phase word didn't. Moved the multiplication into the codec so the dial on the Calibration page takes effect on the next tune.
- **`OcMatrix` state guarded with `QReadWriteLock` for cross-thread access.** Main-thread writes from the OC Outputs page were racing connection-thread reads at C&C compose time on boards with busy OC traffic; per-mask read/write locks remove the race. Default state remains byte-identical.
- **RX1 preamp toggle queued onto the connection thread.** The per-ADC RX1 preamp combo was calling `P2RadioConnection::setRx1Preamp()` synchronously from the GUI thread — could race the codec's next C&C compose. Now invoked via queued connection so the codec sees a stable value at compose time.
- **P1 bank scheduler ceiling now reads `codec->maxBank()`** instead of a hardcoded constant, so HL2 and Anvelina Pro 3 stop clipping at bank 10 and correctly reach bank 11 (S-ATT) / bank 17 (Anvelina Pro 3 extended) during compose cycles.
- **HAL plugin shared-memory layout aligned across app + plugin** — `VaxShmBlock` struct was diverging in field order between `hal-plugin/` and `src/audio/`, causing garbled samples on macOS when the app wrote the plugin's read in-flight; now driven from a single shared header. Also: HAL plugin RX ring backlog clamped to prevent laps-reading garble when the app stalls. (Phase 3O Sub-Phase 5)
- **Speakers-mutex scope narrowed to the push call only** in `AudioEngine` — previously held across format re-negotiation during live-reconfig, producing audible pops when the user switched output devices mid-stream.
- **Saved speakers device is applied to the engine at startup** — on first-launch-with-saved-config the `AudioEngine` was initialised before the speakers device was re-read from AppSettings; startup path now pulls the saved device before engine init.
- **VAX selector now lives inside the VAX tab** (was rendering below the tab bar due to a layout-parent mixup); **VAX channel tags refresh on `sliceRemoved`** and **rebind on slice-0 reshuffle** (RX2 enable/disable cycles no longer orphan the VAX combo).
- **Apply-suggested in the VAX first-run dialog now maps onto the first unassigned slots** instead of overwriting slot 0 every time.
- **Windows CMake now auto-fetches `libfftw3-3.dll`** via a FetchContent fallback when the system package isn't present, fixing cold-build failures on fresh Windows setups.
- **About dialog credits g0orx/wdsp** for the POSIX portability shim that made WDSP cross-platform — missed attribution surfaced by the Phase 3P-H discovery-driven corpus refresh.

### Changed
- **P2RadioConnection** `setReceiverFrequency` / `setTxFrequency` Hz→phase-word conversion now multiplies by `CalibrationController::effectiveFreqCorrectionFactor()` (defaults to 1.0 → byte-identical to pre-cal). Per-MAC freq correction lets users compensate per-radio reference oscillator drift. Ports `NetworkIO.cs:227-254` `FreqCorrectionFactor` + `Freq2PhaseWord()` `[@501e3f5]`. (Phase 3P-G)
- PA Calibration setup tab renamed to **Calibration**; the existing PA Current (A) group preserved exactly. (Phase 3P-G)
- `P1CodecHl2` gains I2C intercept mode — when `IoBoardHl2`'s I2C queue has pending transactions, the next C&C frame's 5 bytes are overwritten with the I2C TLV payload (chip-address + control + register address + data) and the txn dequeued. Normal bank compose runs when queue is empty. Per mi0bot `networkproto1.c:898-943`. ep6 read path extended to parse I2C response frames (C0 bit 7 marker) back into IoBoardHl2 register state. (Phase 3P-E)
- `P1RadioConnection`'s ep6/ep2 packet sizes are now recorded into `HermesLiteBandwidthMonitor`; watchdog tick drives the periodic rate evaluation. The legacy sequence-gap fallback throttle heuristic remains as a safety net when the monitor isn't wired (test seam path). Closes long-open `TODO(3I-T12)` markers at `P1RadioConnection.cpp:892, 939, 1416-1462`. (Phase 3P-E)
- `RadioModel` now owns the per-connection `IoBoardHl2` and `HermesLiteBandwidthMonitor` instances and pushes them to `P1RadioConnection` at connect time, mirroring the OcMatrix ownership pattern from Phase 3P-D. (Phase 3P-E)
- `RxApplet` antenna buttons (Ant 1/2/3) now read from `AlexController::txAnt(currentBand)` / `rxAnt(currentBand)` and re-populate on band change. Click-to-select calls the controller setter, respecting Block-TX safety guards. Was: static placeholder buttons. (Phase 3P-F)
- `RadioModel` now owns `AlexController`, `ApolloController`, `PennyLaneController` instances and pushes MAC + load on connect, mirroring Phase D's OcMatrix and Phase E's IoBoardHl2 + HermesLiteBandwidthMonitor ownership patterns. (Phase 3P-F)
- `P1RadioConnection` and `P2RadioConnection` now source the OC byte at C&C compose time from `OcMatrix::maskFor(currentBand, mox)` (when the matrix is wired via `setOcMatrix()` — `RadioModel` pushes its `m_ocMatrix` to the connections at connect time) instead of the legacy `m_ocOutput` field. Falls through to legacy when the matrix is unset (test seams). Default state byte-identical: empty matrix → `maskFor()==0` matching legacy `m_ocOutput==0`. Regression-freeze gates (P1 + P2) still PASS byte-for-byte. (Phase 3P-D)
- `P1RadioConnection`'s C&C compose layer refactored into per-board codec subclasses (`P1CodecStandard`, `P1CodecHl2`, `P1CodecAnvelinaPro3`, `P1CodecRedPitaya`) behind a stable `IP1Codec` interface. Behavior byte-identical for every non-HL2 board (regression-frozen via `tst_p1_regression_freeze` against a pre-refactor JSON baseline). Set `NEREUS_USE_LEGACY_P1_CODEC=1` to revert to the pre-refactor compose path for one release cycle as a rollback hatch.
- `P2RadioConnection` now calls the shared `AlexFilterMap::computeHpf/Lpf` helpers instead of its own inline copies; byte output unchanged.
- `BoardCapabilities::Attenuator` extended with `mask`, `enableBit`, and `moxBranchesAtt` fields capturing per-board ATT byte encoding parameters.
- `P2RadioConnection`'s C&C compose layer refactored into per-board codec subclasses (`P2CodecOrionMkII` for the OrionMKII / 7000D / 8000D / AnvelinaPro3 family, `P2CodecSaturn` extending it for ANAN-G2 / G2-1K with the G8NJJ BPF1 override) behind the new `IP2Codec` interface. Behavior byte-identical to pre-refactor for all captured tuples (`tst_p2_regression_freeze` with 36 tuples). `NEREUS_USE_LEGACY_P2_CODEC=1` env var reverts to the pre-refactor compose path for one release cycle as a rollback hatch. (Phase 3P-B)
- `BoardCapabilities` extended with `p2SaturnBpf1Edges` (per-band start/end MHz, empty default) and `p2PreampPerAdc` (true for OrionMKII family). `AlexFilterMap` shared between P1 and P2 codecs (was Phase A; Phase B is the first P2 consumer). (Phase 3P-B)
- `AudioEngine` backend dispatch refactored — the engine holds `IAudioBus` instances rather than a raw `QAudioSink`; platform selection via `Q_OS_*` at startup. Default path (no VAX configuration) falls through to the Qt multimedia output, byte-identical to v0.2.1 audio output. (Phase 3O Sub-Phase 4 + 8.5)
- **DAX → VAX UI rebrand (app-wide).** All user-facing "DAX" labels, enum values, settings keys, and doc strings renamed to "VAX". `AppSettings` first-launch migration covers persisted pre-rename keys. Internal identifiers (`VaxSlot`, `VaxChannel`, `vaxChannel`) follow the new name. (Phase 3O)
- **TitleBar relocation**: the 💡 feature-request button moved from the menu-bar corner into the new `TitleBar` strip alongside `MasterOutputWidget` for a consistent top-chrome layout. (Phase 3O Sub-Phase 11)
- `tune(display)` shipped current spectrum / waterfall defaults (smooth-fall value + Clarity Blue palette) as the out-of-box defaults; earlier v0.2.x installs can reset under Setup → Display.

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
