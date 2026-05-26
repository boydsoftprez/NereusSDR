#include "gui/MainWindow.h"
#include "gui/styles/AppTheme.h"
#include "core/AppSettings.h"
#include "core/AudioDeviceConfig.h"
#include "core/MacMicPermission.h"
#include "core/audio/RealtimeAudioPriority.h"
#include "core/RadioConnection.h"
#include "core/mmio/ExternalVariableEngine.h"
#include "core/LogCategories.h"

#include <QApplication>
#include <QThreadPool>
#include <QCommandLineOption>
#include <QMetaObject>
#include <csignal>
#include <QCommandLineParser>
#include <QIcon>
#include <QStyleFactory>
#include <QDir>
#include <QFile>
#include <QDateTime>
#include <QTextStream>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QStringList>

static QFile* s_logFile = nullptr;

// Redact PII from log messages before writing to file.
// Patterns: IP addresses, MAC addresses.
//
// The regex objects are allocated on the heap and leaked intentionally
// so they survive __cxa_finalize. Qt emits shutdown warnings from
// QThreadStoragePrivate::finish *after* function-local static
// destructors have run — if we stored them as `static const
// QRegularExpression`, that call chain would re-enter this handler,
// touch a destroyed regex, and crash with EXC_BAD_ACCESS at exit.
// Leaked statics are the simplest fix for the destruction-order
// fiasco. A belt-and-braces `qInstallMessageHandler(nullptr)` near
// the end of main() still runs first, but this handler path has to
// be safe even if Qt logs something between `return rc` and its own
// thread-storage teardown.
static QString redactPii(const QString& msg)
{
    static const QRegularExpression* ipRe = new QRegularExpression(
        R"((\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3}))");
    static const QRegularExpression* macRe = new QRegularExpression(
        R"(([0-9A-Fa-f]{2}[:-]){5}([0-9A-Fa-f]{2}))");

    QString out = msg;
    // IPv4 addresses: 192.168.50.121 -> *.*.*. 121 (keep last octet)
    out.replace(*ipRe, QStringLiteral("*.*.*. \\4"));
    // MAC addresses: 00:1C:2D:05:37:2A -> **:**:**:**:**:2A
    out.replace(*macRe, QStringLiteral("**:**:**:**:**:\\2"));
    return out;
}

