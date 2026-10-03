// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/core/RadeChannel.h  (NereusSDR)
// =================================================================
//
// NereusSDR - RadeChannel: host-side wrapper around the RADE (Radio
// Autoencoder) v1 codec for FreeDV digital voice on OpenHPSDR radios.
//
// Ported from two upstreams (hybrid port):
//
//   Structure: AetherSDR src/core/RADEEngine.{h,cpp} [@0cd4559]
//     Class layout (Q_OBJECT subclass, public start/stop/isActive/
//     isSynced lifecycle, signals/slots, std::unique_ptr-owned
//     resampler members, paired TX/RX accumulator QByteArrays,
//     opaque-pointer ownership of the rade/LPCNet/FARGAN handles),
//     dtor placement (out-of-line in the .cpp so forward-declared
//     unique_ptr types resolve), and the Qt6 conventions throughout
//     all follow the AetherSDR client.
//
//   DSP API:   freedv-gui src/pipeline/RADEReceiveStep.{h,cpp},
//              RADETransmitStep.{h,cpp}                 [@77e793a]
//     The call patterns I2/I3 will fill in (rade_open with a model
//     filename, rade_n_features_in_out, rade_nin, rade_rx, rade_tx,
//     LPCNet feature extractor lifecycle, FARGAN vocoder warm-up,
//     embedded rade_text aux channel) all come from the freedv-gui
//     pipeline steps. I1 stubs the slot bodies; I2 plugs in the RX
//     path; I3 plugs in the TX path; I4 plugs in the embedded text
//     channel.
//
// License (upstream):
//   - AetherSDR has no per-file copyright header, so per
//     docs/attribution/HOW-TO-PORT.md rule 6 we cite the project URL
//     and primary author at NereusSDR block level rather than copying
//     a verbatim header that does not exist:
//       Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//         - per https://github.com/ten9876/AetherSDR (GPLv3; see
//           LICENSE and About dialog for the live contributor list)
//   - freedv-gui carries an LGPLv2.1+ root license (`freedv-gui/COPYING`).
//     The specific RADEReceiveStep.{h,cpp} and RADETransmitStep.{h,cpp}
//     files each carry a permissive BSD-2-Clause-style file header
//     (Copyright Mooneer Salem, no per-file project copyright line);
//     the BSD permission block is reproduced verbatim below per the
//     upstream redistribution clause. The four upstream files all
//     carry the same header text; we reproduce it once and stack
//     `--- From <path> ---` markers above so the multi-file scope is
//     unambiguous.
//
// LGPL is upgrade-compatible to GPL-3 (LGPL section 3 conversion
// clause); the BSD-2-Clause file-header carve-out is GPL-compatible
// by its own terms. NereusSDR ships under GPLv3.
//
// --- From freedv-gui/src/pipeline/RADEReceiveStep.h ---
// --- From freedv-gui/src/pipeline/RADEReceiveStep.cpp ---
// --- From freedv-gui/src/pipeline/RADETransmitStep.h ---
// --- From freedv-gui/src/pipeline/RADETransmitStep.cpp ---
//   (all four files carry the identical BSD-2-Clause-style header
//    reproduced verbatim below)
//
//=========================================================================
// Name:            RADEReceiveStep.h
// Purpose:         Describes a demodulation step in the audio pipeline.
//
// Authors:         Mooneer Salem
// License:
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
// - Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// - Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
// OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//=========================================================================
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-30  J.J. Boyd / KG4VCF  Fix wave (RADE EOO): queueEndOfOver
//                 encodes the held speech first and never waits for the
//                 decoder; a busy codec defers the EOO (onSent callback).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  RADE reason: test seam setStartFailsForTest.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-05-11  J.J. Boyd / KG4VCF  Phase 3R Task I1. Initial skeleton
//                 port. Hybrid sourcing: class layout / Q_OBJECT shape
//                 / signal-slot surface / member ownership / dtor
//                 placement from AetherSDR src/core/RADEEngine.{h,cpp}
//                 [@0cd4559]; DSP API surface that I2/I3/I4 will plug
//                 into (rade_open call shape, LPCNet feature extractor,
//                 FARGAN vocoder warm-up, embedded rade_text channel)
//                 from freedv-gui src/pipeline/RADE{Receive,Transmit}
//                 Step.{h,cpp} [@77e793a]. NereusSDR divergences vs
//                 AetherSDR: start() takes a model-path argument (the
//                 OpenHPSDR client must load the user-selected .f32
//                 model file rather than hard-coding "dummy" the way
//                 AetherSDR does); the processIq slot accepts I/Q
//                 samples from the receiver (RADE expects RADE_COMP
//                 baseband, not DAX audio); txEncode accepts mic
//                 samples (16 kHz mono int16) so the WdspEngine TX
//                 pump path can feed directly without a 24kHz->16kHz
//                 step the way AetherSDR's feedTxAudio does;
//                 rxTextDecoded signal carries the embedded text-channel
//                 callsign + grid (I4 wires this up). Skeleton bodies
//                 only at I1; isActive()/isSynced() pin the lifecycle
//                 contract the I2/I3/I4 implementations layer onto.
//                 AI tooling: Anthropic Claude Code.
//   2026-05-11  J.J. Boyd / KG4VCF  Phase 3R Task I2. RX path port
//                 from AetherSDR src/core/RADEEngine.cpp:27-78 (start),
//                 :80-106 (stop), :200-303 (feedRxAudio body) [@0cd4559]
//                 cross-checked against freedv-gui
//                 src/pipeline/RADEReceiveStep.cpp:175-310 [@77e793a].
//                 Added second downsampler m_down24to8Q so I/Q input
//                 from the OpenHPSDR DDC stays complex through the
//                 RADE_COMP assembly (vs AetherSDR's stereo-PCM
//                 downmix to mono + imag=0). Added test seam
//                 radeRxCallCountForTest() + m_radeRxCallCount so
//                 the new test suite can pin when rade_rx() is
//                 invoked relative to the rade_nin() accumulator
//                 threshold. AI tooling: Anthropic Claude Code.
//   2026-05-11  J.J. Boyd / KG4VCF  Phase 3R Task I3. TX path port
//                 from AetherSDR src/core/RADEEngine.cpp:134-198
//                 (feedTxAudio body) [@0cd4559] cross-checked against
//                 freedv-gui src/pipeline/RADETransmitStep.cpp
//                 :216-247 (restartVocoder / reset) [@77e793a].
//                 NereusSDR divergence vs AetherSDR: txEncode accepts
//                 16 kHz mono int16 speech samples directly (the
//                 WdspEngine TX pump already feeds 16 kHz mono per the
//                 plan), so AetherSDR's 24 kHz stereo float -> 16 kHz
//                 mono int16 down-conversion at feedTxAudio:139-152 is
//                 dropped; the input bytes go straight into m_txAccum
//                 as int16. The LPCNet feature extraction, the
//                 12-frame feature accumulation, the rade_tx call,
//                 and the 8 kHz RADE_COMP real-leg -> 24 kHz stereo
//                 float32 upsample all follow AetherSDR line-for-line.
//                 Added test seams radeTxCallCountForTest() and
//                 txFeatureAccumSizeForTest() + m_radeTxCallCount so
//                 the test suite can pin when rade_tx() is invoked
//                 relative to the rade_n_features_in_out() threshold
//                 and verify resetTx() actually flushes feature state.
//                 AI tooling: Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 3): test seam
//                 resetTxCountForTest() so a test sees a window's Reset
//                 vocoder reach the Core's channel. NereusSDR-original.
//                 AI tooling: Anthropic Claude Code.
//   2026-09-28  J.J. Boyd / KG4VCF  RADE end-of-over callsigns. RX: an
//                 EOO frame's data goes to the RadeText channel and a
//                 decoded callsign leaves as rxTextDecoded, as
//                 freedv-backend src/pipeline/RADEReceiveStep.cpp:231-242
//                 [@f02e7e9] hands it to rade_text_rx. TX: queueEndOfOver
//                 sends the operator's callsign in the EOO frame followed
//                 by 200 ms of silence, from RADETransmitStep::
//                 restartVocoder (RADETransmitStep.cpp:248-271) and its
//                 NUM_SAMPLES_SILENCE (:65-69) [@f02e7e9], then the
//                 resampler's latency in zeros so all of it leaves;
//                 nothing more is encoded until resetTx(). AI tooling:
//                 Anthropic Claude Code.
//   2026-09-28  J.J. Boyd / KG4VCF  resetTx also clears the 8 -> 24 kHz
//                 resampler, so no modem audio held from one over starts
//                 the next. NereusSDR-original. AI tooling: Anthropic
//                 Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  RADE threads: each channel decodes on
//                 its own thread (RadeRxWorker, "RadeRx<id>"), fed by the
//                 DSP thread. A mutex guards the codec state that both the
//                 decoder thread and the main thread (TX, start, stop)
//                 touch; the flags read across threads are atomic. The
//                 tick log is per channel and names it. setTxSelected
//                 keeps TX encoding to the TX slice's channel alone;
//                 setRxGated stops decoding while that slice transmits.
//                 NereusSDR-original. AI tooling: Anthropic Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  RADE threads review: txEncode never
//                 waits for a decode (a busy codec holds the block for the
//                 next call); snrChanged and freqOffsetChanged are sent
//                 when a value changes and every 100 blocks otherwise; the
//                 tick log carries the decoder's slot and drop counters.
//                 NereusSDR-original. AI tooling: Anthropic Claude Code.
// =================================================================

