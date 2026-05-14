// no-port-check: AetherSDR-derived NereusSDR file.  Transport lifecycle
// (start/stop/onNewConnection/onClientDisconnected) is adapted from
// AetherSDR src/core/TciServer.{h,cpp} [@0cd4559]; NereusSDR diverges in
// bind address, double-start contract, signal set, and client table type.
// Registered in docs/attribution/aethersdr-reconciliation.md.

// src/core/TciServer.h  (NereusSDR)
// NereusSDR-original — TCI WebSocket server.
//
// Transport pattern ported from AetherSDR src/core/TciServer.{h,cpp} [@0cd4559].
// Per-client field set condensed from Thetis TCIServer.cs:684-790 [v2.10.3.13]
// (49 Thetis fields → 14 NereusSDR fields; see TciClientSession.h for the
// detailed divergence rationale).
//
// Key NereusSDR divergences from AetherSDR:
//   - Bind address: QHostAddress::LocalHost (AetherSDR binds to Any).
//     Per design doc Q7 lock-in — TCI is a local-process IPC bus in NereusSDR.
//   - double-start contract: returns false + logs warning (AetherSDR returns
//     true, treating double-start as idempotent).
//   - Signals: finer-grained clientConnected / clientDisconnected carrying
//     QWebSocket* (AetherSDR emits clientCountChanged(int) only).
//   - Client table: QHash<QWebSocket*, shared_ptr<TciClientSession>> instead of
//     QList<ClientState> — O(1) lookup by socket pointer in disconnect handler.
//
// Modification history (NereusSDR):
//   2026-05-10 — Phase 3J-1 Task 2.1 by J.J. Boyd (KG4VCF);
//                AI-assisted transformation via Anthropic Claude Code.

#pragma once
#ifdef HAVE_WEBSOCKETS

#include <QElapsedTimer>
#include <QHash>
#include <QHostAddress>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <array>
#include <memory>

#include "TciClientSession.h"
#include "core/audio/AudioRingSpsc.h"

class QWebSocketServer;
class QWebSocket;
class QTimer;

namespace NereusSDR {

class RadioModel;
class RxChannel;
class TciProtocol;

// TCI WebSocket server — exposes radio state over the ExpertSDR3 TCI protocol.
//
// Lifecycle:
//   start(port)  — bind QWebSocketServer to LocalHost:port; 0 = ephemeral.
//   stop()       — close all client sockets + the server socket.
//   isRunning()  — true between a successful start() and the next stop().
//
// Threading: all methods must be called from the thread that owns this object
// (the main GUI thread in the current NereusSDR architecture).  QWebSocket
// callbacks fire on the same thread via the Qt event loop.
class TciServer : public QObject {
    Q_OBJECT

public:
    explicit TciServer(RadioModel* model, QObject* parent = nullptr);
    ~TciServer() override;

    // Start listening on the given port.  Pass 0 to let the OS assign a port.
    // Returns true if the server started listening; false on failure or if
    // the server is already running (double-start is rejected).
    //
    // Default bind address is loopback (127.0.0.1).  Phase 3J-1 closeout
    // Item 1 (2026-05-12) adds the overload that takes an explicit
    // QHostAddress so the operator can bind to a specific NIC or to all
    // IPv4 interfaces (0.0.0.0).  The Setup → CAT/Network/TCI bind-
    // interface dropdown writes `TciServerBindAddress` to AppSettings;
    // MainWindow reads it and passes the resolved QHostAddress to this
    // overload at start time.
    bool start(quint16 port = 50001);
    bool start(const QHostAddress& bindAddress, quint16 port);

    // Stop the server and disconnect all clients.
    void stop();

    bool    isRunning()   const;
    quint16 port()        const;
    int     clientCount() const { return m_clients.size(); }

    // Phase 16 Task 16.3 (sub-commit b): sum of audioResamplers.size() across
    // all connected sessions.  Exposed for lifecycle test assertions.
    int totalResamplerInstances() const;

