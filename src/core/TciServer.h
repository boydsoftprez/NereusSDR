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
//   2026-09-23 - R3 receiver audio plan, Task 4 (R-R3-42, R-R3-21,
//                R-R3-25) by J.J. Boyd (KG4VCF): remote-window mode
//                (receive audio from the Core's receiver streams, transmit
//                and raw I/Q refused with a plain reason off the wire),
//                per-client read positions and left-channel mono.
//                AI-assisted transformation via Anthropic Claude Code.
//   2026-09-23 - R3 receiver audio fix wave (R-R3-42, R-R3-21) by J.J.
//                Boyd (KG4VCF): stereo resampled per channel; remote
//                rx_sensors from the mirrored meter; a late "cannot send"
//                answer stops the apps. AI-assisted transformation via
//                Anthropic Claude Code.
//   2026-09-24 - R3 Core-owned accessories, Task 3 (R-R3-48, R-R3-25) by
//                J.J. Boyd (KG4VCF): the Core's station TCI server listens
//                on more than one address (the station network and this
//                computer) and refuses transmit with a plain reason until
//                remote transmit. AI-assisted transformation via Anthropic
//                Claude Code.
//   2026-09-24 - Receiver and transmit gaps plan, Task 10 (R-R3-49) by
//                J.J. Boyd (KG4VCF): each app's vfo, dds and tx_frequency
//                updates pass through its own update gap (Thetis
//                udTCIRateLimit, TciUpdateGap). AI-assisted transformation
//                via Anthropic Claude Code.
//   2026-09-25 - R-R3-39 by J.J. Boyd (KG4VCF): receive audio's WDSP
//                resamplers are made, run and destroyed on the model's
//                receive lane, and a resampled block is sent back on this
//                object's thread in the order it was taken. AI-assisted
//                transformation via Anthropic Claude Code.
//   2026-09-25 - iPhone app Task 73 (R-IOS-02, ruling 5.13): the slice write
//                gate, handed to the protocol (setSliceWriteGate).
//                NereusSDR-original. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-25 - iPhone app plan Task 35 (R-IOS-13; the several-devices
//                design, ruling 8.14): a remote window forwards an app's
//                transmit to its Core as tx.key {trigger:"tci"} under the
//                holder rule (setRemoteTransmit); the TX audio lock is taken
//                only after the Core admits the key; trx:N,false releases
//                only this window's own key. NereusSDR-original. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.

//   2026-09-25 - iPhone app plan Task 36 (R-IOS-13): through a remote
//                window the lock holder's transmit audio goes to the Core
//                on the window's microphone line (RemoteTransmit::audio).
//                NereusSDR-original. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-28 - Desktop-host TCI receiver ownership and holder admission.
//                NereusSDR-original, AI-assisted via OpenAI Codex.
//   2026-09-30 - Fix round 2 (Critical 1, RD-C1): a remote window records
//                the app whose trx:N,true it forwarded (m_remoteKeyClient);
//                that app leaving releases the key on the Core, or the
//                Core's answer when it comes. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.

#pragma once
#ifdef HAVE_WEBSOCKETS

#include "core/NereusCoreExport.h"
#include <QElapsedTimer>
#include <QHash>
#include <QHostAddress>
#include <QObject>
#include <QPointer>
#include <QMutex>
#include <QSet>
#include <QVector>
#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "TciClientSession.h"
#include "core/audio/AudioRingSpsc.h"
#include "core/session/media/IReceiverPcmSink.h"
#include "core/session/media/RemoteTciAudioStage.h"

class QWebSocketServer;
class QWebSocket;
class QTimer;

class TestTciRemoteWindow;

namespace NereusSDR {

class DspControlThread;
class RadioModel;
class RxChannel;
class SliceModel;
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
class NEREUS_CORE_EXPORT TciServer : public QObject, public IReceiverPcmSink {
    Q_OBJECT

public:
    explicit TciServer(RadioModel* model, QObject* parent = nullptr);
    ~TciServer() override;

