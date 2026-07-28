// =================================================================
// src/core/TxWorkerThread.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original file.  QThread that drives the TX DSP pump off
// the main thread, mirroring Thetis's `cm_main` worker-thread loop
// from Project Files/Source/ChannelMaster/cmbuffs.c:151-168
// [v2.10.3.13] one-to-one.
//
// =================================================================
//
// Modification history (NereusSDR):
//   2026-04-29 — Original implementation for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.  Phase 3M-1c TX pump
//                 architecture redesign v2 (QTimer-driven, fexchange2,
//                 256-block).  Rewritten by the same author the same
//                 day for v3 (semaphore-wake, fexchange0, 64-block,
//                 cadence sourced from radio mic frames via
//                 TxMicSource).  Plan:
//                 docs/architecture/phase3m-1c-tx-pump-architecture-plan.md
// =================================================================

// no-port-check: NereusSDR-original file.  The Thetis cmbuffs.c /
// cmaster.c citations identify the architectural pattern this class
// mirrors (worker thread + semaphore-wake + uniform block size); no
// Thetis logic is line-for-line ported here (the CMB primitives
// themselves live in src/core/audio/TxMicSource.{h,cpp}).

#pragma once

#include <QByteArray>
#include <QMutex>
#include <QThread>
#include <QVector>

#include <atomic>
#include <memory>
#include <vector>

#include "core/audio/RadeTxFilters.h"

namespace NereusSDR {

class AudioEngine;
class RadeChannel;
class TxChannel;
class TxMicSource;

// ---------------------------------------------------------------------------
// TxWorkerThread — dedicated QThread for the TX DSP pump.
//
// Mirrors Thetis cm_main at cmbuffs.c:151-168 [v2.10.3.13]:
//
//   void cm_main (void *pargs) {
//       ... promote thread to Pro Audio priority ...
//       while (run) {
//           WaitForSingleObject(Sem_BuffReady, INFINITE);
//           cmdata (id, in[id]);     // drain one r1_outsize-block
//           xcmaster(id);            // run fexchange + surrounds
//       }
//   }
//
// NereusSDR mapping:
//   WaitForSingleObject     <==>  m_micSource->waitForBlock(-1)
//   cmdata                  <==>  m_micSource->drainBlock(m_in.data())
//   xcmaster (TX branch)    <==>  m_txChannel->driveOneTxBlockFromInterleaved
//   pcm->in[stream]          <==>  m_in (interleaved I/Q double, 128 elems)
//
// PC mic override (Thetis cmaster.c:379 — `asioIN(pcm->in[stream])`):
//   When AudioEngine::isPcMicOverrideActive() returns true (the user
//   selected MicSource::Pc AND m_txInputBus is open), the worker
//   overwrites the radio mic samples in m_in with PC mic samples
//   pulled via AudioEngine::pullTxMic.  Partial pulls (< kBlockFrames)
//   leave the remaining slots filled with the radio mic data — a
//   "smooth degradation" rather than a hard zero-fill.
//
// VOX/DEXP gating (Thetis cmaster.c:388 — `xdexp(tx)`) is deferred until
// create_dexp is ported (separate follow-up).  VOX setters in TxChannel
// are guarded with pdexp[ch]==nullptr null-checks so attempts to enable
// VOX from the UI are no-ops rather than crashes.
//
// Lifecycle:
//   1. Construct (parent = RadioModel).
//   2. setMicSource / setTxChannel / setAudioEngine — all required
//      before startPump().  TxChannel must already be moveToThread()'d
//      to this worker.
//   3. startPump() — calls QThread::start().  The new thread enters
//      run(), which loops on the semaphore until isRunning() goes false.
//   4. stopPump() — calls m_micSource->stop() (which posts the poison
//      semaphore release that breaks the worker out of waitForBlock),
//      then QThread::wait()s for the thread to exit.  Idempotent.
//   5. Destruct (after stopPump and after TxChannel is moved back to
//      its original thread by RadioModel).
// ---------------------------------------------------------------------------
class TxWorkerThread : public QThread {
    Q_OBJECT

public:
    // ── Phase 3R Task K2: mode-aware TX path enum ───────────────────────
    //
    // The TxPath enum selects which DSP backend the worker dispatches
    // each block through.  RadioModel updates the path on every MOX-on
    // boundary based on the active slice's DSPMode (USB/LSB/AM/etc.
    // -> Wdsp; RADE -> Rade).
    //
    // K2 ships only the scaffolding (enum + atomic + setter slot +
    // one-shot info-log on entry to the RADE branch).  The full real-
    // time integration (mic feed -> 80 Hz HPF -> 48-16 resampler ->
    // RadeChannel::txEncode -> RadeChannel::txModemReady ->
    // RadioConnection::sendTxIq) lands at K-bench time when ANAN/HL2
    // hardware is available to verify the real-time deadline.
    enum class TxPath {
        Wdsp = 0,   // Existing path: TxChannel + WDSP TXA chain.
        Rade = 1    // NereusSDR-native: bypass TXA, route through
                    // RadeChannel.  Run loop currently logs and skips
                    // the WDSP tick; K-bench wires the mic feed in.
    };

