#pragma once

#include "LogCategories.h"

#include <QByteArray>
#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>

namespace NereusSDR {

class RadioModel;

// Collects diagnostic information into a zip archive for bug reports.
// Bundle includes: log files, system info, radio info, sanitized settings,
// and enabled logging categories.
//
// Remote-window parity Task 22 / the iPhone app plan's Task 25 (R-R3-49,
// R-IOS-18): the Core's own bundle (`support.collect`), a ZIP of at most
// kMaxCoreBundleBytes built in memory, and a window's bundle that carries
// the Core's files under core/ beside its own. Every bundle is gathered on
// the calling (main) thread, which only reads values, and written on a
// worker thread, so making one never stalls the radio, in a local window
// or on a Core. Structured settings and daemon configuration are exported
// with a small allowlist of nonsecret values. Logs remove known live
// credentials and recognizable credential forms; arbitrary unknown secrets
// in otherwise innocent text cannot be inferred. J.J. Boyd (KG4VCF),
// 2026-09-27, AI-assisted via Anthropic Claude Code.
class SupportBundle {
public:
    struct SystemInfo {
        QString appVersion;
        QString qtVersion;
        QString osName;
        QString kernelVersion;
        QString cpuArch;
        QString buildDate;
    };

    struct RadioDiagInfo {
        QString model;
        QString macAddress;     // Redacted: only last segment
        QString firmware;
        QString ipAddress;      // Redacted: only last octet
        bool connected{false};
        int protocol{0};
    };

    /// The most a Core's bundle may be (the ZIP itself, before base64).
    static constexpr qint64 kMaxCoreBundleBytes = 2 * 1024 * 1024;
    /// How many recent log files a bundle carries at most.
    static constexpr int kMaxLogFiles = 3;
    static constexpr qint64 kMaxLogTailBytes = 8 * 1024 * 1024;
    static constexpr qint64 kMaxConfigInputBytes = 1024 * 1024;

    /// What a bundle is made from, read on the main thread.
    struct Inputs {
        SystemInfo system;
        RadioDiagInfo radio;
        QList<LogCategoryInfo> categories;
        QString logDir;
        QString settingsPath;
        /// nereusd's configuration file, when the Core runs from one.
        QString daemonConfigPath;
        /// The Core's newest telemetry (empty: none measured).
        QJsonObject telemetry;
        /// Runtime values already held by the caller. Never read credential files for this.
        QStringList knownSecrets;
        QDateTime stamp;
    };

    /// A window's bundle adds the Core's (a remote window only).
    struct CoreAttachment {
        bool wanted = false;
        QByteArray bundle;   // the Core's ZIP; empty when there is none
        QString reason;      // why there is none
    };

    // Collect system info from QSysInfo + app version.
    static SystemInfo collectSystemInfo();

    // Collect radio info from RadioModel (safe if null/disconnected).
    static RadioDiagInfo collectRadioInfo(const RadioModel* model);

    /// Reads what a bundle needs; main thread.
    static Inputs gatherInputs(const RadioModel* model);

    /// The Core's bundle: a ZIP of at most kMaxCoreBundleBytes. Any thread.
    static QByteArray buildCoreBundle(const Inputs& inputs);

    /// Runs buildCoreBundle on a worker thread and hands the ZIP (empty on
    /// failure) to `done` on `context`'s thread. Nothing runs if `context`
    /// is gone by then.
    static void buildCoreBundleAsync(QObject* context, Inputs inputs,
                                     std::function<void(QByteArray)> done);

    // Create a timestamped support bundle archive.
    // Returns the full path to the created archive, or empty on failure.
    static QString createBundle(const RadioModel* model);

    /// Writes this computer's bundle (with the Core's under core/ when
    /// attached) and returns its path, or empty on failure. Any thread.
    static QString writeBundle(const Inputs& inputs, const CoreAttachment& core);

    /// writeBundle on a worker thread; `done` gets the path on `context`'s
    /// thread.
    static void writeBundleAsync(QObject* context, Inputs inputs, CoreAttachment core,
                                 std::function<void(QString)> done);

    // Open the folder containing support bundles in the file manager.
    static void openBundleFolder();

    /// Redacts known runtime secrets and recognizable credential forms.
    static QString sanitizeText(const QString& text, const QStringList& knownSecrets = {});

private:
    static QString bundleDirPath();
    static QByteArray systemInfoJson(const SystemInfo& sys);
    static QByteArray radioInfoJson(const RadioDiagInfo& radio);
    static QByteArray enabledCategoriesText(const QList<LogCategoryInfo>& categories);
    static QStringList recentLogFiles(const QString& logDir);
    static QString createArchive(const QString& sourceDir, const QString& archiveName);
    static QString sanitizeLine(const QString& line);
};

} // namespace NereusSDR