    // ── R-R3-42: TCI in a remote window ─────────────────────────────────────
    //
    // A TciServer built on a Role::Remote RadioModel serves apps on this
    // computer while the receivers live on a Core. It then hooks no
    // RxChannel and no I/Q tap; receive audio for TCI receiver N is the
    // Core's slice N, asked for through RemoteReceiverAudio while at least
    // one app listens to it and released when the last one stops. Transmit
    // (trx) has its own forwarded policy. Raw I/Q is offered only when the
    // connected Core advertises remoteIqVersion 1; older Cores are refused
    // with an operator notice, never text on the TCI wire.
    bool isRemoteWindow() const { return m_remoteWindow; }

    // How a remote window reaches the Core's receiver streams. MainWindow
    // wires request/release to RemoteMediaController (GUI thread only;
    // never called from inside receiverAudioBlock). unavailableReason is
    // the stop reason meaning "this Core cannot send a receiver's audio":
    // an audio_start answered with it is not echoed.
    struct RemoteReceiverAudio {
        std::function<std::shared_ptr<RemoteTciAudioStage>(int sliceId,
                                                           IReceiverPcmSink* sink)> request;
        std::function<void(int sliceId, IReceiverPcmSink* sink)> release;
        QString unavailableReason;
    };
    // Replaces the source; streams held through the old one are released
    // through it first, and receivers apps still listen to are asked for
    // again through the new one.
    void setRemoteReceiverAudio(RemoteReceiverAudio source);
    struct RemoteIqSource {
        std::function<bool()> available;
        std::function<void(int sliceId)> request;
        std::function<void(int sliceId)> release;
    };
    void setRemoteIqSource(RemoteIqSource source);
    void refreshRemoteIqDemand();
    void receiveRemoteIq(int receiver, int sampleRate, const QVector<float>& samples);
    void setRemoteIqRate(int receiver, int sampleRate);
    void remoteIqUnavailable(int receiver, const QString& reason);

    // ── iPhone app plan Task 35: a remote window's TCI transmit ────────────
    //
    // The several-devices design, ruling 8.14: an app's transmit through a
    // remote window is forwarded to the Core as tx.key {trigger:"tci"} and
    // keys only while this window's device holds transmit (the holder rule,
    // decided by the Core). Among this server's apps, which all act as the
    // same device, the gaps plan's Task 4 rule (Thetis's handleTrxMessage)
    // decides whether a second app's trx keys: while this window's key is
    // on, another app's trx:N,true does nothing and is not answered. Where
    // the two differ the holder rule wins.
    //
    // The TX audio lock (m_txAudioActiveClient) is taken only after the
    // Core admits the key. A refused key takes nothing and is answered to
    // the asking app as Task 4 answers a refused trx (trx:N,false, the
    // real state); the Core's reason goes to operatorNotice(), never onto
    // the TCI wire. An app's trx:N,false sends tx.unkey for this window's
    // key only (the Core releases nothing else). When the Core ends the
    // key on its own (RadioModel::transmittingChanged(false) in the
    // window), the lock is released and every app hears trx:0,false.
    //
    // Without a forwarder (an older Core, or a window that does not
    // declare remote transmit) transmit stays refused as before.
    struct RemoteKeyAnswer {
        bool accepted{false};
        /// The accepted key's epoch (tx.unkey names it).
        quint32 epoch{0};
        /// The Core's sentence when refused.
        QString reason;
    };
    struct RemoteTransmit {
        /// Sends tx.key {trigger:"tci"}; `answer` runs once, on this
        /// object's thread, with the Core's verdict.
        std::function<void(std::function<void(const RemoteKeyAnswer&)> answer)> key;
        /// Sends tx.unkey {epoch}.
        std::function<void(quint32 epoch)> unkey;
        /// iPhone app plan Task 36 (R-IOS-13): the app's transmit audio,
        /// sent on the window's microphone line in place of the microphone
        /// (samples interleaved `channels`, at `sampleRate`). Only the app
        /// holding the TX audio lock (taken once the Core accepted its key)
        /// reaches it. Unset, the audio goes where it always went.
        std::function<void(const float* samples, int frames, int channels, int sampleRate)> audio;
    };
    /// Remote window only; an empty forwarder (no key) turns it off.
    void setRemoteTransmit(RemoteTransmit forward);
    bool forwardsRemoteTransmit() const;
    /// This window's key on the Core, or 0.
    quint32 remoteKeyEpoch() const { return m_remoteKeyEpoch; }

