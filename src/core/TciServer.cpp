// no-port-check: AetherSDR-derived NereusSDR file.  Transport lifecycle
// (start/stop/onNewConnection/onClientDisconnected) is adapted from
// AetherSDR src/core/TciServer.{h,cpp} [@0cd4559]; NereusSDR diverges in
// bind address, double-start contract, signal set, and client table type.
// Registered in docs/attribution/aethersdr-reconciliation.md.

// src/core/TciServer.cpp  (NereusSDR)
// NereusSDR-original — TCI WebSocket server implementation.
//
// Transport pattern ported from AetherSDR src/core/TciServer.{h,cpp} [@0cd4559].
// Per-client field set condensed from Thetis TCIServer.cs:684-790 [v2.10.3.13].
//
// Modification history (NereusSDR):
//   2026-05-10 — Phase 3J-1 Task 2.1 by J.J. Boyd (KG4VCF);
//                AI-assisted transformation via Anthropic Claude Code.
//   2026-09-23 — R3 Setup fix wave (R-R3-17, R-R3-21) by J.J. Boyd
//                (KG4VCF): the compat-flag seed is left to the computer
//                that owns the keys. AI-assisted transformation via
//                Anthropic Claude Code.
//   2026-09-23 - R3 receiver audio plan, Task 4 (R-R3-42, R-R3-21,
//                R-R3-25) by J.J. Boyd (KG4VCF): remote-window mode, a
//                per-receiver history read by each client at its own
//                position, left-channel mono, and the corrected seed note
//                (the Tci keys are this computer's). AI-assisted
//                transformation via Anthropic Claude Code.
//   2026-09-23 - R3 receiver audio fix wave (R-R3-42, R-R3-21) by J.J.
//                Boyd (KG4VCF): stereo at rates other than 48 kHz resampled
//                per channel (Thetis TCIServer.cs resampleRxAudioSamples);
//                remote rx_sensors from the Core's mirrored meter; a late
//                "cannot send" answer unsubscribes the apps and tells them.
//                AI-assisted transformation via Anthropic Claude Code.
//   2026-09-24 - R-R3-48 / R-R3-25: the station server tells the operator
//                why a TX profile or XIT change was not made; its extra
//                listeners go through deleteLater, not raw delete. J.J.
//                Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-24 - R-R3-48 follow-up: quiet listen attempts while the Core
//                retries its station listener; the main listener goes
//                through deleteLater, not raw delete; addListener adds an
//                address to a running server (rework). J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-25 - R-R3-49 / R-R3-42 (parity Task 3): in a remote window,
//                tx_profile_ex and tx_profiles_ex go to the apps when the
//                Core's active profile or profile list changes (Thetis
//                TXProfileChangedHandlers / TXProfilesChangedHandlers).
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-24 - Receiver and transmit gaps plan, Task 4 (R-R3-49): an
//                app's trx follows Thetis handleTrxMessage (ignored while
//                the transmitter is keyed; keys it otherwise, TX audio or
//                not) and unkeying releases the TX audio
//                (OnMoxPreChangeHandler); the Core's receive-only station
//                server named in the seed note. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-24 - Receiver and transmit gaps plan, Task 10 (R-R3-49): each
//                app's vfo, dds and tx_frequency updates pass through its
//                own update gap (Thetis udTCIRateLimit), read at start and
//                changed live. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-25 - R-R3-39 (station Task 32): the TX sensors' mic level is
//                TxChannel::txMeter, the transmit lane's last reading,
//                not a GetTXAMeter call on the event loop. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25 - D14 / R-R3-49: the mic level names TxMeterType::MicAvg,
//                mapped to its WDSP index by TxChannel::txMeter. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25 - R-R3-39: receive audio's WDSP resamplers (create, run,
//                destroy) move to the model's receive lane; a resampled
//                block is encoded there and sent back here in order. J.J.
//                Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25 - R-R3-39: stop() ends a TX audio holder's cycle as its
//                disconnect does (txAudioActiveClientChanged(nullptr), then
//                stopTxChrono), so the TCI transmit resampler is freed.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25 - D14 / R-R3-49: the TX sensors' mic level is Thetis's MIC
//                reading, max(-195, TXA_MIC_AV) (thetisTxReading). J.J.
//                Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-24 - Receiver and transmit gaps plan, Task 7 fix wave
//                (R-R3-49): the trx note says what an app's trx:N,false
//                does since Task 7 (it releases a TCI key only). J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-24 - Receiver and transmit gaps plan, Task 7 follow-up
//                (R-R3-49): a trx:N,true,tci that keyed nothing and left no
//                TCI level held gives the TX audio lock back and stops
//                TX_CHRONO. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                Claude Code.
//   2026-09-25 - Receiver and transmit gaps plan, Task 12 (R-R3-49): a
//                slice's frequency and centre changes send dds and if, and
//                each line reaches an app's update gap with the gate its
//                event named. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-25 - iPhone app Task 73 (R-IOS-02, ruling 5.13):
//                setSliceWriteGate. NereusSDR-original. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-25 - iPhone app plan Task 35 (R-IOS-13; the several-devices
//                design, ruling 8.14): a remote window forwards an app's
//                transmit to its Core as tx.key {trigger:"tci"}
//                (setRemoteTransmit, handleRemoteTrx); the TX audio lock
//                only after the Core admits the key; trx:N,false and the
//                lock holder's disconnect release only this window's key.
//                NereusSDR-original. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-25 - iPhone app plan Task 36 (R-IOS-13): through a remote
//                window the lock holder's transmit audio goes to the Core
//                on the window's microphone line (RemoteTransmit::audio).
//                NereusSDR-original. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  iPhone app plan Task 77 (R-IOS-02,
//                                    R-IOS-03, R-IOS-13): the holder rule
//                                    before the TX audio lock (ruling 8.14).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-28 - Desktop-host TCI receiver ownership and holder admission.
//                NereusSDR-original, AI-assisted via OpenAI Codex.
//   2026-09-28 - R-R3-46 / R-R3-11: the RX1 sensor takes slice 0's ADC
//                offset (rxMeterOffsetDbForSlice). J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-28 - Slice control plan Task 2: the hosting desktop's TCI
//                receivers are the slices SliceAccessPolicy lets the
//                station device change. NereusSDR-original. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - Level Cal: calibration_ex carries the meter and display
//                calibration and goes to apps when either changes. J.J.
//                Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30 - Fix round 1 (TX path): the trx intercept matches the
//                command name in any case, as TciProtocol does; a TCI key
//                no app owns ends with the server and with an app that
//                goes. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                Claude Code.
//   2026-09-30 - Fix round 2 (Critical 1, RD-C1): a remote window records
//                the app whose trx:N,true it forwarded (m_remoteKeyClient);
//                that app leaving releases the key on the Core, or the
//                Core's answer when it comes. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.

#ifdef HAVE_WEBSOCKETS

#include "TciServer.h"
#include "TciClientSession.h"
#include "TciProtocol.h"
#include "TciSendQueue.h"
#include "TciBinaryFrame.h"
#include "session/media/RemoteTciAudioStage.h"
#include "TciSensorManager.h"
#include "LogCategories.h"
#include "models/RadioModel.h"
#include "core/meters/SliceMeterPump.h"
#include "models/SliceModel.h"  // Phase 3J-1 closeout: SliceModel signal wireup for local broadcast.
#include "models/NotchModel.h"  // TNF section 6.4: master notch enable broadcast.
#include "models/TransmitModel.h"  // Phase 3J-1 closeout (review P2): MON / TUN broadcast wireup.
#include "MoxController.h"         // Phase 3J-1 closeout (review P2): MOX broadcast wireup.
#include "SliceOwnership.h"
#include "core/session/SliceAccessPolicy.h"
#include "core/safety/TxRefusal.h"
#include "MicProfileManager.h"     // R-R3-49 (parity Task 3): a remote window's TX profiles.
#include "TxSliceArbiter.h"        // Codex review round 6: tx_frequency follows the TX-bound slice.
#include "AudioEngine.h"           // Phase 3J-1 closeout (review P1 #1): volume change broadcast.
#include "TciVolume.h"             // tciLinearToDbVolume / tciAudioGainToDb for volume frames.
#include "WdspEngine.h"
#include "DspControlThread.h"
#include "wdsp_api.h"   // Phase 3J-1 closeout Item 15 (2026-05-12) — GetTXAMeter direct call
#include "RxChannel.h"
#include "TxChannel.h"
#include "AppSettings.h"  // Phase 18: TciIqSwap + TciAlwaysStreamIq flags
#include "session/media/RemoteAudioContext.h"  // R-R3-42: receiver stop reasons
#include "settings/ISettingsBackend.h"  // R3 Setup fix wave: seed only keys this computer owns

// Phase 16 Task 16.3 (sub-commit b): WDSP RESAMPLEF lifecycle.
// resample.h declares create_resampleF / destroy_resampleF / xresampleF, and
// the RESAMPLEF typedef.  The void*-opaque FV wrappers (create_resampleFV /
// xresampleFV / destroy_resampleFV) live in resample.c:342-360 [WDSP TAPR v1.29]
// but are NOT declared in resample.h — they are forward-declared here.
//
// create_resampleFV(in_rate, out_rate) calls create_resampleF(1, 0, 0, 0, in_rate,
// out_rate), so size=0 + null buffers are safe at construction time; xresampleFV
// sets in/out/size per-call.  Verified by reading resample.c:342-360.
extern "C" {
#include "resample.h"
// FV wrappers are not declared in resample.h — forward-declare them:
void* create_resampleFV(int in_rate, int out_rate);
void  xresampleFV(float* input, float* output, int numsamps, int* outsamps, void* ptr);
void  destroy_resampleFV(void* ptr);
}

#include <algorithm>
#include <cmath>
#include <tuple>
#include <set>
#include <ctime>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include <QElapsedTimer>
#include <QScopeGuard>
#include <QHostAddress>
#include <QTimer>
#include <QWebSocket>
#include <QWebSocketServer>
#include <QDateTime>

namespace NereusSDR {

// ── Constructor / destructor ─────────────────────────────────────────────────
//
// Phase 2 Task 2.1: constructor body is intentionally empty — no meter timers,
// no TX-chrono timer, no status-received wiring.  Those arrive in later phases:
//   - Phase 9:  meter broadcast timer (broadcastStatus at 200 ms)
//   - Phase 17: TX_CHRONO timer for WSJT-X
// AetherSDR src/core/TciServer.cpp:53-152 [@0cd4559] shows what the full
// constructor looks like; we port only what Phase 2 needs.

TciServer::TciServer(RadioModel* model, QObject* parent)
    : QObject(parent)
    , m_model(model)
    // From design doc §1 — TciServer owns one TciProtocol; it is the shared
    // dispatch engine across all clients (single-instance, transport-blind).
    , m_protocol(std::make_unique<TciProtocol>(model, this))
    , m_remoteWindow(model != nullptr && model->role() == RadioModel::Role::Remote)
{
    // R-R3-42: a remote window's receivers are the Core's.
    m_protocol->setRemoteWindow(m_remoteWindow);
    for (auto& history : m_rxHistory) {
        history.assign(static_cast<std::size_t>(kRxHistoryFrames) * 2, 0.0f);
    }

    // Phase 3J-1 closeout Item 12 (2026-05-12): seed per-slice RX gain atomics
    // to 1.0 (0 dB).  TciApplet pushes the persisted slice-A gain via
    // setSliceRxGainLinear on construction; this default is the fallback
    // before that hookup runs.
    for (int rx = 0; rx < kMaxTciRxSlices; ++rx) {
        m_sliceRxGainLinear[rx].store(1.0f, std::memory_order_release);
        m_sliceRxPeakAbs[rx].store(0.0f, std::memory_order_release);
    }
    refreshLocalAudioReceiverMap();

    // ── Phase 3J-1 bench fix (2026-05-11): seed TCI compat-flag defaults ────
    //
    // WSJT-X / JTDX / Hamlib's TCI driver gate TCI-audio mode on the server
    // identifier — they enable TCI audio ONLY when the server advertises as
    // ExpertSDR3 protocol + SunSDR2PRO device.  An unknown identifier
    // (Thetis / NereusSDR) makes WSJT-X fall back to non-TCI audio: the
    // radio keys via the trx command but WSJT-X never streams TX_AUDIO_STREAM
    // binary frames, and sends `trx:0,true;` (no `,tci` suffix) because it
    // never entered TCI-audio mode.
    //
    // These compat flags exist as Setup → CAT/Network/TCI checkboxes for
    // users who explicitly want the Thetis/NereusSDR identifier (some Thetis-
    // native loggers prefer it).  But the SAFE default for the most-common
    // client (WSJT-X) is ON.  Seed defaults to "True" on first launch if the
    // keys are absent — this ensures the settings persist to disk (so the
    // user's UI toggle has something concrete to flip) AND that an
    // unconfigured install works with WSJT-X out of the box.
    //
    // Idempotent: only seeds when key is absent; explicit user "False"
    // setting (toggled off in UI) is respected.
    //
    // R3 receiver audio plan, Task 4 (R-R3-42): the Tci keys belong to this
    // computer (SettingsScope), in a remote window as in a local one:
    // MainWindow builds a TciServer, the Core runs a receive-only station
    // server (StationTciController) whose TciServer seeds the Core's own
    // store here, and every reader of these two flags defaults to True. So
    // a remote window seeds them in its own store exactly as a local one
    // does. The guard below stays for
    // a key the installed remote backend does handle: before the Core's
    // settings arrive contains() is false for every one of its keys, and a
    // seed there would count as an edit made while the link was down (R3
    // Setup fix wave, R-R3-17 / R-R3-21).
    {
        auto& s = AppSettings::instance();
        const ISettingsBackend* const remote = s.remoteBackend();
        const auto seedIfAbsent = [&s, remote](const QString& key) {
            if ((remote != nullptr && remote->handlesKey(key)) || s.contains(key)) {
                return false;
            }
            s.setValue(key, QStringLiteral("True"));
            return true;
        };
        bool seeded = false;
        seeded = seedIfAbsent(QStringLiteral("TciEmulateExpertSDR3Protocol")) || seeded;
        seeded = seedIfAbsent(QStringLiteral("TciEmulateSunSDR2Pro")) || seeded;
        if (seeded) {
            qCInfo(lcTci) << "TciServer: seeded TCI compat-flag defaults "
                             "(ExpertSDR3 + SunSDR2PRO emulation) — required for "
                             "WSJT-X TCI-audio mode";
        }
    }

    // Task 10 (R-R3-49): the clock every app's update gap reads.
    m_gapClock.start();

    m_pingTimer = new QTimer(this);  // parented — destroyed with server

    // From Thetis TCIServer.cs:6001-6003 [v2.10.3.13] — PingFrameTimer callback
    // fires sendPingFrame("Thetis") for each connected client.
    // Per Thetis TCIServer.cs:2650-2654 inline comment: ping frames are every 20s
    // per RFC 6455; we don't expect a Pong back within any timeout — we use the
    // ping itself to surface a dead socket via Qt's automatic write-error path.
    connect(m_pingTimer, &QTimer::timeout, this, [this]() {
        for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
            // From Thetis TCIServer.cs:2784 [v2.10.3.13] — sendPingFrame enqueue.
            // Qt6's QWebSocket::ping handles RFC 6455 frame construction and
            // socket-state-based error suppression internally.
            it.key()->ping(QByteArrayLiteral("Thetis"));
        }
    });

    // Phase 14: shared outbound drain timer.
    // Each tick drains each client's TciSendQueue in priority order, capped
    // at kDrainMaxPerTick frames per client to avoid starving the event loop
    // when one client is flooded.
    //
    // Interval 5ms: Thetis uses a sender thread blocked on
    // m_outboundFrameEvent.WaitOne(20) (TCIServer.cs:1770 [v2.10.3.13]),
    // which wakes immediately on enqueue or after 20ms.  A 5ms timer drains
    // promptly without the per-thread overhead and stays well within TCI's
    // 20ms latency budget.
    m_drainTimer = new QTimer(this);
    m_drainTimer->setInterval(5);   // 5ms drain tick; see rationale above
    connect(m_drainTimer, &QTimer::timeout, this, [this]() {
        // Phase 15: collapse coalesced VFO updates into pending notifications
        // BEFORE per-client drain so the just-drained frames participate in
        // this tick. From Thetis TCIServer.cs:1722-1727 [v2.10.3.13].
        m_protocol->drainCoalescedNotifications();

        // Broadcast any drained notifications to all clients.
        // Without this, drainCoalescedNotifications() populates
        // m_pendingNotifications but nothing pumps it to the send queues.
        broadcastPendingNotifications();

        // Task 10 (R-R3-49): the waiting vfo / dds / tx_frequency lines whose
        // gap has passed (the Thetis one-shot timers, TCIServer.cs:6436-6439
        // [v2.10.3.15]), checked on this tick.
        {
            const qint64 nowMs = m_gapClock.elapsed();
            for (auto sit = m_clients.cbegin(); sit != m_clients.cend(); ++sit) {
                for (const QString& line : sit.value()->updateGap.takeDue(nowMs)) {
                    sit.value()->sendQueue.push(TciSendQueue::Priority::Control, line);
                }
            }
        }

        // Phase 14 per-client send-queue drain (unchanged):
        constexpr int kDrainMaxPerTick = 64;
        for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
            QWebSocket* ws    = it.key();
            auto&       session = it.value();
            QString frame;
            int drained = 0;
            while (drained < kDrainMaxPerTick && session->sendQueue.tryPop(&frame)) {
                ws->sendTextMessage(frame);
                // Phase 3J-1 closeout Item 2 (2026-05-12): firehose for
                // TciLogWindow.  Strip the trailing ';' for the log view.
                {
                    QString logLine = frame;
                    if (logLine.endsWith(QLatin1Char(';'))) {
                        logLine.chop(1);
                    }
                    emit messageLogged(QStringLiteral("out"), session->peer,
                                       logLine,
                                       QDateTime::currentMSecsSinceEpoch());
                }
                ++drained;
            }
            // Sync the legacy framesDropped field for Phase 22 ClientChainApplet.
            session->framesDropped = session->sendQueue.dropCount();
        }

        // Phase 16 Task 16.3 (sub-commit c): RX audio drain.
        // From Thetis TCIServer.cs:5444-5512 [v2.10.3.13] — the sendRXAudioStream
        // loop reads samples, resamples, encodes, and calls sendBinaryFrame.
        // NereusSDR replicates this per drain-tick rather than in a dedicated thread.
        //
        // R-R3-42: the rings are emptied once per tick into each receiver's
        // history, then every subscribed client takes its own next block
        // from its own read position (sendRxAudioBlock).
        const QPointer<TciServer> self(this);
        collectRxAudio();
        if (!self) { return; }
        if (m_remoteWindow) {
            // A send or operator notice can synchronously retire this
            // server. Remote draining is the final action in this tick.
            drainRemoteAudio();
            return;
        }
        for (auto cit = m_clients.begin(); cit != m_clients.end(); ++cit) {
            QWebSocket* ws = cit.key();
            TciClientSession& session = *cit.value();
            for (int rx : std::as_const(session.audioStreamEnabled)) {
                sendRxAudioBlock(ws, cit.value(), rx);
            }
        }
    });

    // Phase 19: RX sensor broadcast timer.
    //
    // From Thetis TCIServer.cs:2554-2566 [v2.10.3.13] — setRxSensorsEnabled
    // creates a System.Threading.Timer(RxSensorsTimerCallback, null, 0, intervalMs)
    // when enabled is true.
    //
    // NereusSDR equivalent: a QTimer on the main thread. Default interval 200 ms
    // matches Thetis clsTCISensorManager._rxIntervalMs (TCIServer.cs:491 [v2.10.3.13]).
    // Timer is started in start() and stopped in stop() so it fires only when the
    // server is running.
    //
    // Phase 19 stub: emits placeholder rx_sensors:0,-100.0; to each subscribed
    // client. Phase 24+ wires real RadioModel meter signals here.
    m_rxSensorTimer = new QTimer(this);
    m_rxSensorTimer->setInterval(200);  // default 200ms; updated by rx_sensors_enable:true,<ms>;
    connect(m_rxSensorTimer, &QTimer::timeout, this, [this]() {
        // From Thetis RxSensorsTimerCallback (TCIServer.cs:2587-2616 [v2.10.3.13]):
        // iterate listeners, call sendRxSensors / sendRxChannelSensors for each
        // enabled listener.
        //
        // Phase 3J-1 closeout Item 15 (2026-05-12): pull real S-meter dBm from
        // WDSP via RxChannel::getMeter(RxMeterType::SignalAvg).  Matches
        // Thetis's CalculateRXMeter rx1Main_sig at dsp.cs:932-942 [v2.10.3.13]
        // (RXA_S_AV == SignalAvg).  Same value MeterPoller emits to VfoWidget
        // -- single source of truth for the S-meter.  Falls back to -140 dBm
        // (WDSP noise floor convention) if WDSP isn't initialized or the
        // channel doesn't exist yet.
        //
        // RXOffset port (2026-05-20): apply the same Thetis-faithful offset
        // MeterPoller uses for the SMeter/MeterWidget paths, so TCI clients
        // see the same dBm number as the on-screen S-meter.  Cite: Thetis
        // console.cs:46824 [v2.10.3.13] which adds `+ offset` to every
        // CalculateRXMeter(SIGNAL_STRENGTH/AVG_SIGNAL_STRENGTH) read in
        // the MultiMeter2UpdateRX1 loop (which also feeds TCIServer
        // sensors via Display.tciRX1Sig).  Without this, NereusSDR's TCI
        // clients would see raw ADC dBFS while the GUI shows antenna dBm.
        double rx1Dbm = -140.0;
        // R-R3-42: a remote window has no receiver of its own to meter; it
        // reports the Core's reading it mirrors (fix wave).
        if (m_remoteWindow) {
            rx1Dbm = remoteReceiverLevelDbm();
        } else if (m_desktopHostMode && m_model) {
            if (const SliceModel* slice = m_model->sliceById(desktopSliceForReceiver(0))) {
                rx1Dbm = slice->signalAverageDbm();
            }
        } else if (m_model) {
            if (auto* wdsp = m_model->wdspEngine()) {
                if (auto* rx = wdsp->rxChannel(0)) {
                    // R-R3-46 / R-R3-11: channel 0 is slice 0's; its
                    // ADC's offset.
                    rx1Dbm = rx->getMeter(RxMeterType::SignalAvg)
                           + m_model->rxMeterOffsetDbForSlice(0);
                }
            }
        }

        for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
            auto& session = it.value();
            if (!session->rxSensorsEnabled) { continue; }

            // From Thetis: sendRxSensors(0, rx1Main_sig) (TCIServer.cs:2600 [v2.10.3.13])
            const QString frame = TciSensorManager::formatRxSensors(0, rx1Dbm);
            session->sendQueue.push(TciSendQueue::Priority::Control, frame);

            // Also emit the channel form (dual-emit pattern).
            // From Thetis: sendRxChannelSensors(0, 0, sig, avg, peak) (TCIServer.cs:2601 [v2.10.3.13])
            const QString chanFrame = TciSensorManager::formatRxChannelSensors(0, 0, rx1Dbm);
            session->sendQueue.push(TciSendQueue::Priority::Control, chanFrame);

            const QString chanExFrame =
                TciSensorManager::formatRxChannelSensorsEx(0, 0, rx1Dbm, rx1Dbm, rx1Dbm);
            session->sendQueue.push(TciSendQueue::Priority::Control, chanExFrame);
        }
    });

    // ── Phase 3J-1 bench fix (2026-05-10): TX_CHRONO timer ────────────────────
    //
    // Ported from AetherSDR src/core/TciServer.cpp (verified working with
    // WSJT-X).  One TCI TX block is 2048 floats == 1024 stereo frames at
    // 48 kHz == 21.333 ms.  A fixed 21 ms QTimer runs ~1.6% fast and warps
    // digital-mode tones, so we poll more frequently (5 ms) and emit frames
    // from a monotonic elapsed-time accumulator.
    m_txChronoTimer = new QTimer(this);
    m_txChronoTimer->setTimerType(Qt::PreciseTimer);
    m_txChronoTimer->setInterval(5);  // kTxChronoPollMs — poll faster than period
    connect(m_txChronoTimer, &QTimer::timeout, this, [this]() {
        // Local copy guards against onClientDisconnected nulling the pointer
        // between the check and the send (race with main-thread disconnect).
        QWebSocket* client = m_txChronoClient.data();
        if (!client) { m_txChronoTimer->stop(); return; }

        if (!m_txChronoClock.isValid()) {
            m_txChronoClock.start();
            return;
        }

        // kTxChronoPeriodNs = 1024 stereo frames * 1e9 / 48000 Hz
        constexpr qint64 kTxChronoPeriodNs = (1024LL * 1000000000LL) / 48000LL;
        m_txChronoAccumNs += m_txChronoClock.nsecsElapsed();
        m_txChronoClock.restart();

        while (m_txChronoAccumNs >= kTxChronoPeriodNs) {
            sendTxChronoFrame(client);
            m_txChronoAccumNs -= kTxChronoPeriodNs;
        }
    });

    // Phase 19: TX sensor broadcast timer.
    //
    // From Thetis TCIServer.cs:2569-2581 [v2.10.3.13] — setTxSensorsEnabled
    // creates a System.Threading.Timer(TxSensorsTimerCallback, null, 0, intervalMs)
    // when enabled is true.
    //
    // NereusSDR equivalent: a QTimer on the main thread. Default interval 200 ms
    // matches Thetis clsTCISensorManager._txIntervalMs (TCIServer.cs:492 [v2.10.3.13]).
    // Timer is started in start() and stopped in stop(). Phase 24+ gates on MOX
    // state (m_txAudioActiveClient / RadioModel::moxChanged).
    //
    // Phase 19 stub: always-on; emits placeholder tx_sensors:0,-100.0,0.0,0.0,1.0;
    // to each subscribed client. TODO Phase 24+: gate on MOX + wire real TX meters.
    m_txSensorTimer = new QTimer(this);
    m_txSensorTimer->setInterval(200);  // default 200ms; updated by tx_sensors_enable:true,<ms>;
    connect(m_txSensorTimer, &QTimer::timeout, this, [this]() {
        // From Thetis TxSensorsTimerCallback (TCIServer.cs:2618-2628 [v2.10.3.13]):
        // iterate listeners, call sendTxSensors for each enabled listener.
        //
        // Phase 3J-1 closeout Item 14 (2026-05-12): MOX gate.  Thetis only
        // emits TX sensors while the radio is keyed; the all-on default
        // floods listeners with spurious zero-power frames at idle and
        // wastes 5 frames/sec × clients of bandwidth.  Gate on
        // RadioStatus::isTransmitting (covers MOX from any PTT source, not
        // just TCI-mutex-holder).  No-clients fast-path also skips work.
        if (!m_model || !m_model->radioStatus().isTransmitting()) {
            return;
        }

        // Phase 3J-1 closeout Item 15 (2026-05-12): pull real readings:
        //   mic level:   WDSP TXA MicAvg (dBm convention matches RX side)
        //   fwd watts:   RadioStatus::forwardPowerWatts (from PA-meter loop)
        //   peak watts:  RadioStatus::forwardPowerWatts (no separate peak
        //                tracker in NereusSDR yet; emit current = peak)
        //   SWR:         RadioStatus::swrRatio (1.0 minimum)
        // From Thetis cmaster/dsp.cs:999-1029 [v2.10.3.13] CalculateTXMeter
        // (TXA_MIC_AV) and console.cs PA-meter loop powerChanged.
        // TxChannel doesn't expose a getMeter() overload like RxChannel
        // does -- callers go through GetTXAMeter directly (see MeterPoller
        // pattern at MeterPoller.cpp:321-323).  WdspEngine::kTxChannelId is
        // WDSP.id(1,0) per Thetis dsp.cs:926-944.  Only call when the TX
        // channel exists to avoid a WDSP nullptr deref against an
        // unallocated stage.
        //
        // R-R3-39: through the TX channel, which reads the meter on the
        // transmit lane and hands back the lane's last reading, so this
        // timer never waits on WDSP.
        double micDbm = -140.0;
        if (auto* wdsp = m_remoteWindow ? nullptr : m_model->wdspEngine()) {
            if (auto* tx = wdsp->txChannel(WdspEngine::kTxChannelId)) {
                // D14, R-R3-49: Thetis sends its MIC reading
                // (TCIServer.cs MeterReadingsChanged reads Reading.MIC),
                // max(-195, TXA_MIC_AV) (thetisTxReading, WdspTypes.h).
                micDbm = thetisTxReading(ThetisTxReading::Mic,
                                         [tx](TxMeterType meter) { return tx->txMeter(meter); });
            }
        }
        const double fwdWatts  = m_model->radioStatus().forwardPowerWatts();
        const double swrRatio  = m_model->radioStatus().swrRatio();

        for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
            auto& session = it.value();
            if (!session->txSensorsEnabled) { continue; }

            // From Thetis: sendTxSensors(0, micLevelDbm, powerWatts, peakPowerWatts, swr)
            // (TCIServer.cs:2625 [v2.10.3.13])
            const QString frame = TciSensorManager::formatTxSensors(
                0, micDbm, fwdWatts, fwdWatts, swrRatio);
            session->sendQueue.push(TciSendQueue::Priority::Control, frame);
        }
    });

    // Phase 3J-1 review P2.3: wire the DSP-thread audio tap and the IQ tap at
    // construction time via the shared helper.  The helper is also called from
    // start() so that a stop() → start() cycle reconnects the taps that stop()
    // explicitly severs.
    hookAudioAndIqTaps();

    // Phase 3J-1 closeout (2026-05-22): wire SliceModel signals into the TCI
    // broadcast queue so operator-side tuning / mode / filter / AGC changes
    // propagate to connected clients.  Bench bug: client showed init-burst
    // freq but did not see the local VFO move after connect.
    // From Thetis TCIServer.cs:6730-6790 [v2.10.3.15] -- the +40 Console
    // event subscriptions that drive sendXxx on every property change.
    hookSliceBroadcasts();

    // Radio-global ChangedHandlers (MOX, TUN, MON, AF volume, IQ sample
    // rate, connection state, RX2 enable) -- the slice-agnostic counterparts
    // to hookSliceBroadcasts above.  See hookGlobalBroadcasts implementation
    // for the per-handler Thetis cite chain.
    hookGlobalBroadcasts();
}

