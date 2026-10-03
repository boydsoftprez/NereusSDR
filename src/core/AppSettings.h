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
//   2026-09-23 - R-R3-21: migrateRenamedKeys() one-shot rename for keys
//                 whose writer and reader disagreed (WsjtxSpotLifetime).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-27 - Schema v9 (R-IOS-06, R-IOS-27): each slice's saved NR1
//                 values brought into Thetis's NR spinbox ranges once (old
//                 defaults to the new ones, out-of-range values clamped).
//                 J.J. Boyd (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
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
#include <QByteArray>
#include <QStringList>
#include <QVariant>
#include <QMap>
#include <QDateTime>
#include <QHostAddress>
#include <functional>
#include <optional>

namespace NereusSDR {

class ISettingsBackend;

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

    // Atomic persistence; failure leaves in-memory preferences available for retry.
    bool save(QString* error = nullptr);

    // Exact local store, independent of any installed SettingsProxy. The
    // importer replaces the whole store only after its owner has stopped
    // writing; callers must recreate consumers after success. No per-key
    // change notifications are emitted for this wholesale replacement.
    QByteArray exportLocalXml(QString* error = nullptr) const;
    static bool validateLocalXml(const QByteArray& xml, QString* error = nullptr);
    bool importLocalXml(const QByteArray& xml, QString* error = nullptr);

    // Get/set top-level settings.
    QVariant value(const QString& key, const QVariant& defaultValue = {}) const;
    void setValue(const QString& key, const QVariant& val);
    void remove(const QString& key);
    bool contains(const QString& key) const;
    // Return every top-level key currently in the settings store.
    // Used by the MMIO engine to group keys under the MmioEndpoints/
    // prefix at app startup.
    //
    // Remote Daemon R2, Task 13 -- several other consumers across core
    // and gui prefix-scan this to enumerate settings (including at least
    // one GUI consumer that runs in remote mode too). allKeys() is
    // already the single funnel point (== m_settings.keys(), the same
    // map setValue()/remove() write through), so every consumer is
    // already positioned to pick up Task 15's delegation once it lands
    // here -- no consumer changes were needed for this task. Deliberately
    // not enumerated by filename: that list is stale the first time a
    // consumer is added or removed, and nothing here enforces it.
    QStringList allKeys() const;

    // Remove ALL top-level keys from the in-memory store.
    // Intended for test isolation. Does NOT call save().
    void clear();

