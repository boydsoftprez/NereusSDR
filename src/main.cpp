#include "gui/MainWindow.h"
#include "gui/styles/AppTheme.h"
#include "core/AppSettings.h"
#include "core/AudioDeviceConfig.h"
#include "core/MacMicPermission.h"
#include "core/RadioConnection.h"
#include "core/mmio/ExternalVariableEngine.h"
#include "core/LogCategories.h"

#include <QApplication>
#include <QCommandLineOption>
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

    QApplication app(argc, argv);
    app.setApplicationName("NereusSDR");
    app.setApplicationVersion(NEREUSSDR_VERSION);
    app.setOrganizationName("NereusSDR");
    app.setWindowIcon(QIcon(":/icons/NereusSDR.png"));

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