// ── hookAudioAndIqTaps() ─────────────────────────────────────────────────────
//
// Phase 3J-1 review P2.3: extracted from the original constructor tap-wiring
// block.  Connect:
//   (a) RxChannel::audioFrameReady → TciServer::onAudioFrameReady
//       (Qt::DirectConnection — runs on the DSP thread)
//   (b) RadioModel::rawIqData → TciServer::onRawIqDataReceived
//       (Qt::QueuedConnection — marshals to main thread; see note below)
//
// This method is idempotent: already-tapped channels are skipped; if
// m_iqTapConnected is true the IQ tap is already
// connected.  The double-start guard in start() (m_server already non-null →
// return false) prevents re-entry in production, but idempotency here is the
// belt to the suspenders.
//
// Called from: constructor AND start().
// Taps are severed in stop():
//   - audio: QObject::disconnect(rxCh, nullptr, this, nullptr) for each source
//   - IQ:    QObject::disconnect(m_model, nullptr, this, nullptr)
// After stop() + start() this method reconnects them.

void TciServer::hookAudioAndIqTaps()
{
    if (!m_model) { return; }
    // R-R3-42: a remote window's receivers are on the Core. Receive audio
    // comes from the Core's receiver streams (setRemoteReceiverAudio), and
    // raw I/Q is not offered, so neither local tap is hooked.
    if (m_remoteWindow) { return; }

    // ── (a) RX audio tap ────────────────────────────────────────────────────
    //
    // Phase 16 Task 16.3 (sub-commit c): hook the RX audio tap from RxChannel.
    // WdspEngine may not be initialized yet at call time. We connect
    // once it is, then hook audioFrameReady with Qt::DirectConnection so the
    // slot runs on the DSP thread and can push into AudioRingSpsc non-blockingly.
    //
    // Idempotency guard: each connected channel is skipped. Ordinary local
    // mode retains its original channel-0 tap; desktop hosting follows the
    // station device's slices across the WDSP channel pool.
    {
        WdspEngine* wdsp = m_model->wdspEngine();
        if (wdsp) {
            auto hookAudioTap = [this, wdsp]() {
                const int channelCount = m_desktopHostMode
                    ? WdspEngine::kMaxSliceChannels : 1;
                for (int channel = 0; channel < channelCount; ++channel) {
                    RxChannel* rxCh = wdsp->rxChannel(channel);
                    if (rxCh && !m_audioTapSources.contains(rxCh)) {
                        connect(rxCh, &RxChannel::audioFrameReady,
                                this, &TciServer::onAudioFrameReady,
                                Qt::DirectConnection);
                        // The DSP callback maps the physical channel to the
                        // hosted station device's logical receiver atomically.
                        m_audioTapSources.append(rxCh);
                        qCInfo(lcTci) << "TciServer: RX audio tap connected to RxChannel"
                                      << channel;
                    }
                }
            };

            if (wdsp->isInitialized()) {
                hookAudioTap();
            } else if (!m_wdspInitConn) {
                // Phase 3J-1 bench fix (2026-05-10): use Qt::QueuedConnection so
                // this lambda fires on the next event-loop tick rather than
                // synchronously during emit.  Two listeners are registered on
                // WdspEngine::initializedChanged:
                //   (1) TciServer (this one, registered at MainWindow ctor time)
                //   (2) RadioModel::connectToRadio at RadioModel.cpp:1525 — its
                //       lambda calls m_wdspEngine->createRxChannel(0, ...).
                // (1) is registered FIRST and with the default AutoConnection
                // (== DirectConnection on same thread) would fire synchronously
                // BEFORE (2), at which point wdsp->rxChannel(0) is still null —
                // the audio tap then silently no-ops and never re-arms, leaving
                // the TCI audio drain with nothing to feed.  Forcing this
                // listener onto the queued path lets (2) complete inside the
                // synchronous emit, after which our singleShot-style queued
                // lambda finds the freshly-created RxChannel and hooks it.
                m_wdspInitConn = connect(wdsp, &WdspEngine::initializedChanged,
                                         this, [this, hookAudioTap](bool init) {
                    if (init) {
                        hookAudioTap();
                        disconnect(m_wdspInitConn);
                        m_wdspInitConn = {};
                    }
                }, Qt::QueuedConnection);
            }
        }
    }

    // ── (b) IQ tap ───────────────────────────────────────────────────────────
    //
    // Phase 18 Task 18.1: hook the IQ tap from RadioModel::rawIqData.
    // We use Qt::QueuedConnection so the slot always fires on the main thread
    // (TciServer owner thread).  RadioModel emits rawIqData on the FFT worker
    // thread; m_clients and QWebSocket must only be accessed on the main thread.
    // The QVector is implicitly shared so the queued copy is O(1).
    // Divergence from design doc §Phase 18 Qt::DirectConnection noted here.
    //
    // Idempotency guard: m_iqTapConnected — reset in stop(), set here.
    if (!m_iqTapConnected) {
        connect(m_model, &RadioModel::rawIqDataForStream,
                this, &TciServer::onRawIqDataReceived,
                Qt::QueuedConnection);
        m_iqTapConnected = true;
        qCInfo(lcTci) << "TciServer: IQ tap connected to RadioModel::rawIqData";
    }
}

// ── hookSliceBroadcasts() / wireSliceForBroadcast() ──────────────────────────
//
// Phase 3J-1 closeout (2026-05-22): bench bug fix.  Header comments document
// the architectural intent and the upstream Thetis cite chain
// (TCIServer.cs:6730-6790 [v2.10.3.15]).

void TciServer::hookSliceBroadcasts()
{
    if (!m_model || !m_protocol) {
        return;
    }

    // Wire each existing slice.  Idempotent: wireSliceForBroadcast skips
    // slices already in m_broadcastWiredSlices.
    const auto existingSlices = m_model->slices();
    for (SliceModel* slice : existingSlices) {
        if (slice) {
            wireSliceForBroadcast(slice, slice->sliceIndex());
        }
    }

    // Connect once -- new slices added after TciServer construction get wired
    // via this lambda.  RadioModel::sliceAdded fires after the slice is
    // pushed into m_slices, so sliceById(index) returns the live pointer.
    connect(m_model, &RadioModel::sliceAdded, this, [this](int index) {
        if (auto* slice = m_model->sliceById(index)) {
            wireSliceForBroadcast(slice, index);
        }
        QTimer::singleShot(0, this, [this]() { hookAudioAndIqTaps(); });
    });

    // Codex review round 6, PR #293: a TX handoff changes tx_frequency
    // without anybody tuning anything.
    //
    // tx_frequency is now sourced from whichever slice drives the
    // transmitter, so moving TX from A to B changes it even though neither
    // slice's frequency moved. Without this, a client would keep the old
    // value until the new TX slice happened to be retuned, which on a parked
    // slice could be never.
    //
    // Uses the untagged tx_frequency path deliberately: the transmitter can
    // be handed to Slice C or beyond, which are internal and have no trx:N on
    // the wire, so re-publishing the full VFO triplet here would announce a
    // receiver the client was never told about. tx_frequency carries no
    // receiver index and is correct for any slice.
    if (TxSliceArbiter* arb = m_model->txSliceArbiter()) {
        connect(arb, &TxSliceArbiter::txBoundSliceChanged, this,
                [this](int /*oldId*/, int newId) {
            SliceModel* slice = m_model ? m_model->sliceById(newId) : nullptr;
            if (!slice || !m_protocol) { return; }
            m_protocol->enqueueLocalBroadcastTxFrequency(
                static_cast<qint64>(slice->frequency()));
        });
    }

    // ── Master notch enable (rx_nf_enable:, BOTH rx indices) ────────────────
    // Source: Thetis console.TNFChangedHandlers subscription at
    // TCIServer.cs:6771 [v2.10.3.15], routed to OnTnfChanged
    // (TCIServer.cs:7686-7696 [v2.10.3.15]) which calls NfChanged on every
    // listener; NfChanged sends sendRxNfEnable(0, ...) AND
    // sendRxNfEnable(1, ...) (TCIServer.cs:1315-1320 [v2.10.3.15]) because
    // the flag is global despite the per-rx command shape.  GetMNF returns
    // TNFActive for either index:
    //   // mnf enabled globally  [original inline comment from console.cs:52319]
    //
    // Wired HERE and not in wireSliceForBroadcast because NotchModel is
    // radio-global: wireSliceForBroadcast runs once per slice, so the same
    // connect placed there would emit N frame pairs per flip.
    if (!m_notchBroadcastWired) {
        if (NotchModel* notch = m_model->notchModel()) {
            connect(notch, &NotchModel::globalEnabledChanged, this,
                    [this](bool on) {
                        const QString boolStr = on ? QStringLiteral("true")
                                                   : QStringLiteral("false");
                        m_protocol->enqueueLocalBroadcast(
                            QStringLiteral("rx_nf_enable:0,%1;").arg(boolStr));
                        m_protocol->enqueueLocalBroadcast(
                            QStringLiteral("rx_nf_enable:1,%1;").arg(boolStr));
                    });
            m_notchBroadcastWired = true;
        }
    }
}

// Codex review round 6, PR #293. See TciProtocol::enqueueLocalBroadcastVfo.
bool TciServer::sliceDrivesTx(int sliceId) const
{
    if (!m_model) { return false; }
    const TxSliceArbiter* arb = m_model->txSliceArbiter();
    if (!arb) {
        // No arbiter means single-slice, where slice 0 is the transmitter by
        // construction. Preserves the pre-3F behaviour rather than silently
        // dropping tx_frequency for every client on a one-slice radio.
        return sliceId == 0;
    }
    return arb->txBoundSliceId() == sliceId;
}

void TciServer::wireSliceForBroadcast(SliceModel* slice, int sliceId)
{
    if (!slice || !m_protocol) {
        return;
    }

    // Only the receivers this server actually advertises get TAGGED frames.
    // Codex review round 6, PR #293.
    //
    // The init burst negotiates trx_count:2, and Phase 3J-1 design doc §1.2
    // records that as a locked decision with Slice C/D/E internal. This
    // function wired every slice it found regardless, so a five-slice radio
    // pushed unsolicited vfo, modulation, rx_filter_band and gain frames
    // tagged receiver 2, 3 and 4 at clients that had sized their state from
    // the 2 we told them. Well-formed frames outside the negotiated surface
    // are worse than no frames: a strict client may reject the session, and a
    // lenient one silently keeps state it will never be asked about.
    //
    // Clamped rather than raising the count, because the count is the locked
    // half of the decision and raising it means widening the per-receiver
    // init burst to match. That is the "advanced-user opt-in to trx_count:4"
    // the design doc puts out of scope, not a review fix.
    // Idempotency: skip if already wired.
    for (const auto& wp : m_broadcastWiredSlices) {
        if (wp.data() == slice) {
            return;
        }
    }
    const bool exposed =
        (sliceId >= 0 && sliceId < TciProtocol::kExposedReceiverCount)
        || (m_desktopHostMode && desktopReceiverForSlice(sliceId) >= 0);
    if (!exposed) { return; }
    m_broadcastWiredSlices.append(QPointer<SliceModel>(slice));

    // Helper: a string-format frame template used by most one-shot handlers.
    // Each connect() captures the sliceId by value so per-slice routing is
    // stable across slice add/remove cycles.

    // ── VFO frequency (the bench bug repro) ─────────────────────────────────
    // Routes through the coalescer (Layer 3 dedup) because rotary-encoder
    // spin can fire dozens of frequencyChanged signals per second.
    // Source: Thetis Console.CentreFrequencyHandlers / TXFrequncyChangedHandlers
    // subscription at TCIServer.cs:6731 + 6757 [v2.10.3.15], routed to
    // OnCentreFrequencyChanged + OnTXFrequencyChanged.
    //
    // An unexposed slice still reaches the untagged tx_frequency pair when it
    // holds the transmitter. Those two frames carry no receiver index, so they
    // are the one thing about Slice C the wire may legitimately hear, and
    // dropping them would mean a client following tx_frequency froze the
    // moment the operator handed TX to an internal slice and tuned it.
    //
    // Task 12 (R-R3-49): a tune is a VFO event (if + vfo) and, when it moved
    // the pan with it, a centre event (dds + if) too. The centre event is
    // queued on every tune and dropped at drain when the centre did not move,
    // because which of the two a tune was is only known once RadioModel and
    // the pan have settled the slice's offset.
    connect(slice, &SliceModel::frequencyChanged, this,
            [this, sliceId, exposed](double freq) {
                const auto hz = static_cast<qint64>(freq);
                if (exposed) {
                    m_protocol->enqueueLocalBroadcastVfo(
                        sliceId, hz, sliceDrivesTx(sliceId));
                    m_protocol->enqueueLocalBroadcastCentre(sliceId);
                } else if (sliceDrivesTx(sliceId)) {
                    m_protocol->enqueueLocalBroadcastTxFrequency(hz);
                }
            });

    // Task 12 (R-R3-49): the slice's offset from its stream centre moved (a
    // pan drag, a pan that follows the VFO, a restream): a centre event,
    // as Thetis's CentreFrequency setter fires CentreFrequencyHandlers
    // (console.cs:10781-10789 [v2.10.3.15]) and TCIServer answers with
    // dds and if (TCIServer.cs:7364-7388 [v2.10.3.15]).
    if (exposed) {
        connect(slice, &SliceModel::shiftOffsetHzChanged, this,
                [this, sliceId](double) {
                    m_protocol->enqueueLocalBroadcastCentre(sliceId);
                });
    }

    // Everything below this point is tagged with a receiver index, so it
    // stops at the advertised count.
    if (!exposed) {
        qCDebug(lcTci) << "TciServer: slice" << sliceId
                       << "is beyond trx_count"
                       << TciProtocol::kExposedReceiverCount
                       << "- tagged frames suppressed (internal slice)";
        return;
    }

    // ── DSP mode (modulation: line) ─────────────────────────────────────────
    // Source: Thetis ModeChangedHandlers (implicit via Console.RX1DSPMode/
    // RX2DSPMode setter side effects); the TCI server re-reads via sendMode.
    // NereusSDR fires SliceModel::dspModeChanged directly; we re-read via
    // SliceModel::modeName for the canonical uppercase string used by TCI.
    connect(slice, &SliceModel::dspModeChanged, this,
            [this, sliceId](NereusSDR::DSPMode mode) {
                const QString modeStr = SliceModel::modeName(mode);
                m_protocol->enqueueLocalBroadcast(
                    QStringLiteral("modulation:%1,%2;").arg(sliceId).arg(modeStr));
            });

    // ── Filter (rx_filter_band: line) ───────────────────────────────────────
    // Source: Thetis console.FilterChangedHandlers at TCIServer.cs:6732
    // [v2.10.3.15] routed to OnFilterChanged.
    connect(slice, &SliceModel::filterChanged, this,
            [this, sliceId](int low, int high) {
                m_protocol->enqueueLocalBroadcast(
                    QStringLiteral("rx_filter_band:%1,%2,%3;")
                        .arg(sliceId).arg(low).arg(high));
            });

    // ── AGC mode (agc_mode: line) ───────────────────────────────────────────
    // Source: Thetis AGCModeChangedHandlers at TCIServer.cs:6763 [v2.10.3.15]
    // routed to OnAGCModeChanged.  Re-read via the RadioModel Q_INVOKABLE
    // shim then run through TciProtocol::tciAgcModeForWire to produce the
    // lowercase wire token Thetis emits via agcModeToTciMode
    // (TCIServer.cs:2260-2279 [v2.10.3.15]).  Review P2 #2 fix (2026-05-22):
    // without the normalize step, broadcasts read "agc_mode:0,MED;" instead
    // of "agc_mode:0,normal;".
    connect(slice, &SliceModel::agcModeChanged, this,
            [this, sliceId](NereusSDR::AGCMode) {
                QString modeStr;
                QMetaObject::invokeMethod(m_model, "agcMode",
                                          Qt::DirectConnection,
                                          Q_RETURN_ARG(QString, modeStr),
                                          Q_ARG(int, sliceId));
                m_protocol->enqueueLocalBroadcast(
                    QStringLiteral("agc_mode:%1,%2;")
                        .arg(sliceId)
                        .arg(TciProtocol::tciAgcModeForWire(modeStr)));
            });

    // ── AGC gain / threshold (agc_gain: line) ───────────────────────────────
    // Source: Thetis AGCGainChangedHandlers at TCIServer.cs:6752 [v2.10.3.15]
    // routed to OnAGCGainChanged.
    connect(slice, &SliceModel::agcThresholdChanged, this,
            [this, sliceId](int dBu) {
                m_protocol->enqueueLocalBroadcast(
                    QStringLiteral("agc_gain:%1,%2;").arg(sliceId).arg(dBu));
            });

    // ── Squelch enable / level ─────────────────────────────────────────────
    // Source: Thetis SQLChangedHandlers + SQLLevelChangedHandlers at
    // TCIServer.cs:6768-6769 [v2.10.3.15] routed to OnSqlChanged / OnSqlLevelChanged.
    connect(slice, &SliceModel::ssqlEnabledChanged, this,
            [this, sliceId](bool on) {
                m_protocol->enqueueLocalBroadcast(
                    QStringLiteral("sql_enable:%1,%2;")
                        .arg(sliceId)
                        .arg(on ? QStringLiteral("true") : QStringLiteral("false")));
            });
    connect(slice, &SliceModel::ssqlThreshChanged, this,
            [this, sliceId](double dB) {
                m_protocol->enqueueLocalBroadcast(
                    QStringLiteral("sql_level:%1,%2;").arg(sliceId).arg(static_cast<int>(dB)));
            });

    // ── Lock (lock: + vfo_lock: lines) ──────────────────────────────────────
    // Source: Thetis VfoALockChangedHandlers + VfoBLockChangedHandlers at
    // TCIServer.cs:6766-6767 [v2.10.3.15] routed to OnVfoALockChanged /
    // OnVfoBLockChanged.  NereusSDR collapses per-channel lock onto the slice;
    // emit both the lock:rx form and the vfo_lock:rx,chan cross-product so
    // clients tracking either format see the change.
    connect(slice, &SliceModel::lockedChanged, this,
            [this, sliceId](bool locked) {
                const QString boolStr = locked ? QStringLiteral("true")
                                                : QStringLiteral("false");
                m_protocol->enqueueLocalBroadcast(
                    QStringLiteral("lock:%1,%2;").arg(sliceId).arg(boolStr));
                m_protocol->enqueueLocalBroadcast(
                    QStringLiteral("vfo_lock:%1,0,%2;").arg(sliceId).arg(boolStr));
                m_protocol->enqueueLocalBroadcast(
                    QStringLiteral("vfo_lock:%1,1,%2;").arg(sliceId).arg(boolStr));
            });

    // ── Mute (rx_mute: line) ────────────────────────────────────────────────
    // Source: Thetis MuteChangedHandlers at TCIServer.cs:6743 [v2.10.3.15]
    // routed to OnMuteChanged; NereusSDR's per-slice mute flows through
    // SliceModel::mutedChanged.
    connect(slice, &SliceModel::mutedChanged, this,
            [this, sliceId](bool on) {
                m_protocol->enqueueLocalBroadcast(
                    QStringLiteral("rx_mute:%1,%2;")
                        .arg(sliceId)
                        .arg(on ? QStringLiteral("true") : QStringLiteral("false")));
            });

    // ── RIT enable / offset ─────────────────────────────────────────────────
    // Source: Thetis RITChangedHandlers + RITValueChangedHandlers at
    // TCIServer.cs:6753 + 6755 [v2.10.3.15] routed to OnRITChanged /
    // OnRITValueChanged.  Thetis treats RIT as radio-global; NereusSDR
    // SliceModel exposes ritEnabledChanged / ritHzChanged per slice.  The
    // RadioModel::ritEnable() / ritOffset() Q_INVOKABLE shims return the
    // active-slice value, so a per-slice signal emits a single notification
    // that matches the radio-global semantic clients expect.
    connect(slice, &SliceModel::ritEnabledChanged, this,
            [this, sliceId](bool on) {
                m_protocol->enqueueLocalBroadcast(
                    QStringLiteral("rit_enable:%1,%2;")
                        .arg(sliceId)
                        .arg(on ? QStringLiteral("true") : QStringLiteral("false")));
            });
    connect(slice, &SliceModel::ritHzChanged, this,
            [this, sliceId](int hz) {
                m_protocol->enqueueLocalBroadcast(
                    QStringLiteral("rit_offset:%1,%2;").arg(sliceId).arg(hz));
            });

    // ── XIT enable / offset ─────────────────────────────────────────────────
    // Source: Thetis XITChangedHandlers + XITValueChangedHandlers at
    // TCIServer.cs:6754 + 6756 [v2.10.3.15] routed to OnXITChanged /
    // OnXITValueChanged.  Same per-slice/global divergence as RIT.
    connect(slice, &SliceModel::xitEnabledChanged, this,
            [this, sliceId](bool on) {
                m_protocol->enqueueLocalBroadcast(
                    QStringLiteral("xit_enable:%1,%2;")
                        .arg(sliceId)
                        .arg(on ? QStringLiteral("true") : QStringLiteral("false")));
            });
    connect(slice, &SliceModel::xitHzChanged, this,
            [this, sliceId](int hz) {
                m_protocol->enqueueLocalBroadcast(
                    QStringLiteral("xit_offset:%1,%2;").arg(sliceId).arg(hz));
            });

    // ── Balance / audio pan (rx_balance: line) ─────────────────────────────
    // Source: Thetis BalanceChangedHandlers at TCIServer.cs:6747 [v2.10.3.15]
    // routed to OnBalanceChanged.  Thetis emits two frames per slice (chan 0
    // and chan 1) via sendRxBalance at TCIServer.cs:2187-2191 [v2.10.3.13]
    // with the 40.0 - (pan * 0.8) transform.  NereusSDR's audioPan is already
    // F2 dB in TCI space (mock + production parity); emit both channels.
    connect(slice, &SliceModel::audioPanChanged, this,
            [this, sliceId](double pan) {
                const QString balStr = QString::number(pan, 'f', 2);
                m_protocol->enqueueLocalBroadcast(
                    QStringLiteral("rx_balance:%1,0,%2;").arg(sliceId).arg(balStr));
                m_protocol->enqueueLocalBroadcast(
                    QStringLiteral("rx_balance:%1,1,%2;").arg(sliceId).arg(balStr));
            });

    // ── NB (Noise Blanker) -- rx_nb_enable ─────────────────────────────────
    // Source: Thetis NBChangedHandlers at TCIServer.cs:6760 [v2.10.3.15]
    // routed to OnNbChanged.  NereusSDR's nbModeChanged carries a NbMode enum
    // (None / NB / NB2 / SNB); any non-None mode reports "enabled" to TCI per
    // sendRxNbEnable at TCIServer.cs:1901-1905 [v2.10.3.13].  Production
    // SliceModel exposes the enum directly; treat None as off.
    connect(slice, &SliceModel::nbModeChanged, this,
            [this, sliceId](NereusSDR::NbMode mode) {
                const bool on = (mode != NereusSDR::NbMode::Off);
                m_protocol->enqueueLocalBroadcast(
                    QStringLiteral("rx_nb_enable:%1,%2;")
                        .arg(sliceId)
                        .arg(on ? QStringLiteral("true") : QStringLiteral("false")));
            });

    // ── NR (Noise Reduction) + ANF (Automatic Notch) -- single signal ──────
    // Source: Thetis NRChangedHandlers + ANFChangedHandlers at
    // TCIServer.cs:6759 + 6761 [v2.10.3.15].  We re-read the RadioModel
    // rxNr / rxNrIndex / rxAnf shims to emit Thetis-faithful tri-frame
    // (rx_nr_enable + rx_nr_enable_ex + rx_anf_enable) so the wire format
    // matches the init burst exactly.  Each emit uses tciAgcModeForWire-style
    // normalisation: any active NR slot is "on" via the Thetis "0 = off"
    // collapse already applied in buildInitialRadioStateLines.
    //
    // ANF rides along here because the NR slot changing can change what the
    // client should believe about the whole noise-reduction group, and the
    // init burst emits the three lines together.  It is no longer the ONLY
    // source: Sub-Epic J Task 1 gave ANF its own SliceModel::anfEnabled
    // Q_PROPERTY, independent of the activeNr slot enum, so activeNrChanged
    // stopped firing on a pure ANF toggle and rx_anf_enable went stale for
    // every connected client until the next reconnect.  The dedicated
    // anfEnabledChanged connect below this block is the live source now,
    // matching Thetis's separate ANFChangedHandlers subscription
    // (TCIServer.cs:6761 [v2.10.3.15]).  Emitting rx_anf_enable from both is
    // intentional and harmless: TCI notifications are idempotent state
    // reports, and keeping it here preserves the grouped tri-frame the init
    // burst golden pins.
    connect(slice, &SliceModel::activeNrChanged, this,
            [this, sliceId](NereusSDR::NrSlot /*slot*/) {
                bool nrOn = false;
                int  nrIdx = 0;
                bool anfOn = false;
                QMetaObject::invokeMethod(m_model, "rxNr",
                                          Qt::DirectConnection,
                                          Q_RETURN_ARG(bool, nrOn),
                                          Q_ARG(int, sliceId));
                if (nrOn) {
                    QMetaObject::invokeMethod(m_model, "rxNrIndex",
                                              Qt::DirectConnection,
                                              Q_RETURN_ARG(int, nrIdx),
                                              Q_ARG(int, sliceId));
                }
                QMetaObject::invokeMethod(m_model, "rxAnf",
                                          Qt::DirectConnection,
                                          Q_RETURN_ARG(bool, anfOn),
                                          Q_ARG(int, sliceId));
                const QString trueStr  = QStringLiteral("true");
                const QString falseStr = QStringLiteral("false");
                m_protocol->enqueueLocalBroadcast(
                    QStringLiteral("rx_nr_enable:%1,%2;")
                        .arg(sliceId).arg(nrOn ? trueStr : falseStr));
                m_protocol->enqueueLocalBroadcast(
                    QStringLiteral("rx_nr_enable_ex:%1,%2,%3;")
                        .arg(sliceId).arg(nrOn ? trueStr : falseStr).arg(nrIdx));
                m_protocol->enqueueLocalBroadcast(
                    QStringLiteral("rx_anf_enable:%1,%2;")
                        .arg(sliceId).arg(anfOn ? trueStr : falseStr));
            });

    // ── ANF (Automatic Notch) -- rx_anf_enable, own signal ─────────────────
    // Source: Thetis ANFChangedHandlers at TCIServer.cs:6761 [v2.10.3.15]
    // routed to OnAnfChanged.  Upstream subscribes to ANF separately from NR
    // because they are separate console controls; NereusSDR matched that
    // shape in Sub-Epic J Task 1 by giving ANF its own SliceModel property,
    // but the broadcast wiring was left hanging off activeNrChanged, which
    // that same change stopped firing on a pure ANF toggle.  So flipping ANF
    // from the DSP menu or a VFO flag moved the radio but told no connected
    // client, until reconnect.
    connect(slice, &SliceModel::anfEnabledChanged, this,
            [this, sliceId](bool on) {
                m_protocol->enqueueLocalBroadcast(
                    QStringLiteral("rx_anf_enable:%1,%2;")
                        .arg(sliceId)
                        .arg(on ? QStringLiteral("true") : QStringLiteral("false")));
            });

    // ── Per-receiver AF gain -- rx_volume ──────────────────────────────────
    // Source: Thetis console.RXGainChangedHandlers at TCIServer.cs:6780
    // [v2.10.3.15] routed to OnRxAfGainChanged (TCIServer.cs:7722-7733),
    // whose listener emits one frame for whichever receiver's gain actually
    // changed:
    //   int chan = is_subrx ? 1 : 0;
    //   double db = audioGainToDb(gain / 100f);
    //   sendRxVolume(rx - 1, chan, db);
    // (RxAfGainChanged, TCIServer.cs:1118-1124 [v2.10.3.15]).
    //
    // Mapping: TCI receiver N -> slice id N, the convention documented at
    // TciProtocol::buildInitialRadioStateLines and used by every per-rx shim
    // in RadioModel.cpp.  Both channel slots of the receiver are emitted
    // because NereusSDR has no sub-receiver model, so one SliceModel::afGain
    // stands in for both -- the same collapse the init burst applies, and the
    // same one Thetis itself applies to RX2 (sendRxVolume(1, 1, rx2vol) reuses
    // rx2vol, TCIServer.cs:2557 [v2.10.3.15], because there is no RX2-sub
    // slider).  Emitting only channel 0 would leave a client's channel-1 slot
    // pinned at whatever the init burst reported.
    //
    // rx_volume uses the LOG curve (audioGainToDb), not the LINEAR curve the
    // master volume: line uses.
    connect(slice, &SliceModel::afGainChanged, this,
            [this, sliceId](int gain) {
                const int linear = qBound(0, gain, 100);
                const double gainDb = tciAudioGainToDb(linear / 100.0);
                const QString gainStr = QString::number(gainDb, 'f', 2);
                m_protocol->enqueueLocalBroadcast(
                    QStringLiteral("rx_volume:%1,0,%2;").arg(sliceId).arg(gainStr));
                m_protocol->enqueueLocalBroadcast(
                    QStringLiteral("rx_volume:%1,1,%2;").arg(sliceId).arg(gainStr));
            });

    // ── APF (Audio Peak Filter) -- rx_apf_enable ───────────────────────────
    // Source: Thetis APFChangedHandlers at TCIServer.cs:6770 [v2.10.3.15]
    // routed to OnApfChanged.  SliceModel exposes apfEnabledChanged directly.
    connect(slice, &SliceModel::apfEnabledChanged, this,
            [this, sliceId](bool on) {
                m_protocol->enqueueLocalBroadcast(
                    QStringLiteral("rx_apf_enable:%1,%2;")
                        .arg(sliceId)
                        .arg(on ? QStringLiteral("true") : QStringLiteral("false")));
            });

    // ── BIN (Binaural) -- rx_bin_enable ────────────────────────────────────
    // Source: Thetis BINChangedHandlers at TCIServer.cs:6762 [v2.10.3.15]
    // routed to OnBinChanged.  SliceModel exposes binauralEnabledChanged.
    connect(slice, &SliceModel::binauralEnabledChanged, this,
            [this, sliceId](bool on) {
                m_protocol->enqueueLocalBroadcast(
                    QStringLiteral("rx_bin_enable:%1,%2;")
                        .arg(sliceId)
                        .arg(on ? QStringLiteral("true") : QStringLiteral("false")));
            });

    // ── DIGL / DIGU click-tune offsets ─────────────────────────────────────
    // Source: Thetis DIGLOffsetChangedHandlers + DIGUOffsetChangedHandlers at
    // TCIServer.cs:6772-6773 [v2.10.3.15].  Thetis treats these as radio-
    // global (DIGLClickTuneOffset / DIGUClickTuneOffset Console properties);
    // NereusSDR stores per-slice on SliceModel.  Broadcast only the active
    // slice's change to match the init-burst active-slice semantic; per-slice
    // changes on the inactive slice are silently dropped (operator only sees
    // one DIGL value at a time in UI).
    connect(slice, &SliceModel::diglOffsetHzChanged, this,
            [this, slice](int hz) {
                if (m_model && m_model->activeSlice() == slice) {
                    m_protocol->enqueueLocalBroadcast(
                        QStringLiteral("digl_offset:%1;").arg(hz));
                }
            });
    connect(slice, &SliceModel::diguOffsetHzChanged, this,
            [this, slice](int hz) {
                if (m_model && m_model->activeSlice() == slice) {
                    m_protocol->enqueueLocalBroadcast(
                        QStringLiteral("digu_offset:%1;").arg(hz));
                }
            });
}

