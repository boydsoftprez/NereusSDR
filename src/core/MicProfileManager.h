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
// in DB.ds.Tables["TXProfile"]) to the live-fields-only subset (100 keys:
// mic/VOX/MON, EQ, TX EQ blob, Leveler, ALC, CFC/CPDR/CESSB/PhRot,
// FilterLow/High, DEXP; see liveKeyList() in MicProfileManager.cpp).
// Two-tone is not profile content: Thetis TX profiles carry no two-tone
// fields (database.cs:4299 AddTXProfileTable [v2.10.3.15]); TransmitModel
// persists it under hardware/<mac>/tx/TwoTone*.
// 19 of Thetis's 21 factory profiles are deferred to 3M-3a sub-PRs that ship
// CFC / DEXP backends; 3M-3a-i G adds the EQ + Leveler + ALC bundle on top of
// the 3M-1c chunk-F base (which seeded only the "Default" profile).  The TX EQ
// parametric blob (TXParaEQData) is the 39th live key; landed in Phase
// 3M-3a-ii follow-up Batch 6.  FilterLow/FilterHigh (keys 88-89) added in
// Plan 4 Cluster A.
//
// Per-MAC AppSettings layout (parallel to hardware/<mac>/tx/<key> from
// TransmitModel L.2):
//   hardware/<mac>/tx/profile/_names                      = "Default,ProfileA,..."
//   hardware/<mac>/tx/profile/active                      = "Default"
//   hardware/<mac>/tx/profile/<name>/MicGain              = "-6"
//   hardware/<mac>/tx/profile/<name>/Mic_Input_Boost      = "True"
//   ... (100 live keys per profile; same key names as
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
//   2026-10-13 - Notify profile consumers when the MAC scope changes to
//                 an existing bank, preserving same-scope load idempotence.
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
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
//   2026-09-25 - R-R3-49 (parity Task 3): a remote window's manager
//                 mirrors the Core's profiles (setStationMirror,
//                 applyStationProfiles, applyStationActiveProfile) and asks
//                 the Core to select, save and delete; it never saves here.
//                 NereusSDR-original. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-09 - Two-tone leaves TX profiles; the comments that listed it
//                 as profile content and counted 93 keys now say 100 keys
//                 without two-tone. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-09 - Fix wave: TXParaEQData and FilterLow/FilterHigh positions
//                 recounted from liveKeyList() (39th, 88-89). J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-11-17 - importProfile, normalizedProfileName, readStoredProfile
//                 and liveProfileValues for one profile's file; the station
//                 requester carries the imported values. Brian Sutton
//                 (W4KZO), AI-assisted via xAI Grok.
//   2026-11-22 - Import checks the name and every value with a plain
//                 reason, leaves out this radio's wiring keys and refuses a
//                 file with no settings; profileNameRefusal also guards
//                 save; a remote window hears the Core's answer
//                 (stationRequestFinished). A name with two underscores in
//                 a row is refused; setStationExportSaveWaiting. J.J. Boyd
//                 (KG4VCF), AI-assisted via Claude Code.
//   2026-11-25 - A name is refused only when its keys would not come back
//                 from the settings file. stationRequestFinished carries
//                 the command id; claimStationRequest replaces
//                 setStationExportSaveWaiting. J.J. Boyd (KG4VCF),
//                 AI-assisted via Claude Code.
// =================================================================

// no-port-check: NereusSDR-original file; Thetis-derived handler logic
// is cited inline below.

#pragma once

#include "core/NereusCoreExport.h"
#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariant>

#include <functional>

namespace NereusSDR {

class TransmitModel;

// ---------------------------------------------------------------------------
// MicProfileManager: per-MAC profile bank for the mic / VOX / MON / EQ /
// CFC / DEXP live-fields subset (two-tone is not profile content).
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
class NEREUS_CORE_EXPORT MicProfileManager : public QObject {
    Q_OBJECT

public:
    explicit MicProfileManager(QObject* parent = nullptr);
    ~MicProfileManager() override;

    /// Set the per-MAC scope (hardware/<mac>/tx/profile/...).  Must be called
    /// before any other op.  Mirrors TransmitModel::loadFromSettings(mac).
    /// A changed scope with an existing bank emits profileListChanged so
    /// consumers refresh its names and active selection without applying it.
    /// An unchanged scope, empty/missing bank, or station mirror emits nothing.
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

    /// Store `values` under `name` (commas become underscores, as save does)
    /// and select it. Missing live keys take the factory defaults. Unknown
    /// keys are left out, and so are this radio's wiring keys
    /// (radioWiringKeys): the profile keeps this radio's current values for
    /// them. Refused, with `reason` in plain words, for a name
    /// profileNameRefusal refuses, for values importValuesRefusal refuses,
    /// and for a file with no setting this build knows. On a remote window
    /// this asks the Core (txProfile.import) and changes nothing here; the
    /// Core's answer comes back through stationRequestFinished.
    bool importProfile(const QString& name, const QHash<QString, QVariant>& values,
                       TransmitModel* tx, QString* reason = nullptr);

    /// The name save and import store: trimmed, with commas replaced.
    static QString normalizedProfileName(QString name);

    /// The longest profile name save and import take, in characters.
    static constexpr int kMaxProfileNameLength = 64;