    explicit TxWorkerThread(QObject* parent = nullptr);
    ~TxWorkerThread() override;

    /// Set the components the worker drives.  All three must be non-null
    /// before startPump().
    void setTxChannel(TxChannel* ch);
    void setAudioEngine(AudioEngine* engine);
    void setMicSource(TxMicSource* src);

    /// Phase 3R K-bench: set / clear the RadeChannel the worker emits
    /// RADE mic blocks toward when m_currentTxPath == Rade.  Wired by
    /// RadioModel::wireRadeChannel on mode swap into RADE_U / RADE_L
    /// (and cleared via the channel's destroyed() signal on swap out).
    /// Null clears.  Idempotent.  Storing nullptr while
    /// m_currentTxPath == Rade leaves the RADE branch as a silent
    /// no-op (the worker drops the block); this is the correct
    /// behaviour during the brief window between RadeChannel destroy
    /// and the matching path-flip back to Wdsp.
    void setRadeChannel(RadeChannel* channel);

    /// Start the worker.  Internally calls QThread::start().  Idempotent.
    void startPump();

    /// Stop the worker.  Calls m_micSource->stop() first (so the worker
    /// returns from waitForBlock with isRunning()==false), then waits
    /// for the thread to exit.  Idempotent.
    void stopPump();

    /// Block size in mono frames per pump tick.  Mirrors Thetis
    /// getbuffsize(48000) at cmsetup.c:106-110 [v2.10.3.13].
    static constexpr int kBlockFrames = 64;

#ifdef NEREUS_BUILD_TESTS
    /// Test seam — drive one pump tick synchronously without standing up
    /// the QThread + semaphore wait infrastructure.  Drains one block
    /// from m_micSource (must have been pre-loaded via inbound + a
    /// successful waitForBlock by the caller), applies PC mic override
    /// if active, dispatches fexchange0.
    void tickForTest();

    /// Phase 3R Task K2 test seam — observe the current path without
    /// taking on a QSignalSpy or wiring a real MoxController.  The
    /// production code reads the atomic via acquire-load inside the
    /// run-loop body; tests use this accessor to assert the swap
    /// after setCurrentTxPath().
    TxPath currentTxPathForTest() const;

    /// Phase 3R K-bench test seam — observe the active RADE channel
    /// pointer without exposing the production member.  Tests verify
    /// setRadeChannel round-trip + null-clear via this accessor.
    RadeChannel* radeChannelForTest() const;
#endif

signals:
    /// Phase 3R K-bench: RADE TX mic block — emitted by the RADE
    /// branch of dispatchOneBlock when the worker has processed one
    /// pump tick's worth of mic samples through the HPF + 48->16
    /// resampler.  Carries an int16 mono QByteArray ready for
    /// RadeChannel::txEncode (16 kHz, the LPCNet feature extractor's
    /// native rate).  Empty payload during r8brain warm-up is normal;
    /// downstream slot is a no-op on empty input.
    ///
    /// RadioModel wires this signal to RadeChannel::txEncode via
    /// Qt::QueuedConnection because RadeChannel lives on the main
    /// thread (RadioModel's affinity) while this worker lives on a
    /// dedicated QThread.  The queued delivery serialises across the
    /// thread boundary without holding any audio-thread lock.
    void radeMicBlockReady(const QByteArray& speech16k);