// ── hookGlobalBroadcasts() ───────────────────────────────────────────────────
//
// Radio-global ChangedHandlers from Thetis TCIServer.cs:6727-6788 [v2.10.3.15]
// that hookSliceBroadcasts doesn't cover.  Each Thetis Console.XxxChangedHandlers
// += OnXxxChanged subscription maps to one Qt connect() here.
//
// Production wiring: TciServer outlives RadioModel inside MainWindow but
// stop() severs every RadioModel -> this connection via the wholesale
// QObject::disconnect(m_model, nullptr, this, nullptr) call.  So this method
// is callable on every start() and resets the m_globalBroadcastsWired flag.

void TciServer::hookGlobalBroadcasts()
{
    if (!m_model || !m_protocol || m_globalBroadcastsWired) {
        return;
    }

    // Task 35: in a remote window, the Core ending this window's key on its
    // own (a safety stop, a take) ends it here too.
    if (m_remoteWindow) {
        connect(m_model, &RadioModel::transmittingChanged, this, [this](bool on) {
            if (!on && m_remoteKeyEpoch != 0) {
                qCInfo(lcTci) << "TciServer: the Core ended this window's key";
                m_remoteKeyEpoch = 0;
                endRemoteKey();
            }
        });
    }

    // ── Level Cal (calibration_ex: line) ───────────────────────────────────
    // From Thetis TCIServer.cs:6785-6786 [v2.10.3.15]:
    //   console.MeterCalOffsetChangedHandlers += OnCalibrationChanged;
    //   console.DisplayOffsetChangedHandlers += OnCalibrationChanged;
    // OnCalibrationChanged (TCIServer.cs:7770-7781 [v2.10.3.15]) calls
    // CalibrationChanged(rx) with the handler's 1-based rx. NereusSDR keeps
    // one receive calibration for both receivers, so it sends the line for
    // receiver 0 and receiver 1 on every change.
    connect(m_model, &RadioModel::levelCalibrationChanged, this, [this]() {
        m_protocol->enqueueLocalBroadcast(m_protocol->calibrationExLineFor(0));
        m_protocol->enqueueLocalBroadcast(m_protocol->calibrationExLineFor(1));
    });

    // ── MOX (trx: line) ────────────────────────────────────────────────────
    // Source: Thetis MoxChangeHandlers at TCIServer.cs:6727 [v2.10.3.15]
    // routed to OnMoxChangeHandler -> sendMOX.  MoxController is the
    // authoritative MOX state holder; moxStateChanged fires at the END of
    // the TX/RX walk (Codex P1: TXEnable boundary).  Format from
    // sendMOX at TCIServer.cs:2207-2211 [v2.10.3.13].
    if (auto* mox = m_model->moxController()) {
        // Receiver and transmit gaps plan, Task 4 (R-R3-49): unkeying, by any
        // app or by the operator, releases the TX audio, so the next app's
        // trx takes it.
        // From Thetis TCIServer.cs:7325-7338 [v2.10.3.15] --
        // OnMoxPreChangeHandler calls SyncTciPttToMox(expectedMox) on every
        // listener; SyncTciPttToMox (TCIServer.cs:5965-5981 [v2.10.3.15])
        // clears m_tciPttActive and calls ReleaseActiveTxAudioListener when
        // expectedMox is false.
        connect(mox, &MoxController::moxChanging, this,
                [this](int /*rx*/, bool /*oldMox*/, bool newMox) {
                    if (!newMox && m_desktopHostMode) {
                        if (m_desktopKeyHeld) { ++m_desktopKeyGeneration; }
                        m_desktopKeyHeld = false;
                        m_desktopKeyClient = nullptr;
                    }
                    if (newMox || m_txAudioActiveClient.isNull()) {
                        return;
                    }
                    m_txAudioActiveClient = nullptr;
                    qCInfo(lcTci) << "TciServer: TX audio mutex released: the transmitter"
                                     " is unkeying";
                    emit txAudioActiveClientChanged(nullptr);
                    stopTxChrono();
                });
        connect(mox, &MoxController::moxStateChanged, this,
                [this](bool on) {
                    const QString boolStr =
                        on ? QStringLiteral("true") : QStringLiteral("false");
                    // Thetis sendMOX(0, ...) + sendMOX(1, MOX && VFOBTX &&
                    // bRX2Enabled).  Until VFOBTX state plumbing lands the
                    // rx==1 channel is always false (the !VFOBTX path of
                    // sendInitialRadioState matches this).
                    m_protocol->enqueueLocalBroadcast(
                        QStringLiteral("trx:0,%1;").arg(boolStr));
                    m_protocol->enqueueLocalBroadcast(
                        QStringLiteral("trx:1,false;"));
                    // MOX gates rx_enable / tx_enable per Thetis sendRXEnable
                    // / sendTXEnable at TCIServer.cs:2515-2516 + 2618-2619
                    // [v2.10.3.15].  Re-emit both to mirror the init burst
                    // when MOX flips.
                    // RX2 is on when receiver 1 has a slice, as the
                    // protocol reads it, not from the connection's active
                    // RX count (0 until a connect sets it from the
                    // persisted count, and not tied to slices).  tx_enable
                    // also follows the init burst's transmitRefused() gate.
                    const bool rx2en = m_protocol->rx2EnabledNow();
                    const QString notMox =
                        on ? QStringLiteral("false") : QStringLiteral("true");
                    m_protocol->enqueueLocalBroadcast(
                        QStringLiteral("rx_enable:0,%1;").arg(notMox));
                    m_protocol->enqueueLocalBroadcast(
                        QStringLiteral("rx_enable:1,%1;")
                            .arg((rx2en && !on) ? QStringLiteral("true")
                                                 : QStringLiteral("false")));
                    m_protocol->enqueueLocalBroadcast(
                        QStringLiteral("tx_enable:0,%1;")
                            .arg((!m_protocol->transmitRefused() && !on)
                                     ? QStringLiteral("true")
                                     : QStringLiteral("false")));
                    m_protocol->enqueueLocalBroadcast(
                        QStringLiteral("tx_enable:1,%1;")
                            .arg((!m_protocol->transmitRefused() && rx2en && !on)
                                     ? QStringLiteral("true")
                                     : QStringLiteral("false")));
                });
    }

    // ── TUNE (tune: line) ──────────────────────────────────────────────────
    // Source: Thetis TuneChangedHandlers at TCIServer.cs:6737 [v2.10.3.15]
    // routed to OnTuneChanged -> sendTune.  NereusSDR's TUN state lives on
    // TransmitModel as the m_tune bool + tuneChanged signal (mirrors Thetis
    // chkTUN.Checked at console.cs:18677-18684 [v2.10.3.15]).
    connect(&m_model->transmitModel(), &TransmitModel::tuneChanged, this,
            [this](bool on) {
                const QString boolStr =
                    on ? QStringLiteral("true") : QStringLiteral("false");
                m_protocol->enqueueLocalBroadcast(
                    QStringLiteral("tune:0,%1;").arg(boolStr));
                m_protocol->enqueueLocalBroadcast(
                    QStringLiteral("tune:1,false;"));  // !VFOBTX path
            });

    // ── TX profile (tx_profile_ex: / tx_profiles_ex: lines), remote window ─
    // Source: Thetis TXProfileChangedHandlers + TXProfilesChangedHandlers
    // at TCIServer.cs:6782-6783 [v2.10.3.15] routed to OnTXProfileChanged /
    // OnTXProfilesChanged (TCIServer.cs:7746-7767 [v2.10.3.15]) ->
    // sendTXProfile / sendTXProfiles (TCIServer.cs:5053-5069
    // [v2.10.3.15]). R-R3-49 / R-R3-42 (parity Task 3): a remote window's
    // profiles are the Core's (its MicProfileManager mirrors them), so the
    // apps hear a profile change once the Core has made it, including one
    // an app asked for (TciProtocol does not echo tx_profile_ex there). A
    // local window keeps its immediate echo.
    if (m_remoteWindow) {
        if (MicProfileManager* profiles = m_model->micProfileManager()) {
            connect(profiles, &MicProfileManager::activeProfileChanged, this,
                    [this](const QString& name) {
                        if (name.isEmpty()) { return; }
                        m_protocol->enqueueLocalBroadcast(
                            TciProtocol::buildTxProfileExLine(name));
                    });
            connect(profiles, &MicProfileManager::profileListChanged, this,
                    [this, profiles]() {
                        const QStringList names = profiles->profileNames();
                        if (names.isEmpty()) { return; }
                        m_protocol->enqueueLocalBroadcast(
                            TciProtocol::buildTxProfilesExLine(names));
                    });
        }
    }

    // ── MON enable + volume (mon_enable: / mon_volume: lines) ──────────────
    // Source: Thetis MONChangedHandlers + MONVolumeChangedHandlers at
    // TCIServer.cs:6744-6745 [v2.10.3.15] routed to OnMONChanged /
    // OnMONVolumeChanged.  TransmitModel holds both.
    connect(&m_model->transmitModel(), &TransmitModel::monEnabledChanged, this,
            [this](bool on) {
                m_protocol->enqueueLocalBroadcast(
                    QStringLiteral("mon_enable:%1;")
                        .arg(on ? QStringLiteral("true")
                                : QStringLiteral("false")));
            });
    connect(&m_model->transmitModel(), &TransmitModel::monitorVolumeChanged, this,
            [this](float volume) {
                // sendMONVolume uses linearToDbVolume(TXAF) where TXAF is the
                // 0..100 linear slider (TCIServer.cs:2655 [v2.10.3.15]).
                // monitorVolume is float [0..1]; scale before passing.
                const int linear =
                    qBound(0, static_cast<int>(volume * 100.0f + 0.5f), 100);
                const double db = tciLinearToDbVolume(linear);
                m_protocol->enqueueLocalBroadcast(
                    QStringLiteral("mon_volume:%1;")
                        .arg(QString::number(db, 'f', 1)));
            });

    // ── AF volume (volume: line only) ──────────────────────────────────────
    // Source: Thetis VolumeChangedHandlers at TCIServer.cs:6746 [v2.10.3.15]
    // routed to OnVolumeChanged -> sendVolume.  AudioEngine is the live
    // volume holder.
    //
    // rx_volume is deliberately NOT emitted here.  Thetis's OnVolumeChanged
    // (TCIServer.cs:7806-7817 [v2.10.3.15]) calls only sendVolume and never
    // touches rx_volume: the master AF slider is a separate console field
    // from the per-receiver gains.  This handler used to re-broadcast all
    // four rx_volume slots from that one global value, which collapsed Task
    // 10's per-slice init burst (buildInitialRadioStateLines in
    // TciProtocol.cpp) back to a single shared number the moment the operator
    // touched the master slider after connect.  The live per-rx source is a
    // separate event upstream -- console.RXGainChangedHandlers routed to
    // OnRxAfGainChanged (TCIServer.cs:6780 + 7722-7733 [v2.10.3.15]) -- whose
    // NereusSDR analog is the afGainChanged connect in wireSliceForBroadcast.
    if (auto* audio = m_model->audioEngine()) {
        connect(audio, &AudioEngine::volumeChanged, this,
                [this](float volume) {
                    const int linear =
                        qBound(0, static_cast<int>(volume * 100.0f + 0.5f), 100);
                    // volume: line uses LINEAR curve (linearToDbVolume).
                    const double volDb = tciLinearToDbVolume(linear);
                    m_protocol->enqueueLocalBroadcast(
                        QStringLiteral("volume:%1;")
                            .arg(QString::number(volDb, 'f', 1)));
                });
    }

    // ── HW sample rate (iq_samplerate: + audio_samplerate: implicit) ───────
    // Source: Thetis HWSampleRateChangedHandlers at TCIServer.cs:6739
    // [v2.10.3.15] routed to OnHWSampleRateChanged.  Thetis emits
    // sendIQSampleRate + the IF limits update.  NereusSDR fires
    // RadioModel::wireSampleRateChanged with a double.
    connect(m_model, &RadioModel::wireSampleRateChanged, this,
            [this](double) {
                const int rateInt = publishedIqRate();
                m_protocol->enqueueLocalBroadcast(
                    QStringLiteral("iq_samplerate:%1;").arg(rateInt));
                // sendIFLimits follows in Thetis (TCIServer.cs:2535-2536
                // [v2.10.3.13]).  halfSample = SampleRateRX1 / 2.
                const int halfSample = rateInt / 2;
                m_protocol->enqueueLocalBroadcast(
                    QStringLiteral("if_limits:%1,%2;")
                        .arg(-halfSample).arg(halfSample));
            });

    // ── Power (start; / stop; line) ────────────────────────────────────────
    // Source: Thetis PowerChangeHanders at TCIServer.cs:6735 [v2.10.3.15]
    // routed to OnPowerChangeHander -> sendStartStop.  NereusSDR collapses
    // Power-on and connection-up into a single concept; emit start; on
    // connect, stop; on disconnect.  Architectural divergence already
    // documented in RadioModel::powerOn() shim and the buildInitialRadioState
    // start;/stop; emission site.
    connect(m_model, &RadioModel::connectionStateChanged, this,
            [this](NereusSDR::ConnectionState newState) {
                // Mirror buildInitialRadioStateLines: emit one or the other
                // (never both).  ConnectionState::Connected -> start; any
                // other state -> stop;.  Format from sendStart / sendStop in
                // TCIServer.cs:5786-5792 [v2.10.3.15].
                if (newState == NereusSDR::ConnectionState::Connected) {
                    m_protocol->enqueueLocalBroadcast(QStringLiteral("start;"));
                } else {
                    m_protocol->enqueueLocalBroadcast(QStringLiteral("stop;"));
                }
            });

    // ── RX2 enabled (rx_enable:1 + tx_enable:1) ───────────────────────────
    // Source: Thetis RX2EnabledChangedHandlers at TCIServer.cs:6741
    // [v2.10.3.15] routed to OnRX2EnabledChanged -> RX2EnabledChange.
    // RX2 is on when receiver 1 has a slice (TciProtocol::rx2EnabledNow), so
    // the lines follow every event that can change that: a slice added or
    // removed, the receiver map moving with slice ownership while hosting,
    // and the active RX count. They go out only when the answer flips, as
    // Thetis's handler fires only on a change.
    m_rx2EnabledSent = m_protocol->rx2EnabledNow();
    connect(m_model, &RadioModel::activeRxCountChanged, this,
            [this](int) { refreshRx2Enabled(); });
    connect(m_model, &RadioModel::sliceAdded, this,
            [this](int) { refreshRx2Enabled(); });
    connect(m_model, &RadioModel::sliceRemoved, this, [this](int) {
        refreshRx2Enabled();
        // The ownership marks settle after the removal signal.
        QTimer::singleShot(0, this, [this]() { refreshRx2Enabled(); });
    });
    QObject::disconnect(m_rx2OwnershipConnection);
    if (SliceOwnership* ownership = m_model->sliceOwnership()) {
        m_rx2OwnershipConnection = connect(ownership, &SliceOwnership::markChanged, this,
            [this](int, const QByteArray&, const QByteArray&) { refreshRx2Enabled(); });
    }

    m_globalBroadcastsWired = true;
}

void TciServer::refreshRx2Enabled()
{
    // A stopped server queues nothing (stop() also drops the ownership
    // hook; the singleShot after a slice removal can still land here).
    if (!m_model || !m_protocol || !m_globalBroadcastsWired) { return; }
    const bool en = m_protocol->rx2EnabledNow();
    if (en == m_rx2EnabledSent) { return; }
    m_rx2EnabledSent = en;
    // From Thetis TCIServer.cs:842-847 [v2.10.3.15] (RX2EnabledChange):
    //   sendRXEnable(1, enabled);
    //   sendTXEnable(1, enabled && !consoleThreadSafe.MOX);
    // Only those two lines: rx_channel_enable and lock are not re-sent on
    // this path (console.cs:37522 fires RX2EnabledChangedHandlers alone).
    // tx_enable also follows transmitRefused(), as the first lines do.
    bool mox = false;
    QMetaObject::invokeMethod(m_model, "mox",
                              Qt::DirectConnection,
                              Q_RETURN_ARG(bool, mox));
    const auto flag = [](bool on) {
        return on ? QStringLiteral("true") : QStringLiteral("false");
    };
    m_protocol->enqueueLocalBroadcast(QStringLiteral("rx_enable:1,%1;").arg(flag(en)));
    m_protocol->enqueueLocalBroadcast(QStringLiteral("tx_enable:1,%1;")
        .arg(flag(en && !mox && !m_protocol->transmitRefused())));
}

TciServer::~TciServer()
{
    stop();
    // R-R3-42: stop() releases every receiver stream it held; this covers
    // a server that was never started (nothing is held then either).
    for (int rx = 0; rx < kMaxTciRxSlices; ++rx) {
        if (m_remoteRequested[rx] && m_remoteAudio.release) {
            if (m_remoteStage[rx]) { m_remoteStage[rx]->publish({}); }
            m_remoteStage[rx].reset();
            m_remoteRequested[rx] = false;
            m_remoteAudio.release(rx, this);
        }
    }
}

// ── start() ─────────────────────────────────────────────────────────────────
//
// From AetherSDR src/core/TciServer.cpp:159-181 [@0cd4559] — transport pattern.
// NereusSDR diverges from AetherSDR in two ways:
//   1. Bind address: default QHostAddress::LocalHost, but the
//      bindAddress overload accepts any valid address (Phase 3J-1 closeout
//      Item 1).  The Setup → CAT/Network/TCI bind-interface dropdown
//      writes the operator's choice into AppSettings, and MainWindow
//      resolves it to QHostAddress before calling this overload.
//      Loopback remains the safe default for operators who don't touch
//      the new dropdown.
//   2. double-start contract: return false + log warning (AetherSDR returns
//      m_server->isListening(), treating double-start as idempotent-true).
//      NereusSDR rejects double-start so the caller can detect misuse early.

bool TciServer::start(quint16 port)
{
    return start(QHostAddress(QHostAddress::LocalHost), port);
}

bool TciServer::start(const QList<QHostAddress>& bindAddresses, quint16 port)
{
    if (bindAddresses.isEmpty()) {
        return false;
    }
    if (!start(bindAddresses.constFirst(), port)) {
        return false;
    }
    // R-R3-48: the remaining addresses share the first one's port, so an
    // app reaches the same server whichever address it uses.
    const quint16 boundPort = m_server->serverPort();
    for (qsizetype i = 1; i < bindAddresses.size(); ++i) {
        auto* extra = new QWebSocketServer(QStringLiteral("NereusSDR-TCI"),
                                           QWebSocketServer::NonSecureMode, this);
        if (!extra->listen(bindAddresses.at(i), boundPort)) {
            const QString errStr = extra->errorString();
            if (m_quietListenAttempts) {
                qCDebug(lcTci) << "TciServer: failed to listen on"
                               << bindAddresses.at(i).toString() << "port" << boundPort
                               << errStr;
            } else {
                qCWarning(lcTci) << "TciServer: failed to listen on"
                                 << bindAddresses.at(i).toString() << "port" << boundPort
                                 << errStr;
            }
            // M6: Qt ownership (parented to this); it never listened.
            extra->deleteLater();
            stop();
            emit errorOccurred(errStr);
            return false;
        }
        connect(extra, &QWebSocketServer::newConnection,
                this, &TciServer::onNewConnection);
        m_extraServers.append(extra);
        if (m_quietListenAttempts) {
            qCDebug(lcTci) << "TciServer: also listening on"
                           << bindAddresses.at(i).toString() << "port" << boundPort;
        } else {
            qCInfo(lcTci) << "TciServer: also listening on"
                          << bindAddresses.at(i).toString() << "port" << boundPort;
        }
    }
    return true;
}

bool TciServer::addListener(const QHostAddress& address)
{
    if (!m_server) {
        return false;
    }
    const quint16 port = m_server->serverPort();
    auto* extra = new QWebSocketServer(QStringLiteral("NereusSDR-TCI"),
                                       QWebSocketServer::NonSecureMode, this);
    if (!extra->listen(address, port)) {
        if (m_quietListenAttempts) {
            qCDebug(lcTci) << "TciServer: failed to listen on" << address.toString()
                           << "port" << port << extra->errorString();
        } else {
            qCWarning(lcTci) << "TciServer: failed to listen on" << address.toString()
                             << "port" << port << extra->errorString();
        }
        extra->deleteLater();   // Qt ownership; it never listened
        return false;
    }
    connect(extra, &QWebSocketServer::newConnection, this, &TciServer::onNewConnection);
    m_extraServers.append(extra);
    qCInfo(lcTci) << "TciServer: also listening on" << address.toString() << "port" << port;
    emit listenersChanged();
    return true;
}

QList<QHostAddress> TciServer::listenAddresses() const
{
    QList<QHostAddress> addresses;
    if (m_server && m_server->isListening()) {
        addresses.append(m_server->serverAddress());
    }
    for (QWebSocketServer* extra : m_extraServers) {
        if (extra && extra->isListening()) {
            addresses.append(extra->serverAddress());
        }
    }
    return addresses;
}

void TciServer::setSliceWriteGate(std::function<bool(int sliceId)> gate)
{
    m_externalSliceWriteGate = std::move(gate);
    refreshSliceWriteGate();
}

void TciServer::refreshSliceWriteGate()
{
    if (!m_desktopHostMode && !m_externalSliceWriteGate) {
        m_protocol->setSliceWriteGate({});
        return;
    }
    m_protocol->setSliceWriteGate([this](int sliceId) {
        const bool owned = !m_desktopHostMode || desktopReceiverForSlice(sliceId) >= 0;
        return owned && (!m_externalSliceWriteGate || m_externalSliceWriteGate(sliceId));
    });
}

int TciServer::desktopSliceForReceiver(int receiver) const
{
    if (!m_model || !m_model->sliceOwnership() || receiver < 0
        || receiver >= TciProtocol::kExposedReceiverCount) { return -1; }
    // Slice control plan Task 2: the slices the station device may change
    // (its own, and those it runs held for an absent device), never one it
    // could only see or hear.
    const SliceOwnership& ownership = *m_model->sliceOwnership();
    QList<int> owned;
    for (int sliceId : ownership.liveSlices()) {
        if (SliceAccessPolicy::mayChange(ownership, SliceOwnership::stationDevice(), sliceId)) {
            owned.append(sliceId);
        }
    }
    std::sort(owned.begin(), owned.end());
    return receiver < owned.size() ? owned.at(receiver) : -1;
}

int TciServer::desktopReceiverForSlice(int sliceId) const
{
    for (int receiver = 0; receiver < TciProtocol::kExposedReceiverCount; ++receiver) {
        if (desktopSliceForReceiver(receiver) == sliceId) { return receiver; }
    }
    return -1;
}

void TciServer::refreshLocalAudioReceiverMap()
{
    static_assert(kMaxPhysicalSlices == WdspEngine::kMaxSliceChannels);
    std::array<int, kMaxPhysicalSlices> next{};
    for (int sliceId = 0; sliceId < kMaxPhysicalSlices; ++sliceId) {
        next[sliceId] = m_desktopHostMode ? desktopReceiverForSlice(sliceId)
                                         : (sliceId < kMaxTciRxSlices ? sliceId : -1);
    }
    QMutexLocker guard(&m_localAudioMapMutex);
    for (int rx = 0; rx < kMaxTciRxSlices; ++rx) {
        int before = -1;
        int after = -1;
        for (int sliceId = 0; sliceId < kMaxPhysicalSlices; ++sliceId) {
            if (m_appliedLocalAudioReceiverForSlice[sliceId] == rx) { before = sliceId; }
            if (next[sliceId] == rx) { after = sliceId; }
        }
        if (before == after) { continue; }
        // The main thread is the ring's only consumer. While the producer
        // holds no lock, discard bytes from the old owner, including a
        // partial block, and skip already collected history for every app.
        while (m_audioRing[rx].popInto(
                   reinterpret_cast<uint8_t*>(m_drainScratch.data()),
                   static_cast<qint64>(kMaxDrainSamples) * sizeof(float)) > 0) {}
        ++m_localAudioGeneration[rx];
        for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
            it.value()->audioReadFrame.insert(rx, m_rxFramesWritten[rx]);
        }
        m_sliceRxPeakAbs[rx].store(0.0f, std::memory_order_release);
    }
    for (int sliceId = 0; sliceId < kMaxPhysicalSlices; ++sliceId) {
        m_appliedLocalAudioReceiverForSlice[sliceId] = next[sliceId];
        m_localAudioReceiverForSlice[sliceId].store(next[sliceId], std::memory_order_release);
    }
}

bool TciServer::releaseAppTciKey(QWebSocket* client)
{
    // Fix round 1 (Important 1): a backstop. A TCI level no app owns is
    // released too, by the server's stop (client null) and by any app
    // that goes; only a level another live app owns is left. In desktop
    // host mode every trx is the program key's, which
    // releaseDesktopProgramKey ends (and a key still being made ends in
    // its own path), so an unowned level is left to them.
    if (m_tciPttClient.isNull() && m_desktopHostMode) {
        return true;
    }
    if (client != nullptr && !m_tciPttClient.isNull() && m_tciPttClient.data() != client) {
        return true;
    }
    m_tciPttClient = nullptr;
    const QPointer<MoxController> mox = m_model ? m_model->moxController() : nullptr;
    if (!mox || !mox->isTciPttHeld()) { return true; }
    qCInfo(lcTci) << "TciServer: releasing the TCI key of an app that is gone";
    const QPointer<TciServer> self(this);
    // As the app's own trx:N,false would (RadioModel::setMox): PollPTT
    // falls back to a still-held source or unkeys.
    mox->onTciPtt(false);
    return !self.isNull();
}

void TciServer::releaseDesktopProgramKey()
{
    if (!m_desktopHostMode || !m_desktopKeyHeld) { return; }
    // Retire this app's ownership before setMox emits callbacks. A callback
    // may synchronously start a newer key that this release must not erase.
    ++m_desktopKeyGeneration;
    m_desktopKeyHeld = false;
    m_desktopKeyClient = nullptr;
    QPointer<MoxController> mox = m_model ? m_model->moxController() : nullptr;
    if (!mox) { return; }
    // Fix wave RD-I3: the key came through onTciPtt, so its release drops
    // the TCI level, as a local app's trx:N,false does; a level left up
    // would key again on the next PollPTT pass.
    if (mox->isTciPttHeld()) {
        m_tciPttClient = nullptr;
        mox->onTciPtt(false);
        return;
    }
    const KeyerIdentity keyer = KeyerIdentity::station(PttMode::Tci);
    if (mox->isMox() && mox->currentKeyer() == keyer) {
        mox->setMox(false, keyer);
    }
}

void TciServer::setDesktopHostMode(bool enabled)
{
    if (m_desktopHostMode == enabled) { return; }
    ++m_desktopKeyGeneration;
    const QPointer<TciServer> self(this);
    if (m_desktopHostMode) { releaseDesktopProgramKey(); }
    if (!self) { return; }
    if (m_desktopHostMode && !enabled && m_model && m_model->wdspEngine()) {
        auto* wdsp = m_model->wdspEngine();
        for (int channel = 1; channel < WdspEngine::kMaxSliceChannels; ++channel) {
            if (RxChannel* rxCh = wdsp->rxChannel(channel)) {
                QObject::disconnect(rxCh, &RxChannel::audioFrameReady,
                                    this, &TciServer::onAudioFrameReady);
                m_audioTapSources.removeAll(rxCh);
            }
        }
    }
    QObject::disconnect(m_desktopOwnershipConnection);
    m_desktopHostMode = enabled && !m_remoteWindow;
    refreshLocalAudioReceiverMap();
    m_protocol->setReceiverSliceMap(m_desktopHostMode
        ? [this](int receiver) { return desktopSliceForReceiver(receiver); }
        : std::function<int(int)>{});
    refreshSliceWriteGate();
    if (m_globalBroadcastsWired) { refreshRx2Enabled(); }
    if (m_desktopHostMode) { hookAudioAndIqTaps(); }
    if (m_desktopHostMode && m_model) {
        for (SliceModel* slice : m_model->slices()) {
            if (slice) { wireSliceForBroadcast(slice, slice->sliceIndex()); }
        }
        if (auto* ownership = m_model->sliceOwnership()) {
            m_desktopOwnershipConnection = connect(ownership, &SliceOwnership::markChanged,
                this, [this](int, const QByteArray&, const QByteArray&) {
                    refreshLocalAudioReceiverMap();
                    if (!m_desktopHostMode) { return; }
                    for (int receiver = 0; receiver < TciProtocol::kExposedReceiverCount;
                         ++receiver) {
                        const int sliceId = desktopSliceForReceiver(receiver);
                        if (SliceModel* slice = m_model->sliceById(sliceId)) {
                            wireSliceForBroadcast(slice, sliceId);
                        }
                    }
                });
        }
    }
}

void TciServer::setStationReceiveOnly(bool receiveOnly)
{
    m_stationReceiveOnly = receiveOnly;
    m_protocol->setStationReceiveOnly(receiveOnly);
}

