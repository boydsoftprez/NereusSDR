// =================================================================
// src/gui/spectrum/WaterfallTicker.cpp  (NereusSDR-native)
// =================================================================
// 2026-05-25  J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude.
// See WaterfallTicker.h for design rationale.
// =================================================================
#include "WaterfallTicker.h"

#include <QThread>

namespace NereusSDR {

WaterfallTicker::WaterfallTicker(QObject* parent)
    : QObject(parent)
{
    // NOTE on threading: `QTimer m_timer;` is a value member, not a
    // QObject child of `this`, so when SpectrumWidget calls
    // moveToThread(workerThread) on us, m_timer stays pinned to the
    // thread it was constructed on (main).  Configuring the timer here
    // is fine -- setTimerType/setInterval/setSingleShot don't start
    // it -- but the first start() must run on m_timer's owning thread.
    // ensureTimerOnCurrentThread() (called from the invokeMethod
    // lambdas below) re-parents m_timer onto the worker the first time
    // start()/setUpdatePeriodMs()/stop() lands there.
    m_timer.setTimerType(Qt::PreciseTimer);
    m_timer.setInterval(m_periodMs.load());
    m_timer.setSingleShot(false);
    connect(&m_timer, &QTimer::timeout, this, &WaterfallTicker::onTimerTick);
}

WaterfallTicker::~WaterfallTicker() = default;

void WaterfallTicker::moveToWorkerThread(QThread* target)
{
    // Order matters: m_timer must be moved BEFORE this, because Qt
    // requires the move to be issued from the object's current thread
    // and m_timer currently shares this object's thread (the caller's
    // thread).  Once we moveToThread(target) on `this`, future calls
    // would be issued from `this`'s new thread, which is wrong for
    // m_timer until it has been moved too.  Doing both here, in order,
    // keeps the two QObjects co-located and avoids the "Cannot move to
    // target thread" warning that fired before this hop existed.
    m_timer.moveToThread(target);
    moveToThread(target);
}

void WaterfallTicker::ensureTimerOnCurrentThread()
{
    // Belt-and-braces safety net.  After moveToWorkerThread() this
    // should always be a no-op, but if a caller ever forgets to use
    // moveToWorkerThread() we'll log a warning (Qt will print one of
    // its own) rather than silently swallowing the failed move.
    if (m_timer.thread() != QThread::currentThread()) {
        m_timer.moveToThread(QThread::currentThread());
    }
}

void WaterfallTicker::setUpdatePeriodMs(int ms)
{
    if (ms < 10) { ms = 10; }
    if (ms > 500) { ms = 500; }
    m_periodMs.store(ms);

    // Hop to worker thread to mutate the QTimer state.  QTimer is not
    // thread-safe; setInterval / start must run on the object's thread.
    QMetaObject::invokeMethod(this, [this, ms]() {
        ensureTimerOnCurrentThread();
        m_timer.setInterval(ms);
        if (m_running.load()) {
            m_timer.start();
        }
    }, Qt::QueuedConnection);
}

void WaterfallTicker::start()
{
    if (m_running.exchange(true)) {
        return;  // already running
    }
    QMetaObject::invokeMethod(this, [this]() {
        ensureTimerOnCurrentThread();
        m_timer.start();
    }, Qt::QueuedConnection);
}

void WaterfallTicker::stop()
{
    if (!m_running.exchange(false)) {
        return;  // already stopped
    }
    QMetaObject::invokeMethod(this, [this]() {
        ensureTimerOnCurrentThread();
        m_timer.stop();
    }, Qt::QueuedConnection);
}

void WaterfallTicker::onTimerTick()
{
    // Emit on the worker thread; Qt routes via auto-queued connection
    // to the SpectrumWidget slot on the main thread.
    emit tick();
}

} // namespace NereusSDR
