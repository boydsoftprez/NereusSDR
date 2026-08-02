#include "gui/MainWindow.h"
#include "gui/styles/AppTheme.h"
#include "core/AppSettings.h"
#include "core/AudioDeviceConfig.h"
#include "core/BuildIdentity.h"
#include "core/CoreInit.h"
#include "core/MacMicPermission.h"
#include "core/audio/RealtimeAudioPriority.h"
#include "core/RadioConnection.h"
#include "core/mmio/ExternalVariableEngine.h"

// Generated into the build tree by cmake/NereusBuildTag.cmake, once per
// build, so NEREUSSDR_BUILD_TAG names the commit actually being compiled
// instead of whatever HEAD happened to be at the last cmake configure.
//
// This is the only translation unit that includes it, and that is on
// purpose: it is compiled into the application target alone, so a new commit
// rebuilds this file and relinks this binary, and leaves the test suite (which
// links the NereusCore object library) untouched. See CMakeLists.txt
// section "Build tag" and src/core/BuildIdentity.h.
#include "NereusBuildTag.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QMetaObject>
#include <csignal>
#include <QCommandLineParser>
#include <QIcon>
#include <QStyleFactory>
#include <QFile>
#include <QStandardPaths>
#include <QStringList>

// Parse --profile <name> out of argv *before* constructing QApplication so
// AppSettings can pin the right path on first access. QCommandLineParser
// wants a QCoreApplication instance, so we do a cheap manual scan here and
// re-parse properly inside main() once the app is built (for --help / error
// diagnostics).
//
// Issue #100 — multiple NereusSDR instances against different radios.
static QString extractProfileFromArgv(int argc, char* argv[])
{
    for (int i = 1; i < argc; ++i) {
        const QString a = QString::fromLocal8Bit(argv[i]);
        if (a == QLatin1String("--profile") || a == QLatin1String("-p")) {
            if (i + 1 < argc) {
                return QString::fromLocal8Bit(argv[i + 1]);
            }
        } else if (a.startsWith(QLatin1String("--profile="))) {
            return a.mid(QLatin1String("--profile=").size());
        }
    }
    return {};
}

