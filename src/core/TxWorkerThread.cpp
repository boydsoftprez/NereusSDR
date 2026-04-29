// =================================================================
// src/core/TxWorkerThread.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original file.  See TxWorkerThread.h for the full
// attribution block + design notes.  Phase 3M-1c TX pump
// architecture redesign — replaces D.1/E.1/L.4 + bench-fix-A/B
// chain.  Plan:
//   docs/architecture/phase3m-1c-tx-pump-architecture-plan.md
// =================================================================
//
// Modification history (NereusSDR):
//   2026-04-29 — Phase 3M-1c TX pump redesign — initial
//                 implementation by J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude
//                 Code.
// =================================================================

// no-port-check: NereusSDR-original file.  The Thetis cmbuffs.c /
// cmaster.c citations identify the architectural pattern this class
// mirrors (worker-thread + matching-block-size); no Thetis logic is
// line-for-line ported here.

#include "TxWorkerThread.h"

#include "AudioEngine.h"
#include "TxChannel.h"

#include <QLoggingCategory>
#include <QTimer>

#include <algorithm>
#include <array>
#include <memory>

Q_LOGGING_CATEGORY(lcTxWorker, "nereus.tx.worker")

namespace NereusSDR {

TxWorkerThread::TxWorkerThread(QObject* parent)
    : QThread(parent)
{
    setObjectName(QStringLiteral("TxWorkerThread"));
}

TxWorkerThread::~TxWorkerThread()
{
    // Defensive: stop the pump if RadioModel forgot.  stopPump() is
    // idempotent.  This matches the safe-shutdown invariant required
    // by `cm_main` in cmbuffs.c:151-168 [v2.10.3.13] — the worker
    // must exit before its dependencies are destroyed.
    stopPump();
}

void TxWorkerThread::setTxChannel(TxChannel* ch)
{
    // Caller contract: only valid to swap when the pump is stopped.
    // Live-swap during pump would race onPumpTick's m_txChannel read.
    m_txChannel = ch;
}

void TxWorkerThread::setAudioEngine(AudioEngine* engine)
{
    // Same contract as setTxChannel: only valid when pump stopped.
    m_audioEngine = engine;
}

void TxWorkerThread::startPump()
{
    if (isRunning()) {
        // Idempotent.
        return;
    }
    if (m_txChannel == nullptr || m_audioEngine == nullptr) {
        qCWarning(lcTxWorker)
            << "startPump: missing dependencies (txChannel ="
            << static_cast<const void*>(m_txChannel)
            << ", audioEngine =" << static_cast<const void*>(m_audioEngine)
            << "); pump NOT started.";
        return;
    }
    qCInfo(lcTxWorker) << "startPump: launching worker thread"
                       << "blockFrames=" << kBlockFrames
                       << "intervalMs=" << kPumpIntervalMs;
    QThread::start(QThread::HighPriority);
}

void TxWorkerThread::stopPump()
{
    if (!isRunning()) {
        // Idempotent — also covers the case where startPump never
        // succeeded (missing deps).
        return;
    }
    qCInfo(lcTxWorker) << "stopPump: quitting worker thread";
    QThread::quit();
    // Wait up to 5 seconds — the event loop should exit on the next
    // tick of the timer queue.  The bound is defensive; real-world
    // exit takes <10 ms.
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
    // Construct the QTimer on this thread so its tick fires in this
    // thread's event loop.  Creating the QTimer outside run() and
    // moving it would also work, but the in-run construction matches
    // the pattern used by other QThread subclasses in the codebase.
    //
    // Q_PRECISETIMER: matches the cadence requirement noted in the
    // architecture plan §7 risks table — coarse-timer drift could
    // skew the pump cadence enough to surface SPSC overflow under
    // load.
    //
    // Stack-local std::unique_ptr complies with the no-raw-new
    // CLAUDE.md style guide: timer is destroyed deterministically
    // when run() unwinds (after exec() returns).  Mirror via raw
    // m_pumpTimer for the brief lifetime of run() so onPumpTick
    // null-guards stay symmetric with stopPump's idempotency
    // checks.
    auto timer = std::make_unique<QTimer>();
    m_pumpTimer = timer.get();
    m_pumpTimer->setInterval(kPumpIntervalMs);
    m_pumpTimer->setTimerType(Qt::PreciseTimer);
    QObject::connect(m_pumpTimer, &QTimer::timeout,
                     this, &TxWorkerThread::onPumpTick,
                     Qt::DirectConnection);
    m_pumpTimer->start();

    // Enter the event loop.  Returns when QThread::quit() lands a
    // QEvent::Quit on this thread's queue (from the main thread's
    // stopPump).
    const int rc = QThread::exec();

    // Tear down the timer before the thread exits.  std::unique_ptr
    // does the actual delete on scope exit; null the raw mirror
    // first so any in-flight signal sees nullptr rather than a
    // dangling pointer.
    m_pumpTimer->stop();
    m_pumpTimer = nullptr;
    timer.reset();

    qCInfo(lcTxWorker) << "run: worker thread event loop exited rc=" << rc;
}

void TxWorkerThread::onPumpTick()
{
    // ── Pump tick ──────────────────────────────────────────────────
    //
    // Mirrors the body of Thetis's `cm_main` per cmbuffs.c:151-168
    // [v2.10.3.13]:
    //   - Pull one block-sized chunk from the audio source.
    //   - Run fexchange (TxChannel::driveOneTxBlock owns this side).
    //   - Push to the connection's outbound ring (driveOneTxBlock
    //     owns this side too).
    //
    // Block-size invariant matches Thetis cmaster.c:460-487
    // [v2.10.3.13] — `r1_outsize == xcm_insize == in_size`.  Here:
    //   pullTxMic block == kBlockFrames == TxChannel m_inputBufferSize.
    //
    // Defensive null-guards: in steady-state both pointers are
    // non-null (validated in startPump), but a teardown race could
    // null one transiently.  Skip the tick rather than crash.
    if (m_txChannel == nullptr || m_audioEngine == nullptr) {
        return;
    }

    // thread_local keeps the buffer cache-resident on this thread.
    // 256 floats = 1 KB; trivially fits in L1.
    static thread_local std::array<float, kBlockFrames> buffer;

    // Pull mic samples.  Bus is empty / null / partial → got < kBlockFrames.
    // The zero-fill below lets the silence path "fall out for free":
    // when no PC mic is configured, fexchange2 receives zeros and
    // PostGen TUNE-tone output still produces clean carrier.
    const int got = m_audioEngine->pullTxMic(buffer.data(), kBlockFrames);
    if (got < kBlockFrames) {
        std::fill(buffer.begin() + std::max(0, got), buffer.end(), 0.0f);
    }

    // Drive one fexchange2 cycle.  TxChannel must be on this worker
    // thread (RadioModel arranges that via moveToThread before
    // startPump).  driveOneTxBlock guards against !m_running and
    // !m_connection internally.
    m_txChannel->driveOneTxBlock(buffer.data(), kBlockFrames);
}

} // namespace NereusSDR
