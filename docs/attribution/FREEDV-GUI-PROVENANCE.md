# freedv-gui Provenance - NereusSDR derived-file inventory

This document catalogs every NereusSDR source file derived from, translated
from, or materially based on freedv-gui (drowe67/freedv-gui). Per-file license
headers live in the source files themselves; this index is the grep-able
summary.

NereusSDR is distributed under GPLv3 (root `LICENSE`). freedv-gui is LGPLv2.1+
with a BSD-2-Clause carve-out for `src/integrations/`. See §License below.

## When entries get added

A row is added to the table below - in the **same commit** that introduces
the ported logic - whenever a NereusSDR file:

1. Ports, translates, or materially re-expresses logic from any freedv-gui
   `.c`, `.cpp`, or `.h` file under `src/`, AND
2. That logic is not already covered by the Thetis / mi0bot / WDSP /
   AetherSDR / deskhpsdr lineage (i.e. freedv-gui is the *primary* source
   for that logic, not a cross-reference).

The procedure is identical to `THETIS-PROVENANCE.md` and
`DESKHPSDR-PROVENANCE.md`:

- Add the verbatim freedv-gui file header to the NereusSDR file (per
  `HOW-TO-PORT.md` §"Byte-for-byte headers and multi-file attribution").
- Add a `// From freedv-gui <path>:<line> [v<tag>|@<sha>]` inline cite at
  every ported function / constant (per `HOW-TO-PORT.md` §"Inline cite
  versioning").
- Add a PROVENANCE row here with the NereusSDR file, freedv-gui source,
  line ranges, derivation type, and notes.

## Upstream

- **Project:** FreeDV / freedv-gui
- **Repository:** https://github.com/drowe67/freedv-gui
- **Lineage:** founded 2012 by David Rowe (VK5DGR) and David Witten;
  primary maintainer Mooneer Salem (K6AQ); FreeDV Reporter, RADE, and PSK
  Reporter clients largely Mooneer's contributions.
- **Initial corpus reference SHA:** `@77e793a` (HEAD at time of first
  freedv-gui port plumbing, 2026-05-10)
- **Languages:** C++ (`.cpp` / `.h`), with some C in pipeline (`rade_text.c`)
  and bundled 3rd-party (yyjson / WebRTC_AGC / r8brain / websocketpp).

## License

freedv-gui is distributed under the **GNU Lesser General Public License
v2.1 or later** (LGPLv2.1+), per the project root `COPYING`. A BSD-2-Clause
carve-out applies to the radio-integration code under
`src/integrations/` (see `src/integrations/LICENSE`). Bundled third-party
libraries under `src/3rdparty/` (websocketpp, yyjson, WebRTC_AGC, r8brain)
each carry their own GPL-compatible license.

| Subtree | License |
| --- | --- |
| project root + most `src/**` | LGPLv2.1+ |
| `src/integrations/` | BSD-2-Clause (FreeDV Project, 2026) |
| `src/3rdparty/websocketpp/` | BSD-3 (Peter Thorson) |
| `src/3rdparty/yyjson/` | MIT (ibireme) |
| `src/3rdparty/WebRTC_AGC/` | BSD (Google / WebRTC project) |
| `src/3rdparty/r8brain/` | MIT (Aleksey Vaneev) |

NereusSDR is GPLv3. LGPLv2.1+ is **upgrade-compatible to GPLv3** when
linked into a GPLv3 work (LGPL §3 conversion clause). The BSD and MIT
carve-outs are GPL-compatible by their own terms. No dual-licensing
(Samphire-style or otherwise) exists in the freedv-gui source tree.

LGPL-relink obligations for freedv-gui code that NereusSDR statically
links will be documented in `docs/attribution/LGPL-COMPLIANCE.md` once
the first port lands (mirrors the libspecbleach pattern).

## Verifier and corpus tooling

- `scripts/verify-freedv-headers.py` runs in the pre-commit hook chain
  (`scripts/git-hooks/pre-commit`) and in CI. For every PROVENANCE row
  below, it verifies the NereusSDR file's first ~160 lines contain the
  required upstream-attribution markers.
- `scripts/discover-freedv-author-tags.py` walks `../freedv-gui/src/**`
  and refreshes `docs/attribution/freedv-gui-author-tags.json`. Re-run
  after every upstream sync (`git -C ../freedv-gui pull`).
- The PROVENANCE row + verbatim header + inline cites are what the
  pre-commit hook gates on. Skip any of the three and the commit is
  blocked.

## Inline cite format

- Tagged release: `// From freedv-gui src/path/file.cpp:N [v1.9.10]`
- Between releases: `// From freedv-gui src/path/file.cpp:N [@77e793a]`

(freedv-gui at `@77e793a` had no `git describe`-discoverable tag at the
time of A1 vendoring, so SHA stamps are the practical default until the
upstream tags a release we sync to.)

## Legend

Derivation type:
- `port`       - direct reimplementation in C++/Qt6 of a freedv-gui source file
- `reference`  - consulted for behavior during independent implementation
- `structural` - architectural template with substantive behavioral echo
- `wrapper`    - thin C++ wrapper around vendored C source

## Files derived from drowe67/freedv-gui

| NereusSDR file | freedv-gui source | Line ranges | Type | Notes |
| --- | --- | --- | --- | --- |
| src/core/FreeDVStation.h | src/gui/dialogs/freedv_reporter.h | :367-417 (`ReporterData` inner struct) [@77e793a] | port | Phase 3J-2 Task B5. Ported the field list of upstream's wxWidgets `ReporterData` inner struct into a plain Qt6 value type (QString / QDateTime / QColor). Per-sid station record shared by FreeDVReporterClient and FreeDVStationModel (Task D3). View-bookkeeping fields (`isVisible`, `isPendingDelete`, `isPendingUpdate`, `deleteTime`) dropped because they belong to the QAbstractTableModel layer, not the value type. Q_DECLARE_METATYPE added so the struct flows through QSignalSpy in tst_freedv_reporter_socketio. |
| src/core/FreeDVReporterClient.h | src/reporting/FreeDVReporter.h | full file [@77e793a] | port | Phase 3J-2 Task B5. Hybrid port. Class-shape, public surface (startConnection / stopConnection / setIdentity / requestQSY / updateMessage), Engine.IO Open + Socket.IO Connect handshake, dispatch table, and per-event handler signatures from freedv-gui. Qt6 structural pattern (QWebSocket member + slots, QTimer ping/reconnect, exponential backoff, QSignalSpy-friendly value-type signals) from AetherSDR src/core/FreeDvClient.h [@0cd4559]. Replaces freedv-gui's std::function callbacks (ReporterConnectionFn / ConnectionDataFn / FrequencyChangeFn / TxUpdateFn / RxUpdateFn / QsyRequestFn / MessageUpdateFn / ConnectionSuccessfulFn / AboutToShowSelfFn / RecvEndFn) with Qt signals (stationAdded / stationUpdated / stationRemoved / spotReceived / connected / disconnected / connectionError / rawLineReceived). Replaces freedv-gui's bundled SocketIoClient + websocketpp dependency with Qt6::QWebSocket. NereusSDR is always view-only; the upstream `report` / `report_wo` branches are not exposed (`reporter mode` is a 3M-x follow-up). NereusSDR-architectural addition: dual-feed signal layout per design doc Section 4 Flow 2 (every freq_change / rx_report drives BOTH stationUpdated AND spotReceived). Test seam wrappers (handleEngineIOForTest / handleSocketIOForTest / pingIntervalMsForTest / lastSentMessageForTest) are 1-line forwards exposed so the unit test can drive the wire-protocol layer without an actual WebSocket round-trip. |
| src/core/FreeDVReporterClient.cpp | src/reporting/FreeDVReporter.cpp | full file [@77e793a] | port | Phase 3J-2 Task B5. Hybrid port. Inline cites at requestQSY (`:104-119`), updateMessage (`:122-130` and `:688-702`), the view-mode auth payload (`:295-309`, `{"role":"view","protocol_version":2}`), the Socket.IO event registration table (`:361-369`), and each per-event handler: onNewConnection (`:372-406`), onTxReport (`:470-503`), onRxReport (`:505-553`), onFreqChange (`:555-583`), onMessageUpdate (`:585-607`), onConnectionSuccessful (`:408-434`), onRemoveConnection (`:436-468`), onBulkUpdate (`:633-651`). Engine.IO / Socket.IO state machine (handleEngineIO / handleSocketIO / handleEvent body) follows AetherSDR src/core/FreeDvClient.cpp:141-219 [@0cd4559] which already targeted the same wire format on top of QWebSocket. Dual-feed spot-synthesis helpers (emitSpotFromFreqChange / emitSpotFromRxReport) port the AetherSDR spot field mapping at FreeDvClient.cpp:243-370 [@0cd4559] (DxSpot field assignments, AppSettings FreeDvSpotColor lookup with `#RRGGBB` -> `#FFRRGGBB` promotion, log-line composition); upstream FreeDVReporter.cpp produces no DxSpot and so cannot be cited for those bodies. NereusSDR divergences: `qCDebug(lcDxCluster)` -> `qCDebug(lcSpots)`; log file path uses Qt's AppConfigLocation (already lands under `NereusSDR/`) instead of AetherSDR's GenericConfigLocation + "AetherSDR/freedv.log"; no separate writer thread (Qt event loop handles it); QWebSocket guarded by `#ifdef HAVE_WEBSOCKETS` so the class still compiles when Qt6::WebSockets is absent (test-seam parsers remain functional). |
| src/core/PskReporterClient.h | src/reporting/pskreporter.h | full file [@77e793a] | port | Phase 3J-2 Task B6. PSK Reporter IPFIX (RFC 5101) v0.1 client. Class-shape (PskReporter constructor signature -> setIdentity, addReceiveRecord -> reportDecode, send -> send) and the SenderRecord helper struct (folded into a private PendingRecord struct on the client) come from freedv-gui. Replaces upstream's POSIX socket + detached `std::thread` send path with QUdpSocket; replaces the manual reportCommon_ thread loop with Qt's event-loop QTimer auto-send. NereusSDR-architectural addition: a parser side (parseDatagramForTest + spotReceived signal) so an incoming sender-record datagram round-trips into DxSpot for the SpotModel. freedv-gui's pskreporter is send-only; PSK Reporter's public infrastructure does not push live-spot datagrams to clients (write-only network from the radio's perspective), so the receive side is gated on a locally-bound listener for unit-test coverage and any future relay protocol that delivers PSK Reporter records to NereusSDR. Test seam wrappers (buildDatagramForTest / parseDatagramForTest / senderRecordSizeForTest / encodeSenderRecordForTest) are 1-line forwards exposed so the unit test can drive the wire-protocol layer without an actual UDP socket round-trip. |
| src/core/PskReporterClient.cpp | src/reporting/pskreporter.cpp | full file [@77e793a] | port | Phase 3J-2 Task B6. Faithful translation of upstream's reportCommon_ (`:282-372`), encodeReceiverRecord_ (`:235-259`), encodeSenderRecords_ (`:261-280`), SenderRecord::recordSize (`:113-116`), SenderRecord::encode (`:118-146`), getRxDataSize_ (`:204-213`), getTxDataSize_ (`:215-233`), and addReceiveRecord (`:183-194`) into Qt6 idioms: QByteArray buffers instead of raw `char*` + `new[]`, QUdpSocket instead of POSIX socket + `std::thread::detach()`, QRandomGenerator instead of `std::random_device + std::mt19937`. The two static-const wire-format byte arrays (rxFormatHeader pskreporter.cpp:82-88, txFormatHeader pskreporter.cpp:93-101) are reproduced verbatim. Server hostname `report.pskreporter.info` and port `4739` ported from pskreporter.cpp:73-74. 50-record auto-flush ceiling (pskreporter.cpp:188-193) preserved. Datagram size calculation (16-byte header + sizeof(rxFormatHeader) + sizeof(txFormatHeader) + rxSize + txSize, minus sizeof(txFormatHeader) when no sender records, pskreporter.cpp:290-291) preserved. NereusSDR additions: parseDatagramLocked walks the IPFIX body and emits spotReceived for every sender record found; freedv-gui never parses incoming PSK Reporter datagrams. Logging routes through NereusSDR's `lcSpots` category. |
| src/models/FreeDVStationModel.h | src/gui/dialogs/freedv_reporter.h | :367-417 (`ReporterData` per-station record shape) [@77e793a] | structural | Phase 3J-2 Task D3. NEW model with no AetherSDR equivalent (gap-fill from the design doc Section 4 Flow 2). Class shape (per-sid record, distance + heading slots, foreground / background colour hints) inspired by upstream's wxWidgets `ReporterData` inner struct; the data type itself is `core/FreeDVStation.h` (already registered above), so this row covers the model class only. Public surface (stations / stationBySid / stationCount / setOurGridSquare + 4 slots + 4 signals) and the Qt6 signal-driven shape are NereusSDR-native. Subscribes to FreeDVReporterClient signals so the FreeDVReporterDialog (Phase G) and the Spot Hub Stations tab can present a sortable view of every active station. |
| src/models/FreeDVStationModel.cpp | src/gui/dialogs/freedv_reporter.cpp | :2312-2410 (calculateDistance_ / calculateLatLonFromGridSquare_ / calculateBearingInDegrees_ / DegreesToRadians_ / RadiansToDegrees_) [@77e793a] | port | Phase 3J-2 Task D3. Distance + initial-bearing helpers translated wxString -> QString (`MakeUpper` -> `toUpper`, `GetChar(N)` -> `at(N).toLatin1()`, `Mid(4,2)` -> `mid(4,2)`, `Length()` -> `size()`). The optional 6-character sub-square segment is honored when both characters pass `QChar::isLetter()`, replacing upstream's `wxRegEx ALL_LETTERS_RGX` check (case is already normalized by the surrounding `toUpper()`). Earth radius `6371` km, antimeridian South-Pole start (`lon=-180, lat=-90`), and the `(result == 360) ? 0 : result` bearing wrap-around preserved verbatim. Slot bodies (onStationAdded / onStationUpdated / onStationRemoved / clear / setOurGridSquare) are NereusSDR-native; freedv-gui's dialog wires its callbacks differently (queued lambdas in `fnQueue_`, not Qt signals). |
| src/gui/FreeDVReporterDialog.h | src/gui/dialogs/freedv_reporter.h | full file (class shape) [@77e793a] | port | Phase 3J-2 Task G1. Qt6-native rewrite of upstream's wxFrame-based dialog. Class shape is QWidget (modeless, Qt::WA_DeleteOnClose=false) replacing wxFrame; column-index enum (kCallsignCol..kLastUpdateDateCol = 0..13, kFreeDVReporterColumnCount = 14) ports the upstream `#define CALLSIGN_COL (0)` ... `#define LAST_UPDATE_DATE_COL (13)` macros at freedv_reporter.cpp:47-65 [@77e793a]. Constructor signature accepts `FreeDVStationModel*` (the NereusSDR Task D3 data hub that freedv-gui's wxDataViewCtrl/wxDataViewModel pair replaces) plus an optional `FreeDVReporterClient*` reserved for G2 QSY wiring. G1 shell only - filter / QSY / message bar (G2) and the menu bar with Show / Filter / Idle-longer-than menus (G3) are placeholder hooks (`m_bottomControls` empty QHBoxLayout, `m_menuBar` empty QMenuBar) so G2 and G3 can drop content into the existing shell without restructuring. Test seams `setHighlightClearMsForTest(int)` and `rowHighlightColorForTest(const QString&)` allow the smoke suite to drive the highlight state without waiting on wall-clock time. |
| src/gui/FreeDVReporterDialog.cpp | src/gui/dialogs/freedv_reporter.cpp | :47-65 (column macros), :75-182 (createColumn_ table), :3087-3146 (GetValue switch), :1289-1322 (TX/RX/Msg priority chain), :3180-3210 (refreshAllRows formatting), :3580-3737 (onTransmitUpdateFn_ / onReceiveUpdateFn_) [@77e793a] | port | Phase 3J-2 Task G1. Three NereusSDR-native classes folded into the dialog .cpp: (1) `FreeDVReporterTableModel : QAbstractTableModel` adapts the FreeDVStationModel QHash<sid, station> map into a flat 14-column row view (ports the column-header / per-column alignment table verbatim from createColumn_ at freedv_reporter.cpp:75-182 and the per-column cell-value switch verbatim from GetValue at :3087-3146). (2) `FreeDVReporterRowHighlightDelegate : QStyledItemDelegate` paints TX-red / RX-green / Msg-pink row backgrounds at paint time; replaces upstream's `reportData->backgroundColor` stamp on the row struct (freedv_reporter.cpp:3026, :1304-1339 [@77e793a]). (3) The dialog itself owns a per-sid `QHash<QString, QTimer*>` of clear-timers, each fired once at `kDefaultHighlightClearMs` after the TX/RX event; replaces upstream's 250ms global tick + per-row lastRxDate threshold check (freedv_reporter.cpp:1295-1302 [@77e793a]). Three NereusSDR-architectural divergences: (A) SNR -99 renders as " - " and other SNR values render as a signed integer "+12" / "-3" via formatSnr; upstream calls wxNumberFormatter::ToString(snr, 1) at :3692 [@77e793a] yielding "12.0". (B) Frequency renders MHz only with 4 decimals via formatFrequencyMhz; the kHz/MHz toggle from upstream's reportingFrequencyAsKhz setting (:3183-3191 [@77e793a]) is deferred to G3. (C) RX-row background color picks a NereusSDR-native green (`#3FAF55`); upstream's default `freedvReporterRxRowBackgroundColor = "#379baf"` at ReportingConfiguration.cpp:93 [@77e793a] is cyan-blue (G > R but B > G). TX-row color `#fc4500` and Msg-row color `#E58BE5` are ported verbatim from ReportingConfiguration.cpp:91 / :95 [@77e793a]. The cardinal-direction formatter helper (upstream `GetCardinalDirection_` at freedv_reporter.cpp:2676-2680 [@77e793a]) is deferred to a follow-up FreeDVStationModel D3 patch that stamps `FreeDVStation::headingCardinal`; the dialog's `formatHeading` already reads that field. |
| src/core/RadeChannel.h | src/pipeline/RADEReceiveStep.h + src/pipeline/RADETransmitStep.h | RADEReceiveStep.h full file (BSD-2-Clause header, class shape, member set: `dv_`, `fargan_`, `inputSampleFifo_`, `outputSampleFifo_`, `pendingFeatures_`, `pendingFeaturesIdx_`, `textPtr_`, `syncFn_`, `inputBufCplx_`, `inputBuf_`, `featuresOut_`, `eooOut_`, `outputSamples_`, `freqOffsetFn_`, `rxFreqOffsetPhaseRectObjs_`, `rxFdmOffset_`) [@77e793a]; RADETransmitStep.h full file (class shape, `dv_`, `lpcnetEncState_`, `featureList_`, `radioSpeechRunningFifo_`, `radioSpeechRunningOutputFifo_`, `numFeaturesToStore_`, embedded text channel hook) [@77e793a] | port | Phase 3R Task I1. Hybrid port - the I1 skeleton class header carries the freedv-gui BSD-2-Clause-style header verbatim (the four upstream files RADEReceiveStep.h / RADEReceiveStep.cpp / RADETransmitStep.h / RADETransmitStep.cpp all carry the identical text; the header reproduces it once with `--- From <path> ---` markers above to make the multi-file scope unambiguous). The DSP API surface that Tasks I2 / I3 / I4 will plug in (rade_open call shape, rade_n_features_in_out / rade_n_tx_out / rade_nin queries, lpcnet_encoder_create / lpcnet_encoder_destroy, fargan_init / FARGANState, RADE_COMP modem-sample shape, rade_text aux channel surface) all come from these four freedv-gui files. Class layout and Q_OBJECT shape come from AetherSDR `src/core/RADEEngine.h` [@0cd4559]; that lineage is recorded in `docs/attribution/aethersdr-reconciliation.md` Bucket A "Phase 3R Task I1 - RADE codec wrapper skeleton" subsection. NereusSDR divergences vs freedv-gui: the wrapper is a single QObject (not paired RX/TX pipeline-step classes) because the OpenHPSDR audio path is bi-directional from one channel rather than two; the upstream `realtime_fp<...>` callback function-pointer pattern is replaced with Qt6 signals; the upstream `PreAllocatedFIFO` / `Semaphore` / `std::thread utFeatureThread_` machinery is replaced with `QByteArray` accumulators and the Qt event loop (RADE's RX-side DSP is sample-rate-bounded, so a worker thread is not required when the audio engine already provides one). I2/I3/I4 will fill in the slot bodies. |
| src/core/FreeDVRadeReporterBridge.h | src/main.cpp + src/integrations/common/ReportingController.cpp + src/freedv_interface.cpp | main.cpp:1971-1996 (MainFrame::OnTimer RADE sync-only else-if), main.cpp:1884 (`(int)(g_snr + 0.5)` rounding), main.cpp:1982 (`m_reportCounter % 10` throttle), main.cpp:121 (`g_snr` single-station design), main.h:101 + defines.h:41 (`_REFRESH_TIMER_PERIOD = DT*1000 = 100 ms`), ReportingController.cpp:50 (`#define MODE_STRING "RADEV1"`), freedv_interface.cpp:62-63 (`FREEDV_MODE_RADE -> "RADEV1"`) [@77e793a]; main.cpp GPL v2.1 file header reproduced verbatim in the new file | port | Phase 3R-bridge. Drives the Path B rx_report upload that fills the gap freedv-gui handles in `MainFrame::OnTimer`: while RADE has sync but no callsign decoded via EOO yet, emit a Socket.IO `rx_report` with empty callsign + mode "RADEV1" + rounded SNR every 1 s (10 Hz timer modulo 10). Path A (callsign-decoded via EOO) is already driven by `RadioModel::onRadeTextDecoded` which calls `FreeDVReporterClient::sendRxReport` directly; the bridge does not duplicate it. Class shape (QObject + QTimer + slot methods) follows AetherSDR `src/core/FreeDvClient.h` [@0cd4559] for Qt6 lifecycle. NereusSDR divergence vs freedv-gui: upstream uses a single `g_snr` global driven from `MainFrame::OnTimer`'s 10 Hz heartbeat; NereusSDR sources SNR per-RADE-frame via `RadioModel::radeSnrChanged` and collapses to a single double member (multi-slice RADE is a 3F future per CLAUDE.md "RADE multi-slice ... Phase 3F future"). PSK Reporter is intentionally not fed from Path B - upstream uses `m_sharedReporterObject` (FreeDV only), not the `m_reporters[]` fan-out, because an empty callsign is not a meaningful PSK Reporter record. |
| src/core/FreeDVRadeReporterBridge.cpp | src/main.cpp:1971-1996, src/main.cpp:1884, src/main.cpp:1737 [@77e793a] | port | Phase 3R-bridge implementation. Slot bodies (`onRadeSyncChanged`, `onRadeSnrChanged`, `onMoxStateChanged`, `onTick`) and the `shouldEmitPathB_` gate composite (reportingEnabled + synced + !tx + reporter connected + !isnan(snr)) follow the upstream `if (m_sharedReporterObject && FREEDV_MODE_RADE && syncState)` gate at main.cpp:1971-1974 plus the `!(isnan(snrEstimate) || isinf(snrEstimate)) && syncState` gate at main.cpp:1737. `roundedSnrDb_` ports the `(int)(g_snr + 0.5)` rounding at main.cpp:1884 verbatim (C-style cast with the +0.5 nudge for round-half-up on positive values and toward-zero on negative). |
| src/core/RadeChannel.cpp | src/pipeline/RADEReceiveStep.cpp + src/pipeline/RADETransmitStep.cpp | RADEReceiveStep.cpp full file (BSD-2-Clause header, ctor at :70-150 - rade handle / FARGAN state / textPtr storage + FIFO allocation + frequency-offset bookkeeping; dtor at :152-175 - thread shutdown + FIFO cleanup; execute at :200-330 - input-FIFO drain into RADE_COMP buffer + rade_nin frame readiness + rade_rx call + feature accumulation + FARGAN synthesis + output-FIFO write; reset at :340-360 - FIFO clear) [@77e793a]; RADETransmitStep.cpp full file (ctor at :30-130 - rade handle / LPCNet state / pre-allocated speech FIFO; dtor at :140-148 - LPCNet encoder destroy + FIFO cleanup; execute at :150-260 - speech-FIFO drain into LPCNet encoder + 12-frame feature accumulation + rade_tx call + modem-output write; restartVocoder at :216-240 - EOO queue; reset at :242-247 - input-FIFO / output-FIFO / featureListIdx_ clear) [@77e793a] | port | Phase 3R Task I1 (skeleton), Task I2 (RX-path body), Task I3 (TX-path body). I1 shipped lifecycle bodies only. I2 fills in the RX path (24 kHz I/Q -> 8 kHz RADE_COMP -> rade_rx -> features -> FARGAN -> 16 kHz mono speech -> 24 kHz stereo output) following AetherSDR `src/core/RADEEngine.cpp:200-303` (feedRxAudio) [@0cd4559] as the structural template, cross-checked block-by-block against freedv-gui `src/pipeline/RADEReceiveStep.cpp:175-310` [@77e793a] for DSP-authority correctness. The two upstreams diverge in two places that the inline cite comments call out: (A) freedv-gui takes mono short PCM, sets imag=0.0, and uses `freq_shift_coh` to mix to baseband; AetherSDR takes stereo PCM and averages L+R to mono, also imag=0.0; NereusSDR takes interleaved I/Q float (already complex baseband from the OpenHPSDR DDC) and downsamples I and Q independently through `m_down24to8` / `m_down24to8Q` then interleaves into RADE_COMP. (B) freedv-gui drives output through pre-allocated FIFOs with a thread shutdown / IOPolicy block on macOS; AetherSDR uses QByteArray accumulators with implicit pacing. NereusSDR matches AetherSDR's accumulator model. I3 fills in the TX path (16 kHz mono int16 speech -> LPCNet feature extraction over LPCNET_FRAME_SIZE chunks -> NB_TOTAL_FEATURES feature accumulator -> rade_n_features_in_out-bounded drain to rade_tx -> 8 kHz RADE_COMP real-leg take -> 8 kHz->24 kHz stereo upsample via m_up8to24 -> txModemReady emit) following AetherSDR `src/core/RADEEngine.cpp:134-198` (feedTxAudio) [@0cd4559] as the structural template, cross-checked block-by-block against freedv-gui `RADETransmitStep.cpp` :216-247 (restartVocoder EOO-queue + reset FIFO-flush) [@77e793a] for the MOX-release flush contract. NereusSDR-architectural divergence vs AetherSDR for the TX path: input is already 16 kHz mono int16 from the WdspEngine TX pump per the plan, so AetherSDR's 24 kHz stereo float -> 16 kHz mono int16 conversion at :139-152 is dropped and the input bytes append straight into m_txAccum. resetTx() flushes m_txAccum + m_txFeatAccum + m_radeTxCallCount per AetherSDR :126-132 [@0cd4559]; freedv-gui's RADETransmitStep::reset additionally zeros featureListIdx_ and uses pre-allocated FIFOs, but the QByteArray accumulator model collapses both surfaces into the two `.clear()` calls. The embedded rade_text aux channel lands in I4. The skeleton bodies' structural pattern (out-of-line dtor calling `stop()`, ctor/dtor pair shape, accessor inlines, slot signatures, opaque FARGAN void* + new/delete pair) follows AetherSDR `src/core/RADEEngine.cpp` [@0cd4559]; that lineage is recorded in `docs/attribution/aethersdr-reconciliation.md` Bucket A "Phase 3R Task I1 - RADE codec wrapper skeleton", "Phase 3R Task I2 - RadeChannel RX path", "Phase 3R Task I2a - Resampler port", and "Phase 3R Task I3 - RadeChannel TX path" subsections. |

## Independently implemented - freedv-gui-like but not derived

Files whose behavior resembles freedv-gui but whose implementation was
written without consulting freedv-gui source. No per-file freedv-gui
header required. These rows are intentionally formatted with the file
path in column 2 (not column 1) so the header-verifier script does not
scan them.

| Behavioral resemblance | NereusSDR file | Basis of implementation |
| --- | --- | --- |
| (none yet) | | |