#pragma once

#include <QLoggingCategory>
#include <QObject>
#include <QByteArray>
#include <QString>
#include <atomic>
#include <functional>
#include <vector>
#include <memory>
#include <mutex>

// Forward-declare the lcRade logging category so other translation
// units (notably RadioModel.cpp's wireRadeChannel extension at K4)
// can qCInfo/qCWarning into "nereus.rade" without duplicating the
// Q_LOGGING_CATEGORY definition.  The definition lives in
// RadeChannel.cpp; this declaration just publishes the symbol.
Q_DECLARE_LOGGING_CATEGORY(lcRade)

// Forward declarations for the upstream RADE / LPCNet / FARGAN handles.
// We keep them as `struct ...*` and `void*` here per the AetherSDR
// pattern (see RADEEngine.h:8-12 [@0cd4559]) so the freedv-gui /
// opus headers do not bleed into every translation unit that
// includes RadeChannel.h. The .cpp resolves them via the
// third_party/rade headers at instantiation time.
struct rade;
struct LPCNetEncState;

namespace NereusSDR {

// Forward declarations for the NereusSDR-native helpers RadeChannel
// owns by unique_ptr. The full type definitions live in Resampler.h
// and RadeText.h; RadeChannel.cpp includes them so the unique_ptr
// destructors resolve.
class Resampler;
class RadeText;
class RadeRxBridge;
class RadeRxWorker;

// Host-side wrapper for the RADE v1 codec. Lifecycle is:
//
//   1. Construct                            !isActive(), !isSynced()
//   2. start(modelPath)                     loads the .f32 model file,
//                                           opens rade/LPCNet/FARGAN,
//                                           builds the resamplers;
//                                           flips isActive() to true.
//                                           I1 ships path-exists only;
//                                           I2 wires up the real load.
//   3. processIq(iq)        (RX path)       feeds I/Q from the
//                                           receiver. Body lands at I2.
//   4. txEncode(speech)     (TX path)       feeds mic samples to the
//                                           LPCNet encoder + rade_tx.
//                                           Body lands at I3.
//   5. resetTx()                            flushes TX state on MOX
//                                           release. Body lands at I3.
//   6. stop()                               unwinds in reverse; flips
//                                           isActive() back to false.
//
// Signals follow the AetherSDR surface (syncChanged / snrChanged /
// freqOffsetChanged) plus the NereusSDR-specific rxTextDecoded
// signal that I4 will hook up to the embedded rade_text channel.
class RadeChannel : public QObject {
    Q_OBJECT

public:
    explicit RadeChannel(QObject* parent = nullptr);
    ~RadeChannel() override;

