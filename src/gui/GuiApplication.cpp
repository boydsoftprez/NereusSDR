// no-port-check: NereusSDR-original application shutdown boundary.
// Modification history (NereusSDR):
//   2026-10-04 — J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-22 - J.J. Boyd (KG4VCF), AI-assisted via Claude Code: a
//                 main-thread stall trace (slow event deliveries and a
//                 lateness heartbeat) under NEREUS_TRACE_MAIN_STALLS=1;
//                 diagnostics only, capped at 40 lines a 10 s window.

#include "GuiApplication.h"

#include "core/LogCategories.h"

#include <QEvent>
#include <QMetaEnum>
#include <QPointer>
#include <QScopedValueRollback>
#include <QThread>
#include <QTime>
#include <QTimer>

namespace NereusSDR {

namespace {

// A delivery this long or longer logs a MAIN_STALL line.
constexpr qint64 kMainStallLogNs = 12'000'000;
// The lateness heartbeat's period, and how late past it logs MAIN_LATE.
constexpr int kStallHeartbeatMs = 5;
constexpr qint64 kStallHeartbeatLateNs = 15'000'000;

QString stallWallTime()
{
    return QTime::currentTime().toString(QStringLiteral("hh:mm:ss.zzz"));
}

QString eventTypeName(QEvent::Type type)
{
    const char* key = QMetaEnum::fromType<QEvent::Type>().valueToKey(int(type));
    return key ? QString::fromLatin1(key) : QString::number(int(type));
}

// Counts a delivery's nesting for as long as it runs.
struct StallDepthGuard {
    explicit StallDepthGuard(int& depth)
        : m_depth(depth)
    {
        ++m_depth;
    }
    ~StallDepthGuard() { --m_depth; }
    StallDepthGuard(const StallDepthGuard&) = delete;
    StallDepthGuard& operator=(const StallDepthGuard&) = delete;