    // The plain reasons this server gives the operator (never an app).
    static constexpr const char* kRemoteTransmitRefusedReason =
        "Apps cannot transmit through TCI from a remote window.";
    static constexpr const char* kRemoteIqRefusedReason =
        "This Core does not send raw I/Q to this window. Updating the Core may help.";

    // ── R-R3-48 / R-R3-25: the Core's station TCI server ────────────────────
    //
    // The Core serves TCI on the station network so devices like the RF-Kit
    // RF2K-S follow its radio. Until remote transmit, it transmits for no
    // app: the init burst says receive_only:true and tx_enable false, trx
    // never touches MOX (the asking app hears trx:N,false) and no app gets
    // the transmit audio lock. The reason goes to operatorNotice() and the
    // Core's log, never onto the TCI wire. Receive (vfo:, split_enable:,
    // audio, I/Q, sensors) is unchanged. Off by default.
    void setStationReceiveOnly(bool receiveOnly);
    bool stationReceiveOnly() const { return m_stationReceiveOnly; }
    // iPhone app Task 73 (ruling 5.13): the slices this server's apps may
    // change (TciProtocol::setSliceWriteGate). The Core's own server passes
    // the station device's; a server without one changes every slice.
    void setSliceWriteGate(std::function<bool(int sliceId)> gate);
    // Explicit local hosting mode. Programs operate as the station device;
    // receivers are its owned slices in ascending id order.
    void setDesktopHostMode(bool enabled);
    bool desktopHostMode() const { return m_desktopHostMode; }
    // Follow-up 1b (R-R3-48): while its owner retries a listener that could
    // not start, the per-try listen, start and stop lines go to debug; the
    // owner logs once per state change instead.
    void setQuietListenAttempts(bool quiet) { m_quietListenAttempts = quiet; }
    static constexpr const char* kStationTransmitRefusedReason =
        "Apps cannot transmit through the Core's TCI server until remote transmit is ready.";

    // The most recent reason given through operatorNotice(), or empty
    // after operatorNoticeCleared().
    QString operatorNoticeReason() const { return m_noticeReason; }

    // Test and status hook: whether this server holds a request for the
    // Core's receiver `rx` (at least one app listens to it).
    bool remoteReceiverRequested(int rx) const;
    std::optional<RemoteTciAudioStage::Diagnostics> remoteAudioDiagnostics(int rx) const;
    quint64 remoteAudioSocketBackpressureDrops() const { return m_socketBackpressureDrops; }

    // IReceiverPcmSink (remote window). The receive worker signals arrival
    // here; its own stage holds PCM and performs TCI conversion.
    void receiverAudioBlock(int sliceId, const float* interleavedStereo, int frames) override;
    void receiverAudioStopped(int sliceId, const QString& reason) override;

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
    // R-R3-48: listen on every address in `bindAddresses` at one port (the
    // first address picks it when `port` is 0). All or nothing: when any
    // address cannot be bound, nothing listens and errorOccurred() says why.
    bool start(const QList<QHostAddress>& bindAddresses, quint16 port);
    // The addresses this server listens on (empty while stopped).
    QList<QHostAddress> listenAddresses() const;
    /// Rework part 3 (R-R3-48): while running, also listen on `address`
    /// (the running port), without restarting; false when it cannot (the
    /// server keeps what it has).
    bool addListener(const QHostAddress& address);

    // Stop the server and disconnect all clients.
    void stop();

    bool    isRunning()   const;
    quint16 port()        const;
    int     clientCount() const { return m_clients.size(); }

    // Phase 16 Task 16.3 (sub-commit b): sum of audioResamplers.size() across
    // all connected sessions.  Exposed for lifecycle test assertions.
    int totalResamplerInstances() const;

    // R-R3-39: WDSP resamplers (one per channel) that receive audio holds
    // right now, across every TciServer. Counted where they are made and
    // destroyed, on the receive lane or at once. For tests.
    static int liveRxAudioResamplersForTest();

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

    // Receiver and transmit gaps plan, Task 10 (R-R3-49): the shortest gap
    // in ms between outgoing vfo / if, dds and tx_frequency updates to each
    // app (Thetis udTCIRateLimit, 0..1000, default 100; see TciUpdateGap.h).
    // start() reads it from the TciRateLimitMs setting; this applies a new
    // value to every connected app and to later ones. Clamped to 0..1000.
    void setUpdateGapMs(int ms);
    int updateGapMs() const { return m_updateGapMs; }