    bool start(const QString& modelPath);
    void stop();
    bool isActive() const;
    bool isSynced() const;

    // Hook for sideband selection (RADE_U vs RADE_L).  Stored only at
    // the v0.5.0 fix-up that split the original single RADE entry into
    // upper/lower variants; not yet consumed by the I/Q routing layer.
    // K-bench
    // follow-up will wire the stored value into any future spectral
    // mirroring at the TX modulator stage.  Default is upper.
    bool sidebandUpper() const;

    // Test seam. Returns the number of times rade_rx() has been invoked
    // since start(). Used by tst_rade_channel to verify the RX
    // accumulator pumps the codec only when a full rade_nin()-sized
    // chunk is buffered.
    int radeRxCallCountForTest() const;

    // Test seam. Returns the number of times rade_tx() has been invoked
    // since start(). Used by tst_rade_channel to verify the TX
    // accumulator pumps the codec only once rade_n_features_in_out()
    // features are buffered.
    int radeTxCallCountForTest() const;

    // Test seam. Returns the byte size of the TX feature accumulator.
    // Used by tst_rade_channel to verify resetTx() actually flushes
    // state on MOX release.
    int txFeatureAccumSizeForTest() const;

    // Test seam (R-R3-49, parity Task 3). How many times resetTx() ran.
    int resetTxCountForTest() const { return m_resetTxCountForTest; }

#ifdef NEREUS_BUILD_TESTS
    // Test seam (RADE reason). While set, start() refuses as a codec that
    // will not open does, so a test can drive a slice whose decoder did not
    // start. No production caller sets it.
    void setStartFailsForTest(bool fails) { m_startFailsForTest = fails; }
#endif

