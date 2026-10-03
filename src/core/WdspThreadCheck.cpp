// no-port-check: NereusSDR-original.
// =================================================================
// src/core/WdspThreadCheck.cpp  (NereusSDR)
// =================================================================
// See WdspThreadCheck.h. NereusSDR-original; no upstream logic.
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code (R-R3-39).
// =================================================================

#include "core/WdspThreadCheck.h"

#if defined(NEREUS_BUILD_TESTS) || !defined(NDEBUG)
#include "core/LogCategories.h"
#include "core/wdsp_api.h"

#include <QAbstractEventDispatcher>
#include <QMetaObject>
#include <QThread>

#include <atomic>
#endif

namespace NereusSDR::WdspThreadCheck {

#if defined(NEREUS_BUILD_TESTS) || !defined(NDEBUG)

namespace {

// The event loop's native thread ID (nullptr: not counting). Compared with
// QThread::currentThreadId(), which, unlike QThread::currentThread(), never
// creates a Qt object for a WDSP worker or flush thread.
std::atomic<Qt::HANDLE> g_eventLoopThread{nullptr};
std::atomic<quint64> g_entries{0};
// Moves on every install() and uninstall(), so a deferred install that
// arrives after a later call does nothing.
std::atomic<quint64> g_generation{0};

#ifdef HAVE_WDSP
void callerCheckHook(int channel, int kind)
{
    Q_UNUSED(channel);
    Q_UNUSED(kind);
    const Qt::HANDLE eventLoop = g_eventLoopThread.load(std::memory_order_acquire);
    if (eventLoop != nullptr && QThread::currentThreadId() == eventLoop) {
        g_entries.fetch_add(1, std::memory_order_relaxed);
    }
}
#endif

} // namespace

void install(const QThread* eventLoop)
{
    if (eventLoop == nullptr) {
        return;
    }
    const quint64 generation = g_generation.fetch_add(1, std::memory_order_acq_rel) + 1;
    g_eventLoopThread.store(nullptr, std::memory_order_release);
    g_entries.store(0, std::memory_order_relaxed);
#ifdef HAVE_WDSP
    WDSPSetCallerCheckHook(&callerCheckHook);
#endif
    if (eventLoop == QThread::currentThread()) {
        g_eventLoopThread.store(QThread::currentThreadId(), std::memory_order_release);
        return;
    }
    // Learn the event loop's thread ID on that thread, through its event
    // dispatcher (a QObject that lives there).
    QAbstractEventDispatcher* dispatcher =
        QAbstractEventDispatcher::instance(const_cast<QThread*>(eventLoop));
    if (dispatcher == nullptr) {
        qCWarning(lcDsp) << "WDSP thread check: the event loop's thread has no event"
                            " loop yet; nothing is counted";
        return;
    }
    QMetaObject::invokeMethod(
        dispatcher,
        [generation]() {
            if (g_generation.load(std::memory_order_acquire) == generation) {
                g_eventLoopThread.store(QThread::currentThreadId(),
                                        std::memory_order_release);
            }
        },
        Qt::QueuedConnection);
}

void uninstall()
{
    g_generation.fetch_add(1, std::memory_order_acq_rel);
#ifdef HAVE_WDSP
    WDSPSetCallerCheckHook(nullptr);
#endif
    g_eventLoopThread.store(nullptr, std::memory_order_release);
}

quint64 eventLoopEntries() noexcept
{
    return g_entries.load(std::memory_order_relaxed);
}

#else // release build: nothing is compiled in

void install(const QThread* eventLoop)
{
    Q_UNUSED(eventLoop);
}

void uninstall()
{
}

quint64 eventLoopEntries() noexcept
{
    return 0;
}

#endif

} // namespace NereusSDR::WdspThreadCheck