bool TciServer::start(const QHostAddress& bindAddress, quint16 port)
{
    if (m_desktopHostMode && desktopSliceForReceiver(0) < 0) {
        qCWarning(lcTci) << "TciServer: desktop host has no station-owned receiver";
        return false;
    }
    if (m_server) {
        qCWarning(lcTci) << "TciServer::start called while already listening on port"
                         << m_server->serverPort();
        return false;
    }

    m_server = new QWebSocketServer(
        QStringLiteral("NereusSDR-TCI"),
        QWebSocketServer::NonSecureMode, this);

    // From AetherSDR src/core/TciServer.cpp:168-174 [@0cd4559] — listen + error path.
    // Phase 3J-1 closeout Item 1: bindAddress comes from the operator's
    // Setup choice (default 127.0.0.1).  Any/0.0.0.0 exposes the TCI
    // server to the LAN — there is no authentication in TCI 2.0 — so the
    // Setup UI surfaces a tooltip warning when a non-loopback option is
    // selected.
    if (!m_server->listen(bindAddress, port)) {
        if (m_quietListenAttempts) {
            qCDebug(lcTci) << "TciServer: failed to listen on" << bindAddress.toString()
                           << "port" << port << m_server->errorString();
        } else {
            qCWarning(lcTci) << "TciServer: failed to listen on" << bindAddress.toString()
                             << "port" << port << m_server->errorString();
        }
        const QString errStr = m_server->errorString();
        // Qt ownership (parented to this): it never listened, so a later
        // deletion holds nothing.
        m_server->deleteLater();
        m_server = nullptr;
        emit errorOccurred(errStr);
        return false;
    }

    connect(m_server, &QWebSocketServer::newConnection,
            this, &TciServer::onNewConnection);

    if (m_quietListenAttempts) {
        qCDebug(lcTci) << "TciServer: listening on" << m_server->serverPort();
    } else {
        qCInfo(lcTci) << "TciServer: listening on" << m_server->serverPort();
    }
    emit serverStarted(m_server->serverPort());

    // From Thetis TCIServer.cs:2650-2654 [v2.10.3.13] — 20s server-driven ping
    // with payload "Thetis", per RFC 6455 keepalive semantics.
    // Thetis: "per websock spec ping frames are every 20 seconds. Ideally we
    // should receive something back within 20 seconds, but just use it to cause
    // exception on socket if client has dc'ed without telling us with a
    // disconnect frame."
    // Detects dead clients via Qt's automatic close-on-write-error path.
    m_pingTimer->start(m_pingIntervalMs);

    // Task 10 (R-R3-49): the update gap the operator set (Setup > Network >
    // TCI Server > Rate limit), applied when the server starts as Thetis
    // does (setup.cs:22517 [v2.10.3.15]). Default and range from Thetis
    // udTCIRateLimit (setup.designer.cs:58645-58664 [v2.10.3.15]).
    {
        bool ok = false;
        const int gapMs = AppSettings::instance()
                              .value(QString::fromLatin1(TciUpdateGap::kSettingKey),
                                     TciUpdateGap::kDefaultGapMs)
                              .toInt(&ok);
        setUpdateGapMs(ok ? gapMs : TciUpdateGap::kDefaultGapMs);
    }

    // The TX channel for an app's stereo transmit audio, read when the
    // server starts. From Thetis StartServer, TCIServer.cs:6688
    // [v2.10.3.15]: m_txStereoInputMode = c.SetupForm.TCITXInputChannel;
    setTxStereoInputMode(txStereoInputModeFromText(
        AppSettings::instance().value(QStringLiteral("TciTxChannel"),
                                      QStringLiteral("Both")).toString()));

    // Phase 14: start the outbound drain timer (stops again in stop()).
    m_drainTimer->start();

    // Phase 19: start sensor broadcast timers.
    // From Thetis: RxSensorsTimerCallback / TxSensorsTimerCallback are started
    // by setRxSensorsEnabled / setTxSensorsEnabled per-listener
    // (TCIServer.cs:2566, 2581 [v2.10.3.13]).  NereusSDR uses server-wide
    // timers that check per-client rxSensorsEnabled / txSensorsEnabled flags
    // on each tick — simpler with the Qt architecture.
    m_rxSensorTimer->start();
    m_txSensorTimer->start();

    // Phase 3J-1 review P2.3: reconnect DSP audio tap + IQ tap after a
    // stop() → start() cycle.  stop() severs these connections; hookAudioAndIqTaps
    // re-establishes them idempotently (no-op on the first start() call, since
    // the constructor already called hookAudioAndIqTaps).
    hookAudioAndIqTaps();

    // Review P2 #5 fix (2026-05-22): re-hook slice broadcasts on every start().
    // stop()'s QObject::disconnect(m_model, nullptr, this, nullptr) severs ALL
    // RadioModel -> TciServer connections, including the sliceAdded subscriber
    // wired by hookSliceBroadcasts.  Without re-hooking, slices added between
    // a stop()/start() cycle never get broadcast wiring (operator tunes after
    // server restart go un-broadcast).  hookSliceBroadcasts is idempotent --
    // it skips slices already in m_broadcastWiredSlices and the new
    // sliceAdded connect is a fresh wire each call.  Same reasoning applies
    // to hookGlobalBroadcasts (MOX, TUN, MON, AF volume, sample rate, etc.).
    hookSliceBroadcasts();
    hookGlobalBroadcasts();
    refreshRemoteIqDemand();

    return true;
}

// ── stop() ───────────────────────────────────────────────────────────────────
//
// From AetherSDR src/core/TciServer.cpp:184-207 [@0cd4559] — disconnect-and-
// cleanup loop pattern.  NereusSDR uses QHash iteration instead of QList.

void TciServer::stop()
{
    const QPointer<TciServer> self(this);
    ++m_desktopKeyGeneration;
    releaseDesktopProgramKey();
    if (!self) { return; }
    // Fix wave RD-C1 (JJ ruling 1): an app's key ends with the server,
    // released before its TX audio lock below. Not in Thetis.
    if (m_server && !releaseAppTciKey(nullptr)) { return; }
    if (!self || !m_server) { return; }

    // Phase 26 review finding #4: explicitly sever DSP-thread signal connections
    // BEFORE stopping timers and clearing client state.  The audio tap from
    // RxChannel::audioFrameReady uses Qt::DirectConnection, meaning the slot
    // runs on the DSP thread.  If the DSP thread emits after we clear m_clients
    // (below) but before TciServer's vtable is gone, the slot accesses freed
    // memory.  Disconnecting here closes that window.
    //
    // RadioModel::rawIqData uses Qt::QueuedConnection so its slot is marshalled
    // to the main thread and cannot race with destruction, but we disconnect it
    // here for symmetry and safety.
    //
    // Phase 3J-1 review P2.3: reset guard flags so hookAudioAndIqTaps() re-arms
    // on the next start() call.
    if (m_model) {
        QObject::disconnect(m_model, nullptr, this, nullptr);
        m_iqTapConnected = false;  // P2.3: reset so start() can reconnect
        // Review P2 #5 fix (2026-05-22): the wholesale disconnect above
        // severs hookGlobalBroadcasts' MOX / TUN / MON / volume / sample-rate
        // / connection-state subscribers (all rooted on m_model and its
        // sub-models).  Reset the guard so start() re-arms them.
        m_globalBroadcastsWired = false;
        // SliceOwnership is not m_model, so its RX2 hook needs its own
        // disconnect; hookGlobalBroadcasts reconnects it on start().
        QObject::disconnect(m_rx2OwnershipConnection);
    }
    // 2026-05-17 crash fix: m_audioTapSources is now QSet<QPointer<RxChannel>>
    // (see TciServer.h).  Skip entries whose underlying RxChannel was
    // already destroyed (typical after a disconnect-from-radio that ran
    // m_wdspEngine->shutdown()).  Qt::~QObject already auto-disconnected
    // those signals when the channel died, so the missed iteration is a
    // no-op rather than missed cleanup.
    for (const QPointer<RxChannel>& rxCh : std::as_const(m_audioTapSources)) {
        if (rxCh) {
            QObject::disconnect(rxCh.data(), nullptr, this, nullptr);
        }
    }
    m_audioTapSources.clear();  // P2.3: reset so hookAudioAndIqTaps() re-arms
    QObject::disconnect(m_wdspInitConn);
    m_wdspInitConn = {};

    m_pingTimer->stop();
    m_drainTimer->stop();        // Phase 14: stop drain before disconnecting clients
    m_rxSensorTimer->stop();     // Phase 19: stop sensor broadcast timers
    m_txSensorTimer->stop();

    // Phase 17: release TX audio mutex — no client is active after stop().
    // R-R3-39: as a holder's disconnect does, end its TX cycle: the TCI
    // input ring drains and the transmit lane frees the TCI resampler
    // (TxChannel::clearTciAudio). Without it a server stopped mid-cycle left
    // the resampler to leak.
    // Task 35: a remote window's key on the Core ends with the server.
    if (m_remoteKeyEpoch != 0 && m_remoteTransmit.unkey) {
        m_remoteTransmit.unkey(m_remoteKeyEpoch);
    }
    m_remoteKeyEpoch = 0;
    m_remoteKeyPending = false;
    m_remoteReleaseWhilePending = false;
    m_remoteKeyClient = nullptr;
    ++m_remoteKeyGeneration;
    const bool heldTxAudio = !m_txAudioActiveClient.isNull();
    m_txAudioActiveClient = nullptr;
    if (heldTxAudio) {
        // As on a holder's disconnect: nothing keeps showing a TX audio
        // client (the indicator, and MainWindow's TCI audio gate on the TX
        // channel) once the server has stopped.
        emit txAudioActiveClientChanged(nullptr);
        stopTxChrono();
    }

    // Disconnect all connected clients.  We disconnect the socket's signals
    // from this object first to prevent onClientDisconnected() re-entry during
    // the explicit close() calls.
    //
    // Phase 16 Task 16.3 (sub-commit b): destroy all RESAMPLEF instances for
    // each session before clearing the client table. cleanupResamplers is called
    // here so resamplers are destroyed even if QWebSocket::disconnected never
    // fires (e.g. on forceful server shutdown).
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
        cleanupResamplers(it.value());
        QWebSocket* ws = it.key();
        ws->disconnect(this);
        // A key-release callback may destroy this server from inside the
        // socket's disconnected signal. Keep that socket alive until Qt's
        // close stack unwinds, even when its parent server is destroyed now.
        ws->setParent(nullptr);
        ws->close();
        ws->deleteLater();
    }
    m_clients.clear();
    m_remoteAudioSockets.clear();

    for (int rx = 0; rx < kMaxTciRxSlices; ++rx) {
        if (m_remoteIqRequested[rx]) {
            m_remoteIqRequested[rx] = false;
            if (m_remoteIq.release) { m_remoteIq.release(rx); }
        }
    }

    // R-R3-42: no app listens any more; release the Core's receivers.
    for (int rx = 0; rx < kMaxTciRxSlices; ++rx) {
        updateRemoteReceiverDemand(rx);
        updateRemoteIqDemand(rx);
    }
    if (m_remoteSaturationNotified
        && m_noticeReason == QStringLiteral(
            "Remote TCI audio cannot keep up; older audio is being skipped.")) {
        m_noticeReason.clear();
        emit operatorNoticeCleared();
    }
    m_remoteSaturationNotified = false;
    m_remoteSaturationDrops = 0;
    m_remoteSaturationQuietTicks = 0;

    // Close ingress now, but defer destruction of the listener and its
    // internal TCP children until any active socket-close stack unwinds.
    // A key-release callback can delete this TciServer on that very stack.
    m_server->close();
    m_server->setParent(nullptr);
    m_server->deleteLater();
    m_server = nullptr;
    for (QWebSocketServer* extra : std::as_const(m_extraServers)) {
        // M6: closed now, so the port is free at once; Qt deletes it.
        extra->close();
        extra->setParent(nullptr);
        extra->deleteLater();
    }
    m_extraServers.clear();

    if (m_quietListenAttempts) {
        qCDebug(lcTci) << "TciServer: stopped";
    } else {
        qCInfo(lcTci) << "TciServer: stopped";
    }
    emit serverStopped();
}

// ── isRunning() / port() ─────────────────────────────────────────────────────
//
// From AetherSDR src/core/TciServer.cpp:209-217 [@0cd4559]

bool TciServer::isRunning() const
{
    return m_server && m_server->isListening();
}

quint16 TciServer::port() const
{
    return m_server ? m_server->serverPort() : 0;
}

// ── onNewConnection() ────────────────────────────────────────────────────────
//
// From AetherSDR src/core/TciServer.cpp:247-273 [@0cd4559] — accept-loop
// pattern adapted to the QHash<QWebSocket*, shared_ptr<TciClientSession>> table.

void TciServer::onNewConnection()
{
    // R-R3-48: any of the listening servers (the station network's or this
    // computer's) may have the new connection.
    auto* server = qobject_cast<QWebSocketServer*>(sender());
    if (!server) {
        server = m_server;
    }
    if (!server) {
        return;
    }
    while (server->hasPendingConnections()) {
        auto* ws = server->nextPendingConnection();
        if (m_desktopHostMode && desktopSliceForReceiver(0) < 0) {
            ws->close();
            ws->deleteLater();
            continue;
        }

        // Phase 26 review finding #7: bound incoming binary (and text) frame
        // size against hostile or malformed frames from a misbehaving local
        // client.  Legitimate TCI audio is ≤ 2048 samples × 2 ch × 4 bytes =
        // 16 KiB + 64-byte header.  2 MiB gives 128× headroom while preventing
        // pathological heap allocations from oversized frames.
        //
        // QWebSocket::setMaxAllowedIncomingMessageSize is per-socket;
        // QWebSocketServer has no equivalent in this Qt6 version.
        //
        // NereusSDR-original (Thetis hand-rolls RFC 6455 framing with no cap;
        // we're 127.0.0.1-only but a misbehaving local process can still send).
        static constexpr quint64 kMaxIncomingMessageBytes = 2u * 1024u * 1024u;  // 2 MiB
        ws->setMaxAllowedIncomingMessageSize(kMaxIncomingMessageBytes);

        auto session = std::make_shared<TciClientSession>();
        session->socket = ws;
        session->peer   = ws->peerAddress().toString()
                        + QStringLiteral(":")
                        + QString::number(ws->peerPort());
        // TODO Phase 4: extract userAgent from QWebSocket request header
        // (ws->request() is available after the WebSocket handshake; the
        // User-Agent HTTP header maps to session->userAgent).
        session->connectedAt.start();
        // Task 10 (R-R3-49): the app's update gap, from Thetis
        // TCPIPtciSocketListener(..., rateLimit) at TCIServer.cs:792-795
        // [v2.10.3.15].
        session->updateGap.setGapMs(m_updateGapMs);
        // The sensor interval an app gets when its rx_sensors_enable or
        // tx_sensors_enable names none. Thetis keeps 200 ms
        // (clsTCISensorManager _rxIntervalMs / _txIntervalMs,
        // TCIServer.cs:486-487 [v2.10.3.15]) and handleRxSensorsEnable /
        // handleTxSensorsEnable fall back to it (TCIServer.cs:4636, 4647);
        // NereusSDR's Setup > TCI Server > Sensors sets it, 200 by default,
        // held to Thetis clampIntervalMs's 30 to 1000 (TCIServer.cs:500-505).
        {
            auto& settings = AppSettings::instance();
            const auto interval = [&settings](const char* key) {
                bool ok = false;
                const int ms = settings.value(QString::fromLatin1(key), 200).toInt(&ok);
                return std::clamp(ok ? ms : 200, 30, 1000);
            };
            session->rxSensorIntervalMs = interval("TciRxSensorIntervalMs");
            session->txSensorIntervalMs = interval("TciTxSensorIntervalMs");
        }

        // Phase 26 review finding #3: apply AudioTciPage AppSettings defaults
        // at connect time so that a client that never sends explicit audio
        // config commands inherits the operator's configured preferences.
        //
        // Each key can still be overridden by the client sending an explicit
        // audio_samplerate:N; / audio_stream_* command (Finding #1 interceptor
        // runs after this and wins).
        //
        // TciSliceA_OutputSampleRate / TciSliceB_OutputSampleRate: per-slice
        // defaults.  Phase 3J-1 serves only rx=0 (Slice A), so apply Slice A
        // rate regardless of which slice the client ultimately requests.
        // Slice B rate applied when the session subscribes to rx=1 (Phase 3F+).
        //
        // TciAudioStreamSampleType: stored as "Int16"/"Int24"/"Int32"/"Float32"
        // (capitalised, per AudioTciPage combo items); convert to the int enum
        // matching TciBinaryFrame header format (0=int16, 1=int24, 2=int32, 3=float32).
        //
        // TciAudioStreamSamples: shared key with CatTciServerPage; range [100..2048].
        //
        // TciTxStreamBufferingMs: TX-side buffering; stored in the session for
        // future TxChannel feed latency tuning.  No TciClientSession field for
        // TX buffering yet — documented here for Phase 3J-2 wiring.
        {
            auto& s = AppSettings::instance();

            // Slice A output sample rate.
            // Phase 3J-1 review P1.2: clamp persisted rate to [8000, 384000]
            // (same bounds as the runtime audio_samplerate: interceptor) so a
            // manually-edited settings file cannot inject an out-of-range value
            // that overflows the drain-path resampler output buffer.
            constexpr int kMinAudioSampleRateSettings = 8000;
            constexpr int kMaxAudioSampleRateSettings = 384000;
            const int sliceARateSaved = s.value(
                QStringLiteral("TciSliceA_OutputSampleRate"),
                QStringLiteral("48000")).toString().toInt();
            if (sliceARateSaved >= kMinAudioSampleRateSettings
                    && sliceARateSaved <= kMaxAudioSampleRateSettings) {
                session->audioSampleRate = sliceARateSaved;
            }

            // Audio stream sample type.
            const QString typeSaved = s.value(
                QStringLiteral("TciAudioStreamSampleType"),
                QStringLiteral("Float32")).toString().toLower();
            if (typeSaved == QStringLiteral("int16"))       { session->audioSampleType = 0; }
            else if (typeSaved == QStringLiteral("int24"))  { session->audioSampleType = 1; }
            else if (typeSaved == QStringLiteral("int32"))  { session->audioSampleType = 2; }
            else                                             { session->audioSampleType = 3; }  // float32 default

            // Audio stream block size.
            const int samplesSaved = s.value(
                QStringLiteral("TciAudioStreamSamples"), 2048).toInt();
            if (samplesSaved >= 100 && samplesSaved <= 2048) {
                session->audioStreamSamples = samplesSaved;
            }

            // Audio stream channel count. Thetis starts each app at 2
            // (m_audioStreamChannels = 2, TCIServer.cs:781 [v2.10.3.15]) and
            // takes only 1 or 2 from it afterwards:
            //     if (channels == 1 || channels == 2)
            //         m_audioStreamChannels = channels;
            // (handleAudioStreamChannels, TCIServer.cs:6340-6354
            // [v2.10.3.15]). Setup > Audio > TCI's Channels is that starting
            // count; anything else keeps Thetis's 2. The app's own
            // audio_stream_channels still wins.
            const int channelsSaved = s.value(
                QStringLiteral("TciAudioStreamChannels"), 2).toInt();
            if (channelsSaved == 1 || channelsSaved == 2) {
                session->audioStreamChannels = channelsSaved;
            }

            // TciTxStreamBufferingMs — no TciClientSession field yet; log only.
            // TODO Phase 3J-2: add txStreamBufferingMs to TciClientSession and
            // wire into the TX audio drain path so the operator-configured
            // pre-buffer is honored.
            (void)s.value(QStringLiteral("TciTxStreamBufferingMs"), 50).toInt();
        }

        m_clients.insert(ws, session);

        connect(ws, &QWebSocket::textMessageReceived,
                this, &TciServer::onTextMessageReceived);
        connect(ws, &QWebSocket::binaryMessageReceived,
                this, &TciServer::onBinaryMessageReceived);
        connect(ws, &QWebSocket::disconnected,
                this, &TciServer::onClientDisconnected);

        qCInfo(lcTci) << "TciServer: client connected from" << session->peer;
        emit clientConnected(ws);

        // From Thetis TCIServer.cs:2713 [v2.10.3.13] — sendInitialisationData()
        // is called immediately after upgradeToWebSocket() completes. Without
        // this, real TCI clients (N1MM Logger+, Log4OM, RUMlog-TCI, ESDR3) see
        // a silent connection and either stay in a "waiting" state or close.
        // Bench-discovered 2026-05-10 against websocat — Phase 2 Task 2.1
        // wired the session lifecycle but never invoked buildInitBurst()
        // (Phase 4 Task 4.1+4.2 built the burst but no commit wired it to
        // the connect path).
        if (m_protocol) {
            const QStringList burst = m_protocol->buildInitBurst();
            for (QString line : burst) {
                if (line.startsWith(QLatin1String("iq_samplerate:"))) {
                    line = QStringLiteral("iq_samplerate:%1;").arg(publishedIqRate());
                } else if (line.startsWith(QLatin1String("audio_stream_channels:"))) {
                    // Thetis announces the app's own count
                    // (sendAudioStreamChannels(m_audioStreamChannels),
                    // TCIServer.cs:2645 [v2.10.3.15]).
                    line = QStringLiteral("audio_stream_channels:%1;")
                               .arg(session->audioStreamChannels);
                }
                session->sendQueue.push(TciSendQueue::Priority::Control, line);
            }
        }
    }
}

// ── onClientDisconnected() ───────────────────────────────────────────────────
//
// From AetherSDR src/core/TciServer.cpp:275+ [@0cd4559] — sender()-based
// lookup pattern, adapted from QList linear search to QHash O(1) lookup.

void TciServer::onClientDisconnected()
{
    auto* ws = qobject_cast<QWebSocket*>(sender());
    if (!ws) { return; }

    auto it = m_clients.find(ws);
    if (it == m_clients.end()) { return; }

    qCInfo(lcTci) << "TciServer: client disconnected from" << it.value()->peer;
    if (m_desktopHostMode) { ++m_desktopKeyGeneration; }
    const QPointer<TciServer> self(this);
    const QPointer<QWebSocket> client(ws);
    emit clientDisconnected(ws);
    if (!self || !client) { return; }
    it = m_clients.find(ws);
    if (it == m_clients.end()) { return; }

    if (m_desktopHostMode && m_desktopKeyClient.data() == ws) {
        releaseDesktopProgramKey();
        if (!self || !client) { return; }
        it = m_clients.find(ws);
        if (it == m_clients.end()) { return; }
    }

    // Fix wave RD-C1 (JJ ruling 1): the app that keyed through its trx is
    // gone, so nothing would ever send its trx:N,false. Its key is released
    // first; releasing only the TX audio lock below would leave the
    // transmitter keyed with the microphone as its audio.
    //
    // Deliberate departure from Thetis TCIServer.cs:3010-3026 [v2.10.3.15]
    // (StopSocketListener), which clears m_tciPttActive and releases the
    // audio listener but leaves console.TCIPTT, and so the key, as it was.
    // Fix round 1 (Important 1): also a TCI level no app owns.
    if (m_tciPttClient.isNull() || m_tciPttClient.data() == ws) {
        if (!releaseAppTciKey(ws) || !client) { return; }
        it = m_clients.find(ws);
        if (it == m_clients.end()) { return; }
    }

    // Fix round 2 (Critical 1, RD-C1): in a remote window, the app whose
    // trx:N,true was forwarded is gone, with or without TCI audio. Nothing
    // would send its trx:N,false, so the window's key is released on the
    // Core now, or the Core's answer is when it comes.
    if (m_remoteWindow && !m_remoteKeyClient.isNull() && m_remoteKeyClient.data() == ws) {
        m_remoteKeyClient = nullptr;
        if (m_remoteKeyPending) {
            m_remoteReleaseWhilePending = true;
        }
        if (m_remoteKeyEpoch != 0 && m_remoteTransmit.unkey) {
            qCInfo(lcTci) << "TciServer: the app that keyed left; unkeys this window's key, epoch"
                          << m_remoteKeyEpoch;
            m_remoteTransmit.unkey(m_remoteKeyEpoch);
            m_remoteKeyEpoch = 0;
            endRemoteKey();
            if (!self || !client) { return; }
            it = m_clients.find(ws);
            if (it == m_clients.end()) { return; }
        }
    }

    // Phase 17: release TX audio mutex if this client held it.
    // QPointer auto-nulls when the socket is deleted (ws->deleteLater below),
    // but we clear explicitly here so activeTxClientCount() returns 0 in the
    // same event-loop pass as the disconnect.
    if (!m_txAudioActiveClient.isNull() && m_txAudioActiveClient.data() == ws) {
        // Task 35: the app whose audio a remote window's key carried is
        // gone; nothing would ever send its trx:N,false, so the key is
        // released on the Core.
        if (m_remoteWindow && m_remoteKeyEpoch != 0 && m_remoteTransmit.unkey) {
            m_remoteTransmit.unkey(m_remoteKeyEpoch);
            m_remoteKeyEpoch = 0;
            broadcastRemoteKeyState(false);
        }
        m_txAudioActiveClient = nullptr;
        qCInfo(lcTci) << "TciServer: TX audio mutex released on disconnect";
        // Phase 23: notify indicator / MainWindow.
        emit txAudioActiveClientChanged(nullptr);
        // Phase 3J-1 bench fix: stop TX_CHRONO frames on client disconnect.
        stopTxChrono();
    }

    // Phase 16 Task 16.3 (sub-commit b): destroy all RESAMPLEF instances for
    // this client before removing the session from the map.
    cleanupResamplers(it.value());

    m_clients.erase(it);
    ws->deleteLater();

    // R-R3-42: the last app listening to a Core receiver releases it.
    for (int rx = 0; rx < kMaxTciRxSlices; ++rx) {
        updateRemoteReceiverDemand(rx);
        updateRemoteIqDemand(rx);
    }
}

// ── totalResamplerInstances() ─────────────────────────────────────────────────
//
// Phase 16 Task 16.3 (sub-commit b): sums audioResamplers.size() across all
// connected sessions. Exposed for lifecycle test assertions and future
// diagnostic tooling (Phase 22 ClientChainApplet).
int TciServer::totalResamplerInstances() const
{
    int total = 0;
    for (auto it = m_clients.cbegin(); it != m_clients.cend(); ++it) {
        total += it.value()->audioResamplers.size();
    }
    return total;
}

// ── activeTxClientCount() ────────────────────────────────────────────────────
//
// Phase 17: returns 1 when m_txAudioActiveClient is set and still connected;
// 0 otherwise (QPointer auto-nulls on socket destruction).
// Used by Phase 22 ClientChainApplet to render the TX badge.
int TciServer::activeTxClientCount() const
{
    return m_txAudioActiveClient.isNull() ? 0 : 1;
}

// ── activeTxClientPeer() ────────────────────────────────────────────────────
//
// Phase 17: returns the peer string of the active TX client,
// or an empty string when there is no active TX client.
QString TciServer::activeTxClientPeer() const
{
    if (m_txAudioActiveClient.isNull()) {
        return {};
    }
    auto it = m_clients.find(m_txAudioActiveClient.data());
    if (it == m_clients.cend()) {
        return {};
    }
    return it.value()->peer;
}

// ── activeTxAudioClient() ────────────────────────────────────────────────────
//
// Phase 22: returns the raw QWebSocket* of the active TX audio client,
// or nullptr when no client holds the TX mutex.
// Not inlined in the header because moc compilation units may include
// TciClientSession.h which forward-declares QWebSocket, preventing
// QPointer<QWebSocket>::data() from instantiating.
QWebSocket* TciServer::activeTxAudioClient() const
{
    return m_txAudioActiveClient.data();
}

// ── TciRxAudioResampler ──────────────────────────────────────────────────────
//
// R-R3-42 fix wave: one resampler per channel, created and destroyed
// together, so stereo keeps its left and right apart at every client rate.
// From Thetis TCIServer.cs:1022-1023 [v2.10.3.15], resampleRxAudioSamples:
//   state.LeftResampler = (IntPtr)WDSP.create_resampleFV(inputRate, targetRate);
//   state.RightResampler = (IntPtr)WDSP.create_resampleFV(inputRate, targetRate);
// and TCIServer.cs:945-962 [v2.10.3.15]: destroyRxAudioResamplerState
// destroys both.
// create_resampleFV(in_rate, out_rate) — from resample.c:342-344 [WDSP v1.29]:
//   return (void *)create_resampleF(1, 0, 0, 0, in_rate, out_rate);
// size=0 + null buffers are intentional; xresampleFV sets them per-call.
//
// Local TCI clients keep the model receive-lane ownership from R-R3-39.
// Remote clients use RemoteTciAudioRun instead: each pair and its scratch
// are created, resampled, and destroyed by the RemoteAudioReceiver worker.
namespace {
std::atomic<int> s_liveRxAudioResamplers{0};
} // namespace

struct TciRxAudioResampler {
    // Output at most 8x the input (384000 / 48000).
    static constexpr int kMaxOutFrames = 2048 * 8;

    int inRate{48000};
    int outRate{48000};
    void* left{nullptr};
    void* right{nullptr};
    std::vector<float> inLeft;
    std::vector<float> inRight;
    std::vector<float> outLeft;
    std::vector<float> outRight;
    std::vector<float> out;

    TciRxAudioResampler(int in, int outHz) : inRate(in), outRate(outHz) {}
    ~TciRxAudioResampler() { destroy(); }
    TciRxAudioResampler(const TciRxAudioResampler&) = delete;
    TciRxAudioResampler& operator=(const TciRxAudioResampler&) = delete;

    void create()
    {
        if (left || right) {
            return;
        }
        left = create_resampleFV(inRate, outRate);
        right = create_resampleFV(inRate, outRate);
        s_liveRxAudioResamplers.fetch_add((left ? 1 : 0) + (right ? 1 : 0),
                                          std::memory_order_relaxed);
        if (!left || !right) {
            qCWarning(lcTci) << "TciServer: create_resampleFV failed"
                             << "in_rate" << inRate << "out_rate" << outRate;
            destroy();
            return;
        }
        inLeft.assign(static_cast<std::size_t>(kMaxOutFrames / 8), 0.0f);
        inRight.assign(static_cast<std::size_t>(kMaxOutFrames / 8), 0.0f);
        outLeft.assign(static_cast<std::size_t>(kMaxOutFrames), 0.0f);
        outRight.assign(static_cast<std::size_t>(kMaxOutFrames), 0.0f);
        out.assign(static_cast<std::size_t>(kMaxOutFrames) * 2, 0.0f);
    }

    void destroy()
    {
        // destroy_resampleFV, from resample.c:358-360 [WDSP v1.29]:
        //   destroy_resampleF((RESAMPLEF)ptr);
        if (left) {
            destroy_resampleFV(left);
            left = nullptr;
            s_liveRxAudioResamplers.fetch_sub(1, std::memory_order_relaxed);
        }
        if (right) {
            destroy_resampleFV(right);
            right = nullptr;
            s_liveRxAudioResamplers.fetch_sub(1, std::memory_order_relaxed);
        }
    }

