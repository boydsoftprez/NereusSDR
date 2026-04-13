#pragma once

#include "RadioDiscovery.h"

#include <QString>
#include <QStringList>
#include <QVariant>
#include <QMap>
#include <QDateTime>
#include <QHostAddress>
#include <optional>

namespace NereusSDR {

// Saved-radio bundle (Phase 3I Task 15).
// Combines RadioInfo with the client-side flags that only live in settings.
struct SavedRadio {
    RadioInfo info;
    bool      pinToMac{false};
    bool      autoConnect{false};
    QDateTime lastSeen;
};

// XML-based application settings.
// Stored at ~/.config/NereusSDR/NereusSDR.settings (or overridden for tests).
//
// Usage (singleton — app code):
//   auto& s = AppSettings::instance();
//   s.setValue("LastConnectedRadioMac", "00:1C:2D:05:37:2A");
//   QString mac = s.value("LastConnectedRadioMac").toString();
//   s.save();
//
// Usage (direct construction — for tests only):
//   AppSettings s("/tmp/testdir/NereusSDR.settings");
//   s.saveRadio(info, true, false);
//
// Per-station settings:
//   s.setStationValue("AnalogRXMeterSelection", "S-Meter");
//   QString sel = s.stationValue("AnalogRXMeterSelection", "S-Meter").toString();

class AppSettings {
public:
    // Singleton accessor — use this in all non-test code.
    static AppSettings& instance();

    // Direct construction for tests — provide a full file path.
    // The file need not exist; it is created on first save().
    explicit AppSettings(const QString& filePath);

    ~AppSettings() = default;
    AppSettings(const AppSettings&) = delete;
    AppSettings& operator=(const AppSettings&) = delete;

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

    // -------------------------------------------------------------------------
    // Saved-radio management (Phase 3I Task 15).
    //
    // Keys stored as flat entries in m_settings:
    //   radios/<macKey>/name
    //   radios/<macKey>/ipAddress
    //   radios/<macKey>/port
    //   radios/<macKey>/macAddress
    //   radios/<macKey>/boardType       (int — HPSDRHW value)
    //   radios/<macKey>/protocol        (int — ProtocolVersion value)
    //   radios/<macKey>/firmwareVersion (int)
    //   radios/<macKey>/pinToMac        (bool as "True"/"False")
    //   radios/<macKey>/autoConnect     (bool as "True"/"False")
    //   radios/<macKey>/lastSeen        (ISO 8601 UTC string)
    //   radios/lastConnected            (macKey string)
    //   radios/discoveryProfile         (int — DiscoveryProfile value)
    //
    // macKey = MAC address if present, else "manual-<ip>-<port>".
    // -------------------------------------------------------------------------

    // Save (or update) a radio entry. Overwrites if macKey already exists.
    // Does NOT call save() — caller must save() or rely on shutdown flush.
    void saveRadio(const RadioInfo& info, bool pinToMac, bool autoConnect);

    // Remove the entry for macKey. No-op if not found.
    void forgetRadio(const QString& macKey);

    // Remove ALL radios/* entries (does not touch lastConnected/discoveryProfile).
    void clearSavedRadios();

    // Return all saved radios. Order is undefined (QMap key iteration).
    QList<SavedRadio> savedRadios() const;

    // Return one saved radio by macKey. nullopt if not found.
    std::optional<SavedRadio> savedRadio(const QString& macKey) const;

    // Last-connected MAC (for auto-reconnect, Task 17).
    QString lastConnected() const;
    void    setLastConnected(const QString& macKey);

    // Discovery profile preference.
    DiscoveryProfile discoveryProfile() const;
    void             setDiscoveryProfile(DiscoveryProfile p);

private:
    // Private default constructor — used only by instance().
    AppSettings();

    // Shared init (resolves file path on macOS/Linux/Win).
    void initFilePath();

    // Key helper: prefix for a specific radio's fields.
    static QString radioKeyPrefix(const QString& macKey);

    // Extract macKey from a flat settings key like "radios/aa:bb/name" → "aa:bb".
    // Returns empty string if key is not a per-radio field.
    static QString macKeyFromSettingsKey(const QString& settingsKey);

    QString m_filePath;
    QMap<QString, QString> m_settings;
    QMap<QString, QString> m_stationSettings;
    QString m_stationName{"NereusSDR"};
};

} // namespace NereusSDR
