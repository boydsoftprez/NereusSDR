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
// =================================================================

// no-port-check: NereusSDR-original file.  The Thetis cmbuffs.c /
// cmaster.c citations identify the architectural pattern this class
// mirrors (worker-thread + semaphore-wake + uniform block size); no
// Thetis logic is line-for-line ported here.

#include "TxWorkerThread.h"

#include "AudioEngine.h"
#include "TxChannel.h"
#include "audio/TxMicSource.h"

#include <QCoreApplication>
#include <QLoggingCategory>

#include <algorithm>

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
    // AFTER establishing the cross-thread connect()s at RadioModel.cpp:
    //   - 1463-1464  TransmitModel::micPreampChanged → setMicPreamp
    //   - 1673-1676  MoxController::txaFlushed       → setRunning(false)
    //   - 1680-1683  voxRunRequested                  → setVoxRun
    //   - 1687-1690  voxThresholdRequested            → setVoxAttackThreshold
    //   - 1695-1698  voxHangTimeRequested             → setVoxHangTime
    //   - 1703-1706  antiVoxGainRequested             → setAntiVoxGain
    //   - 1715-1718  antiVoxSourceWhatRequested       → setAntiVoxRun
    //
    // Once moveToThread runs, AutoConnection auto-resolves to
    // QueuedConnection because the receiver lives on this thread but the
    // sender (TransmitModel / MoxController) lives on the main thread.
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
    if (m_audioEngine != nullptr && m_audioEngine->isPcMicOverrideActive()) {
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

} // namespace NereusSDR