    // Resamples one block of `frames` frames of `channels` interleaved
    // samples. Returns the output and sets `outSamples` (the flat count);
    // with no resamplers the input comes back unchanged.
    const float* resample(const float* input, int frames, int channels, int& outSamples)
    {
        outSamples = frames * channels;
        if (!left || !right || frames > kMaxOutFrames / 8) {
            return input;
        }
        // R-R3-42 fix wave: each channel through its own resampler, then
        // interleaved again. One resampler used to run over the
        // interleaved L/R, mixing the channels at the wrong rate.
        // From Thetis TCIServer.cs:1060-1064 [v2.10.3.15]:
        //   WDSP.xresampleFV(pLeftInput, pLeftOutput, samples, &leftOutputSamples, ...);
        //   WDSP.xresampleFV(pRightInput, pRightOutput, samples, &rightOutputSamples, ...);
        //   int outputSamples = Math.Min(leftOutputSamples, rightOutputSamples);
        // A mono block is the left channel alone (the right resampler idles).
        if (channels <= 1) {
            int leftOut = 0;
            xresampleFV(const_cast<float*>(input), outLeft.data(), frames, &leftOut, left);
            outSamples = std::clamp(leftOut, 0, kMaxOutFrames);
            return outLeft.data();
        }
        for (int i = 0; i < frames; ++i) {
            inLeft[static_cast<std::size_t>(i)] = input[2 * i];
            inRight[static_cast<std::size_t>(i)] = input[2 * i + 1];
        }
        int leftOut = 0;
        int rightOut = 0;
        xresampleFV(inLeft.data(), outLeft.data(), frames, &leftOut, left);
        xresampleFV(inRight.data(), outRight.data(), frames, &rightOut, right);
        const int outFrames = std::clamp(std::min(leftOut, rightOut), 0, kMaxOutFrames);
        for (int i = 0; i < outFrames; ++i) {
            out[static_cast<std::size_t>(2 * i)] = outLeft[static_cast<std::size_t>(i)];
            out[static_cast<std::size_t>(2 * i + 1)] = outRight[static_cast<std::size_t>(i)];
        }
        outSamples = outFrames * 2;
        return out.data();
    }
};

RemoteTciAudioStage::RemoteTciAudioStage(int receiver) : m_receiver(receiver)
{
    auto config = std::make_shared<ConfigSnapshot>();
    config->receiver = receiver;
    config->receiverGeneration = m_generation.load();
    m_config = std::move(config);
}

RemoteTciAudioStage::~RemoteTciAudioStage() = default;

quint64 RemoteTciAudioStage::generation() const { return m_generation.load(); }

void RemoteTciAudioStage::invalidate()
{
    const quint64 next = m_generation.fetch_add(1) + 1;
    auto config = std::make_shared<ConfigSnapshot>();
    config->receiver = m_receiver;
    config->receiverGeneration = next;
    std::lock_guard<std::mutex> lock(m_mutex);
    config->clients = m_config->clients;
    m_config = std::move(config);
    m_results.clear();
}

void RemoteTciAudioStage::publish(std::vector<ClientConfig> clients)
{
    if (clients.size() > kMaxClients) { clients.resize(kMaxClients); }
    auto config = std::make_shared<ConfigSnapshot>();
    config->receiver = m_receiver;
    config->clients = std::move(clients);
    std::lock_guard<std::mutex> lock(m_mutex);
    config->receiverGeneration = m_generation.load();
    m_config = config;
    for (auto it = m_results.begin(); it != m_results.end();) {
        const auto found = std::find_if(config->clients.begin(), config->clients.end(),
            [token = it->first](const ClientConfig& client) { return client.token == token; });
        if (found == config->clients.end()) {
            it = m_results.erase(it);
            continue;
        }
        auto& queue = it->second;
        std::erase_if(queue, [&](const Result& result) {
            return result.receiverGeneration != config->receiverGeneration
                || result.revision != found->revision;
        });
        ++it;
    }
}

void RemoteTciAudioStage::push(Result result)
{
    if (result.bytes.size() > kMaxPayloadBytes) { return; }
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto found = std::find_if(m_config->clients.begin(), m_config->clients.end(),
        [token = result.token](const ClientConfig& client) { return client.token == token; });
    if (found == m_config->clients.end() || result.revision != found->revision
        || result.receiverGeneration != m_config->receiverGeneration) { return; }
    auto& queue = m_results[result.token];
    if (queue.size() == kMaxFramesPerClient) {
        queue.pop_front();
        ++m_mailboxEvictions;
    }
    queue.push_back(std::move(result));
}

bool RemoteTciAudioStage::popNext(Result* out)
{
    if (!out) { return false; }
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_results.empty()) { return false; }
    auto it = m_results.upper_bound(m_nextDrainToken);
    for (std::size_t tried = 0; tried < m_results.size(); ++tried) {
        if (it == m_results.end()) { it = m_results.begin(); }
        if (!it->second.empty()) {
            m_nextDrainToken = it->first;
            *out = std::move(it->second.front());
            it->second.pop_front();
            return true;
        }
        ++it;
    }
    return false;
}

void RemoteTciAudioStage::noteSocketBackpressureDrop()
{
    ++m_socketBackpressureDrops;
}

RemoteTciAudioStage::Diagnostics RemoteTciAudioStage::diagnostics() const
{
    return {m_historySkippedFrames.load(), m_mailboxEvictions.load(),
            m_socketBackpressureDrops.load(),
            m_serviceWallNs.load(), m_serviceCpuNs.load(), m_maxServiceWallNs.load(),
            m_maxQuantumWallNs.load(), m_serviceQuanta.load(),
            m_resamplerRecreates.load(), m_resamplerRecreateWallNs.load(),
            m_resamplerRecreateCpuNs.load(), m_maxResamplerRecreateWallNs.load(),
            m_liveWdspPairs.load()};
}

namespace {
quint64 threadCpuNs()
{
#ifdef Q_OS_WIN
    FILETIME created{}, exited{}, kernel{}, user{};
    if (!GetThreadTimes(GetCurrentThread(), &created, &exited, &kernel, &user)) { return 0; }
    ULARGE_INTEGER k{}, u{};
    k.LowPart = kernel.dwLowDateTime; k.HighPart = kernel.dwHighDateTime;
    u.LowPart = user.dwLowDateTime; u.HighPart = user.dwHighDateTime;
    return (k.QuadPart + u.QuadPart) * 100;
#else
    timespec time{};
    return clock_gettime(CLOCK_THREAD_CPUTIME_ID, &time) == 0
        ? quint64(time.tv_sec) * 1'000'000'000 + quint64(time.tv_nsec) : 0;
#endif
}

void storeMax(std::atomic<quint64>& destination, quint64 value)
{
    quint64 previous = destination.load();
    while (previous < value && !destination.compare_exchange_weak(previous, value)) {}
}
} // namespace

class RemoteTciAudioRun final : public IRemotePcmWorkerStage::Run {
public:
    explicit RemoteTciAudioRun(RemoteTciAudioStage& owner) : m_owner(owner) {}
    ~RemoteTciAudioRun() override
    {
        int pairs = 0;
        for (const auto& [token, client] : m_clients) {
            Q_UNUSED(token);
            if (client.resampler) { ++pairs; }
        }
        m_clients.clear();
        m_owner.m_liveWdspPairs.fetch_sub(pairs);
    }

    void appendPcm(const float* stereo, int frames) override
    {
        if (!stereo || frames <= 0) { return; }
        for (int i = 0; i < frames; ++i) {
            const std::size_t slot = std::size_t((m_written + quint64(i))
                                                 % RemoteTciAudioStage::kHistoryFrames) * 2;
            m_history[slot] = stereo[2 * i];
            m_history[slot + 1] = stereo[2 * i + 1];
        }
        m_written += quint64(frames);
    }

    void reconcile() override
    {
        std::shared_ptr<const RemoteTciAudioStage::ConfigSnapshot> config;
        {
            std::lock_guard<std::mutex> lock(m_owner.m_mutex);
            config = m_owner.m_config;
        }
        if (config == m_config) {
            m_reconcileBase = m_written;
            return;
        }
        // Destroy every retired or revised pair before creating replacements.
        std::set<quint64> revised;
        for (auto it = m_clients.begin(); it != m_clients.end();) {
            const auto found = std::find_if(config->clients.begin(), config->clients.end(),
                [token = it->first](const RemoteTciAudioStage::ClientConfig& client) {
                    return client.token == token;
                });
            if (found == config->clients.end() || found->revision != it->second.config.revision
                || (m_config && config->receiverGeneration != m_config->receiverGeneration)) {
                if (found != config->clients.end()) { revised.insert(it->first); }
                const bool hadPair = bool(it->second.resampler);
                it = m_clients.erase(it);
                if (hadPair) { --m_owner.m_liveWdspPairs; }
            } else {
                ++it;
            }
        }
        for (const auto& desired : config->clients) {
            if (m_clients.contains(desired.token)) { continue; }
            Client client;
            client.config = desired;
            // This wake's PCM was appended before reconciliation. Start a
            // newly published client at the previous wake boundary so it
            // receives the first packet after admission or a config change.
            client.cursor = revised.contains(desired.token) ? m_written
                : std::max(m_reconcileBase,
                    m_written > RemoteTciAudioStage::kHistoryFrames
                        ? m_written - RemoteTciAudioStage::kHistoryFrames : quint64{0});
            client.output.reserve(std::size_t(desired.blockFrames * desired.channels * 8));
            if (desired.rate != 48000) {
                client.resampler = std::make_unique<TciRxAudioResampler>(48000, desired.rate);
                client.resampler->create();
                if (!client.resampler->left || !client.resampler->right) { continue; }
                ++m_owner.m_liveWdspPairs;
            }
            m_clients.emplace(desired.token, std::move(client));
        }
        m_config = std::move(config);
        m_reconcileBase = m_written;
    }

    bool hasRunnableWork() const override
    {
        for (const auto& [token, client] : m_clients) {
            Q_UNUSED(token);
            if (!client.failed && client.cursor < m_written) { return true; }
        }
        return false;
    }

    void serviceUntil(std::chrono::steady_clock::time_point deadline, int maxQuanta) override
    {
        const auto started = std::chrono::steady_clock::now();
        const quint64 cpuStarted = threadCpuNs();
        for (int q = 0; q < maxQuanta && !m_clients.empty(); ++q) {
            auto it = m_clients.upper_bound(m_nextToken);
            if (it == m_clients.end()) { it = m_clients.begin(); }
            bool found = false;
            for (std::size_t tried = 0; tried < m_clients.size(); ++tried) {
                if (it == m_clients.end()) { it = m_clients.begin(); }
                if (!it->second.failed && it->second.cursor < m_written) {
                    found = true;
                    break;
                }
                ++it;
            }
            if (!found) { break; }
            m_nextToken = it->first;
            const auto quantumStart = std::chrono::steady_clock::now();
            service(it->second);
            const auto quantumEnd = std::chrono::steady_clock::now();
            storeMax(m_owner.m_maxQuantumWallNs, quint64(std::chrono::duration_cast<
                std::chrono::nanoseconds>(quantumEnd - quantumStart).count()));
            ++m_owner.m_serviceQuanta;
            if (quantumEnd >= deadline) { break; }
        }
        const quint64 elapsed = quint64(std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - started).count());
        m_owner.m_serviceWallNs.fetch_add(elapsed);
        const quint64 cpuEnded = threadCpuNs();
        if (cpuStarted != 0 && cpuEnded >= cpuStarted) {
            m_owner.m_serviceCpuNs.fetch_add(cpuEnded - cpuStarted);
        }
        storeMax(m_owner.m_maxServiceWallNs, elapsed);
    }

private:
    struct Client {
        RemoteTciAudioStage::ClientConfig config;
        quint64 cursor = 0;
        quint64 sequence = 0;
        int blockInput = 0;
        bool failed = false;
        std::unique_ptr<TciRxAudioResampler> resampler;
        std::vector<float> output;
    };

    void service(Client& client)
    {
        if (m_written - client.cursor > RemoteTciAudioStage::kHistoryFrames) {
            const quint64 skipped = m_written - client.cursor
                - RemoteTciAudioStage::kHistoryFrames;
            m_owner.m_historySkippedFrames.fetch_add(skipped);
            client.cursor = m_written - RemoteTciAudioStage::kHistoryFrames;
            client.output.clear();
            client.blockInput = 0;
            if (client.resampler) {
                const auto resetStarted = std::chrono::steady_clock::now();
                const quint64 cpuStarted = threadCpuNs();
                client.resampler->destroy();
                client.resampler->create();
                const quint64 resetWallNs = quint64(std::chrono::duration_cast<
                    std::chrono::nanoseconds>(std::chrono::steady_clock::now()
                                             - resetStarted).count());
                ++m_owner.m_resamplerRecreates;
                m_owner.m_resamplerRecreateWallNs.fetch_add(resetWallNs);
                storeMax(m_owner.m_maxResamplerRecreateWallNs, resetWallNs);
                const quint64 cpuEnded = threadCpuNs();
                if (cpuStarted != 0 && cpuEnded >= cpuStarted) {
                    m_owner.m_resamplerRecreateCpuNs.fetch_add(cpuEnded - cpuStarted);
                }
                if (!client.resampler->left || !client.resampler->right) {
                    client.resampler.reset();
                    client.failed = true;
                    --m_owner.m_liveWdspPairs;
                    return;
                }
            }
        }
        const int frames = int(std::min<quint64>({96,
            quint64(client.config.blockFrames - client.blockInput),
            m_written - client.cursor}));
        if (frames <= 0) { return; }
        const int channels = client.config.channels;
        for (int i = 0; i < frames; ++i) {
            const std::size_t slot = std::size_t((client.cursor + quint64(i))
                                                 % RemoteTciAudioStage::kHistoryFrames) * 2;
            m_input[std::size_t(i * channels)] = m_history[slot] * client.config.gain;
            if (channels == 2) {
                m_input[std::size_t(2 * i + 1)] = m_history[slot + 1] * client.config.gain;
            }
        }
        int outSamples = frames * channels;
        const float* samples = m_input.data();
        if (client.resampler) {
            samples = client.resampler->resample(m_input.data(), frames, channels, outSamples);
        }
        client.output.insert(client.output.end(), samples, samples + outSamples);
        client.cursor += quint64(frames);
        client.blockInput += frames;
        if (client.blockInput == client.config.blockFrames) {
            RemoteTciAudioStage::Result result;
            result.receiverGeneration = m_config->receiverGeneration;
            result.token = client.config.token;
            result.revision = client.config.revision;
            result.sequence = ++client.sequence;
            result.bytes = TciBinaryFrame::buildStreamPayload(
                m_config->receiver, client.config.rate, client.config.type,
                int(client.output.size()), int(TciStreamType::RxAudioStream),
                channels, client.output.data());
            m_owner.push(std::move(result));
            client.output.clear();
            client.blockInput = 0;
        }
    }

    RemoteTciAudioStage& m_owner;
    std::shared_ptr<const RemoteTciAudioStage::ConfigSnapshot> m_config;
    std::map<quint64, Client> m_clients;
    std::array<float, RemoteTciAudioStage::kHistoryFrames * 2> m_history{};
    std::array<float, 96 * 2> m_input{};
    quint64 m_written = 0;
    quint64 m_reconcileBase = 0;
    quint64 m_nextToken = 0;
};

std::unique_ptr<IRemotePcmWorkerStage::Run> RemoteTciAudioStage::createRun()
{
    return std::make_unique<RemoteTciAudioRun>(*this);
}

int TciServer::liveRxAudioResamplersForTest()
{
    return s_liveRxAudioResamplers.load(std::memory_order_relaxed);
}

DspControlThread* TciServer::rxAudioLane() const
{
    return m_model ? m_model->receiveLane() : nullptr;
}

std::shared_ptr<TciRxAudioResampler> TciServer::makeRxAudioResampler(int inRate, int outRate)
{
    auto resampler = std::make_shared<TciRxAudioResampler>(inRate, outRate);
    if (DspControlThread* lane = rxAudioLane()) {
        lane->post([resampler]() { resampler->create(); });
    } else {
        resampler->create();
    }
    return resampler;
}

void TciServer::releaseRxAudioResampler(std::shared_ptr<TciRxAudioResampler> resampler)
{
    if (!resampler) {
        return;
    }
    // After every block of it already posted: the lane runs jobs in order.
    if (DspControlThread* lane = rxAudioLane()) {
        lane->post([resampler = std::move(resampler)]() { resampler->destroy(); });
    } else {
        resampler->destroy();
    }
}

// ── handleAudioSubscribe() ────────────────────────────────────────────────────
//
// Phase 16 Task 16.3 (sub-commit b): creates a RESAMPLEF instance for the
// given (session, rx) pair if one doesn't already exist.  Idempotent.
//
// From Thetis TCIServer.cs — audio_start handler stores the rx in
// m_audioStreamEnabled (a HashSet<int>) and instantiates a Resampler from
// its m_rxAudioResamplers Dictionary [v2.10.3.13]. NereusSDR maps this to
// create_resampleFV (the void*-opaque exported wrapper in resample.c:342-344
// [WDSP TAPR v1.29]) which calls create_resampleF(run=1, size=0, in=0, out=0,
// in_rate, out_rate).  The size=0/null buffers are fine because xresampleFV
// sets in/out/size per-call — verified by reading resample.c:342-360.
void TciServer::handleAudioSubscribe(std::shared_ptr<TciClientSession>& session, int rx)
{
    if (session->audioStreamEnabled.contains(rx)) {
        return;  // idempotent — Thetis HashSet.Add returns false on duplicate
    }
    session->audioStreamEnabled.insert(rx);
    if (m_remoteWindow) {
        if (session->remoteAudioToken == 0) {
            session->remoteAudioToken = ++m_nextRemoteAudioToken;
            m_remoteAudioSockets.insert(session->remoteAudioToken, session->socket);
        }
        ++session->remoteAudioRevision[rx];
        session->remoteAudioLastSequence.remove(rx);
        session->remoteAudioLastGeneration.remove(rx);
        publishRemoteAudioConfig(rx);
        return;
    }
    // R-R3-42: this client hears the receiver from now on, at its own pace.
    session->audioReadFrame.insert(rx, rx >= 0 && rx < kMaxTciRxSlices
                                           ? m_rxFramesWritten[rx] : 0);

    if (!session->audioResamplers.contains(rx)) {
        const int inRate  = 48000;                        // WDSP RX output is always 48 kHz
        const int outRate = session->audioSampleRate;     // negotiated client rate (default 48000)
        // R-R3-39: made on the receive lane; a failure is logged there.
        session->audioResamplers.insert(rx, makeRxAudioResampler(inRate, outRate));
        qCInfo(lcTci) << "TciServer: audio resamplers created for rx" << rx
                      << "peer" << session->peer
                      << "in_rate" << inRate << "out_rate" << outRate;
    }
}

// ── handleAudioUnsubscribe() ──────────────────────────────────────────────────
//
// Phase 16 Task 16.3 (sub-commit b): destroys the RESAMPLEF for the given
// (session, rx) pair and removes it from the subscription set.  Idempotent.
//
// From Thetis TCIServer.cs — audio_stop handler removes the rx from
// m_audioStreamEnabled and disposes the corresponding Resampler [v2.10.3.13].
void TciServer::handleAudioUnsubscribe(std::shared_ptr<TciClientSession>& session, int rx)
{
    if (!session->audioStreamEnabled.contains(rx)) {
        return;  // idempotent
    }
    session->audioStreamEnabled.remove(rx);
    session->audioReadFrame.remove(rx);
    if (m_remoteWindow) {
        ++session->remoteAudioRevision[rx];
        session->remoteAudioLastSequence.remove(rx);
        session->remoteAudioLastGeneration.remove(rx);
        publishRemoteAudioConfig(rx);
        if (session->audioStreamEnabled.isEmpty()) {
            m_remoteAudioSockets.remove(session->remoteAudioToken);
            session->remoteAudioToken = 0;
        }
        return;
    }

    auto rIt = session->audioResamplers.find(rx);
    if (rIt != session->audioResamplers.end()) {
        releaseRxAudioResampler(rIt.value());
        session->audioResamplers.erase(rIt);
        qCInfo(lcTci) << "TciServer: audio resampler destroyed for rx" << rx
                      << "peer" << session->peer;
    }
}

// ── cleanupResamplers() ───────────────────────────────────────────────────────
//
// Phase 16 Task 16.3 (sub-commit b): destroys all RESAMPLEF instances for the
// given session. Called from onClientDisconnected and stop() to prevent leaks.
void TciServer::cleanupResamplers(std::shared_ptr<TciClientSession>& session)
{
    if (m_remoteWindow) {
        session->audioStreamEnabled.clear();
        for (int rx = 0; rx < kMaxTciRxSlices; ++rx) {
            ++session->remoteAudioRevision[rx];
        }
        m_remoteAudioSockets.remove(session->remoteAudioToken);
        session->remoteAudioToken = 0;
        session->remoteAudioLastSequence.clear();
        session->remoteAudioLastGeneration.clear();
        for (int rx = 0; rx < kMaxTciRxSlices; ++rx) { publishRemoteAudioConfig(rx); }
        return;
    }
    for (auto rIt = session->audioResamplers.begin();
         rIt != session->audioResamplers.end(); ++rIt) {
        releaseRxAudioResampler(rIt.value());
    }
    session->audioResamplers.clear();
    session->audioStreamEnabled.clear();
    session->audioReadFrame.clear();
}

// ── onAudioFrameReady() ──────────────────────────────────────────────────────
//
// Phase 16 Task 16.3 (sub-commit c): RX audio tap slot.
// Connected via Qt::DirectConnection — runs on the WDSP DSP thread, not the
// main thread. MUST NOT touch Qt objects (QWebSocket, QTimer, m_clients) —
// those are main-thread owned. Only writes to m_audioRing[slice] which is a
// lock-free AudioRingSpsc safe for one producer (DSP thread) + one consumer
// (main thread drain timer).
//
// Contract of our own RxChannel::processIq (src/core/RxChannel.cpp): the
// audioFrameReady signal fires post-DSP with outI (L) and outQ (R) as
// scratch float arrays of length n at srcRate Hz (always 48000 for WDSP
// RX output).
//
// Until 2026-07-28 this was written in "From Thetis" cite grammar
// naming RxChannel.cpp, which claimed Thetis provenance for a
// NereusSDR-original file. Thetis has no such file. Rewritten as a
// plain internal cross-reference so it neither overclaims upstream
// attribution nor gets resolved against the Thetis clone. Deliberately
// avoids repeating the old file:line form, which the author-tag
// verifier would match inside this very comment.
//
// Uses tryPushCopy (non-blocking) so the DSP thread never blocks. Overflow
// (ring full) silently drops the oldest portion — audible as a dropout rather
// than a deadlock.
void TciServer::onAudioFrameReady(int slice, const float* L, const float* R,
                                   int n, int srcRate)
{
    (void)srcRate;  // always 48000 per kWdspRxOutputRate in RxChannel.cpp

    if (slice < 0 || slice >= kMaxPhysicalSlices) { return; }
    if (!m_localAudioMapMutex.tryLock()) { return; }
    const auto unlock = qScopeGuard([this] { m_localAudioMapMutex.unlock(); });
    slice = m_localAudioReceiverForSlice[slice].load(std::memory_order_acquire);
    if (slice < 0 || slice >= kMaxTciRxSlices) { return; }
    if (!L || !R || n <= 0) { return; }

    // Interleave L[i], R[i] into a local scratch then push into the ring.
    // We use a stack-local buffer to avoid heap alloc on the audio thread.
    // Max n = audioStreamSamples (2048) per the WDSP buffer size contract;
    // stereo interleaved = 2 * 2048 = 4096 floats max.
    constexpr int kInterleaveMax = 2 * 2048;
    float interleaved[kInterleaveMax];
    const int total = std::min(n * 2, kInterleaveMax);
    const int count = total / 2;
    for (int i = 0; i < count; ++i) {
        interleaved[2 * i]     = L[i];
        interleaved[2 * i + 1] = R[i];
    }

    // tryPushCopy: drops the newest bytes on overflow (partial write).
    // Audio ring is single-producer (DSP thread) / single-consumer (main thread).
    m_audioRing[slice].tryPushCopy(
        reinterpret_cast<const uint8_t*>(interleaved),
        total * static_cast<int>(sizeof(float)));
}

// ── collectRxAudio() / sendRxAudioBlock() ────────────────────────────────────
//
// R-R3-42. The producers (the DSP thread locally, a receive worker in a
// remote window) push interleaved stereo frames into m_audioRing[rx], a
// single-consumer ring. Before this, every subscribed client popped that
// one ring, so two apps on one receiver each received about half of its
// blocks. Now the drain tick is the ring's only consumer: it moves
// everything into m_rxHistory[rx], and each client reads the history from
// its own position. Main thread only; the history is allocated once.

void TciServer::collectRxAudio()
{
    constexpr int kFrameBytes = 2 * static_cast<int>(sizeof(float));
    for (int rx = 0; rx < kMaxTciRxSlices; ++rx) {
        std::vector<float>& history = m_rxHistory[rx];
        bool arrived = m_remotePcmArrived[rx].exchange(false, std::memory_order_acq_rel);
        while (true) {
            const qint64 got = m_audioRing[rx].popInto(
                reinterpret_cast<uint8_t*>(m_drainScratch.data()),
                static_cast<qint64>(kMaxDrainSamples) * static_cast<qint64>(sizeof(float)));
            const int frames = static_cast<int>(got / kFrameBytes);
            if (frames <= 0) {
                break;
            }
            arrived = true;
            for (int i = 0; i < frames; ++i) {
                const std::size_t slot = static_cast<std::size_t>(
                    (m_rxFramesWritten[rx] + static_cast<quint64>(i)) % kRxHistoryFrames);
                history[2 * slot]     = m_drainScratch[static_cast<std::size_t>(2 * i)];
                history[2 * slot + 1] = m_drainScratch[static_cast<std::size_t>(2 * i + 1)];
            }
            m_rxFramesWritten[rx] += static_cast<quint64>(frames);
        }
        // A receiver whose stop the operator was told about is back. The
        // notice goes once no receiver is stopped, unless something newer
        // than a stop (a refused transmit) is showing.
        if (arrived && m_rxStoppedNotice[rx]) {
            m_rxStoppedNotice[rx] = false;
            m_remoteUnavailable[rx] = false;
            bool anyStopped = false;
            for (bool stopped : m_rxStoppedNotice) { anyStopped = anyStopped || stopped; }
            if (!anyStopped && m_noticeFromReceiverStop) {
                m_noticeReason.clear();
                const QPointer<TciServer> self(this);
                emit operatorNoticeCleared();
                if (!self) { return; }
            }
        }
    }
}

void TciServer::sendRxAudioBlock(QWebSocket* ws,
                                 const std::shared_ptr<TciClientSession>& sessionPtr, int rx)
{
    TciClientSession& session = *sessionPtr;
    if (rx < 0 || rx >= kMaxTciRxSlices) { return; }

    // audioStreamSamples is per channel (frames); default 2048.
    const int channels     = session.audioStreamChannels;  // 1 or 2
    const int frames       = session.audioStreamSamples;
    const int totalSamples = frames * channels;
    if (frames <= 0 || totalSamples > kMaxDrainSamples) { return; }  // safety

    const quint64 written = m_rxFramesWritten[rx];
    auto posIt = session.audioReadFrame.find(rx);
    if (posIt == session.audioReadFrame.end()) {
        posIt = session.audioReadFrame.insert(rx, written);
    }
    quint64& readPos = posIt.value();
    if (written - readPos > static_cast<quint64>(kRxHistoryFrames)) {
        // Fell behind by more than the history holds: skip to the oldest
        // audio still there, a dropout rather than a stale backlog.
        readPos = written - static_cast<quint64>(kRxHistoryFrames);
    }
    if (written - readPos < static_cast<quint64>(frames)) {
        return;  // not a whole block yet; wait for the next tick
    }

    // Phase 3J-1 closeout Item 12 (2026-05-12): apply per-slice TCI RX gain
    // BEFORE resample (so the gain stays in the 48k domain and the
    // resampler sees clean amplitude).  Item 13: track block peak |sample|
    // for TciApplet's slice level meter.
    const float sliceGain = m_sliceRxGainLinear[rx].load(std::memory_order_acquire);
    const std::vector<float>& history = m_rxHistory[rx];
    float blockPeak = 0.0f;
    for (int i = 0; i < frames; ++i) {
        const std::size_t slot = static_cast<std::size_t>(
            (readPos + static_cast<quint64>(i)) % kRxHistoryFrames);
        const float left  = history[2 * slot] * sliceGain;
        const float right = history[2 * slot + 1] * sliceGain;
        if (channels <= 1) {
            // From Thetis TCIServer.cs:5897-5900 [v2.10.3.15]: a mono
            // stream carries the left channel: leftPending.CopyTo(...,
            // packetSamples). Before R-R3-42 this path took half a block of
            // interleaved L/R floats and sent them as mono samples.
            m_drainScratch[static_cast<std::size_t>(i)] = left;
            blockPeak = std::max(blockPeak, std::fabs(left));
        } else {
            m_drainScratch[static_cast<std::size_t>(2 * i)]     = left;
            m_drainScratch[static_cast<std::size_t>(2 * i + 1)] = right;
            blockPeak = std::max({blockPeak, std::fabs(left), std::fabs(right)});
        }
    }
    readPos += static_cast<quint64>(frames);
    m_sliceRxPeakAbs[rx].store(blockPeak, std::memory_order_release);

    // Resample if the client requested a rate other than 48000 Hz.
    // Phase 16: xresampleFV resamples using the per-session per-slice
    // RESAMPLEF instance created in handleAudioSubscribe.
    auto rIt = session.audioResamplers.find(rx);
    std::shared_ptr<TciRxAudioResampler> resampler;
    if (rIt != session.audioResamplers.end() && session.audioSampleRate != 48000
        && frames <= kMaxResampleFrames) {
        resampler = rIt.value();
    }
    const int sampleRate = session.audioSampleRate;
    const int sampleType = session.audioSampleType;

    // R-R3-39: WDSP's resampler runs on the receive lane, never here. The
    // lane resamples and encodes the block, and it is sent from this thread
    // once the lane is done. Jobs run in the order they were posted and
    // their results arrive here in that order, so each client's blocks keep
    // their order; a block that needs no resampler goes the same way while
    // an earlier block of this client is still on the lane.
    if (DspControlThread* lane = rxAudioLane();
        lane != nullptr && (resampler || session.rxAudioBlocksOnLane > 0)) {
        const quint64 generation = m_localAudioGeneration[rx];
        std::vector<float> block(m_drainScratch.begin(),
                                 m_drainScratch.begin() + totalSamples);
        ++session.rxAudioBlocksOnLane;
        const std::weak_ptr<TciClientSession> weakSession = sessionPtr;
        const QPointer<QWebSocket> socket(ws);
        lane->request<QByteArray>(
            [resampler, block = std::move(block), frames, channels, rx, sampleRate,
             sampleType]() {
                int outSamples = frames * channels;
                const float* samples = block.data();
                if (resampler) {
                    samples = resampler->resample(block.data(), frames, channels, outSamples);
                }
                // The same encoding as the at-once path below.
                return TciBinaryFrame::buildStreamPayload(
                    rx, sampleRate, sampleType, outSamples,
                    static_cast<int>(TciStreamType::RxAudioStream), channels, samples);
            },
            this,
            [this, weakSession, socket, rx, generation](QByteArray frame) {
                const std::shared_ptr<TciClientSession> live = weakSession.lock();
                if (!live) {
                    return;   // the client has gone
                }
                --live->rxAudioBlocksOnLane;
                if (!socket || !m_clients.contains(socket.data())
                    || generation != m_localAudioGeneration[rx]
                    || !live->audioStreamEnabled.contains(rx)) {
                    return;
                }
                socket->sendBinaryMessage(frame);
            });
        return;
    }

    // A local model without a receive lane uses the original at-once path.
    const float* samples = m_drainScratch.data();
    int outSamples = totalSamples;
    if (resampler) {
        samples = resampler->resample(m_drainScratch.data(), frames, channels, outSamples);
    }

    // Encode + send binary frame.
    // From Thetis TCIServer.cs:5510 [v2.10.3.13]:
    //   sendBinaryFrame(buildStreamPayload(receiver, sampleRate,
    //       sampleType, interleaved.Length, RX_AUDIO_STREAM,
    //       channels, encoded));
    const QByteArray frame = TciBinaryFrame::buildStreamPayload(
        rx,
        sampleRate,
        sampleType,
        outSamples,         // flat count (length field in header)
        static_cast<int>(TciStreamType::RxAudioStream),
        channels,
        samples);
    ws->sendBinaryMessage(frame);
}