    // Phase 17: TX audio mutex status — 0 or 1 active TX clients.
    // Used by Phase 22 ClientChainApplet to render the TX badge.
    // Returns 1 when m_txAudioActiveClient is set and still connected;
    // 0 otherwise.
    int activeTxClientCount() const;

    // Phase 17: peer string of the active TX client, or empty when none.
    // Used by Phase 22 ClientChainApplet for per-client TX badge display.
    QString activeTxClientPeer() const;

    // Phase 22: ClientChainApplet enumerates clients per refresh tick.
    // Returns a snapshot of the client map. Const-correct; lifetimes managed
    // by TciServer (do NOT cache pointers across event-loop boundaries).
    QHash<QWebSocket*, std::shared_ptr<TciClientSession>> clients() const { return m_clients; }

    // Phase 22: returns the raw QWebSocket* of the active TX audio client,
    // or nullptr when no client holds the TX mutex.  Used by ClientChainApplet
    // to match the socket pointer from clients() for TX badge rendering.
    // QPointer::data() is safe here — caller is on the main thread.
    QWebSocket* activeTxAudioClient() const;

    // Test-only: return the number of bytes currently pending in the server-wide
    // TX audio ring.  Used by tst_tci_tx_mutex to verify frames landed without
    // needing a real TxChannel.
    int peekTxRingSize() const { return static_cast<int>(m_txAudioRing.usedBytes()); }

    // Override the ping interval (milliseconds) for testability.
    // Default 20000ms matches Thetis TCIServer.cs:2650 [v2.10.3.13] (1000 * 20).
    // Call before or after start(); if the timer is already running the new
    // interval takes effect immediately.
    void setPingIntervalMs(int ms);

    // Test-only: bypass the RxChannel signal chain and inject audio directly
    // into the per-slice ring buffer.  Used by tst_tci_audio_roundtrip;
    // production code paths go through the Qt::DirectConnection signal at
    // RxChannel::audioFrameReady → TciServer::onAudioFrameReady.
    //
    // Audio thread safety: this method must only be called from the audio
    // thread (or in tests, the main test thread which substitutes for the
    // audio thread).  It delegates to onAudioFrameReady which writes only
    // to m_audioRing[slice] — the lock-free SPSC ring safe for one producer
    // (the caller) and one consumer (the main thread drain timer).
    void injectAudioFrameForTest(int slice, const float* L, const float* R,
                                 int n, int srcRate);

    // Phase 18 Task 18.1: test-only hook — bypass the RadioModel::rawIqData
    // signal chain and inject IQ directly.  Used by tst_tci_iq_roundtrip;
    // production code goes through the Qt::DirectConnection signal at
    // RadioModel::rawIqData → TciServer::onRawIqDataReceived.
    void injectRawIqForTest(const QVector<float>& interleavedIQ);

    // Phase 18 Task 18.1: count of sessions currently subscribed to IQ
    // stream for the given receiver index.  Exposed for test assertions.
    // Returns the number of connected clients whose iqStreamEnabled set
    // contains `receiver`, plus 1 if TciAlwaysStreamIq is True (simulating
    // the AlwaysStreamIQ override path).
    int activeIqSubscriberCount(int receiver) const;

signals:
    // Emitted after the server begins listening.  port is the actual bound port
    // (useful when start() was called with port=0).
    void serverStarted(quint16 port);

    // Emitted after stop() completes and all clients have been disconnected.
    void serverStopped();

    // Emitted when a new TCI client connects.
    void clientConnected(QWebSocket* socket);

    // Emitted when a TCI client disconnects (cleanly or on error).
    void clientDisconnected(QWebSocket* socket);

    // Emitted when the TX audio mutex changes hands.
    // newOwner is null when no client holds the mutex (released or
    // disconnected); non-null when a client acquires it.
    // Phase 23: used by MainWindow::updateTciIndicator() for the
    // "On · N ▸TX" indicator state.
    void txAudioActiveClientChanged(QWebSocket* newOwner);

    // Emitted when the server fails to bind.
    void errorOccurred(const QString& errStr);