    int& m_depth;
};

} // namespace

GuiApplication::GuiApplication(int& argc, char** argv)
    : QApplication(argc, argv)
{
    // Register before window shutdown-save handlers. Qt 6.4 emits this
    // from execCleanup after the Quit event returns; Qt 6.11 emits in exit.
    // https://github.com/qt/qtbase/blob/v6.4.0/src/corelib/kernel/qcoreapplication.cpp#L1362
    // https://github.com/qt/qtbase/blob/v6.11.0/src/corelib/kernel/qcoreapplication.cpp#L1505
    connect(this, &QCoreApplication::aboutToQuit, this, [this] {
        m_terminalShutdown = true;
    });

    // Read once: the main-thread stall trace (diagnostics only).
    m_traceMainStalls = qEnvironmentVariableIntValue("NEREUS_TRACE_MAIN_STALLS") == 1;
    if (m_traceMainStalls) {
        // Catches stalls that never pass through notify (macOS run-loop
        // observers, Core Animation commits): the timer fires late.
        m_stallHeartbeat = new QTimer(this);
        m_stallHeartbeat->setTimerType(Qt::PreciseTimer);
        m_stallHeartbeat->setInterval(kStallHeartbeatMs);
        connect(m_stallHeartbeat, &QTimer::timeout, this, [this] { onStallHeartbeat(); });
        m_stallHeartbeatClock.start();
        m_stallTraceWindow.start();
        m_stallHeartbeat->start();
    }
}

bool GuiApplication::notify(QObject* receiver, QEvent* event)
{
    if (!m_traceMainStalls) {
        return QApplication::notify(receiver, event);
    }
    return notifyTraced(receiver, event);
}

bool GuiApplication::notifyTraced(QObject* receiver, QEvent* event)
{
    if (receiver == nullptr || event == nullptr
        || QThread::currentThread() != thread()
        || (receiver == m_stallHeartbeat && event->type() == QEvent::Timer)) {
        return QApplication::notify(receiver, event);
    }

    // Read what may not survive the delivery (the receiver can be deleted
    // inside it, and a posted event is freed after it).
    const QEvent::Type type = event->type();
    const char* className = receiver->metaObject()->className();
    int timerId = -1;
    if (type == QEvent::Timer) {
        timerId = static_cast<QTimerEvent*>(event)->timerId();
    }
    int timerInterval = -1;
    if (const auto* timer = qobject_cast<const QTimer*>(receiver)) {
        timerInterval = timer->interval();
    }
    const QPointer<QObject> guard(receiver);

    int depth = 0;
    bool result = false;
    qint64 elapsedNs = 0;
    {
        const StallDepthGuard depthGuard(m_stallDepth);
        depth = m_stallDepth;
        QElapsedTimer clock;
        clock.start();
        result = QApplication::notify(receiver, event);
        elapsedNs = clock.nsecsElapsed();
    }

    if (elapsedNs < kMainStallLogNs) {
        return result;
    }

    QString name = QStringLiteral("-");
    QString parent = QStringLiteral("-");
    if (guard) {
        if (!guard->objectName().isEmpty()) {
            name = guard->objectName();
        }
        if (const QObject* owner = guard->parent()) {
            parent = QString::fromLatin1(owner->metaObject()->className());
        }
    }
    QString line = QStringLiteral("MAIN_STALL ms=%1 depth=%2 type=%3 receiver=%4 name=%5 parent=%6 wall=%7")
        .arg(elapsedNs / 1e6, 0, 'f', 1)
        .arg(depth)
        .arg(eventTypeName(type), QString::fromLatin1(className), name, parent, stallWallTime());
    if (timerId >= 0) {
        line += QStringLiteral(" timerId=%1").arg(timerId);
    }
    if (timerInterval >= 0) {
        line += QStringLiteral(" interval=%1").arg(timerInterval);
    }
    logStallTraceLine(line);
    return result;
}

void GuiApplication::onStallHeartbeat()
{
    const qint64 sinceLastNs = m_stallHeartbeatClock.nsecsElapsed();
    m_stallHeartbeatClock.restart();
    const qint64 lateNs = sinceLastNs - qint64(kStallHeartbeatMs) * 1'000'000;
    if (lateNs >= kStallHeartbeatLateNs) {
        logStallTraceLine(QStringLiteral("MAIN_LATE late=%1 wall=%2")
            .arg(lateNs / 1e6, 0, 'f', 1)
            .arg(stallWallTime()));
    } else {
        // The heartbeat rolls a quiet window too, so its count is not held
        // back until the next slow delivery.
        rollStallTraceWindow();
    }
}

void GuiApplication::rollStallTraceWindow()
{
    if (m_stallTraceWindow.nsecsElapsed() < m_stallTraceWindowNs) {
        return;
    }
    if (m_stallTraceSuppressed > 0) {
        qCInfo(lcApp).noquote() << QStringLiteral("MAIN_STALL_SUPPRESSED count=%1")
            .arg(m_stallTraceSuppressed);
    }
    m_stallTraceLines = 0;
    m_stallTraceSuppressed = 0;
    m_stallTraceWindow.restart();
}

void GuiApplication::logStallTraceLine(const QString& line)
{
    // As the remote row trace caps its events: a slow frame repeating every
    // 16 ms would otherwise log a line a frame and disturb what it measures.
    rollStallTraceWindow();
    if (m_stallTraceLines >= kStallTraceLineCap) {
        ++m_stallTraceSuppressed;
        return;
    }
    ++m_stallTraceLines;
    qCInfo(lcApp).noquote() << line;
}

bool GuiApplication::applicationQuitInProgress()
{
    const auto* application = qobject_cast<GuiApplication*>(QCoreApplication::instance());
    return application
        && (application->m_quitEventInProgress || application->m_terminalShutdown);
}

bool GuiApplication::event(QEvent* event)
{
    if (event->type() == QEvent::Quit) {
        // QApplication closes top-level widgets before deciding whether Quit
        // succeeds. Installed event filters run before this handler, so station
        // preflight retains its veto. Roll back even for rejected/nested Quit.
        // https://github.com/qt/qtbase/blob/v6.4.0/src/widgets/kernel/qapplication.cpp#L1631
        // https://github.com/qt/qtbase/blob/v6.11.0/src/widgets/kernel/qapplication.cpp#L1678
        QScopedValueRollback<bool> guard(m_quitEventInProgress, true);
        return QApplication::event(event);
    }
    return QApplication::event(event);
}

} // namespace NereusSDR