// ── R-R3-42: remote window receiver audio ────────────────────────────────────

void TciServer::publishRemoteAudioConfig(int receiver)
{
    if (!m_remoteWindow || receiver < 0 || receiver >= kMaxTciRxSlices
        || !m_remoteStage[receiver]) { return; }
    std::vector<RemoteTciAudioStage::ClientConfig> clients;
    clients.reserve(RemoteTciAudioStage::kMaxClients);
    for (auto it = m_clients.cbegin(); it != m_clients.cend(); ++it) {
        const TciClientSession& session = *it.value();
        if (!session.audioStreamEnabled.contains(receiver) || session.remoteAudioToken == 0) {
            continue;
        }
        clients.push_back({session.remoteAudioToken, session.remoteAudioRevision.value(receiver),
                           session.audioSampleRate, session.audioStreamChannels,
                           session.audioSampleType, session.audioStreamSamples,
                           m_sliceRxGainLinear[receiver].load(std::memory_order_acquire)});
    }
    m_remoteStage[receiver]->publish(std::move(clients));
}

void TciServer::refreshRemoteAudioGain(int receiver)
{
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
        TciClientSession& session = *it.value();
        if (session.audioStreamEnabled.contains(receiver)) {
            ++session.remoteAudioRevision[receiver];
            session.remoteAudioLastSequence.remove(receiver);
        }
    }
    publishRemoteAudioConfig(receiver);
}

void TciServer::drainRemoteAudio(
    const std::function<qint64(QWebSocket*, const QByteArray&)>& send)
{
    if (!m_remoteWindow) { return; }
    constexpr qint64 kMaxSocketPendingBytes = 262272;
    constexpr qint64 kMaxTickBytes = 262272;
    qint64 sentBytes = 0;
    int sentFrames = 0;
    int attemptedFrames = 0;
    int emptyReceivers = 0;
    quint64 droppedThisTick = 0;
    for (int rx = 0; rx < kMaxTciRxSlices; ++rx) {
        if (!m_remoteStage[rx]) { continue; }
        const auto diagnostics = m_remoteStage[rx]->diagnostics();
        if (diagnostics.mailboxEvictions < m_lastRemoteMailboxEvictions[rx]) {
            m_lastRemoteMailboxEvictions[rx] = 0;
        }
        if (diagnostics.historySkippedFrames < m_lastRemoteHistorySkips[rx]) {
            m_lastRemoteHistorySkips[rx] = 0;
        }
        droppedThisTick += diagnostics.mailboxEvictions - m_lastRemoteMailboxEvictions[rx];
        droppedThisTick += diagnostics.historySkippedFrames - m_lastRemoteHistorySkips[rx];
        m_lastRemoteMailboxEvictions[rx] = diagnostics.mailboxEvictions;
        m_lastRemoteHistorySkips[rx] = diagnostics.historySkippedFrames;
    }
    // A rejected result consumes work too. Otherwise a refilling mailbox
    // can keep this UI tick in the loop without ever increasing sentFrames.
    while (attemptedFrames < 2 && emptyReceivers < kMaxTciRxSlices) {
        const int rx = m_nextRemoteDrainReceiver;
        m_nextRemoteDrainReceiver = (rx + 1) % kMaxTciRxSlices;
        const std::shared_ptr<RemoteTciAudioStage> stage = m_remoteStage[rx];
        RemoteTciAudioStage::Result result;
        if (!stage || !stage->popNext(&result)) {
            ++emptyReceivers;
            continue;
        }
        ++attemptedFrames;
        emptyReceivers = 0;
        if (result.receiverGeneration != stage->generation()
            || !m_remoteRequested[rx] || result.bytes.size() > kMaxTickBytes - sentBytes) {
            continue;
        }
        const QPointer<QWebSocket> socket = m_remoteAudioSockets.value(result.token);
        if (!socket) { continue; }
        const auto sessionIt = m_clients.constFind(socket.data());
        if (sessionIt == m_clients.cend()) { continue; }
        TciClientSession& session = *sessionIt.value();
        if (session.remoteAudioToken != result.token
            || session.remoteAudioRevision.value(rx) != result.revision
            || !session.audioStreamEnabled.contains(rx) || session.socket != socket.data()
            || session.disconnected) { continue; }
        if (session.remoteAudioLastGeneration.value(rx) != result.receiverGeneration) {
            session.remoteAudioLastGeneration.insert(rx, result.receiverGeneration);
            session.remoteAudioLastSequence.insert(rx, 0);
        }
        if (result.sequence <= session.remoteAudioLastSequence.value(rx)) { continue; }
        session.remoteAudioLastSequence.insert(rx, result.sequence);
        if (socket->bytesToWrite() + result.bytes.size() > kMaxSocketPendingBytes) {
            ++m_socketBackpressureDrops;
            stage->noteSocketBackpressureDrop();
            ++droppedThisTick;
            continue;
        }
        const QPointer<TciServer> self(this);
        const qint64 queued = send ? send(socket.data(), result.bytes)
                                   : socket->sendBinaryMessage(result.bytes);
        // Sending can synchronously retire the socket, stream, or this
        // server. No session reference or server state is safe until the
        // server and stage are checked again.
        if (!self) { return; }
        if (m_remoteStage[rx] != stage || !m_remoteRequested[rx]) { return; }
        if (queued < 0) {
            ++m_socketBackpressureDrops;
            stage->noteSocketBackpressureDrop();
            ++droppedThisTick;
            continue;
        }
        sentBytes += result.bytes.size();
        ++sentFrames;
    }
    if (droppedThisTick > 0) {
        m_remoteSaturationDrops += droppedThisTick;
        m_remoteSaturationQuietTicks = 0;
        if (!m_remoteSaturationNotified && m_remoteSaturationDrops >= 100) {
            m_remoteSaturationNotified = true;
            raiseOperatorNotice(QString(),
                QStringLiteral("Remote TCI audio cannot keep up; older audio is being skipped."));
        }
    } else if (m_remoteSaturationNotified && sentFrames > 0) {
        if (++m_remoteSaturationQuietTicks >= 200) {
            m_remoteSaturationNotified = false;
            m_remoteSaturationDrops = 0;
            m_remoteSaturationQuietTicks = 0;
            if (m_noticeReason == QStringLiteral(
                    "Remote TCI audio cannot keep up; older audio is being skipped.")) {
                m_noticeReason.clear();
                emit operatorNoticeCleared();
            }
        }
    }
}

void TciServer::setRemoteReceiverAudio(RemoteReceiverAudio source)
{
    for (int rx = 0; rx < kMaxTciRxSlices; ++rx) {
        if (m_remoteRequested[rx]) {
            if (m_remoteStage[rx]) { m_remoteStage[rx]->publish({}); }
            m_remoteStage[rx].reset();
            m_remoteRequested[rx] = false;
            m_remoteUnavailable[rx] = false;
            if (m_remoteAudio.release) {
                m_remoteAudio.release(rx, this);
            }
        }
    }
    m_remoteAudio = std::move(source);
    for (int rx = 0; rx < kMaxTciRxSlices; ++rx) {
        updateRemoteReceiverDemand(rx);
    }
}

void TciServer::setRemoteIqSource(RemoteIqSource source)
{
    for (int rx = 0; rx < kMaxTciRxSlices; ++rx) {
        if (m_remoteIqRequested[rx]) {
            m_remoteIqRequested[rx] = false;
            if (m_remoteIq.release) { m_remoteIq.release(rx); }
        }
    }
    m_remoteIq = std::move(source);
    refreshRemoteIqDemand();
}

void TciServer::refreshRemoteIqDemand()
{
    for (int rx = 0; rx < kMaxTciRxSlices; ++rx) { updateRemoteIqDemand(rx); }
}

void TciServer::updateRemoteIqDemand(int rx)
{
    if (!m_remoteWindow || rx < 0 || rx >= kMaxTciRxSlices) { return; }
    const bool always = AppSettings::instance()
        .value(QStringLiteral("TciAlwaysStreamIq"), QStringLiteral("False")).toString()
        == QStringLiteral("True");
    bool wanted = isRunning() && always;
    for (auto it = m_clients.cbegin(); it != m_clients.cend() && !wanted; ++it) {
        wanted = it.value()->iqStreamEnabled.contains(rx);
    }
    if (wanted && !m_remoteIqRequested[rx]) {
        if (!m_remoteIq.available || !m_remoteIq.available() || !m_remoteIq.request) { return; }
        m_remoteIqRequested[rx] = true;
        m_remoteIq.request(rx);
    } else if (!wanted && m_remoteIqRequested[rx]) {
        m_remoteIqRequested[rx] = false;
        m_remoteIqRate[rx] = 0;
        if (m_remoteIq.release) { m_remoteIq.release(rx); }
    }
}

void TciServer::receiveRemoteIq(int receiver, int sampleRate,
                                const QVector<float>& samples)
{
    if (!m_remoteWindow || receiver < 0 || receiver >= kMaxTciRxSlices
        || !m_remoteIqRequested[receiver] || !isRunning()) { return; }
    setRemoteIqRate(receiver, sampleRate);
    sendIqToSubscribers(receiver, sampleRate, samples);
}

void TciServer::setRemoteIqRate(int receiver, int sampleRate)
{
    if (!m_remoteWindow || receiver < 0 || receiver >= kMaxTciRxSlices
        || sampleRate < 48000 || sampleRate > 384000
        || m_remoteIqRate[receiver] == sampleRate) { return; }
    m_remoteIqRate[receiver] = sampleRate;
    m_protocol->enqueueLocalBroadcast(
        QStringLiteral("iq_samplerate:%1;").arg(
            *std::max_element(m_remoteIqRate.begin(), m_remoteIqRate.end())));
}

void TciServer::remoteIqUnavailable(int receiver, const QString& reason)
{
    if (!m_remoteWindow || receiver < 0 || receiver >= kMaxTciRxSlices) { return; }
    m_remoteIqRate[receiver] = 0;
    for (auto it = m_clients.cbegin(); it != m_clients.cend(); ++it) {
        if (it.value()->iqStreamEnabled.contains(receiver)) {
            raiseOperatorNotice(it.value()->peer, reason);
        }
    }
}

bool TciServer::remoteReceiverRequested(int rx) const
{
    return rx >= 0 && rx < kMaxTciRxSlices && m_remoteRequested[rx];
}

std::optional<RemoteTciAudioStage::Diagnostics> TciServer::remoteAudioDiagnostics(int rx) const
{
    if (rx < 0 || rx >= kMaxTciRxSlices || !m_remoteStage[rx]) { return std::nullopt; }
    return m_remoteStage[rx]->diagnostics();
}

void TciServer::updateRemoteReceiverDemand(int rx)
{
    if (!m_remoteWindow || rx < 0 || rx >= kMaxTciRxSlices) { return; }
    bool wanted = false;
    for (auto it = m_clients.cbegin(); it != m_clients.cend() && !wanted; ++it) {
        wanted = it.value()->audioStreamEnabled.contains(rx);
    }
    if (wanted && !m_remoteRequested[rx]) {
        if (!m_remoteAudio.request) { return; }  // asked for once wired
        m_remoteRequested[rx] = true;
        qCInfo(lcTci) << "TciServer: asking the Core for receiver" << rx << "audio";
        // May call receiverAudioStopped() before it returns (no media yet,
        // or a Core that cannot send it). A "cannot send" answer is acted
        // on here, once request() has returned, never inside it.
        m_remoteRequesting[rx] = true;
        m_remoteStage[rx] = m_remoteAudio.request(rx, this);
        m_remoteRequesting[rx] = false;
        publishRemoteAudioConfig(rx);
        if (m_remoteUnavailable[rx]) {
            stopUnavailableReceiver(rx);
        }
    } else if (!wanted && m_remoteRequested[rx]) {
        if (m_remoteStage[rx]) { m_remoteStage[rx]->publish({}); }
        m_remoteStage[rx].reset();
        m_remoteRequested[rx] = false;
        m_remoteUnavailable[rx] = false;
        qCInfo(lcTci) << "TciServer: releasing receiver" << rx << "audio";
        if (m_remoteAudio.release) {
            m_remoteAudio.release(rx, this);
        }
    }
}

void TciServer::receiverAudioBlock(int sliceId, const float* interleavedStereo, int frames)
{
    if (m_remoteWindow && sliceId >= 0 && sliceId < kMaxTciRxSlices) {
        m_remotePcmArrived[sliceId].store(true, std::memory_order_release);
        return;
    }
    // Receive worker thread, under the stream's fan-out lock. The worker
    // appends the same PCM to its TCI stage after this callback returns and
    // the fan-out lock is released. Only the arrival notice is needed here.
    if (sliceId < 0 || sliceId >= kMaxTciRxSlices || !interleavedStereo || frames <= 0) {
        return;
    }
    m_audioRing[sliceId].tryPushCopy(
        reinterpret_cast<const uint8_t*>(interleavedStereo),
        static_cast<qint64>(frames) * 2 * static_cast<qint64>(sizeof(float)));
}

void TciServer::receiverAudioStopped(int sliceId, const QString& reason)
{
    if (sliceId < 0 || sliceId >= kMaxTciRxSlices) { return; }
    const bool unavailable = !m_remoteAudio.unavailableReason.isEmpty()
        && reason == m_remoteAudio.unavailableReason;
    if (unavailable) {
        m_remoteUnavailable[sliceId] = true;
    }
    m_rxStoppedNotice[sliceId] = true;
    qCInfo(lcTci) << "TciServer: receiver" << sliceId << "audio stopped:" << reason;
    // No toast while the window has no media connection yet (an app that
    // starts before the window connects, or a link that drops): the window
    // already says that, and the audio returns by itself. The applet and
    // the TCI log window still show it.
    const bool quiet =
        reason == remoteAudioOffReasonToWire(RemoteAudioOffReason::MediaNotReady);
    const bool toast =
        raiseOperatorNotice(QString(), reason, /*receiverStop=*/true, quiet, sliceId);
    emit receiverStopNotice(sliceId, reason, toast);
    // R-R3-42 fix wave: the answer can come long after audio_start was
    // echoed (the media connection came up later). The apps must not keep
    // waiting on a stream that will never come.
    if (unavailable && !m_remoteRequesting[sliceId] && m_remoteRequested[sliceId]) {
        stopUnavailableReceiver(sliceId);
    }
}

void TciServer::stopUnavailableReceiver(int rx)
{
    if (rx < 0 || rx >= kMaxTciRxSlices) { return; }
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
        std::shared_ptr<TciClientSession>& session = it.value();
        if (!session->audioStreamEnabled.contains(rx)) { continue; }
        handleAudioUnsubscribe(session, rx);
        // The app whose audio_start is being answered right now was never
        // told the stream started; every other one was.
        if (session.get() != m_subscribingSession) {
            // From Thetis TCIServer.cs:5891-5906 [v2.10.3.13]: the stop
            // notice an app gets for audio_stop (sendAudioStartStop).
            session->sendQueue.push(TciSendQueue::Priority::Control,
                                    QStringLiteral("audio_stop:%1;").arg(rx));
        }
        qCInfo(lcTci) << "TciServer: receiver" << rx
                      << "audio cannot come from this Core; stopped for" << session->peer;
    }
    updateRemoteReceiverDemand(rx);
}

double TciServer::remoteReceiverLevelDbm() const
{
    // The Core calibrates each slice's meter as a local window calibrates
    // rx1Dbm (SliceMeterPump: RXA_S_AV plus rxMeterOffsetDb, Thetis
    // console.cs:46828 [v2.10.3.13]), and the window mirrors it: the value
    // the window's own S-meter draws (MeterPoller::pollRemoteRxMeters).
    constexpr double kFloorDbm = -140.0;
    const SliceModel* slice = m_model ? m_model->sliceById(0) : nullptr;
    if (!slice) { return kFloorDbm; }
    const double dbm = slice->signalAverageDbm();
    // The Core has no reading.
    if (!std::isfinite(dbm) || dbm <= SliceMeterPump::kNoReadingDbm) { return kFloorDbm; }
    return dbm;
}

// ── iPhone app plan Task 35: a remote window's TCI transmit ────────────────

void TciServer::setRemoteTransmit(RemoteTransmit forward)
{
    if (!m_remoteWindow) {
        return;
    }
    // A key this window holds through the old forwarder is released there.
    if (m_remoteKeyEpoch != 0 && m_remoteTransmit.unkey) {
        m_remoteTransmit.unkey(m_remoteKeyEpoch);
        m_remoteKeyEpoch = 0;
        endRemoteKey();
    }
    m_remoteKeyPending = false;
    m_remoteReleaseWhilePending = false;
    m_remoteKeyClient = nullptr;
    ++m_remoteKeyGeneration;
    m_remoteTransmit = std::move(forward);
    m_protocol->setRemoteTransmitForwarded(forwardsRemoteTransmit());
}

bool TciServer::forwardsRemoteTransmit() const
{
    return m_remoteWindow && static_cast<bool>(m_remoteTransmit.key)
        && static_cast<bool>(m_remoteTransmit.unkey);
}

void TciServer::handleRemoteTrx(QWebSocket* ws, const QString& peer, int rx, bool wantsMox,
                                bool hasTciArg)
{
    const QPointer<QWebSocket> asker(ws);
    const auto answerAsker = [this, asker](int trx, bool keyed) {
        if (asker.isNull()) {
            return;
        }
        const auto it = m_clients.find(asker.data());
        if (it != m_clients.end()) {
            it.value()->sendQueue.push(TciSendQueue::Priority::Control,
                                       QStringLiteral("trx:%1,%2;")
                                           .arg(trx)
                                           .arg(keyed ? QStringLiteral("true")
                                                      : QStringLiteral("false")));
        }
    };

    if (!wantsMox) {
        // trx:N,false: this window's key only (ruling 8.14); any app of this
        // server may release it, as any app's trx:N,false releases a TCI key
        // in Thetis's handleTrxMessage (TCIServer.cs:3623-3672
        // [v2.10.3.15]).
        if (m_remoteKeyPending) {
            m_remoteReleaseWhilePending = true;
        }
        if (m_remoteKeyEpoch != 0) {
            qCInfo(lcTci) << "TciServer: trx release from" << peer
                          << "unkeys this window's key, epoch" << m_remoteKeyEpoch;
            m_remoteTransmit.unkey(m_remoteKeyEpoch);
            m_remoteKeyEpoch = 0;
            endRemoteKey();
        }
        answerAsker(rx, false);
        return;
    }

    // Thetis's handleTrxMessage, TCIServer.cs:3623-3661 [v2.10.3.15]:
    //   if (bMox && alreadyMox) { ...; return; }
    // Among this server's apps: while this window's key is on (or being
    // asked for), another trx:N,true does nothing and is not answered.
    if (m_remoteKeyEpoch != 0 || m_remoteKeyPending) {
        qCInfo(lcTci) << "TciServer: trx from" << peer
                      << "ignored: this window's key is already on, as Thetis does";
        return;
    }

    m_remoteKeyPending = true;
    m_remoteReleaseWhilePending = false;
    // Fix round 2 (Critical 1, RD-C1): which app keyed.
    m_remoteKeyClient = asker;
    const quint64 generation = ++m_remoteKeyGeneration;
    qCInfo(lcTci) << "TciServer: trx from" << peer << "forwarded to the Core as a program's key";
    m_remoteTransmit.key([this, asker, peer, rx, hasTciArg, generation, answerAsker](
                             const RemoteKeyAnswer& answer) {
        if (generation != m_remoteKeyGeneration || !m_remoteKeyPending) {
            // A stale answer (the server stopped or the forwarder changed):
            // an accepted key it brings is released at once.
            if (answer.accepted && answer.epoch != 0 && m_remoteTransmit.unkey) {
                m_remoteTransmit.unkey(answer.epoch);
            }
            return;
        }
        m_remoteKeyPending = false;
        if (!answer.accepted) {
            // The holder rule refused it: no TX audio lock; the app is
            // answered with the real state, and the operator hears why.
            qCInfo(lcTci) << "TciServer: the Core refused the key for" << peer << ":"
                          << answer.reason;
            answerAsker(rx, false);
            raiseOperatorNotice(peer, answer.reason);
            return;
        }
        if (m_remoteReleaseWhilePending) {
            // The app let go before the Core answered.
            m_remoteReleaseWhilePending = false;
            m_remoteKeyClient = nullptr;
            m_remoteTransmit.unkey(answer.epoch);
            answerAsker(rx, false);
            return;
        }
        // Fix round 2 (Critical 1, RD-C1): the app that asked is gone; no
        // key is kept for it.
        if (asker.isNull() || !m_clients.contains(asker.data())) {
            qCInfo(lcTci) << "TciServer: the Core accepted the key for" << peer
                          << "after it left; released, epoch" << answer.epoch;
            m_remoteKeyClient = nullptr;
            m_remoteTransmit.unkey(answer.epoch);
            return;
        }
        m_remoteKeyEpoch = answer.epoch;
        // Admitted: now the TX audio lock (Thetis's
        // TryAcquireActiveTxAudioListener, TCIServer.cs:8146-8163
        // [v2.10.3.15]: granted when nobody holds it or the asker does).
        if (hasTciArg && !asker.isNull()
            && (m_txAudioActiveClient.isNull() || m_txAudioActiveClient.data() == asker.data())) {
            m_txAudioActiveClient = asker.data();
            qCInfo(lcTci) << "TciServer: TX audio mutex acquired by" << peer;
            emit txAudioActiveClientChanged(asker.data());
            startTxChrono(asker.data(), rx);
        }
        answerAsker(rx, true);
        broadcastRemoteKeyState(true);
    });
}

void TciServer::endRemoteKey()
{
    if (!m_txAudioActiveClient.isNull()) {
        m_txAudioActiveClient = nullptr;
        qCInfo(lcTci) << "TciServer: TX audio mutex released: this window's key ended";
        emit txAudioActiveClientChanged(nullptr);
        stopTxChrono();
    }
    broadcastRemoteKeyState(false);
}

void TciServer::broadcastRemoteKeyState(bool on)
{
    // As the MOX change broadcast (hookGlobalBroadcasts, Thetis sendMOX at
    // TCIServer.cs:2207-2211 [v2.10.3.13]) tells every app, for this
    // window's key.
    const QString boolStr = on ? QStringLiteral("true") : QStringLiteral("false");
    m_protocol->enqueueLocalBroadcast(QStringLiteral("trx:0,%1;").arg(boolStr));
    m_protocol->enqueueLocalBroadcast(QStringLiteral("trx:1,false;"));
    broadcastPendingNotifications();
}

bool TciServer::raiseOperatorNotice(const QString& peer, const QString& reason,
                                    bool receiverStop, bool quiet, int rx)
{
    // A repeat of the same notice within 30 s is logged and shown, but not
    // toasted again: WSJT-X, for one, asks to transmit every period. A
    // receiver stop is the same notice only for the same receiver.
    constexpr qint64 kRepeatToastQuietMs = 30000;
    if (!m_noticeClock.isValid()) {
        m_noticeClock.start();
    }
    const qint64 nowMs = m_noticeClock.elapsed();
    const QString key = QString::number(rx) + QLatin1Char(':') + reason;
    const auto last = m_noticeToastAtMs.constFind(key);
    const bool repeat = last != m_noticeToastAtMs.constEnd()
        && nowMs - last.value() < kRepeatToastQuietMs;
    m_noticeReason = reason;
    m_noticeFromReceiverStop = receiverStop;
    if (!repeat) {
        m_noticeToastAtMs.insert(key, nowMs);
    }
    const bool toast = !repeat && !quiet;
    emit operatorNotice(peer, reason, toast);
    return toast;
}

// ── onTextMessageReceived() ──────────────────────────────────────────────────