static void messageHandler(QtMsgType type, const QMessageLogContext& ctx, const QString& msg)
{
    Q_UNUSED(ctx);
    static const char* labels[] = {"DBG", "WRN", "CRT", "FTL", "INF"};
    const char* label = (type <= QtInfoMsg) ? labels[type] : "???";

    const QString safeMsg = redactPii(msg);
    const QString line = QString("[%1] %2: %3\n")
        .arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz"), label, safeMsg);

    if (s_logFile && s_logFile->isOpen()) {
        QTextStream ts(s_logFile);
        ts << line;
        ts.flush();
    }
    fprintf(stderr, "%s", line.toLocal8Bit().constData());
}

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

    // 2026-05-26 KG4VCF jitter investigation finding.  Profiled with
    // macOS `sample` during a stress build: the main thread was
    // repeatedly blocking in QLatch::waitInternal -> __ulock_wait2,
    // adding up to ~300 samples (~58 ms over 8 s) just waiting for
    // QThreadPool::globalInstance() workers to finish parallel span-
    // blend operations dispatched from QRasterPaintEngine.
    //
    // Those pool workers run at DEFAULT QoS.  Under heavy compile
    // load they get starved, and the main thread sits blocked on
    // their completion -- visible as 60-138 ms paint-event gaps even
    // though our hot buffers are mlock'd and our own threads are at
    // USER_INTERACTIVE.
    //
    // Disable Qt's parallel rasterization globally.  Our paint
    // operations (3 small ellipses, polylines, ~16 chrome ops) are
    // all tiny -- well below the threshold where parallel dispatch
    // beats single-threaded raster.  The dispatch + latch wait is
    // pure overhead for our workload, and the wait becomes a hard
    // failure mode when the pool can't be scheduled.  Single-threaded
    // raster has zero latch waits.
    //
    // Env var name from Qt 6 qrasterizer.cpp.  Setting both the
    // "_PARALLEL=0" and "_NO_PARALLEL=1" forms because Qt's check
    // logic has varied between minor versions; one of them will
    // always be the one Qt looks for.
    qputenv("QT_RASTER_PARALLEL", "0");
    qputenv("QT_NO_RASTER_PARALLEL", "1");

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

    // 2026-05-26 KG4VCF backup for the parallel-raster fix above.  Even
    // with QT_RASTER_PARALLEL=0 set, other Qt internals occasionally
    // dispatch work to QThreadPool::globalInstance() (e.g. style
    // pixmap caching, layout computations).  Reduce the worst-case
    // scheduling damage by clamping the global pool to a small thread
    // count -- so a runaway parallel dispatch can't spin up 12 default-
    // QoS workers that all compete with our latency-critical threads.
    // Two workers is enough for the rare legitimate uses without
    // starving the rest of the system.
    QThreadPool::globalInstance()->setMaxThreadCount(2);

    // 2026-05-22 bench-finding: pkill / kill / system shutdown sends SIGTERM
    // by default; the OS terminates the process without giving Qt a chance
    // to run aboutToQuit handlers.  Without translation, this skips
    // RadioConnection::disconnect, the radio gateware never sees run=0, and
    // some community P2 firmwares require power-cycle to recover.  Install
    // POSIX signal handlers that convert SIGTERM / SIGINT into
    // QApplication::quit, which fires aboutToQuit and runs the graceful
    // disconnect path.  SIGKILL (kill -9, Activity Monitor "Force Quit") is
    // uncatchable — power-cycle is still the only recovery there.
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

    // Set up file logging in ~/.config/NereusSDR/ (or the profile's
    // isolated config dir when --profile is set).
    const QString logDir = NereusSDR::AppSettings::resolveConfigDir(activeProfile);
    QDir().mkpath(logDir);

    const QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss");
    const QString logPath = logDir + "/nereussdr-" + timestamp + ".log";

    // Prune old log files (keep newest 4 + the one we're about to create = 5)
    {
        QDir dir(logDir);
        QStringList logs = dir.entryList({"nereussdr-*.log"}, QDir::Files, QDir::Name);
        while (logs.size() >= 5) {
            dir.remove(logs.takeFirst());
        }
    }

    s_logFile = new QFile(logPath);
    if (s_logFile->open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        s_logFile->setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
        qInstallMessageHandler(messageHandler);

        const QString symlink = logDir + "/nereussdr.log";
        QFile::remove(symlink);
        QFile::link(logPath, symlink);
    } else {
        fprintf(stderr, "Warning: could not open log file %s\n", logPath.toLocal8Bit().constData());
        delete s_logFile;
        s_logFile = nullptr;
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
    qRegisterMetaType<NereusSDR::RadioConnectionError>();
    qRegisterMetaType<NereusSDR::AudioDeviceConfig>();

    // Load XML settings
    NereusSDR::AppSettings::instance().load();

    // Phase 3O schema migration — must run before any AppSettings reads.
    NereusSDR::AppSettings::migrateVaxSchemaV1ToV2();

    // hermes-filter-debug Bug 2: legacy global "hl2IoBoard/n2adrFilter" key
    // → per-MAC scope under hardware/<mac>/hl2IoBoard/n2adrFilter for every
    // saved HL2.  Idempotent.
    NereusSDR::AppSettings::migrateLegacyN2adrFilter(
        NereusSDR::AppSettings::instance());

    // Issue #174: drop the orphan "hardware/oc/n2adrFilter" key written by
    // the now-removed OcOutputsHfTab checkbox.  Idempotent.
    NereusSDR::AppSettings::removeOrphanOcN2adrFilter(
        NereusSDR::AppSettings::instance());

    // v0.3.0 / v0.3.x settings schema migrations — must run after load(),
    // after other one-shot migrations above. v3 retires legacy display
    // keys; v4 retires DisplayAverageAlpha after the averaging-math fix
    // moved to per-side millisecond time constants; v5 splits the shared
    // DspOptionsBufferSize<Mode> / DspOptionsFilterSize<Mode> keys into
    // <Mode>Rx + <Mode>Tx variants so the UI can expose Thetis-faithful
    // per-channel combos.  See AppSettings::ensureSettingsAtVersion for the
    // upstream Thetis cites.
    NereusSDR::AppSettings::instance().ensureSettingsAtVersion(5);

    // Restore logging category toggles from settings
    NereusSDR::LogManager::instance().loadSettings();

    qDebug() << "Starting NereusSDR" << app.applicationVersion();
    if (!activeProfile.isEmpty()) {
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

    // Restore the default message handler before statics start
    // tearing down. Qt's QThreadStoragePrivate::finish() emits
    // warnings from __cxa_finalize, and if we leave our custom
    // handler installed those warnings land in messageHandler ->
    // redactPii() after its function-local statics (or anything
    // else in this TU) could already be destroyed. Belt-and-braces
    // for the leaked-regex fix in redactPii().
    qInstallMessageHandler(nullptr);
    if (s_logFile) {
        s_logFile->close();
        // Intentionally leaked — Qt may still try to log between
        // here and __cxa_finalize; the default handler routes to
        // stderr which is safe.
    }
    return rc;
}
