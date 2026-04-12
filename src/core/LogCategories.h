#pragma once

#include <QLoggingCategory>
#include <QObject>
#include <QString>
#include <QList>

namespace NereusSDR {

// Logging categories for NereusSDR.
// Usage: qCDebug(lcDiscovery) << "message";
Q_DECLARE_LOGGING_CATEGORY(lcDiscovery)
Q_DECLARE_LOGGING_CATEGORY(lcConnection)
Q_DECLARE_LOGGING_CATEGORY(lcProtocol)
Q_DECLARE_LOGGING_CATEGORY(lcReceiver)
Q_DECLARE_LOGGING_CATEGORY(lcAudio)
Q_DECLARE_LOGGING_CATEGORY(lcDsp)
Q_DECLARE_LOGGING_CATEGORY(lcSpectrum)
Q_DECLARE_LOGGING_CATEGORY(lcContainer)
Q_DECLARE_LOGGING_CATEGORY(lcMeter)
Q_DECLARE_LOGGING_CATEGORY(lcMmio)

// Runtime-manageable logging category metadata.
struct LogCategoryInfo {
    QString id;            // e.g. "nereus.connection"
    QString label;         // e.g. "Connection"
    QString description;   // e.g. "TCP/UDP command channel, protocol framing"
    bool enabled{false};   // runtime toggle state
};

// Manages logging categories with runtime enable/disable, persistence,
// and log file queries. Singleton.
class LogManager : public QObject {
    Q_OBJECT

public:
    static LogManager& instance();

    // --- Category Management ---
    QList<LogCategoryInfo> categories() const { return m_categories; }
    bool isEnabled(const QString& id) const;
    void setEnabled(const QString& id, bool on);
    void setAllEnabled(bool on);

    // --- Log File ---
    QString logFilePath() const;
    QString logDirPath() const;
    qint64 logFileSize() const;
    void clearLog();

    // --- Persistence ---
    void saveSettings();
    void loadSettings();

signals:
    void categoryChanged(const QString& id, bool enabled);

private:
    LogManager();
    ~LogManager() override = default;
    LogManager(const LogManager&) = delete;
    LogManager& operator=(const LogManager&) = delete;

    void applyFilterRules();

    QList<LogCategoryInfo> m_categories;
};

} // namespace NereusSDR