    // RADE end-of-over callsigns. Queue the end-of-over frame, carrying
    // `callsign` in FreeDV's format, and 200 ms of silence behind the modem
    // output already sent: emits txModemReady once with both. After it the
    // channel encodes no more speech until resetTx(), as FreeDV stops
    // taking microphone audio once the over is ending. Returns false (and
    // sends nothing) when the channel is not running.
    //
    // Fix wave (RADE EOO): speech recorded before the release (blocks held
    // while the decoder had the codec, and whole LPCNet frames waiting in
    // the accumulator) is encoded and emitted first, as FreeDV encodes the
    // recorded audio left while ending TX before the EOO. The main thread
    // never waits for the decoder: with the codec busy the EOO is sent
    // once the decode releases it (queued to this object's thread), and
    // true is returned at once. `onSent` runs on this object's thread
    // right after the EOO's txModemReady; a dropped over (dropTxAudio,
    // resetTx, stop) cancels a deferred EOO and its onSent.
    bool queueEndOfOver(const QString& callsign, std::function<void()> onSent = {});
    bool endOfOverQueued() const { return m_endOfOverQueued; }

    // The samples queueEndOfOver adds at 8 kHz: rade_n_tx_eoo_out(), the
    // 200 ms of silence, and the zeros that push both out of the 8 -> 24 kHz
    // resampler; 0 when not running.
    int endOfOverSamples8k() const;

    // The callsign channel (test seam and RadioModel's view).
    RadeText* textChannel() const { return m_textChannel.get(); }

    // ── RADE threads (2026-09-30) ───────────────────────────────────────
    //
    // The slice this channel decodes for; named in the tick log and in the
    // decoder thread's name. -1 until set.
    void setChannelId(int id) { m_channelId.store(id, std::memory_order_relaxed); }
    int channelId() const { return m_channelId.load(std::memory_order_relaxed); }