    /// Which channel of an app's stereo transmit audio the radio sends
    /// (Setup > TCI Server > TX channel; Thetis TCITxStereoInputMode,
    /// TCIServer.cs:6518 [v2.10.3.15]). Read from TciTxChannel when the
    /// server starts, as Thetis does (StartServer, TCIServer.cs:6688).
    enum class TxStereoInputMode { Left = 0, Right = 1, Both = 2 };
    void setTxStereoInputMode(TxStereoInputMode mode) { m_txStereoInputMode = mode; }
    TxStereoInputMode txStereoInputMode() const { return m_txStereoInputMode; }
    /// TciTxChannel's text ("Left", "Right", "Both"); anything else is Both.
    static TxStereoInputMode txStereoInputModeFromText(const QString& text);
    /// Thetis cmaster.cs:1401-1427 [v2.10.3.15]: `frames` interleaved stereo
    /// frames folded into `frames` mono samples at the front of `samples`.
    static void foldTxStereoToMono(float* samples, int frames, TxStereoInputMode mode);

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
    void injectRawIqForTest(int receiver, int sampleRate,
                            const QVector<float>& interleavedIQ);

    // Phase 18 Task 18.1: count of sessions currently subscribed to IQ
    // stream for the given receiver index.  Exposed for test assertions.
    // Returns the number of connected clients whose iqStreamEnabled set
    // contains `receiver`, plus 1 if TciAlwaysStreamIq is True (simulating
    // the AlwaysStreamIQ override path).
    int activeIqSubscriberCount(int receiver) const;

    // Test-only: the shared TciProtocol whose pending-notification queue every
    // broadcast lands in before the 5 ms drain timer hands it to clients.
    // Exposed so broadcast tests can assert on frame CONTENT (which receiver a
    // frame addresses) rather than on signal-receiver counts, which cannot
    // distinguish "wired to the right slice" from "wired at all".  Same
    // unguarded test-hook convention as injectAudioFrameForTest above.
    TciProtocol* protocolForTest() const { return m_protocol.get(); }

signals:
    // Emitted after the server begins listening.  port is the actual bound port
    // (useful when start() was called with port=0).
    void serverStarted(quint16 port);

    // Emitted after stop() completes and all clients have been disconnected.
    void serverStopped();
    /// A listener was added while running (addListener).
    void listenersChanged();

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

    // R-R3-42: something the operator should know about TCI that no app is
    // told: a refused transmit or raw I/Q request, or a receiver's audio
    // stopping in a remote window. `reason` is either one of this class's
    // plain sentences or the Core's wire reason; show it through
    // OperatorReasonText::forDisplay(). `peer` is the app's "host:port",
    // empty when the notice is about a receiver rather than one app.
    // raiseToast is false for a repeat of the same reason within 30 s, so
    // an app retrying every transmit period does not stack toasts, and for
    // "media-not-ready" (the window says so already; audio returns by
    // itself).
    void operatorNotice(const QString& peer, const QString& reason, bool raiseToast);
    // The same notice when it is about receiver `rx`'s audio stopping (a
    // remote window), so one stop heard by several apps on this computer
    // can be told once (ReceiverStopNotices).
    void receiverStopNotice(int rx, const QString& reason, bool raiseToast);
    // The receiver audio the last notice was about is flowing again.
    void operatorNoticeCleared();

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
    void onRawIqDataReceived(int streamIndex, const QVector<float>& interleavedIQ);
    void sendIqToSubscribers(int receiver, int sampleRate,
                             const QVector<float>& interleavedIQ);
    int publishedIqRate() const;

    // Destroys all RESAMPLEF instances for the given session and clears the map.
    // Called from onClientDisconnected and stop().
    void cleanupResamplers(std::shared_ptr<TciClientSession>& session);
    // R-R3-42 fix wave: a receiver's left and right resamplers, made and
    // destroyed together (both null when either could not be made).
    // R-R3-39: made (and destroyed) by a job on the model's receive lane,
    // at once when the model has no lane (a remote window, or no model).
    std::shared_ptr<TciRxAudioResampler> makeRxAudioResampler(int inRate, int outRate);
    void releaseRxAudioResampler(std::shared_ptr<TciRxAudioResampler> resampler);
    // R-R3-39: the receive lane WDSP resampling runs on, or null.
    DspControlThread* rxAudioLane() const;

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

