// =================================================================
// src/core/PaProfileManager.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/setup.cs:23295-23316 (initPAProfiles —
//     factory profile bank seeding loop)
//   Project Files/Source/Console/setup.cs:1920-1959 (RecoverPAProfiles —
//     deserialization with stored-wins-over-factory semantics)
//   original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-03 — Original implementation for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code. Phase 2 Agent 2B of issue #167 PA-cal
//                 safety hotfix (K2GX field report — drive-slider math
//                 without per-band PA gain compensation produced >300 W
//                 output on a 200 W ANAN-8000DLE).
//
//                 NereusSDR-original glue file: per-MAC scoping, in-memory
//                 cache, AppSettings persistence, and active-profile-on-
//                 connect resolver are NereusSDR-native (parallel to
//                 MicProfileManager); Thetis-derived handler logic is
//                 cited inline below.
//
//                 Mirrors MicProfileManager API verbatim. Factory regen on
//                 first connect (16 Default-<model> + Bypass) per Thetis
//                 initPAProfiles. Stored profiles win over factory
//                 defaults at deserialization per Thetis RecoverPAProfiles.
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

#pragma once

#include "HpsdrModel.h"
#include "PaProfile.h"

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>

namespace NereusSDR {

// ---------------------------------------------------------------------------
// PaProfileManager — per-MAC profile bank for PaProfile instances.
//
// Per-MAC AppSettings layout (parallel to hardware/<mac>/tx/profile/...
// from MicProfileManager):
//   hardware/<mac>/pa/profile/_names                "Default - HPSDR,...,Bypass"
//   hardware/<mac>/pa/profile/active                "Default - ANAN_G2"
//   hardware/<mac>/pa/profile/<name>                full PaProfile::dataToString() blob
//
// AppSettings does not natively enumerate keys by prefix. We keep a
// parallel manifest key (`_names`, comma-separated) as the source of
// truth for the profile list. This matches the MicProfileManager pattern.
//
// Active-profile-on-connect (NereusSDR-spin enhancement over Thetis):
// on load(connectedModel):
//   1. Read hardware/<mac>/pa/profile/active. If present and the named
//      profile is in the manifest, use it.
//   2. Else if "Default - <connectedModel>" exists, use it.
//   3. Else fall back to the first profile in the sorted manifest.
//
// Lifetime: lives on the main thread, owned by TransmitModel (wired in
// Phase 3A). All ops require a per-MAC scope set via setMacAddress() before
// any other call. Without a MAC, every mutator is a no-op (same convention
// as MicProfileManager).
// ---------------------------------------------------------------------------
class PaProfileManager : public QObject {
    Q_OBJECT

public:
    explicit PaProfileManager(QObject* parent = nullptr);
    ~PaProfileManager() override;

    /// Set the per-MAC scope (hardware/<mac>/pa/profile/...). Must be called
    /// before any other op.
    void setMacAddress(const QString& mac);

    /// Load the profile bank from AppSettings. On first launch, seeds 16
    /// `"Default - <HPSDRModel>"` factory profiles + one `"Bypass"` profile,
    /// sets the active profile to `"Default - <connectedModel>"`. On
    /// reconnect, the existing manifest is preserved (stored profiles win
    /// over factory defaults); the active-profile-on-connect logic restores
    /// either the stored active profile, falls back to
    /// `"Default - <connectedModel>"`, or finally to the first profile in
    /// the sorted manifest.
    ///
    /// Cite: setup.cs:23295-23316 [v2.10.3.13] — Thetis initPAProfiles.
    /// Cite: setup.cs:1920-1959 [v2.10.3.13] — Thetis RecoverPAProfiles
    ///       (stored-wins-over-factory semantics).
    void load(HPSDRModel connectedModel);

    /// List all profile names currently in the manager (sorted).
    QStringList profileNames() const;

    /// The currently-active profile name, or empty string if no MAC scope
    /// has been set / no load has run yet.
    QString activeProfileName() const;

    /// Returns a pointer to the profile with the given name, or nullptr if
    /// not found. Caller does NOT own; PaProfileManager retains ownership.
    /// Pointer remains valid until the next load() / saveProfile() /
    /// deleteProfile() / regenerateFactoryDefaults() call (any of which can
    /// invalidate it).
    const PaProfile* profileByName(const QString& name) const;

