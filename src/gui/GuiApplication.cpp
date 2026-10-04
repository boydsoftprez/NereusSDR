// no-port-check: NereusSDR-original application shutdown boundary.
// Modification history (NereusSDR):
//   2026-10-04 — J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.

#include "GuiApplication.h"

#include <QEvent>
#include <QScopedValueRollback>

namespace NereusSDR {

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