    // ── Phase 3J-1 closeout (2026-05-22): SliceModel broadcast wireup ────────
    //
    // hookSliceBroadcasts wires each existing SliceModel signal into the
    // TciProtocol broadcast queue, then subscribes to RadioModel::sliceAdded
    // so future slices are wired automatically.  Idempotent via
    // m_broadcastWiredSlices: re-calling on a slice already wired is a no-op.
    //
    // Mirrors Thetis TCIServer.cs:6730-6790 [v2.10.3.15]: TCIServer subscribes
    // to ~40 Console events (FilterChangedHandlers, NRChangedHandlers,
    // VfoALockChangedHandlers, ...) and routes each to OnXxxChanged which
    // calls sendXxx.  NereusSDR per-slice signals route through SliceModel.
    //
    // wireSliceForBroadcast handles the per-slice signal-to-frame mapping.
    // Bench bug fix 2026-05-22: without this, operator-side VFO/mode/filter
    // changes never propagate to connected TCI clients.
    void hookSliceBroadcasts();
    void wireSliceForBroadcast(SliceModel* slice, int rxIndex);

    /// Does this slice currently drive the transmitter?
    ///
    /// Codex review round 6, PR #293. tx_frequency must come from the slice
    /// TxSliceArbiter has bound TX to, not from slice 0. Before 3F they were
    /// always the same slice and the distinction did not exist.
    bool sliceDrivesTx(int sliceId) const;

    // hookGlobalBroadcasts wires the radio-global signals that aren't tied to
    // a specific slice: MOX, TUN, AF volume, MON enable/volume, IQ sample
    // rate, connection state (Power on/off).  Mirrors the radio-global
    // ChangedHandlers from Thetis TCIServer.cs:6727-6788 [v2.10.3.15] that
    // hookSliceBroadcasts doesn't cover.  Called from constructor and start()
    // (after stop() severs all RadioModel -> this connections).  Idempotency
    // via m_globalBroadcastsWired -- re-calling is a no-op except after stop()
    // which resets the flag.
    void hookGlobalBroadcasts();

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
    friend class ::TestTciRemoteWindow;
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
    // R-R3-48: the other addresses' servers, same port, same clients table.
    QList<QWebSocketServer*> m_extraServers;
    bool m_stationReceiveOnly{false};
    bool m_desktopHostMode{false};
    QPointer<QWebSocket> m_desktopKeyClient;
    bool m_desktopKeyHeld{false};
    quint64 m_desktopKeyGeneration{0};
    quint64 m_desktopLatestOnIntent{0};
    QMetaObject::Connection m_desktopOwnershipConnection;
    std::function<bool(int)> m_externalSliceWriteGate;
    void refreshSliceWriteGate();
    int desktopSliceForReceiver(int receiver) const;
    // Sends rx_enable:1 and tx_enable:1 when TciProtocol::rx2EnabledNow()
    // flips (Thetis OnRX2EnabledChanged -> RX2EnabledChange,
    // TCIServer.cs:7451-7462 and 842-847 [v2.10.3.15]).
    void refreshRx2Enabled();
    bool m_rx2EnabledSent{false};
    QMetaObject::Connection m_rx2OwnershipConnection;
    int desktopReceiverForSlice(int sliceId) const;
    void releaseDesktopProgramKey();
    void refreshLocalAudioReceiverMap();
    bool m_quietListenAttempts{false};
    QHash<QWebSocket*, std::shared_ptr<TciClientSession>> m_clients;
    // Only admitted remote audio subscribers appear here (at most eight).
    QHash<quint64, QPointer<QWebSocket>> m_remoteAudioSockets;

    QTimer* m_pingTimer{nullptr};