    // ------------------------------------------------------------------
    // Remote Daemon R2, Task 13 -- change-hook seam.
    //
    // Every mutation that reaches the top-level settings store funnels
    // through setValue()/remove() now -- saveRadio, forgetRadio,
    // clearSavedRadios, setLastConnected, setDiscoveryProfile,
    // setHardwareValue, clearHardwareValues, and setModelOverride all
    // call one of the two internally rather than touching the storage
    // map directly. Installing a hook here therefore observes all of
    // them through a single seam. Fires AFTER the in-memory store has
    // already been updated, so a hook body that reads back via
    // value()/contains()/hardwareValue() sees the new state. Fires once
    // per key touched: a multi-field call like saveRadio() fires once
    // per field it writes, not once per call, because a future
    // delegation backend (SettingsProxy, Task 15) needs per-key
    // granularity to mirror individual values, not a single "something
    // changed" pulse.
    //
    // A fire does NOT imply the value actually changed. setValue() fires
    // even when writing a value identical to what was already stored,
    // and remove() fires whether or not the key existed beforehand (for
    // example, setLastConnected(QString()) on a store that never had
    // "radios/lastConnected" still fires with that key). Harmless for an
    // idempotent mirror; wrong for anything counting deltas, which must
    // diff against the prior value itself.
    //
    // Deliberately does NOT fire for:
    //   - clear(): a bulk test-isolation wipe with no single key to
    //     report (see its doc comment above).
    //   - load()'s corrupt-file recovery fallback: the same shape as
    //     clear() (a wholesale reset, not a value change) triggered
    //     internally rather than by a caller, and it runs at startup
    //     before any caller could plausibly have installed a hook that
    //     would mean anything yet. Treated identically to clear() so the
    //     two bulk-wipe paths stay consistent with each other.
    //   - plain reads (value(), hardwareValue(), hardwareValues(),
    //     contains(), allKeys() on their own).
    //   - stationValue()/setStationValue(): a separate map
    //     (m_stationSettings) entirely outside this task's scope.
    //
    // Re-entrancy is NOT guarded. A hook body that calls
    // setValue()/remove()/saveRadio() (or anything else routed through
    // those two) on the SAME instance re-enters this hook and will
    // recurse without bound. A delegation backend applying an inbound
    // remote write must suppress its own hook rather than relying on
    // AppSettings to break the cycle -- see this project's CLAUDE.md,
    // "GUI to Model Sync (No Feedback Loops)", for the same pattern
    // applied elsewhere in this codebase. No suppression is implemented
    // here on purpose: it would presume a Task 15 design nobody has
    // written yet, and a naive one would silently swallow legitimate
    // derived writes a hook body makes, trading a loud stack overflow
    // for a quiet data-loss bug.
    //
    // The hook body runs synchronously, on whatever thread called the
    // mutator. AppSettings has no internal locking; synchronizing a hook
    // against an instance touched from more than one thread is entirely
    // the caller's responsibility.
    //
    // AppSettings has no Q_OBJECT (see the class doc above), so this
    // cannot be a Qt signal -- std::function is the mechanism the R2
    // Task 13 brief specifies. Instance-scoped (a plain member, not a
    // static), because AppSettings(filePath) is a real construction path
    // every isolated test uses, and each instance's hook must stay
    // independent of every other instance's.
    //
    // Pass an empty std::function (or nullptr) to stop observing.
    // ------------------------------------------------------------------
    void setChangeHook(std::function<void(const QString& key)> hook);

    // ------------------------------------------------------------------
    // Remote Daemon R2, Task 15 -- the delegation seam.
    //
    // Non-owning, nullptr default. When installed, value(), setValue()
    // and contains() each start with the SAME one-branch guard: "if a
    // backend is installed AND it claims this key
    // (ISettingsBackend::handlesKey()), delegate the whole call and
    // return -- otherwise fall through to the body these methods have
    // always run." allKeys() is additive instead (unions its own local
    // keys, MINUS any the backend now claims, with the backend's own
    // handledKeys()) since there is no single key for a guard clause to
    // test -- see its own .cpp comment for why the local half must
    // exclude backend-claimed keys (fix round 1 (review), Important 1).
    // remove() is the ONE exception to strict one-branch delegation: it
    // still delegates to a claiming backend, but ALSO always clears any
    // local m_settings leftover for that key, because removal (unlike a
    // read, which must pick a single source of truth, or a write, which
    // must never let local and station data cross) is safe to apply to
    // both -- see remove()'s own .cpp comment (fix round 1 (review),
    // Important 1) for why a purely-delegated remove() left "forget
    // radio" unable to actually forget a key left over from a previous
    // LOCAL session.
    //
    // setRemoteBackend(nullptr) -- the default, and the state of every
    // AppSettings instance nothing has opted into remote mode -- means
    // every one of these guards is false on every call, so the local
    // path below them is BYTE-IDENTICAL to what it was before this task:
    // nothing between here and that fallback code changed, only a
    // skipped conditional was added above it.
    // tst_settings_proxy.cpp's nullBackendLeavesLocalPathByteIdentical
    // pins this directly.
    //
    // ---- Reads never touch the network (a header invariant, asserted
    // ---- by the caller of this seam, ISettingsBackend.h) ----
    //
    // AppSettings::value() runs inside widget and model constructors --
    // SetupDialog alone default-constructs on the order of 187 controls
    // that each read a setting in their own constructor body, on the GUI
    // thread, before any event loop exists to service anything else. A
    // delegated value()/contains() call that spins a nested event loop or
    // blocks on a socket read deadlocks the GUI the first time Setup
    // opens against a real link -- this is NOT a hypothetical: it is
    // exactly the kind of defect that passes a unit test against a fast
    // loopback fake and only shows up on a bench with real network
    // latency. The synchronous, cache-only contract is stated in full,
    // and its runtime proxy (tst_settings_proxy.cpp's
    // valueNeverBlocksOrSpinsEventLoop) lives, on ISettingsBackend.h --
    // this comment exists so a reader arriving from THIS seam, rather
    // than from the interface itself, does not miss it.
    //
    // The only shipped implementation is SettingsProxy
    // (src/core/settings/SettingsProxy.h), installed on a remote-mode
    // GUI's AppSettings singleton. The daemon's own AppSettings never has
    // a backend installed -- SettingsProxyServer (same directory) wraps
    // it from the OUTSIDE, through this class's existing public API plus
    // setChangeHook() above, rather than being installed as one.
    // ------------------------------------------------------------------
    void setRemoteBackend(ISettingsBackend* backend) { m_remoteBackend = backend; }
    ISettingsBackend* remoteBackend() const { return m_remoteBackend; }