    // Run one receive block through the codec and return the speech (24 kHz
    // stereo float, one frame out for each frame in). Also emits
    // rxSpeechReady with it. Thread-safe: the decoder thread calls it.
    QByteArray decodeRxBlock(const QByteArray& iqSamples);

    // Start this channel's decoder thread, "RadeRx<channelId>" (idempotent),
    // and stop and join it. stopRxWorker waits for a block being decoded.
    void startRxWorker();
    void stopRxWorker();
    bool rxWorkerRunning() const;
    // The rings the DSP thread feeds and reads back; null when the decoder
    // thread is not running.
    std::shared_ptr<RadeRxBridge> rxBridge() const;

    // While gated, received blocks are taken but not decoded (the slice is
    // transmitting). Any thread.
    void setRxGated(bool gated);
    bool rxGated() const { return m_rxGated.load(std::memory_order_acquire); }

    // Only the selected channel encodes microphone audio: RadioModel selects
    // the TX slice's channel and deselects every other. Default selected, so
    // a lone channel behaves as before.
    void setTxSelected(bool selected) { m_txSelected.store(selected, std::memory_order_release); }
    bool txSelected() const { return m_txSelected.load(std::memory_order_acquire); }

#ifdef NEREUS_BUILD_TESTS
    // Until every block handed to the decoder thread has been taken.
    bool waitRxIdleForTest(int timeoutMs);
    // Runs on the decoder thread before each block; a test can block in it.
    void setRxStallHookForTest(std::function<void()> hook);
    // The decoder thread's id, null when it is not running.
    Qt::HANDLE rxThreadIdForTest() const;
    QString rxThreadNameForTest() const;
    int rxTickCountForTest() const { return m_rxTickCount.load(std::memory_order_relaxed); }
    // Runs on the decoder thread inside decodeRxBlock while it holds the
    // codec; a test can block in it to hold the codec busy.
    void setRxDecodeLockedHookForTest(std::function<void()> hook);
    // How many txEncode calls found the codec busy and held their block
    // instead of waiting, and the bytes held now.
    int txCodecBusyCountForTest() const { return m_txCodecBusyCount.load(std::memory_order_relaxed); }
    int txHeldBytesForTest() const { return static_cast<int>(m_txHeld.size()); }
    // How many times snrChanged has been emitted.
    int snrEmitCountForTest() const { return m_snrEmitCount.load(std::memory_order_relaxed); }
#endif

    // RADE threads review: microphone blocks dropped because the codec was
    // busy for longer than kTxHeldMaxBytes of speech. Any thread.
    quint64 txHeldDrops() const { return m_txHeldDrops.load(std::memory_order_relaxed); }

public slots:
    // Sideband selection hook.  Set true for RADE_U (upper) and false
    // for RADE_L (lower).  Stored on the channel; not yet consumed by
    // the I/Q routing layer.  Default state (no caller) is upper.
    void setSideband(bool upper);

    // RX path: feed I/Q from the receiver. RADE expects baseband
    // RADE_COMP at the codec's sample rate; the conversion path
    // lands at I2.
    void processIq(const QByteArray& iqSamples);

    // TX path: feed mic samples (16 kHz mono int16) for LPCNet
    // feature extraction and rade_tx encoding. Conversion + encoder
    // path lands at I3.
    //
    // RADE threads review: never waits for the decoder thread. When the
    // codec is busy decoding, the block is held (in order) and encoded
    // with the next call. Call it on one thread only (the main thread).
    void txEncode(const QByteArray& speechSamples);

    // Flush TX encoder state on MOX release to prevent stale audio
    // from carrying across PTT transitions. Body lands at I3.
    void resetTx();

public:
    // RADE end-of-over callsigns: resetTx's flush (speech and feature
    // accumulators, the 8 -> 24 kHz resampler, the end-of-over flag)
    // without counting as the operator's Reset vocoder. Run at every unkey.
    void dropTxAudio();

signals:
    // Decoded speech, 24 kHz stereo int16, ready for the speaker bus.
    void rxSpeechReady(const QByteArray& pcm);