    // Phase 14: shared drain timer; fires every 5ms and pumps queued frames
    // from each client's TciSendQueue in priority order (Urgent > Binary >
    // Control), capped at kDrainMaxPerTick frames per client per tick.
    // Mirrors Thetis's per-client sender thread + AutoResetEvent (WaitOne 20ms)
    // at TCIServer.cs:1754-1795 [v2.10.3.13]; NereusSDR uses a single shared
    // timer on the event loop instead of per-client threads.
    QTimer* m_drainTimer{nullptr};

    // Task 10 (R-R3-49): hand the protocol's pending notifications to every
    // app, each through its own update gap (TciClientSession::updateGap).
    // Called from the drain tick and after each handled command.
    void broadcastPendingNotifications();

    // Task 10 (R-R3-49): the update gap every app gets, and the clock its
    // gates read (milliseconds since this server was built).
    int m_updateGapMs{TciUpdateGap::kDefaultGapMs};
    // From Thetis TCIServer.cs:6518 [v2.10.3.15]: default Both.
    TxStereoInputMode m_txStereoInputMode{TxStereoInputMode::Both};
    QElapsedTimer m_gapClock;

    // From design doc §1 — TciServer owns one TciProtocol; it is the shared
    // dispatch engine across all clients (single-instance, transport-blind).
    std::unique_ptr<TciProtocol> m_protocol;

    // From Thetis TCIServer.cs:2650 [v2.10.3.13] — 1000 * 20 = 20000ms.
    // Thetis comment: "per websock spec ping frames are every 20 seconds."
    int m_pingIntervalMs{20000};

    // Phase 16 Task 16.3 (sub-commit c): local per-slice RX audio ring buffers.
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
    // WdspEngine::kMaxSliceChannels is five. The DSP callback reads this
    // atomic map without touching main-thread SliceOwnership state.
    static constexpr int kMaxPhysicalSlices = 5;
    std::array<std::atomic<int>, kMaxPhysicalSlices> m_localAudioReceiverForSlice{};
    // The DSP callback uses tryLock so an ownership change can discard old
    // receiver audio without ever waiting on the realtime thread.
    QMutex m_localAudioMapMutex;
    std::array<int, kMaxPhysicalSlices> m_appliedLocalAudioReceiverForSlice{};
    std::array<quint64, kMaxTciRxSlices> m_localAudioGeneration{};
    std::array<AudioRingSpsc<131072>, kMaxTciRxSlices> m_audioRing;

    // R-R3-42: each 5 ms drain tick empties m_audioRing[rx] into this
    // main-thread history (interleaved stereo, kRxHistoryFrames frames,
    // allocated once), and every subscribed client reads it from its own
    // TciClientSession::audioReadFrame. Before this, clients popped the
    // shared ring directly, so two apps on one receiver each got about
    // half the blocks. Sized like the ring (131072 bytes = 16384 stereo
    // frames, ~341 ms at 48 kHz); a client that falls further behind
    // skips to the oldest audio still held.
    static constexpr int kRxHistoryFrames = 16384;
    std::array<std::vector<float>, kMaxTciRxSlices> m_rxHistory;
    std::array<quint64, kMaxTciRxSlices> m_rxFramesWritten{};

    // Moves everything the producers pushed into m_rxHistory.
    void collectRxAudio();
    // One block of receiver `rx` for one client, or nothing when the
    // client has not got a whole block waiting yet.
    // R-R3-39: a block that needs resampling (and any block while an
    // earlier one of this client is still on the lane) is resampled and
    // encoded on the receive lane and sent back here, in order.
    void sendRxAudioBlock(QWebSocket* ws, const std::shared_ptr<TciClientSession>& session,
                          int rx);

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
            if (m_remoteWindow) { refreshRemoteAudioGain(rx); }
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
    // txChannel(kTxChannelId).  Safe before WDSP init; setter is a no-op, getter
    // returns 0 if the TX channel isn't up yet.
    void setTciTxGainLinear(float lin);
    float tciTxPeakAbs() const;
private:

    // Scratch buffer for deinterleaving + resampling in the drain loop.
    // Max samples per drain tick = audioStreamSamples (default 2048) * channels (2).
    // We size for the largest legal audioStreamSamples (2048) * 2 channels.
    static constexpr int kMaxDrainSamples = 2048 * 2;
    std::array<float, kMaxDrainSamples> m_drainScratch{};
    // R-R3-42 fix wave: a block of up to 2048 frames per channel is
    // resampled; the resampler's scratch (TciRxAudioResampler) holds up to
    // 8x that (384 kHz).
    static constexpr int kMaxResampleFrames = 2048;