int main(int argc, char* argv[])
{
    // Hand the build stamp to the core accessor before anything can build a
    // window title from it. Empty on release artifacts, in which case the
    // title stays exactly as it was.
    NereusSDR::BuildIdentity::setBuildTag(
        QString::fromUtf8(NEREUSSDR_BUILD_TAG));

    // Resolve profile name first — downstream path lookups (AppSettings,
    // log dir, pre-QApplication UI scale read) all consult it.
    const QString earlyProfile = extractProfileFromArgv(argc, argv);
    if (!earlyProfile.isEmpty()) {
        if (NereusSDR::AppSettings::isValidProfileName(earlyProfile)) {
            NereusSDR::AppSettings::setProfileOverride(earlyProfile);
        } else {
            fprintf(stderr,
                    "NereusSDR: ignoring invalid --profile '%s' "
                    "(allowed: [A-Za-z0-9_-]+)\n",
                    earlyProfile.toLocal8Bit().constData());
        }
    }
    const QString activeProfile = NereusSDR::AppSettings::profileOverride();

    // Apply saved UI scale factor BEFORE QApplication is created.
    {
        const QString settingsPath =
            NereusSDR::AppSettings::resolveSettingsPath(activeProfile);
        QFile f(settingsPath);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QByteArray data = f.readAll();
            QByteArray tag = "<UiScalePercent>";
            int idx = data.indexOf(tag);
            if (idx >= 0) {
                idx += tag.size();
                int end = data.indexOf('<', idx);
                if (end > idx) {
                    int pct = data.mid(idx, end - idx).trimmed().toInt();
                    if (pct > 0 && pct != 100) {
                        qputenv("QT_SCALE_FACTOR", QByteArray::number(pct / 100.0, 'f', 2));
                    }
                }
            }
        }
    }

    QApplication app(argc, argv);
    app.setApplicationName("NereusSDR");
    app.setApplicationVersion(NEREUSSDR_VERSION);
    app.setOrganizationName("NereusSDR");
    app.setWindowIcon(QIcon(":/icons/NereusSDR.png"));

    // 2026-05-25 KG4VCF bench fix: elevate the main GUI thread to
    // USER_INTERACTIVE QoS so heavy user-initiated background work
    // (parallel compiles, mdworker indexing, Time Machine snapshots,
    // etc.) does not preempt the Qt event loop and produce visibly
    // choppy spectrum / waterfall rendering.  The audio DSP thread
    // already gets a stronger elevation (see RxDspWorker::onThreadStarted)
    // but the GUI thread runs the spectrum paint cycle and was still
    // being preempted at DEFAULT QoS.  Bench symptom: "whole program
    // stutters when a build happens".
    //
    // Cross-platform via src/core/audio/RealtimeAudioPriority.cpp:
    //   macOS:   pthread_set_qos_class_self_np(USER_INTERACTIVE)
    //   Linux:   nice(-5)  (soft-fail without privilege)
    //   Windows: SetThreadPriority(HIGHEST)
    NereusSDR::elevateGuiMainThreadPriority();

    // 2026-05-22 bench-finding: pkill / kill / system shutdown sends SIGTERM
    // by default; the OS terminates the process without giving Qt a chance
    // to run aboutToQuit handlers.  Without translation, this skips
    // RadioConnection::disconnect, the radio gateware never sees run=0, and
    // some community P2 firmwares require power-cycle to recover.  Install
    // POSIX signal handlers that convert SIGTERM / SIGINT into
    // QApplication::quit, which fires aboutToQuit and runs the graceful
    // disconnect path.  SIGKILL (kill -9, Activity Monitor "Force Quit") is
    // uncatchable — power-cycle is still the only recovery there.
    //
    // R1 Task 9: resolved by giving src/server_main.cpp its own SIGTERM/
    // SIGINT pair rather than sharing this one -- same QMetaObject::
    // invokeMethod + Qt::QueuedConnection pattern, adapted to that file's
    // simpler global-pointer structure. See task-9-report.md for why a
    // shared call was not worth it (a daemon's signal set may still grow a
    // SIGHUP handler for config reload that this GUI pair never will).
    std::signal(SIGTERM, [](int) {
        // Async-signal-safe: only QCoreApplication::quit() is approximately
        // safe to call.  Internally it just sets an atomic flag the event
        // loop polls.
        if (QCoreApplication::instance()) {
            QMetaObject::invokeMethod(QCoreApplication::instance(),
                                      "quit", Qt::QueuedConnection);
        }
    });
    std::signal(SIGINT, [](int) {
        if (QCoreApplication::instance()) {
            QMetaObject::invokeMethod(QCoreApplication::instance(),
                                      "quit", Qt::QueuedConnection);
        }
    });

    // Trigger the macOS microphone permission dialog deterministically
    // (issue #203). The OS only prompts when something actually engages
    // TCC; relying on PortAudio's CoreAudio backend to do so is unreliable
    // on machines without a built-in mic, so call AVCaptureDevice directly.
    NereusSDR::requestMicrophonePermission();

    // Re-parse properly so --help / --version / unknown options surface
    // via Qt's standard machinery. The earlyProfile pass above already
    // pinned AppSettings; this second pass is purely for user-facing UX.
    {
        QCommandLineParser parser;
        parser.setApplicationDescription(
            QStringLiteral("NereusSDR — cross-platform OpenHPSDR client."));
        parser.addHelpOption();
        parser.addVersionOption();
        QCommandLineOption profileOpt(
            QStringList() << QStringLiteral("p") << QStringLiteral("profile"),
            QStringLiteral(
                "Run in an isolated profile (separate settings + logs). "
                "Lets two instances drive two radios without clobbering "
                "each other. Name must match [A-Za-z0-9_-]+."),
            QStringLiteral("name"));
        parser.addOption(profileOpt);
        parser.process(app);
    }

    // Fusion style as a clean cross-platform base, then layer the
    // NereusSDR dark palette + minimal baseline QSS on top so every
    // widget (including ones without their own stylesheet) renders
    // with the dark theme. Without this, Linux/Ubuntu Yaru leaks
    // light-grey backgrounds and orange Highlight through into popups,
    // group-box titles, tooltips, and any unstyled control.
    app.setStyle(QStyleFactory::create("Fusion"));
    NereusSDR::applyDarkPalette(app);
    NereusSDR::applyAppBaselineQss(app);

    // Register custom metatypes for cross-thread signal/slot connections.
    // R1 Task 9: src/server_main.cpp registers this same pair itself
    // (duplicated, not moved here or folded into CoreInit -- see
    // task-9-report.md); this is now the first of two call sites.
    qRegisterMetaType<NereusSDR::RadioConnectionError>();
    qRegisterMetaType<NereusSDR::AudioDeviceConfig>();

    // Shared startup sequence (R1 Task 8): loads AppSettings, applies every
    // one-shot settings-schema migration, restores LogManager's category
    // toggles, and installs file-backed logging. `activeProfile` is the
    // same already-resolved name pinned into AppSettings::setProfileOverride()
    // above; CoreInit::initialize() only uses it to resolve the log
    // directory, it does not re-pin the override itself. See
    // src/core/CoreInit.h for the full contract and its idempotency guard.
    NereusSDR::CoreInit::initialize(activeProfile);

    qDebug() << "Starting NereusSDR" << app.applicationVersion();
    if (!activeProfile.isEmpty()) {
        const QString logDir = NereusSDR::AppSettings::resolveConfigDir(activeProfile);
        qDebug() << "Profile:" << activeProfile
                 << "config dir:" << logDir;
    }

    // Phase 3G-6 block 5: bring up the MMIO subsystem so persisted
    // endpoints (under AppSettings MmioEndpoints/<guid>/*) start
    // their transport workers before the main window is shown.
    NereusSDR::ExternalVariableEngine::instance().init();

    NereusSDR::MainWindow window;
    window.show();

    const int rc = app.exec();

    // Graceful shutdown so worker threads drain before the engine
    // singleton is destroyed.
    NereusSDR::ExternalVariableEngine::instance().shutdown();

    // Uninstalls the custom message handler and closes the log file that
    // CoreInit::initialize() installed above. See src/core/CoreInit.cpp
    // for why the handler teardown has to be safe even if Qt logs
    // something between here and its own thread-storage teardown.
    NereusSDR::CoreInit::shutdown();
    return rc;
}