void TciServer::onTextMessageReceived(const QString& msg)
{
    auto* ws = qobject_cast<QWebSocket*>(sender());
    if (!ws) { return; }
    auto it = m_clients.find(ws);
    if (it == m_clients.end()) { return; }

    auto& session = it.value();
    session->lastCommand   = msg;
    session->lastCommandAt = QDateTime::currentMSecsSinceEpoch();

    // Task 7 follow-up (R-R3-49): this app took (or kept) the TX audio lock
    // for its trx:N,true,tci. Checked again after the protocol's setMox.
    bool trxTookTxAudio = false;
    // Fix wave RD-C1: a trx the protocol acts on locally (not a desktop
    // host, remote window or station server), its request, and the TCI
    // level before it, to record which app keyed.
    bool localTrx = false;
    bool localTrxWantsMox = false;
    const QPointer<MoxController> trxMox = m_model ? m_model->moxController() : nullptr;
    const bool tciLevelBefore = trxMox && trxMox->isTciPttHeld();

    // Phase 3J-1 closeout Item 2 (2026-05-12): firehose for TciLogWindow.
    // Strip the trailing ';' for readability in the log view.  Peer comes
    // from the session struct populated in onNewConnection.
    {
        QString logLine = msg;
        if (logLine.endsWith(QLatin1Char(';'))) {
            logLine.chop(1);
        }
        emit messageLogged(QStringLiteral("in"), session->peer, logLine,
                           session->lastCommandAt);
    }

    // Phase 16 Task 16.3 (sub-commit b): intercept audio_start/audio_stop for
    // per-client subscription state and WDSP resampler lifecycle. This runs
    // BEFORE TciProtocol dispatch because TciProtocol is transport-blind and
    // has no concept of per-client sessions.
    //
    // Phase 17: intercept trx:N,true,tci; / trx:N,false; for TX audio mutex.
    //
    // From Thetis TCIServer.cs:4406-4440 [v2.10.3.13] — audio_start / audio_stop
    // parse the rx index and update m_audioStreamEnabled per-listener.
    // NereusSDR mirrors: parse rx from stripped command, delegate to
    // handleAudioSubscribe / handleAudioUnsubscribe which manage the QHash.
    {
        QString trimmed = msg.trimmed();
        if (trimmed.endsWith(QLatin1Char(';'))) {
            trimmed.chop(1);
        }
        const QString kAudioStart = QStringLiteral("audio_start:");
        const QString kAudioStop  = QStringLiteral("audio_stop:");
        const QString kIqStart    = QStringLiteral("iq_start:");
        const QString kIqStop     = QStringLiteral("iq_stop:");
        if (trimmed.startsWith(kAudioStart)) {
            bool ok = false;
            const int rx = trimmed.mid(kAudioStart.size()).trimmed().toInt(&ok);
            if (ok && rx >= 0 && rx <= 1) {
                if (m_remoteWindow && session->audioStreamEnabled.isEmpty()) {
                    int active = 0;
                    for (auto cit = m_clients.cbegin(); cit != m_clients.cend(); ++cit) {
                        if (!cit.value()->audioStreamEnabled.isEmpty()) { ++active; }
                    }
                    if (active >= RemoteTciAudioStage::kMaxClients) {
                        raiseOperatorNotice(session->peer,
                            QStringLiteral("Remote TCI receiver audio is limited to eight clients"));
                        return;
                    }
                }
                handleAudioSubscribe(session, rx);
                if (m_remoteWindow) {
                    // R-R3-42: TCI receiver N is the Core's slice N, asked
                    // for while at least one app listens.
                    m_subscribingSession = session.get();
                    updateRemoteReceiverDemand(rx);
                    m_subscribingSession = nullptr;
                    if (!session->audioStreamEnabled.contains(rx)) {
                        // The Core answered at once that it cannot send it
                        // (stopUnavailableReceiver): not told it started.
                        return;
                    }
                }
                // Phase 26 review finding #2: send confirmation echo.
                // From Thetis TCIServer.cs:5891-5906 [v2.10.3.13] —
                // handleAudioStart calls sendAudioStartStop(rx, true) after
                // adding rx to m_audioStreamEnabled.  Confirmation verb matches
                // the incoming command verb exactly.
                session->sendQueue.push(TciSendQueue::Priority::Control,
                    QStringLiteral("audio_start:%1;").arg(rx));
            }
        } else if (trimmed.startsWith(kAudioStop)) {
            bool ok = false;
            const int rx = trimmed.mid(kAudioStop.size()).trimmed().toInt(&ok);
            if (ok && rx >= 0 && rx <= 1) {
                handleAudioUnsubscribe(session, rx);
                updateRemoteReceiverDemand(rx);  // R-R3-42: last app out releases it
                // Phase 26 review finding #2: send confirmation echo.
                // From Thetis TCIServer.cs:5891-5906 [v2.10.3.13] —
                // handleAudioStart calls sendAudioStartStop(rx, false) after
                // removing rx from m_audioStreamEnabled.
                session->sendQueue.push(TciSendQueue::Priority::Control,
                    QStringLiteral("audio_stop:%1;").arg(rx));
            }
        } else if (trimmed.startsWith(kIqStart)) {
            // Phase 18 Task 18.1: promote iq_start:N; stub to real per-client
            // IQ subscription tracking.  Mirrors audio_start handling above.
            // From Thetis TCIServer.cs:5022-5025 [v2.10.3.13] — iq_start/stop
            // update m_iqStreamEnabled per-listener.
            bool ok = false;
            const int rx = trimmed.mid(kIqStart.size()).trimmed().toInt(&ok);
            if (m_remoteWindow) {
                if (ok && rx >= 0 && rx <= 1) {
                    if (!m_remoteIq.available || !m_remoteIq.available()) {
                        raiseOperatorNotice(session->peer,
                                            QString::fromLatin1(kRemoteIqRefusedReason));
                    } else {
                        session->iqStreamEnabled.insert(rx);
                        updateRemoteIqDemand(rx);
                        session->sendQueue.push(TciSendQueue::Priority::Control,
                                                QStringLiteral("iq_start:%1;").arg(rx));
                    }
                }
            } else if (ok && rx >= 0 && rx <= 1) {
                if (!session->iqStreamEnabled.contains(rx)) {
                    session->iqStreamEnabled.insert(rx);
                    qCInfo(lcTci) << "TciServer: IQ stream subscribed rx" << rx
                                  << "peer" << session->peer;
                }
                // Phase 26 review finding #2: send confirmation echo.
                // From Thetis TCIServer.cs:5797-5813 [v2.10.3.13] —
                // handleIQStart calls sendIQStartStop(rx, true) after updating
                // m_iqStreamEnabled.
                session->sendQueue.push(TciSendQueue::Priority::Control,
                    QStringLiteral("iq_start:%1;").arg(rx));
            }
        } else if (trimmed.startsWith(kIqStop)) {
            bool ok = false;
            const int rx = trimmed.mid(kIqStop.size()).trimmed().toInt(&ok);
            if (ok && rx >= 0 && rx <= 1) {
                if (session->iqStreamEnabled.remove(rx)) {
                    qCInfo(lcTci) << "TciServer: IQ stream unsubscribed rx" << rx
                                  << "peer" << session->peer;
                }
                // Phase 26 review finding #2: send confirmation echo.
                // From Thetis TCIServer.cs:5797-5813 [v2.10.3.13] —
                // handleIQStart calls sendIQStartStop(rx, false) after removing
                // from m_iqStreamEnabled.
                session->sendQueue.push(TciSendQueue::Priority::Control,
                    QStringLiteral("iq_stop:%1;").arg(rx));
                updateRemoteIqDemand(rx);
            }
        }

        // Phase 26 review finding #1: audio config commands must write the
        // per-client session struct so the drain loop picks up negotiated
        // parameters.  TciProtocol handlers update the shared RadioModel (for
        // backward-compat with protocol-level tests) but cannot see per-client
        // state; this interceptor is the authoritative write path.
        //
        // From Thetis TCIServer.cs:5740-5795 [v2.10.3.13] — handleAudioSampleRate.
        // From Thetis TCIServer.cs:5908-5934 [v2.10.3.13] — handleAudioStreamSampleType.
        // From Thetis TCIServer.cs:5935-5949 [v2.10.3.13] — handleAudioStreamChannels.
        // From Thetis TCIServer.cs:5951-5982 [v2.10.3.13] — handleAudioStreamSamples.
        {
            const QString kAudioSampleRate      = QStringLiteral("audio_samplerate:");
            const QString kAudioStreamSamples   = QStringLiteral("audio_stream_samples:");
            const QString kAudioStreamChannels  = QStringLiteral("audio_stream_channels:");
            const QString kAudioStreamSampleType = QStringLiteral("audio_stream_sample_type:");
            const auto previousConfig = std::tuple(session->audioSampleRate,
                session->audioStreamSamples, session->audioStreamChannels,
                session->audioSampleType);

            if (trimmed.startsWith(kAudioSampleRate)) {
                // From Thetis TCIServer.cs:5740-5795 [v2.10.3.13]:
                // Thetis comment: "// we can't change the H/W sample rate here"
                // — it echoes back whatever the client requests.
                //
                // Phase 3J-1 review P1.2: bound the accepted range to [8000,
                // 384000].  The drain path uses a fixed-size output buffer sized
                // for kMaxDrainSamples * 8 = 2048*2*8 = 32768 floats, which gives
                // exactly 8x upsample headroom (384000 / 48000 = 8).  A client
                // that sends audio_samplerate:1000000; would overflow that buffer
                // (1000000/48000 ≈ 20.8x) with no guard.  Silently ignore values
                // outside [8000, 384000] — do NOT echo confirmation for out-of-
                // range values (callers like WSJT-X re-send until they see the
                // echo; rejecting out-of-range is the safe failure mode).
                //
                // kMinAudioSampleRate 8000: lowest practical monaural rate;
                //   matches WDSP resample lower-bound.
                // kMaxAudioSampleRate 384000: highest HPSDR audio rate; 8x input.
                constexpr int kMinAudioSampleRate = 8000;
                constexpr int kMaxAudioSampleRate = 384000;

                bool ok = false;
                const int sr = trimmed.mid(kAudioSampleRate.size()).trimmed().toInt(&ok);
                if (!ok || sr < kMinAudioSampleRate || sr > kMaxAudioSampleRate) {
                    qCWarning(lcTci) << "TciServer: rejecting out-of-range audio_samplerate="
                                     << (ok ? sr : -1) << "peer" << session->peer;
                    // Do not write session->audioSampleRate; do not echo confirmation.
                } else {
                    session->audioSampleRate = sr;
                    qCInfo(lcTci) << "TciServer: session audioSampleRate set to" << sr
                                  << "peer" << session->peer;
                    // Recreate the resampler for any active audio subscriptions,
                    // since the target rate has changed.  Destroy old, rebuild.
                    // R-R3-39: both on the receive lane, after every block
                    // already posted at the old rate.
                    if (!m_remoteWindow) {
                        for (int rx : session->audioStreamEnabled) {
                            auto rIt = session->audioResamplers.find(rx);
                            if (rIt != session->audioResamplers.end()) {
                                releaseRxAudioResampler(rIt.value());
                                session->audioResamplers.erase(rIt);
                            }
                            session->audioResamplers.insert(rx, makeRxAudioResampler(48000, sr));
                        }
                    }
                }
            } else if (trimmed.startsWith(kAudioStreamSamples)) {
                // From Thetis TCIServer.cs:5951-5982 [v2.10.3.13]:
                // Range [100..2048]; values outside range silently ignored.
                bool ok = false;
                const int n = trimmed.mid(kAudioStreamSamples.size()).trimmed().toInt(&ok);
                if (ok && n >= 100 && n <= 2048) {
                    session->audioStreamSamples = n;
                    session->audioStreamSamplesExplicitlySet = true;
                    qCInfo(lcTci) << "TciServer: session audioStreamSamples set to" << n
                                  << "peer" << session->peer;
                }
            } else if (trimmed.startsWith(kAudioStreamChannels)) {
                // From Thetis TCIServer.cs:5935-5949 [v2.10.3.13]:
                // Accepts 1 (mono) or 2 (stereo); ignores other values.
                bool ok = false;
                const int n = trimmed.mid(kAudioStreamChannels.size()).trimmed().toInt(&ok);
                if (ok && (n == 1 || n == 2)) {
                    session->audioStreamChannels = n;
                    qCInfo(lcTci) << "TciServer: session audioStreamChannels set to" << n
                                  << "peer" << session->peer;
                }
            } else if (trimmed.startsWith(kAudioStreamSampleType)) {
                // From Thetis TCIServer.cs:5908-5934 [v2.10.3.13]:
                // Valid: "int16", "int24", "int32", "float32".  Defaults to float32.
                // int enum encoding: 0=int16, 1=int24, 2=int32, 3=float32.
                const QString typeStr = trimmed.mid(kAudioStreamSampleType.size()).trimmed().toLower();
                int typeInt = 3;  // float32 default (matches TciClientSession default)
                if (typeStr == QStringLiteral("int16"))   { typeInt = 0; }
                else if (typeStr == QStringLiteral("int24"))  { typeInt = 1; }
                else if (typeStr == QStringLiteral("int32"))  { typeInt = 2; }
                else if (typeStr == QStringLiteral("float32")) { typeInt = 3; }
                session->audioSampleType = typeInt;
                qCInfo(lcTci) << "TciServer: session audioSampleType set to" << typeStr
                              << "(" << typeInt << ")"
                              << "peer" << session->peer;
            }
            if (m_remoteWindow && previousConfig != std::tuple(session->audioSampleRate,
                    session->audioStreamSamples, session->audioStreamChannels,
                    session->audioSampleType)) {
                for (int rx : session->audioStreamEnabled) {
                    ++session->remoteAudioRevision[rx];
                }
                session->remoteAudioLastSequence.clear();
                for (int rx : session->audioStreamEnabled) { publishRemoteAudioConfig(rx); }
            }
        }

        // Phase 19: sensor subscription — intercept rx_sensors_enable and
        // tx_sensors_enable before passing to TciProtocol dispatch.
        //
        // From Thetis TCIServer.cs:4449-4469 [v2.10.3.13] —
        // handleRxSensorsEnable / handleTxSensorsEnable.
        //
        // Wire format:
        //   rx_sensors_enable:true;          — enable with current interval
        //   rx_sensors_enable:true,200;      — enable at 200ms
        //   rx_sensors_enable:false;         — disable
        //   tx_sensors_enable:true[,ms];
        //   tx_sensors_enable:false;
        //
        // From Thetis: args[0] = true/false, args[1] (optional) = intervalMs.
        // If intervalMs is not parseable, the command is silently ignored
        // (matches Thetis handleRxSensorsEnable return-on-parse-fail).
        {
            const QString kRxSensEnable = QStringLiteral("rx_sensors_enable:");
            const QString kTxSensEnable = QStringLiteral("tx_sensors_enable:");

            // Shared helper: parses "true|false[,intervalMs]" and returns
            // false if parsing should be aborted (parse fail per Thetis).
            // From Thetis TCIServer.cs:4449-4469 [v2.10.3.13] — both
            // handleRxSensorsEnable and handleTxSensorsEnable share the
            // same parse shape: args[0]=bool, args[1]=optional int.
            auto parseSensorEnable = [](const QStringList& parts,
                                        int currentInterval,
                                        bool& outEnabled,
                                        int& outInterval) -> bool {
                if (parts.size() < 1 || parts.size() > 2) { return false; }
                const QString enableStr = parts.at(0).trimmed();
                if (enableStr.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0) {
                    outEnabled = true;
                } else if (enableStr.compare(QLatin1String("false"), Qt::CaseInsensitive) == 0) {
                    outEnabled = false;
                } else {
                    return false;  // not a valid bool — ignore per Thetis
                }
                outInterval = currentInterval;
                if (parts.size() == 2) {
                    bool ok = false;
                    const int parsed = parts.at(1).trimmed().toInt(&ok);
                    if (!ok) { return false; }  // intervalMs parse fail — ignore per Thetis
                    outInterval = parsed;
                }
                return true;
            };

            if (trimmed.startsWith(kRxSensEnable)) {
                // From Thetis TCIServer.cs:4449-4459 [v2.10.3.13] — handleRxSensorsEnable.
                const QStringList parts = trimmed.mid(kRxSensEnable.size())
                                              .split(QLatin1Char(','));
                bool enabled   = false;
                int intervalMs = session->rxSensorIntervalMs;
                if (parseSensorEnable(parts, intervalMs, enabled, intervalMs)) {
                    session->rxSensorsEnabled   = enabled;
                    session->rxSensorIntervalMs = intervalMs;

                    // Update the server-wide RX sensor timer interval to the
                    // minimum required across all clients (per Thetis server-level
                    // MinimumRequiredRxSensorInterval, TCIServer.cs:7571-7589).
                    QList<int> intervals;
                    for (auto sit = m_clients.cbegin(); sit != m_clients.cend(); ++sit) {
                        if (sit.value()->rxSensorsEnabled) {
                            intervals.append(sit.value()->rxSensorIntervalMs);
                        }
                    }
                    const int newInterval = TciSensorManager::minimumRequiredInterval(intervals);
                    if (m_rxSensorTimer->interval() != newInterval) {
                        m_rxSensorTimer->setInterval(newInterval);
                    }

                    qCInfo(lcTci) << "TciServer: rx_sensors_enable" << enabled
                                  << "intervalMs" << intervalMs
                                  << "peer" << session->peer;
                }
            } else if (trimmed.startsWith(kTxSensEnable)) {
                // From Thetis TCIServer.cs:4460-4469 [v2.10.3.13] — handleTxSensorsEnable.
                const QStringList parts = trimmed.mid(kTxSensEnable.size())
                                              .split(QLatin1Char(','));
                bool enabled   = false;
                int intervalMs = session->txSensorIntervalMs;
                if (parseSensorEnable(parts, intervalMs, enabled, intervalMs)) {
                    session->txSensorsEnabled   = enabled;
                    session->txSensorIntervalMs = intervalMs;

                    // Update the server-wide TX sensor timer interval to the
                    // minimum required across all clients (per Thetis server-level
                    // MinimumRequiredTxSensorInterval, TCIServer.cs:7591-7603).
                    QList<int> intervals;
                    for (auto sit = m_clients.cbegin(); sit != m_clients.cend(); ++sit) {
                        if (sit.value()->txSensorsEnabled) {
                            intervals.append(sit.value()->txSensorIntervalMs);
                        }
                    }
                    const int newInterval = TciSensorManager::minimumRequiredInterval(intervals);
                    if (m_txSensorTimer->interval() != newInterval) {
                        m_txSensorTimer->setInterval(newInterval);
                    }

                    qCInfo(lcTci) << "TciServer: tx_sensors_enable" << enabled
                                  << "intervalMs" << intervalMs
                                  << "peer" << session->peer;
                }
            }
        }

        // Phase 17: TX audio mutex — intercept trx:N,true,tci; and trx:N,false;
        //
        // Porting from Thetis TCIServer.cs:3489-3516 [v2.10.3.13]:
        //   bool useTciAudio = args.Length > 2 && args[2].ToLower() == "tci";
        //   bool wantsActiveTciPtt = useTciAudio && bOK && bMox && ...;
        //   if (wantsActiveTciPtt) ownsActiveTciPtt = m_server.TryAcquireActiveTxAudioListener(this);
        //   else m_server.ReleaseActiveTxAudioListener(this);
        //   m_tciPttActive = wantsActiveTciPtt && ownsActiveTciPtt;
        //
        // NereusSDR simplification: TryAcquire/Release runs directly in the
        // main-thread slot; no per-listener thread lock needed because all
        // WebSocket callbacks run on the same Qt event loop.
        // M1 (R-R3-48 / R-R3-25): a TX profile or XIT change on the
        // receive-only station server is not made (TciProtocol answers with
        // the kept value); the operator hears why, off the wire.
        if (m_stationReceiveOnly && !m_remoteWindow
            && TciProtocol::isTransmitSettingChange(trimmed)) {
            qCInfo(lcTci) << "TciServer: transmit setting refused on the station server"
                          << trimmed << ", peer" << session->peer;
            raiseOperatorNotice(session->peer,
                                QString::fromLatin1(kStationTransmitRefusedReason));
        }
        {
            // Fix round 1 (Important 1): the command name as TciProtocol
            // reads it, lowered and trimmed (Thetis TCIServer.cs:5288
            // [v2.10.3.15]: parts[0].ToLower().Trim()), so TRX:0,true; is
            // intercepted and owned like trx:0,true;.
            const qsizetype trxColon = trimmed.indexOf(QLatin1Char(':'));
            const bool isTrx = trxColon > 0
                && trimmed.left(trxColon).trimmed().compare(
                       QLatin1String("trx"), Qt::CaseInsensitive) == 0;
            if (isTrx) {
                // Parse "trx:N,bool[,tci]"
                const QString args = trimmed.mid(trxColon + 1);
                const QStringList parts = args.split(QLatin1Char(','));
                if (parts.size() >= 2) {
                    // Is the third arg "tci"?
                    const bool hasTciArg = (parts.size() >= 3 &&
                        parts.at(2).trimmed().compare(QLatin1String("tci"),
                            Qt::CaseInsensitive) == 0);

                    const bool wantsMox = (parts.at(1).trimmed().compare(
                        QLatin1String("true"), Qt::CaseInsensitive) == 0);

                    if (m_desktopHostMode) {
                        const QString requested = parts.at(1).trimmed();
                        if (requested.compare(QLatin1String("true"), Qt::CaseInsensitive) != 0
                            && requested.compare(QLatin1String("false"), Qt::CaseInsensitive) != 0) {
                            return;
                        }
                        bool rxOk = false;
                        const int trxIdx = parts.at(0).trimmed().toInt(&rxOk);
                        const quint64 requestGeneration = ++m_desktopKeyGeneration;
                        if (wantsMox) { m_desktopLatestOnIntent = requestGeneration; }
                        auto* mox = m_model ? m_model->moxController() : nullptr;
                        const KeyerIdentity keyer = KeyerIdentity::station(PttMode::Tci);
                        const QPointer<TciServer> self(this);
                        const QPointer<QWebSocket> client(ws);
                        const QPointer<MoxController> controller(mox);
                        const auto liveClient = [this, self, client, session]() {
                            return self && client && m_server
                                && m_clients.value(client.data()) == session;
                        };
                        const auto answer = [liveClient, session, trxIdx](bool on) {
                            if (!liveClient()) { return; }
                            session->sendQueue.push(TciSendQueue::Priority::Control,
                                QStringLiteral("trx:%1,%2;").arg(trxIdx)
                                    .arg(on ? QStringLiteral("true") : QStringLiteral("false")));
                        };
                        if (!rxOk || desktopSliceForReceiver(trxIdx) < 0 || !mox) {
                            if (rxOk) { answer(false); }
                            return;
                        }
                        if (!wantsMox) {
                            // A replacement app, a manual MOX, the radio PTT, and
                            // another device's key are never this app's program key.
                            if (m_desktopKeyClient.data() == ws) {
                                releaseDesktopProgramKey();
                            } else if (m_tciPttClient.data() == ws) {
                                // Fix wave RD-I3: its key ended another way
                                // (an unkey, StopAllTx) and its TCI level is
                                // still up; its own release drops it.
                                if (!releaseAppTciKey(ws)) { return; }
                            }
                            answer(false);
                            return;
                        }
                        if (mox->isMox()) {
                            // Thetis handleTrxMessage: a second true while keyed
                            // does nothing; it cannot take an existing audio lock.
                            return;
                        }
                        const TxRefusal holderRefusal = mox->hasKeyingGate()
                            ? mox->programKeyRefusal(keyer)
                            : TxRefusals::programNeedsTransmit();
                        if (!holderRefusal.isEmpty()) {
                            raiseOperatorNotice(session->peer, holderRefusal.text);
                            answer(false);
                            return;
                        }
                        if (!m_model->txSliceArbiter()
                            || desktopReceiverForSlice(
                                   m_model->txSliceArbiter()->txBoundSliceId()) < 0) {
                            raiseOperatorNotice(session->peer, TxRefusals::notReady().text);
                            answer(false);
                            return;
                        }
                        // Fix wave RD-I3: the app's key goes through the
                        // same admission as a local app's trx (RadioModel::
                        // setMox), MoxController::onTciPtt and PollPTT: the
                        // manual-key gate, the StopAllTx latch, the held-off
                        // sources and the holder gate at the press edge.
                        // From Thetis console.cs:25470 [v2.10.3.15]:
                        //   if (!_manual_mox && !_disable_ptt && !_rx_only && !_tx_inhibit && !QSKEnabled && !_ganymede_pa_issue)
                        // (cw_ptt, on the lines below it, carries: //[2.10.3.9]MW0LGE only want to do this on semi breakin  [original inline comment from console.cs:25473])
                        // and console.cs:25479-25492 [v2.10.3.15] (_stop_all_tx):
                        //   // we can come in here from a ToT ( StopAllTX() ) //[2.10.3.6]MWLGE fixes #518
                        // A desktop host refuses rather than queues: a press
                        // that keyed nothing drops its TCI level below, so it
                        // never keys later for an app that was told false.
                        // Only its accepted station TCI key may acquire TX
                        // audio. stop()/destruction can run synchronously
                        // inside the key. A transition away from this key
                        // means a later key may own MOX.
                        bool keyTransitionedAway = false;
                        const auto observeKey = connect(mox, &MoxController::moxChanging,
                            mox, [&keyTransitionedAway](int, bool, bool on) {
                                if (!on) { keyTransitionedAway = true; }
                            });
                        mox->onTciPtt(true);
                        QObject::disconnect(observeKey);
                        if (!self) {
                            if (controller && !keyTransitionedAway
                                && controller->isTciPttHeld()) {
                                controller->onTciPtt(false);
                            }
                            return;
                        }
                        if (!controller) { return; }
                        if (requestGeneration != m_desktopKeyGeneration
                            || !liveClient() || !m_desktopHostMode
                            || !controller->isMox()
                            || !(controller->currentKeyer() == keyer)) {
                            // A callback may have stopped the server or
                            // replaced the requesting connection during
                            // the key, or nothing keyed (a manual key, the
                            // StopAllTx latch, a refusal). Never grant its
                            // abandoned key audio, and drop its level.
                            if (m_desktopLatestOnIntent == requestGeneration
                                && !m_desktopKeyHeld
                                && controller->isTciPttHeld()) {
                                controller->onTciPtt(false);
                                if (!self) { return; }
                            }
                            answer(false);
                            return;
                        }
                        m_desktopKeyClient = ws;
                        m_desktopKeyHeld = true;
                        m_tciPttClient = ws;
                        if (hasTciArg && m_txAudioActiveClient.isNull()) {
                            m_txAudioActiveClient = ws;
                            emit txAudioActiveClientChanged(ws);
                            if (!self || !controller) { return; }
                            if (requestGeneration != m_desktopKeyGeneration
                                || !liveClient() || !m_desktopHostMode
                                || !m_desktopKeyHeld
                                || m_desktopKeyClient.data() != client.data()
                                || m_txAudioActiveClient.data() != client.data()
                                || !controller->isMox()
                                || !(controller->currentKeyer() == keyer)) {
                                if (requestGeneration == m_desktopKeyGeneration
                                    && m_txAudioActiveClient.data() == client.data()) {
                                    m_txAudioActiveClient = nullptr;
                                    emit txAudioActiveClientChanged(nullptr);
                                    if (!self) { return; }
                                }
                                if (requestGeneration == m_desktopKeyGeneration
                                    && m_desktopKeyHeld
                                    && m_desktopKeyClient.data() == client.data()) {
                                    releaseDesktopProgramKey();
                                }
                                return;
                            }
                            startTxChrono(client.data(), trxIdx);
                        }
                        answer(true);
                        broadcastPendingNotifications();
                        return;
                    } else if (m_remoteWindow && forwardsRemoteTransmit()) {
                        // Task 35: forwarded to the Core under the holder
                        // rule; the answer comes back later.
                        bool rxOk = false;
                        const int trxIdx = parts.at(0).trimmed().toInt(&rxOk);
                        if (rxOk) {
                            handleRemoteTrx(ws, session->peer, trxIdx, wantsMox, hasTciArg);
                        }
                    } else if (m_remoteWindow || m_stationReceiveOnly) {
                        // R-R3-42 / R-R3-25: no TX audio lock and no
                        // TX_CHRONO in a remote window, nor on the Core's
                        // station server until remote transmit (R-R3-48).
                        // TciProtocol answers trx:N,false without touching
                        // MOX; the operator hears why.
                        if (wantsMox) {
                            qCInfo(lcTci) << "TciServer: transmit refused"
                                          << (m_remoteWindow ? "in a remote window"
                                                             : "on the station server")
                                          << ", peer" << session->peer;
                            raiseOperatorNotice(session->peer,
                                                QString::fromLatin1(m_remoteWindow
                                                    ? kRemoteTransmitRefusedReason
                                                    : kStationTransmitRefusedReason));
                        }
                    } else {
                        // Receiver and transmit gaps plan, Task 4 (R-R3-49):
                        // what an app's trx does while another app holds the
                        // TX audio, as Thetis does it.
                        //
                        // From Thetis TCIServer.cs:3623-3661 [v2.10.3.15] --
                        // handleTrxMessage:
                        //   bool useTciAudio = args.Length > 2 && args[2].ToLower() == "tci";
                        //   bool alreadyMox = consoleThreadSafe.MOX;
                        //   bool wantsActiveTciPtt = useTciAudio && bOK && bMox && (!alreadyMox || alreadyActiveTciPtt);
                        //   if (wantsActiveTciPtt) ownsActiveTciPtt = m_server.TryAcquireActiveTxAudioListener(this);
                        //   else m_server.ReleaseActiveTxAudioListener(this);
                        //   ...
                        //   if (bOK) {
                        //       if (bMox && alreadyMox) { ...; return; }
                        //       ... if (consoleThreadSafe.MOX != bMox) consoleThreadSafe.TCIPTT = bMox;
                        //
                        // So an app's trx:N,true while the transmitter is
                        // keyed does nothing and is not answered; with it
                        // unkeyed, the trx keys it even when another app
                        // holds the TX audio (only the asker's audio is
                        // refused); and any app's trx:N,false releases a TCI
                        // key (the protocol's setMox). Since Task 7 a key
                        // another source holds (the MOX button, the mic, VOX)
                        // stays: PollPTT releases only in PTTMode.TCI. The
                        // asking app is answered with the real state and no
                        // app is told the requested one (Task 7 fix wave).
                        // Unkeying also releases the TX audio
                        // (hookGlobalBroadcasts, OnMoxPreChangeHandler).
                        // Still not ported: the CW break-in guard
                        // (shouldIgnoreTrxForCurrentCwBreakIn, CW transmit is
                        // not built) and the VFOATX/VFOBTX choice by
                        // receiver (TciProtocol::handleTrxCommand).
                        bool rxOk = false;
                        const int trxIdx = parts.at(0).trimmed().toInt(&rxOk);
                        localTrx = rxOk;
                        localTrxWantsMox = wantsMox;
                        const bool alreadyMox = m_model && m_model->mox();
                        const bool alreadyActiveTciPtt =
                            !m_txAudioActiveClient.isNull()
                            && m_txAudioActiveClient.data() == ws;
                        // iPhone app plan Task 77 (ruling 8.14): the holder
                        // rule first. A program keys only while this window
                        // holds transmit; the TX audio lock is taken only
                        // once the holder rule would admit its key, so a
                        // refused key never holds it.
                        const bool holderAdmits = !wantsMox || !m_model
                            || m_model->moxController() == nullptr
                            || m_model->moxController()
                                   ->programKeyRefusal(KeyerIdentity::station(PttMode::Tci))
                                   .isEmpty();
                        const bool wantsActiveTciPtt = hasTciArg && rxOk && wantsMox
                            && holderAdmits && (!alreadyMox || alreadyActiveTciPtt);

                        if (wantsActiveTciPtt) {
                            // From Thetis TCIServer.cs:8146-8163 [v2.10.3.15] --
                            // TryAcquireActiveTxAudioListener: grant if no
                            // current owner or the owner IS this client; else
                            // deny.
                            if (m_txAudioActiveClient.isNull() ||
                                m_txAudioActiveClient.data() == ws) {
                                m_txAudioActiveClient = ws;
                                qCInfo(lcTci) << "TciServer: TX audio mutex acquired by"
                                              << session->peer;
                                // Phase 23: notify indicator / MainWindow.
                                emit txAudioActiveClientChanged(ws);
                                // Phase 3J-1 bench fix (2026-05-10): start
                                // TX_CHRONO timing frames so WSJT-X begins
                                // streaming TX_AUDIO_STREAM binary frames.
                                startTxChrono(ws, trxIdx);
                                trxTookTxAudio = true;
                            } else {
                                // Phase 26 review finding #10: explicit find +
                                // fallback string, zero allocation path.
                                auto heldIt = m_clients.find(m_txAudioActiveClient.data());
                                const QString heldBy = (heldIt != m_clients.end())
                                    ? heldIt.value()->peer
                                    : QStringLiteral("(unknown)");
                                qCInfo(lcTci) << "TciServer: TX audio mutex denied for"
                                              << session->peer
                                              << "(held by" << heldBy << ");"
                                              << "its trx still keys the transmitter, as"
                                                 " Thetis does";
                            }
                        } else if (alreadyActiveTciPtt) {
                            // From Thetis TCIServer.cs:8166-8173 [v2.10.3.15] --
                            // ReleaseActiveTxAudioListener: clear if owner
                            // matches.
                            m_txAudioActiveClient = nullptr;
                            qCInfo(lcTci) << "TciServer: TX audio mutex released by"
                                          << session->peer;
                            // Phase 23: notify indicator / MainWindow.
                            emit txAudioActiveClientChanged(nullptr);
                            // Phase 3J-1 bench fix: stop TX_CHRONO frames.
                            stopTxChrono();
                        }

                        if (rxOk && wantsMox && alreadyMox) {
                            qCInfo(lcTci) << "TciServer: trx from" << session->peer
                                          << "ignored: the transmitter is already keyed,"
                                             " as Thetis does";
                            return;
                        }
                    }
                }
            }
        }
    }

    // From design doc §1 + Sweep B silent-error invariant:
    // handleCommand returns the synchronous response (empty for unknown
    // commands per Sweep B; non-empty for queries that have a reply).
    // Response goes only to the originating client (unicast).
    //
    // Phase 14: push into the per-client TciSendQueue instead of calling
    // sendTextMessage directly. The drain timer pumps frames from the queue
    // in priority order. Coalescing (Thetis m_outboundCoalescedFrames at
    // TCIServer.cs:769-774 [v2.10.3.13]) lands in Phase 15.
    const QString trimmedIq = msg.trimmed();
    if ((trimmedIq == QLatin1String("iq_samplerate;")
        || trimmedIq.startsWith(QLatin1String("iq_samplerate:")))) {
        session->sendQueue.push(TciSendQueue::Priority::Control,
                                QStringLiteral("iq_samplerate:%1;").arg(publishedIqRate()));
        return;
    }
    const QString response = m_protocol->handleCommand(msg);

    // Task 7 follow-up (R-R3-49): the trx keyed nothing and holds no TCI
    // level (refused by the band plan, the interlock or the microphone
    // check, or made under TX inhibit or a PA trip): give the TX audio
    // back and stop TX_CHRONO. Otherwise the lock stays with the app until
    // its trx:N,false or any unkey, and meanwhile a MOX-button, mic or VOX
    // key transmits the app's TCI buffer instead of the microphone and
    // skips R-R3-36's microphone-ready check (RadioModel::
    // pcCaptureGatesKeying). A trx held off by a manual key keeps its
    // level, and the lock, for the key that follows.
    //
    // Not in Thetis: its handleTrxMessage keeps the listener's
    // ownsActiveTciPtt until the app's trx:false or OnMoxPreChangeHandler
    // (TCIServer.cs:3623-3672 [v2.10.3.15]).
    // Fix wave RD-C1: the app whose trx raised the TCI level owns that
    // key until the level drops (any app's trx:N,false, a refusal, an
    // unkey under a block).
    if (localTrx && trxMox) {
        if (!trxMox->isTciPttHeld()) {
            m_tciPttClient = nullptr;
        } else if (localTrxWantsMox && !tciLevelBefore) {
            m_tciPttClient = ws;
        }
    }

    if (trxTookTxAudio && m_model && m_model->moxController() != nullptr
        && !m_model->moxController()->isTciPttHeld()
        && !m_txAudioActiveClient.isNull() && m_txAudioActiveClient.data() == ws) {
        m_txAudioActiveClient = nullptr;
        qCInfo(lcTci) << "TciServer: TX audio mutex released for" << session->peer
                      << "(its trx keyed nothing)";
        emit txAudioActiveClientChanged(nullptr);
        stopTxChrono();
    }

    if (!response.isEmpty()) {
        session->sendQueue.push(TciSendQueue::Priority::Control, response);
    }

    // From design doc §1: notifications drain after each handleCommand and
    // broadcast to ALL clients (including the originator), mirroring Thetis's
    // outbound-frame fan-out at TCIServer.cs:1662-1791 [v2.10.3.13].
    // Phase 14: push into each client's queue instead of direct sendTextMessage.
    broadcastPendingNotifications();
}

