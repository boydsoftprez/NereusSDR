#include "CoreInit.h"

#include "AppSettings.h"
#include "LogCategories.h"
#include "LogSink.h"

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
    // MAC addresses, and ONLY MAC addresses. The four lookarounds are the
    // whole point of this pattern rather than defensive noise: without
    // them the six-pair body matches happily INSIDE any longer
    // colon-separated hex run, and the longest one this project prints is
    // a TLS SHA-256 certificate fingerprint
    // (CertificateStore::fingerprintSha256(), 32 colon-separated pairs).
    // Applied to one of those, the unguarded rule matched five times over
    // and replaced 25 of the 32 bytes with asterisks, on stderr and in
    // this file's own on-disk log alike, leaving the operator with an
    // unusable copy of the value StationClient refuses to connect without.
    //
    // The guard is "not part of a longer run", NOT "surrounded by
    // whitespace". A candidate is refused when it is preceded by a hex
    // digit (that would be mid-byte) or by <hex><hex><separator> (a pair
    // already sits in front of it), and likewise when it is followed by a
    // hex digit or by <separator><hex><hex>. Genuine MAC redaction is
    // untouched in every shape this tree logs one: bare, inside an
    // AppSettings key ("hardware/00:1C:2D:05:37:2A/..."), or after a word
    // and a hyphen. A run of seven pairs or more is left alone, which is
    // correct: it is not a MAC.
    static const QRegularExpression* macRe = new QRegularExpression(
        R"((?<![0-9A-Fa-f])(?<![0-9A-Fa-f]{2}[:-])([0-9A-Fa-f]{2}[:-]){5})"
        R"(([0-9A-Fa-f]{2})(?![0-9A-Fa-f])(?![:-][0-9A-Fa-f]{2}))");

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

    // Remote-window parity Task 22 (R-R3-49): the line is only offered to
    // the sink here, which never waits on the writer. Its writer thread
    // writes the file and stderr.
    LogSink& sink = LogSink::instance();
    const bool offered = sink.offer(line);
    if (type == QtFatalMsg) {
        // Fix wave (2026-09-30): Qt aborts as soon as this returns, before
        // the writer's next pass, so the line saying why never reached the
        // file. Drain it here, unless a drain is already running (never
        // wait on one: it may be stalled on the disk); then, or when the
        // ring had no room for it, the line goes straight to stderr.
        const bool drained = sink.tryDrainNow();
        if (!offered || !drained) {
            const QByteArray utf8 = line.toUtf8();
            std::fwrite(utf8.constData(), 1, static_cast<std::size_t>(utf8.size()), stderr);
            std::fflush(stderr);
        }
    }
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

    // The sink owns the bounded per-profile files and their writer lock.
    // A failed file setup still leaves stderr and the recent record stream
    // available; logging must never prevent the Core from starting.
    LogSink::instance().setRotatingOutput(logDir, true);
    LogSink::instance().start();
    qInstallMessageHandler(messageHandler);

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

    // R-R3-21: the global "hardware/oc/pennyExtCtrl" Setup used to save
    // goes to every saved radio's own Penny Ext Control key, then away.
    // Idempotent.
    AppSettings::migrateLegacyPennyExtCtrl(AppSettings::instance());

    // R-R3-21: settings whose writer and reader used different names keep
    // the value users saved under the old name. Idempotent.
    AppSettings::migrateRenamedKeys(AppSettings::instance());

    // v0.3.0 / v0.3.x settings schema migrations: must run after load(),
    // after other one-shot migrations above. v3 retires legacy display
    // keys; v4 retires DisplayAverageAlpha after the averaging-math fix
    // moved to per-side millisecond time constants; v5 splits the shared
    // DspOptionsBufferSize<Mode> / DspOptionsFilterSize<Mode> keys into
    // <Mode>Rx + <Mode>Tx variants so the UI can expose Thetis-faithful
    // per-channel combos; v6 (Phase 3F) is additive only: new per-slice
    // per-band keys populate lazily on first write; v7 (R-R3-49) resets
    // NetworkWatchdogEnabled once, since it was saved while nothing read it;
    // v8 (R-R3-49) drops the TCI rate limit saved in messages per second;
    // v9 (R-IOS-06, R-IOS-27) brings each slice's saved NR1 values into
    // Thetis's NR spinbox ranges.
    // See AppSettings::ensureSettingsAtVersion for the upstream Thetis cites.
    AppSettings::instance().ensureSettingsAtVersion(9);

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
    // Remote-window parity Task 22: what the sink still holds reaches the
    // file before it closes.
    LogSink::instance().stop();
    LogSink::instance().setOutputs(nullptr, false);
}

#ifdef NEREUS_BUILD_TESTS
int initializeRunCount()
{
    return s_initializeRunCount;
}

QString redactPiiForTest(const QString& message)
{
    return redactPii(message);
}
#endif

} // namespace CoreInit
} // namespace NereusSDR