    /// Why `name` (as normalizedProfileName gives it) cannot name a
    /// profile, in plain words, or empty when it can: empty, longer than
    /// kMaxProfileNameLength, or holding a control character (a tab, a
    /// line break) or a character that is not text, or one whose keys
    /// would not come back from the settings file (it reads __c__ or
    /// __u0042__ in a key back as one character). "My__Profile" is fine.
    /// Needs no radio: a remote window knows no radio address.
    static QString profileNameRefusal(const QString& name);

    /// The keys that describe how this radio is wired (the mic jack, PTT,
    /// bias, line input, digital outputs and the mic source). A profile
    /// file leaves them out and an import ignores them.
    static const QStringList& radioWiringKeys();

    /// `values` without the radio wiring keys: what a profile file carries.
    static QHash<QString, QVariant> portableValues(const QHash<QString, QVariant>& values);

    /// The keys of `values` this build does not keep in a profile (from a
    /// newer build, or not profile settings). Wiring keys are not counted.
    static QStringList unknownProfileKeys(const QHash<QString, QVariant>& values);

    /// Why imported `values` cannot be stored, in plain words naming the
    /// setting, or empty when every known key holds a value its setting
    /// takes: True or False for a switch, a number in its range (the
    /// ranges TransmitModel::settingRangeRefusal refuses on the link), a
    /// readable equalizer curve. Unknown and wiring keys are not checked.
    static QString importValuesRefusal(const QHash<QString, QVariant>& values);

    /// The saved keys of one profile under hardware/<mac>/tx/profile/.
    /// Empty when that radio has none. A remote window reads the Core's
    /// copy through the settings proxy.
    static QHash<QString, QVariant> readStoredProfile(const QString& mac,
                                                      const QString& name);

    /// The live transmit settings a profile save would store.
    static QHash<QString, QVariant> liveProfileValues(const TransmitModel* tx);

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

    // ── R-R3-49 (parity Task 3): a remote window's mirror of the Core ─────
    //
    // In a remote window the profiles are the Core's. setStationMirror()
    // turns this manager into a mirror: profileNames() and
    // activeProfileName() are the Core's (applyStationProfiles,
    // applyStationActiveProfile, fed from the mirrored `transmit` object),
    // and setActiveProfile / saveProfile / saveActiveProfile / deleteProfile
    // ask the Core through `requester` instead of changing anything here.
    // A mirror never reads or writes AppSettings (setMacAddress and load do
    // nothing), so this computer keeps no copy of the Core's profiles. The
    // combos show the Core's active profile when it arrives; a refused
    // request (reportStationRequestRefused) shows it again.
    enum class StationRequest { Select, Save, Delete, Import };
    Q_ENUM(StationRequest)
    /// Sends one request to the Core; false when it was not sent (the
    /// requester says why to the operator). `values` is the imported
    /// profile for Import, and empty for the others.
    using StationRequester = std::function<bool(StationRequest, const QString& name,
                                                const QHash<QString, QVariant>& values)>;
    void setStationMirror(StationRequester requester);
    bool isStationMirror() const { return static_cast<bool>(m_stationRequester); }
    /// The Core's profile list, in the Core's order.
    void applyStationProfiles(const QStringList& names);
    /// The Core's active profile.
    void applyStationActiveProfile(const QString& name);
    /// The Core refused a request: the combos show its active profile again.
    void reportStationRequestRefused();
    /// The Core answered request `commandId` (txProfile.*):
    /// stationRequestFinished. Ends the claim on it.
    void reportStationRequestFinished(StationRequest request, quint32 commandId, bool accepted,
                                      const QString& reason);
    /// The requester sent a request under this command id.
    void noteStationRequestSent(quint32 commandId);
    /// The command id of the latest request sent.
    quint32 lastStationRequestId() const { return m_lastStationRequestId; }
    /// A remote window: the TX Profile page sent request `commandId` and
    /// shows the Core's answer itself. While it is claimed, StationClient
    /// keeps its refusal out of the general refusal notices. A page that
    /// closes releases its claims, so the refusal is shown after all; a
    /// dropped link releases every claim.
    void claimStationRequest(quint32 commandId);
    void releaseStationRequest(quint32 commandId);
    void releaseAllStationRequests();
    bool stationRequestClaimed(quint32 commandId) const
    { return m_claimedStationRequests.contains(commandId); }

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

    /// A remote window: the Core answered one request, with its reason
    /// when it refused.
    void stationRequestFinished(MicProfileManager::StationRequest request, quint32 commandId,
                                bool accepted, const QString& reason);

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
    /// 100 keys (the liveKeyList() in MicProfileManager.cpp; no two-tone);
    /// matches defaultProfileValues() in shape.
    static QHash<QString, QVariant> captureLiveValues(const TransmitModel* tx);

    /// Apply a profile's key→value hash back to a TransmitModel via the
    /// public setter API.  Idempotent on values that match the model's
    /// current state (TransmitModel setters guard against no-op writes).
    static void applyValuesToModel(const QHash<QString, QVariant>& values,
                                   TransmitModel* tx);

    /// Per-MAC scope.  Empty until setMacAddress() is called; all mutators
    /// no-op when empty.
    QString m_mac;

    // R-R3-49 (parity Task 3): a remote window's mirror of the Core.
    StationRequester m_stationRequester;
    QStringList m_stationNames;
    QString m_stationActive;
    quint32 m_lastStationRequestId{0};
    QSet<quint32> m_claimedStationRequests;
};

} // namespace NereusSDR
