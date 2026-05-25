// =================================================================
// src/gui/spectrum/WaterfallTicker.h  (NereusSDR-native)
// =================================================================
// 2026-05-25  J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude.
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
//   * start() / stop()        -- thread-safe; starts/stops the timer.
//   * tick()                  -- signal, fires on worker thread.
//
// The ticker does NOT own waterfall texture state -- that stays on
// the main thread.  This is the timing-isolation half of "Option A";
// the texture-isolation half is a separate refactor (see notes in
// design doc).
// =================================================================
#pragma once

#include <QObject>
#include <QTimer>
#include <atomic>

namespace NereusSDR {

class WaterfallTicker : public QObject {
    Q_OBJECT
public:
    explicit WaterfallTicker(QObject* parent = nullptr);
    ~WaterfallTicker() override;

signals:
    // Emitted on the worker thread at every PreciseTimer tick.
    // Connect to SpectrumWidget's slot with Qt::QueuedConnection so the
    // handler runs on the main thread when the event loop is free.
    void tick();

public slots:
    // Reschedule the timer.  Called from any thread; the internal
    // QMetaObject::invokeMethod hop ensures the start() lands on the
    // worker thread that owns m_timer.
    void setUpdatePeriodMs(int ms);

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

    QTimer            m_timer;
    std::atomic<int>  m_periodMs{33};
    std::atomic<bool> m_running{false};
};

} // namespace NereusSDR
