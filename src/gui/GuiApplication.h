#pragma once
// no-port-check: NereusSDR-original application shutdown boundary.
// Modification history (NereusSDR):
//   2026-10-04 — J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-22 - J.J. Boyd (KG4VCF), AI-assisted via Claude Code: a
//                 main-thread stall trace (slow event deliveries and a
//                 lateness heartbeat) under NEREUS_TRACE_MAIN_STALLS=1;
//                 diagnostics only, capped at 40 lines a 10 s window.

#include <QApplication>
#include <QElapsedTimer>

class QTimer;

namespace NereusSDR {

class GuiApplication : public QApplication
{
    Q_OBJECT

public:
    GuiApplication(int& argc, char** argv);
    static bool applicationQuitInProgress();

    // With NEREUS_TRACE_MAIN_STALLS=1 (read once at construction), times
    // each event delivery on the GUI thread and logs a MAIN_STALL line for
    // one of 12 ms or more. Off, it is a straight call to QApplication.
    bool notify(QObject* receiver, QEvent* event) override;

    // MAIN_STALL and MAIN_LATE share one budget: at most this many lines a
    // window, then a MAIN_STALL_SUPPRESSED count=<n> line as the window rolls.
    static constexpr int kStallTraceLineCap = 40;
    static constexpr qint64 kStallTraceWindowNs = 10'000'000'000LL;
    // Tests shorten the window to see it roll.
    void setStallTraceWindowNsForTest(qint64 windowNs) { m_stallTraceWindowNs = windowNs; }

protected:
    bool event(QEvent* event) override;

private:
    bool notifyTraced(QObject* receiver, QEvent* event);
    void onStallHeartbeat();
    void rollStallTraceWindow();
    void logStallTraceLine(const QString& line);

    bool m_traceMainStalls = false;
    int m_stallDepth = 0;
    QTimer* m_stallHeartbeat = nullptr;
    QElapsedTimer m_stallHeartbeatClock;
    QElapsedTimer m_stallTraceWindow;
    qint64 m_stallTraceWindowNs = kStallTraceWindowNs;
    int m_stallTraceLines = 0;
    int m_stallTraceSuppressed = 0;
    bool m_quitEventInProgress = false;
    bool m_terminalShutdown = false;
};

} // namespace NereusSDR
