// no-port-check: NereusSDR-original struct condensing Thetis TCIServer.cs:684-790
// [v2.10.3.13] field semantics into a Qt6-native layout.  The inline cites are
// traceability markers — the struct body is NereusSDR-original C++ code;
// Thetis threading/locking/queue primitives are replaced by Qt6 signal/slot.
// Copyright notice for the upstream field semantics is in the Upstream reference
// block below.

// src/core/TciClientSession.h  (NereusSDR)
// NereusSDR-original — per-client session state for the TCI WebSocket server.
//
// This struct condenses the 49-field TCPIPtciSocketListener class from Thetis
// to ~14 fields that NereusSDR's architecture actually requires.  See the
// divergence note below.
//
// Fields ported from Thetis TCIServer.cs:684-790 [v2.10.3.13] are cited
// inline.  Subsequent phases extend this struct:
//   - Phase 14: outbound send queues (m_outboundUrgentFrames etc.) replace
//               Thetis's per-client sender thread + AutoResetEvent.
//   - Phase 16: RX audio resampler state (one Resampler per DAX channel per
//               client, replacing the m_rxAudioResamplers Dictionary).
//
// Upstream reference: Thetis TCIServer.cs:684-790 [v2.10.3.13]
//   https://github.com/ramdor/Thetis
//   Copyright (C) 2020-2025 Richard Samphire MW0LGE
//
// Modification history (NereusSDR):
//   2026-05-10 — Phase 3J-1 Task 2.1 by J.J. Boyd (KG4VCF);
//                AI-assisted transformation via Anthropic Claude Code.
//   2026-09-23 - R3 receiver audio plan, Task 4 (R-R3-42) by J.J. Boyd
//                (KG4VCF): each client's own read position per receiver,
//                so two apps on one receiver each get all of its audio.
//                AI-assisted transformation via Anthropic Claude Code.
//   2026-09-23 - R3 receiver audio fix wave (R-R3-42) by J.J. Boyd
//                (KG4VCF): a left and a right resampler per receiver.
//                AI-assisted transformation via Anthropic Claude Code.
//   2026-09-24 - Receiver and transmit gaps plan, Task 10 (R-R3-49) by
//                J.J. Boyd (KG4VCF): each client's own update gap for vfo,
//                dds and tx_frequency lines (TciUpdateGap).
//                AI-assisted transformation via Anthropic Claude Code.
//   2026-09-25 - R-R3-39 by J.J. Boyd (KG4VCF): a receiver's resampler
//                pair is a shared TciRxAudioResampler that the receive lane
//                makes, runs and destroys; rxAudioBlocksOnLane keeps a
//                client's blocks in order across the lane.
//                AI-assisted transformation via Anthropic Claude Code.

#pragma once
#ifdef HAVE_WEBSOCKETS

#include <QtCore/QElapsedTimer>
#include <QtCore/QHash>
#include <QtCore/QSet>
#include <QtCore/QString>

#include "TciSendQueue.h"
#include "TciUpdateGap.h"

#include <memory>

class QWebSocket;

namespace NereusSDR {

// R-R3-39: one receiver's left and right WDSP resamplers for one client
// (defined in TciServer.cpp, the only code that touches its members).
struct TciRxAudioResampler;

// ── Architectural divergence: Thetis 49 fields → NereusSDR 14 fields ────────
//
// Thetis TCPIPtciSocketListener (TCIServer.cs:684-790 [v2.10.3.13]) holds:
//   - 4 per-client threads (listener / sender / VFO-drain / one-shot timers)
//   - 3 mutex-style lock objects (m_objStreamLock, m_objOutboundLock,
//     m_objRxAudioLock) + an AutoResetEvent (m_outboundFrameEvent)
//   - 5 outbound queues (urgent / binary / control / coalesced-order /
//     coalesced-frames)
//   - Stopwatch + Timer pairs for VFO throttle, centre throttle, TX freq
//   - Per-channel resampler state Dictionaries
//
// NereusSDR replaces all threading + locking + queue primitives with:
//   - Qt6 QWebSocket signal/slot — the event loop IS the listener thread
//   - Phase 14 TciSendQueue — a lock-free per-client output queue whose
//     drain runs on the TCI event loop, replacing the sender thread +
//     AutoResetEvent + three outbound queues
//   - Phase 15 VFO coalescer (TciVfoCoalescer): Layer 3 dedup of the
//     outgoing vfo / dds / tx_frequency lines, shared by every client
//   - Task 10 (R-R3-49) updateGap below: the per-client m_swVFO /
//     m_tmVFOtimer, m_swCentre / m_tmCentretimer and m_swTXFrequency /
//     m_tmTXFrequency Stopwatch + Timer pairs (Thetis TCIServer.cs:753-758
//     [v2.10.3.15]), ported in TciUpdateGap
//   - Phase 16 per-client Resampler* QHash — a QHash<int, Resampler*> added
//     to this struct at that phase, replacing m_rxAudioResamplers
//
// Fields retained below are the subset that NereusSDR's Phase 2–13 logic
// actively reads or writes.  All other Thetis fields are either not-needed
// (their functionality disappears with the Qt6 architecture) or deferred to
// the phase that uses them.

struct TciClientSession {
    // ── Lifecycle / identity ─────────────────────────────────────────────────
    // From Thetis TCIServer.cs:739 [v2.10.3.13] (m_client / m_disconnected)

