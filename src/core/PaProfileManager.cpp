// =================================================================
// src/core/PaProfileManager.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/setup.cs:23295-23316 (initPAProfiles)
//   Project Files/Source/Console/setup.cs:1920-1959   (RecoverPAProfiles)
//   original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-03 — Original implementation for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code. Phase 2 Agent 2B of issue #167.
// =================================================================

// --- From setup.cs ---

//=================================================================
// setup.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
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
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to:
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//
//=================================================================
// Continual modifications Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
//=================================================================
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

// no-port-check: NereusSDR-original file; Thetis-derived handler logic
// is cited inline below.

#include "PaProfileManager.h"

#include "AppSettings.h"
#include "LogCategories.h"
#include "PaGainProfile.h"
#include "models/Band.h"

#include <QLoggingCategory>

#include <algorithm>

namespace NereusSDR {

namespace {

QString profilePathPrefix(const QString& mac)
{
    return QStringLiteral("hardware/%1/pa/profile/").arg(mac);
}

QString profileBlobKeyFor(const QString& mac, const QString& name)
{
    return profilePathPrefix(mac) + name;
}

QString manifestKey(const QString& mac)
{
    return profilePathPrefix(mac) + QStringLiteral("_names");
}

QString activeKey(const QString& mac)
{
    return profilePathPrefix(mac) + QStringLiteral("active");
}

// Map HPSDRModel enum values to their C#-compatible enum names. These match
// the Thetis enum identifiers exactly so factory profile names round-trip
// across implementations (e.g. ANAN_G2 → "ANAN_G2" not "ANAN-G2").
const char* modelEnumName(HPSDRModel m) noexcept
{
    switch (m) {
        case HPSDRModel::HPSDR:        return "HPSDR";
        case HPSDRModel::HERMES:       return "HERMES";
        case HPSDRModel::ANAN10:       return "ANAN10";
        case HPSDRModel::ANAN10E:      return "ANAN10E";
        case HPSDRModel::ANAN100:      return "ANAN100";
        case HPSDRModel::ANAN100B:     return "ANAN100B";
        case HPSDRModel::ANAN100D:     return "ANAN100D";
        case HPSDRModel::ANAN200D:     return "ANAN200D";
        case HPSDRModel::ORIONMKII:    return "ORIONMKII";
        case HPSDRModel::ANAN7000D:    return "ANAN7000D";
        case HPSDRModel::ANAN8000D:    return "ANAN8000D";
        case HPSDRModel::ANAN_G2:      return "ANAN_G2";
        case HPSDRModel::ANAN_G2_1K:   return "ANAN_G2_1K";
        case HPSDRModel::ANVELINAPRO3: return "ANVELINAPRO3";
        case HPSDRModel::HERMESLITE:   return "HERMESLITE";
        case HPSDRModel::REDPITAYA:    return "REDPITAYA";
        case HPSDRModel::FIRST:
        case HPSDRModel::LAST:         return "Unknown";
    }
    return "Unknown";
}

QString defaultProfileNameForModel(HPSDRModel model)
{
    // Thetis setup.cs:23309 [v2.10.3.13] — "Default - " + model.ToString()
    // (C# enum.ToString() yields the enum name). NereusSDR uses the same
    // name format so manifest entries can be cross-referenced byte-for-byte
    // with Thetis-formatted profile names.
    return QStringLiteral("Default - ") +
           QString::fromLatin1(modelEnumName(model));
}

// Reverse-map a "Default - <model>" name back to the corresponding
// HPSDRModel. For any name that doesn't match the prefix, returns FIRST
// (which the Bypass path handles via its own branch in seedFactoryProfile).
HPSDRModel modelFromFactoryName(const QString& name) noexcept
{
    for (int n = static_cast<int>(HPSDRModel::HPSDR);
         n <= static_cast<int>(HPSDRModel::REDPITAYA); ++n) {
        const HPSDRModel m = static_cast<HPSDRModel>(n);
        if (name == QStringLiteral("Default - ") +
                    QString::fromLatin1(modelEnumName(m))) {
            return m;
        }
    }
    return HPSDRModel::FIRST;
}

// The string the Thetis Setup → PA Gain editor uses for the bypass entry.
// From Thetis setup.cs:23314 [v2.10.3.13] — `_sPA_PROFILE_BYPASS = "Bypass"`.
constexpr const char* kBypassProfileName = "Bypass";

}  // namespace

// ---------------------------------------------------------------------------
// Construction / dtor
// ---------------------------------------------------------------------------

PaProfileManager::PaProfileManager(QObject* parent)
    : QObject(parent)
{
}

PaProfileManager::~PaProfileManager() = default;

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void PaProfileManager::setMacAddress(const QString& mac)
{
    m_mac = mac;
}

void PaProfileManager::load(HPSDRModel connectedModel)
{
    if (m_mac.isEmpty()) {
        return;
    }

    QStringList names = readManifest();
    const bool firstLaunch = names.isEmpty();

    if (firstLaunch) {
        // First-launch seed.
        // From Thetis setup.cs:23295-23316 [v2.10.3.13] — initPAProfiles
        // builds one "Default - <model>" entry per HPSDRModel.LAST iteration
        // plus one "Bypass" profile. NereusSDR persists each profile blob
        // under hardware/<mac>/pa/profile/<name> as its dataToString() form.
        const QStringList factoryNames = factoryProfileNames();
        for (const QString& name : factoryNames) {
            const bool isBypass =
                (name == QString::fromLatin1(kBypassProfileName));
            // Bypass uses HPSDRModel::FIRST sentinel — the PaProfile
            // constructor maps that to the all-100.0f bypass row via
            // PaGainProfile (Thetis precedent: setup.cs:23314 passes
            // HPSDRModel.FIRST so ResetGainDefaultsForModel hits the
            // sentinel branch).
            const HPSDRModel modelForRow =
                isBypass ? HPSDRModel::FIRST
                         : modelFromFactoryName(name);
            seedFactoryProfile(name, modelForRow, isBypass);
        }
        names = factoryNames;
        writeManifest(names);
        emit profileListChanged();
    } else {
        // Reconnect path. Stored profiles win over factory defaults.
        // From Thetis setup.cs:1920-1959 [v2.10.3.13] — RecoverPAProfiles
        // reads the persisted blobs into _PAProfiles, replacing any
        // factory defaults with the same name. NereusSDR does the same:
        // pull each profile blob from AppSettings into the in-memory cache.
        m_profiles.clear();
        for (const QString& name : names) {
            PaProfile p;
            if (readProfileBlob(name, p)) {
                m_profiles.insert(name, p);
            }
        }
    }

    // Active-profile-on-connect (NereusSDR-spin):
    //   1. Stored active in manifest? Use it.
    //   2. Else if "Default - <connectedModel>" exists, use it.
    //   3. Else fall back to the first profile in the sorted manifest.
    const QString storedActive = readActiveKey();
    QString resolved;
    if (!storedActive.isEmpty() && names.contains(storedActive)) {
        resolved = storedActive;
    } else {
        const QString defaultName = defaultProfileNameForModel(connectedModel);
        if (names.contains(defaultName)) {
            resolved = defaultName;
        } else if (!names.isEmpty()) {
            QStringList sorted = names;
            std::sort(sorted.begin(), sorted.end());
            resolved = sorted.first();
        }
    }

    m_activeProfileName = resolved;
    if (!resolved.isEmpty()) {
        writeActiveKey(resolved);
    }
}

QStringList PaProfileManager::profileNames() const
{
    QStringList names = readManifest();
    std::sort(names.begin(), names.end());
    return names;
}

QString PaProfileManager::activeProfileName() const
{
    return m_activeProfileName;
}

const PaProfile* PaProfileManager::profileByName(const QString& name) const
{
    auto it = m_profiles.find(name);
    if (it == m_profiles.end()) {
        return nullptr;
    }
    return &it.value();
}

const PaProfile* PaProfileManager::activeProfile() const
{
    if (m_activeProfileName.isEmpty()) {
        return nullptr;
    }
    return profileByName(m_activeProfileName);
}

bool PaProfileManager::setActiveProfile(const QString& name)
{
    if (m_mac.isEmpty()) {
        return false;
    }

    const QStringList names = readManifest();
    if (!names.contains(name)) {
        return false;
    }

    m_activeProfileName = name;
    writeActiveKey(name);
    emit activeProfileChanged(name);
    return true;
}

bool PaProfileManager::saveProfile(const QString& rawName, const PaProfile& profile)
{
    if (m_mac.isEmpty()) {
        return false;
    }

    // Comma-strip on save — keeps the comma-separated manifest unambiguous.
    // Thetis precedent: setup.cs:9550-9552 [v2.10.3.13] (TX-profile flow)
    //   no more , in profile names, because it will make the tci tx profile messages look like they have multiple parts
    //   existing ones will cause issues no doubt, but just not worth the effort to reparse the database
    // PA profiles aren't sent over TCI today, but applying the same rule
    // here keeps PaProfileManager and MicProfileManager consistent.
    QString name = rawName;
    name.replace(QLatin1Char(','), QLatin1Char('_'));

    if (name.isEmpty()) {
        return false;
    }

    writeProfileBlob(name, profile);
    m_profiles.insert(name, profile);

    QStringList names = readManifest();
    const bool wasInList = names.contains(name);
    if (!wasInList) {
        names.append(name);
        writeManifest(names);
        emit profileListChanged();
    }
    return true;
}

bool PaProfileManager::deleteProfile(const QString& name)
{
    if (m_mac.isEmpty()) {
        return false;
    }

    QStringList names = readManifest();

    // Last-profile guard. Thetis precedent: setup.cs:9617-9624 [v2.10.3.13]
    //   if (comboTXProfileName.Items.Count == 1) { MessageBox.Show("It is not
    //   possible to delete the last remaining TX profile."); return; }
    // Same rule applied to PA profiles for consistency with MicProfileManager.
    if (names.size() <= 1) {
        qCWarning(lcDsp)
            << "It is not possible to delete the last remaining PA profile";
        return false;
    }

    if (!names.contains(name)) {
        return false;
    }

    removeProfileBlob(name);
    m_profiles.remove(name);
    names.removeAll(name);
    writeManifest(names);

    // Active fallback. If we just deleted the active profile, fall back to
    // the lexicographically-first remaining profile (same surface the UI
    // sees via profileNames()).
    if (m_activeProfileName == name) {
        QStringList sorted = names;
        std::sort(sorted.begin(), sorted.end());
        const QString fallback = sorted.first();
        m_activeProfileName = fallback;
        writeActiveKey(fallback);
        emit activeProfileChanged(fallback);
    }

    emit profileListChanged();
    return true;
}

void PaProfileManager::regenerateFactoryDefaults(HPSDRModel connectedModel)
{
    Q_UNUSED(connectedModel);  // factory rows are pulled from each row's own
                                // model (not the connected one) — connected
                                // model only matters for the initial active-
                                // selection logic in load().

    if (m_mac.isEmpty()) {
        return;
    }

    QStringList names = readManifest();
    const QStringList factoryNames = factoryProfileNames();

    // Re-seed every factory entry. Custom profiles (any name not in
    // factoryNames) are left untouched — the user's Reset Defaults click
    // refreshes only canonical rows.
    bool manifestChanged = false;
    for (const QString& name : factoryNames) {
        const bool isBypass =
            (name == QString::fromLatin1(kBypassProfileName));
        const HPSDRModel modelForRow =
            isBypass ? HPSDRModel::FIRST
                     : modelFromFactoryName(name);
        seedFactoryProfile(name, modelForRow, isBypass);
        if (!names.contains(name)) {
            names.append(name);
            manifestChanged = true;
        }
    }

    if (manifestChanged) {
        writeManifest(names);
        emit profileListChanged();
    }
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

QStringList PaProfileManager::readManifest() const
{
    if (m_mac.isEmpty()) {
        return {};
    }
    const QString raw = AppSettings::instance().value(manifestKey(m_mac)).toString();
    if (raw.isEmpty()) {
        return {};
    }
    QStringList names = raw.split(QLatin1Char(','), Qt::SkipEmptyParts);
    names.removeDuplicates();
    return names;
}

void PaProfileManager::writeManifest(const QStringList& names)
{
    if (m_mac.isEmpty()) {
        return;
    }
    AppSettings::instance().setValue(manifestKey(m_mac),
                                      names.join(QLatin1Char(',')));
}

QString PaProfileManager::readActiveKey() const
{
    if (m_mac.isEmpty()) {
        return {};
    }
    return AppSettings::instance().value(activeKey(m_mac)).toString();
}

void PaProfileManager::writeActiveKey(const QString& name)
{
    if (m_mac.isEmpty()) {
        return;
    }
    AppSettings::instance().setValue(activeKey(m_mac), name);
}

void PaProfileManager::writeProfileBlob(const QString& name, const PaProfile& profile)
{
    if (m_mac.isEmpty()) {
        return;
    }
    AppSettings::instance().setValue(profileBlobKeyFor(m_mac, name),
                                      profile.dataToString());
}

bool PaProfileManager::readProfileBlob(const QString& name, PaProfile& outProfile) const
{
    if (m_mac.isEmpty()) {
        return false;
    }
    const QString blob =
        AppSettings::instance().value(profileBlobKeyFor(m_mac, name)).toString();
    if (blob.isEmpty()) {
        return false;
    }
    return outProfile.dataFromString(blob);
}

void PaProfileManager::removeProfileBlob(const QString& name)
{
    if (m_mac.isEmpty()) {
        return;
    }
    AppSettings::instance().remove(profileBlobKeyFor(m_mac, name));
}

void PaProfileManager::seedFactoryProfile(const QString& name,
                                           HPSDRModel modelForDefaults,
                                           bool isBypass)
{
    // Thetis setup.cs:23309 [v2.10.3.13] passes isFactoryDefault=true so
    // PaProfile::PaProfile(name, model, true) builds a row with the
    // canonical PaGainProfile values via ResetGainDefaultsForModel.
    PaProfile p(name, modelForDefaults, /*isFactoryDefault=*/true);

    // NereusSDR-spin: the Bypass profile must use the all-100.0f sentinel
    // row from `bypassPaGainsForBand` (PaGainProfile.h), NOT the
    // HPSDRModel::FIRST row from `defaultPaGainsForBand` (which Thetis
    // groups with HERMES at 41.x dB). The 100.0f sentinel triggers the
    // gbb >= 99.5 short-circuit in `TransmitModel::computeAudioVolume`
    // (Phase 3B), giving callers a linear-fallback path that's the
    // documented purpose of the Bypass entry.
    if (isBypass) {
        for (int n = 0; n < PaProfile::kBandCount; ++n) {
            const Band band = static_cast<Band>(n);
            p.setGainForBand(band, bypassPaGainsForBand(band));
        }
    }

    writeProfileBlob(name, p);
    m_profiles.insert(name, p);
}

QStringList PaProfileManager::factoryProfileNames()
{
    // From Thetis setup.cs:23306-23316 [v2.10.3.13] — initPAProfiles loops
    // for (int n = 0; n < (int)HPSDRModel.LAST; n++) and adds one
    // "Default - <model>" per iteration, then appends one "Bypass" entry.
    // NereusSDR walks the same enum range (HPSDR..REDPITAYA = 0..15) and
    // appends Bypass at the end — manifest order matches Thetis combo
    // order.
    QStringList out;
    for (int n = static_cast<int>(HPSDRModel::HPSDR);
         n <= static_cast<int>(HPSDRModel::REDPITAYA); ++n) {
        const HPSDRModel m = static_cast<HPSDRModel>(n);
        out.append(QStringLiteral("Default - ") +
                   QString::fromLatin1(modelEnumName(m)));
    }
    out.append(QString::fromLatin1(kBypassProfileName));
    return out;
}

}  // namespace NereusSDR
