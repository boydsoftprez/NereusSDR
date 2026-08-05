# Phase 3J-2 + 3R Design: Spot System + FreeDV Reporter + RADE Mode

| Field | Value |
|---|---|
| Phases | 3J-2 (Spots + FreeDV/PSK Reporter), 3R (RADE-as-mode) |
| Status | Design approved, ready to plan |
| Author | JJ Boyd / KG4VCF |
| Date | 2026-05-10 |
| Ships in | Single PR; tagged for v0.5.0 (release number TBD) |
| Upstream pins | freedv-gui [@77e793a], AetherSDR [@0cd4559], Thetis [v2.10.3.13] |
| Sibling worktrees | Phase 3J-1 TCI server in `feature/phase3j-1-tci-server-port` (not in this PR) |

## 1. Goals & scope

This single PR ships two phases: **3J-2 (Spot system + FreeDV Reporter + PSK Reporter)** and **3R (RADE as a true peer mode)**. Both subsystems land together because the deepest data-flow tie is FreeDV Reporter `freq_change` events feeding both `SpotModel` (spot overlay) and `FreeDVStationModel` (rich 14-column live view) in parallel; separating them would mean writing the same client twice or shipping incomplete reporter data.

### What's in scope

**Phase 3J-2:**

- Full port of AetherSDR's spot stack: `DxClusterClient` (telnet, multi-flavor login prompts, IAC stripping, RBN-suffix tagging), `WsjtxClient` (UDP multicast 224.0.0.1:2237, big-endian QDataStream, magic 0xADBCCBDA, callsign extraction across all WSJT-X message families), `SpotCollectorClient` (DXLab UDP 9999, reuses cluster parser), `PotaClient` (HTTPS poller with ID-based dedup), `DxccColorProvider` + `DxccWorkedStatus` + `CtyDatParser` + `AdifParser` (worked-before color overlay).
- New from `freedv-gui`: `FreeDVReporterClient` (replaces AetherSDR's "lacking" `FreeDvClient`; full Engine.IO v4 / Socket.IO v4 protocol against `qso.freedv.org`), `PskReporterClient` (UDP datagram protocol). Both ported from `freedv-gui/src/reporting/` [@77e793a].
- New `FreeDVStationModel` holding all 14 station fields per Socket.IO `sid`, modeled on `freedv-gui/src/gui/dialogs/freedv_reporter.h::ReporterData` [@77e793a].
- New `SpotHubDialog` modeled on AetherSDR's `DxClusterDialog` [@0cd4559]: nine tabs in upstream order (Cluster / RBN / WSJT-X / SpotCollector / POTA / FreeDV / PSK Reporter / Spot List / Display). Per-source tabs use the uniform connection + Auto-start + Status + raw-event console template. Spot List tab uses `SpotTableModel` + `BandFilterProxy` (both ported from AetherSDR). Display tab folds in all knobs from AetherSDR's standalone `SpotSettingsDialog` (Levels / Position / Font / Lifetime / Override Colors / Override BG / Auto / BG Opacity / Memories toggle).
- New standalone `FreeDVReporterDialog` (Qt6-native port of `freedv-gui/src/gui/dialogs/freedv_reporter.{h,cpp}` [@77e793a]): 14-column live `QTableView` with TX/RX/RX-only row highlighting on a 5-20 second timer, column show/hide menu, per-column filter operators, band filter combo with track-frequency mode, QSY button, status-message editor with MRU dropdown, idle-station auto-delete, right-click QRZ/HamQTH/HamCall lookup.
- Panadapter spot-overlay rendering ported from AetherSDR `SpectrumWidget::drawSpotMarkers()` [@0cd4559]: collision-avoiding multi-level label stacking with vertical-nudge re-scan, dotted vertical ticks, configurable max-levels with overflow into `+N` cluster badges, click-to-tune with hit-test rectangles, popup menu for cluster spots.
- `cty.dat` (already in NereusSDR root) and ADIF worked-status integration with 4-tier color resolution: Override → DXCC status (red new entity / orange new band / gold new mode / dim grey worked) → per-source color → default cyan.
- Two new Tools menu entries: `Spot Hub…` (⌘⇧S) and `FreeDV Reporter…` (⌘⇧R). No new top-level menu bar entry.

**Phase 3R:**

- Vendored RADE library in `third_party/rade/` (peterbmarks/radae_nopy at pinned commit `b2891023`, sub-vendored libopus fetched at first build via radae_nopy's stock `BuildOpus.cmake` per Task A2 finding).
- **Neural-net weights are compiled directly into librade** (per Task A2b finding): `radae_nopy/src/rade_{dec,enc}_data.c` ship the weights as `static const float[]` arrays. The `rade_open(model_file, ...)` parameter is upstream-API-compat-only and ignored at runtime. No external `.f32` file to bundle, install, or distribute. Adds about 9 MB to the librade static-link size; absorbed in A2.
- New `RadeChannel` Qt6 wrapper modeled on `AetherSDR/src/core/RADEEngine.{h,cpp}` [@0cd4559] for structure (Qt class, signals/slots, threading, lifecycle, sample-rate plumbing); every DSP-touching line cross-checked against `freedv-gui/src/pipeline/RADE{Receive,Transmit}Step.{h,cpp}` and `rade_text.{h,c}` [@77e793a]. Owns the `rade*` codec handle, `LPCNetEncState*` (TX features), `FARGANState*` (RX vocoder), four r8brain resamplers (24↔16 / 24↔8 between speech and modem rates), TX/RX accumulation FIFOs, and the `rade_text` channel for embedded callsign + grid in voice frames.
- New "RADE" entry in mode dispatch as a true peer of SSB/AM/CW/FM/DigU/DigL.
- VFO flag SNR row, mode-aware (populated by `RadeChannel::snrChanged` for RADE; future digital modes wire the same `SliceModel::setSnrDb` slot).
- New `RadeApplet` in the right applet column (TX profile / sync indicator / SNR readout / freq offset / last decoded callsign + grid / reset vocoder button).
- New "RADE" factory preset in `MicProfileManager` with sensible defaults (EQ flat + disabled, Leveler enabled at -9 dBFS / 5 ms attack / 100 ms release, all transmitter shaping bypassed).
- Mode-aware TX path that bypasses PhRot / BPSNBA / AMSQ / PreEmph / FMMod / CFC / CFCOMP / CESSB / AM_Carrier / ALC / USB/LSB modulators when RADE is active.

### What's NOT in scope

- TCI server (Phase 3J-1, separate worktree `feature/phase3j-1-tci-server-port`). We expose a `SpotModel::applySpotStatus(int, kvs)` seam so the TCI worktree's `TciServer` can push spots through later when it lands.
- Classic FreeDV codecs (700C / 700D / 700E / 2020). Confirmed RADE-only; codec2's `freedv_open` API stays out of NereusSDR.
- Memory-channel data model (the toggle in Spot Hub → Display scaffolds; wires nothing until NereusSDR has a memory model).
- Multi-slice RADE (Phase 3F future). Single-slice RADE works; RADE-on-A while SSB-on-B should function but is not a verification target for this PR.
- Spot persistence across launches (transient by design; spots expire by lifetime).

### License posture

freedv-gui is LGPLv2.1+ (the `src/integrations/` subdirectory is BSD-3); GPLv2-or-later compatible upgrade on ingest into NereusSDR. peterbmarks/radae_nopy license to be confirmed at vendoring time; if not GPL-compatible we fall back to `ExternalProject_Add` (slower CI, network-dependent). libopus is BSD. cty.dat is freely redistributable.

### Architectural objective: RADE is a true peer mode

AetherSDR's `RADEEngine.h` header [@0cd4559] is explicit about why AetherSDR's RADE flow is awkward: `"The radio is set to DIGU (SSB passthrough). RADE handles the encoding and decoding locally."` FlexRadio hardware only supports SSB / AM / CW / FM at the radio. AetherSDR is forced to:

1. Force the slice into DIGU so the radio passes baseband audio unchanged.
2. Route audio to/from the host via the DAX virtual audio bus.
3. Mute the slice's normal audio output (raw RADE modem audio sounds like noise).
4. Run RADE on the host and pipe decoded speech through DAX RX to the speakers.

NereusSDR has no such constraint. We own the entire DSP chain. Phase 3R commits to RADE as a true peer mode: I/Q from the radio routes through `WdspEngine`'s mode dispatch to either `RxChannel` (SSB and friends) or `RadeChannel` (RADE). Audio output goes to `AudioEngine` either way. **No slice mute. No DIGU pretense. No virtual audio bus.** RADE shows up in the Mode menu as a peer of SSB and "just works."

## 2. Models & engines

### Two-model split for spots vs stations

Spots and FreeDV stations are different shapes of data. Spots are one-shot events (callsign at frequency); stations are live state (callsign with grid, last-TX time, last-RX callsign + SNR, status message, version, etc.). Trying to unify them into one model loses the rich live-state semantics that drive the FreeDV Reporter dialog. The clean architecture is two separate models.

**`SpotModel`** (`src/models/SpotModel.{h,cpp}`, port from AetherSDR [@0cd4559])

- `QMap<int, SpotData>` keyed by monotonic spot index.
- Sink for one-shot spots from every source.
- Mutation API: `applySpotStatus(int index, QMap<QString,QString> kvs)`  -  TCI-keyed update with keys `callsign`, `rx_freq`, `tx_freq`, `mode`, `color`, `background_color`, `source`, `spotter_callsign`, `comment`, `timestamp`, `lifetime_seconds`, `priority`. Decodes 0x7F (DEL) → space placeholder per TCI wire convention.
- Signals: `spotAdded(SpotData)`, `spotUpdated(SpotData)`, `spotRemoved(int)`, `spotsCleared()`, `spotsRefreshed()`, `spotTriggered(int, QString)`.
- The TCI-keyed contract is the seam the 3J-1 TCI worktree will hook into when it lands.

**`FreeDVStationModel`** (`src/models/FreeDVStationModel.{h,cpp}`, new, modeled on `freedv-gui/src/gui/dialogs/freedv_reporter.h::ReporterData` [@77e793a])

- `QHash<QString sid, FreeDVStation>` keyed by Socket.IO session ID.
- Holds the rich 14-field live state per station: callsign, gridSquare, distanceVal + distanceText, headingVal + headingText, version, frequency, txMode, status, userMessage, lastTxDate, lastRxDate, lastRxCallsign, lastRxMode, snr, lastUpdate. Plus highlight metadata: foregroundColor, backgroundColor, isPendingDelete, isPendingUpdate.
- Lifecycle: stations come and go on `new_connection` / `remove_connection` events. NOT a downconversion to spots.
- Signals: `stationAdded(QString sid, FreeDVStation)`, `stationUpdated(QString sid, FreeDVStation)`, `stationRemoved(QString sid)`, `connectionStateChanged(bool)`.

**`RxDecodeModel`** (`src/models/RxDecodeModel.{h,cpp}`, new, NereusSDR-native)

- Bounded ring buffer of recent local decodes from MY radio's receivers (default 200 entries).
- Each entry: `{callsign, freqMhz, snr, mode, source, utcTime, payload}`.
- Sources: `rade_text` decodes (when RADE is active), WSJT-X UDP `decode` events (always when the WsjtxClient is running). Distinguishes "what my radio just heard" from "what spots are flowing in from cluster / network sources."
- Drives the Activity → Decodes view inside `SpotHubDialog` (or wherever we surface local decodes). Also feeds the `RadeApplet`'s "last decoded callsign" display.

**`SliceModel`** extension (`src/models/SliceModel.{h,cpp}`)

- Adds `Q_PROPERTY(double snrDb)` with `setSnrDb(double)` slot and `snrDbChanged(double)` notify signal.
- Mode-aware: populated by `RadeChannel` when slice is in RADE mode; future digital modes (and possibly WDSP `RXAGetMeter` SNR estimator for SSB/CW) wire the same slot.
- `VfoWidget` binds to this property for the new SNR row in the flag.

### RADE codec wrappers

**`RadeChannel`** (`src/core/wdsp/RadeChannel.{h,cpp}`, per-slice, parallel to existing `RxChannel` / `TxChannel`)

Structure ported from AetherSDR's `RADEEngine` [@0cd4559]. DSP-touching code cross-checked against `freedv-gui/src/pipeline/RADE{Receive,Transmit}Step.{h,cpp}` [@77e793a].

- Owns: `rade*` codec handle, `LPCNetEncState*` (TX feature extraction), `FARGANState*` (RX vocoder), four r8brain `Resampler` instances (24↔16 kHz between speech and stereo, 24↔8 kHz between modem and stereo), TX/RX accumulation FIFOs, `rade_text` channel for embedded callsign + grid.
- Public API: `start()`, `stop()`, `isActive()`, `isSynced()`, `processIq(samples)` (RX path), `txEncode(samples)` (TX path), `resetTx()` (flush on MOX release).
- Signals: `rxSpeechReady(QByteArray)` (decoded speech 24 kHz stereo int16), `txModemReady(QByteArray)` (encoded modem 24 kHz stereo int16), `syncChanged(bool)`, `snrChanged(float)`, `freqOffsetChanged(float)`, `rxTextDecoded(QString call, QString grid)`.
- Lives on the audio/DSP thread same as `RxChannel` / `TxChannel`.

**`RadeText`** (`src/core/wdsp/RadeText.{h,cpp}`, port from `freedv-gui/src/pipeline/rade_text.{h,c}` [@77e793a])

- Embedded text channel for callsign + grid in voice frames.
- TX side: `rade_text_tx()` interleaves operator's callsign + grid into the feature stream periodically.
- RX side: `rade_text_rx()` extracts a valid text frame when CRC passes.
- Same C-style API as upstream; thin Qt6 wrapper for signal emission.

### `WdspEngine` extension

- Adds `createRadeChannel(int sliceId)` and `destroyRadeChannel(int sliceId)` to manage `RadeChannel` lifetime alongside `RxChannel` / `TxChannel`.
- Mode dispatch in `SliceModel::setMode(...)` triggers the right channel: SSB swaps to `RxChannel`, RADE swaps to `RadeChannel`. Audio output goes to `AudioEngine` through the existing audio bus contract regardless of which channel produced the samples.

### `MicProfileManager` extension

- Adds a new "RADE" factory preset shipped in the bundled profile set. Same on-disk format as the existing 21 factory profiles (defined by Phase 3M-3a-ii).
- Defaults: EQ flat + disabled, Leveler enabled (target -9 dBFS to match `freedv-gui`'s WebRTC AGC default, 5 ms attack, 100 ms release), CFC + CESSB + ALC + Phase Rotator bypassed, Mic Gain inherits operator setting.
- Operator-clonable per the existing profile-management UI; per-mode-per-band selection (existing) carries over.

## 3. Components / file list

### New files

**Spot ingest clients** (`src/core/`):

- `DxClusterClient.{h,cpp}` (port from AetherSDR [@0cd4559])
- `WsjtxClient.{h,cpp}` (port from AetherSDR [@0cd4559])
- `SpotCollectorClient.{h,cpp}` (port from AetherSDR [@0cd4559])
- `PotaClient.{h,cpp}` (port from AetherSDR [@0cd4559])
- `FreeDVReporterClient.{h,cpp}` (NEW, port from `freedv-gui/src/reporting/FreeDVReporter.{h,cpp}` [@77e793a])
- `PskReporterClient.{h,cpp}` (NEW, port from `freedv-gui/src/reporting/pskreporter.{h,cpp}` [@77e793a])
- `DxccColorProvider.{h,cpp}` + `DxccWorkedStatus.{h,cpp}` + `CtyDatParser.{h,cpp}` + `AdifParser.{h,cpp}` (port from AetherSDR [@0cd4559])

**RADE engine** (`src/core/wdsp/`):

- `RadeChannel.{h,cpp}` (NEW, AetherSDR structure + freedv-gui DSP cites)
- `RadeText.{h,cpp}` (NEW, port from freedv-gui [@77e793a])

**Models** (`src/models/`):

- `SpotModel.{h,cpp}` (port from AetherSDR [@0cd4559])
- `FreeDVStationModel.{h,cpp}` (NEW)
- `RxDecodeModel.{h,cpp}` (NEW)

**Spot Hub + dialogs** (`src/gui/`):

- `SpotHubDialog.{h,cpp}` (NEW, modeled on AetherSDR `DxClusterDialog` [@0cd4559])
- `SpotTableModel.{h,cpp}` + `BandFilterProxy.{h,cpp}` (port from AetherSDR `DxClusterDialog.h` [@0cd4559])
- `FreeDVReporterDialog.{h,cpp}` (NEW, Qt6-native port from `freedv-gui/src/gui/dialogs/freedv_reporter.{h,cpp}` [@77e793a])

**Applets** (`src/gui/applets/`):

- `RadeApplet.{h,cpp}` (NEW, parallel to `PhoneCwApplet`)

**Vendored dependencies** (`third_party/`):

- `third_party/rade/` (radae_nopy at pinned commit)
- `third_party/rade/opus/` if upstream RADE expects co-located, otherwise `third_party/opus/`

**Resources**:

- `resources/rade/model.f32` (~13 MB neural-net weights, sibling resource)

**Attribution + verifiers**:

- `docs/attribution/FREEDV-GUI-PROVENANCE.md` (NEW, parallel to `THETIS-PROVENANCE.md`)
- `docs/attribution/freedv-gui-author-tags.json` (NEW, generated)
- `scripts/discover-freedv-author-tags.py` (NEW)
- `scripts/verify-freedv-headers.py` (NEW)

**Tests** (`tests/unit/` + `tests/integration/`):

- ~12 unit-test executables (see Section 9)
- ~3 integration-test executables

### Modified files

- `src/models/SliceModel.{h,cpp}`  -  add `snrDb` Q_PROPERTY + signal
- `src/models/RadioModel.{h,cpp}`  -  own all six clients + three models, wire spot-source adapter slots, add reporter dialog ownership
- `src/core/wdsp/WdspEngine.{h,cpp}`  -  `createRadeChannel(sliceId)` / `destroyRadeChannel(sliceId)`, mode-aware channel lifecycle
- `src/core/audio/RxDspWorker.{h,cpp}`  -  RADE I/Q routing on RX path
- `src/core/audio/TxWorkerThread.{h,cpp}`  -  RADE TX path with mode-aware TXA stage bypass
- `src/core/audio/MicProfileManager.{h,cpp}`  -  add "RADE" factory preset
- `src/models/BandDefaults.{h,cpp}`  -  RADE band slot defaults (40m: 7.180 MHz, 20m: 14.236 MHz)
- `src/models/Mode.h` (or equivalent enum)  -  add `Mode::RADE`
- `src/gui/MainWindow.{h,cpp}`  -  wire all clients, add Tools menu entries, add hotkeys, instantiate dialogs on demand
- `src/gui/VfoWidget.{h,cpp}`  -  bind SNR row to `SliceModel::snrDb`, mode-aware visibility
- `src/gui/SpectrumWidget.{h,cpp}`  -  port `drawSpotMarkers()` + `showSpotClusterPopup()` + click-to-tune handling + memory-overlay layer
- `CMakeLists.txt` (top-level)  -  `add_subdirectory(third_party/rade)`, register all new sources + tests
- `packaging/{linux,macos,windows}/...`  -  include `resources/rade/model.f32` in installer manifests
- `CHANGELOG.md`  -  new entries for v0.5.0
- `docs/MASTER-PLAN.md`  -  add Phase 3J-2 + 3R rows, update phase status table
- `CLAUDE.md`  -  add freedv-gui as Reference Repository #4, extend pre-port checklist + author-tag corpus enforcement

### Rough scale

~28 new file pairs + ~12 modified file pairs + 2 vendored directories + 1 resource file + ~15 new test executables.

## 4. Data flow

Five flows, with threading model called out per flow.

### Flow 1: Spot ingest (every source → SpotModel)

Each client emits its own `spotReceived(DxSpot)` signal on its own thread:

- `DxClusterClient` (network thread, `QTcpSocket`)
- `WsjtxClient` (network thread, `QUdpSocket` multicast)
- `SpotCollectorClient` (network thread, `QUdpSocket`)
- `PotaClient` (network thread, `QNetworkAccessManager` poll)
- `FreeDVReporterClient` (network thread, `QWebSocket`)

`RadioModel` owns all six clients and a per-source adapter slot. The adapter slot:

1. Receives `DxSpot` via auto-queued signal (thread hop to main).
2. Converts `DxSpot` → TCI-style key/value `QMap` with the 12 keys from Section 2.
3. Calls `SpotModel::applySpotStatus(index, kvs)` where `index = monotonic_counter++`.

`SpotModel` emits `spotAdded` / `spotUpdated` on the main thread. The same TCI-keyed contract is what the 3J-1 TCI worktree's `TciServer` will hook into when it lands.

### Flow 2: FreeDV Reporter dual feed (FreeDVReporterClient → both models)

The FreeDV Reporter client maintains a live station map AND emits spots. On every Socket.IO event:

1. Updates internal `QHash<QString sid, FreeDVStation>` state.
2. Forwards the full station record to `FreeDVStationModel::updateStation(sid, info)`  -  drives the standalone Reporter dialog.
3. Synthesizes a `DxSpot` (callsign + freq + mode + grid + snr) and emits `spotReceived` like the other clients  -  drives `SpotModel` and the panadapter overlay.

Both models update on the main thread via auto-queued signals. The reason FreeDV needs dual-feed is that AetherSDR's existing `FreeDvClient` only does the spot feed, throwing away 9 of the 14 station fields  -  which is exactly what the user means by "lacking" and what this port fixes.

### Flow 3: Panadapter spot overlay rendering

`SpotModel` signals connect to `SpectrumWidget::onSpotsChanged()`:

1. Rebuild `m_spotMarkers` from live `SpotModel::spots()` filtered by current visible freq range and lifetime expiry.
2. For each marker, call `DxccColorProvider::colorForSpot(callsign, freqMhz, mode)`  -  returns red (new DXCC) / orange (new band) / gold (new mode) / dim grey (worked) or default cyan.
3. Store `SpotMarker{freqMhz, callsign, color, dxccColor, source, mode, index}`.

At paint time, `drawSpotMarkers(p, specRect)` runs the AetherSDR algorithm [@0cd4559]:

- Compute `x = mhzToX(freqMhz)` per marker.
- Compute label rect at `startY` (configurable %), width = font metric.
- Nudge down past every previous label that overlaps; re-scan from top after each nudge.
- If labelRect bottom exceeds `startY + th * maxLevels`, push the marker into `overflowGroups[x / 40]`.
- For placed labels: draw dotted vertical tick from spectrum bottom to label, optional bg pill (when `m_spotOverrideBg`), then text in `col`.
- For overflow groups: draw `+N` cluster badges at `maxBottom + 2` with click handling that pops up a menu of overflowed spots.

Click handling: hit-test `m_spotClickRects`; matching click → `frequencyClicked(freq)` signal → tune. Cluster badge click → popup menu listing overflowed spots.

Color priority chain (highest precedence first): override → DXCC status → spot's per-source color → default cyan.

All paint code on the GUI thread. The DXCC color lookup is lock-free read-only after ADIF parse completes.

### Flow 4: RADE peer-mode RX path

When slice mode changes to RADE via `SliceModel::setMode(Mode::RADE)`:

1. `WdspEngine::onSliceModeChanged(sliceId, RADE)` destroys the slice's `RxChannel` (or marks it inactive) and creates a `RadeChannel`.
2. The connection's I/Q-for-receiver dispatch  -  same one that fed `RxChannel::processIq()` for SSB  -  instead routes to `RadeChannel::processIq()`.
3. `RadeChannel`:
   - Resamples 24 kHz I/Q → 8 kHz `RADE_COMP` (modem rate)
   - Accumulates to `rade_nin()` samples
   - Calls `rade_rx()` → produces speech features
   - Feeds features → FARGAN vocoder → 16 kHz mono speech
   - Resamples 16 kHz → 24 kHz stereo
   - Emits `rxSpeechReady(QByteArray)` per chunk
4. `AudioEngine::feedAudio()` consumes the same way it does for SSB. Same audio sink, same volume control, same `QAudioSink`.
5. `RadeChannel` also runs `rade_text_rx()` on the decoded feature stream → emits `rxTextDecoded(QString call, QString grid)` when a valid text frame arrives.
6. That signal feeds `RxDecodeModel::addDecode({call, freqMhz, snr, mode="RADE", source="rade_text", utcTime, payload})`.
7. `RadeChannel::snrChanged(float)` → `SliceModel::setSnrDb(float)` → `VfoWidget` flag binding lights the SNR row green.

**No slice mute. No DIGU pretense. No DAX virtual bus.** Mode dispatch routes I/Q to the right channel; everything else identical to SSB.

### Flow 5: RADE peer-mode TX path

When MOX engages and slice mode is RADE:

1. `TxWorkerThread::onMoxOn()` checks slice mode. For RADE, takes the RADE TX path; for SSB, the existing WDSP TXA path.
2. RADE TX path:
   - Mic samples come in from `CompositeTxMicRouter` at 48 kHz mono float (same source as SSB).
   - Apply **Mic Gain** (`TransmitModel::micGainDb`).
   - Run through **WDSP TXAEQ** with the new "RADE" MicProfile preset's settings (flat default, user-tunable).
   - Run through **WDSP TXALVL** (leveler) with the RADE profile's settings (on by default, target -9 dBFS).
   - **Skip** PhRot, BPSNBA, AMSQ, PreEmph, FMMod, CFC, CFCOMP, CESSB, AM_Carrier, ALC, USB/LSB modulators.
   - High-pass filter at 80 Hz (kill mic rumble).
   - Resample 48 kHz → 16 kHz mono.
   - Feed to `RadeChannel::txEncode(samples)` → LPCNet features → `rade_tx()` → `RADE_COMP` modem samples at 8 kHz.
   - `rade_text_tx()` interleaves the operator's callsign + grid into the feature stream periodically.
   - Resample 8 kHz → 24 kHz stereo (TX I/Q rate).
   - Output goes directly to the radio's I/Q TX path. **No USB/LSB modulator**  -  RADE produces its own baseband.

### Tap-point design rationale

The tap point (post-EQ + post-Leveler, pre-everything-else) is justified by `freedv-gui/src/pipeline/TxRxThread.cpp:195-275` [@77e793a]: freedv-gui's actual TX pipeline is `Mic → optional RNNoise → optional AGC (WebRTC) → EqualizerStep → digital encoder`. The encoder is preceded by EQ + AGC (both user-toggleable). NereusSDR's WDSP TXAEQ is a 10-band parametric (superset of freedv-gui's 3-band biquad); TXALVL is the WDSP feedback leveler (different algorithm than WebRTC AGC but same intent). Order divergence (EQ→Leveler in NereusSDR vs AGC→EQ in freedv-gui) is operator-detectable only at extreme settings. CFC / CESSB / ALC are SSB-transmitter shaping that distorts what LPCNet was trained on; they get bypassed.

### Flow 6: VFO flag SNR

Already described in Flow 4. Pulled out for reference: `RadeChannel::snrChanged(float)` → auto-queued → `SliceModel::setSnrDb(float)` → `Q_PROPERTY` notify → `VfoWidget` binding paints the SNR row. AetherSDR pattern: yellow under +5 dB, green at +5 dB and above. When mode is not RADE (or future SNR-producing mode), the row shows ` -   - ` and grays out.

### Threading summary

| Thread | Components |
|---|---|
| Main / GUI | All models, all dialogs and widgets, MicProfileManager, DxccColorProvider reads |
| Connection (P1/P2) | RadioConnection UDP I/O, protocol framing |
| Audio/DSP | AudioEngine, WdspEngine, RxChannel / TxChannel / RadeChannel, RxDspWorker, TxWorkerThread |
| Spectrum | FFTEngine, panadapter prep |
| Per-source network | DxClusterClient (QTcpSocket), WsjtxClient (QUdpSocket multicast), SpotCollectorClient (QUdpSocket), PotaClient (QNetworkAccessManager), FreeDVReporterClient (QWebSocket), PskReporterClient (QUdpSocket) |
| ADIF parse | AdifParser (one-shot worker thread, 2 s debounced auto-reload) |

Per CLAUDE.md: cross-thread comms via auto-queued signals; no mutex in the audio callback; RADE's internal state lives on the DSP thread alongside the other channels.

## 5. Build & packaging

### Vendoring decision (verified 2026-05-10)
- radae_nopy commit pinned: `b2891023f3aecdf8b1793618000b1be6bcb2c4d1` ([@b289102])
- License: BSD 2-Clause (Copyright 2026, Peter B Marks) - GPL-compatible: YES
- Source URL: https://github.com/peterbmarks/radae_nopy
- Verified sub-vendored licenses (all GPL-compatible):
  - `src/kiss_fft.c`, `src/kiss_fftr.c`, `src/_kiss_fft_guts.h`: BSD 3-Clause (Mark Borgerding, 2003-2010)
  - `src/lpcnet_demo.c`: BSD 2-Clause (Mozilla, 2018)
  - `cmake/BuildOpus.cmake` pulls Opus from xiph at build time (BSD 3-Clause, well-known GPL-compatible)
- Decision: vendor in `third_party/rade/` per Option A (submodule or vendor-copy). No fallback to `ExternalProject_Add` needed.

### `third_party/rade/`

- Source: `peterbmarks/radae_nopy` cloned at a pinned commit (recorded in `third_party/rade/VERSION.txt` and on the row in `FREEDV-GUI-PROVENANCE.md`). The "no-Python" fork is the C/C++ build path freedv-gui uses.
- Build: `add_subdirectory(third_party/rade)` in top-level `CMakeLists.txt`. Produces a `rade` target (shared on Linux/macOS, static-with-DLL on Windows to match libopus's linkage).
- Sub-vendored Opus: pinned to commit `940d4e5af64351ca8ba8390df3f555484c567fbb` (same SHA `freedv-gui/cmake/BuildRADE.cmake` uses [@77e793a]).
- Build-time deps: Python3 for some macOS / cross-compile configurations. `find_package(Python3 COMPONENTS Interpreter)` gated by `APPLE OR CMAKE_CROSSCOMPILING`.
- No new runtime system deps. librade and libopus link into the app bundle.
- First clean build adds ~90-180 seconds. Incremental builds unchanged.

### Model file (REVISED per Task A2b finding 2026-05-10)

The C-port we vendored embeds the neural-net weights directly into the static library. No external model file needs to be bundled, downloaded, or installed.

- `radae_nopy/src/rade_dec_data.c` (~222k lines) and `radae_nopy/src/rade_enc_data.c` (~227k lines) contain the encoder + decoder weights as `static const float[]` arrays that link into `librade`.
- `radae_nopy/src/rade_api_nopy.c::rade_open()` accepts a `model_file` parameter for upstream-API compatibility but emits `"rade_open: model_file=%s (ignored, using built-in weights)"` and ignores it.
- `RadeChannel::start()` will pass an empty string (or `nullptr`) to `rade_open()`. No filesystem lookup, no `QStandardPaths` resolution, no missing-file fallback path needed. RADE mode is always available wherever NereusSDR runs.
- librade size impact: ~9 MB bigger static-link size on every platform (absorbed in A2).
- Future-proofing: if we ever swap to the Python upstream `drowe67/radae` (PyTorch runtime weights), the original sibling-resource design will need to be re-introduced. Documented here so future maintainers know why `RadeChannel::start()` ignores its `modelPath` arg.

### CMake glue

- `add_subdirectory(third_party/rade)` after the existing `third_party/wdsp` line.
- `target_link_libraries(NereusSDRObjs PUBLIC rade)` (the actual main object library is `NereusSDRObjs`, not `NereusSDR_core` as originally assumed; see Task A2 finding).
- `target_include_directories(NereusSDRObjs PUBLIC ${CMAKE_SOURCE_DIR}/third_party/rade/src ...)` plus four additional radae_nopy include dirs for transitive headers (Opus types, NN data) so `RadeChannel` sees `rade_api.h` and friends.
- New test targets for `test_rade_channel`, `test_rade_text` inherit through `NereusSDRObjs`.
- Three small patches to `third_party/rade/cmake/BuildOpus.cmake` for Ninja support: `BUILD_COMMAND make` (not `$(MAKE)`), `BUILD_BYPRODUCTS` declared, `PATCH_COMMAND ${CMAKE_CURRENT_LIST_DIR}/..` (not `${CMAKE_SOURCE_DIR}`). All documented inline + in the A2 commit message; upstreamable to radae_nopy as a small PR.

### Installer / packaging manifests

- **Linux AppImage** (`packaging/linux/build-appimage.sh`): bundle `librade.so` to `AppDir/usr/lib/`. No model-file step (weights embedded).
- **macOS DMG** (`packaging/macos/build-dmg.sh`): librade.dylib gets embedded in `NereusSDR.app/Contents/Frameworks/` and codesigned + notarized. Universal-binary build flag `BUILD_OSX_UNIVERSAL=ON`. No model-file step.
- **Windows portable ZIP + NSIS installer**: `rade.dll` next to `NereusSDR.exe`. No model-file step.

Release artifact size impact: roughly +9 MB per platform (librade with embedded weights). libopus is statically baked into librade by radae_nopy's BuildOpus.cmake, no separate libopus.so/dylib/dll to ship.

### CI implications

The existing `release.yml` (Prepare → Build×3 → Sign-and-publish) needs the RADE source available at build time. Prefer submodule for `third_party/rade/` (and `third_party/opus/` if separate)  -  keeps the NereusSDR repo lean and makes upstream syncs visible. If submodule complexity is too high, fall back to vendor-copy matching the existing `third_party/wdsp/` pattern.

## 6. UI surfaces

Mockups in `.superpowers/brainstorm/43838-1778453439/content/spothub-aether-faithful.html` (latest approved) capture the visual design.

### New / changed surfaces

- **Tools menu** gains two entries at the top of its dropdown:
  - `Spot Hub…` (⌘⇧S)  -  opens `SpotHubDialog`, modeless, single-instance (raise existing if open).
  - `FreeDV Reporter…` (⌘⇧R)  -  opens `FreeDVReporterDialog`, modeless, single-instance.
- **`SpotHubDialog`**  -  AetherSDR-faithful tab strip: `Cluster · RBN · WSJT-X · SpotCollector · POTA · FreeDV · PSK Reporter · Spot List · Display`. Per-source tabs use the uniform template (identity inputs + Auto-start toggle + Start/Stop button + status line + raw event console + optional filters/colors). Spot List tab has band+source pill filters and the 8-column `SpotTableModel` table with sortable columns. Display tab has stats blocks plus all `SpotSettingsDialog` knobs from AetherSDR (Levels / Position / Font / Lifetime / Override Colors / Override BG / Auto / BG Opacity / Memories toggle) plus a red "Clear all spots" button.
- **`FreeDVReporterDialog`**  -  standalone window with the 14-column live `QTableView` (Callsign / Locator / km / Hdg / Version / MHz / Mode / Status / Msg / Last TX / RX Call / Mode / SNR / Last Update). TX row red highlight, RX row green highlight, RX-only italic grey. Menu bar (Show / Filter / Idle longer than). Bottom: band filter combo + Track-frequency toggle + QSY freq input + Send QSY + Open Website + OK. Status message bar with MRU dropdown + Send / Save / Clear.
- **`SpectrumWidget`** spot overlay  -  collision-avoiding multi-level label stacking, dotted vertical ticks, `+N` cluster badges for overflow, click-to-tune, cluster popup menu. Visibility/style driven by Spot Hub → Display tab settings.
- **`VfoWidget`** flag  -  new SNR row at the bottom, mode-aware, reads `SliceModel::snrDb` Q_PROPERTY. Format: `SNR  +12 dB`. Color: dim grey ` -   - ` when no SNR available, yellow when SNR < +5 dB, green when SNR ≥ +5 dB.
- **`Mode` menu / mode buttons**  -  new `RADE` entry alongside SSB/AM/CW/FM/DigU/DigL. Mode-button row in the VFO flag adds RADE chip with purple `#a78bfa` accent when active.
- **`RadeApplet`**  -  new applet in the right column, parallel to `PhoneCwApplet`. TX profile selector, sync indicator (LED), SNR readout, freq offset readout, last decoded callsign + grid, reset vocoder button.
- **Right applet column**  -  gains a small "Activity (N spots)" status row near the bottom. Click opens Spot Hub.

### Hotkeys

- `⌘⇧S`  -  open Spot Hub
- `⌘⇧R`  -  open FreeDV Reporter dialog
- `⌘⇧K`  -  clear all spots
- Right-click on a spot tag in the panadapter  -  popup menu: Tune / Lookup on QRZ / Add to log

### What's NOT in the UI

- No new top-level menu bar entry (everything goes under Tools per AetherSDR parity).
- No new Setup dialog page. All spot/reporter config lives in Spot Hub.
- No persistently docked Activity Panel (replaced by on-demand Spot Hub + small applet-column launcher).
- No separate Spot Settings dialog (folded into Spot Hub → Display tab).
- No separate Raw Lines viewer (each source tab has its own console).

## 7. Settings & persistence

All settings via `AppSettings` (XML at `~/.config/NereusSDR/NereusSDR.settings`). PascalCase keys, `"True"`/`"False"` for booleans. No `QSettings`. All client-side.

### Spot display knobs (port AetherSDR keys verbatim [@0cd4559])

`IsSpotsEnabled` (bool, default `True`), `IsMemorySpotsEnabled` (bool, default `False`), `IsSpotsOverrideColorsEnabled` (bool, default `False`), `IsSpotsOverrideBackgroundColorsEnabled` (bool, default `True`), `IsSpotsOverrideToAutoBackgroundColorEnabled` (bool, default `True`), `SpotsMaxLevel` (int 1-10, default 3), `SpotsStartingHeightPercentage` (int 0-100, default 50), `SpotFontSize` (int 8-32, default 16), `SpotsOverrideColor` (#RRGGBB, default `#FFFF00`), `SpotsOverrideBgColor` (#RRGGBB, default `#000000`), `SpotsBackgroundOpacity` (int 0-100, default 48), `DxClusterSpotLifetimeSec` (int seconds, default 1800; migration from older `DxClusterSpotLifetime` minutes key), `FreeDvSpotColor` (#RRGGBB, default `#FF8C00`).

### Per-source connection config

Sub-keyed under each source name. Examples: `DxCluster/Host`, `DxCluster/Port`, `DxCluster/Callsign`, `DxCluster/AutoConnect`; `Wsjtx/Address`, `Wsjtx/Port`, `Wsjtx/AutoStart`, `Wsjtx/FilterCQ`, `Wsjtx/FilterPOTA`, `Wsjtx/FilterCallingMe`, `Wsjtx/ColorCQ`, `Wsjtx/ColorPOTA`, `Wsjtx/ColorCallingMe`, `Wsjtx/ColorDefault`; `FreeDvReporter/Callsign`, `FreeDvReporter/GridSquare`, `FreeDvReporter/StatusMessage`, `FreeDvReporter/ServerUrl`, `FreeDvReporter/AutoConnect`, `FreeDvReporter/IdleTimeoutMin`, `FreeDvReporter/ReconnectOnDisconnect`, `FreeDvReporter/ModeReported`, `FreeDvReporter/SavedMessages`, `FreeDvReporter/VisibleColumns`, `FreeDvReporter/RxHighlightSec`, `FreeDvReporter/FrequencyUnit`. Same pattern for `Rbn/`, `SpotCollector/`, `Pota/`, `PskReporter/`.

### DXCC color tracking

`Dxcc/AdifPath` (string), `Dxcc/AutoReload` (bool, default `True`), `Dxcc/ColorNewDxcc` (#RRGGBB, default `#FF3030`), `Dxcc/ColorNewBand` (#RRGGBB, default `#FF8C00`), `Dxcc/ColorNewMode` (#RRGGBB, default `#FFD700`), `Dxcc/ColorWorked` (#RRGGBB, default `#606060`), `Dxcc/Enabled` (bool, default `True`).

### RADE-specific

`Rade/ModelPath` (string, default empty for sibling), `Rade/TxProfile` (string, default `"RADE"`), `Rade/TextChannelEnabled` (bool, default `True`).

### Per-slice (existing per-slice-per-band scoping from Phase 3G-10)

`slice/<id>/snrDbVisible` (bool, default `True`).

### MicProfileManager

New "RADE" factory preset shipped in the bundled set. Per-mode-per-band selection (existing) carries over: `MicProfile/<slice>/<mode>/<band>` keys point at active preset name.

### BandDefaults

RADE band slot defaults registered for 7.180 MHz (40m) and 14.236 MHz (20m). Follows existing `BandDefaults` mechanism.

### Scoping

Most keys global. MicProfile data profile-scoped. BandDefaults band-scoped. Per-slice SNR-row visibility per-slice. No keys under `hardware/<mac>/`  -  spots and reporter identity are operator-level.

### Migration

No existing NereusSDR keys to migrate. Inherits AetherSDR's `DxClusterSpotLifetime` minutes → `DxClusterSpotLifetimeSec` seconds shim verbatim.

## 8. Attribution protocol

Mirrors the existing Thetis protocol piece-for-piece. No new policy decisions  -  clones the machinery for a new upstream.

### License compatibility

freedv-gui LGPLv2.1+ (src/integrations/ is BSD-3) → GPL upgrade on ingest. peterbmarks/radae_nopy license verified at vendoring time; if not GPL-compatible, fall back to `ExternalProject_Add`. libopus BSD. websocketpp / yyjson / WebRTC_AGC / r8brain bundled in freedv-gui src/3rdparty are all GPL-compatible.

### New files

- `docs/attribution/FREEDV-GUI-PROVENANCE.md`  -  registry table parallel to `THETIS-PROVENANCE.md`. Columns: Upstream file · Upstream version · NereusSDR file · Port date · Port author · AI tooling disclosure.
- `docs/attribution/freedv-gui-author-tags.json`  -  corpus of author tags, refreshed by the discover script.
- `scripts/discover-freedv-author-tags.py`  -  scans `../freedv-gui/src/**` for `// AUTHOR_TAG` patterns. Expected to find under 20 tags.
- `scripts/verify-freedv-headers.py`  -  verifies `// From freedv-gui X:N [vX.Y.Z|@sha]` cites have corresponding upstream author tags within ±10 port lines. Same shape as `scripts/verify-inline-tag-preservation.py`.
- Hook integration via `scripts/install-hooks.sh`  -  adds the new verifier to pre-commit + pre-push chain.

### Modified files

- `scripts/check-new-ports.py`  -  extend to accept freedv-gui as a fifth upstream alongside Thetis, mi0bot-Thetis, AetherSDR, WDSP.
- `CLAUDE.md`  -  add freedv-gui to Reference Repositories §4, extend Source layout quick reference, extend Pre-port checklist (Ring 1) to include freedv-gui, extend Constants and Magic Numbers rule example.

### Inline cite format

- Tagged release: `// From freedv-gui src/gui/dialogs/freedv_reporter.cpp:127 [v1.9.10]`
- Between releases: `// From freedv-gui src/pipeline/RADEReceiveStep.cpp:87 [@77e793a]`
- Multi-file port: `// --- From freedv-gui src/X ---` and `// --- From freedv-gui src/Y ---` markers per existing Thetis pattern.
- Cross-source mix (RadeChannel case): paired cites  -  AetherSDR scaffolding gets `// From AetherSDR RADEEngine.cpp:140 [@0cd4559]`; DSP body inside it gets `// DSP from freedv-gui src/pipeline/RADEReceiveStep.cpp:213 [@77e793a]`.

### Header preservation

Each freedv-gui source file's full upstream header copied byte-for-byte at the top of the NereusSDR file. Below it, NereusSDR modification block per the existing template in `docs/attribution/HOW-TO-PORT.md`. Multi-file ports include all upstream headers with `// --- From <file> ---` markers.

### Inline comment preservation (SHIP-BLOCKING)

Same rule as Thetis. Every `//AUTHOR_TAG` and behavioral comment in a freedv-gui line being ported preserved verbatim. Position-adjusted with `[original inline comment from <file>:<line>]` suffix when restructuring moves code around. `verify-freedv-headers.py` enforces mechanically.

### Pre-port checklist (Ring 1)

Before reading any freedv-gui file:

1. **freedv-gui file** about to be read.
2. **NereusSDR file(s)** the port will touch.
3. **Provenance status**  -  `grep -l "<nereussdr-path>" docs/attribution/FREEDV-GUI-PROVENANCE.md`. If not registered, the port is a new attribution event.
4. **Plan**: if (3) returned nothing, add upstream header AND PROVENANCE row in same commit.

Identical workflow to Thetis ports.

## 9. Testing strategy

Three layers: unit tests (TDD), integration tests, bench verification on real radios.

### Unit tests (~12 new exe targets)

`test_dx_cluster_parser`, `test_wsjtx_decoder`, `test_freedv_reporter_socketio`, `test_psk_reporter_protocol`, `test_pota_client`, `test_dxcc_color_provider`, `test_spot_model`, `test_freedv_station_model`, `test_rx_decode_model`, `test_rade_channel`, `test_rade_text`, `test_mic_profile_rade`, `test_spot_overlay_render`.

Per memory rule: TDD-as-shipped means write the test before implementation, run the new test 2x during the task (red → green), rely on pre-commit hook chain for verifier coverage. NOT full ctest suite per task  -  only once at end-of-epic.

### Mocking strategy

- Network clients: fixtures are raw byte buffers / pre-recorded protocol streams. No live sockets in unit tests.
- RADE library: real `librade` at link time; canned I/Q + audio fixtures from a known-good upstream transmission. Generated once by hand against a reference station, committed to `tests/fixtures/rade/`.
- ADIF: small fixture log (~20 QSOs, 6 DXCC entities × 3 bands × 2 modes) at `tests/fixtures/adif/sample.adi`.
- `cty.dat`: tree copy read directly (~340 entities, parse-once at test setUp).

### Integration tests (~3 new)

`test_spothub_dialog_smoke` (constructs SpotHubDialog with mock clients, verifies all 9 tabs + LED states + Display knobs round-trip), `test_reporter_dialog_smoke` (constructs FreeDVReporterDialog, pushes synthetic events, verifies 14-column populate + TX/RX highlights), `test_full_spot_pipeline` (end-to-end canned data → adapter → SpotModel → SpectrumWidget offscreen render).

### Bench verification matrices

Drafted as `docs/architecture/phase3j2-verification/README.md` and `docs/architecture/phase3r-verification/README.md`. Per-row checklist a maintainer ticks off after on-air testing.

**Phase 3J-2 rows:** DX cluster connect + spot stream verify, WSJT-X UDP decode flow, SpotCollector synthetic UDP, POTA dedup, FreeDV Reporter 14-col populate + TX/RX highlights + QSY, PSK Reporter UDP datagram verify, DXCC coloring with real ADIF, panadapter overlay across multiple bands + cluster badges + click-to-tune.

**Phase 3R rows:** RADE RX on ANAN-G2 (sync, decode, rade_text, SNR row green), RADE TX on ANAN-G2 (RX station decodes successfully, embedded callsign + grid), TX preset (Mic Gain + EQ + Leveler applied; CFC/CESSB/ALC NOT applied), mode dispatch sanity (SSB → RADE → SSB across band changes), HL2 RADE bench (gated on HL2 ATT/filter audit; may slip past initial release).

### CI integration

All unit + integration tests on every PR via existing GitHub Actions CMake workflow (Linux + macOS matrix). RADE library build is part of CMake  -  CI runners build fresh on every job (~90s first build, cached on incremental). Bench matrices are markdown checklists filled in by hand after release-candidate testing.

## 10. Risks & rollout

### Technical risks (with mitigations)

- **`radae_nopy` license unverified.** Verify upfront as the first port step. If GPL-incompatible, fall back to `ExternalProject_Add` (slower CI, network-dependent).
- **RADE library build complexity** (Python3 + Opus + librade across three platforms). Smoke-test the build on Linux + macOS + Windows CI in the first commit; treat as a CI gate.
- ~~**Model file shipping** in three platform installers~~ NO LONGER A RISK per Task A2b finding: weights are compiled into librade. The "missing model" UX path is moot for the C-port we chose.
- **FreeDV Reporter as single point of failure.** Server URL is a setting (operator can point at private relay). Auto-reconnect with exponential backoff caps at 60s.
- **Spot dedup edge cases.** Add dedup layer at SpotModel adapter  -  `(callsign, freqMhz rounded to 100 Hz, source)` tuple with 10s window suppresses same-source dupes; cross-source kept (spotter info is meaningful).
- **DxccColorProvider performance under load.** ~4260 lookups/sec at worst (142 spots × 30 fps). Lock-free reads after parse. Profile once with realistic ADIF; cache `(callsign, band)` → color if hot.
- **RADE codec compatibility drift.** Pin both library commit and model file together; record in `third_party/rade/VERSION.txt` and PROVENANCE row.
- **Single-RX assumption.** RADE-on-A while SSB-on-B plausible but untested. Multi-pan is Phase 3F future. Document as known limitation if state-sharing bugs surface.
- **TX-on-RADE PA safety.** RADE's average power profile differs from SSB voice (more constant duty cycle). Add explicit bench-matrix row verifying PA temperature stays safe during 5-minute RADE TX. Gate behind confirmation dialog if 3M-3 PA-protection logic doesn't handle digital modes safely.
- **DEXP / VOX with RADE.** Untested. Add bench-matrix row; user-visible warning in RadeApplet if DEXP enabled while RADE selected.

### Rollout risks

- **Large PR review burden** (~28 new file pairs + 12 modified + vendored dependency + new attribution infrastructure). End-of-epic subagent review gate. Maintainer reviews piecewise (clients → models → engine → UI → attribution scripts).
- **Hardware bench testing requires real RADE QSOs.** Ship as pre-release (v0.5-rc1), bench-test with maintainer + 2-3 alpha testers over a few weeks, ship v0.5.0 final once on-air confirmed.
- **HL2 testing slip risk.** HL2 ATT/filter audit hasn't closed; RADE-on-HL2 bench rows depend on safe HL2 hardware. Initial release may ship without HL2 verification rows green-ticked; add `Known limitation: HL2 RADE TX untested` to release notes; close in fast-follow.

### Cut list (only if scope balloons before merge)

- POTA client (~120 lines; defer if needed).
- WSJT-X filter/color knobs (CQ/POTA/CallingMe checkboxes + color pickers). Ship without them, add later.

**PSK Reporter does NOT cut.** It ships even if it becomes a burden; we find a way.

### Order of operations during implementation

1. Verify radae_nopy license (block here if not GPL-compatible).
2. Vendor RADE library + Opus, get CMake building on all three platforms (single commit; high CI risk, isolate it).
3. Land attribution scaffolding (`FREEDV-GUI-PROVENANCE.md`, scripts, hook integration). Empty registry; populated as files are ported.
4. Port spot-source clients in order of decreasing simplicity: SpotCollectorClient → PotaClient → DxClusterClient → WsjtxClient → FreeDVReporterClient → PskReporterClient. Each lands with its own unit test (TDD).
5. Port DxccColorProvider stack. Worker-thread heavy; isolate.
6. Port SpotModel + adapter layer + SpotTableModel + BandFilterProxy.
7. Port spot overlay rendering into SpectrumWidget.
8. New FreeDVStationModel + FreeDVReporterDialog (Qt6-native port from freedv-gui's wx).
9. Build SpotHubDialog with all 9 tabs.
10. Wire RadeChannel + RadeText. Mode dispatch in WdspEngine. SliceModel SNR property.
11. RADE TX path: MicProfileManager "RADE" preset + TXEQ + TXLEV bypass logic in TxWorkerThread.
12. VfoWidget SNR row, RadeApplet, Activity launcher row.
13. Settings persistence + Spot Hub Display tab knobs end-to-end.
14. End-of-epic review pass. Bench verification matrices filled.

Estimated 4-6 weeks of focused work. First ~2 days are the build-system gate.

## 11. Master plan integration

### New rows in `docs/MASTER-PLAN.md` phase status table

```
| 3J-2: Spot System + FreeDV Reporter + PSK Reporter |
   SpotModel + FreeDVStationModel + RxDecodeModel + DXCC color provider
   + DX cluster + RBN + WSJT-X + SpotCollector + POTA + FreeDV Reporter +
   PSK Reporter + SpotHubDialog (9 tabs, AetherSDR-faithful) + standalone
   FreeDVReporterDialog (14-col live view) + panadapter overlay with
   collision avoidance + memory-spots layer + cty.dat + ADIF worked-status |
   In progress |

| 3R: RADE Mode |
   Vendored RADE library + sibling model file + RadeChannel (peer-mode, no
   slice mute) + rade_text callsign+grid channel + new "RADE" entry in mode
   dispatch + VFO flag SNR row + RadeApplet + MicProfileManager "RADE"
   preset + TX path tap post-EQ/post-Leveler (skip CFC/CESSB/ALC) |
   In progress |
```

### Adjacent doc updates

- **Current Phase callout** flipped from `post-v0.4.0` to v0.5.0.
- **Execution order line** updated to `3M-1..4 → 3J-2 + 3R → 3M-2 → 3F → 3H → 3K → ...`.
- **Architecture Quick Reference section** gains entries for: SpotModel / FreeDVStationModel / RxDecodeModel / SpotHubDialog / FreeDVReporterDialog / RadeChannel / RadeText / DxccColorProvider / DxccWorkedStatus / CtyDatParser / AdifParser / DxClusterClient / WsjtxClient / SpotCollectorClient / PotaClient / FreeDVReporterClient / PskReporterClient.
- **Documentation Index** gains a row pointing at this design doc.
- **Implementation Plans table** gains rows for Phase 3J-2 and Phase 3R once writing-plans produces the plans.
- **Reference Repositories section** gains entry #4 (freedv-gui).

### CHANGELOG.md update for v0.5.0

```
## [0.5.0] - YYYY-MM-DD

### Added
- Phase 3J-2 - Spot system + FreeDV Reporter + PSK Reporter.
  - DX cluster + RBN + WSJT-X UDP + SpotCollector + POTA + FreeDV Reporter
    + PSK Reporter spot ingestion.
  - SpotHubDialog (Tools > Spot Hub, Cmd+Shift+S) with per-source tabs,
    unified Spot List, and Display knobs.
  - FreeDVReporterDialog (Tools > FreeDV Reporter, Cmd+Shift+R) with
    14-column live station view, TX/RX highlights, QSY support, MRU
    status messages.
  - Panadapter spot overlay with multi-level stacking, +N cluster badges,
    click-to-tune. DXCC color priority via cty.dat + ADIF worked-status.

- Phase 3R - RADE mode.
  - RADE as a true peer mode (no slice mute, no DIGU pretense, no virtual
    audio bus). Mode dispatch routes I/Q through RadeChannel.
  - Full RX + TX with embedded rade_text callsign+grid channel.
  - VFO flag SNR row, mode-aware. RadeApplet in the right column.
  - New "RADE" MicProfileManager preset (flat EQ + Leveler on by default).
  - Vendored radae_nopy + libopus in third_party/rade/. Neural-net
    weights compiled into librade (no external model file). Adds about
    9 MB to the binary on every platform.
```

## Appendix: Pre-port checklist for freedv-gui

When porting any file from freedv-gui, before reading source:

1. State the **freedv-gui file** about to be read.
2. State the **NereusSDR file(s)** the port will touch.
3. Run `grep -l "<nereussdr-path>" docs/attribution/FREEDV-GUI-PROVENANCE.md`. If empty, the port is a **new attribution event**.
4. Plan: if (3) returned nothing, add upstream header byte-for-byte AND a PROVENANCE row in the same commit.
5. Inline cite format: `// From freedv-gui <path>:<line> [vX.Y.Z|@sha]`.
6. Preserve all author tags and behavioral comments verbatim. Position-adjust with `[original inline comment from <file>:<line>]` if restructuring moves code.
7. For DSP-touching code: also cross-reference against AetherSDR if a Qt6-native pattern exists (RadeChannel case). Cite both: AetherSDR for structure, freedv-gui for DSP truth.

`scripts/verify-freedv-headers.py` runs in pre-commit + CI and fails the commit if any cite is missing a corresponding upstream author tag within ±10 port lines.
