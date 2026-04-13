#include "gui/MainWindow.h"
#include "core/AppSettings.h"
#include "core/RadioConnection.h"
#include "core/mmio/ExternalVariableEngine.h"
#include "core/LogCategories.h"

#include <QApplication>
#include <QStyleFactory>
#include <QDir>
#include <QFile>
#include <QDateTime>
#include <QTextStream>
#include <QStandardPaths>
#include <QRegularExpression>

static QFile* s_logFile = nullptr;

// Redact PII from log messages before writing to file.
// Patterns: IP addresses, MAC addresses.
static QString redactPii(const QString& msg)
{
    QString out = msg;

    // IPv4 addresses: 192.168.50.121 -> *.*.*. 121 (keep last octet)
    static const QRegularExpression ipRe(
        R"((\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3}))");
    out.replace(ipRe, QStringLiteral("*.*.*. \\4"));

    // MAC addresses: 00:1C:2D:05:37:2A -> **:**:**:**:**:2A
    static const QRegularExpression macRe(
        R"(([0-9A-Fa-f]{2}[:-]){5}([0-9A-Fa-f]{2}))");
    out.replace(macRe, QStringLiteral("**:**:**:**:**:\\2"));

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

    // Fusion style as a clean cross-platform base
    app.setStyle(QStyleFactory::create("Fusion"));

    // Register custom metatypes for cross-thread signal/slot connections.
    qRegisterMetaType<NereusSDR::RadioConnectionError>();

    // Load XML settings
    NereusSDR::AppSettings::instance().load();

    // Restore logging category toggles from settings
    NereusSDR::LogManager::instance().loadSettings();

    qDebug() << "Starting NereusSDR" << app.applicationVersion();

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
    return rc;
}
