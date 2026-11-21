#pragma once
// NereusSDR-original start/exit script runner.
//
// Mirrors Thetis's Scripts tab: a batch file runs hidden when the program
// starts (for example the Denkovi relay that powers the SDR) and another when
// it exits. The start script is launched as early as possible; auto-connect
// then waits for it to finish plus the configured delay, so the radio has
// time to boot before NereusSDR talks to it.
#include <QObject>
#include <QString>
#include <functional>

class QProcess;

namespace NereusSDR {

class ScriptRunner : public QObject {
    Q_OBJECT
public:
    static ScriptRunner& instance();

    // %LOCALAPPDATA%/NereusSDR/helpers/SDR Scripts
    static QString scriptsDirectory();
    static QString defaultStartScript();
    static QString defaultStopScript();

    // Copies the shipped SDR Scripts folder (scripts + Denkovi tool) into
    // scriptsDirectory(). Existing files are never overwritten, as Thetis
    // keeps a user-edited script.
    static bool seed(QString* error = nullptr);
    // Removes the per-user SDR Scripts folder. The shipped copy beside the app
    // is left alone, so enabling a script again re-seeds it.
    static bool removeInstalled(QString* error = nullptr);

    static bool startEnabled();
    static bool stopEnabled();
    static QString startScriptPath();
    static QString stopScriptPath();
    static int powerOnDelaySeconds();

    // Launches the start script hidden. Safe to call more than once.
    void runStartScript();
    // Runs the exit script and waits up to 10 s for it, like Thetis.
    void runStopScript();

    // True when the caller must not auto-connect yet: the callback runs after
    // the start script has exited and the configured delay has elapsed. False
    // when no start script is configured, so the caller proceeds normally.
    bool holdAutoConnect(QObject* context, std::function<void()> callback);

signals:
    void startScriptFinished();

private:
    explicit ScriptRunner(QObject* parent = nullptr);
    bool launchStartScript();
    void notifyStartScriptFinished();

    QProcess* m_startScript = nullptr;
    bool m_startLaunched = false;
    bool m_finishedNotified = false;
};

} // namespace NereusSDR
