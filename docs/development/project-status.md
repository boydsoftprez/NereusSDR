# NereusSDR Project Status and Reference Detail

Material moved out of `CLAUDE.md` on 2026-09-29 to keep the per-session agent
context lean. `CLAUDE.md` keeps the rules; this file keeps the history, the
per-phase status, and the long-form reference detail. Content below is
verbatim from `CLAUDE.md` as of commit 86a1b20d6.

## Key classes (full list)

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
* `TciServer` — Qt6 QWebSocketServer wrapper + multi-client lifecycle + 5ms drain timer + per-client `TciSendQueue`; loopback bind port 50001; ping-interval 20s; emits `clientConnected` / `clientDisconnected`
* `TciProtocol` — Thetis-faithful command dispatch (two-switch: 60 set + 21 query handlers across 8 families); parse → dispatch → optional synchronous response string
* `TciClientSession` — per-client state struct (subscriptions, RX/TX audio ring lifecycle, IQ stream state, drop counters, last-command log); condenses Thetis's 49-field `TCPIPtciSocketListener` to 14 fields
* `TciBinaryFrame` — 64-byte LE header binary frame encode/decode; `TCISampleType` + `TCIStreamType` enum mirrors; `encodeSamples` handles FLOAT32/INT16/INT24/INT32 paths
* `TciSensorManager` — 4 wire format helpers (`formatRxSensors`, `formatRxChannelSensors`, `formatRxChannelSensorsEx`, `formatTxSensors`) + `minimumRequiredInterval` clamp (30..1000 ms, default 200 ms)
* `TciVfoCoalescer` — outbound-coalesced-map dedup (Layer 3 of Thetis 3-layer VFO throttle); Layers 1+2 subsumed by Qt event loop
* `TciSendQueue` — 3-priority FIFO per client (Urgent / Binary / Control) with bounded-depth oldest-drop; drain order mirrors Thetis `tryDequeueNextOutboundFrameLocked`
* `TciApplet` — operator-facing TCI status applet (Container #0): status dot + port + client count + Setup button; Slice A + TX level meters with gain sliders
* `ClientChainApplet` — per-client TCI connection detail applet (Container #0): TX badge, peer/name, subscription badges, last command, drop counter, disconnect button; 1 Hz auto-refresh
* `CatTciServerPage` (inside `CatNetworkSetupPages`) — Setup → Network → TCI Server: 6 group boxes (Server / Compatibility / IQ Stream / Audio Stream / Sensors / VFO Quirks), 17 AppSettings keys

**Phase 3J-2 (Spot system) classes (shipped v0.5.0):**

* `SpotModel` (`src/models/SpotModel.h`): TCI-keyed sink for all spot sources (ported from AetherSDR). Owns the canonical SpotData ring + emits `spotReceived` / `spotExpired`. Per-source dedup window (10 s, configurable).
* `SpotTableModel` (`src/models/SpotTableModel.h`): QAbstractTableModel backing the Spot List tab (extracted from AetherSDR DxClusterDialog).
* `BandFilterProxy` (`src/models/BandFilterProxy.h`): QSortFilterProxyModel for band + source pill filtering.
* `FreeDVStationModel` (`src/models/FreeDVStationModel.h`): NereusSDR-native 14-field live station map driven by FreeDVReporterClient.
* `RxDecodeModel` (`src/models/RxDecodeModel.h`): local decode ring buffer; sources WSJT-X UDP + RADE callsign-over-EOO decodes.
* `DxClusterClient` / `WsjtxClient` / `SpotCollectorClient` / `PotaClient` / `FreeDVReporterClient` / `PskReporterClient` (`src/core/`): 6 spot-source clients (RBN handled through DxClusterClient on the RBN telnet host).
* `CtyDatParser` / `AdifParser` / `DxccWorkedStatus` / `DxccColorProvider` (`src/core/`): 4-tier DXCC color resolver stack (ported from AetherSDR).
* `SpotHubDialog` (`src/gui/SpotHubDialog.h`): modeless 9-tab dialog (Tools > Spot Hub, Ctrl+Shift+S) with per-source tabs + unified Spot List + Display knobs (ported from AetherSDR's DxClusterDialog pattern).
* `FreeDVReporterDialog` (`src/gui/FreeDVReporterDialog.h`): modeless 14-column live station view (Tools > FreeDV Reporter, Ctrl+Shift+R) with TX/RX highlights, QSY, status messages, idle auto-removal (Qt6 port from freedv-gui's wx UI).

**Phase 3R (RADE mode) classes (shipped v0.5.0):**

* `RadeChannel` (`src/core/wdsp/RadeChannel.h`): peer-mode DSP channel for `DSPMode::RADE_U` / `DSPMode::RADE_L`. RX + TX paths both live; confirmed on-air on ANAN-G2 via remote receivers. Hybrid port: AetherSDR for Qt6 channel structure + freedv-gui for DSP pipeline truth.
* `RadeText` (`src/core/wdsp/RadeText.h`): thin Qt6 wrapper over third_party/rade's native callsign-over-EOO API. Task I4 Option B decision avoided porting freedv-gui's rade_text.c + roughly 1500 lines of codec2 deps.
* `RadeApplet` (`src/gui/applets/RadeApplet.h`): right-column applet auto-docked when RADE is the active mode. Profile combo + sync indicator + Reset Vocoder button.
* `Resampler` / `RadeTxHpf80` / `RadeTx48to16` (`src/core/audio/`): TX-path helpers (HPF + 48-to-16 kHz polyphase resampler). Used by TxWorkerThread's RADE TxPath; the earlier K-bench deferral has been retired.
* `MicProfileManager` (existing): gains a new RADE factory profile (22 total, was 21). Leveler enabled; ALC + CFC + CESSB + Phase Rotator all bypassed. Auto-selected on mode entry to RADE.
* `SliceModel` (existing): gains `snrDb` Q_PROPERTY for the VFO flag SNR row, mode-aware visibility (RADE only).

## Sample-rate live-apply history

**Live-apply of rate changes lands via `RadioModel::setSampleRateLive` (12-step
sequence ported from Thetis `setup.cs:7003-7159 [v2.10.3.13]`) + the
`WdspEngine::setRxChannelRate` Thetis-faithful `SetXcmInrate` path
(`cmaster.c:453-507 [v2.10.3.13]`).** The earlier rebuild-based approach
shipped briefly in PR #219 + the codex-P2 wiring in PR #221, then crashed
on the first user combo-change because destroying the C++ wrapper invalidated
seven raw-pointer holders (RadioModel, TxWorkerThread, PureSignal,
MeterPoller, TwoToneController, TxCfcDialog, the TxChannel VOX-key static).
The new path keeps all wrapper pointers alive across a rate change.
Active-RX count live-apply remains in the same `setActiveRxCountLive` form
shipped by PR #219 — pending the Phase 3F multi-panadapter work that
actually exercises RX2.

## freedv-gui Source Layout

```
../freedv-gui/
├── src/
│   ├── reporting/                  ← FreeDVReporter + pskreporter
│   │   ├── FreeDVReporter.{h,cpp}
│   │   └── pskreporter.{h,cpp}
│   ├── pipeline/                   ← RADE pipeline + EQ + AGC steps
│   │   ├── RADEReceiveStep.{h,cpp}
│   │   ├── RADETransmitStep.{h,cpp}
│   │   ├── rade_text.{h,c}
│   │   ├── EqualizerStep.{h,cpp}
│   │   └── AgcStep.{h,cpp}
│   └── gui/dialogs/freedv_reporter.{h,cpp}  ← 14-col live station view
```

## Implementation plans

| Plan | Phase | Status |
| --- | --- | --- |
| [phase3d-spectrum-waterfall-plan.md](../architecture/phase3d-spectrum-waterfall-plan.md) | 3D: GPU Spectrum & Waterfall | **Complete** |
| [ctun-zoom-plan.md](../architecture/ctun-zoom-plan.md) | 3E: CTUN Zoom Bin Subsetting | **Complete** |
| [phase3g1-container-infrastructure-plan.md](../architecture/phase3g1-container-infrastructure-plan.md) | 3G-1: Container Infrastructure | **Complete** |
| [phase3g2-meter-widget.md](../superpowers/plans/2026-04-10-phase3g2-meter-widget.md) | 3G-2: MeterWidget GPU Renderer | **Complete** |
| [phase3g3-core-meter-groups.md](../superpowers/plans/2026-04-10-phase3g3-core-meter-groups.md) | 3G-3: Core Meter Groups | **Complete** |
| [phase3-ui-skeleton-plan.md](../architecture/phase3-ui-skeleton-plan.md) | 3-UI: Full UI Skeleton | **Complete** |
| [phase3g4-g6-advanced-meters-design.md](../architecture/phase3g4-g6-advanced-meters-design.md) | 3G-4/5/6: Advanced Meters Design Spec | **Approved** |
| [phase3g4-advanced-meters-plan.md](../architecture/phase3g4-advanced-meters-plan.md) | 3G-4: Advanced Meter Items | **Complete** |
| 3G-5: Interactive Meter Items | **Complete.** No plan document was ever written for this sub-phase; the work is recorded in the 3G-4 plan and the meter-parity audit below. |
| [phase3g6a-plan.md](../architecture/phase3g6a-plan.md) | 3G-6 (one-shot): Full Thetis Parity + MMIO | **Complete** |
| [phase3g8-rx1-display-parity-plan.md](../architecture/phase3g8-rx1-display-parity-plan.md) | 3G-8: RX1 Display Parity (47 Spectrum/Waterfall/Grid controls, Band enum, per-band grid, GPU polish) | **Complete** |
| [phase3g8-verification/README.md](../architecture/phase3g8-verification/README.md) | 3G-8: 47-control manual verification matrix | Matrix drafted |
| [phase3j2-3r-spots-and-rade-design.md](../architecture/phase3j2-3r-spots-and-rade-design.md) | 3J-2 + 3R: Spot system + FreeDV Reporter + PSK Reporter + RADE peer mode design spec | **Complete (shipped v0.5.0)** |
| [phase3j2-3r-spots-and-rade-plan.md](../architecture/phase3j2-3r-spots-and-rade-plan.md) | 3J-2 + 3R: implementation plan (50+ commits landed in v0.5.0) | **Complete (shipped v0.5.0)** |
| [phase3j2-verification/README.md](../architecture/phase3j2-verification/README.md) | 3J-2: 11-row bench verification matrix (DX cluster, RBN, WSJT-X, SpotCollector, POTA, FreeDV Reporter, PSK Reporter, DXCC coloring, panadapter collisions, auto-connect, Display knobs) | Matrix verified (v0.5.0) |
| [phase3r-verification/README.md](../architecture/phase3r-verification/README.md) | 3R: 12-row bench verification matrix (RADE RX, RADE TX confirmed on-air on ANAN-G2, preset routing, mode dispatch, UI surfaces, HL2 gated, PA safety, DEXP/VOX, multi-slice deferred) | Matrix verified (v0.5.0; Rows 9/12 deferred) |
| [2026-05-18-pgxl-tgxl-and-analog-smeter-design.md](../architecture/2026-05-18-pgxl-tgxl-and-analog-smeter-design.md) | 3P-II: PGXL/TGXL + analog S-Meter port (Phase 1-4) design spec | **Complete (pending bench in v0.5.2)** |
| [phase-pgxl-tgxl-smeter-verification/README.md](../architecture/phase-pgxl-tgxl-smeter-verification/README.md) | 3P-II: 36-row bench verification matrix | Matrix drafted (pending live PGXL + TGXL hardware) |
| [anan-g2e-verification/README.md](../architecture/anan-g2e-verification/README.md) | ANAN-G2E SKU port: 12-row bench verification matrix | Matrix drafted (4 documented gaps; pending live G2E hardware) |
| [phase3f-multi-panadapter-plan.md](../architecture/phase3f-multi-panadapter-plan.md) | 3F: Multi-Panadapter + DDC Assignment | **Complete — shipped in `[Unreleased]`, 8 sub-epics, 17/17 targeted ctest green; 47-row bench matrix still unticked** |
| [2026-08-02-bottom-banner-and-pan-menu-design.md](../architecture/2026-08-02-bottom-banner-and-pan-menu-design.md) | Bottom Banner Cleanup + AetherSDR-Shaped Pan Menu design spec: single `ChromeBarController` layout authority replacing 3 competing responsive systems, 9-layout pan menu thumbnail grid | **Complete (pending PR merge)** |
| [2026-08-02-bottom-banner-and-pan-menu-plan.md](../architecture/2026-08-02-bottom-banner-and-pan-menu-plan.md) | Bottom Banner Cleanup + Pan Menu: 14-task implementation plan (Phase A ChromeBarController + fold ladder, Phase B pan-menu thumbnail grid) plus a final fix wave closing the merge-blocking audit findings | **Complete (14/14 tasks + fix wave; pending PR merge)** |
| [2026-08-02-bottom-banner-and-pan-menu-verification/README.md](../architecture/2026-08-02-bottom-banner-and-pan-menu-verification/README.md) | Bottom Banner + Pan Menu: 7-row bench verification matrix | Matrix drafted (pending live G2 + HL2 hardware) |

## Phase 1 analysis docs (`docs/phase1/`)

| Document | Key Findings |
| --- | --- |
| 1A: AetherSDR Analysis | RadioModel hub, auto-queued signals, worker threads, AppSettings XML, GPU rendering via QRhi |
| 1B: Thetis Analysis | Dual-thread DSP (RX1/RX2), pre-allocated receivers, one-way protocol, skin system |
| 1C: WDSP Analysis | 256 API functions, channel-based DSP, fexchange2() for I/Q, PureSignal feedback loop |

## FPGA gateware key facts

   * Verilog FPGA gateware for an OpenHPSDR Protocol 2 board. This is the
     **hardware** authority for facts Thetis can only report second-hand:
     receiver/DDC count, board-type identification byte, protocol version,
     master clock. Added 2026-07-25 after discovering every row in
     `BoardCapabilities.cpp` cited only Thetis client code.
   * Key facts (`Orion.v` @ `8e86a61`):
     - `Orion.v:958` — `localparam NR = 8; // number of receivers to implement`
     - `Orion.v:956-957` — fabric fits up to 14 RX; bootloader's 2 MB file-size
       limit caps the practical build at 10
     - `Orion.v:964` — `board_type = 8'h05` with the authoritative ID list
       (00 Metis, 01 Hermes, 02 Griffin, 03 Angelia, 05 Orion)
     - `Orion.v:632` — NR=8 runs 8 receivers at 192 kHz, but only 2 at 1536 kHz
   * **`NR` is a compile-time constant that changes between firmware releases**
     (shipped as 2, 4, 7 and 8 at different times on the same board). No static
     per-board DDC count can be correct across firmware versions — see the
     Radio-Authoritative Settings Policy.

## Release history and phase status

### Current Phase: Phase 3F multi-pan / multi-slice has shipped and sits in `[Unreleased]` targeting v0.6.0. The last release is **v0.5.2 (2026-05-24)**. Next major epic: 3F-1 per-slice DSP routing, then 3M-2 CW TX.

**Released v0.5.2 (2026-05-24)** carried one major epic (Phase 3P-II) plus a new SKU port (ANAN-G2E / HermesC10), a new UI subsystem (applet visibility controller), 4O3A integration polish, TCI live-state fixes, PS-A persistence fixes, a G2E P2 RX unblock, and a PA-profile / quit-handling tail, all landing on top of v0.5.1. Since then, unreleased work has landed on `main`: Phase 3F multi-pan multi-slice (8 sub-epics), the tunable notch filter, the RF-Kit RF2K-S applet, and the bottom-banner `ChromeBarController` consolidation with the nine-layout `+PAN` picker.

* **Phase 3P-II (External RF accessories + analog S-Meter port).** Four-phase epic.
  - **Phase 1, PGXL/TGXL baseline (AetherSDR 1:1).** `PgxlConnection` (TCP 9008 V/R/S frame parser), `TgxlConnection` (TCP 9010 V/R/S parser), `TunerModel` (13 Q_PROPERTYs), `LanDiscovery` (UDP broadcast on 9008/9010), `AmpApplet`, `TunerApplet` rewire, `RelayBar`, Peripherals page under Setup → Network with Scan LAN dialogs.
  - **Phase 2, Analog S-Meter port.** `SMeterWidget` (180-degree needle arc, S-unit scale, animated needle 45/180 ms, peak hold Fast/Medium/Slow) wired to four WDSP meter sources: Signal / Signal Average (`RXA_S_AV`, Thetis `console.cs:957 [@501e3f5]`) / Signal Peak / Max Bin (`SetupDetectMaxBin` + `GetDetectMaxBin`, Thetis `wdsp/analyzer.c:688..830 [@501e3f5]`). Right-click context menu replaces inline strip. PGXL 2 kW power-scale snap. AppSettings round-trip.
  - **Phase 3, Connection robustness.** Exponential auto-reconnect (1/2/5/10/30/60 s), keepalive (30 s default), RTT-correlated ping (10 s default), full PGXL pairing flow (`amplifier create` + `flexradio pair` with paired-serial capture), band-change notifications, `ConnectionDiagnostics` (10 Q_PROPERTYs, 1 Hz coalesce), PeripheralsPage live status labels.
  - **Phase 4, Advanced UI.** `PgxlAdvancedPage` + `TgxlAdvancedPage` (Identity / Hardware / Network / Pairing / Diagnostics / Fault History / Tune Memory sections), `FaultLog` (10-entry ring buffer, JSON persisted, likelyCause heuristic), `TuneMemoryStore` (per-(antenna, band) auto-recall on `bandChanged`), `TxInterlockPolicy` (Disabled / Warn / Block + SWR gate + grace period), `PgxlInterlockPage` under Setup → Transmit, antenna label persistence (`TGXL_AntLabel_1/2/3`), power-cap soft-alert toast, applet right-click navigation. Bench verification matrix at `docs/architecture/phase-pgxl-tgxl-smeter-verification/README.md` (36 rows pending live PGXL + TGXL hardware; HL2 row gated on ATT/filter audit).
* **ANAN-G2E (HermesC10) SKU port.** New board enum + capability row, hardware profile init verified against Thetis v2.10.3.15, codec wrappers (`SetADCSupply`, `LRAudioSwap`), discovery byte 0x14 → HPSDRHW::HermesC10, BPF1 algorithm family (HPF setAlex1HPF), Hermes-class DDC4 + DDC0 + PS-DDC config, PA telemetry (fwd-power triplet, current / supply-volts), per-model preamp items, `SkuUiProfile` EXT label overrides, AddCustomRadioDialog wiring. Tasks A3 through F6 (12 ANAN-G2E bench tasks); bench matrix at `docs/architecture/anan-g2e-verification/README.md` (12 rows pending live G2E hardware).
* **Applet visibility controller.** New `AppletVisibilityController` + AppSettings round-trip, hamburger menu embedded in the AppletPanelWidget banner / S-Meter title bar, View > Containers > Applets show/hide section, two-way menu sync, capability-gated `setAvailable` axis, RADE-aware routing, master-toggle live UI gating via `RadioModel::fourO3AEnabledChanged`. Retires View > Network Applets.
* **4O3A integration polish.** PGXL bar-graph zero-on-post-TX fix, TGXL identity labels populate, PGXL pre-standby on TGXL hardware TUNE, event-driven FlexAPI interlock chain + MOX RF-flow gate, route-aware FLEX-discovery broadcast IP (computed subnet, not 255.255.255.255), canonical FlexRadio-format 16-digit serial from MAC, PGXL SmartSDR API responder explicit-IPv4 bind (macOS IPv6 default blocked PGXL connect), "AMP" → "Power Genius" rebrand, master-toggle auto-connect gating on PGXL + TGXL, AmpApplet reconnect uses canonical `PGXL_Manual*` keys.
* **TCI live-state + 5 review-issue fixes.** Init burst defaults wire-aligned with Thetis (P1 fix), broadcast slice state changes to connected clients, ChangedHandlers port, af/mon roundtrip in-spec, sliceAdded hook restored after stop/start, setFilterBand single-emit, live VFO broadcast reads `rx2Enabled`, agc_mode wire-token conversion.
* **PS-A persistence + bench tail.** Direct AppSettings save for autoCalEnabled, P2 baseline regen, two-tone power defaults to 10 W, PS-A direct save via `scheduleSettingsSave`, source-first per-packet PS pairing via Thetis `sync.c InboundBlock(id=1)`.
* **G2E P2 RX unblock + crash fix.** Mask dither/random for HermesC10, Hermes-class DDC0, zero rate on disabled DDCs (P2 connect unblock), retry SendStop + bounds-check on I/Q batch (G2E gateware lockup + crash).
* **PA profile + quit handling.** Manifest backfill on factory-profile lookup, disconnect-on-quit, SIGTERM handler.

**Build + packaging:** `third_party/rade/` (radae_nopy SHA b289102) and `third_party/r8brain/` (24-bit polyphase resampler, MIT) added to vendored dependency set. CMake glue builds RADE + Opus + LPCNet via ExternalProject. Approximately 9 MB added to the binary on every platform.

**Deferred / known limitations for v0.5.2:**

* Phase 3P-II bench verification on live PGXL + TGXL hardware (36-row matrix pending; ANAN-G2 bench available for control-plane / DSP verification).
* ANAN-G2E bench verification on live G2E hardware (12-row matrix pending; F2/F3/F4/F6 documented as `DONE_WITH_CONCERNS`).
* HL2 RADE bench verification still gated on HL2 ATT/filter audit closure.
* RADE multi-slice (RADE on A while SSB on B); Phase 3F future.

**Shipped in v0.5.1 (2026-05-15):** patch release. Eight fix-only PRs. Three close release-artifact ship-blockers carried over from v0.5.0 (Windows installer missing `rade.dll`, macOS x86_64 DMG built without `Qt6::WebSockets` silently disabling FreeDV Reporter / PSK Reporter / TCI, HL2 + Windows 11 waterfall sliders did not stick across launches). Three are persistence / connection-state correctness fixes that surfaced after v0.5.0 hit a bench (orphan `.bak` data-loss path, connection state stuck `Connected` on failed initial connect, VOX needed juggling to prime on every connect). Two keep the CodeQL SAST pipeline green after the `Qt6::WebSockets` REQUIRED gate (Qt 6.8 + `qt6-websockets-dev`).

**Shipped in v0.5.0 (2026-05-13).** Substantial minor release on top of v0.4.0, three major epics + extended bench-fix tail:

* **Phase 3J-1 (TCI v2.0 WebSocket server).** 8 new core classes (`TciServer` / `TciProtocol` / `TciClientSession` / `TciBinaryFrame` / `TciSensorManager` / `TciVfoCoalescer` / `TciSendQueue` / `TciVolume`), 2 new applets (`TciApplet` + `ClientChainApplet`), Tools > TCI Server + View > Network Applets menu, 4-state bottom-bar indicator. 62 dispatch commands across 8 families. Binary RX audio with per-stream WDSP resampler (FreeDV 8 kHz / Quisk / JTDX 12 kHz end-to-end). Init burst byte-for-byte parity with Thetis (~98 wire frames). 15 closeout items shipped after the initial port stabilized against real clients. ~80-row matrix verification harness.
* **Phase 3J-2 (Spot system + FreeDV Reporter + PSK Reporter).** 7 spot-source clients (DX cluster telnet, RBN, WSJT-X UDP, SpotCollector / DXLab UDP, POTA HTTPS, FreeDV Reporter Socket.IO, PSK Reporter IPFIX). SpotHubDialog (Tools > Spot Hub, Ctrl+Shift+S, 9 tabs). FreeDVReporterDialog (Tools > FreeDV Reporter, Ctrl+Shift+R, 14-col live view with TX/RX highlights + QSY + 2-hour idle auto-remove). Panadapter spot overlay with collision-avoidance stacking + `+N` cluster badges. DXCC color resolver (cty.dat + ADIF 4-tier). Auto-connect restore on launch.
* **Phase 3R (RADE as a true peer mode, RX + TX end-to-end).** Vendored `radae_nopy` (BSD-2-Clause SHA b289102) + Opus (LPCNet + FARGAN) at `third_party/rade/` (~9 MB embedded weights, no external model file). `DSPMode::RADE_U` / `DSPMode::RADE_L` peer mode dispatch. `RadeChannel` (RX + TX paths). Task I4 Option B (native callsign-over-EOO API via thin `RadeText` wrapper avoids ~1500 lines of codec2 deps). RADE TX shipped end-to-end (earlier K-bench deferral retired; confirmed on-air on ANAN-G2 via remote receivers). VFO flag mode-aware SNR row + speaker callsign from EOO decode. `RadeApplet`. `MicProfileManager` RADE factory profile (22 total).
* **Bench-fix tail (2026-05-11 → 2026-05-12).** 16 wire / parser / UX gaps: first-MOX audio_volume seed, 10 missing spot-client lifecycle wires, DXSpider parser fallthrough, SpotModel → SpectrumWidget bridge restored, SpotTableModel ownership moved RadioModel-side, per-source UI feedback wired across all source tabs, PSK Reporter source-first port from freedv-gui, 10-gap spot overlay closure against AetherSDR audit, FreeDV Reporter 4-feature port (hide-self / kHz toggle / sort+column persist) + distance/heading fix + row-highlight-vs-selection fix + Socket.IO ACK message push + VFO-spin throttle, RADE speaker callsign on the flag + idle-clear timer, first-time setup banner.

**Prior context (v0.4.0, shipped 2026-05-08)** with five major pieces of work landing together:

* **3M-4 PureSignal arrives.** Feedback DDC plumbing on Protocol 1 and Protocol 2, `calcc.c` + `iqc.c` vendored verbatim from Thetis, `PureSignal` coordinator class, `PsccPump` driver, per-board `PsDdcConfig`, `PsForm` modeless dialog (Tools → PureSignal), `AmpView` modeless dialog, two-tone IMD overlay on the spectrum, `PsaIndicatorWidget` bottom-banner FB+PS pair. Enabled on every supported P1 and P2 SKU including HL2 (with HL2-specific negative-ATT support, AutoAtt convergence, ATT-on-TX master force-enable, psSampleRate=0 sentinel resolution) and plain Hermes. NereusSDR-only PureSignal Setup pages retired in favour of the Thetis-parity PsForm dialog.
* **Display + DSP-Options refactor.** WDSP `avenger()` and `detector()` ported (Thetis-faithful frame averaging + bin-to-pixel reduction). New Setup → DSP page (18 controls, RX/TX combo split, in-place filter resize, Filter Impulse Cache, per-mode buffer/filter/filter-type live-apply). Spectrum: full Thetis-faithful FFT slider with 7 windows + live bin width, K-based auto-zoom, NF-aware grid, Hz/bin auto-zoom override, SpectrumPeaksPage with PeakBlobDetector + ActivePeakHoldTrace, source-first port of Thetis `processNoiseFloor`. New Multimeter page with configurable MeterPoller. SettingsSchemaVersion v5 migrates DSP-Options Buffer/Filter Size to per-direction.
* **3M-3a-iv anti-VOX cancellation feed.** Closes the v0.3.2 gap where the gain control was plumbed but `SendAntiVOXData` was never called. 4 new WDSP wrappers, RxDspWorker → TxWorkerThread → TxChannel pump per chunk (single-RX direct path; aamix port deferred to 3F). Setup → Transmit → DEXP/VOX gains the full grpAntiVOX trio (Enable / Gain / Tau) with verbatim Thetis tooltips. Architectural divergence: source-selector (RX vs VAC) is dropped because VAX is a digital-mode app bus with no mic-feedback path.
* **Live-apply sample rate (no disconnect).** PR #219 + #221's destroy-and-recreate path replaced with the Thetis-faithful `SetXcmInrate` route. New `RxChannel::setSampleRate` (carry-only) + `WdspEngine::setRxChannelRate` (`SetInputSamplerate` + `SetInputBuffsize` on the live channel, `cmaster.c:453-507 [v2.10.3.13]`). `RadioModel::setSampleRateLive` is now the 12-step Thetis sequence from `setup.cs:7003-7159 [v2.10.3.13]`. TX channel untouched (Thetis routes `SetXcmInrate(0|1)` for RX1/RX2 only). HL2 P1 384 kHz parity (`mi0bot-Thetis setup.cs:849-851 [v2.10.3.13]`).
* **AF Gain rewire (PR #218, KM4BLG) + VAX bus calibration.** `RxChannel::setAfGain` → `WDSP.SetRXAPanelGain1` instead of the post-DSP setVolume scalar. Closes a long-standing distortion bug (`panel.gain1=4.0` default leaked +12 dB silently). VAX tap inverse-scales by `1 / afGain` (clamped at 0.001) so digital-mode apps stay calibrated regardless of speaker AF slider position. Edge case: AF=0 silences VAX too; full decoupling needs a pre-`PanelGain1` WDSP tap (deferred).

**Plus** persistence + stability fixes: MainWindow position/size/maximized state across launches (#206), audio bus master-mute flush (#201), PA Gain spinbox 38.8 dB clamp (#199), PA profile auto-pick fixes (#202), pre-connect Mic_Source persistence, macOS mic-permission dialog at launch + app icon + DMG background, step-att MOX clobber fix (#200), HL2 FPGA temperature on bottom banner. Plus Setup → Hardware combo + spinbox SVG-arrow styling and the Active RX Count widget hidden in single-RX builds. Plus compliance / cite touchups.

**Prior context (v0.3.2, shipped 2026-05-05):** 3M-3a-iii DEXP/VOX speech processing end-to-end, HL2 mi0bot RF/Tune Power slider parity, Setup → PA full Thetis parity + PA over-drive safety hotfix kernel, persistence and stability tail. **Prior context (v0.3.1, shipped 2026-05-03):** 3M-1 SSB TX (PR #152), Phase 3Q connection workflow refactor + chrome layer (PR #158), 3M-3a-i TX EQ + Leveler + ALC, 3M-3a-ii CFC + CPDR + CESSB + Phase Rotator (MicProfileManager 91 keys, 19/21 factory profiles ported verbatim), ParametricEq widget port (PR #159), Plan 4 ui-polish-foundation epic (PR #166). HL2 ATT/filter safety audit closed. Pre-emphasis de-scoped from 3M-3a-ii to 3M-3b (FM-mode follow-up).

**Next on the TX epic: 3M-2 CW TX.** Sidetone, firmware keyer, QSK/break-in. Absorbs the HL2 CWX bit-3 follow-up (`networkproto1.c:1247-1252 [@c26a8a4]`; desk-review B3, "HL2 firmware uses bit 3 of I-low byte for CWX PTT, non-HL2 boards don't"). After 3M-2: 3M-3b (FM pre-emphasis), the RADE TX K-bench follow-up (real-time integration into TxWorkerThread), then 3F multi-panadapter (which finally exercises `RadioModel::setActiveRxCountLive` + the aamix anti-VOX path + unblocks RADE-on-A while SSB-on-B multi-slice) and the longer-tail 3H / 3J-1 (TCI server) / 3K phases.

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
| **3M-1: Basic SSB TX (was 3I-TX)** | **TxChannel, mic input, MOX state machine, I/Q output. 3M-1a TUNE-only first RF (PR #144) → 3M-1b SSB voice + mic-jack family (PR #149) → 3M-1c polish + persistence + Thetis-faithful semaphore-wake TX pump v3 + HL2 setTxDrive triage + Codex P1/P2 fixes (PR #152).** | **Complete (2026-04-29)** |
| **3M-3: TX Processing** | **18-stage TXA chain (Equalizer / Pre-emphasis / Leveler / CFC / CESSB Compressor / Phase Rotator / AM-Squelch / ALC) + Setup pages + TX-side RX DSP additions. Schedule swap (2026-04-29) — was originally planned after 3M-2 CW TX; pulled forward because (a) it doesn't need the HL2 hardware bench (DSP stages introspectable on ANAN-G2), (b) it lets HL2 ATT/filter safety audit run in parallel without blocking forward TX progress, (c) it improves voice TX users notice (broadcast-grade preprocessing) before adding the CW state machine. Pre-emphasis de-scoped to 3M-3b (FM-mode follow-up). 3M-3a-i, -ii, and -iv all landed by v0.4.0; 3M-3 itself considered functionally complete pending operator bench feedback.** | **Complete (pending bench, shipped through v0.4.0)** |
| **3M-3a-i: TX EQ + Leveler + ALC** | TxChannel WDSP wrappers (10 EQ + 5 Lev + 7 ALC setters) + TransmitModel schema + MicProfileManager bundles 27 new keys (was 23 → now 50) + 20 Thetis factory profiles ported verbatim from `database.cs` + AgcAlcSetupPage TX Leveler/TX ALC sections + TxApplet `[LEV] [EQ] [PROC]` toggle row + TxEqDialog modeless editor (10 sliders + freq spinboxes + preamp + Nc / Mp / Ctfmode / Wintype + profile combo) + SpeechProcessorPage TX dashboard (3 status rows + 3 cross-link buttons). 13 GPG-signed commits, 8 new test executables, 246/246 ctest green. PR #TBD. | **Complete (pending bench)** |
| **3M-3a-ii: CFC + CPDR + CESSB + Phase Rotator** | TxChannel WDSP wrappers (6 CFC + 2 CPDR + 1 CESSB + 3 PhRot setters); cfcomp.c synced to Thetis v2.10.3.13 for 7-arg `SetTXACFCOMPprofile` (Qg/Qe ceiling-Q skirts, MW0LGE Samphire dual-license preserved); TransmitModel +15 properties (PhRot enabled/freq/stages/reverse, CFC enabled/post-EQ/precomp/postEqGain + 10×F/COMP/POST-EQ band arrays, paraEqData blob, global CPDR on, CPDR level-dB, CESSB on); MicProfileManager bundles **+41 keys** (was 50 → 91) with **155 verbatim overrides** across 19 of 21 factory profiles ported from `database.cs:9282-9418 [v2.10.3.13]` (incl. AM 10k unique `PhRotStages=9`); CfcSetupPage rewrite (PhRot + CFC + CESSB groups + open-dialog signal); SpeechProcessorPage live-status bindings; TxApplet `[CFC]` button + right-click → `TxCfcDialog` modeless editor (profile combo + Save/SaveAs/Delete/Reset + 2 globals + 30 per-band spinboxes for F/COMP/POST-EQ); PhoneCwApplet PROC button/slider wired to `cpdrOn`/`cpdrLevelDb` (0..2 → 0..20 dB) — duplicate `[PROC]` button removed from TxApplet to dedup (JJ caught 2026-04-30; saved as `feedback_survey_before_adding_controls`). 17 GPG-signed commits, 7 new test executables (TxChannel setters/TM properties/profile round-trip/profile live-path/CfcSetupPage/TxCfcDialog/PhoneCwApplet PROC), 253/253 ctest green. **TxCfcDialog landed scalar-complete but spartan** — full Thetis-faithful `ucParametricEq` widget port (3396-line `ucParametricEq.cs` UserControl, used by both CFC and EQ dialogs) is queued as a separate sub-PR; hand-off design doc at `docs/architecture/phase3m-3a-ii-cfc-eq-parametriceq-handoff.md`. **Pre-emphasis de-scoped** to 3M-3b (FM-mode work) per master design §7.2 ("run as written"). PR #TBD. | **Complete (pending bench)** |
| **3M-3a-iv: Anti-VOX Cancellation Feed + grpAntiVOX UI parity** | Closes the gap left in 3M-3a-iii where the anti-VOX gain control was plumbed end-to-end but `SendAntiVOXData` was never called, leaving `antivox_data` zero and the cancellation silent. 4 new WDSP wrappers (`SetAntiVOXSize` / `Rate` / `DetectorTau` / `SendData`) on `TxChannel`. `RxDspWorker` emits `antiVoxSampleReady` per chunk + `bufferSizesChanged` from `setBufferSizes`. `TxWorkerThread` queued slots `onAntiVoxSamplesReady` / `setAntiVoxBlockGeometry` / `setAntiVoxDetectorTau` / `setAntiVoxRun` (new wrapper that flips `m_antiVoxRun` atomic gate). `MoxController::setAntiVoxTau(int ms)` slot with NaN-sentinel idempotency, ms->s scaling. **Independent run flag refactor (post-review M2 fix):** `TransmitModel::antiVoxRun` Q_PROPERTY (bool, default false, persisted per-MAC under `AntiVox_Enable`), `MoxController::setAntiVoxRun(bool)` slot + `antiVoxRunRequested(bool)` signal with first-call-emit guard. `TransmitModel::antiVoxTauMs` Q_PROPERTY (range 1-500 ms, default 20, persisted per-MAC under `AntiVox_Tau_Ms`). **Post-bench Option A refactor (NereusSDR-architectural divergence):** dropped `TransmitModel::antiVoxSourceVax`, `MoxController::setAntiVoxSourceVax`, `antiVoxSourceWhatRequested` signal, the `chkAntiVoxSource` UI, and the rejected-VAX scaffolding. Thetis chkAntiVoxSource (RX vs VAC at `setup.designer.cs:44646-44657 [v2.10.3.13]`) does not map to NereusSDR's architecture: VAX is a digital-mode app bus with no mic-feedback path, so the audio output device is the only valid cancellation reference. Replaced with a static "Source: Audio Output Device(s)" info row whose tooltip cites the divergence. Existing users with `AntiVox_Source_VAX` persisted leave it as a harmless orphan (no migration). Tap-point signpost comments added at `RxDspWorker.cpp` and `AudioEngine.h` for the future radio-speaker output work (anti-VOX tap relocates from `RxDspWorker` to post-mixer when per-bus processing diverges). **grpAntiVOX UI** on Setup → Transmit → DEXP/VOX: `chkAntiVoxEnable` ("Anti-VOX Enable"), `udAntiVoxGain` ("Gain (dB)"), `udAntiVoxTau` ("Tau (ms)"), tooltips verbatim from Thetis `setup.designer.cs:44631-44760 [v2.10.3.13]`. WdspEngine constant fix: anti-VOX tau default `0.01` → `0.02` to match Thetis `udAntiVoxTau.Value=20`. Single-RX direct pump (no aamix); aamix port deferred to 3F multi-pan. Bench-verification matrix at `docs/architecture/phase3m-3a-iv-verification/README.md` (5 rows × ANAN-G2 + HL2). Full divergence rationale at `docs/architecture/phase3m-3a-iv-antivox-feed-design.md` §18. PR #TBD. | **Complete (pending bench)** |
| **3J-2: Spot System + FreeDV Reporter + PSK Reporter** | **DX cluster + RBN + WSJT-X UDP + SpotCollector + POTA + FreeDV Reporter + PSK Reporter spot ingest. SpotHubDialog (9 tabs, AetherSDR-faithful). FreeDVReporterDialog (14-col live view, TX/RX highlights, QSY support, idle auto-delete). Panadapter spot overlay with collision-avoidance multi-level stacking + cluster badges. DXCC color priority via cty.dat + ADIF worked-status. Tools menu (Ctrl+Shift+S / Ctrl+Shift+R). Auto-connect on launch. Display knob persistence.** | **Complete (shipped v0.5.0)** |
| **3R: RADE as a True Peer Mode** | **Vendored radae_nopy (BSD-2-Clause SHA b289102) + Opus (LPCNet/FARGAN) at `third_party/rade/` (~9 MB embedded weights, no external model file). `DSPMode::RADE_U` / `DSPMode::RADE_L` peer mode + RadeChannel (RX + TX paths). Task I4 Option B: native callsign-over-EOO API via thin RadeText wrapper (avoids ~1500 lines of codec2 deps). Mode dispatch swaps RxChannel <-> RadeChannel; band changes inside RADE keep channel alive. VFO flag mode-aware SNR row + EOO-decoded speaker callsign. RadeApplet (profile combo + sync indicator + Reset Vocoder). Mode menu RADE entry. MicProfileManager RADE factory profile (22 total). RxDecodeModel sources from WSJT-X + RADE. RADE TX confirmed on-air on ANAN-G2 via remote receivers (K-bench deferral retired).** | **Complete (shipped v0.5.0; HL2 row + multi-slice deferred)** |
| **3P-II: External RF Accessories (PGXL + TGXL + Analog S-Meter)** | **Phase 1 PGXL/TGXL baseline (`PgxlConnection` + `TgxlConnection` V/R/S frame parsers + `TunerModel` + `LanDiscovery` + `AmpApplet` + `TunerApplet` rewire + `RelayBar` + Peripherals page). Phase 2 analog S-Meter port from Thetis (`SMeterWidget`: 180° needle arc, 4 RX modes: Signal / Sig Avg `RXA_S_AV` / Signal Peak / Max Bin `SetupDetectMaxBin`+`GetDetectMaxBin`; right-click context menu; PGXL 2 kW snap; peak hold Fast/Medium/Slow). Phase 3 connection robustness (exponential auto-reconnect, keepalive, RTT-correlated ping, full PGXL pairing flow with serial capture, band-change notifications, `ConnectionDiagnostics`, PeripheralsPage live status). Phase 4 advanced UI (`PgxlAdvancedPage` + `TgxlAdvancedPage` Setup pages, `FaultLog` ring buffer with likelyCause heuristic, `TuneMemoryStore` per-(antenna, band) auto-recall, `TxInterlockPolicy` Disabled/Warn/Block + SWR gate + grace, antenna label persistence, power-cap toast, applet right-click navigation).** | **Complete (pending bench in v0.5.2)** |
| **3P-III: RF-Kit RF2K-S** | Applet + Setup pages + SMeter generalization + 8 new tests. REST polling, TCI band tracking via existing TciServer. TUNE/BYPASS greyed pending firmware. | **Complete (pending bench)** |
| **ANAN-G2E (HermesC10) SKU Port** | **Tasks A3-A4 + B4'-B7' + D1-D5 + E1-E5 + F1-F6: new board enum + capability row, hardware profile init (verified against Thetis v2.10.3.15), codec wrappers (`SetADCSupply` + `LRAudioSwap`), discovery byte 0x14 → HPSDRHW::HermesC10, BPF1 algorithm family (`setAlex1HPF`), Hermes-class DDC4 + DDC0 + PS-DDC, PA telemetry (fwd-power triplet, current / supply-volts), per-model preamp items, `SkuUiProfile` EXT label overrides, AddCustomRadioDialog wiring, G2E P2 RX unblock (mask dither/random for HermesC10, zero rate on disabled DDCs, retry SendStop + bounds-check I/Q batch).** | **Complete (pending G2E bench in v0.5.2; F2/F3/F4/F6 documented as DONE_WITH_CONCERNS)** |
| 3M-2: CW TX (was 3I-CW) | Sidetone, firmware keyer, QSK/break-in. Deferred until after 3M-3 ships AND the HL2 ATT/filter audit closes (so an HL2 can be CW-bench'd safely). Absorbs the HL2 CWX bit-3 follow-up (`networkproto1.c:1247-1252 [@c26a8a4]` — desk-review B3). | Planned |
| **3M-4: PureSignal** | **Feedback DDC plumbing on P1 + P2, `calcc.c` + `iqc.c` vendored verbatim from Thetis, `PureSignal` coordinator + `PsccPump` + per-board `PsDdcConfig`, `PsForm` modeless dialog (Tools → PureSignal), `AmpView` modeless dialog, two-tone IMD overlay, `PsaIndicatorWidget` bottom-banner FB+PS pair. Enabled on every supported P1 + P2 SKU including HL2 (negative-ATT support, AutoAtt convergence, ATT-on-TX master force-enable, psSampleRate=0 sentinel resolution) and plain Hermes.** | **Complete (shipped v0.4.0)** |
| 3F: Multi-Panadapter | DDC assignment (incl. PS states), FFTRouter, PanadapterStack, RX2 enable, 5-slice codec chain, TxSliceArbiter, P2 wideband data path, diversity | **Complete (in `[Unreleased]`, targeting v0.6.0)** |
| 3F-1: Per-slice DSP routing | NR / AGC / CTUN / audio bus per slice, wideband-bin visual fill, full DiversityDialog UI, direction-finding, antenna-conflict emission, HardwareDdcRoutingPage codec consumption | Planned |
| 3H: Skins | Thetis-inspired skin format, 4-pan, legacy import | Planned |
| **3J-1: TCI Server** | **TCI WebSocket server + 6 setup group boxes + 2 applets + bottom-bar indicator + Tools/View menu integration + matrix-driven verification harness with ~80 rows + init burst golden + 17 unit tests** | **Complete (shipped v0.5.0)** |
| 3K: CAT/rigctld | 4-channel rigctld, TCP CAT server | Planned |
| 3L: HL2 ChannelMaster.dll port | HL2 IoBoardHl2 I2C-over-ep2 wire encoding, bandwidth monitor full port | Planned |
| 3M: Recording | WAV record/playback, I/Q record, scheduled | Planned |
| **3N: Packaging** | **Consolidated `release.yml` (prepare → build×3 → sign-and-publish), `/release` skill, GPG-signed alpha builds: Linux AppImage ×2 archs, macOS Apple Silicon DMG, Windows portable ZIP + NSIS installer** | **Complete** |

---