    /// Phase 3F Sub-Epic J Task 9: internal anti-VOX handoff.  Carries an
    /// OWNED copy of one mixed anti-VOX reference block from
    /// onAntiVoxBlockReady (which runs on the DSP thread) to
    /// onAntiVoxSamplesReady (which runs on this object's own thread).
    ///
    /// Self-connected Qt::QueuedConnection in the constructor.  This is the
    /// thread boundary, and it is crossed by value on purpose: the raw
    /// pointer AudioEngine hands out must never travel across it.  A signal
    /// rather than QMetaObject::invokeMethod for the same lifetime reason
    /// recorded at RxDspWorker.h's radeIqReady: ~QObject
    /// auto-disconnects and drops in-flight slot calls under the connection
    /// lock, so a teardown racing the DSP thread cannot deliver into a
    /// half-destroyed worker.
    ///
    /// Not for external use; nothing outside this class connects to it.
    void antiVoxReferenceCopied(const QVector<float>& interleaved,
                                int sampleCount);

public slots:
    // ── Phase 3R Task K2: mode-aware TX path setter ─────────────────────
    //
    // Cross-thread queued slot.  RadioModel posts a Wdsp/Rade swap on
    // every MOX-on transition based on the active slice's DSPMode.
    // The atomic is read with acquire-load at the top of dispatchOneBlock
    // (the run-loop body) so the path takes effect on the next tick.
    //
    // Idempotent: setting to the current value is a cheap no-op store.
    void setCurrentTxPath(TxPath path);

    // ── Phase 3R K-bench (source-first reframe): RADE mic substitute ────
    //
    // RadioModel's txModemReady lambda calls this after extracting the
    // real component of RadeChannel's modem output and upsampling
    // 24 -> 48 kHz mono float. The worker copies the bytes under a
    // small mutex (RADE TX is ~24 kHz upstream so contention is
    // minimal) and drains them into m_in's I channel from dispatch-
    // OneBlock's RADE branch. Empty payload is treated as silence.
    //
    // Cross-thread queued slot (RadeChannel lives on the main thread,
    // worker thread differs). Invoked via QMetaObject::invokeMethod
    // with Qt::QueuedConnection from the wireRadeChannel lambda.
    void setRadeAudioBlock(const QByteArray& audio48k);

    // ── Phase 3R K-bench: RADE pre-encoder mic processing config ────────
    //
    // Per K1 design + user bench feedback: mic input must be gain-staged
    // AND leveled BEFORE reaching the RADE encoder. Earlier scaffolding
    // fed raw mic at peak ~0.02 (-34 dBFS) into RADE, well below the
    // ~-9 dBFS target the codec expects.
    //
    // Config is pushed from RadioModel as TransmitModel properties
    // change (micGainDb, txLevelerOn / MaxGain / Decay). Atomic stores
    // so the worker thread reads the latest values lock-free at each
    // dispatchOneBlock tick.
    void setRadeMicGainDb(int dB);
    void setRadeLeveler(bool on, int maxGainDb, int decayMs);

    // ── Anti-VOX slots (3M-3a-iv) ────────────────────────────────────────
    //
    // These slots form the worker-thread proxy for anti-VOX state and
    // sample-feed.  The state setters are wired in RadioModel via
    // QueuedConnection so emissions from the main thread (MoxController)
    // deliver onto the worker's event queue and apply between waitForBlock
    // cycles (cf. the run() narrative on cross-thread queued setter
    // delivery).
    //
    // The sample feed is the exception and is wired DirectConnection: see
    // onAntiVoxBlockReady, which owns the pointer contract.
    //
    // Architecture: TxChannel itself lives on this worker thread once
    // RadioModel::connectToRadio() runs moveToThread().  These wrappers
    // exist on TxWorkerThread (a QThread) rather than TxChannel because
    // (a) TxChannel methods are not declared as Qt slots, and (b) the
    // m_antiVoxRun atomic gate is a worker-local optimisation that skips
    // the float→double conversion in TxChannel::sendAntiVoxData when the
    // user has anti-VOX off.

    // setAntiVoxRun: forwards run flag to TxChannel::setAntiVoxRun AND
    // mirrors it into m_antiVoxRun (release-store) so onAntiVoxSamplesReady
    // can short-circuit via acquire-load when anti-VOX is disabled.
    //
    // From Thetis cmaster.cs:208-209 [v2.10.3.13] — SetAntiVOXRun.
    void setAntiVoxRun(bool run);