    // ------------------------------------------------------------------
    // Remote Daemon R2, Task 15 -- bulk, prefix-scoped extraction.
    //
    // Returns every key in THIS instance's own m_settings map whose text
    // starts with at least one of `prefixes`, as FULLY QUALIFIED keys
    // (deliberately unlike hardwareValues(), which strips its
    // "hardware/<mac>/" prefix down to bare keys -- see that method's own
    // doc comment, which points back here) mapped to their raw stored
    // QString values (not QVariant: setValue() already collapses to
    // QString before storing, and value() only wraps it in a QVariant on
    // the way OUT -- there is no richer type to preserve on the way in).
    // An empty `prefixes` list matches nothing (not "everything" --
    // callers that want everything should pass an explicit prefix
    // matching every key they care about, or use allKeys() directly).
    //
    // Pure read: does not consult m_remoteBackend and is not one of the
    // five delegated accessors above. Intended for a DAEMON's own
    // AppSettings instance -- one that never has a remote backend
    // installed -- to pull out the subset a connecting client needs; see
    // SettingsProxyServer::buildSnapshot() (src/core/settings/
    // SettingsProxyServer.h), which is the production caller and which
    // also explains why it does NOT simply pass every Station-classified
    // prefix here (hardware/ is per-MAC and needs its own scoping; every
    // other Station family is instead found by scanning allKeys() with
    // classifySettingsKey() directly, to stay in sync with Task 14's
    // rule table without re-deriving it as a second, driftable copy).
    //
    // There is no matching applySnapshot() on THIS class. See
    // SettingsProxy::applySnapshot() for where the client-side merge
    // happens instead, and its class comment for why it is deliberately
    // NOT here: merging a station's ~2,900 keys into a remote-mode GUI's
    // own m_settings would let that GUI's own save() write them into its
    // local NereusSDR.settings file, contaminating one operator's local
    // store with another station's data. snapshot() only ever reads.
    // ------------------------------------------------------------------
    QMap<QString, QString> snapshot(const QStringList& prefixes) const;

    // Per-station settings (nested under <StationName> element).
    QVariant stationValue(const QString& key, const QVariant& defaultValue = {}) const;
    void setStationValue(const QString& key, const QVariant& val);

    // Station name (defaults to "NereusSDR").
    QString stationName() const;
    void setStationName(const QString& name);

    // File path for the settings file.
    QString filePath() const { return m_filePath; }

