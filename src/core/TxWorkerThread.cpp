// =================================================================
// src/core/TxWorkerThread.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original file.  See TxWorkerThread.h for the full
// attribution block + design notes.  Phase 3M-1c TX pump
// architecture redesign v3 — semaphore-driven worker loop sourced
// from radio mic frames via TxMicSource.  Plan:
//   docs/architecture/phase3m-1c-tx-pump-architecture-plan.md
// =================================================================
//
// Modification history (NereusSDR):
//   2026-04-29 — Phase 3M-1c TX pump redesign v3 — semaphore-wake
//                 loop replaces v2's QTimer-driven polling.  J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
//   2026-04-29 — Stage-2 review fix C1 — added
//                 QCoreApplication::sendPostedEvents(m_txChannel, 0)
//                 inside run() so cross-thread queued slot calls
//                 (TransmitModel / MoxController → TxChannel setters)
//                 actually deliver after m_txChannel->moveToThread(this).
//                 (Initially shipped as processEvents(AllEvents); refined
//                 to sendPostedEvents on Stage-2 follow-up review for a
//                 surgical TxChannel-targeted drain.)  Same author /
//                 same AI tooling.
//   2026-04-29 — Stage-2 review fix I2 — zero-fill the unfilled slots
//                 in m_in when AudioEngine::pullTxMic returns
//                 got < kBlockFrames.  Prevents radio mic data from
//                 leaking into a PC-mic-selected TX block.  Same
//                 author / same AI tooling.
//   2026-05-03 — Phase 3M-3a-iii Task 20 — dispatchOneBlock now invokes
//                 TxChannel::pumpDexp BEFORE driveOneTxBlockFromInterleaved,
//                 mirroring Thetis cmaster.c:388-389 [v2.10.3.13]
//                 (xdexp(tx) before fexchange0).  Replaces the placeholder
//                 comment that documented the gap.  J.J. Boyd (KG4VCF),
//                 with AI-assisted implementation via Anthropic Claude Code.
//   2026-05-07 — Phase 3M-3a-iv Task 6 — added 4 anti-VOX queued slots
//                 (setAntiVoxRun, onAntiVoxSamplesReady,
//                 setAntiVoxBlockGeometry, setAntiVoxDetectorTau) plus
//                 the m_antiVoxRun atomic gate.  setAntiVoxRun mirrors
//                 the bool into m_antiVoxRun (release) so
//                 onAntiVoxSamplesReady can short-circuit via
//                 acquire-load when anti-VOX is off.  Single-RX
//                 equivalent of Thetis ChannelMaster aamix output stage
//                 (cmaster.c:159-175 [v2.10.3.13]).  RadioModel wires
//                 the actual signal connections in 3M-3a-iv Task 9.
//                 Plan:
//                 docs/superpowers/plans/2026-05-07-phase3m-3a-iv-antivox-feed.md
//                 J.J. Boyd (KG4VCF), with AI-assisted implementation
//                 via Anthropic Claude Code.
//   2026-05-11 — Phase 3R Task K2 — mode-aware TxPath enum scaffolding.
//                 Added the TxPath enum (Wdsp / Rade), the
//                 std::atomic<TxPath> m_currentTxPath member (default
//                 Wdsp), the setCurrentTxPath cross-thread queued slot,
//                 a one-shot info-log on the first RADE entry, and an
//                 early-return at the top of dispatchOneBlock when the
//                 path is Rade.  The full RADE-path integration (mic
//                 feed -> 80 Hz HPF -> 48-16 resampler ->
//                 RadeChannel::txEncode) is deferred to K-bench
//                 follow-up; this commit only pins the scaffolding so
//                 future work can wire the mic source without touching
//                 the run-loop again.  J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
//   2026-05-11 — Phase 3R K-bench — RADE TX pump completion.  Replaced
//                 the K2 early-return-with-log RADE branch with the
//                 real-time pump (HPF -> 48-16 resample -> int16
//                 conversion -> radeMicBlockReady signal).  Added
//                 setRadeChannel cross-thread setter + std::atomic
//                 m_radeChannel member, three scratch buffers
//                 (m_radeMicFloat / m_radeMic16k / m_radeMicInt16),
//                 the radeMicBlockReady(QByteArray) signal, the
//                 RadeTxHpf80 + RadeTx48to16 K3 helpers as members,
//                 and a radeChannelForTest accessor.  J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
// =================================================================

// no-port-check: NereusSDR-original file.  The Thetis cmbuffs.c /
// cmaster.c citations identify the architectural pattern this class
// mirrors (worker-thread + semaphore-wake + uniform block size); no
// Thetis logic is line-for-line ported here.

#include "TxWorkerThread.h"

#include "AudioEngine.h"
#include "RadeChannel.h"
#include "TxChannel.h"
#include "audio/TxMicSource.h"

#include <QCoreApplication>
#include <QLoggingCategory>

#include <algorithm>
#include <cmath>

Q_LOGGING_CATEGORY(lcTxWorker, "nereus.tx.worker")

