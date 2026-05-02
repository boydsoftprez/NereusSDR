// =================================================================
// src/core/MicProfileManager.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original file. The mic-profile-bank API is a port of the
// Thetis comboTXProfile / Setup → TX Profile editor flow:
//   setup.cs:9505-9543 [v2.10.3.13] — comboTXProfileName_SelectedIndexChanged
//   setup.cs:9545-9612 [v2.10.3.13] — btnTXProfileSave_Click
//   setup.cs:9615-9656 [v2.10.3.13] — btnTXProfileDelete_Click
//
// NereusSDR collapses the Thetis many-table TXProfile schema (~206 columns
// in DB.ds.Tables["TXProfile"]) to the live-fields-only subset (93 keys =
// 15 mic/VOX/MON + 7 two-tone + 1 drive-power-source enum + 22 EQ + 1
// TX EQ blob + 3 Leveler + 2 ALC + 41 CFC/CPDR/CESSB/PhRot + 2 FilterLow/High).
// 19 of Thetis's 21 factory profiles are deferred to 3M-3a sub-PRs that ship
// CFC / DEXP backends; 3M-3a-i G adds the EQ + Leveler + ALC bundle on top of
// the 3M-1c chunk-F base (which seeded only the "Default" profile).  The TX EQ
// parametric blob (TXParaEQData) is the 51st live key; landed in Phase
// 3M-3a-ii follow-up Batch 6.  FilterLow/FilterHigh (keys 92-93) added in
// Plan 4 Cluster A.
//
// Per-MAC AppSettings layout (parallel to hardware/<mac>/tx/<key> from
// TransmitModel L.2):
//   hardware/<mac>/tx/profile/_names                      = "Default,ProfileA,..."
//   hardware/<mac>/tx/profile/active                      = "Default"
//   hardware/<mac>/tx/profile/<name>/MicGain              = "-6"
//   hardware/<mac>/tx/profile/<name>/Mic_Input_Boost      = "True"
//   ... (93 live keys per profile; same key names as
//        TransmitModel::persistOne uses under hardware/<mac>/tx/<key>)
//
// AppSettings does not natively enumerate keys by prefix.  We keep a
// parallel manifest key (`_names`, comma-separated) as the source of
// truth for the profile list — this also matches the comma-strip
// invariant on saveProfile (see F.2 below) so "_names" stays unambiguous.
//
// =================================================================
//
// Modification history (NereusSDR):
//   2026-04-28 — Original implementation for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
//                 Phase 3M-1c chunk F (F.1-F.6) — MicProfileManager class
//                 (load/save/delete/setActive) covering the per-MAC mic-
//                 profile-bank behaviour.  Ports
//                 setup.cs:9505-9656 [v2.10.3.13] handler logic
//                 (comboTXProfileName / btnTXProfileSave_Click /
//                  btnTXProfileDelete_Click).  UI surface (TxApplet combo,
//                 Setup → TX Profile page) lands in Phases J.1 / J.3;
//                 RadioModel ownership wires up in Phase L.
//   2026-05-02 — saveActiveProfile + isActiveProfileModified +
//                 profileModifiedChanged (Plan 4 Cluster A, Task 2/D1).
//                 FilterLow/FilterHigh added to liveKeyList, defaultProfileValues,
//                 captureLiveValues, and applyValuesToModel (93 keys total).
//                 NereusSDR-original additions.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

// no-port-check: NereusSDR-original file; Thetis-derived handler logic
// is cited inline below.

#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>

namespace NereusSDR {

class TransmitModel;

// ---------------------------------------------------------------------------
// MicProfileManager — per-MAC profile bank for the mic / VOX / MON / two-tone
// live-fields subset.
//
// Lifetime: lives on the main thread, owned by RadioModel (wired in Phase L).
// All ops require a per-MAC scope set via setMacAddress() before any other
// call.  Without a MAC, every mutator is a no-op (same convention as
// TransmitModel::persistOne(): m_persistMac empty → silent skip).
//
// Profile list discovery: AppSettings does not enumerate keys by prefix.
// We keep a manifest key (`hardware/<mac>/tx/profile/_names`) holding a
// comma-separated profile list.  saveProfile() / deleteProfile() update
// the manifest atomically with the profile-key writes.  load() reads the
// manifest.  Comma-stripping on saveProfile (F.2 spec) keeps the manifest
// unambiguous.
// ---------------------------------------------------------------------------
class MicProfileManager : public QObject {
    Q_OBJECT

public:
    explicit MicProfileManager(QObject* parent = nullptr);
    ~MicProfileManager() override;

    /// Set the per-MAC scope (hardware/<mac>/tx/profile/...).  Must be called
    /// before any other op.  Mirrors TransmitModel::loadFromSettings(mac).
    void setMacAddress(const QString& mac);

    /// Load the profile list from AppSettings.  Seeds "Default" on first launch
    /// with the live-field defaults (per F.5).  Idempotent: subsequent loads on
    /// the same scope are no-ops if the manifest already exists.
    void load();

    /// List all profile names currently in the manager (sorted).
    QStringList profileNames() const;

    /// The currently-active profile name, or "Default" if none has been set.
    QString activeProfileName() const;