    // ------------------------------------------------------------------
    // Crash-safety + corruption recovery (issue #241).
    //
    // load() may detect a corrupt settings file (leading zero bytes from
    // an NTFS journal rollback, a mid-stream XML parse failure, or any
    // other condition that prevents a clean parse). When that happens:
    //
    //   1. The corrupt file is renamed in place to
    //      "<filePath>.corrupt-YYYYMMDD-HHMMSS" so the user can attempt
    //      manual recovery (or hand it to a developer).
    //   2. If a "<filePath>.bak" sidecar exists from the previous good
    //      save() it is parsed; on a clean parse those values become the
    //      live settings and the next save() rewrites .bak as the
    //      one-deep history again.
    //   3. If .bak is missing or also corrupt the in-memory state is
    //      empty (defaults will be written on the next save()).
    //
    // load() also handles the "orphan .bak" case (PR #244 follow-up):
    // when main is *missing* but .bak exists, recovery is attempted from
    // .bak before declaring first-run.  This closes the data-loss hole
    // where the corrupt-preserve rename in (1) leaves main missing on
    // disk; if the user kills the app between recovery and the next
    // save(), the next launch must still find the .bak rather than
    // silently restoring defaults.  In that case wasCorruptedOnLoad()
    // stays false (no corruption was observed *this* load) but
    // recoveredFromBackup() is true.
    //
    // wasCorruptedOnLoad() returns true once load() has hit case (1).
    // preservedCorruptFilePath() returns the renamed path for use in a
    // post-startup UI notification. recoveredFromBackup() reports
    // whether case (2) succeeded.  All three are query-only and reset
    // on the next load().
    // ------------------------------------------------------------------
    bool    wasCorruptedOnLoad() const     { return m_wasCorruptedOnLoad; }
    QString preservedCorruptFilePath() const { return m_preservedCorruptFilePath; }
    bool    recoveredFromBackup() const    { return m_recoveredFromBackup; }

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

    // ------------------------------------------------------------------
    // Remote Daemon R2, Task 1 -- nereusd's own reserved profile.
    //
    // Before this task, nereusd's --profile silently resolved to the
    // empty/default profile whenever the flag was omitted entirely,
    // sharing the GUI client's own settings/log directory. That let R2's
    // state-mirroring feature (SettingsProxy, R2 Task 15) pass its own
    // verification while doing nothing at all: a "remote" GUI reading the
    // same on-disk file the local daemon just wrote looks correct whether
    // or not anything was actually mirrored over the wire. See
    // docs/architecture/2026-08-03-remote-daemon-r2-r3-design-addendum.md
    // §2.1. DaemonConfig.h's resolveDaemonProfileArgument() is where the
    // --profile CLI argument resolves to this constant; an operator can
    // still opt back into sharing by passing an explicit --profile "".
    // (Unrelated to the "Phase 3I Task 15" cited just below -- both
    // epics happened to number a task 15.)
    // ------------------------------------------------------------------
    static constexpr const char* kDaemonProfileName = "daemon";

    // First-run marker for a freshly-reserved profile. R2 Task 15's Setup
    // gate needs to tell "this profile's settings snapshot is empty
    // because it is a legitimately fresh nereusd --profile daemon" apart
    // from "the snapshot is empty because something is broken". The key
    // is deliberately UNPREFIXED (no "daemon/" or "hardware/<mac>/"
    // scoping) so it falls inside R2 Task 15's snapshot of top-level keys
    // (see allKeys()). Callers must reference this constant rather than
    // hardcode the string literal.
    static constexpr const char* kDaemonProfileSeededKey = "DaemonProfileSeeded";

    // Idempotent: writes kDaemonProfileSeededKey = "True" (and save()s)
    // only the first time this is called against a given settings store,
    // so it is safe to call unconditionally on every daemon startup.
    // Instance-scoped rather than routed through the instance() singleton,
    // so it is directly unit-testable via the AppSettings(filePath)
    // constructor.
    void seedDaemonProfileMarker();

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
    // R-R3-21: changes only a saved radio's auto-connect flag (saveRadio
    // would also rewrite its other fields and its lastSeen time). No-op for
    // a radio that is not saved.
    void setRadioAutoConnect(const QString& macKey, bool autoConnect);

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