    // Encoded modem signal, 24 kHz stereo int16, ready for the TX
    // I/Q output stage.
    void txModemReady(const QByteArray& iq);

    // RADE decoder sync indication. Emitted on every state transition;
    // I2's syncFn will drive this.
    void syncChanged(bool synced);

    // Signal-to-noise ratio in dB as reported by the RADE decoder.
    void snrChanged(float snrDb);

    // Carrier-frequency offset in Hz as reported by the RADE decoder
    // (used by the panadapter overlay to draw a sync mark).
    void freqOffsetChanged(float hz);

    // Embedded text-channel decode (callsign + Maidenhead grid). FreeDV's
    // end-of-over frame carries a callsign only, so grid is empty.
    void rxTextDecoded(const QString& callsign, const QString& grid);

private:
    // Fix wave (RADE EOO). Encode m_txHeld then m_txAccum into modem
    // chunks; m_codecMutex held by the caller.
    void encodeSpeechLocked(std::vector<QByteArray>& modemOut);
    // Send a pending end-of-over if the codec is free. Main thread.
    enum class EndOfOverRun { Sent, Busy, Failed, NothingPending };
    EndOfOverRun runPendingEndOfOver();

    // Custom deleter for the opaque FARGANState handle so we can hold it
    // in unique_ptr<void, FarganDeleter> without dragging the opus
    // FARGAN header into the include surface. The operator() resolves
    // to `delete static_cast<FARGANState*>(p)` in RadeChannel.cpp where
    // fargan.h is included. NereusSDR-only refactor of AetherSDR's
    // raw-void* pattern at RADEEngine.h:8-12 [@0cd4559] to comply with
    // the project's "no raw new/delete" rule (CLAUDE.md).
    struct FarganDeleter {
        void operator()(void* p) const noexcept;
    };

    // Upstream RADE / LPCNet / FARGAN handles. Opaque here; the .cpp
    // resolves struct rade / LPCNetEncState / FARGANState* (held as
    // void* per AetherSDR's pattern so the opus FARGAN header does
    // not need to be visible at the include site).
    struct rade*         m_rade{nullptr};
    LPCNetEncState*      m_lpcnetEnc{nullptr};
    std::unique_ptr<void, FarganDeleter> m_fargan;

    // RADE threads: guards the codec handles, resamplers and accumulators,
    // which the decoder thread (RX) and the main thread (TX, start, stop)
    // both use. Never taken on the audio callback or the DSP thread.
    mutable std::mutex   m_codecMutex;

    std::atomic<bool>    m_active{false};
    std::atomic<bool>    m_synced{false};
    bool                 m_farganWarmedUp{false};
    std::atomic<int>     m_channelId{-1};
    std::atomic<int>     m_rxTickCount{0};  // per channel, was a static
    std::atomic<bool>    m_rxGated{false};
    std::atomic<bool>    m_txSelected{true};

    // RADE threads review: microphone blocks that arrived while the codec
    // was busy decoding, in order; only txEncode's thread (and dropTxAudio
    // and stop, on the same thread) touch it. At most kTxHeldMaxBytes.
    QByteArray           m_txHeld;
    std::atomic<int>     m_txCodecBusyCount{0};
    std::atomic<quint64> m_txHeldDrops{0};
    std::atomic<int>     m_snrEmitCount{0};

    // RADE threads review: the last SNR and offset sent, and the block they
    // were sent on; decoder side, under m_codecMutex.
    bool                 m_metricsSent{false};
    float                m_lastSnrSent{0.0f};
    float                m_lastFoffSent{0.0f};
    int                  m_lastMetricsTick{0};
    // Test seam state (setRxDecodeLockedHookForTest). Present in every
    // build so the class has one layout whatever defines a unit sees.
    std::mutex           m_rxLockedHookMutex;
    std::function<void()> m_rxLockedHook;