    // Phase 3J-1 closeout Item 2 (2026-05-12): per-message firehose for the
    // Setup -> CAT/Network/TCI "Show Log..." viewer.  Emitted from
    // onTextMessageReceived (direction="in") and from each per-client drain
    // sendTextMessage (direction="out").  TX_CHRONO timing frames at
    // ~47/sec are intentionally NOT emitted -- they would flood the log
    // and aren't useful for diagnosing TCI protocol issues.
    //   direction: "in"  -> client -> server
    //              "out" -> server -> client
    //   peer:      "host:port" of the relevant client; empty string for
    //              broadcast emissions where the per-client peer isn't
    //              singular (presently no such case -- always one peer).
    //   text:      the TCI line minus the trailing ';' separator
    //   epochMs:   QDateTime::currentMSecsSinceEpoch() at emit time
    void messageLogged(const QString& direction,
                       const QString& peer,
                       const QString& text,
                       qint64 epochMs);

private slots:
    // From AetherSDR src/core/TciServer.cpp:247-273 [@0cd4559] — accept loop
    void onNewConnection();

    // From AetherSDR src/core/TciServer.cpp:275+ [@0cd4559] — cleanup on disconnect
    void onClientDisconnected();

    // Text-frame handler — Phase 3 Task 3.2 wires TciProtocol dispatch here.
    void onTextMessageReceived(const QString& msg);

    // Binary-frame handler — Phase 17 wires TX audio + IQ inbound here.
    void onBinaryMessageReceived(const QByteArray& data);

    // Phase 16 Task 16.3 (sub-commit c): RX audio tap.
    // Connected to RxChannel::audioFrameReady with Qt::DirectConnection so the
    // slot fires on the audio/DSP thread. The slot interleaves L/R and pushes
    // bytes into m_audioRing[slice] using tryPushCopy (non-blocking; safe for RT).
    //
    // RxChannel::audioFrameReady(int slice, const float* L, const float* R,
    //                             int n, int srcRate)
    void onAudioFrameReady(int slice, const float* L, const float* R,
                           int n, int srcRate);

    // Phase 16 Task 16.3 (sub-commit b): audio subscription + resampler lifecycle.
    // Intercepts audio_start:N; / audio_stop:N; commands in onTextMessageReceived
    // BEFORE handing to TciProtocol (which has no concept of client sessions).
    // From Thetis TCIServer.cs:4406-4440 [v2.10.3.13] — audio_start/stop handlers.
    void handleAudioSubscribe(std::shared_ptr<TciClientSession>& session, int rx);
    void handleAudioUnsubscribe(std::shared_ptr<TciClientSession>& session, int rx);

    // Phase 18 Task 18.1: IQ binary stream tap.
    // Connected to RadioModel::rawIqData with Qt::DirectConnection.
    // Applies IQSwap, then broadcasts IQ frames to subscribed clients.
    // From Thetis TCIServer.cs:5397-5435 [v2.10.3.13] — wantsIQStream +
    // PublishIQSamples.
    void onRawIqDataReceived(const QVector<float>& interleavedIQ);

    // Destroys all RESAMPLEF instances for the given session and clears the map.
    // Called from onClientDisconnected and stop().
    void cleanupResamplers(std::shared_ptr<TciClientSession>& session);

    // Phase 3J-1 review P2.3: connect RX audio tap (RxChannel::audioFrameReady
    // → onAudioFrameReady) and IQ tap (RadioModel::rawIqData →
    // onRawIqDataReceived).  Called from BOTH the constructor AND start() so
    // that a stop() → start() cycle reconnects the taps that stop() severs.
    //
    // If WDSP is not yet initialized at call time, defers the audio tap via
    // WdspEngine::initializedChanged (same lazy-connect path as the constructor
    // originally used).  The IQ tap is always eager (RadioModel is always ready
    // when TciServer is constructed/started).
    //
    // Idempotent: if the taps are already connected (m_audioTapSources non-empty
    // / m_iqTapConnected true) this method is a no-op.
    void hookAudioAndIqTaps();