    // Handle for the WdspEngine::initializedChanged connection so we can
    // disconnect it if TciServer is destroyed before WDSP initializes.
    QMetaObject::Connection m_wdspInitConn;

    // Phase 26 review finding #4: track connected RxChannel audio-tap sources
    // so stop() can explicitly sever Qt::DirectConnection slots before any
    // TciServer state is torn down.  A DirectConnection from the DSP thread
    // could race with destruction if the signal fires after m_clients is
    // cleared but before the TciServer stack frame is gone.
    //
    // 2026-05-17 crash fix: was QSet<RxChannel*> — raw pointers.  WdspEngine
    // destroys RxChannels on disconnect-from-radio, but nothing pruned this
    // set, leaving dangling raw pointers.  On the next stop() (typically at
    // app exit) the disconnect loop dereferenced freed memory and SIGSEGV'd.
    // QPointer is Qt's guarded pointer: it auto-nulls when the underlying
    // QObject is destroyed (zero-overhead hook into QObject::~QObject), so
    // the stop() loop can skip dead entries safely.  We still do NOT own
    // these channels — WdspEngine remains the owner.
    //
    // QVector rather than QSet because QPointer has no built-in qHash; the
    // set is always 0 or 1 entry in practice (single RX channel until 3F),
    // so the linear-scan dedupe in hookAudioAndIqTaps is free.
    QVector<QPointer<RxChannel>> m_audioTapSources;

    // Phase 3J-1 closeout (2026-05-22): tracks slices that have had their
    // local-broadcast signals wired by wireSliceForBroadcast.  QPointer
    // auto-nulls when the underlying SliceModel is destroyed, mirroring
    // the safety guarantee on m_audioTapSources.  hookSliceBroadcasts
    // skips slices already present in this set so re-calling on
    // stop()/start() is a no-op (slice signals are also auto-disconnected
    // when the SliceModel goes away, so no manual disconnect required).
    QVector<QPointer<SliceModel>> m_broadcastWiredSlices;

    // Review P2 #5 follow-up (2026-05-22): guard flag for hookGlobalBroadcasts.
    // stop()'s QObject::disconnect(m_model, nullptr, this, nullptr) severs ALL
    // RadioModel -> this connections including the global broadcast subscribers,
    // so this flag is reset there and re-armed by hookGlobalBroadcasts on the
    // next start().  Idempotent: if already true, hookGlobalBroadcasts no-ops.
    bool m_globalBroadcastsWired{false};

    // TNF section 6.4: guard flag for the NotchModel master-enable broadcast
    // wired by hookSliceBroadcasts.  Deliberately NOT reset in stop(), unlike
    // m_globalBroadcastsWired above: stop()'s wholesale
    // QObject::disconnect(m_model, nullptr, this, nullptr) is rooted on
    // RadioModel, and NotchModel is a separate QObject, so that connection
    // survives a stop()/start() cycle intact.  hookSliceBroadcasts runs from
    // the constructor AND from every start(), so without this flag each
    // restart would add another subscriber and every flip would emit
    // duplicate rx_nf_enable frames.
    bool m_notchBroadcastWired{false};

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

    // Fix wave RD-C1 (JJ ruling 1): the app whose trx raised the TCI PTT
    // level (MoxController::isTciPttHeld) on a server that keys locally.
    // Cleared when the level drops. Its disconnect, or this server
    // stopping, releases that key before its TX audio lock.
    QPointer<QWebSocket> m_tciPttClient;
    // Releases the TCI key of `client` (any keying app when null) through
    // MoxController::onTciPtt(false), and a TCI level no app owns (fix
    // round 1). False when a callback destroyed this server meanwhile.
    bool releaseAppTciKey(QWebSocket* client);

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

