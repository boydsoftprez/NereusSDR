// =================================================================
// src/gui/spectrum/WaterfallTicker.cpp  (NereusSDR-native)
// =================================================================
// 2026-05-25  J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude.
// See WaterfallTicker.h for design rationale.
// =================================================================
#include "WaterfallTicker.h"

namespace NereusSDR {

WaterfallTicker::WaterfallTicker(QObject* parent)
    : QObject(parent)
{
    m_timer.setTimerType(Qt::PreciseTimer);
    m_timer.setInterval(m_periodMs.load());
    m_timer.setSingleShot(false);
    connect(&m_timer, &QTimer::timeout, this, &WaterfallTicker::onTimerTick);
}

WaterfallTicker::~WaterfallTicker() = default;

void WaterfallTicker::setUpdatePeriodMs(int ms)
{
    if (ms < 10) { ms = 10; }
    if (ms > 500) { ms = 500; }
    m_periodMs.store(ms);

    // Hop to worker thread to mutate the QTimer state.  QTimer is not
    // thread-safe; setInterval / start must run on the object's thread.
    QMetaObject::invokeMethod(this, [this, ms]() {
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
        m_timer.start();
    }, Qt::QueuedConnection);
}

void WaterfallTicker::stop()
{
    if (!m_running.exchange(false)) {
        return;  // already stopped
    }
    QMetaObject::invokeMethod(this, [this]() {
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
