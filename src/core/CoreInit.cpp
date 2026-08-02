#include "CoreInit.h"

#include "AppSettings.h"
#include "LogCategories.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QStringList>
#include <QTextStream>
#include <QDebug>

#include <cstdio>

namespace NereusSDR {
namespace CoreInit {

// Guards the body of initialize() so a second (or later) call is a
// genuine no-op: it returns true immediately without touching AppSettings,
// the log file, or the message handler again.
static bool s_initialized = false;

#ifdef NEREUS_BUILD_TESTS
// Test-only hook backing initializeRunCount(); see CoreInit.h.
static int s_initializeRunCount = 0;
#endif

// Relocated from src/main.cpp (R1 Task 8). Owns the same file the custom
// Qt message handler below writes through.
static QFile* s_logFile = nullptr;

// Redact PII from log messages before writing to file.
// Patterns: IP addresses, MAC addresses.
//
// The regex objects are allocated on the heap and leaked intentionally
// so they survive __cxa_finalize. Qt emits shutdown warnings from
// QThreadStoragePrivate::finish *after* function-local static
// destructors have run; if we stored them as `static const
// QRegularExpression`, that call chain would re-enter this handler,
// touch a destroyed regex, and crash with EXC_BAD_ACCESS at exit.
// Leaked statics are the simplest fix for the destruction-order
// fiasco. A belt-and-braces `qInstallMessageHandler(nullptr)` in
// shutdown() (called near the end of main()) still runs first, but
// this handler path has to be safe even if Qt logs something between
// `return rc` and its own thread-storage teardown.
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

bool initialize(const QString& profile)
{
    if (s_initialized) {
        return true;
    }

    // Set up file logging in ~/.config/NereusSDR/ (or the profile's
    // isolated config dir when --profile is set). Uses the `profile`
    // argument directly rather than AppSettings::profileOverride(): the
    // caller (main.cpp, or nereusd's own early argv scan) already pinned
    // the override before constructing its Q(Core)Application, so by the
    // time initialize() runs the two agree; passing profile explicitly
    // means a daemon does not have to replay that pre-application step
    // just to tell this function where to put its log file.
    const QString logDir = AppSettings::resolveConfigDir(profile);
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

    // Load XML settings
    AppSettings::instance().load();

    // Phase 3O schema migration: must run before any AppSettings reads.
    AppSettings::migrateVaxSchemaV1ToV2();

    // hermes-filter-debug Bug 2: legacy global "hl2IoBoard/n2adrFilter" key
    // → per-MAC scope under hardware/<mac>/hl2IoBoard/n2adrFilter for every
    // saved HL2. Idempotent.
    AppSettings::migrateLegacyN2adrFilter(AppSettings::instance());

    // Issue #174: drop the orphan "hardware/oc/n2adrFilter" key written by
    // the now-removed OcOutputsHfTab checkbox. Idempotent.
    AppSettings::removeOrphanOcN2adrFilter(AppSettings::instance());

    // v0.3.0 / v0.3.x settings schema migrations: must run after load(),
    // after other one-shot migrations above. v3 retires legacy display
    // keys; v4 retires DisplayAverageAlpha after the averaging-math fix
    // moved to per-side millisecond time constants; v5 splits the shared
    // DspOptionsBufferSize<Mode> / DspOptionsFilterSize<Mode> keys into
    // <Mode>Rx + <Mode>Tx variants so the UI can expose Thetis-faithful
    // per-channel combos; v6 (Phase 3F) is additive only: new per-slice
    // per-band keys populate lazily on first write.
    // See AppSettings::ensureSettingsAtVersion for the upstream Thetis cites.
    AppSettings::instance().ensureSettingsAtVersion(6);

    // Restore logging category toggles from settings
    LogManager::instance().loadSettings();

#ifdef NEREUS_BUILD_TESTS
    ++s_initializeRunCount;
#endif
    s_initialized = true;
    return true;
}

void shutdown()
{
    // Restore the default message handler before statics start tearing
    // down. Qt's QThreadStoragePrivate::finish() emits warnings from
    // __cxa_finalize, and if we leave our custom handler installed those
    // warnings land in messageHandler -> redactPii() after its
    // function-local statics (or anything else in this TU) could already
    // be destroyed. Belt-and-braces for the leaked-regex fix in
    // redactPii().
    qInstallMessageHandler(nullptr);
    if (s_logFile) {
        s_logFile->close();
        // Intentionally leaked: Qt may still try to log between here and
        // __cxa_finalize; the default handler routes to stderr, which is
        // safe.
    }
}

#ifdef NEREUS_BUILD_TESTS
int initializeRunCount()
{
    return s_initializeRunCount;
}
#endif

} // namespace CoreInit
} // namespace NereusSDR
