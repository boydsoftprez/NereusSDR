// =================================================================
// src/core/AppSettings.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/database.cs, original licence from Thetis source is included below
//   AetherSDR src/core/AppSettings.{h,cpp} — AetherSDR has no per-file headers; project-level GPLv3 and contributor list per About dialog per https://github.com/ten9876/AetherSDR
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-18 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//                 AppSettings XML persistence: key/value semantics (PascalCase keys, True/False string booleans, per-StationName nesting) port Thetis database.cs SaveVarsDictionary/RestoreVarsDictionary pattern; QXmlStream file I/O skeleton follows AetherSDR `src/core/AppSettings.{h,cpp}`.
// =================================================================

//=================================================================
// database.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2012  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact us via email at: gpl@flexradio.com.
// Paper mail may be sent to:
//    FlexRadio Systems
//    4616 W. Howard Lane  Suite 1-150
//    Austin, TX 78728
//    USA
//=================================================================
// Modifications to the database import function to allow using files created with earlier versions.
// by Chris Codella, W2PA, May 2017.  Indicated by //-W2PA comment lines.
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

// Upstream source 'AetherSDR src/core/AppSettings.{h,cpp}' has no top-of-file header — project-level LICENSE applies.

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

    // Remove ALL top-level keys from the in-memory store.
    // Intended for test isolation. Does NOT call save().
    void clear();

    // Per-station settings (nested under <StationName> element).
    QVariant stationValue(const QString& key, const QVariant& defaultValue = {}) const;
    void setStationValue(const QString& key, const QVariant& val);

    // Station name (defaults to "NereusSDR").
    QString stationName() const;
    void setStationName(const QString& name);

    // File path for the settings file.
    QString filePath() const { return m_filePath; }

    // ------------------------------------------------------------------
    // Profile support (Issue #100) — multiple concurrent NereusSDR
    // instances against different radios. A profile name scopes the
    // settings file (and the log dir, in main.cpp) to a per-profile
    // subdirectory so two instances don't clobber each other's XML.
    //
    //   (default / empty profile) → <config>/NereusSDR/NereusSDR.settings
    //   profile = "hf"            → <config>/NereusSDR/profiles/hf/NereusSDR.settings
    //
    // Call setProfileOverride() from main() BEFORE the first
    // AppSettings::instance() call; the singleton resolves its file
    // path once in its default constructor.
    // ------------------------------------------------------------------
    static void    setProfileOverride(const QString& profile);
    static QString profileOverride();

    // Path resolvers — pure functions, safe to call before/without the
    // singleton. main.cpp uses resolveConfigDir() to scope the log dir
    // and the pre-QApplication UiScalePercent read.
    static QString resolveSettingsPath(const QString& profile);
    static QString resolveConfigDir(const QString& profile);

    // Profile names are restricted to [A-Za-z0-9_-] and non-empty.
    // Anything else (path traversal, whitespace, separators) falls back
    // to the default/empty profile in the resolvers above.
    static bool isValidProfileName(const QString& profile);

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

    // Model override for a specific radio MAC (Phase 3I-RP).
    HPSDRModel modelOverride(const QString& macKey) const;
    void setModelOverride(const QString& macKey, HPSDRModel model);

    // Migrate all fields stored under oldKey to newKey (Phase 3Q Task 12).
    // Intended for the "saved offline → probed successfully → real MAC known"
    // transition: moves a synthetic "manual-<ip>-<port>" entry to the real
    // MAC address key so user customisations (name, autoConnect, pinToMac, etc.)
    // survive the first successful probe.
    //
    // No-op when oldKey == newKey or oldKey has no stored entry.
    // After migration the oldKey entry is fully removed; newKey entry reflects
    // all fields from oldKey, with info.macAddress set to newKey.
    void migrateRadioKey(const QString& oldKey, const QString& newKey);

    // -------------------------------------------------------------------------
    // Per-slice-per-band DSP state (Phase 3G-10 Stage 2 — S2.P).
    //
    // Keys are stored as flat top-level AppSettings entries. The namespace
    // uses PascalCase path segments separated by "/".
    //
    // Per-band DSP state (varies by band — restored on band change):
    //   Slice<N>/Band<key>/AgcThreshold   — int dBu
    //   Slice<N>/Band<key>/AgcHang        — int ms
    //   Slice<N>/Band<key>/AgcSlope       — int dB
    //   Slice<N>/Band<key>/AgcAttack      — int ms
    //   Slice<N>/Band<key>/AgcDecay       — int ms
    //   Slice<N>/Band<key>/FilterLow      — int Hz
    //   Slice<N>/Band<key>/FilterHigh     — int Hz
    //   Slice<N>/Band<key>/DspMode        — int (DSPMode enum)
    //   Slice<N>/Band<key>/AgcMode        — int (AGCMode enum)
    //   Slice<N>/Band<key>/StepHz         — int Hz
    //   Slice<N>/Band<key>/NbMode         — int  (0=Off, 1=NB, 2=NB2)  — default 0
    //   Slice<N>/Band<key>/NbThreshold    — double                       — default 30.0
    //   Slice<N>/Band<key>/NbTauMs        — double                       — default 0.1 ms
    //   Slice<N>/Band<key>/NbLeadMs       — double                       — default 0.1 ms
    //   Slice<N>/Band<key>/NbLagMs        — double                       — default 0.1 ms
    //
    // NB/NB2 tuning defaults trace to Thetis ChannelMaster/cmaster.c:43-68 [v2.10.3.13]
    // (0.0001s=0.1ms for tau/hang/adv; 30.0 threshold; 0.05 backtau fixed, not per-slice).
    //
    // Session state (band-agnostic — restored on startup, not on band change):
    //   Slice<N>/Locked     — "True"/"False"
    //   Slice<N>/Muted      — "True"/"False"
    //   Slice<N>/RitEnabled — "True"/"False"
    //   Slice<N>/RitHz      — int Hz
    //   Slice<N>/XitEnabled — "True"/"False"
    //   Slice<N>/XitHz      — int Hz
    //   Slice<N>/AfGain     — int 0-100
    //   Slice<N>/RfGain     — int 0-100
    //   Slice<N>/RxAntenna  — string (e.g. "ANT1")
    //   Slice<N>/TxAntenna  — string (e.g. "ANT1")
    //   Slice<N>/SnbEnabled — "True"/"False"           — default "False" (session-level)
    //
    // <N> = slice index (0-based). <key> = bandKeyName(band) from Band.h
    // (e.g. "20m", "40m", "GEN"). Written by SliceModel::saveToSettings();
    // read by SliceModel::restoreFromSettings(). Legacy "Vfo*" flat keys
    // are one-shot migrated to this namespace by SliceModel::migrateLegacyKeys()
    // on startup.
    // -------------------------------------------------------------------------

    // -------------------------------------------------------------------------
    // Hardware tab persistence (Phase 3I Task 21).
    // Keys stored under hardware/<mac>/<tabKey>/<field>.
    // The MAC address and internal slashes/colons are encoded via the same
    // __c__ / __s__ scheme used for radios/* keys.
    //
    // Example:
    //   setHardwareValue("aa:bb:cc:11:22:33", "radioInfo/sampleRate", 192000)
    //   → stored as flat key "hardware/aa:bb:cc:11:22:33/radioInfo/sampleRate"
    // -------------------------------------------------------------------------
    void    setHardwareValue(const QString& mac, const QString& key, const QVariant& value);
    QVariant hardwareValue(const QString& mac, const QString& key,
                           const QVariant& defaultValue = QVariant()) const;
    // Returns all key/value pairs stored under hardware/<mac>/, with the
    // "hardware/<mac>/" prefix stripped so callers see bare keys like
    // "radioInfo/sampleRate".
    QMap<QString, QVariant> hardwareValues(const QString& mac) const;
    void    clearHardwareValues(const QString& mac);

    // Phase 3O VAX schema migration. Call once at app startup. Idempotent.
    // Migrates legacy audio/OutputDevice → audio/Speakers/DeviceName with
    // sensible platform defaults, and flags the first-run dialog to show
    // (audio/FirstRunComplete = "False").
    // See docs/architecture/2026-04-19-vax-design.md §5.5.
    static void migrateVaxSchemaV1ToV2();

    // One-shot migration: legacy global "hl2IoBoard/n2adrFilter" (Bug 2 in
    // hermes-filter-debug) → per-MAC "hardware/<mac>/hl2IoBoard/n2adrFilter"
    // for every saved radio whose boardType is HermesLite. Removes the global
    // key after migration. Idempotent (no-op if global key absent).
    //
    // Why per-MAC: NereusSDR scopes radio-specific settings under
    // hardware/<mac>/ to support multi-radio installations from a single
    // settings file. Thetis (mi0bot) achieves the same effective semantic
    // by swapping DB files per radio (database.cs:11237 ImportDatabase
    // [@c26a8a4]); we do it within one file via MAC scoping.
    //
    // Test-friendly: takes AppSettings& so unit tests can drive an isolated
    // instance via QTemporaryDir without touching the singleton.
    static void migrateLegacyN2adrFilter(AppSettings& s);

    // Issue #174 cleanup: the OcOutputsHfTab "N2ADR Filter (HERCULES)"
    // checkbox wrote to a global "hardware/oc/n2adrFilter" key that had
    // no consumer.  The actual N2ADR setting lives at per-MAC
    // hardware/<mac>/hl2IoBoard/n2adrFilter (Hl2IoBoardTab).  This
    // one-shot removes the orphan key so it doesn't linger in users'
    // settings files after upgrade.  Idempotent (no-op if key absent).
    static void removeOrphanOcN2adrFilter(AppSettings& s);

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
