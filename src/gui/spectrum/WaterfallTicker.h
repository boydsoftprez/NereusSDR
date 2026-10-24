// =================================================================
// src/gui/spectrum/WaterfallTicker.h  (NereusSDR-native)
// =================================================================
// 2026-05-25  J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude.
// 2026-10-21  J.J. Boyd (KG4VCF), AI-assisted via Claude Code: the
//             period is held in nanoseconds and each tick is due at a
//             fixed multiple of it, so a remote pan can tick at the Core's
//             row period (66.67 ms for two frames at 30 fps) with no drift.
// 2026-10-22  J.J. Boyd (KG4VCF), AI-assisted via Claude Code: exported
//             from the GUI DLL (NEREUS_GUI_EXPORT), so a connect to tick()
//             from another image works on Windows.
//
// Dedicated worker-thread ticker for waterfall row pushes.  Isolates
// the timer from main-thread state so a momentarily-busy GUI thread
// can no longer delay the push cadence.
//
// Bench symptom 2026-05-25 KG4VCF: even after PreciseTimer + push-
// timer + thread-priority elevation, the waterfall scroll still had
// occasional ~1 Hz stutters.  Diagnosis pointed at main-thread
// availability: the PreciseTimer was firing on the main thread, so
// any main-thread delay (focus event, layout pass, etc.) delayed
// the push by the same amount.
//
// This class lives on its own QThread.  The QTimer fires there.
// Each tick emits tick() as an auto-queued signal to the consumer
// (SpectrumWidget) on the main thread.  The consumer's queued slot
// processes the next time the main thread reaches its event loop;
// no individual paint, layout, or signal can delay the timer's
// internal cadence, so multiple delayed tickets queue up and drain
// in a burst when the main thread frees -- visibly cleaner than a
// long pause followed by a single push.
//
// API:
//   * setUpdatePeriodMs(int)  -- thread-safe; reschedules the timer.
//   * setUpdatePeriodNs(qint64) -- the same, for a period that is not a
//                                  whole number of milliseconds.
//   * start() / stop()        -- thread-safe; starts/stops the timer.
//   * tick()                  -- signal, fires on worker thread.
//
// The ticker does NOT own waterfall texture state -- that stays on
// the main thread.  This is the timing-isolation half of "Option A";
// the texture-isolation half is a separate refactor (see notes in
// design doc).
// =================================================================
#pragma once

#include "gui/NereusGuiExport.h"

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>
#include <atomic>

namespace NereusSDR {

class NEREUS_GUI_EXPORT WaterfallTicker : public QObject {
    Q_OBJECT
public:
    explicit WaterfallTicker(QObject* parent = nullptr);
    ~WaterfallTicker() override;

signals:
    // Emitted on the worker thread at every PreciseTimer tick.
    // Connect to SpectrumWidget's slot with Qt::QueuedConnection so the
    // handler runs on the main thread when the event loop is free.
    void tick();

public:
    // Move both *this* and m_timer onto the given worker thread.  Must
    // be called from the calling-construction thread BEFORE
    // m_workerThread->start() so that m_timer (a value member that
    // moveToThread() would otherwise strand on the construction
    // thread) ends up on the same thread as *this*.  After this call
    // the worker thread owns both objects and the existing
    // m_timer.timeout -> onTimerTick DirectConnection continues to
    // work on the worker thread.
    void moveToWorkerThread(QThread* target);

    // The period's bounds: 10 ms to 500 ms, as setUpdatePeriodMs() has
    // always clamped.
    static constexpr qint64 kMinPeriodNs = 10'000'000;
    static constexpr qint64 kMaxPeriodNs = 500'000'000;

    // Milliseconds from elapsedNs (time since the cadence's anchor) until
    // tick number nextTick falls due at nextTick * periodNs. Rounded up,
    // so a tick never fires before its slot; 0 when it is already due.
    // Each slot is a fixed multiple of the period, so the rounding never
    // accumulates: 15 ticks of 66.67 ms span 1000 ms.
    static int msUntilTick(qint64 elapsedNs, qint64 nextTick, qint64 periodNs);

    qint64 updatePeriodNs() const { return m_periodNs.load(); }

public slots:
    // Reschedule the timer.  Called from any thread; the internal
    // QMetaObject::invokeMethod hop ensures the start() lands on the
    // worker thread that owns m_timer.
    void setUpdatePeriodMs(int ms);
    // The period in nanoseconds (a remote pan's row period, which is
    // rarely whole milliseconds). Clamped to kMinPeriodNs..kMaxPeriodNs;
    // the same period again leaves the cadence alone.
    void setUpdatePeriodNs(qint64 ns);

    // Start / stop the timer.  Idempotent.
    void start();
    void stop();

private slots:
    void onTimerTick();   // runs on worker thread

private:
    // Lazily re-parent m_timer (a value member, so moveToThread on
    // `this` does not bring it along) onto the worker thread the first
    // time a control slot lands here.  Must be called only from a
    // worker-thread context (e.g. inside the invokeMethod lambdas in
    // start / stop / setUpdatePeriodMs).
    void ensureTimerOnCurrentThread();
    // Worker thread only: start the cadence again from now, and arm the
    // single-shot timer for the next tick's slot.
    void restartCadence();
    void scheduleNextTick();

    QTimer              m_timer;
    // Worker thread only: the cadence's anchor and the ticks since it.
    QElapsedTimer       m_anchor;
    qint64              m_ticksSinceAnchor{0};
    std::atomic<qint64> m_periodNs{33'000'000};
    std::atomic<bool>   m_running{false};
};

} // namespace NereusSDR