    // RADE-U / RADE-L sideband flag.  True for upper (RADE_U, default),
    // false for lower (RADE_L).  Set via setSideband() in SliceModel's
    // mode-swap path; not yet consumed by the I/Q routing layer at
    // v0.5.0.  K-bench follow-up will wire this into any future
    // spectral mirroring at the TX modulator stage.
    std::atomic<bool>    m_sidebandUpper{true};

    // Test seam counter: incremented every time rade_rx() runs in
    // processIq(). Cleared on start().
    std::atomic<int>     m_radeRxCallCount{0};

    // Test seam counter: incremented every time rade_tx() runs in
    // txEncode(). Cleared on start() and on resetTx().
    std::atomic<int>     m_radeTxCallCount{0};
    std::atomic<int>     m_resetTxCountForTest{0};  // R-R3-49 parity Task 3
#ifdef NEREUS_BUILD_TESTS
    bool                 m_startFailsForTest{false};  // RADE reason seam
#endif

    // RADE end-of-over callsigns: the EOO frame is queued; txEncode takes
    // no more speech until resetTx().
    std::atomic<bool>    m_endOfOverQueued{false};
    // Fix wave (RADE EOO): an end-of-over waiting for the decoder to
    // release the codec. The flag is read by the decoder thread after it
    // releases m_codecMutex; the callsign and callback are main-thread only.
    std::atomic<bool>    m_endOfOverPending{false};
    QString              m_pendingEndOfOverCallsign;
    std::function<void()> m_pendingEndOfOverSent;

    // Resampler chain. The AetherSDR client owns four resamplers
    // (24kHz<->8kHz for the modem leg and 24kHz<->16kHz for the
    // LPCNet/FARGAN leg). NereusSDR's TX-side input is 16 kHz mono
    // (per txEncode's contract), but the RX-side output is still
    // 24 kHz stereo for the speaker bus, so we keep the full set.
    //
    // NereusSDR divergence: AetherSDR takes stereo DAX audio and
    // averages L+R into a single mono leg via processStereoToMono
    // before feeding RADE_COMP with imag=0. Our processIq receives
    // I/Q from the OpenHPSDR DDC, which is already complex baseband,
    // so we run two parallel downsamplers (m_down24to8 for the I leg,
    // m_down24to8Q for the Q leg) and interleave the outputs back
    // into RADE_COMP pairs. The TX-side modem-up leg and the LPCNet
    // leg remain single-resampler because LPCNet operates on real
    // mono speech and the modem TX output is also real mono.
    std::unique_ptr<Resampler> m_down24to8;
    std::unique_ptr<Resampler> m_down24to8Q;
    std::unique_ptr<Resampler> m_up8to24;
    std::unique_ptr<Resampler> m_down24to16;
    std::unique_ptr<Resampler> m_up16to24;

    // TX accumulation buffers: speech samples queue up until LPCNet
    // has a frame worth, then feature vectors queue up until RADE
    // has 12 frames (NB_TOTAL_FEATURES = 432 floats) for rade_tx.
    // RX accumulation buffers: RADE_COMP samples queue up until
    // rade_nin() worth are available for rade_rx, then decoded
    // features queue up for FARGAN.
    QByteArray m_txAccum;
    QByteArray m_txFeatAccum;
    QByteArray m_rxAccum;
    QByteArray m_rxFeatAccum;
    QByteArray m_rxOutAccum;

    // The end-of-over callsign channel (FreeDV's format). Built in the
    // constructor; its textDecoded leaves as rxTextDecoded(callsign, "").
    std::unique_ptr<RadeText> m_textChannel;

    // RADE threads: the decoder thread. Declared last so it is destroyed
    // first; the destructor also stops it before stop() runs.
    std::unique_ptr<RadeRxWorker> m_rxWorker;
};

}  // namespace NereusSDR