    // Stable namespace for the new DSP settings. Invalid/non-MAC identities
    // return empty, so an unconnected radio cannot inherit another's tuning.
    static QString normalizedRadioMac(const QString& mac);
    // Called during load, before a new connection can change lastConnected.
    // Migrates only legacy NR selection for the saved owner, preserving keys.
    void migrateLegacyNnrSettings();

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

    // PGXL/TGXL peripherals (Phase 3P-II baseline)
    // Empty manualIp disables auto-connect; ports default per FlexRadio API
    //   PGXL_ManualIp      string  ""     (default empty)
    //   PGXL_ManualPort    int     9008
    //   TGXL_ManualIp      string  ""
    //   TGXL_ManualPort    int     9010

    // PGXL/TGXL connection robustness (Phase 3P-II Phase 3, Tasks 58+59+60)
    // Wire formats from FlexRadio PowerGenius Ethernet API wiki spec (design §6.4).
    //   PGXL_KeepaliveSec   int     30    Cadence for keepalive status pokes.
    //   PGXL_PingSec        int     0     Auto-ping interval in seconds (0 = off, the
    //                                     default: the Core and the desktop both leave
    //                                     it off; the amp's reply to `ping` has never
    //                                     been captured).
    //   PGXL_AutoReconnect  bool   "True" Enable exponential-backoff auto-reconnect on drop.
    //                                     Backoff sequence: 1/2/5/10/30/60 s (cap at 60 s).
    //
    // PGXL pairing-flow (Phase 3P-II Phase 3, Task 62)
    // Used by RadioModel's connected-lambda to configure amplifierCreate + flexradioPair.
    //   PGXL_AntMap        string "ANT1:PORTA,ANT2:PORTB"
    //                                     Antenna port mapping passed to amplifierCreate;
    //                                     comma-separated RADIO_ANT:AMP_PORT pairs.
    //   PGXL_PairAttempt   bool   "True"  Gate for flexradioPair call after amplifierCreate.
    //                                     Set to "False" to skip pairing (amp standalone).
    //   PGXL_FlexAmpSlice  string "A"     Slice letter (A/B/C/D) passed to flexradioPair
    //                                     and setBand.
    //   PGXL_TxAnt         string "ANT1"  TX antenna name passed to flexradioPair.
    //   PGXL_FlexRadioSerial string ""   (default: derived from MAC; format XXXX-XXXX-XXXX-XXXX)
    //                                     Override when the auto-derived serial collides with
    //                                     another NereusSDR installation on the same PGXL.
    //   PGXL_BroadcastDiscovery string "True"       Toggle the 1 Hz UDP 4992 SmartSDR-format
    //                                               discovery beacon. PGXL/TGXL listen for these
    //                                               to populate their FlexRadio dropdown.
    //   PGXL_BroadcastNickname  string "NereusSDR"  Nickname shown in PGXL UI.
    //   PGXL_DiscoveryModel     string "FLEX-6400"  Model string in the SmartSDR discovery beacon;
    //                                               must match a real Flex model for PGXL to
    //                                               accept the broadcast and populate its dropdown.
    //   PGXL_PairModel          string "FLEX-8600M" Model passed to amplifierCreate when pairing.
    //
    // TGXL connection robustness (Phase 3P-II Phase 3, Task 60)
    // Wire formats from design §4.2.1 + §6.4 (4O3A TGXL Ethernet API).
    //   TGXL_KeepaliveSec   int     30    Cadence for keepalive status pokes.
    //   TGXL_PingSec        int     10    Auto-ping interval (0 = disabled); wired in Task 67.
    //   TGXL_AutoReconnect  bool   "True" Enable exponential-backoff auto-reconnect on drop.
    //                                     Backoff sequence: 1/2/5/10/30/60 s (cap at 60 s).

