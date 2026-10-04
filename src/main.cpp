#include "gui/GuiConnectionController.h"
#include "gui/GuiApplication.h"
#include "gui/styles/AppTheme.h"
#include "core/AppSettings.h"
#include "core/AudioDeviceConfig.h"
#include "core/BuildIdentity.h"
#include "core/CoreInit.h"
#include "core/MacMicPermission.h"
#include "core/audio/RealtimeAudioPriority.h"
#include "core/RadioConnection.h"
#include "core/mmio/ExternalVariableEngine.h"
#include "core/station/StationHandover.h"

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
#if defined(Q_OS_MAC)
#include "gui/QtCocoaAccessibilityOwnershipGuard.h"
#include <QLoggingCategory>
Q_LOGGING_CATEGORY(lcMainAccessibility, "nereus.gui.accessibility")
#endif
#include <QCommandLineOption>
#include <QMetaObject>
#include <csignal>
#include <QCommandLineParser>
#include <QIcon>
#include <QMessageBox>
#include <QPushButton>
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

    NereusSDR::GuiApplication app(argc, argv);
#if defined(Q_OS_MAC)
    QString accessibilityRejection;
    if (!installQtCocoaAccessibilityOwnershipGuard(&accessibilityRejection)) {
        qCWarning(lcMainAccessibility).noquote() << "NereusSDR startup refused:" << accessibilityRejection;
        return EXIT_FAILURE;
    }
#endif
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
    //
    // Remote-daemon R2 Task 20: --station and --token are read here rather
    // than in the early argv scan, because nothing they affect happens
    // before this point. The profile scan has to be early (AppSettings
    // resolves its path on first access); the station does not.
    NereusSDR::StationStartupRequest request;
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

        QCommandLineOption stationOpt(
            QStringLiteral("station"),
            QStringLiteral(
                "Drive a radio owned by a NereusSDR Core instead of one "
                "attached to this machine. Takes a wss:// (or ws://) URL. "
                "Without this, use the saved connection selection. "
                "Use --local to select this computer's Core."),
            QStringLiteral("wss://host:port"));
        parser.addOption(stationOpt);

        QCommandLineOption tokenOpt(
            QStringLiteral("token"),
            QStringLiteral(
                "Shared token for --station, as printed by nereusd on its "
                "first run. Overrides the selected Core's saved token."),
            QStringLiteral("token"));
        parser.addOption(tokenOpt);

        QCommandLineOption fingerprintOpt(
            QStringLiteral("station-fingerprint"),
            QStringLiteral(
                "SHA-256 fingerprint of the Core's certificate to pin."),
            QStringLiteral("sha256"));
        parser.addOption(fingerprintOpt);

        QCommandLineOption allowUnpinnedOpt(
            QStringLiteral("station-allow-unpinned"),
            QStringLiteral(
                "Accept the Core's self-signed certificate without a "
                "pinned fingerprint. Bench use only."));
        parser.addOption(allowUnpinnedOpt);

        QCommandLineOption localOpt(
            QStringLiteral("local"),
            QStringLiteral("Use this computer's built-in Core for local radios."));
        parser.addOption(localOpt);

        parser.process(app);

        // Command-line values only. The saved-Setup fallback cannot be read
        // yet: AppSettings is not loaded until CoreInit::initialize() below,
        // and a value() call before that returns the ship default rather
        // than what the operator saved. Resolved after CoreInit instead.
        request.connection.url = parser.value(stationOpt);
        request.connection.token = parser.value(tokenOpt);
        request.connection.fingerprint = parser.value(fingerprintOpt);
        request.connection.allowUnpinned = parser.isSet(allowUnpinnedOpt);
        request.stationSpecified = parser.isSet(stationOpt);
        request.tokenSpecified = parser.isSet(tokenOpt);
        request.fingerprintSpecified = parser.isSet(fingerprintOpt);
        request.allowUnpinnedSpecified = parser.isSet(allowUnpinnedOpt);
        request.local = parser.isSet(localOpt);
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

    // The raw pre-QApplication scale read above is read-only. All CoreInit,
    // settings migrations, logs, identity and later GUI writers run only
    // while this process owns the selected profile.
    NereusSDR::StationHandover ownership(activeProfile);
    QString handoverError;
    while (!ownership.reclaimFromBackground(15000, &handoverError)) {
        QMessageBox handover(QMessageBox::Warning, QStringLiteral("NereusSDR"),
            QStringLiteral("NereusSDR could not take ownership of this Core profile."));
        handover.setInformativeText(handoverError);
        QPushButton* const retry = handover.addButton(QStringLiteral("Retry"),
                                                       QMessageBox::AcceptRole);
        handover.addButton(QStringLiteral("Quit"), QMessageBox::RejectRole);
        handover.exec();
        if (handover.clickedButton() != retry) { return 1; }
    }

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

    // R-R3-38: one application owner replaces complete local/remote sessions.
    // It resolves credentials as a single target tuple, installs a fresh proxy
    // before each remote model, and retires the window before its backend.
    int rc = 0;
    std::optional<NereusSDR::StationServiceOptions> background;
    {
        NereusSDR::GuiConnectionController connections;
        connections.sessions()->configureDesktopStation(activeProfile, ownership.ownsProfile());
        connections.start(request);
        rc = app.exec();
        background = connections.sessions()->backgroundServiceOptions();
        connections.shutdown();
    }

    // Graceful shutdown so worker threads drain before the engine
    // singleton is destroyed.
    NereusSDR::ExternalVariableEngine::instance().shutdown();

    // Model, window and MMIO destructors can all write settings. Keep the
    // profile lock until their final values have reached disk, not merely
    // until the hosted listener or radio connection has stopped.
    QString finalSaveError;
    while (!NereusSDR::AppSettings::instance().save(&finalSaveError)) {
        const auto answer = QMessageBox::warning(nullptr, QStringLiteral("NereusSDR"),
            QStringLiteral("The Core could not save its final settings. The background Core "
                           "has not been started.\n\n%1").arg(finalSaveError),
            QMessageBox::Retry | QMessageBox::Close, QMessageBox::Retry);
        if (answer != QMessageBox::Retry) {
            NereusSDR::CoreInit::shutdown();
            return 1;
        }
    }

    // Uninstalls the custom message handler and closes the log file that
    // CoreInit::initialize() installed above. See src/core/CoreInit.cpp
    // for why the handler teardown has to be safe even if Qt logs
    // something between here and its own thread-storage teardown.
    NereusSDR::CoreInit::shutdown();
    ownership.release();
    if (background) {
        // Options were copied while the local runtime existed. The launcher
        // uses that explicit profile without touching shutdown AppSettings.
        NereusSDR::StationServiceManager service(*background);
        while (!service.startBackground()) {
            const auto answer = QMessageBox::warning(nullptr, QStringLiteral("NereusSDR"),
                QStringLiteral("The background Core could not start.\n\n%1")
                    .arg(service.lastError()),
                QMessageBox::Retry | QMessageBox::Close, QMessageBox::Retry);
            if (answer != QMessageBox::Retry) { return 1; }
        }
    }
    return rc;
}
