#include "gui/MainWindow.h"
#include "core/AppSettings.h"
#include "core/RadioConnection.h"
#include "core/mmio/ExternalVariableEngine.h"
#include "core/LogCategories.h"

#include <QApplication>
#include <QIcon>
#include <QStyleFactory>
#include <QDir>
#include <QFile>
#include <QDateTime>
#include <QTextStream>
#include <QStandardPaths>
#include <QRegularExpression>

#include <cstdlib>
#include <exception>

static QFile* s_logFile = nullptr;

// Issue #83 diagnostic: capture any C++ exception that escapes into
// std::terminate() (noexcept destructors, unhandled throws during
// shutdown) so the crash log shows *what* threw. Without this, Qt's
// shutdown path on Windows lands in RaiseFailFastException with no
// message, leaving us with a symbolless minidump. Handler must not
// itself throw; wrap everything and fall through to std::abort at
// the end to preserve the FAST_FAIL signature the minidump expects.
static void issue83TerminateHandler() noexcept
{
    try {
        if (auto ep = std::current_exception()) {
            try {
                std::rethrow_exception(ep);
            } catch (const std::exception& e) {
                qCritical("issue-83: std::terminate — std::exception: %s "
                          "(type=%s)", e.what(), typeid(e).name());
            } catch (...) {
                qCritical("issue-83: std::terminate — non-std::exception "
                          "(no what() available)");
            }
        } else {
            qCritical("issue-83: std::terminate — no active exception "
                      "(raw std::terminate call)");
        }
    } catch (...) {
        // Handler itself threw; nothing useful to do here.
    }

    if (s_logFile && s_logFile->isOpen()) {
        s_logFile->flush();
    }
    std::abort();
}

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

int main(int argc, char* argv[])
{
    // Apply saved UI scale factor BEFORE QApplication is created.
    {
#ifdef Q_OS_MAC
        QString settingsPath = QDir::homePath() + "/Library/Preferences/NereusSDR/NereusSDR.settings";
#else
        QString settingsPath = QDir::homePath() + "/.config/NereusSDR/NereusSDR.settings";
#endif
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

    // Set up file logging in ~/.config/NereusSDR/
    const QString logDir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
                           + "/NereusSDR";
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

    // Issue #83 diagnostic: install terminate handler after the file
    // log handler, so the log captures whatever escaped.
    std::set_terminate(issue83TerminateHandler);

    // Fusion style as a clean cross-platform base
    app.setStyle(QStyleFactory::create("Fusion"));

    // Register custom metatypes for cross-thread signal/slot connections.
    qRegisterMetaType<NereusSDR::RadioConnectionError>();

    // Load XML settings
    NereusSDR::AppSettings::instance().load();

    // Restore logging category toggles from settings
    NereusSDR::LogManager::instance().loadSettings();

    qDebug() << "Starting NereusSDR" << app.applicationVersion()
             << "[debug/issue-83-shutdown-diagnostic]";

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