    // Phase 3P-II Phase 4 (Advanced UI + helpers)
    //
    // FaultLog ring buffer (Task 75). JSON arrays; newest entry first.
    //   PGXL_FaultHistory   string  ""    Up to 10 FaultEvent JSON objects.
    //   TGXL_FaultHistory   string  ""    Up to 10 FaultEvent JSON objects.
    //   RfKit_FaultHistory  string  ""    Up to 10 FaultEvent JSON objects (R-R3-47).
    //   Each element: { "whenMs":<qint64>, "state":<str>,
    //                   "fwdAtFaultW":<float>, "swrAtFault":<float>,
    //                   "tempAtFaultC":<float>, "likelyCause":<str>,
    //                   "device":<"pgxl"|"tgxl"|"rfkit">, "text":<plain words>,
    //                   "detail":<the device's own words> }   (the last three R-R3-47)
    //   On the Core these are written to disk within half a second of a
    //   fault or a clear (StationAccessoryData), not only at a clean stop.
    //
    // TuneMemoryStore per-(antenna,band) relay cache (Task 76).
    //   TGXL_TuneMemory_Ant<N>_Band<M>  string  ""
    //     One key per (antenna, band) pair. N is 1..3; M is Band::bandKeyName()
    //     suffix (e.g. "20m", "160m", "GEN"). Value is a JSON object:
    //     { "c1":<int>, "l":<int>, "c2":<int>, "savedAt":<qint64> }
    //   TGXL_AutoTuneMemoryRecall  bool  "False"
    //     When "True", SliceModel::bandChanged triggers auto-recall if a
    //     memory slot exists for the new (antenna, band) combination.
    //
    // TgxlAdvancedPage identity + antenna labels (Task 85).
    //   TGXL_Nickname   string  ""    Operator-assigned nickname for the TGXL device.
    //   TGXL_Ant1_Label string  ""    User-defined label for ANT 1 (e.g. "80 m dipole").
    //   TGXL_Ant2_Label string  ""    User-defined label for ANT 2 (e.g. "vertical").
    //   TGXL_Ant3_Label string  ""    User-defined label for ANT 3 (e.g. "beverage").
    //     Labels propagate to TunerApplet antenna buttons (Task 95).
    //
    // TxInterlockPolicy (Task 77).
    //   PGXL_TxInterlockMode    string  "Disabled"  "Disabled" / "Warn" / "Block"
    //   PGXL_TxInterlockGraceMs int     3000        Grace period (ms) after OPERATE-on
    //                                               before SWR gate activates.
    //   PGXL_TxSwrGate          bool    "False"     Gate TX when SWR exceeds swrGateMax.
    //   PGXL_TxSwrGateMax       float   "3.0"       SWR limit for the gate (dimensionless).
    //
    // PGXL power-cap soft-alert (Task 97).
    //   PGXL_PowerCapEnabled  string  "False"  Soft-alert only; TX is not blocked.
    //   PGXL_PowerCapW        int     1500     Forward-power threshold (watts) for the
    //                                          toast; de-bounced per exceedance event.
    //
    // PgxlAdvancedPage identity + hardware, saved beside the amp's own setup
    // commands (and by the Core for a window's request, R-R3-47 / R-R3-22).
    //   PGXL_Nickname      string  ""        Operator-assigned nickname for the PGXL.
    //   PGXL_BiasMode      string  "ClassAB" "ClassA" / "ClassAB".
    //   PGXL_FanMode       string  "Auto"    "Auto" / "Quiet" / "Continuous".
    //   PGXL_LedIntensity  int     75        Front-panel LED brightness, 0 to 100.
    //
    // Station accessory switches and addresses, per radio under
    // hardware/<mac>/peripherals/ (R-R3-47; the Core's own on a headless Core).
    //   FourO3A_Enabled    bool    "False"   The 4O3A (Power Genius, Tuner Genius) switch.
    //   RfKit_Enabled      bool    "False"   The RF-Kit RF2K-S switch.
    //   RfKit_ManualIp     string  ""        The RF2K-S address.
    //   RfKit_ManualPort   int     8080      The RF2K-S REST port.
    //
    // RF-Kit RF2K-S (Phase 3P-III; station-wide, R-R3-47).
    //   RfKit_AutoReconnect  bool    "True"  Retry after a lost connection.
    //   RfKit_PollIntervalMs int     1000    REST poll cycle, 250 to 5000 ms. A remote
    //                                        window's change reaches the Core's amp at
    //                                        once (remoteRfKitControlVersion 3).
    //   RfKit_Ant1_Label .. RfKit_Ant4_Label  string  ""  User-defined antenna names.
    //
    // The Core's station TCI server (R-R3-48), behind setStationTci.
    //   StationTci_Enabled bool    "False"   Whether the station TCI server runs.
    //   StationTci_Port    int     50001     Its port.

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

