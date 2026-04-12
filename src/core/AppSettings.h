#pragma once

#include <QString>
#include <QStringList>
#include <QVariant>
#include <QMap>

namespace NereusSDR {

// XML-based application settings.
// Stored at ~/.config/NereusSDR/NereusSDR.settings.
//
// Usage:
//   auto& s = AppSettings::instance();
//   s.setValue("LastConnectedRadioMac", "00:1C:2D:05:37:2A");
//   QString mac = s.value("LastConnectedRadioMac").toString();
//   s.save();
//
// Per-station settings:
//   s.setStationValue("AnalogRXMeterSelection", "S-Meter");
//   QString sel = s.stationValue("AnalogRXMeterSelection", "S-Meter").toString();

class AppSettings {
public:
    static AppSettings& instance();

    // Load settings from disk. Called once at startup.
    void load();

    // Save settings to disk.
    void save();

    // Get/set top-level settings.
    QVariant value(const QString& key, const QVariant& defaultValue = {}) const;
    void setValue(const QString& key, const QVariant& val);
    void remove(const QString& key);
    bool contains(const QString& key) const;
    // Return every top-level key currently in the settings store.
    // Used by the MMIO engine to group keys under the MmioEndpoints/
    // prefix at app startup.
    QStringList allKeys() const;

    // Per-station settings (nested under <StationName> element).
    QVariant stationValue(const QString& key, const QVariant& defaultValue = {}) const;
    void setStationValue(const QString& key, const QVariant& val);

    // Station name (defaults to "NereusSDR").
    QString stationName() const;
    void setStationName(const QString& name);

    // File path for the settings file.
    QString filePath() const { return m_filePath; }

private:
    AppSettings();
    ~AppSettings() = default;
    AppSettings(const AppSettings&) = delete;
    AppSettings& operator=(const AppSettings&) = delete;

    QString m_filePath;
    QMap<QString, QString> m_settings;
    QMap<QString, QString> m_stationSettings;
    QString m_stationName{"NereusSDR"};
};

} // namespace NereusSDR
