#pragma once

#include <QByteArray>
#include <QString>

namespace NereusSDR {

// One backup carries each owner's own XML store as a separate UTF-8 part.
struct SettingsBackup {
    static constexpr qsizetype kMaxXmlBytes = 16 * 1024 * 1024;
    static constexpr qsizetype kMaxFileBytes = 64 * 1024 * 1024;

    QByteArray windowXml;
    QByteArray coreXml;

    static bool encode(const SettingsBackup& backup, QByteArray* output,
                       QString* error = nullptr);
    static bool decode(const QByteArray& input, SettingsBackup* output,
                       QString* error = nullptr);
    static bool readFile(const QString& path, SettingsBackup* output,
                         QString* error = nullptr);
    static bool writeFile(const QString& path, const SettingsBackup& backup,
                          QString* error = nullptr);
};

} // namespace NereusSDR