namespace NereusSDR {

TxWorkerThread::TxWorkerThread(QObject* parent)
    : QThread(parent)
{
    setObjectName(QStringLiteral("TxWorkerThread"));
    // Pre-allocate scratch buffers — mirror Thetis CMB allocation in
    // create_cmbuffs (cmbuffs.c:50 [v2.10.3.13]).  Sized for fexchange0:
    //   m_in       == 2 * kBlockFrames doubles  (interleaved I/Q)
    //   m_pcMicBuf == kBlockFrames floats        (mono, AudioEngine API)
    m_in.assign(static_cast<size_t>(kBlockFrames) * 2, 0.0);
    m_pcMicBuf.assign(static_cast<size_t>(kBlockFrames), 0.0f);

    // Phase 3R K-bench: RADE TX scratch.  Sized for one pump tick at
    // 48 kHz in / 16 kHz out.  m_radeMic16k has headroom (kBlockFrames
    // floats) so the first warm-up ticks where r8brain may emit more
    // than its steady-state output count do not overflow.
    m_radeMicFloat.assign(static_cast<size_t>(kBlockFrames), 0.0f);
    m_radeMic16k.assign(static_cast<size_t>(kBlockFrames), 0.0f);
    m_radeMicInt16.assign(static_cast<size_t>(kBlockFrames), 0);
}

TxWorkerThread::~TxWorkerThread()
{
    // Defensive: stop the pump if RadioModel forgot.  stopPump() is
    // idempotent.
    stopPump();
}

void TxWorkerThread::setTxChannel(TxChannel* ch)
{
    m_txChannel = ch;
}

void TxWorkerThread::setAudioEngine(AudioEngine* engine)
{
    m_audioEngine = engine;
}

void TxWorkerThread::setMicSource(TxMicSource* src)
{
    m_micSource = src;
}

// ---------------------------------------------------------------------------
// setRadeChannel — Phase 3R K-bench
//
// Stores the channel pointer with release ordering so the matching
// acquire-load in dispatchOneBlock's RADE branch picks it up on the
// next pump tick.  Idempotent.  Null clears (the RADE branch then
// drops the block).
//
// Thread affinity: invoked from the main thread via
// RadioModel::wireRadeChannel.  TxWorkerThread the QObject itself
// lives on the main thread (only m_txChannel is moveToThread'd into
// the worker), so a direct call from main is fine.  The acquire-load
// in dispatchOneBlock runs on the worker thread; release/acquire
// ordering correctly handles the cross-thread visibility.
// ---------------------------------------------------------------------------
void TxWorkerThread::setRadeChannel(RadeChannel* channel)
{
    m_radeChannel.store(channel, std::memory_order_release);
}

// ---------------------------------------------------------------------------
// setRadeAudioBlock — Phase 3R K-bench (source-first reframe)
//
// Append the next 48 kHz mono float32 RADE audio block to the
// override buffer. dispatchOneBlock's RADE branch drains up to
// kBlockFrames samples per tick into m_in's I channel before
// running the WDSP TXA modulator.
//
// Called from RadioModel's txModemReady lambda after extracting
// the real component of RadeChannel's stereo output and upsampling
// 24 -> 48 kHz mono. The default Qt::AutoConnection resolves to
// Qt::QueuedConnection because RadeChannel lives on the main thread
// and TxWorkerThread (as a QThread instance) processes its event
// queue between mic-block drains via the sendPostedEvents pump in
// run(). The mutex serialises against the worker thread's drain
// inside dispatchOneBlock.
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// setRadeMicGainDb / setRadeLeveler — Phase 3R K-bench
//
// Cross-thread setters pushed from RadioModel as TransmitModel
// properties change. Atomic stores; the worker reads with relaxed
// load each dispatchOneBlock tick.
// ---------------------------------------------------------------------------
void TxWorkerThread::setRadeMicGainDb(int dB)
{
    m_radeMicGainDb.store(dB, std::memory_order_relaxed);
}

void TxWorkerThread::setRadeLeveler(bool on, int maxGainDb, int decayMs)
{
    m_radeLevelerOn.store(on,         std::memory_order_relaxed);
    m_radeLevelerMaxGainDb.store(maxGainDb, std::memory_order_relaxed);
    m_radeLevelerDecayMs.store(decayMs, std::memory_order_relaxed);
}

void TxWorkerThread::setRadeAudioBlock(const QByteArray& audio48k)
{
    // BENCH DEBUG: one-shot first-receive log so we can verify the
    // RadioModel txModemReady lambda is reaching the worker.
    static int s_radeAudioFirstRecvLogged = 0;
    if (s_radeAudioFirstRecvLogged < 3) {
        qCInfo(lcTxWorker)
            << "setRadeAudioBlock recv #" << (s_radeAudioFirstRecvLogged + 1)
            << "bytes=" << audio48k.size();
        ++s_radeAudioFirstRecvLogged;
    }

    QMutexLocker lk(&m_radeAudioOverrideMutex);
    m_radeAudioOverride.append(audio48k);
    // Bound the buffer to keep latency low if the producer outruns the
    // consumer (e.g. user holding MOX with no actual mic input — RADE
    // still emits silence frames). 1 second @ 48 kHz mono float =
    // 192 KB.
    constexpr int kMaxBytes = 48000 * 1 * static_cast<int>(sizeof(float));
    if (m_radeAudioOverride.size() > kMaxBytes) {
        m_radeAudioOverride.remove(
            0, m_radeAudioOverride.size() - kMaxBytes);
    }
}

void TxWorkerThread::startPump()
{
    if (isRunning()) {
        return;  // idempotent
    }
    if (m_txChannel == nullptr || m_audioEngine == nullptr ||
        m_micSource == nullptr) {
        qCWarning(lcTxWorker)
            << "startPump: missing dependencies (txChannel ="
            << static_cast<const void*>(m_txChannel)
            << ", audioEngine =" << static_cast<const void*>(m_audioEngine)
            << ", micSource ="   << static_cast<const void*>(m_micSource)
            << "); pump NOT started.";
        return;
    }
    qCInfo(lcTxWorker) << "startPump: launching worker thread"
                       << "blockFrames=" << kBlockFrames
                       << "(semaphore-wake, fexchange0)";
    QThread::start(QThread::HighPriority);
}

void TxWorkerThread::stopPump()
{
    if (!isRunning()) {
        return;  // idempotent
    }
    qCInfo(lcTxWorker) << "stopPump: requesting worker exit";

    // Mirror destroy_cmbuffs (cmbuffs.c:60-76 [v2.10.3.13]) — closing the
    // mic source's accept gate AND posting the poison semaphore release
    // breaks the worker out of its waitForBlock().  After that the loop
    // condition (m_micSource->isRunning()) goes false and run() returns.
    if (m_micSource) {
        m_micSource->stop();
    }

    // Wait up to 5 seconds for the worker to exit; bound is defensive.
    if (!QThread::wait(5000)) {
        qCWarning(lcTxWorker)
            << "stopPump: worker thread did not exit within 5 s; "
               "forcing terminate (this is a bug — investigate).";
        QThread::terminate();
        QThread::wait();
    }
}

void TxWorkerThread::run()
{
    // Mirrors Thetis cm_main at cmbuffs.c:151-168 [v2.10.3.13]:
    //   while (_InterlockedAnd (&a->run, 1)) {
    //       WaitForSingleObject(a->Sem_BuffReady, INFINITE);
    //       cmdata (id, pcm->in[id]);
    //       xcmaster(id);
    //   }
    //
    // Note: Thetis's `a->run` flag is NereusSDR's m_micSource->isRunning().
    // The poison release in TxMicSource::stop() wakes us out of
    // waitForBlock; we then re-check isRunning and exit cleanly.
    //
    // ── Why processEvents() inside the loop ──────────────────────────────
    //
    // RadioModel::connectToRadio() calls m_txChannel->moveToThread(this)
    // AFTER establishing the cross-thread connect()s at RadioModel.cpp.
    // Two flavours of receivers:
    //
    //   (a) Direct → TxChannel slots (queued because TxChannel lives on
    //       this worker after moveToThread):
    //         TransmitModel::micPreampChanged       → TxChannel::setMicPreamp
    //         MoxController::txaFlushed             → TxChannel::setRunning(false)
    //         MoxController::voxRunRequested        → TxChannel::setVoxRun
    //         MoxController::voxThresholdRequested  → TxChannel::setVoxAttackThreshold
    //         MoxController::voxHangTimeRequested   → TxChannel::setVoxHangTime
    //         MoxController::antiVoxGainRequested   → TxChannel::setAntiVoxGain
    //         (3M-3a-iv post-bench refactor (Option A) removed the
    //          MoxController::antiVoxSourceWhatRequested → TxChannel::setAntiVoxRun
    //          wire that previously lived here; see MoxController.h header
    //          comment for the architectural rationale.)
    //
    //   (b) NEW (3M-3a-iv) → TxWorkerThread anti-VOX slots.  The
    //       TxWorkerThread QObject itself lives on the MAIN thread (only
    //       m_txChannel is moveToThread'd into this worker via
    //       RadioModel.cpp:2686), so these slots execute on main thread
    //       when the queued connections deliver.  The wrappers forward
    //       to TxChannel (which IS on this worker thread, so the forward
    //       crosses thread boundaries via Qt's auto-queueing) and also
    //       maintain the worker-local m_antiVoxRun atomic gate.  The
    //       atomic's release-store happens on the TX worker thread (via
    //       the RadioModel.cpp:2131 lambda whose receiver context is
    //       m_txChannel); the acquire-load happens on main thread inside
    //       onAntiVoxSamplesReady — release/acquire ordering correctly
    //       handles the cross-thread visibility.  No mutex is held in
    //       any audio-thread path:
    //         RxDspWorker::antiVoxSampleReady          → onAntiVoxSamplesReady
    //         RxDspWorker::bufferSizesChanged          → setAntiVoxBlockGeometry
    //         MoxController::antiVoxDetectorTauRequested → setAntiVoxDetectorTau
    //
    // Once moveToThread runs for m_txChannel, AutoConnection auto-resolves
    // to QueuedConnection for group (a) because the receiver lives on
    // this thread but the sender (TransmitModel / MoxController) lives on
    // the main thread.
    // Each emission posts a QMetaCallEvent into THIS thread's event
    // queue.  Without a pumper, those events sit in the queue forever
    // and the lambda / setter NEVER fires — UI changes during active TX
    // silently fail to reach WDSP.
    //
    // QThread::exec() would dispatch them, but our top-level loop is a
    // semaphore-wake (waitForBlock(-1)) sourced from radio mic frames —
    // we cannot replace it with exec() without restructuring the cadence
    // source.  The minimal correct fix is to call processEvents() once
    // per iteration: drain any queued slot calls between waking from
    // waitForBlock and dispatching the next fexchange0 cycle so updates
    // apply to the upcoming block.
    //
    // Thetis itself doesn't need this — its cm_main is a native pthread
    // (no Qt event loop), and its setters drop straight into WDSP via
    // P/Invoke regardless of which managed thread is calling.
    // NereusSDR's setters are Qt slots dispatched via signals, so we
    // have to give the worker an event pump.
    while (m_micSource && m_micSource->isRunning()) {
        // INFINITE wait — mirrors `WaitForSingleObject(..., INFINITE)`.
        // Returns false when stop() releases the poison semaphore AND
        // m_running has flipped; in that case we exit the loop.
        if (!m_micSource->waitForBlock(-1)) {
            break;
        }
        if (!m_micSource->isRunning()) {
            break;
        }

        // Drain one block of kBlockFrames pairs (== 2*kBlockFrames doubles)
        // into m_in.  Equivalent to Thetis cmdata (cmbuffs.c:123-149).
        m_micSource->drainBlock(m_in.data());

        // Drain any queued cross-thread slot calls (setMicPreamp,
        // setVoxRun, setVoxAttackThreshold, setVoxHangTime,
        // setAntiVoxGain, setAntiVoxRun, txaFlushed→setRunning(false))
        // BEFORE dispatching the DSP cycle so the updated WDSP state
        // applies to the upcoming block.  This is the Qt-Posted-Event
        // analogue of the implicit event-pump that Thetis's Win32
        // cm_main loop never needed (cmbuffs.c [v2.10.3.13] —
        // pthread/native, no Qt event loop in the picture).
        //
        // We call sendPostedEvents targeted at m_txChannel (rather than
        // QCoreApplication::processEvents) so the dispatch is surgical:
        // only QMetaCallEvents posted to the TxChannel are delivered.
        // No SocketNotifier, DeferredDeletion, timer, or paint events for
        // any other QObject that might one day be affined to this worker
        // get pulled in.  Faster (no event-filter walk), narrower
        // surface, and locks in the invariant "only TxChannel is
        // addressable on this thread" against future refactors that
        // might affine additional QObjects to the worker.
        QCoreApplication::sendPostedEvents(m_txChannel, 0);

        dispatchOneBlock();
    }

    qCInfo(lcTxWorker) << "run: worker thread loop exited";
}

void TxWorkerThread::dispatchOneBlock()
{
    if (m_txChannel == nullptr) {
        return;
    }

    // ── Phase 3J-1 bench fix (2026-05-10): TCI audio source override ───────
    //
    // When a TCI client holds the TX audio mutex (trx:N,true,tci;) the
    // binary-frame pipeline pushes WSJT-X's audio into m_tciInputRing on
    // TxChannel.  Pull one 64-frame block from that ring here and splice it
    // into m_in (overwriting whatever the mic source put there) — same
    // pattern as the PC/VAX mic override branches below.
    //
    // Rate match: TCI producer pushes ~2048 samples per WSJT-X message
    // (every ~43 ms in bursts of <1 ms).  Consumer (us) pulls 64 samples
    // every ~1.33 ms == 48 kHz, matching the HL2 wire rate exactly.  The
    // ring (~680 ms headroom) absorbs the burst-vs-steady mismatch.  The
    // downstream TX I/Q ring sees a steady 64-frame trickle — no
    // overflow, no dropped samples.
    //
    // If the ring is empty (TCI producer hasn't pushed yet, or briefly
    // between WSJT-X messages), zero-fill the block.  Silence is the
    // correct degradation — same policy as the PC/VAX mic override
    // partial-pull case.
    if (m_txChannel->isTciAudioActive()) {
        const int got = m_txChannel->pullTciAudio(m_pcMicBuf.data(), kBlockFrames);
        const int n   = std::clamp(got, 0, kBlockFrames);
        for (int i = 0; i < n; ++i) {
            m_in[static_cast<size_t>(2 * i + 0)] =
                static_cast<double>(m_pcMicBuf[static_cast<size_t>(i)]);
            m_in[static_cast<size_t>(2 * i + 1)] = 0.0;
        }
        for (int i = n; i < kBlockFrames; ++i) {
            m_in[static_cast<size_t>(2 * i + 0)] = 0.0;
            m_in[static_cast<size_t>(2 * i + 1)] = 0.0;
        }
        // Skip the PC/VAX mic override branches below — TCI is the
        // active source.  Fall through to pumpDexp + driveOneTxBlock so
        // WDSP processes the TCI audio just like a normal mic block.
        m_txChannel->pumpDexp(m_in.data());
        m_txChannel->driveOneTxBlockFromInterleaved(m_in.data());
        return;
    }

    // ── Phase 3R K-bench: mode-aware path RADE pump ────────────────────
    //
    // RadioModel updates m_currentTxPath on every MOX-on transition
    // based on the active slice's DSPMode.  When the slice is in
    // DSPMode::RADE_U or DSPMode::RADE_L the worker bypasses the WDSP
    // TXA chain entirely and routes the mic block through the RADE
    // neural codec: HPF -> 48 -> 16 kHz resample -> float -> int16 ->
    // radeMicBlockReady signal (queued to RadeChannel::txEncode on
    // main thread).  RadeChannel then emits txModemReady to the
    // RadioModel hook which converts to I/Q + drives
    // RadioConnection::sendTxIq.
    //
    // No early-return-with-log scaffolding; the pump runs every tick.
    const TxPath path = m_currentTxPath.load(std::memory_order_acquire);
    if (path == TxPath::Rade) {
        // ── Phase 3R K-bench (source-first reframe) ──────────────────────
        //
        // RADE TX runs THROUGH the WDSP TXA modulator stage, NOT around
        // it. freedv-gui's RADETransmitStep.cpp:196-200 [@77e793a] takes
        // ONLY the real component of rade_tx's RADE_COMP output and
        // feeds it as mono audio; the radio's hardware SSB modulator
        // handles USB/LSB. NereusSDR's equivalent: WDSP's TXA modulator
        // (in USB or LSB mode per TxChannel::setTxMode's RADE_U/L ->
        // USB/LSB mapping).
        //
        // Flow:
        //   1. Drain mic block (m_in already populated by run()).
        //   2. HPF + 48 -> 16 kHz resample (RADE input).
        //   3. emit radeMicBlockReady -> RadeChannel::txEncode (queued
        //      to main thread). RadeChannel runs rade_tx and emits
        //      txModemReady (24 kHz stereo float).
        //   4. RadioModel's wireRadeChannel lambda extracts the real
        //      component, upsamples 24 -> 48 kHz, and pushes the result
        //      back to TxWorkerThread via setRadeAudioBlock().
        //   5. Below the override switch (continuing into the WDSP
        //      path), m_in's I channel is REPLACED with the latest
        //      RADE audio block when path == Rade. The WDSP TXA chain
        //      then modulates it as ordinary SSB voice.
        //
        // Earlier scaffolding (K2 early-return + K4 direct sendTxIq
        // with I=mono, Q=0) produced DSB modulation and bypassed the
        // WDSP modulator, which broke TUNE in RADE mode (TUNE writes
        // PostGen + relies on the modulator stage running). This
        // version runs the full WDSP TXA chain so TUNE and RADE TX
        // share the same modulator path.

        // Step 1: gather the actual mic samples for RADE encoding.
        //
        // The user's selected mic source (PC mic, VAX, or radio mic)
        // determines where speech comes from. The PC/VAX override
        // paths below the RADE branch are gated on path != Rade now,
        // so they no longer write to m_in — we must pull the correct
        // source HERE for the RADE encoder input.
        //
        // Priority order matches the existing PC/VAX paths below:
        //   1. VAX TX bus (shared-memory HAL bridge).
        //   2. PC mic (PortAudio/QAudio capture).
        //   3. Radio mic (already in m_in from m_micSource->drainBlock).
        //
        // Empty pulls and short-pulls are zero-filled so silent ticks
        // produce silent RADE output (no leaked radio mic).
        if (m_audioEngine != nullptr
            && m_audioEngine->isVaxMicOverrideActive()) {
            const int got = m_audioEngine->pullVaxTxMic(
                m_pcMicBuf.data(), kBlockFrames);
            const int n = std::clamp(got, 0, kBlockFrames);
            for (int i = 0; i < n; ++i) {
                m_radeMicFloat[static_cast<size_t>(i)] =
                    m_pcMicBuf[static_cast<size_t>(i)];
            }
            for (int i = n; i < kBlockFrames; ++i) {
                m_radeMicFloat[static_cast<size_t>(i)] = 0.0f;
            }
        } else if (m_audioEngine != nullptr
                   && m_audioEngine->isPcMicOverrideActive()) {
            const int got = m_audioEngine->pullTxMic(
                m_pcMicBuf.data(), kBlockFrames);
            const int n = std::clamp(got, 0, kBlockFrames);
            for (int i = 0; i < n; ++i) {
                m_radeMicFloat[static_cast<size_t>(i)] =
                    m_pcMicBuf[static_cast<size_t>(i)];
            }
            for (int i = n; i < kBlockFrames; ++i) {
                m_radeMicFloat[static_cast<size_t>(i)] = 0.0f;
            }
        } else {
            // Radio mic — extract I channel from m_in (populated by
            // run() / tickForTest from m_micSource->drainBlock).
            for (int i = 0; i < kBlockFrames; ++i) {
                m_radeMicFloat[static_cast<size_t>(i)] =
                    static_cast<float>(m_in[static_cast<size_t>(2 * i)]);
            }
        }

        // ── Phase 3R K-bench: pre-RADE mic gain + leveler ──────────────
        //
        // Per K1 design + bench feedback: raw mic at typical line level
        // is ~-30 dBFS, well below RADE's ~-9 dBFS sweet spot. Apply
        // user mic gain (dB) + one-pole peak-tracking AGC (Lev_MaxGain
        // dB ceiling, Lev_Decay ms release) here BEFORE handing audio
        // to the RADE encoder. Mirrors freedv-gui's WebRTC-AGC -9 dBFS
        // target pre-codec stage.
        //
        // Why not the WDSP TXA Leveler stage? It runs AFTER our RADE
        // audio substitution (further down dispatchOneBlock), which
        // means WDSP would level the RADE modem signal itself —
        // destroying the modem's amplitude characteristics. The K1
        // RADE profile must therefore disable WDSP TXA Leveler;
        // NereusSDR-side leveling here is the substitute.
        {
            // Step A: apply linear mic gain (m_micGainDb dB -> linear).
            const int gainDb = m_radeMicGainDb.load(std::memory_order_relaxed);
            const float gainLin = std::pow(10.0f, gainDb / 20.0f);
            if (std::abs(gainLin - 1.0f) > 1e-4f) {
                for (int i = 0; i < kBlockFrames; ++i) {
                    m_radeMicFloat[static_cast<size_t>(i)] *= gainLin;
                }
            }

            // Step B: one-pole peak-tracking AGC.
            // Target peak ~= -9 dBFS = 10^(-9/20) ≈ 0.3548.
            // Attack: instant (peak goes up immediately so transients
            //         don't clip mid-block).
            // Release: per Lev_Decay ms (K1 default 100 ms).
            // Max gain: per Lev_MaxGain dB (K1 default 15 dB).
            if (m_radeLevelerOn.load(std::memory_order_relaxed)) {
                constexpr float kTargetPeak = 0.3548f;  // -9 dBFS
                const int decayMs = m_radeLevelerDecayMs.load(
                    std::memory_order_relaxed);
                const int maxGdb  = m_radeLevelerMaxGainDb.load(
                    std::memory_order_relaxed);
                const float blockTimeMs =
                    1000.0f * kBlockFrames / 48000.0f;  // ~1.33 ms
                const float release = std::exp(
                    -blockTimeMs / std::max(1.0f,
                                            static_cast<float>(decayMs)));

                float blockPeak = 0.0f;
                for (int i = 0; i < kBlockFrames; ++i) {
                    const float a = std::abs(
                        m_radeMicFloat[static_cast<size_t>(i)]);
                    if (a > blockPeak) { blockPeak = a; }
                }
                if (blockPeak > m_radeLevelerPeakEnv) {
                    m_radeLevelerPeakEnv = blockPeak;  // instant attack
                } else {
                    m_radeLevelerPeakEnv =
                        m_radeLevelerPeakEnv * release
                        + blockPeak * (1.0f - release);
                }
                if (m_radeLevelerPeakEnv > 1e-5f) {
                    const float maxGainLin =
                        std::pow(10.0f, maxGdb / 20.0f);
                    const float desiredGain =
                        kTargetPeak / m_radeLevelerPeakEnv;
                    const float clampedGain =
                        std::min(desiredGain, maxGainLin);
                    if (std::abs(clampedGain - 1.0f) > 1e-4f) {
                        for (int i = 0; i < kBlockFrames; ++i) {
                            m_radeMicFloat[static_cast<size_t>(i)] *=
                                clampedGain;
                        }
                    }
                }
            }
        }

        // BENCH DEBUG: one-shot log to see what mic source RADE is
        // using and the peak amplitude of the first few blocks AFTER
        // gain + leveler. Should be ~-9 dBFS (0.35) for steady speech.
        static int s_radeMicSourceLogged = 0;
        if (s_radeMicSourceLogged < 3) {
            float peak = 0.0f;
            for (int i = 0; i < kBlockFrames; ++i) {
                const float a = std::abs(
                    m_radeMicFloat[static_cast<size_t>(i)]);
                if (a > peak) { peak = a; }
            }
            const char* src =
                (m_audioEngine && m_audioEngine->isVaxMicOverrideActive())
                    ? "VAX"
                    : (m_audioEngine
                       && m_audioEngine->isPcMicOverrideActive())
                          ? "PC"
                          : "Radio";
            qCInfo(lcTxWorker)
                << "RADE mic block #" << (s_radeMicSourceLogged + 1)
                << "source=" << src
                << "peak(post-gain+lev)=" << peak
                << "(gain=" << m_radeMicGainDb.load() << "dB lev="
                << m_radeLevelerOn.load() << ")";
            ++s_radeMicSourceLogged;
        }

        // Step 2: 80 Hz Butterworth HPF in place (K3 RadeTxHpf80).
        m_radeHpf.process(m_radeMicFloat.data(), kBlockFrames);

        // Step 3: 48 kHz -> 16 kHz r8brain CDSPResampler24.
        // r8brain may emit zero on warm-up; later ticks emit roughly
        // kBlockFrames * 16 / 48 = 21-22 samples per call.
        const int out16k = m_radeResampler.process(
            m_radeMicFloat.data(), kBlockFrames,
            m_radeMic16k.data(),   kBlockFrames);

        if (out16k > 0) {
            // Step 4: float -> int16 conversion. RadeChannel::txEncode
            // takes int16 mono at 16 kHz (per the I3 contract at
            // RadeChannel.h:258-261 [Phase 3R I3]).
            m_radeMicInt16.resize(static_cast<size_t>(out16k));
            for (int i = 0; i < out16k; ++i) {
                const float s = m_radeMic16k[static_cast<size_t>(i)];
                const float scaled = s * 32767.0f
                                      + (s >= 0.0f ? 0.5f : -0.5f);
                int v = static_cast<int>(scaled);
                if (v >  32767) { v =  32767; }
                if (v < -32768) { v = -32768; }
                m_radeMicInt16[static_cast<size_t>(i)] =
                    static_cast<int16_t>(v);
            }

            // Step 5: wrap the int16 samples in a QByteArray and emit
            // (queued connection to RadeChannel::txEncode on main
            // thread; queues until txEncode runs).
            const QByteArray payload(
                reinterpret_cast<const char*>(m_radeMicInt16.data()),
                out16k * static_cast<int>(sizeof(int16_t)));
            // BENCH DEBUG: one-shot first-emit log so the bench operator
            // can confirm the worker's RADE pump is actually firing.
            static int s_radeMicFirstEmitLogged = 0;
            if (s_radeMicFirstEmitLogged < 3) {
                qCInfo(lcTxWorker)
                    << "RADE pump emit #" << (s_radeMicFirstEmitLogged + 1)
                    << "payload bytes=" << payload.size()
                    << "(16k samples=" << out16k << ")";
                ++s_radeMicFirstEmitLogged;
            }
            emit radeMicBlockReady(payload);
        }
        // r8brain warm-up tick (out16k == 0) just skips the RADE encode;
        // the WDSP TXA chain still runs below with whatever RADE audio
        // is currently queued in m_radeAudioOverride (silence if empty).

        // Step 6: substitute RADE-encoded audio for the mic block in
        // m_in's I channel before falling through to the WDSP TXA
        // chain. m_radeAudioOverride is populated by RadioModel's
        // txModemReady lambda via setRadeAudioBlock (queued).
        {
            QMutexLocker lk(&m_radeAudioOverrideMutex);
            const int avail = static_cast<int>(m_radeAudioOverride.size())
                              / static_cast<int>(sizeof(float));
            const int take = std::min(avail, kBlockFrames);
            const float* src = reinterpret_cast<const float*>(
                m_radeAudioOverride.constData());
            for (int i = 0; i < take; ++i) {
                m_in[static_cast<size_t>(2 * i + 0)] =
                    static_cast<double>(src[i]);
                m_in[static_cast<size_t>(2 * i + 1)] = 0.0;
            }
            for (int i = take; i < kBlockFrames; ++i) {
                m_in[static_cast<size_t>(2 * i + 0)] = 0.0;
                m_in[static_cast<size_t>(2 * i + 1)] = 0.0;
            }
            if (take > 0) {
                m_radeAudioOverride.remove(
                    0, take * static_cast<int>(sizeof(float)));
            }
        }
        // Fall through to the WDSP TXA chain below.
    }

    // PC mic override — mirrors Thetis cmaster.c:379 [v2.10.3.13]:
    //   asioIN(pcm->in[stream]);
    // ASIO is the OS-mic source; in NereusSDR this is the PortAudio /
    // QAudio bus owned by AudioEngine.  When the user has selected
    // MicSource::Pc AND the bus is open, AudioEngine::isPcMicOverrideActive
    // returns true and we splice PC mic samples into m_in's I channel,
    // overwriting whatever the radio sent.
    //
    // Partial-pull policy: PC mic is the user's chosen TX input; partial
    // pulls (got < kBlockFrames) are zero-filled across the remaining
    // slots — the radio mic must NOT leak into a PC-selected TX block.
    // Silence is the correct degradation under PC-mic short-pull (the
    // user expects "the PC mic" — if the bus stalls, what they hear on
    // the air is silence, not their radio's hand mic on top of dead
    // audio).  Q channel always stays zero (real-only TX input).
    //
    // Phase 3R K-bench: when path == Rade the RADE branch above has
    // already written RADE-encoded audio into m_in. Skip the PC/VAX
    // overrides; otherwise live mic samples would clobber the RADE
    // baseband and produce a mic + RADE chimera on the air. The
    // RADE pump on the input side already drained the mic block to
    // feed RadeChannel::txEncode (Step 1 above); the WDSP TXA chain
    // below must see ONLY the RADE audio.
    if (path != TxPath::Rade
        && m_audioEngine != nullptr && m_audioEngine->isVaxMicOverrideActive()) {
        // VAX TX override (eager-borg-d64bed, 2026-05-06).  Mirrors the
        // PC mic override path below, but pulls from the VAX TX shared-
        // memory bus that the HAL plugin populates from a 3rd-party
        // app's writes to "NereusSDR TX".  Same partial-pull / zero-fill
        // policy: VAX is the user's chosen TX input, so a short pull
        // must NOT leak radio mic samples through; we zero-fill the
        // remainder of the block.
        const int got = m_audioEngine->pullVaxTxMic(m_pcMicBuf.data(), kBlockFrames);
        const int n   = std::clamp(got, 0, kBlockFrames);
        for (int i = 0; i < n; ++i) {
            m_in[static_cast<size_t>(2 * i + 0)] =
                static_cast<double>(m_pcMicBuf[static_cast<size_t>(i)]);
            m_in[static_cast<size_t>(2 * i + 1)] = 0.0;
        }
        for (int i = n; i < kBlockFrames; ++i) {
            m_in[static_cast<size_t>(2 * i + 0)] = 0.0;
            m_in[static_cast<size_t>(2 * i + 1)] = 0.0;
        }
    } else if (path != TxPath::Rade
               && m_audioEngine != nullptr && m_audioEngine->isPcMicOverrideActive()) {
        const int got = m_audioEngine->pullTxMic(m_pcMicBuf.data(), kBlockFrames);
        const int n   = std::clamp(got, 0, kBlockFrames);
        for (int i = 0; i < n; ++i) {
            m_in[static_cast<size_t>(2 * i + 0)] =
                static_cast<double>(m_pcMicBuf[static_cast<size_t>(i)]);
            m_in[static_cast<size_t>(2 * i + 1)] = 0.0;
        }
        // Stage-2 review fix I2: zero-fill the unfilled slots so a short
        // PortAudio pull does not leak the radio mic block through.  The
        // worker just drained kBlockFrames of radio mic into m_in above;
        // that data must be overwritten when PC mic is the selected
        // source.  Silent degradation is the user-correct behaviour.
        for (int i = n; i < kBlockFrames; ++i) {
            m_in[static_cast<size_t>(2 * i + 0)] = 0.0;
            m_in[static_cast<size_t>(2 * i + 1)] = 0.0;
        }
    }

    // VOX / DEXP detector — mirrors Thetis cmaster.c:388 [v2.10.3.13]:
    //   xdexp (tx);   // vox-dexp
    // (called BEFORE fexchange0 at cmaster.c:389).
    //
    // Phase 3M-3a-iii Task 20: TxChannel::pumpDexp copies m_in into the
    // WdspEngine-owned per-channel DEXP buffer and runs xdexp.  WDSP
    // fires the pushvox callback synchronously from inside xdexp if the
    // mic envelope crosses the attack threshold or if the HOLD timer
    // expires after audio drops below threshold — that callback is the
    // VOX-keying entry point (TxChannel::s_pushVoxCallback emits
    // voxActiveChanged → MoxController::onVoxActive).
    //
    // NereusSDR's DEXP buffer architecture is parallel-only (see
    // WdspEngine.cpp create_dexp callsite for the full narrative): the
    // copy + xdexp here drive the detector for VOX-keying purposes only;
    // the DEXP module's audio-domain output is not chained into m_in,
    // so DEXP audio expansion (run_dexp=1) does not affect the bytes
    // that fexchange0 reads.  This is a known limitation; VOX-keying
    // is what was bench-failing and is now fixed.
    //
    // Null-safe inside pumpDexp: degrades to a no-op if pdexp[id] /
    // txa[].rsmpin.p / m_dexpBuffer are missing.
    m_txChannel->pumpDexp(m_in.data());

    // fexchange0 — mirrors cmaster.c:389 [v2.10.3.13]:
    //   fexchange0 (chid (stream, 0), pcm->in[stream],
    //               pcm->xmtr[tx].out[0], &error);
    // TxChannel::driveOneTxBlockFromInterleaved internally handles
    // fexchange0 + interleave-to-float + sendTxIq + sip1OutputReady emit.
    m_txChannel->driveOneTxBlockFromInterleaved(m_in.data());
}

#ifdef NEREUS_BUILD_TESTS
void TxWorkerThread::tickForTest()
{
    if (m_micSource == nullptr || m_txChannel == nullptr) {
        return;
    }
    // Match the run() loop's order: wait briefly for a block, drain it,
    // then dispatch.  Tests must call inbound() on the mic source before
    // tickForTest() so the semaphore is ready.
    if (!m_micSource->waitForBlock(/*timeoutMs=*/100)) {
        return;
    }
    m_micSource->drainBlock(m_in.data());
    dispatchOneBlock();
}
#endif

// ---------------------------------------------------------------------------
// setAntiVoxRun()  — Phase 3M-3a-iv Task 6
//
// Forwards the run flag to TxChannel::setAntiVoxRun (the WDSP wrapper
// that calls SetAntiVOXRun) AND mirrors it into m_antiVoxRun via a
// release-store so onAntiVoxSamplesReady can short-circuit via
// acquire-load when anti-VOX is disabled.
//
// Thread affinity: this method is invoked by the lambda at
// RadioModel.cpp:2131-2135 whose receiver context is m_txChannel (TX
// worker thread).  The release-store therefore happens on TX worker
// thread.  onAntiVoxSamplesReady (where the acquire-load lives) runs on
// MAIN thread because the TxWorkerThread QObject is parented to
// RadioModel and is NOT itself moveToThread'd — only m_txChannel is.
// Release/acquire ordering correctly handles this cross-thread
// visibility.
//
// The atomic gate is the worker-local optimisation that skips the
// float→double conversion in TxChannel::sendAntiVoxData entirely when
// anti-VOX is off; DEXP itself has an inner state-gate at
// dexp.c:288-297 [v2.10.3.13] that runs after the buffer copy, so the
// outer atomic saves both the conversion and the memcpy.
//
// From Thetis cmaster.cs:208-209 [v2.10.3.13] — SetAntiVOXRun.
// ---------------------------------------------------------------------------
void TxWorkerThread::setAntiVoxRun(bool run)
{
    // release-store ahead of the forward so a subsequent
    // onAntiVoxSamplesReady acquire-load (on main thread) that observes
    // the new flag is guaranteed to see m_txChannel's anti-VOX state
    // already updated by the time the forwarded setter returns.  Both
    // this method and the forwarded m_txChannel->setAntiVoxRun run on
    // TX worker thread (this method is invoked from the m_txChannel
    // receiver-context lambda, the forward is a same-thread direct
    // call); onAntiVoxSamplesReady on main thread serialises behind the
    // release via the cross-thread acquire.
    m_antiVoxRun.store(run, std::memory_order_release);
    if (m_txChannel != nullptr) {
        m_txChannel->setAntiVoxRun(run);
    }
}

// ---------------------------------------------------------------------------
// onAntiVoxSamplesReady()  — Phase 3M-3a-iv Task 6
//
// Queued slot from RxDspWorker::antiVoxSampleReady.  Single-RX
// equivalent of Thetis cmaster.c:171 [v2.10.3.13]: aamix's
// SendAntiVOXData callback.  Skipped when anti-VOX is off (acquire-load
// of m_antiVoxRun) to avoid the float→double conversion cost in
// TxChannel::sendAntiVoxData.
//
// Thread affinity: this slot runs on the MAIN thread because the
// TxWorkerThread QObject lives on main thread (only m_txChannel is
// moveToThread'd into the worker).  The forwarded
// m_txChannel->sendAntiVoxData call therefore crosses thread boundaries
// — but Qt's direct-call dispatch is fine here: TxChannel's
// sendAntiVoxData is documented audio-safe (no allocation, no mutex,
// resident scratch buffer) and pdexp[] is null-guarded.  No mic-input
// audio callback is involved at any point in this path.
//
// The sliceId == 0 gate scopes 3M-3a-iv to a single RX feeding the
// single TX; multi-RX mux (where N RXs combine into one anti-VOX
// stream via aamix) is a 3F multi-pan concern.
// ---------------------------------------------------------------------------
void TxWorkerThread::onAntiVoxSamplesReady(int sliceId,
                                           const QVector<float>& interleaved,
                                           int sampleCount)
{
    if (!m_antiVoxRun.load(std::memory_order_acquire)) { return; }
    if (m_txChannel == nullptr) { return; }
    if (sliceId != 0) { return; }  // single-RX gate; multi-RX mux is 3F
    m_txChannel->sendAntiVoxData(interleaved.constData(), sampleCount);
}

// ---------------------------------------------------------------------------
// setAntiVoxBlockGeometry()  — Phase 3M-3a-iv Task 6
//
// Queued slot from RxDspWorker::bufferSizesChanged.  Calls both
// TxChannel::setAntiVoxSize and TxChannel::setAntiVoxRate so DEXP's
// antivox_size and antivox_rate stay aligned with the post-decimation
// RX block geometry.
//
// From Thetis cmaster.c:154-155 [v2.10.3.13]: DEXP create-time uses
// pcm->audio_outsize / pcm->audio_outrate (the post-decimation audio
// output dimensions) — not TX in_size / in_rate — for anti-VOX
// detector geometry.  When the RX path renegotiates its output
// geometry (e.g. on bandwidth change), DEXP must follow.
// ---------------------------------------------------------------------------
void TxWorkerThread::setAntiVoxBlockGeometry(int outSize, double outRate)
{
    if (m_txChannel == nullptr) { return; }
    m_txChannel->setAntiVoxSize(outSize);
    m_txChannel->setAntiVoxRate(outRate);
}

// ---------------------------------------------------------------------------
// setAntiVoxDetectorTau()  — Phase 3M-3a-iv Task 6
//
// Queued slot from MoxController::antiVoxDetectorTauRequested.
// Pass-through to TxChannel::setAntiVoxDetectorTau (which calls WDSP
// SetAntiVOXDetectorTau).  Tau here is in seconds; ms→s conversion is
// done at the controller, this slot takes seconds directly.
//
// From Thetis setup.cs:18992-18996 [v2.10.3.13] —
//   private void udAntiVoxTau_ValueChanged(...)
//   { cmaster.SetAntiVOXDetectorTau(0, (double)udAntiVoxTau.Value / 1000.0); }
// ---------------------------------------------------------------------------
void TxWorkerThread::setAntiVoxDetectorTau(double seconds)
{
    if (m_txChannel == nullptr) { return; }
    m_txChannel->setAntiVoxDetectorTau(seconds);
}

// ---------------------------------------------------------------------------
// setCurrentTxPath()  — Phase 3R Task K2
//
// Cross-thread queued slot.  Stored with release ordering so the
// matching acquire-load at the top of dispatchOneBlock() picks up the
// new value on the next tick.  Idempotent: setting to the current
// value is a cheap no-op store (the atomic API does no comparison
// short-circuit, but the cost is one write versus one compare-then-
// write, which is the same cache traffic).
//
// Thread affinity: caller is RadioModel on the main thread (via the
// MoxController moxStateChanged hook in RadioModel::connectToRadio
// extension); the load runs on the TX worker thread inside
// dispatchOneBlock.  release/acquire ordering correctly handles
// cross-thread visibility.
// ---------------------------------------------------------------------------
void TxWorkerThread::setCurrentTxPath(TxPath path)
{
    m_currentTxPath.store(path, std::memory_order_release);
}

#ifdef NEREUS_BUILD_TESTS
TxWorkerThread::TxPath TxWorkerThread::currentTxPathForTest() const
{
    return m_currentTxPath.load(std::memory_order_acquire);
}

RadeChannel* TxWorkerThread::radeChannelForTest() const
{
    return m_radeChannel.load(std::memory_order_acquire);
}
#endif

} // namespace NereusSDR