    /// Returns a pointer to the currently-active profile, or nullptr if no
    /// active is set. Used by `TransmitModel::computeAudioVolume` (Phase 3B)
    /// to consult the dB-gain table for the dBm-target math.
    const PaProfile* activeProfile() const;

    /// Set the active profile by name. Returns true on success and emits
    /// activeProfileChanged. Returns false (no signal) if the name is not in
    /// the manifest.
    bool setActiveProfile(const QString& name);

    /// Save a profile blob under the given name. Strips commas from the
    /// name (same precedent as MicProfileManager — keeps the comma-separated
    /// manifest unambiguous). Overwrites if the name already exists.
    /// Returns true on success.
    ///
    /// Emits profileListChanged ONLY when the profile is newly added to the
    /// manifest (overwrites do not emit). This matches MicProfileManager.
    bool saveProfile(const QString& name, const PaProfile& profile);

    /// Delete a profile by name. Refuses to delete the LAST remaining
    /// profile and returns false (Thetis precedent: setup.cs:9617-9624
    /// [v2.10.3.13] btnTXProfileDelete_Click — "It is not possible to
    /// delete the last remaining ..." — applied to PA profiles for
    /// consistency with MicProfileManager).
    /// Returns true on success.
    ///
    /// If the deleted profile was the active one, the active profile falls
    /// back to the lexicographically-first remaining profile (same precedent
    /// as MicProfileManager::deleteProfile).
    bool deleteProfile(const QString& name);

    /// Force-regenerate the 16 "Default - <model>" factory profiles + Bypass
    /// for the given connected-model context. Used by the "Reset Defaults"
    /// UI button (PaGainByBandPage). Does NOT change the active profile.
    /// Custom user profiles (i.e. profiles whose name is not one of the 17
    /// factory names) are left untouched.
    void regenerateFactoryDefaults(HPSDRModel connectedModel);

signals:
    /// Emitted when the profile-list membership changes (a profile was
    /// added or deleted). Plain overwrites do NOT emit this — only set
    /// membership changes do.
    void profileListChanged();

    /// Emitted when the active profile has changed (either via
    /// setActiveProfile or via deleteProfile of the active one).
    void activeProfileChanged(const QString& name);

private:
    /// Read the profile-name manifest from AppSettings. Empty list means
    /// no profiles exist (first-launch state — load() seeds the factory bank).
    QStringList readManifest() const;

    /// Write the profile-name manifest to AppSettings.
    void writeManifest(const QStringList& names);

    /// Set/get the active profile key (hardware/<mac>/pa/profile/active).
    QString readActiveKey() const;
    void    writeActiveKey(const QString& name);

    /// Write a single profile blob under hardware/<mac>/pa/profile/<name>
    /// using PaProfile::dataToString().
    void writeProfileBlob(const QString& name, const PaProfile& profile);

    /// Read a single profile blob from AppSettings and deserialize via
    /// PaProfile::dataFromString(). Returns false on missing entry or
    /// malformed blob.
    bool readProfileBlob(const QString& name, PaProfile& outProfile) const;

    /// Remove the hardware/<mac>/pa/profile/<name> entry.
    void removeProfileBlob(const QString& name);

    /// Build a single factory profile (Default - <model> or Bypass) and
    /// stash it in the in-memory cache + AppSettings. Used by load() and
    /// regenerateFactoryDefaults().
    void seedFactoryProfile(const QString& name,
                            HPSDRModel modelForDefaults,
                            bool isBypass);

    /// Compute the canonical 17 factory profile names: 16 Default - <model>
    /// (one per HPSDRModel enum entry from HPSDR=0 .. REDPITAYA=15) + 1 Bypass.
    static QStringList factoryProfileNames();

    /// Per-MAC scope. Empty until setMacAddress() is called; all mutators
    /// no-op when empty.
    QString m_mac;

    /// In-memory cache of every profile in this MAC's bank. Indexed by
    /// profile name. Reloaded from AppSettings on every load() / setActive
    /// / regen.
    QHash<QString, PaProfile> m_profiles;

    /// Active profile name (mirror of hardware/<mac>/pa/profile/active).
    QString m_activeProfileName;
};

}  // namespace NereusSDR