    // ── Phase 3J-1 bench fix (2026-05-10): TX_CHRONO frame senders ───────────
    //
    // Start/stop the TX_CHRONO timer when a TCI client acquires/releases the
    // TX audio mutex.  See m_txChronoTimer doc-comment for the full
    // narrative on why this is required for WSJT-X compatibility.

    /// Start the TX_CHRONO timer.  Sends an immediate frame so the client
    /// can begin TX audio without waiting for the first 21 ms period.
    void startTxChrono(QWebSocket* client, int trx);

    /// Stop the TX_CHRONO timer and clear m_txChronoClient.  Safe to call
    /// even when the timer isn't running.
    void stopTxChrono();

    /// Emit a single header-only TX_CHRONO frame to `client`.
    /// From Thetis TCIServer.cs:5530-5533 [v2.10.3.13] —
    /// sendBinaryFrame(buildStreamPayload(receiver, sampleRate, sampleType,
    /// requestLength, TCIStreamType.TX_CHRONO, channels, Array.Empty<byte>())).
    void sendTxChronoFrame(QWebSocket* client);

private:
    // Phase 3J-1 closeout Item 9 (2026-05-12): QPointer instead of raw
    // pointer.  TciServer outlives in normal operation, but during
    // MainWindow's child destruction (deleteChildren walk) RadioModel
    // may die before TciServer if it was added as a child first.  When
    // ~TciServer -> stop() then runs QObject::disconnect(m_model, ...)
    // on the dangling raw pointer, the QObject::d pointer dereference
    // segfaults (crash report 2026-05-12 15:29 at TciServer.cpp:579).
    // QPointer auto-nulls on the watched object's destruction, so the
    // existing `if (m_model)` guard now actually does what it looks
    // like it does.  No other access pattern changes -- QPointer has
    // implicit conversion to T* for member access.
    QPointer<RadioModel> m_model;
    QWebSocketServer*  m_server{nullptr};
    QHash<QWebSocket*, std::shared_ptr<TciClientSession>> m_clients;

    QTimer* m_pingTimer{nullptr};

    // Phase 14: shared drain timer; fires every 5ms and pumps queued frames
    // from each client's TciSendQueue in priority order (Urgent > Binary >
    // Control), capped at kDrainMaxPerTick frames per client per tick.
    // Mirrors Thetis's per-client sender thread + AutoResetEvent (WaitOne 20ms)
    // at TCIServer.cs:1754-1795 [v2.10.3.13]; NereusSDR uses a single shared
    // timer on the event loop instead of per-client threads.
    QTimer* m_drainTimer{nullptr};

    // From design doc §1 — TciServer owns one TciProtocol; it is the shared
    // dispatch engine across all clients (single-instance, transport-blind).
    std::unique_ptr<TciProtocol> m_protocol;

    // From Thetis TCIServer.cs:2650 [v2.10.3.13] — 1000 * 20 = 20000ms.
    // Thetis comment: "per websock spec ping frames are every 20 seconds."
    int m_pingIntervalMs{20000};

    // Phase 16 Task 16.3 (sub-commit c): per-slice RX audio ring buffers.
    // The audio thread (DSP worker) pushes interleaved stereo F32 samples via
    // onAudioFrameReady() using tryPushCopy (non-blocking).  The 5ms drain
    // timer on the main thread pops bytes and sends TCI binary frames.
    //
    // Capacity 131072 bytes = ~341ms of 48kHz stereo F32 (48000 * 2ch * 4B = 384kB/s).
    // This gives ~341ms headroom against a 5ms drain period — well above the
    // ratio needed to absorb event-loop latency spikes.
    //
    // kMaxTciRxSlices: we support slice 0 and slice 1 (RX1 + RX2).
    static constexpr int kMaxTciRxSlices = 2;
    std::array<AudioRingSpsc<131072>, kMaxTciRxSlices> m_audioRing;

