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

#ifdef HAVE_WEBSOCKETS

#include "TciServer.h"
#include "TciClientSession.h"
#include "TciProtocol.h"
#include "TciSendQueue.h"
#include "TciBinaryFrame.h"
#include "TciSensorManager.h"
#include "LogCategories.h"
#include "models/RadioModel.h"
#include "WdspEngine.h"
#include "wdsp_api.h"   // Phase 3J-1 closeout Item 15 (2026-05-12) — GetTXAMeter direct call
#include "RxChannel.h"
#include "TxChannel.h"
#include "AppSettings.h"  // Phase 18: TciIqSwap + TciAlwaysStreamIq flags

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

#include <QElapsedTimer>
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
{
    // Phase 3J-1 closeout Item 12 (2026-05-12): seed per-slice RX gain atomics
    // to 1.0 (0 dB).  TciApplet pushes the persisted slice-A gain via
    // setSliceRxGainLinear on construction; this default is the fallback
    // before that hookup runs.
    for (int rx = 0; rx < kMaxTciRxSlices; ++rx) {
        m_sliceRxGainLinear[rx].store(1.0f, std::memory_order_release);
        m_sliceRxPeakAbs[rx].store(0.0f, std::memory_order_release);
    }

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
    {
        auto& s = AppSettings::instance();
        bool seeded = false;
        if (!s.contains(QStringLiteral("TciEmulateExpertSDR3Protocol"))) {
            s.setValue(QStringLiteral("TciEmulateExpertSDR3Protocol"), QStringLiteral("True"));
            seeded = true;
        }
        if (!s.contains(QStringLiteral("TciEmulateSunSDR2Pro"))) {
            s.setValue(QStringLiteral("TciEmulateSunSDR2Pro"), QStringLiteral("True"));
            seeded = true;
        }
        if (seeded) {
            qCInfo(lcTci) << "TciServer: seeded TCI compat-flag defaults "
                             "(ExpertSDR3 + SunSDR2PRO emulation) — required for "
                             "WSJT-X TCI-audio mode";
        }
    }

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
        while (m_protocol->hasPendingNotification()) {
            const QString notif = m_protocol->takePendingNotification();
            for (auto sit = m_clients.cbegin(); sit != m_clients.cend(); ++sit) {
                sit.value()->sendQueue.push(TciSendQueue::Priority::Control, notif);
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
        // For each client that has audio subscriptions, read from the per-slice
        // AudioRingSpsc, optionally resample, encode as a TCI binary frame, and
        // send directly via sendBinaryMessage.
        //
        // From Thetis TCIServer.cs:5444-5512 [v2.10.3.13] — the sendRXAudioStream
        // loop reads samples, resamples, encodes, and calls sendBinaryFrame.
        // NereusSDR replicates this per drain-tick rather than in a dedicated thread.
        for (auto cit = m_clients.begin(); cit != m_clients.end(); ++cit) {
            QWebSocket* ws = cit.key();
            auto& session  = cit.value();

            for (int rx : session->audioStreamEnabled) {
                if (rx < 0 || rx >= kMaxTciRxSlices) { continue; }

                // Number of interleaved float samples to pop each tick.
                // audioStreamSamples is per-channel; multiply by channels.
                const int channels = session->audioStreamChannels;  // 1 or 2
                const int perChSamples = session->audioStreamSamples;  // default 2048
                const int totalSamples = perChSamples * channels;
                const int wantBytes = totalSamples * static_cast<int>(sizeof(float));

                if (m_audioRing[rx].usedBytes() < static_cast<size_t>(wantBytes)) {
                    continue;  // not enough data yet; wait for next tick
                }

                // Pop from the ring into the scratch buffer.
                // Scratch is sized for kMaxDrainSamples = 2048*2 floats.
                const int maxScratch = kMaxDrainSamples;
                if (totalSamples > maxScratch) { continue; }  // safety

                const qint64 got = m_audioRing[rx].popInto(
                    reinterpret_cast<uint8_t*>(m_drainScratch.data()),
                    wantBytes);
                if (got < wantBytes) { continue; }

                // Phase 3J-1 closeout Item 12 (2026-05-12): apply per-slice
                // TCI RX gain BEFORE resample (so the gain stays in the 48k
                // domain and the resampler sees clean amplitude).  Item 13:
                // track block peak |sample| for TciApplet's slice level meter
                // -- replaces the fake sine-wave placeholder.
                const float sliceGain =
                    m_sliceRxGainLinear[rx].load(std::memory_order_acquire);
                float blockPeak = 0.0f;
                if (sliceGain != 1.0f) {
                    for (int i = 0; i < totalSamples; ++i) {
                        m_drainScratch[i] *= sliceGain;
                        const float a = std::fabs(m_drainScratch[i]);
                        if (a > blockPeak) { blockPeak = a; }
                    }
                } else {
                    // No gain adjust -- just track peak without mutating samples.
                    for (int i = 0; i < totalSamples; ++i) {
                        const float a = std::fabs(m_drainScratch[i]);
                        if (a > blockPeak) { blockPeak = a; }
                    }
                }
                m_sliceRxPeakAbs[rx].store(blockPeak, std::memory_order_release);

                // Resample if the client requested a rate other than 48000 Hz.
                // Phase 16: xresampleFV resamples in-place using the per-session
                // per-slice RESAMPLEF instance created in handleAudioSubscribe.
                const float* samples = m_drainScratch.data();
                int outSamples = totalSamples;

                auto rIt = session->audioResamplers.find(rx);
                if (rIt != session->audioResamplers.end() &&
                    session->audioSampleRate != 48000) {
                    // Allocate a temporary output buffer on the stack.
                    // Max output = totalSamples * max_ratio (48000/8000 = 6).
                    static constexpr int kMaxOutSamples = kMaxDrainSamples * 8;
                    static thread_local std::array<float, kMaxOutSamples> outBuf{};
                    xresampleFV(m_drainScratch.data(), outBuf.data(),
                                totalSamples, &outSamples, rIt.value());
                    samples = outBuf.data();
                }

                // Encode + send binary frame.
                // From Thetis TCIServer.cs:5510 [v2.10.3.13]:
                //   sendBinaryFrame(buildStreamPayload(receiver, sampleRate,
                //       sampleType, interleaved.Length, RX_AUDIO_STREAM,
                //       channels, encoded));
                const QByteArray frame = TciBinaryFrame::buildStreamPayload(
                    rx,
                    session->audioSampleRate,
                    session->audioSampleType,
                    outSamples,         // flat count (length field in header)
                    static_cast<int>(TciStreamType::RxAudioStream),
                    channels,
                    samples);

                ws->sendBinaryMessage(frame);
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
        double rx1Dbm = -140.0;
        if (m_model) {
            if (auto* wdsp = m_model->wdspEngine()) {
                if (auto* rx = wdsp->rxChannel(0)) {
                    rx1Dbm = rx->getMeter(RxMeterType::SignalAvg);
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
        // pattern at MeterPoller.cpp:321-323).  Channel ID 1 == WDSP.id(1,0)
        // per Thetis dsp.cs:926-944.  Only call when the TX channel exists
        // to avoid a WDSP nullptr deref against an unallocated stage.
        double micDbm = -140.0;
        if (auto* wdsp = m_model->wdspEngine()) {
            if (wdsp->txChannel(1) != nullptr) {
                micDbm = GetTXAMeter(1, static_cast<int>(TxMeterType::MicAvg));
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
// This method is idempotent: if m_audioTapSources is non-empty the audio tap
// is already connected; if m_iqTapConnected is true the IQ tap is already
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

    // ── (a) RX audio tap ────────────────────────────────────────────────────
    //
    // Phase 16 Task 16.3 (sub-commit c): hook the RX audio tap from RxChannel.
    // WdspEngine may not be initialized yet at call time. We connect
    // once it is, then hook audioFrameReady with Qt::DirectConnection so the
    // slot runs on the DSP thread and can push into AudioRingSpsc non-blockingly.
    //
    // Idempotency guard: if m_audioTapSources is already non-empty, the tap is
    // connected — skip to avoid double-signal.
    if (m_audioTapSources.isEmpty()) {
        WdspEngine* wdsp = m_model->wdspEngine();
        if (wdsp) {
            auto hookAudioTap = [this, wdsp]() {
                RxChannel* rxCh = wdsp->rxChannel(0);
                if (rxCh && !m_audioTapSources.contains(rxCh)) {
                    connect(rxCh, &RxChannel::audioFrameReady,
                            this, &TciServer::onAudioFrameReady,
                            Qt::DirectConnection);
                    // Phase 26 review finding #4: track so stop() can
                    // explicitly disconnect before tearing down TciServer state.
                    m_audioTapSources.insert(rxCh);
                    qCInfo(lcTci) << "TciServer: RX audio tap connected to RxChannel 0";
                }
            };

            if (wdsp->isInitialized()) {
                hookAudioTap();
            } else {
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
        connect(m_model, &RadioModel::rawIqData,
                this, &TciServer::onRawIqDataReceived,
                Qt::QueuedConnection);
        m_iqTapConnected = true;
        qCInfo(lcTci) << "TciServer: IQ tap connected to RadioModel::rawIqData";
    }
}

TciServer::~TciServer()
{
    stop();
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

bool TciServer::start(const QHostAddress& bindAddress, quint16 port)
{
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
        qCWarning(lcTci) << "TciServer: failed to listen on"
                         << bindAddress.toString() << "port" << port
                         << m_server->errorString();
        const QString errStr = m_server->errorString();
        delete m_server;
        m_server = nullptr;
        emit errorOccurred(errStr);
        return false;
    }

    connect(m_server, &QWebSocketServer::newConnection,
            this, &TciServer::onNewConnection);

    qCInfo(lcTci) << "TciServer: listening on" << m_server->serverPort();
    emit serverStarted(m_server->serverPort());

    // From Thetis TCIServer.cs:2650-2654 [v2.10.3.13] — 20s server-driven ping
    // with payload "Thetis", per RFC 6455 keepalive semantics.
    // Thetis: "per websock spec ping frames are every 20 seconds. Ideally we
    // should receive something back within 20 seconds, but just use it to cause
    // exception on socket if client has dc'ed without telling us with a
    // disconnect frame."
    // Detects dead clients via Qt's automatic close-on-write-error path.
    m_pingTimer->start(m_pingIntervalMs);

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

    return true;
}

// ── stop() ───────────────────────────────────────────────────────────────────
//
// From AetherSDR src/core/TciServer.cpp:184-207 [@0cd4559] — disconnect-and-
// cleanup loop pattern.  NereusSDR uses QHash iteration instead of QList.

void TciServer::stop()
{
    if (!m_server) { return; }

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
    }
    for (RxChannel* rxCh : std::as_const(m_audioTapSources)) {
        QObject::disconnect(rxCh, nullptr, this, nullptr);
    }
    m_audioTapSources.clear();  // P2.3: reset so hookAudioAndIqTaps() re-arms

    m_pingTimer->stop();
    m_drainTimer->stop();        // Phase 14: stop drain before disconnecting clients
    m_rxSensorTimer->stop();     // Phase 19: stop sensor broadcast timers
    m_txSensorTimer->stop();

    // Phase 17: release TX audio mutex — no client is active after stop().
    m_txAudioActiveClient = nullptr;

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
        ws->close();
        ws->deleteLater();
    }
    m_clients.clear();

    m_server->close();
    delete m_server;
    m_server = nullptr;

    qCInfo(lcTci) << "TciServer: stopped";
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
    while (m_server->hasPendingConnections()) {
        auto* ws = m_server->nextPendingConnection();

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
            for (const QString& line : burst) {
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
    emit clientDisconnected(ws);

    // Phase 17: release TX audio mutex if this client held it.
    // QPointer auto-nulls when the socket is deleted (ws->deleteLater below),
    // but we clear explicitly here so activeTxClientCount() returns 0 in the
    // same event-loop pass as the disconnect.
    if (!m_txAudioActiveClient.isNull() && m_txAudioActiveClient.data() == ws) {
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

    if (!session->audioResamplers.contains(rx)) {
        const int inRate  = 48000;                        // WDSP RX output is always 48 kHz
        const int outRate = session->audioSampleRate;     // negotiated client rate (default 48000)
        // create_resampleFV(in_rate, out_rate) — from resample.c:342-344 [WDSP v1.29]:
        //   return (void *)create_resampleF(1, 0, 0, 0, in_rate, out_rate);
        // size=0 + null buffers are intentional; xresampleFV sets them per-call.
        void* resampler = create_resampleFV(inRate, outRate);
        if (resampler) {
            session->audioResamplers.insert(rx, resampler);
            qCInfo(lcTci) << "TciServer: audio resampler created for rx" << rx
                          << "peer" << session->peer
                          << "in_rate" << inRate << "out_rate" << outRate;
        } else {
            qCWarning(lcTci) << "TciServer: create_resampleFV failed for rx" << rx
                             << "in_rate" << inRate << "out_rate" << outRate;
        }
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

    auto rIt = session->audioResamplers.find(rx);
    if (rIt != session->audioResamplers.end()) {
        // destroy_resampleFV — from resample.c:358-360 [WDSP v1.29]:
        //   destroy_resampleF((RESAMPLEF)ptr);
        destroy_resampleFV(rIt.value());
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
    for (auto rIt = session->audioResamplers.begin();
         rIt != session->audioResamplers.end(); ++rIt) {
        destroy_resampleFV(rIt.value());
    }
    session->audioResamplers.clear();
    session->audioStreamEnabled.clear();
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
// From Thetis RxChannel.cpp:1479-1492 [v2.10.3.13] — the audioFrameReady
// signal fires post-DSP with outI (L) and outQ (R) as scratch float arrays
// of length n at srcRate Hz (always 48000 for WDSP RX output).
//
// Uses tryPushCopy (non-blocking) so the DSP thread never blocks. Overflow
// (ring full) silently drops the oldest portion — audible as a dropout rather
// than a deadlock.
void TciServer::onAudioFrameReady(int slice, const float* L, const float* R,
                                   int n, int srcRate)
{
    (void)srcRate;  // always 48000 per kWdspRxOutputRate in RxChannel.cpp

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
                handleAudioSubscribe(session, rx);
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
            if (ok && rx >= 0 && rx <= 1) {
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
                    for (int rx : session->audioStreamEnabled) {
                        auto rIt = session->audioResamplers.find(rx);
                        if (rIt != session->audioResamplers.end()) {
                            destroy_resampleFV(rIt.value());
                            session->audioResamplers.erase(rIt);
                        }
                        void* newResampler = create_resampleFV(48000, sr);
                        if (newResampler) {
                            session->audioResamplers.insert(rx, newResampler);
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
        {
            const QString kTrx = QStringLiteral("trx:");
            if (trimmed.startsWith(kTrx)) {
                // Parse "trx:N,bool[,tci]"
                const QString args = trimmed.mid(kTrx.size());
                const QStringList parts = args.split(QLatin1Char(','));
                if (parts.size() >= 2) {
                    // Is the third arg "tci"?
                    const bool hasTciArg = (parts.size() >= 3 &&
                        parts.at(2).trimmed().compare(QLatin1String("tci"),
                            Qt::CaseInsensitive) == 0);

                    const bool wantsMox = (parts.at(1).trimmed().compare(
                        QLatin1String("true"), Qt::CaseInsensitive) == 0);

                    if (hasTciArg && wantsMox) {
                        // Client wants TX audio ownership.
                        // From Thetis TCIServer.cs:7625-7643 [v2.10.3.13] —
                        // TryAcquireActiveTxAudioListener: grant if no current
                        // owner or the owner IS this client; else deny.
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
                            // Parse rx from "trx:N,..." first arg.
                            const int trxIdx = parts.at(0).trimmed().toInt();
                            startTxChrono(ws, trxIdx);
                        } else {
                            // Phase 26 review finding #10: m_clients.value(key, make_shared<>())
                            // allocates a default TciClientSession just to read its peer field.
                            // Use explicit find + fallback string instead — zero allocation path.
                            auto heldIt = m_clients.find(m_txAudioActiveClient.data());
                            const QString heldBy = (heldIt != m_clients.end())
                                ? heldIt.value()->peer
                                : QStringLiteral("(unknown)");
                            qCInfo(lcTci) << "TciServer: TX audio mutex denied for"
                                          << session->peer
                                          << "(held by" << heldBy << ")";
                        }
                    } else if (!wantsMox) {
                        // trx:N,false — release mutex if this client held it.
                        // From Thetis TCIServer.cs:7646-7652 [v2.10.3.13] —
                        // ReleaseActiveTxAudioListener: clear if owner matches.
                        if (!m_txAudioActiveClient.isNull() &&
                            m_txAudioActiveClient.data() == ws) {
                            m_txAudioActiveClient = nullptr;
                            qCInfo(lcTci) << "TciServer: TX audio mutex released by"
                                          << session->peer;
                            // Phase 23: notify indicator / MainWindow.
                            emit txAudioActiveClientChanged(nullptr);
                            // Phase 3J-1 bench fix: stop TX_CHRONO frames.
                            stopTxChrono();
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
    const QString response = m_protocol->handleCommand(msg);
    if (!response.isEmpty()) {
        session->sendQueue.push(TciSendQueue::Priority::Control, response);
    }

    // From design doc §1: notifications drain after each handleCommand and
    // broadcast to ALL clients (including the originator), mirroring Thetis's
    // outbound-frame fan-out at TCIServer.cs:1662-1791 [v2.10.3.13].
    // Phase 14: push into each client's queue instead of direct sendTextMessage.
    while (m_protocol->hasPendingNotification()) {
        const QString notif = m_protocol->takePendingNotification();
        for (auto sit = m_clients.cbegin(); sit != m_clients.cend(); ++sit) {
            sit.value()->sendQueue.push(TciSendQueue::Priority::Control, notif);
        }
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
        channels = (actualValueCount >= length * 2) ? 2 : 1;
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
            // TX channel uses WDSP channel ID 1 (== WDSP.id(kind=1=TX, instance=0))
            // per Thetis dsp.cs:926-944 [v2.10.3.13].  Created by
            // RadioModel::connectToRadio at src/models/RadioModel.cpp:1792-1798
            // with channelId=1.  Calling txChannel(0) returns nullptr because
            // ID 0 is the RX channel slot in WdspEngine::m_txChannels (the map
            // is keyed by raw WDSP channel ID, not by TX-instance index).
            TxChannel* txCh = wdsp->txChannel(1);
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
    if (!m_model) { return; }
    if (auto* wdsp = m_model->wdspEngine()) {
        if (auto* tx = wdsp->txChannel(1)) {
            tx->setTciTxGainLinear(lin);
        }
    }
}

float TciServer::tciTxPeakAbs() const
{
    if (!m_model) { return 0.0f; }
    if (auto* wdsp = m_model->wdspEngine()) {
        if (auto* tx = wdsp->txChannel(1)) {
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
            if (auto* txCh = wdsp->txChannel(1)) {
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
// RadioModel emits rawIqData only for RX1 (slice 0) in the current
// single-receiver architecture; Phase 3F multi-pan will add per-receiver
// variants.  This slot therefore treats all incoming data as receiver=0.

void TciServer::onRawIqDataReceived(const QVector<float>& interleavedIQ)
{
    if (m_clients.isEmpty()) { return; }
    if (interleavedIQ.isEmpty()) { return; }

    // Phase 18: RadioModel::rawIqData fires only for RX1 (slice 0).
    // Phase 3F will add per-receiver variants.
    constexpr int kReceiver = 0;  // From design doc Phase 18 §Note

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

    // IQ sample rate: 192000 Hz is the typical HPSDR DDC rate and the default
    // Thetis negotiates via iq_samplerate:.  Phase 3F multi-pan will pass the
    // actual per-receiver rate here.  For now, match the Thetis Phase 11 default
    // of 192000 from TciProtocol.cpp:265 [v2.10.3.13 port].
    constexpr int iqSampleRate = 192000;

    for (auto it = m_clients.cbegin(); it != m_clients.cend(); ++it) {
        QWebSocket* ws     = it.key();
        const auto& session = it.value();

        // wantsIQStream(kReceiver) — From Thetis TCIServer.cs:5397-5404 [v2.10.3.13]:
        //   if (AlwaysStreamIQ) return true;
        //   return m_iqStreamEnabled.Contains(receiver);
        const bool wants = alwaysStream || session->iqStreamEnabled.contains(kReceiver);
        if (!wants) { continue; }

        // Encode and send.  Always FLOAT32, always 2 channels for IQ.
        // From Thetis TCIServer.cs:5430-5434 [v2.10.3.13] — encodeSamples +
        // buildStreamPayload(receiver, sampleRate, FLOAT32, complexSamples*2,
        //                    IQ_STREAM, 2, encoded).
        const QByteArray frame = TciBinaryFrame::buildStreamPayload(
            kReceiver,
            iqSampleRate,
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
    onRawIqDataReceived(interleavedIQ);
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