    /// Apply a profile by name: writes its values back to TransmitModel via the
    /// public setter API so per-property auto-persist (TransmitModel::persistOne)
    /// flushes the live state under hardware/<mac>/tx/<key>.  Returns false if
    /// the profile doesn't exist; returns true and emits activeProfileChanged
    /// otherwise.
    ///
    /// Cite: setup.cs:9535-9541 [v2.10.3.13] — Thetis loadTXProfile + active
    /// assignment.  NereusSDR omits the Thetis focus-gated unsaved-changes
    /// prompt (UI-layer concern; Phase J.4 owns it) — this method is the
    /// pure mechanical apply.
    bool setActiveProfile(const QString& name, TransmitModel* tx);

    /// Save the current TransmitModel state under the given profile name.
    /// Strips commas (Thetis precedent: TCI safety per setup.cs:9550-9552
    /// [v2.10.3.13] — "no more , in profile names, because it will make
    /// the tci tx profile messages look like they have multiple parts").
    /// Overwrites if the name already exists.  Returns true on success.
    ///
    /// Cite: setup.cs:9545-9612 [v2.10.3.13] — Thetis btnTXProfileSave_Click.
    /// NereusSDR omits the empty-name and overwrite-confirm dialogs — UI layer
    /// (Phase J.3) handles those before calling into this method.
    bool saveProfile(const QString& name, const TransmitModel* tx);

    /// Delete a profile by name.  Refuses to delete the LAST remaining
    /// profile and returns false (qCWarning logs the verbatim Thetis string
    /// per F.3).  Returns true on success.
    ///
    /// Cite: setup.cs:9615-9656 [v2.10.3.13] — Thetis btnTXProfileDelete_Click.
    /// NereusSDR omits the are-you-sure confirmation dialog — UI layer
    /// (Phase J.3) handles that before calling into this method.
    bool deleteProfile(const QString& name);

    /// Returns the live-field default values (the "Default" profile values
    /// used by F.5 on first launch).  Static helper exposed for tests and
    /// for any future "reset to defaults" UI button.
    static QHash<QString, QVariant> defaultProfileValues();

    /// Convenience wrapper: saves the current TransmitModel state under the
    /// active profile name.  Equivalent to saveProfile(activeProfileName(), tx).
    /// Emits profileModifiedChanged(false) when the save succeeds (the live
    /// state now matches the stored profile).
    /// Returns false if no active profile is set or the save fails.
    bool saveActiveProfile(TransmitModel* tx);

    /// Returns true if the active profile's stored FilterLow or FilterHigh
    /// values differ from the live TransmitModel state.  Returns false if
    /// no active profile has been set yet (profile hasn't been activated),
    /// or if the active profile doesn't exist in AppSettings.
    ///
    /// Plan 4 scope: checks FilterLow + FilterHigh only.  Plan 5 will
    /// expand this to all 93 profile fields.
    bool isActiveProfileModified(const TransmitModel* tx) const;

signals:
    /// Emitted when the dirty/clean state of the active profile changes.
    /// true = active profile has unsaved changes; false = in sync with stored.
    ///
    /// For Plan 4 minimum scope: emitted from setActiveProfile
    /// (profileModifiedChanged(false) — freshly loaded profile is clean) and
    /// from saveActiveProfile (profileModifiedChanged(false) after save).
    /// Plan 5 will also emit true whenever a field changes away from stored.
    void profileModifiedChanged(bool modified);
    /// Emitted when the profile list has changed (a profile was added or
    /// deleted).  Plain overwrites do NOT emit this — only set membership
    /// changes do.
    void profileListChanged();

    /// Emitted when the active profile has changed (either via
    /// setActiveProfile or via deleteProfile of the active one).
    void activeProfileChanged(const QString& name);

private:
    /// Read the profile-name manifest from AppSettings.  Empty list means
    /// no profiles exist (first-launch state — load() seeds "Default").
    QStringList readManifest() const;

    /// Write the profile-name manifest to AppSettings.
    void writeManifest(const QStringList& names);

    /// Set/get the active profile key (hardware/<mac>/tx/profile/active).
    QString readActiveKey() const;
    void writeActiveKey(const QString& name);

    /// Write a single profile under hardware/<mac>/tx/profile/<name>/<key>.
    /// values is a key→value hash (e.g. from defaultProfileValues() or from
    /// captureLiveValues(tx)).
    void writeProfileKeys(const QString& name, const QHash<QString, QVariant>& values);

    /// Read a single profile from AppSettings.  Returns an empty hash if
    /// no keys exist for the named profile.
    QHash<QString, QVariant> readProfileKeys(const QString& name) const;

    /// Remove all hardware/<mac>/tx/profile/<name>/<key> entries.
    void removeProfileKeys(const QString& name);

    /// Capture current TransmitModel state into a live-field key→value hash.
    /// 93 keys (51 mic/VOX/MON/two-tone/EQ/Lev/ALC/TXParaEQData + 41
    /// CFC/CPDR/CESSB/PhRot + 2 FilterLow/FilterHigh); matches
    /// defaultProfileValues() in shape.
    static QHash<QString, QVariant> captureLiveValues(const TransmitModel* tx);

    /// Apply a profile's key→value hash back to a TransmitModel via the
    /// public setter API.  Idempotent on values that match the model's
    /// current state (TransmitModel setters guard against no-op writes).
    static void applyValuesToModel(const QHash<QString, QVariant>& values,
                                   TransmitModel* tx);

    /// Per-MAC scope.  Empty until setMacAddress() is called; all mutators
    /// no-op when empty.
    QString m_mac;
};

} // namespace NereusSDR