    // Phase 3J-1 closeout Item 12 (2026-05-12): per-slice RX gain applied
    // to the audio drained from m_audioRing BEFORE the resample + encode.
    // Driven by the TciApplet "Slice A gain" slider (currently slice 0 only;
    // future per-slice sliders would extend to slice 1).  Default 1.0
    // (0 dB, no attenuation).  Independent of the radio's AF Gain (which
    // affects WDSP RXA PanelGain1 / the speaker bus) -- this is a TCI-only
    // trim on the audio going OUT to TCI clients.
    std::array<std::atomic<float>, kMaxTciRxSlices> m_sliceRxGainLinear;

    // Phase 3J-1 closeout Item 13 (2026-05-12): per-slice RX audio peak,
    // updated after gain multiply in the drain loop and read by the
    // TciApplet refresh timer to drive the slice-A level meter.  Replaces
    // the placeholder sine-wave animation.
    std::array<std::atomic<float>, kMaxTciRxSlices> m_sliceRxPeakAbs;
public:
    void setSliceRxGainLinear(int rx, float lin) {
        if (rx >= 0 && rx < kMaxTciRxSlices) {
            m_sliceRxGainLinear[rx].store(lin, std::memory_order_release);
        }
    }
    float sliceRxPeakAbs(int rx) const {
        if (rx >= 0 && rx < kMaxTciRxSlices) {
            return m_sliceRxPeakAbs[rx].load(std::memory_order_acquire);
        }
        return 0.0f;
    }

    // Phase 3J-1 closeout Items 11+13 (2026-05-12): forwarders to TxChannel
    // so TciApplet doesn't have to traverse RadioModel -> WdspEngine ->
    // txChannel(1).  Safe before WDSP init; setter is a no-op, getter
    // returns 0 if the TX channel isn't up yet.
    void setTciTxGainLinear(float lin);
    float tciTxPeakAbs() const;
private:

    // Scratch buffer for deinterleaving + resampling in the drain loop.
    // Max samples per drain tick = audioStreamSamples (default 2048) * channels (2).
    // We size for the largest legal audioStreamSamples (2048) * 2 channels.
    static constexpr int kMaxDrainSamples = 2048 * 2;
    std::array<float, kMaxDrainSamples> m_drainScratch{};

    // Handle for the WdspEngine::initializedChanged connection so we can
    // disconnect it if TciServer is destroyed before WDSP initializes.
    QMetaObject::Connection m_wdspInitConn;

    // Phase 26 review finding #4: track connected RxChannel audio-tap sources
    // so stop() can explicitly sever Qt::DirectConnection slots before any
    // TciServer state is torn down.  A DirectConnection from the DSP thread
    // could race with destruction if the signal fires after m_clients is
    // cleared but before the TciServer stack frame is gone.
    //
    // QSet of raw pointers — we do NOT own these; they are owned by WdspEngine.
    // stop() iterates and calls QObject::disconnect(rxCh, nullptr, this, nullptr).
    // The set is cleared last so the disconnect calls are always valid.
    QSet<RxChannel*> m_audioTapSources;

    // Phase 3J-1 review P2.3: guard flag for the IQ tap connection.
    // hookAudioAndIqTaps() sets this to true after connecting
    // RadioModel::rawIqData → onRawIqDataReceived; stop() resets it when it
    // disconnects the signal.  Prevents double-connect on repeated start() calls.
    bool m_iqTapConnected{false};

    // ── Phase 17: TX audio single-client mutex ───────────────────────────────
    //
    // Only ONE client may be the active TX audio source at a time.
    // Mirrors Thetis TCIServer.cs:7625-7651 [v2.10.3.13] —
    // TryAcquireActiveTxAudioListener / ReleaseActiveTxAudioListener.
    //
    // QPointer<QWebSocket> automatically nulls itself when the socket is
    // destroyed (on disconnect), so stale-pointer reads are safe:
    //   m_txAudioActiveClient.isNull()  →  client disconnected, mutex free.
    //
    // Access is main-thread only (onTextMessageReceived + onBinaryMessageReceived
    // both run on the Qt event loop that owns TciServer).  No additional locking.
    QPointer<QWebSocket> m_txAudioActiveClient;

