#pragma once
// no-port-check: NereusSDR-original application shutdown boundary.
// Modification history (NereusSDR):
//   2026-10-04 — J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.

#include <QApplication>

namespace NereusSDR {

class GuiApplication : public QApplication
{
    Q_OBJECT

public:
    GuiApplication(int& argc, char** argv);
    static bool applicationQuitInProgress();

protected:
    bool event(QEvent* event) override;

private:
    bool m_quitEventInProgress = false;
    bool m_terminalShutdown = false;
};

} // namespace NereusSDR