    QWebSocket* socket{nullptr};        // owning socket (not owned by this struct)
    QString     peer;                   // "ip:port" — for ClientChainApplet display
    QString     userAgent{QStringLiteral("(unknown)")};  // from WS upgrade-request header (best-effort)
    QElapsedTimer connectedAt;          // for connection-duration display

    // From Thetis TCIServer.cs:742 [v2.10.3.13] — m_disconnected.
    // Redundant with QWebSocket::state() but kept for parity with per-client
    // guards in Phases 11+.
    bool disconnected{false};

    // ── Stream subscriptions ─────────────────────────────────────────────────
    // From Thetis TCIServer.cs:766 [v2.10.3.13] — m_iqStreamEnabled HashSet<int>
    QSet<int> iqStreamEnabled;

    // From Thetis TCIServer.cs:767 [v2.10.3.13] — m_audioStreamEnabled HashSet<int>
    QSet<int> audioStreamEnabled;
    // Remote-window audio identity. A new token is assigned after the last
    // subscription ends; each format change advances the revision.
    quint64 remoteAudioToken{0};
    QHash<int, quint64> remoteAudioRevision;
    QHash<int, quint64> remoteAudioLastSequence;
    QHash<int, quint64> remoteAudioLastGeneration;

    // Phase 16 Task 16.3 (sub-commit b): per-slice WDSP RESAMPLEF instance.
    // Created lazily on audio_start, destroyed on audio_stop + disconnect.
    // Key = rx index (slice).  void* avoids pulling WDSP resample.h into
    // TciClientSession.h; TciServer manages create/destroy via
    // handleAudioSubscribe / handleAudioUnsubscribe / cleanupResamplers.
    //
    // From Thetis TCIServer.cs:789 [v2.10.3.13] — m_rxAudioResamplers
    // Dictionary<int, Resampler> replaced by QHash<int, void*> (opaque ptr
    // to RESAMPLEF struct allocated via create_resampleF / create_resampleFV).
    //
    // R-R3-42 fix wave: one resampler per channel, created and destroyed
    // together. A single one run over interleaved L/R mixed the channels at
    // every rate other than 48 kHz.
    // From Thetis TCIServer.cs:702-708 [v2.10.3.15]: TCIRxAudioResamplerState
    // holds a LeftResampler and a RightResampler per receiver.
    //
    // R-R3-39: local TCI keeps the pair on the receive lane. A remote
    // window instead keeps it in RemoteTciAudioStage::Run on the receiver
    // worker; this map is empty there.
    QHash<int, std::shared_ptr<TciRxAudioResampler>> audioResamplers;

    // R-R3-39: receive audio blocks of this client still on the receive lane
    // (posted, not yet sent). While any is, every block goes through the
    // lane, so a block that needs no resampling never overtakes one that
    // did. Main thread only.
    int rxAudioBlocksOnLane{0};

    // R-R3-42: this client's read position in each subscribed receiver's
    // audio, counted in stereo frames since TciServer started collecting
    // that receiver. Set to "now" on audio_start. Every client reads the
    // same receiver history at its own pace, so two apps on one receiver
    // each receive all of the audio instead of taking turns at one ring.
    QHash<int, quint64> audioReadFrame;

    // ── Audio stream configuration ───────────────────────────────────────────
    // From Thetis TCIServer.cs:779 [v2.10.3.13] — m_audioSampleRate = 48000
    int audioSampleRate{48000};