    // onAntiVoxBlockReady: the anti-VOX reference block, straight off
    // AudioEngine's mixer drain.
    //
    // **Qt::DirectConnection ONLY.**  `samples` points into thread_local
    // scratch inside AudioEngine::rxBlockReady that the next audio period
    // overwrites, so it is valid for the duration of this synchronous call
    // and no longer.  This method therefore runs on the DSP thread, takes
    // ownership of the audio while the pointer is still live, and hands the
    // COPY to onAntiVoxSamplesReady through antiVoxReferenceCopied.  The
    // raw pointer never crosses a thread boundary.
    //
    // Do not re-point this at a queued connection to "fix" anything.  Qt
    // cannot marshal `const float*` (it is not a registered metatype), so a
    // queued connect fails loudly at connect time and drops the call rather
    // than dangling, but the correct shape is Direct plus the copy taken
    // here, and that is what RadioModel wires.
    //
    // Gates on m_antiVoxRun (acquire) BEFORE the copy, so anti-VOX off (the
    // default) costs the audio path one atomic load per period: no
    // allocation, no posted event.  onAntiVoxSamplesReady re-checks the
    // same gate for the benefit of direct callers.
    //
    // From Thetis cmaster.c:371-372 [v2.10.3.15]: every sub-receiver's
    // audio goes to the transmitter's anti-VOX mixer, whose SendAntiVOXData
    // callback (create_aamix argument at cmaster.c:171 [v2.10.3.15])
    // delivers one mixed block.  AudioEngine::m_antiVoxMix is that mixer;
    // this is its callback.
    void onAntiVoxBlockReady(const float* samples, int frames);

    // onAntiVoxSamplesReady: pump one owned anti-VOX reference block into
    // TxChannel::sendAntiVoxData when anti-VOX is enabled.  Gates on
    // m_antiVoxRun (acquire) so the float→double conversion in TxChannel is
    // skipped when the user has anti-VOX off.
    //
    // Runs on this object's own thread (main), which is why the block
    // arrives here by value: see antiVoxReferenceCopied above.
    //
    // Phase 3F Sub-Epic J Task 9 dropped the leading sliceId argument.  The
    // buffer used to be slice A's audio and the slot gated on `sliceId != 0`
    // to keep the single-RX feed honest; it is now the sum of every audible
    // slice (Thetis cmaster.c:371-372 [v2.10.3.15]) and has no per-slice
    // identity left to gate on.
    void onAntiVoxSamplesReady(const QVector<float>& interleaved, int sampleCount);

    // setAntiVoxBlockGeometry: queued slot for RxDspWorker::bufferSizesChanged.
    // Calls both TxChannel::setAntiVoxSize and TxChannel::setAntiVoxRate so
    // DEXP's antivox_size and antivox_rate stay aligned with the RX-side
    // post-decimation block.
    //
    // From Thetis cmaster.c:154-155 [v2.10.3.13] — DEXP create-time uses
    // pcm->audio_outsize / pcm->audio_outrate (the post-decimation audio
    // output dimensions) for anti-VOX detector geometry.
    void setAntiVoxBlockGeometry(int outSize, double outRate);

    // setAntiVoxDetectorTau: queued slot for MoxController::antiVoxDetectorTauRequested.
    // Pass-through to TxChannel::setAntiVoxDetectorTau (which calls WDSP
    // SetAntiVOXDetectorTau).  Tau here is in seconds; ms→s conversion
    // already done by MoxController.
    //
    // From Thetis setup.cs:18992-18996 [v2.10.3.13] —
    //   private void udAntiVoxTau_ValueChanged(...)
    //   { cmaster.SetAntiVOXDetectorTau(0, (double)udAntiVoxTau.Value / 1000.0); }
    void setAntiVoxDetectorTau(double seconds);

protected:
    /// QThread entry point.  Runs the cm_main-equivalent loop:
    ///   while (m_micSource->isRunning())
    ///     waitForBlock; drainBlock; PcMicOverride?; fexchange0;
    void run() override;

private:
    /// Body of one pump tick.  Used by both run() and tickForTest().
    /// Assumes the caller has already drained the block into m_in
    /// (or the caller is the worker loop, which does the drain itself).
    /// Performs PC mic override + driveOneTxBlockFromInterleaved.
    void dispatchOneBlock();

    TxChannel*    m_txChannel{nullptr};   // not owned
    AudioEngine*  m_audioEngine{nullptr}; // not owned
    TxMicSource*  m_micSource{nullptr};   // not owned

    // Phase 3R K-bench: active RADE channel.  Not owned (lifecycle is
    // WdspEngine::m_radeChannels via unique_ptr; SliceModel triggers
    // create/destroy on DSPMode swap into / out of RADE_U / RADE_L).
    // Accessed only on the worker thread inside dispatchOneBlock's
    // RADE branch and from setRadeChannel; the setter is wired as a
    // queued slot when invoked cross-thread (default AutoConnection
    // auto-resolves correctly because TxWorkerThread itself remains
    // on the main thread — only m_txChannel is moveToThread'd into
    // the worker; see RadioModel.cpp:3570 [3M-1c v3]).
    std::atomic<RadeChannel*> m_radeChannel{nullptr};