// Task 10 (R-R3-49): every pending notification goes to every app, through
// that app's own update gap. The protocol queue holds one list for all apps,
// as before; the gap is per app, as Thetis keeps it per listener
// (TCIServer.cs:750-758 [v2.10.3.15]).
void TciServer::broadcastPendingNotifications()
{
    // Task 12 (R-R3-49): each line keeps the gate its event bound to it
    // when it was queued, so an if line is never sorted by its neighbours.
    QStringList pending;
    std::vector<std::optional<TciUpdateGap::Gate>> gates;
    while (m_protocol->hasPendingNotification()) {
        TciProtocol::PendingLine line = m_protocol->takePendingLine();
        pending << line.frame;
        gates.push_back(line.gate);
    }
    if (pending.isEmpty()) {
        return;
    }
    const qint64 nowMs = m_gapClock.elapsed();
    for (auto sit = m_clients.cbegin(); sit != m_clients.cend(); ++sit) {
        for (const QString& line : sit.value()->updateGap.offer(pending, gates, nowMs)) {
            sit.value()->sendQueue.push(TciSendQueue::Priority::Control, line);
        }
    }
}

// Task 10 (R-R3-49). Thetis applies udTCIRateLimit when the server starts
// (setup.cs:22517 [v2.10.3.15] -> console.SetupTCI -> StartServer) and each
// listener keeps the value it was built with (TCIServer.cs:792-795
// [v2.10.3.15]). Here a change reaches every connected app at once, so the
// control does what it says without restarting the server.
void TciServer::setUpdateGapMs(int ms)
{
    m_updateGapMs = std::clamp(ms, TciUpdateGap::kMinGapMs, TciUpdateGap::kMaxGapMs);
    for (auto sit = m_clients.cbegin(); sit != m_clients.cend(); ++sit) {
        sit.value()->updateGap.setGapMs(m_updateGapMs);
    }
}

// ── onBinaryMessageReceived() ────────────────────────────────────────────────
//
// Phase 17: parse inbound TCI binary frames and route TX_AUDIO_STREAM (type 2)
// to the TX audio pipeline.  All other stream types are silently ignored per
// Thetis TCIServer.cs:5614 [v2.10.3.13] ("if streamType != TX_AUDIO_STREAM … return").
//
// Porting from Thetis TCIServer.cs:5602-5703 [v2.10.3.13] — handleBinaryFrame.
//
// TX mutex: only the client registered as m_txAudioActiveClient may push audio.
// Other clients' binary frames are silently dropped and their txFramesDropped
// counter incremented.  This maps to the Thetis TryAcquireActiveTxAudioListener /
// m_tciPttActive per-client gate (TCIServer.cs:7625-7651 [v2.10.3.13]).

void TciServer::onBinaryMessageReceived(const QByteArray& data)
{
    auto* ws = qobject_cast<QWebSocket*>(sender());
    if (!ws) { return; }
    auto it = m_clients.find(ws);
    if (it == m_clients.end()) { return; }
    auto& session = it.value();

    // From Thetis TCIServer.cs:5604-5605 [v2.10.3.13]:
    //   if (payload == null || payload.Length < 64) return;
    if (data.size() < 64) { return; }

    // ── Parse 64-byte LE header ───────────────────────────────────────────────
    //
    // From Thetis TCIServer.cs:5607-5612 [v2.10.3.13]:
    //   int receiver   = BitConverter.ToInt32(payload, 0);
    //   int sampleRate = BitConverter.ToInt32(payload, 4);
    //   TCISampleType sampleType = (TCISampleType)BitConverter.ToUInt32(payload, 8);
    //   int length     = BitConverter.ToInt32(payload, 20);
    //   TCIStreamType streamType = (TCIStreamType)BitConverter.ToUInt32(payload, 24);
    //   int headerChannels = BitConverter.ToInt32(payload, 28);
    auto readI32 = [&](int off) -> qint32 {
        const auto* p = reinterpret_cast<const uchar*>(data.constData() + off);
        return static_cast<qint32>(
            static_cast<quint32>(p[0]) |
            (static_cast<quint32>(p[1]) << 8) |
            (static_cast<quint32>(p[2]) << 16) |
            (static_cast<quint32>(p[3]) << 24));
    };

    // const int receiver     = readI32(0);   // future: multi-RX routing
    const int sampleRate    = readI32(4);
    const int sampleTypeInt = readI32(8);
    const int length        = readI32(20);  // flat count of encoded values
    const int streamTypeInt = readI32(24);
    const int headerChannels = readI32(28);

    // From Thetis TCIServer.cs:5614-5615 [v2.10.3.13]:
    //   if (streamType != TCIStreamType.TX_AUDIO_STREAM || length <= 0) return;
    if (streamTypeInt != static_cast<int>(TciStreamType::TxAudioStream)) { return; }
    if (length <= 0) { return; }

    // Fix wave minor: the header's sample rate becomes the TX channel's
    // resampler input rate (TxChannel::feedTciAudioBlock), whose output
    // buffer is sized from it; a rate of 1, or of millions, made it
    // allocate per frame without bound or build an unusable resampler.
    // Same bounds as the audio_samplerate: interceptor (8x headroom). A
    // rate of 0 or less is passed on as Thetis does: no resampling
    // (cmaster.cs:1446-1447 [v2.10.3.15], inputRate <= 0 returns the
    // input). Not in Thetis, which passes any positive rate on.
    constexpr int kMinTxAudioSampleRate = 8000;
    constexpr int kMaxTxAudioSampleRate = 384000;
    if (sampleRate > 0
        && (sampleRate < kMinTxAudioSampleRate || sampleRate > kMaxTxAudioSampleRate)) {
        session->txFramesDropped++;
        if (!session->txRateRejectionLogged) {
            session->txRateRejectionLogged = true;
            qCWarning(lcTci) << "TciServer: TX audio dropped, sample rate" << sampleRate
                             << "is outside" << kMinTxAudioSampleRate << "to"
                             << kMaxTxAudioSampleRate << ", peer" << session->peer;
        }
        return;
    }

    // ── TX mutex gate ─────────────────────────────────────────────────────────
    //
    // Only the active TX client may push audio. All others silently dropped.
    // Mirrors Thetis per-client m_tciPttActive gate (TCIServer.cs:5547 [v2.10.3.13]).
    if (m_txAudioActiveClient.isNull() || m_txAudioActiveClient.data() != ws) {
        session->txFramesDropped++;
        return;
    }

    // ── bytesPerSample + payload bounds check ─────────────────────────────────
    //
    // From Thetis TCIServer.cs:5617-5621 [v2.10.3.13]:
    //   int bytesPerSample = getBytesPerSample(sampleType);
    //   int dataOffset = 64;
    //   int actualDataBytes = payload.Length - dataOffset;
    //   if (actualDataBytes < bytesPerSample) return;
    const int bps = TciBinaryFrame::bytesPerSample(sampleTypeInt);
    const int dataOffset = 64;
    const int actualDataBytes = data.size() - dataOffset;
    if (actualDataBytes < bps) { return; }

    const int actualValueCount = actualDataBytes / bps;

    // ── Modern vs legacy header detection ─────────────────────────────────────
    //
    // From Thetis TCIServer.cs:5628-5652 [v2.10.3.13]:
    //   bool modernHeader = (headerChannels == 1 || headerChannels == 2);
    //   if (modernHeader) {
    //       channels = headerChannels;
    //       decodedValueCount = Math.Min(length, actualValueCount);
    //       if (channels > 1) decodedValueCount -= decodedValueCount % channels;
    //   } else {
    //       // legacy/JTDX: no real channels field
    //       if (actualValueCount >= length * 2) channels = 2; else channels = 1;
    //       decodedValueCount = Math.Min(length, actualValueCount);
    //       if (channels > 1) decodedValueCount -= decodedValueCount % channels;
    //   }
    const bool modernHeader = (headerChannels == 1 || headerChannels == 2);

    int channels;
    int decodedValueCount;

    if (modernHeader) {
        channels = headerChannels;
        decodedValueCount = std::min(length, actualValueCount);
        if (channels > 1) {
            decodedValueCount -= decodedValueCount % channels;
        }
    } else {
            // legacy/JTDX
        // Fix wave minor: length * 2 in 64 bits; a header length above
        // INT_MAX / 2 overflowed the int product (undefined behaviour) and
        // could read a mono block as stereo.
        channels = (static_cast<qint64>(actualValueCount)
                    >= static_cast<qint64>(length) * 2) ? 2 : 1;
        decodedValueCount = std::min(length, actualValueCount);
        if (channels > 1) {
            decodedValueCount -= decodedValueCount % channels;
        }
    }

    if (decodedValueCount <= 0) { return; }

    // ── Decode samples ────────────────────────────────────────────────────────
    //
    // From Thetis TCIServer.cs:5657 [v2.10.3.13]:
    //   float[] decoded = decodeSamples(payload, dataOffset, decodedValueCount, sampleType);
    std::vector<float> decoded = TciBinaryFrame::decodeSamples(
        data, dataOffset, decodedValueCount, sampleTypeInt);

    // ── NaN/Inf zero + clamp [-4.0, 4.0] ─────────────────────────────────────
    //
    // From Thetis TCIServer.cs:5658-5673 [v2.10.3.13]:
    //   for (int i = 0; i < decoded.Length; i++) {
    //       float sample = decoded[i];
    //       if (float.IsNaN(sample) || float.IsInfinity(sample)) decoded[i] = 0.0f;
    //       else if (sample > 4.0f)  decoded[i] = 4.0f;
    //       else if (sample < -4.0f) decoded[i] = -4.0f;
    //   }
    // Note: clamp range is [-4.0, 4.0] — Thetis permits TX-side overdrive.
    for (float& s : decoded) {
        if (std::isnan(s) || std::isinf(s)) {
            s = 0.0f;
        } else if (s > 4.0f) {
            s = 4.0f;
        } else if (s < -4.0f) {
            s = -4.0f;
        }
    }

    // ── Stereo to mono by the TX channel ──────────────────────────────────────
    //
    // From Thetis cmaster.cs:1401-1427 [v2.10.3.15] (queueTCITxAudio, fed
    // tciServer.TXStereoInputMode at cmaster.cs:1315): a mono block passes
    // as it is; a stereo one becomes mono by the TX channel setting (Left,
    // Right, or Both averaged) before it reaches the transmitter.
    if (channels > 1) {
        const int stereoFrames = decodedValueCount / channels;
        foldTxStereoToMono(decoded.data(), stereoFrames, m_txStereoInputMode);
        decoded.resize(static_cast<size_t>(stereoFrames));
        decodedValueCount = stereoFrames;
        channels = 1;
    }

    // ── Push to TX audio ring ─────────────────────────────────────────────────
    //
    // Thetis enqueues a TCIQueuedTxAudio (with bounded drop-oldest) at
    // TCIServer.cs:5687-5702 [v2.10.3.13].  NereusSDR pushes raw decoded
    // float bytes into a server-wide SPSC ring.  Drop behaviour: tryPushCopy
    // drops the newest bytes on overflow (partial write) — the ring's natural
    // behaviour matches Thetis's oldest-drop semantics for practical purposes
    // (both prevent unbounded growth; TCI latency is <20ms so overflow is rare).
    //
    // `decodedValueCount` is the flat interleaved count (L,R,L,R... for stereo
    // or L,L,L... for mono).  The ring stores raw float bytes; TxChannel's
    // feedTxAudioFromTci drains them per block.
    const int frames = (channels > 1) ? (decodedValueCount / channels) : decodedValueCount;
    // iPhone app plan Task 36 (R-IOS-13): through a remote window, the app's
    // transmit audio goes to the Core on the window's microphone line.
    if (m_remoteTransmit.key && m_remoteTransmit.audio) {
        if (frames > 0) {
            m_remoteTransmit.audio(decoded.data(), frames, channels, sampleRate);
        }
        return;
    }
    if (frames > 0) {
        m_txAudioRing.tryPushCopy(
            reinterpret_cast<const uint8_t*>(decoded.data()),
            static_cast<qint64>(decodedValueCount) * static_cast<qint64>(sizeof(float)));
    }

    // ── Dispatch to TxChannel via cross-thread queued invoke ─────────────────
    //
    // Phase 3J-1 review P1.1: TxChannel lives on TxWorkerThread; calling
    // feedTxAudioFromTci directly from this (main-thread) handler would race
    // the worker pump.  We use QMetaObject::invokeMethod with
    // Qt::QueuedConnection so the slot fires on TxChannel's owning thread.
    //
    // The decoded samples are packed into a QByteArray (value type) before the
    // invoke so the argument is safely deep-copied into the queued event — a
    // raw float* would be dangling by the time the event is processed.
    //
    // When m_model is null (unit tests with no TxChannel) we skip the invoke
    // and leave the bytes in m_txAudioRing so peekTxRingSize() assertions work.
    if (m_model && frames > 0) {
        WdspEngine* wdsp = m_model->wdspEngine();
        if (wdsp && wdsp->isInitialized()) {
            // TX channel uses WdspEngine::kTxChannelId (== WDSP.id(kind=1=TX,
            // instance=0)) per Thetis dsp.cs:926-944 [v2.10.3.15].  Created by
            // RadioModel::connectToRadio with that same constant.  Calling
            // txChannel(0) returns nullptr because ID 0 is an RX slice channel
            // (the map is keyed by raw WDSP channel ID, not by TX-instance
            // index).
            TxChannel* txCh = wdsp->txChannel(WdspEngine::kTxChannelId);
            if (txCh) {
                const QByteArray payloadCopy(
                    reinterpret_cast<const char*>(decoded.data()),
                    static_cast<qsizetype>(decoded.size()) * static_cast<qsizetype>(sizeof(float)));

                QMetaObject::invokeMethod(txCh, "feedTxAudioFromTci",
                                          Qt::QueuedConnection,
                                          Q_ARG(QByteArray, payloadCopy),
                                          Q_ARG(int, frames),
                                          Q_ARG(int, channels),
                                          Q_ARG(int, sampleRate));

                // Drop the bytes we staged in the ring — the queued invoke will
                // drain via driveOneTxBlock on the worker thread.
                m_txAudioRing.dropOldest(
                    static_cast<size_t>(decodedValueCount) * sizeof(float));
            }
        }
    }
    // Note: m_txAudioRing holds the data for test-only peekTxRingSize() calls
    // when m_model is null (unit test scenario without a real TxChannel).
}

// ── TX channel (Thetis TCITxStereoInputMode) ──────────────────────────────────

TciServer::TxStereoInputMode TciServer::txStereoInputModeFromText(const QString& text)
{
    // Thetis setup.cs:35513-35527 [v2.10.3.15] (TCITXInputChannel): the
    // combo's Left, Right or Both.
    if (text == QLatin1String("Left")) { return TxStereoInputMode::Left; }
    if (text == QLatin1String("Right")) { return TxStereoInputMode::Right; }
    return TxStereoInputMode::Both;
}

void TciServer::foldTxStereoToMono(float* samples, int frames, TxStereoInputMode mode)
{
    // From Thetis cmaster.cs:1408-1426 [v2.10.3.15]:
    //   for (int i = 0; i < complexSamples; i++)
    //   {
    //       double left = queuedAudio.Samples[2 * i];
    //       double right = queuedAudio.Samples[2 * i + 1];
    //       switch (stereoInputMode)
    //       {
    //           case TCITxStereoInputMode.Left:  mono[i] = (float)left; break;
    //           case TCITxStereoInputMode.Right: mono[i] = (float)right; break;
    //           case TCITxStereoInputMode.Both:
    //           default: mono[i] = (float)((left + right) * 0.5); break;
    //       }
    //   }
    // In place: sample i is written after frames 2i and 2i+1 are read.
    for (int i = 0; i < frames; ++i) {
        const double left = samples[2 * i];
        const double right = samples[2 * i + 1];
        switch (mode) {
        case TxStereoInputMode::Left:
            samples[i] = static_cast<float>(left);
            break;
        case TxStereoInputMode::Right:
            samples[i] = static_cast<float>(right);
            break;
        case TxStereoInputMode::Both:
        default:
            samples[i] = static_cast<float>((left + right) * 0.5);
            break;
        }
    }
}

// ── setPingIntervalMs() ──────────────────────────────────────────────────────
//
// From Thetis TCIServer.cs:2650 [v2.10.3.13] — Thetis hardcodes 20000ms
// (1000 * 20); we expose a setter for testability.
// If the ping timer is already running, apply the new interval immediately
// so that test-driven calls to setPingIntervalMs(200) take effect without
// requiring a stop/start cycle.

void TciServer::setPingIntervalMs(int ms)
{
    m_pingIntervalMs = ms;
    if (m_pingTimer->isActive()) {
        m_pingTimer->setInterval(ms);
    }
}

// ── injectAudioFrameForTest() ─────────────────────────────────────────────────
//
// Phase 16 Task 16.4: test-only hook.  Delegates to the private
// onAudioFrameReady slot so integration tests can feed synthetic audio into
// the per-slice ring buffer without needing a real RxChannel / WdspEngine.
//
// This wrapper exists because onAudioFrameReady is private (signal-connected
// internally via Qt::DirectConnection).  Production code never calls this
// method; the only caller is tst_tci_audio_roundtrip.

void TciServer::injectAudioFrameForTest(int slice, const float* L, const float* R,
                                         int n, int srcRate)
{
    onAudioFrameReady(slice, L, R, n, srcRate);
}

// ── TX_CHRONO frame senders ──────────────────────────────────────────────────
//
// Phase 3J-1 bench fix (2026-05-10): WSJT-X (and JTDX, FlDigi-TCI, etc.) only
// stream TX_AUDIO_STREAM binary frames in response to TX_CHRONO timing frames
// that the server sends.  Without these, the client engages PTT, the server
// acquires the mutex, the radio keys, but the client never streams any audio
// — exactly the bench symptom we hit and that the user diagnosed.
//
// Ported from AetherSDR src/core/TciServer.cpp (verified working with
// WSJT-X 2.7.x).  AetherSDR comment: "WSJT-X only sends TX audio in response
// to TX_CHRONO (type=3) frames."  Also matches Thetis TCIServer.cs:5530-5533
// [v2.10.3.13] which calls
//   sendBinaryFrame(buildStreamPayload(receiver, sampleRate, sampleType,
//                                       requestLength, TCIStreamType.TX_CHRONO,
//                                       channels, Array.Empty<byte>()));
// (header-only frame; payload is empty).

void TciServer::startTxChrono(QWebSocket* client, int trx)
{
    if (!client || !m_txChronoTimer) {
        return;
    }
    m_txChronoClient = client;
    m_txChronoTrx    = trx;
    m_txChronoClock.start();
    m_txChronoAccumNs = 0;
    m_txChronoTimer->start();
    // Send an immediate frame so the client can begin streaming audio
    // without waiting for the first 21 ms period to elapse.  AetherSDR
    // does the same (src/core/TciServer.cpp:1117).
    sendTxChronoFrame(client);
    qCInfo(lcTci) << "TciServer: TX_CHRONO started for trx" << trx
                  << "client" << static_cast<const void*>(client);
}

// ── Phase 3J-1 closeout Items 11+13 (2026-05-12): TX gain + peak forwarders.
//
// TxChannel may not exist yet at call time (WDSP not initialized, or no
// hardware connection).  The setter no-ops in that case; the getter
// returns 0 so the TciApplet meter sits at the floor.

void TciServer::setTciTxGainLinear(float lin)
{
    // R-R3-42: a remote window has no transmit channel of its own.
    if (!m_model || m_remoteWindow) { return; }
    if (auto* wdsp = m_model->wdspEngine()) {
        if (auto* tx = wdsp->txChannel(WdspEngine::kTxChannelId)) {
            tx->setTciTxGainLinear(lin);
        }
    }
}

float TciServer::tciTxPeakAbs() const
{
    if (!m_model || m_remoteWindow) { return 0.0f; }
    if (auto* wdsp = m_model->wdspEngine()) {
        if (auto* tx = wdsp->txChannel(WdspEngine::kTxChannelId)) {
            return tx->tciTxPeakAbs();
        }
    }
    return 0.0f;
}

void TciServer::stopTxChrono()
{
    if (m_txChronoTimer && m_txChronoTimer->isActive()) {
        m_txChronoTimer->stop();
    }
    m_txChronoClient = nullptr;
    m_txChronoClock.invalidate();
    m_txChronoAccumNs = 0;

    // Phase 3J-1 bench fix (2026-05-10): drain the TCI input ring on
    // cycle stop so the next TX cycle starts with a clean buffer.
    // Without this, leftover audio from the just-ended cycle sits in the
    // ring; the next cycle's worker pull plays that stale tail FIRST,
    // throwing off FT8's strict 15 s timing cadence.  Worker has already
    // stopped pulling (m_tciAudioActive flipped false on mutex release),
    // so this is safe to call from the main thread.
    if (m_model) {
        if (auto* wdsp = m_model->wdspEngine()) {
            if (auto* txCh = wdsp->txChannel(WdspEngine::kTxChannelId)) {
                txCh->clearTciAudio();
            }
        }
    }

    qCInfo(lcTci) << "TciServer: TX_CHRONO stopped";
}

void TciServer::sendTxChronoFrame(QWebSocket* client)
{
    if (!client) { return; }
    // From Thetis TCIServer.cs:5530-5533 [v2.10.3.13] —
    // sendBinaryFrame(buildStreamPayload(receiver, sampleRate, sampleType,
    //   requestLength, TCIStreamType.TX_CHRONO, channels, Array.Empty<byte>())).
    //
    // length = 2048 matches AetherSDR (verified working with WSJT-X) and
    // corresponds to 1024 stereo float pairs == 21.33 ms at 48 kHz.  Both
    // operands are passed verbatim — buildStreamPayload with samples=nullptr
    // produces the 64-byte header-only frame Thetis sends as TX_CHRONO.
    const QByteArray frame = TciBinaryFrame::buildStreamPayload(
        /*receiver=*/m_txChronoTrx,
        /*sampleRate=*/48000,
        /*sampleType=*/3,           // Float32
        /*length=*/2048,
        /*streamType=*/3,           // TX_CHRONO
        /*channels=*/2,
        /*samples=*/nullptr);       // header-only — no payload
    client->sendBinaryMessage(frame);
}

// ── onRawIqDataReceived() ─────────────────────────────────────────────────────
//
// Phase 18 Task 18.1: IQ binary stream tap.
// Connected to RadioModel::rawIqData with Qt::QueuedConnection so this slot
// always fires on the main thread (where m_clients and QWebSocket live).
//
// Porting from Thetis TCIServer.cs:5397-5435 [v2.10.3.13] —
//   wantsIQStream(receiver): AlwaysStreamIQ override OR per-client
//   m_iqStreamEnabled.Contains(receiver).
//   PublishIQSamples: encode + sendBinaryFrame per subscribed client.
//
// IQSwap: from Thetis TCIServer.cs:6111 [v2.10.3.13].  When TciIqSwap is
// True (default), each (I, Q) pair is swapped to (Q, I) before encoding.
// Default True per design doc §10.
//
// Header `length` field: for IQ frames, length = complexSamples * 2 (total
// floats), NOT per-channel.  Bug-for-bug parity with Thetis which passes
// complexSamples * 2 at cs:5434 [v2.10.3.13].
//
// RadioModel's tagged tap provides the stream index. Every local slice bound
// to that stream receives its own TCI receiver number and actual hardware
// rate, without resampling.

void TciServer::onRawIqDataReceived(int streamIndex, const QVector<float>& interleavedIQ)
{
    if (!m_model || streamIndex < 0) { return; }
    const int sampleRate = m_model->streamSampleRateHz(streamIndex);
    if (sampleRate < 48000 || sampleRate > 384000) { return; }
    for (SliceModel* slice : m_model->slices()) {
        if (slice && slice->streamIndex() == streamIndex) {
            const int receiver = m_desktopHostMode
                ? desktopReceiverForSlice(slice->sliceIndex()) : slice->sliceIndex();
            if (receiver >= 0 && receiver < kMaxTciRxSlices) {
                sendIqToSubscribers(receiver, sampleRate, interleavedIQ);
            }
        }
    }
}

int TciServer::publishedIqRate() const
{
    if (m_remoteWindow) {
        return *std::max_element(m_remoteIqRate.begin(), m_remoteIqRate.end());
    }
    int rate = 0;
    if (!m_model) { return rate; }
    for (const SliceModel* slice : m_model->slices()) {
        if (!slice || (m_desktopHostMode
            ? desktopReceiverForSlice(slice->sliceIndex()) < 0 : slice->sliceIndex() > 1)) {
            continue;
        }
        const int stream = slice->streamIndex();
        if (stream < 0 || !m_model->streamActive(stream)) { continue; }
        const int accepted = m_model->streamSampleRateHz(stream);
        if (accepted >= 48000 && accepted <= 384000) {
            rate = std::max(rate, accepted);
        }
    }
    return rate;
}

void TciServer::sendIqToSubscribers(int receiver, int sampleRate,
                                    const QVector<float>& interleavedIQ)
{
    if (m_clients.isEmpty()) { return; }
    if (interleavedIQ.isEmpty() || (interleavedIQ.size() & 1) != 0
        || receiver < 0 || receiver > 1 || sampleRate < 48000
        || sampleRate > 384000) { return; }

    // Read per-call so AppSettings changes take effect immediately.
    auto& settings = AppSettings::instance();

    // IQSwap flag — From Thetis TCIServer.cs:6111 [v2.10.3.13].
    // Default True per design doc §10.
    const bool iqSwap = settings.value(QStringLiteral("TciIqSwap"),
                                        QStringLiteral("True")).toString()
                         == QStringLiteral("True");

    // AlwaysStreamIQ override — From Thetis TCIServer.cs:5401 [v2.10.3.13]:
    //   if (m_server != null && m_server.AlwaysStreamIQ) return true;
    const bool alwaysStream = settings.value(QStringLiteral("TciAlwaysStreamIq"),
                                              QStringLiteral("False")).toString()
                              == QStringLiteral("True");

    // Apply IQSwap in-place on a copy so the original QVector stays unchanged.
    // From Thetis TCIServer.cs:6111 [v2.10.3.13] — swap I/Q sample order.
    QVector<float> outBuf = interleavedIQ;
    if (iqSwap) {
        const int pairs = outBuf.size() / 2;
        for (int i = 0; i < pairs; ++i) {
            std::swap(outBuf[i * 2], outBuf[i * 2 + 1]);
        }
    }

    // complexSamples is the number of (I, Q) pairs.
    // length field for IQ frames = complexSamples * 2 (total floats).
    // From Thetis TCIServer.cs:5434 [v2.10.3.13]:
    //   sendBinaryFrame(buildStreamPayload(receiver, sampleRate,
    //       TCISampleType.FLOAT32, complexSamples * 2, TCIStreamType.IQ_STREAM,
    //       2, encoded));
    // NOTE: audio uses perChSamples * channels (same math but different semantic
    // labelling); IQ uses complexSamples * 2.  Bug-for-bug parity with Thetis.
    const int complexSamples = outBuf.size() / 2;
    const int lengthField    = complexSamples * 2;  // total floats in the IQ frame

    for (auto it = m_clients.cbegin(); it != m_clients.cend(); ++it) {
        QWebSocket* ws     = it.key();
        const auto& session = it.value();

        // wantsIQStream(kReceiver) — From Thetis TCIServer.cs:5397-5404 [v2.10.3.13]:
        //   if (AlwaysStreamIQ) return true;
        //   return m_iqStreamEnabled.Contains(receiver);
        const bool wants = alwaysStream || session->iqStreamEnabled.contains(receiver);
        if (!wants) { continue; }

        // Encode and send.  Always FLOAT32, always 2 channels for IQ.
        // From Thetis TCIServer.cs:5430-5434 [v2.10.3.13] — encodeSamples +
        // buildStreamPayload(receiver, sampleRate, FLOAT32, complexSamples*2,
        //                    IQ_STREAM, 2, encoded).
        const QByteArray frame = TciBinaryFrame::buildStreamPayload(
            receiver,
            sampleRate,
            static_cast<int>(TciSampleType::Float32),
            lengthField,
            static_cast<int>(TciStreamType::IqStream),
            2,             // always 2 channels for IQ (I + Q)
            outBuf.constData());

        ws->sendBinaryMessage(frame);
    }
}

// ── injectRawIqForTest() ──────────────────────────────────────────────────────
//
// Phase 18 Task 18.1: test-only hook.  Delegates directly to
// onRawIqDataReceived so integration tests can feed synthetic IQ into the
// pipeline without needing a real RadioModel / FFTEngine.
//
// This wrapper exists because onRawIqDataReceived is a private slot
// (Qt-connected internally).  Production code never calls this method;
// the only caller is tst_tci_iq_roundtrip.

void TciServer::injectRawIqForTest(const QVector<float>& interleavedIQ)
{
    sendIqToSubscribers(0, 192000, interleavedIQ);
}

void TciServer::injectRawIqForTest(int receiver, int sampleRate,
                                   const QVector<float>& interleavedIQ)
{
    sendIqToSubscribers(receiver, sampleRate, interleavedIQ);
}

// ── activeIqSubscriberCount() ─────────────────────────────────────────────────
//
// Phase 18 Task 18.1: count of sessions currently subscribed to IQ stream
// for the given receiver index.  Counts per-client iqStreamEnabled hits;
// does NOT add 1 for AlwaysStreamIQ (that flag applies globally, not per
// session count).  Exposed for tst_tci_iq_roundtrip assertions.

int TciServer::activeIqSubscriberCount(int receiver) const
{
    int count = 0;
    for (auto it = m_clients.cbegin(); it != m_clients.cend(); ++it) {
        if (it.value()->iqStreamEnabled.contains(receiver)) {
            ++count;
        }
    }
    return count;
}

} // namespace NereusSDR

#endif // HAVE_WEBSOCKETS