    // From Thetis TCIServer.cs:780 [v2.10.3.13] — m_audioSampleType = FLOAT32
    // Encoded as int: 0=int16, 3=float32 (matches TCI binary frame header format field).
    int audioSampleType{3};

    // From Thetis TCIServer.cs:781 [v2.10.3.13] — m_audioStreamChannels = 2
    int audioStreamChannels{2};

    // From Thetis TCIServer.cs:782 [v2.10.3.13] — m_audioStreamSamples = 2048 (range 100..2048)
    int audioStreamSamples{2048};

    // From Thetis TCIServer.cs:783 [v2.10.3.13] — m_audioStreamSamplesExplicitlySet
    bool audioStreamSamplesExplicitlySet{false};

    // ── TX audio negotiation ─────────────────────────────────────────────────
    // From Thetis TCIServer.cs:788 [v2.10.3.13] — m_seenModernTxAudioNegotiation
    bool seenModernTxAudioNegotiation{false};

    // ── Outbound priority send queue (Phase 14) ──────────────────────────────
    // Per-client TciSendQueue replaces direct QWebSocket::sendTextMessage
    // calls in TciServer's broadcast/unicast paths. The per-client drain
    // timer in TciServer pumps frames in priority order (Urgent > Binary >
    // Control), capped at 64 frames per tick, matching the Thetis sender
    // thread + AutoResetEvent at TCIServer.cs:1754-1795 [v2.10.3.13].
    // Coalesced-key map (Thetis m_outboundCoalescedFrames) is Phase 15.
    TciSendQueue sendQueue{1024};   // 1024 frames per priority bucket

    // Task 10 (R-R3-49): this client's shortest gap between outgoing vfo /
    // if, dds and tx_frequency updates, from Thetis TCIServer.cs:750-758
    // [v2.10.3.15] (m_nRateLimit plus the three Stopwatch + Timer pairs).
    // TciServer sets the gap at connect and when the operator changes it.
    TciUpdateGap updateGap;

    // ── ClientChainApplet display state (NereusSDR-original) ────────────────
    // No Thetis equivalent — drives the per-client row in the future
    // ClientChainApplet (Phase 13).
    QString lastCommand;
    qint64  lastCommandAt{0};   // QDateTime::currentMSecsSinceEpoch() at last command

    // Backpressure drop counter. Synced from sendQueue.dropCount() by the
    // drain timer so Phase 22 ClientChainApplet can read it without touching
    // the queue directly.
    int     framesDropped{0};

    // Phase 17: inbound TX audio drop counter.
    // Incremented when this client sends a binary TX_AUDIO_STREAM frame but
    // does not hold the TX audio mutex (m_txAudioActiveClient != this).
    // Separate from framesDropped (outbound) to keep semantics clean.
    // Phase 22 ClientChainApplet reads both: "outbound: N" + "TX: M dropped".
    int     txFramesDropped{0};
    // Fix wave minor: a TX_AUDIO_STREAM header with a sample rate the
    // TX resampler cannot take was logged once for this app.
    bool    txRateRejectionLogged{false};

    // ── Phase 19: per-client sensor subscriptions ────────────────────────────
    // From Thetis TCIServer.cs:684-790 [v2.10.3.13] — per-listener
    // m_sensorManager.RxSensorsEnabled / TxSensorsEnabled flags.
    //
    // NereusSDR flattens clsTCISensorManager's per-listener state into two
    // bools + interval fields on the session struct. TciServer intercepts
    // rx_sensors_enable:true[,intervalMs]; / tx_sensors_enable:true|false[,intervalMs];
    // commands to toggle these flags before passing to TciProtocol dispatch.
    //
    // From Thetis handleRxSensorsEnable (TCIServer.cs:4449-4459 [v2.10.3.13]).
    bool rxSensorsEnabled{false};

    // From Thetis handleTxSensorsEnable (TCIServer.cs:4460-4469 [v2.10.3.13]).
    bool txSensorsEnabled{false};

    // From Thetis clsTCISensorManager._rxIntervalMs default 200 ms
    // (TCIServer.cs:491 [v2.10.3.13]).
    int rxSensorIntervalMs{200};

    // From Thetis clsTCISensorManager._txIntervalMs default 200 ms
    // (TCIServer.cs:492 [v2.10.3.13]).
    int txSensorIntervalMs{200};
};

} // namespace NereusSDR

#endif // HAVE_WEBSOCKETS