    // Phase 3R K-bench: DSP helpers for the RADE TX pump.  Owned per-
    // worker so their stateful internals (biquad z1/z2, r8brain
    // CDSPResampler24 filterbank) survive across pump ticks.  Reset
    // on RADE entry happens implicitly: the HPF transients decay
    // within ~10 ms (well under the first emit), and r8brain's
    // warm-up latency is roughly 3 blocks at the 48 -> 16 kHz ratio
    // — both are acceptable artefacts on TX onset.
    RadeTxHpf80     m_radeHpf;
    RadeTx48to16    m_radeResampler;

    // Scratch buffers for the RADE branch.  Sized once in the ctor.
    // m_radeMicFloat: 64 mono floats drained from m_in's I channel.
    // m_radeMic16k:   up to ceil(64 * 16/48) = 22 floats post-resample
    //                (sized to 64 for headroom on the first few blocks
    //                while r8brain catches up).
    // m_radeMicInt16: int16 conversion of the above; ready for the
    //                RadeChannel::txEncode QByteArray payload.
    std::vector<float>   m_radeMicFloat;
    std::vector<float>   m_radeMic16k;
    std::vector<int16_t> m_radeMicInt16;

    // Phase 3R K-bench (source-first reframe): RADE-side mic-input
    // substitution. setRadeAudioBlock appends 48 kHz mono float32
    // samples here under m_radeAudioOverrideMutex; dispatchOneBlock's
    // RADE branch drains up to kBlockFrames samples per tick. Mutex
    // is fine for this low-rate path (~24 kHz upstream; emits arrive
    // every ~25 ms during RADE TX, contention is negligible).
    QMutex      m_radeAudioOverrideMutex;
    QByteArray  m_radeAudioOverride;

    // Phase 3R K-bench: RADE pre-encoder mic processing state.
    // Mic gain dB is read live from TransmitModel via setRadeMicGainDb;
    // leveler state is similarly pushed from RadioModel. The peak
    // envelope tracker is worker-local state (no cross-thread writes).
    std::atomic<int>  m_radeMicGainDb{0};       // dB, default 0 (no gain)
    std::atomic<bool> m_radeLevelerOn{true};    // default ON per K1
    std::atomic<int>  m_radeLevelerMaxGainDb{15};  // K1 default
    std::atomic<int>  m_radeLevelerDecayMs{100};   // K1 default
    float             m_radeLevelerPeakEnv{0.0f};  // one-pole peak tracker

    // Worker-thread scratch — interleaved I/Q double buffer drained from
    // m_micSource each tick.  Sized 2 * kBlockFrames doubles.
    std::vector<double> m_in;

    // PC-mic-override scratch — float buffer for AudioEngine::pullTxMic.
    // Sized kBlockFrames floats.
    std::vector<float> m_pcMicBuf;

    // Anti-VOX run gate (3M-3a-iv).  Mirrors the most-recent
    // setAntiVoxRun(bool) call.  Read with acquire in
    // onAntiVoxBlockReady (DSP thread) and again in
    // onAntiVoxSamplesReady, written with release in setAntiVoxRun.
    // Default false matches the existing TxChannel::m_antiVoxRunLast
    // default and the WDSP DEXP create-time `antivox_run = 0` argument
    // (cmaster.c:153 [v2.10.3.13]).
    //
    // Skipping the float→double conversion in TxChannel::sendAntiVoxData
    // when anti-VOX is off is a worker-local optimisation; DEXP itself
    // also has a state-gate at dexp.c:288-297 [v2.10.3.13]
    // (`if (a->state == DEXP_LOW && a->antivox_new != 0)`), but that
    // gate fires AFTER the buffer copy, so the outer atomic saves the
    // conversion + memcpy entirely.
    std::atomic<bool> m_antiVoxRun{false};

    // ── Phase 3R Task K2: mode-aware TX path ────────────────────────────
    //
    // Tracks whether the next block should flow through the WDSP TXA
    // chain (Wdsp, default) or skip it for the RADE neural codec
    // (Rade).  Written from any thread via setCurrentTxPath()
    // (Qt::QueuedConnection from MoxController on main thread), read
    // at the top of dispatchOneBlock() on the worker thread.
    //
    // release/acquire ordering is the standard pattern for a Qt-side
    // member updated from one thread and consumed by another: the
    // writer publishes the new value with release, the reader picks
    // it up with acquire, and the C++ memory model guarantees no
    // tearing or stale read.
    std::atomic<TxPath> m_currentTxPath{TxPath::Wdsp};
};

} // namespace NereusSDR