    // ── Phase 19: sensor broadcast timers ────────────────────────────────────
    //
    // From Thetis TCIServer.cs:2554-2581 [v2.10.3.13] — setRxSensorsEnabled /
    // setTxSensorsEnabled create System.Threading.Timer instances for their
    // respective callbacks.
    //
    // NereusSDR uses QTimers owned by TciServer (main-thread event loop).
    // Default interval 200 ms matches Thetis clsTCISensorManager._rxIntervalMs
    // / _txIntervalMs defaults (TCIServer.cs:491-492 [v2.10.3.13]).
    //
    // RX timer: always-on once start() is called; emits placeholder rx_sensors
    //   frames to subscribed clients (real readings wired in Phase 24+).
    // TX timer: always-on for Phase 19 stub; Phase 24+ gates on MOX state.
    QTimer* m_rxSensorTimer{nullptr};   // 200ms default; broadcasts rx_sensors to subscribed clients
    QTimer* m_txSensorTimer{nullptr};   // 200ms default; MOX-gated (Phase 24+ wires real gate)

    // ── Phase 3J-1 bench fix (2026-05-10): TX_CHRONO timing frames ───────────
    //
    // WSJT-X (and most TCI clients) only stream TX_AUDIO_STREAM binary frames
    // in response to TX_CHRONO (streamType=3) timing frames sent by the
    // server.  Without these, WSJT-X sits silent during a TX cycle even after
    // engaging PTT and acquiring the TX audio mutex — exactly the bench
    // symptom we hit.
    //
    // Ported from AetherSDR src/core/TciServer.cpp [verified working with
    // WSJT-X].  AetherSDR comment: "WSJT-X only sends TX audio in response to
    // TX_CHRONO (type=3) frames."  Also matches Thetis TCIServer.cs:5530-5533
    // [v2.10.3.13] — sendBinaryFrame(buildStreamPayload(..., TX_CHRONO, ...))
    // with Array.Empty<byte>() payload (header-only).
    //
    // Timing: 1024 stereo frames per period at 48 kHz == ~21.33 ms.  A fixed
    // 21 ms QTimer runs ~1.6% fast and warps digital-mode tones over a typical
    // FT8 12.6 s slot, so we poll more frequently (5 ms) and emit frames from
    // a monotonic elapsed-time accumulator — same approach as AetherSDR.
    //
    // m_txChronoTimer is created in start() and runs only while the TX audio
    // mutex is held (started on trx:N,true,tci; / stopped on trx:N,false; or
    // client disconnect).  m_txChronoClient mirrors m_txAudioActiveClient so
    // the timer slot can null-guard without re-reading the mutex pointer
    // (which can change mid-tick on a fast-reconnect).
    QTimer* m_txChronoTimer{nullptr};
    QPointer<QWebSocket> m_txChronoClient;
    QElapsedTimer m_txChronoClock;
    qint64 m_txChronoAccumNs{0};
    int m_txChronoTrx{0};

    // ── Phase 17: server-wide TX audio ring buffer ───────────────────────────
    //
    // Inbound TX binary frames from the active client are decoded and pushed
    // here by onBinaryMessageReceived (main thread).  TxChannel::feedTxAudioFromTci
    // drains this ring per audio-thread tick.
    //
    // Capacity 131072 bytes ≈ 16384 stereo float32 frames ≈ 341 ms at 48 kHz.
    // Matches the RX m_audioRing capacity so the same reasoning applies:
    // well above the drain headroom required against typical event-loop jitter.
    //
    // Phase 17 NereusSDR-original — Thetis keeps per-client m_txAudioQueue
    // (Queue<TCIQueuedTxAudio>) at TCIServer.cs:762-764 [v2.10.3.13].
    // NereusSDR uses a server-wide SPSC ring because only one client can
    // own the TX mutex at a time.
    AudioRingSpsc<131072> m_txAudioRing;
};

} // namespace NereusSDR

#endif // HAVE_WEBSOCKETS