    // ── R-R3-42: remote window state (GUI thread only) ──────────────────────
    bool m_remoteWindow{false};
    RemoteReceiverAudio m_remoteAudio;
    RemoteIqSource m_remoteIq;
    std::array<bool, kMaxTciRxSlices> m_remoteIqRequested{};
    std::array<int, kMaxTciRxSlices> m_remoteIqRate{};
    void updateRemoteIqDemand(int receiver);
    // A request is held with the Core for this receiver.
    std::array<bool, kMaxTciRxSlices> m_remoteRequested{};
    std::array<std::shared_ptr<RemoteTciAudioStage>, kMaxTciRxSlices> m_remoteStage;
    std::array<std::atomic<bool>, kMaxTciRxSlices> m_remotePcmArrived{};
    quint64 m_nextRemoteAudioToken{0};
    int m_nextRemoteDrainReceiver{0};
    quint64 m_socketBackpressureDrops{0};
    std::array<quint64, kMaxTciRxSlices> m_lastRemoteMailboxEvictions{};
    std::array<quint64, kMaxTciRxSlices> m_lastRemoteHistorySkips{};
    quint64 m_remoteSaturationDrops{0};
    int m_remoteSaturationQuietTicks{0};
    bool m_remoteSaturationNotified{false};
    void publishRemoteAudioConfig(int receiver);
    void refreshRemoteAudioGain(int receiver);
    void drainRemoteAudio(
        const std::function<qint64(QWebSocket*, const QByteArray&)>& send = {});
    // The Core cannot send this receiver's audio (an older Core); set from
    // receiverAudioStopped, cleared when the request is released.
    std::array<bool, kMaxTciRxSlices> m_remoteUnavailable{};
    // The receiver's audio is stopped with a notice showing.
    std::array<bool, kMaxTciRxSlices> m_rxStoppedNotice{};
    // A request for this receiver is being made (the Core may answer it
    // before request() returns); audio_start handles that answer itself.
    std::array<bool, kMaxTciRxSlices> m_remoteRequesting{};
    // The app whose audio_start is being handled, while it is.
    const TciClientSession* m_subscribingSession{nullptr};
    // Task 35: the forwarder, this window's key on the Core (its epoch, 0
    // for none), a key asked for and not yet answered, an app's
    // trx:N,false that arrived while it was, and which asks are current.
    RemoteTransmit m_remoteTransmit;
    quint32 m_remoteKeyEpoch{0};
    bool m_remoteKeyPending{false};
    bool m_remoteReleaseWhilePending{false};
    quint64 m_remoteKeyGeneration{0};
    // Fix round 2 (Critical 1, RD-C1): the app whose trx:N,true this
    // window forwarded, with or without ",tci". Its leaving releases the
    // key, or the Core's answer if the key is still being asked for.
    QPointer<QWebSocket> m_remoteKeyClient;
    // Task 35: an app's trx through a remote window that forwards transmit.
    void handleRemoteTrx(QWebSocket* ws, const QString& peer, int rx, bool wantsMox,
                         bool hasTciArg);
    // Task 35: this window's key ended (released, or ended by the Core):
    // the TX audio lock and TX_CHRONO stop, and every app hears it.
    void endRemoteKey();
    void broadcastRemoteKeyState(bool on);

    QString m_noticeReason;
    bool m_noticeFromReceiverStop{false};
    // When each notice ("rx:reason", rx -1 when not about a receiver) was
    // last toasted, on m_noticeClock.
    QHash<QString, qint64> m_noticeToastAtMs;
    QElapsedTimer m_noticeClock;

    // Asks for, or releases, receiver `rx` to match whether any app
    // listens to it. Remote window only; never from receiverAudioBlock.
    void updateRemoteReceiverDemand(int rx);
    // The Core cannot send receiver `rx`'s audio: every app listening to
    // it is told the stream stopped, and the request is released.
    void stopUnavailableReceiver(int rx);
    // Remote window: the level TCI reports for receiver 0, from the Core's
    // meter reading the window mirrors for slice 0 (-140 dBm without one).
    double remoteReceiverLevelDbm() const;
    // receiverStop: the notice is about a receiver's audio stopping, and
    // goes once that audio flows again. quiet: shown, never toasted.
    // rx: the receiver a receiver stop is about (-1 otherwise). Returns
    // whether the notice asked for a toast.
    bool raiseOperatorNotice(const QString& peer, const QString& reason,
                             bool receiverStop = false, bool quiet = false, int rx = -1);
};

} // namespace NereusSDR

#endif // HAVE_WEBSOCKETS