    // v0.3.0 settings schema migration. Call once at app startup (after load()).
    // Detects pre-v0.3.0 state (SettingsSchemaVersion missing or < currentVersion)
    // and applies all pending migrations in version order. Idempotent.
    //
    // Currently: v0 → v3 (covers v0.2.x → v0.3.0)
    //   - Removes DisplayAverageMode (split into Detector + Averaging in Task 2.1)
    //   - Removes DisplayPeakHold + DisplayPeakHoldDelayMs (→ ActivePeakHold keys, Task 2.5)
    //   - Removes DisplayReverseWaterfallScroll (W5 removed in Task 2.8)
    //   - v7 (R-R3-49): resets NetworkWatchdogEnabled once
    //   - v8 (R-R3-49): drops TciRateLimitMsgsPerSec (old msg/s unit) once
    //   - v9 (R-IOS-06, R-IOS-27): each slice's saved NR1 values into
    //     Thetis's NR spinbox ranges once (old defaults to new, clamps)
    //   - Sets SettingsSchemaVersion=currentVersion
    void ensureSettingsAtVersion(int currentVersion);

    // One-shot migration: legacy global "hl2IoBoard/n2adrFilter" (Bug 2 in
    // hermes-filter-debug) → per-MAC "hardware/<mac>/hl2IoBoard/n2adrFilter"
    // for every saved radio whose boardType is HermesLite or HermesLiteRxOnly
    // (the receive-only kit, Task 16). Removes the global
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

    // R-R3-21: Setup's Penny Ext Control checkbox used to save a global
    // "hardware/oc/pennyExtCtrl" that nothing read; PennyLaneController
    // reads per-MAC "hardware/<mac>/penny/extCtrlEnabled". Copies the
    // global value to every saved radio that has no value of its own, then
    // removes the global key (kept while no radio is saved, or while a
    // manual radio is saved without its real MAC, so a later launch can
    // still carry it over), so a radio added later starts
    // at Thetis's default (True) instead of the old global value.
    // Same shape as migrateLegacyN2adrFilter. Idempotent.
    static void migrateLegacyPennyExtCtrl(AppSettings& s);

    // R-R3-21: one-shot renames for settings whose writer and reader used
    // different names, so the saved value was never read. Each old name is
    // read once and written under the name everything now uses, then
    // removed; a value already saved under the new name wins. Idempotent.
    //   WsjtxSpotLifetime -> WsjtxSpotLifetimeSec (Spot Hub WSJT-X Spot Life)
    // Penny Ext Control's rename is per radio and happens in
    // PennyLaneController::load().
    static void migrateRenamedKeys(AppSettings& s);

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

    // Remote Daemon R2, Task 13 -- see setChangeHook()'s doc comment
    // above. Instance-scoped; empty (default-constructed) means "no
    // observer", checked before every fire.
    std::function<void(const QString& key)> m_changeHook;

    // Remote Daemon R2, Task 15 -- see setRemoteBackend()'s doc comment
    // above. Non-owning; nullptr means "not in remote mode", checked
    // before every delegated call.
    ISettingsBackend* m_remoteBackend = nullptr;

    // Issue #241 — corruption-recovery diagnostics (cleared at the top of
    // every load()).
    bool    m_wasCorruptedOnLoad{false};
    QString m_preservedCorruptFilePath;
    bool    m_recoveredFromBackup{false};
};

} // namespace NereusSDR
