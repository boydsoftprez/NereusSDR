#pragma once
// =================================================================
// src/core/settings/SettingsProxy.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Remote-daemon R2 Task 15.
//
// The client-side half of the settings mirror. A remote-mode GUI installs
// one instance via AppSettings::instance().setRemoteBackend(&proxy);
// from that point on every Station-classified key (SettingsScope.h, Task
// 14) reads and writes through this class's own in-memory cache instead
// of the local NereusSDR.settings file, while every OperatorLocal key
// keeps flowing through AppSettings's ordinary local path completely
// untouched -- see ISettingsBackend.h for the delegation seam this
// implements and why the split happens per-key rather than "whichever
// store is installed handles everything."
//
// ---- Why the cache is NOT AppSettings's own m_settings map ----
//
// A tempting shortcut is: proxy the ENTIRE key space, including
// OperatorLocal keys, and just reuse AppSettings's own m_settings QMap
// as the cache (applySnapshot() merging straight into it). Rejected: a
// remote-mode GUI still wants ITS OWN window geometry, trace colours and
// local sound-card selection to persist locally across launches,
// independent of which station it happens to be connected to this
// session, and AppSettings::save() serialises m_settings wholesale to
// that GUI's own NereusSDR.settings file. If a station's ~2,900 keys
// (Step 5's measured snapshot scope) were merged into that same map,
// the NEXT save() -- and every debounced settings-save timer already
// wired throughout the app, none of which know about Role::Remote --
// would write every one of them into the operator's own local file,
// which then out-of-syncs the moment they connect to a DIFFERENT
// station, or reappears as stale ghost values on a later LOCAL-mode
// launch. AppSettings gains a SEPARATE, symmetric extraction method
// instead (AppSettings::snapshot(prefixes), used server-side by
// SettingsProxyServer::buildSnapshot() -- see that class) so the
// station's real values are read out of the DAEMON's own store, and this
// class's m_cache is the only place they are ever merged INTO on the
// client side: purely in memory, never touched by AppSettings::save().
//
// ---- The three-state read ----
//
// A key this proxy handles (handlesKey() below) resolves through
// value()/contains() to one of three states, tracked by two members:
//
//   1. CACHE HIT       -- m_cache has a real value (from a snapshot, or
//                          from this client's own optimistic write).
//   2. PROVEN UNSET     -- no entry in m_cache, but at least one snapshot
//                          HAS been applied (m_snapshotEverApplied), so
//                          the absence is read as "the daemon's own
//                          AppSettings::value() would ALSO return the
//                          caller's default for this key" -- the same
//                          shape of fact hardwareValue()'s own bare-
//                          default fallback represents on the daemon
//                          side. Recorded into m_provenUnset lazily, on
//                          first read, purely for diagnostics (Step 9's
//                          log -- see below) and for tests
//                          (provenUnsetKeys()); it never changes what
//                          value() RETURNS, which is `defaultValue`
//                          either way.
//   3. NO SNAPSHOT YET  -- no entry in m_cache and NO snapshot has ever
//                          been applied. Still returns `defaultValue`
//                          (there is nothing else a synchronous,
//                          network-free read could do -- see
//                          ISettingsBackend.h's invariant), but is a
//                          DIFFERENT fact worth telling apart in the log:
//                          "we don't know yet" rather than "we asked and
//                          the answer was no."
//
// All three states satisfy "value() returns the caller's default for an
// unwritten key" (AppSettings.h's own contract, Task 13's
// unwrittenKeyStillReturnsCallerDefault, and the controller notes' own
// Region/"United States" example) -- the distinction exists purely for
// Step 9's log and does not change any return value.
//
// ---- Step 9: the proxied-read log ----
//
// Every call into value() that this backend actually answers (i.e. every
// call AppSettings delegated here via handlesKey()) logs one line via
// qCDebug(lcSettingsProxy), unconditionally, with the literal, greppable
// first token "proxied-read" so `grep proxied-read <log>` produces
// Step 9's scannable list. Exact shape (space-separated qCDebug streaming,
// one line per read):
//
//   proxied-read <key> outcome=CacheHit value="<value>"
//   proxied-read <key> outcome=ProvenUnset default="<defaultValue>"
//   proxied-read <key> outcome=NoSnapshotYet default="<defaultValue>"
//
// tests/tst_settings_proxy.cpp's proxiedReadLogHasScannableShape case
// installs a message handler and greps for exactly this.
//
// ---- Optimistic writes, origin tags, and rejection ----
//
// setValue() (ISettingsBackend) updates m_cache immediately -- BEFORE any
// daemon confirmation -- so a Setup control reads back what the operator
// just set on its very next value() call, then (while ready()) emits
// outboundWriteRequested() for a live session (Task 18) to relay over
// the wire tagged with localOriginTag(). AppSettings has no signals of
// its own (it is not a QObject -- AppSettings.h's own class comment), so
// every notification this class needs to give a caller lives here.
//
// The origin tag is a SESSION identifier, not a per-write sequence
// number: Task 18 assigns this client's own session/connection id once
// (setLocalOriginTag()) and tags every write this client sends with it.
// The daemon (SettingsProxyServer) echoes that SAME tag back on the
// resulting outbound broadcast to EVERY connected client, including the
// one that sent it. applyRemoteValue() below applies the incoming value
// to m_cache regardless of whose tag it carries (the daemon's report is,
// by definition, the current truth), but a caller with a live
// slider-drag in progress can compare `originTag == localOriginTag()`
// itself to decide "this is my own echo, I already have this" versus "a
// third party (or the daemon itself) changed this" without this class
// needing to guess at UI intent it has no visibility into. This class
// does not attempt to solve out-of-order redelivery across MULTIPLE
// in-flight writes from the SAME client (e.g. a rapid slider drag
// producing several writes before any confirmation returns); the
// transport is a single ordered reliable channel (R2 design addendum
// section 7.3's "Control" envelope), so writes and their confirmations
// arrive in the order they were sent, which is what keeps "same tag,
// last write" sufficient without a sequence number.
//
// Rejection (Step 6) reverts m_cache to whatever the daemon reports as
// the restored value (an invalid QVariant means "the daemon has nothing
// for this key either" -- proven-unset, not merely reset to empty
// string) and emits valueRejected() unconditionally, so a Setup widget
// can revert its own displayed value.
//
// ---- Offline behaviour (Step 8) ----
//
// ready() gates the OUTBOUND side only: while false, setValue()/remove()
// still update m_cache (so the UI stays interactive and consistent
// during an outage) but do NOT emit outboundWriteRequested()/
// outboundRemoveRequested() -- there is no live session to send them
// over, and Step 8 is explicit that a dropped write must not be queued
// for later replay (the daemon's own store can move underneath a queued
// write between now and reconnect; blind replay would silently revert
// someone else's change). Reconnect is Task 19's job: it re-establishes
// ready() and calls applySnapshot() again with a fresh snapshot, which
// -- being a MERGE, not a replace -- naturally supersedes whatever this
// client held locally for every key the fresh snapshot covers.
//
// Fix round 1 (review, Important 2): "dropped, not queued" originally
// destroyed the information Task 19 needs at the moment of the drop,
// rather than merely deferring it -- nothing recorded WHICH keys had a
// pending edit while offline, so a reconnect had no way to tell "your
// offline edit was overwritten" apart from "the daemon changed this key
// while you were away". m_droppedWhileOffline (a QSet<QString>,
// droppedWhileOffline() below) is the minimum enabling state that
// closes this: setValue()/remove() insert into it in their !ready()
// branch. applySnapshot() is where it is read down: every key the fresh
// snapshot ALSO covers is a genuine contradiction (the daemon has its
// own authoritative value now, so whatever this client tried to set
// while offline did not reach it) and is recorded into
// keysContradictedByLastSnapshot() BEFORE the merge overwrites m_cache;
// m_droppedWhileOffline itself is then cleared in full, regardless of
// which members were contradicted -- a fresh full snapshot is new
// ground truth over its own scope, and carrying stale per-key drop
// bookkeeping across it would let a LATER, unrelated snapshot appear to
// "contradict" an edit that was actually resolved (or superseded by a
// newer local write) long before. This is intentionally NOT a value-level
// diff (this class does not retain the pre-snapshot cached value
// separately from what applySnapshot() merges over it) -- it is "this
// key had a pending offline edit AND the snapshot has an opinion about
// it".
//
// Whole-branch review, Important 2 -- who turns that into operator-facing
// language, since for two rounds the answer was "nobody". Task 19 was
// named here as the owner and never did it: both accessors were read only
// from tests, so a remote operator who changed a Setup control during an
// outage was told the LINK dropped (MainWindow's "Link to the Core lost"
// toast) and never that their EDIT had been thrown away -- while value()
// went on returning the offline value, so the control itself read back as
// applied. applySnapshot() now logs the contradicted key names at warning
// level and emits offlineEditsSuperseded(), which MainWindow::
// connectToStation() turns into a counted toast. The accessors stay: they
// are the introspectable form, and the tests read them.
//
// Fix round 2 (review, smaller item) -- what Task 19 is told, precisely:
// keysContradictedByLastSnapshot() names only the CONTRADICTED subset,
// not every key that was dropped while offline. An offline edit whose
// key the fresh snapshot does NOT cover at all leaves no record in
// EITHER set once applySnapshot() returns -- m_droppedWhileOffline was
// cleared in full (the paragraph above explains why), and that key was
// never inserted into keysContradictedByLastSnapshot() either, since the
// contradiction check only fires for keys present in `data`. The edit's
// VALUE is not lost (it is still sitting in m_cache, exactly where
// setValue() put it, and value() keeps returning it), but nothing
// records that it is a value the daemon never actually received. This
// is not a regression from any behaviour this class ever had -- Step 8
// was always "the daemon's store can move underneath a queued write" --
// and reviewed as an acceptable outcome, not a bug: it is the honest
// shape of "we can only tell you what the snapshot had an opinion
// about", not a promise to reconcile everything. Recorded here so Task
// 19 does not assume droppedWhileOffline()'s absence of a key, post-
// snapshot, means that key's edit is known-applied.
//
// ---- The Setup-dialog gate (Step 7) ----
//
// setupDialogAllowed() is the single predicate behind every SetupDialog
// construction. Task 20 wired it: MainWindow::createSetupDialog() is the
// only place in src/gui that runs `new SetupDialog`, all twelve former
// call sites go through it, and it asks
// setupDialogAllowedForCurrentBackend() (declared at the bottom of this
// header) which resolves the installed backend and delegates here. Until
// fix round 2 this sentence described an intention rather than a fact:
// the predicate had no production caller at all, so the gate was written
// and not hung. ready() alone is NOT sufficient: Task
// 1's daemon profile starts genuinely empty, so a freshly-reserved
// `nereusd --profile daemon` reports ready() (the handshake completed)
// with zero station settings for a Setup page to show. Without the
// second condition, 187 widget constructors would each read their
// AppSettings default, and the FIRST interaction with any one of them
// would write that ship default into the station store as if the
// operator had chosen it. AppSettings::kDaemonProfileSeededKey (Task 1)
// is what tells "empty because fresh" apart from "empty because
// something is broken": Task 1's seedDaemonProfileMarker() writes it
// unconditionally, idempotently, on every daemon startup, and
// SettingsProxyServer::buildSnapshot() includes it explicitly whenever
// present (see that class's comment) specifically so it survives even a
// snapshot that is otherwise completely empty. handlesKey() below
// carries the matching special case on the read side, because the key
// itself does not classify Station under SettingsScope.h's rules (it is
// not a "setting" in that sense at all -- it is this protocol's own
// bookkeeping) and would otherwise never reach this cache.
//
// Minor 10 (fix round 1 review): the asymmetry this creates is a DEAD
// PATH today, not a bug -- handlesKey() claims the seed marker (so a
// client-side setValue() on it would be cached and, if ready(), offered
// outbound), but SettingsProxyServer::applyInboundWrite() rejects it on
// arrival (it classifies OperatorLocal, and that method's gate is
// classifySettingsKey() == Station with no special case of its own).
// Nothing in this codebase ever calls setValue() on the seed marker from
// a client, so this never actually fires; recorded here in case a future
// caller does.
//
// ---- ready()==false is load-bearing beyond this class (record, not fix) ----
//
// Several model constructors -- SliceModel.cpp, NotchModel.cpp,
// FilterPresetStore.cpp, TciServer.cpp -- do a contains()-then-seed
// pattern against Station-classified key prefixes at construction time:
// if AppSettings doesn't have a value yet, they write one. On a
// remote-mode GUI those constructors run BEFORE any snapshot can
// possibly have landed (the same RadioModel-construction-before-
// handshake ordering FaultLog.h documents for reload()), so every one of
// those seed-if-absent writes reaches setValue() while this class's
// handlesKey() already routes the key here. What keeps them from
// silently baking ship defaults into the STATION store is entirely
// external to this class: ready() is false at that point, so the write
// updates m_cache but is dropped, not sent -- the exact Step 8 path,
// operating as an accidental safety net for a problem it was not
// designed to solve. This is correct TODAY only because nothing sets
// ready() before those constructors run. Task 20 needs to confirm that
// ordering explicitly rather than inherit it as an assumption -- a
// future change that flips ready() early (e.g. to unblock some other
// gate) would silently start writing client-observed ship defaults into
// the station store the first time any of those four classes
// constructs.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-06  J.J. Boyd / KG4VCF  Remote daemon R2 Task 15: client-side
//                                    settings proxy. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
//   2026-08-06  J.J. Boyd / KG4VCF  Fix round 1 (review): Important 2
//                                    (droppedWhileOffline() /
//                                    keysContradictedByLastSnapshot()),
//                                    Minor 8 (proven-unset gated on
//                                    m_snapshotEverApplied in remove()
//                                    too), Minor 10 (seed-marker
//                                    handlesKey()/applyInboundWrite()
//                                    dead-path note), the ready()==false
//                                    record-not-fix section. This entry
//                                    was missed on the .h file in that
//                                    round (only .cpp's history was
//                                    updated); added retroactively here.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
//   2026-08-06  J.J. Boyd / KG4VCF  Fix round 2 (review): documented
//                                    that keysContradictedByLastSnapshot()
//                                    names only the contradicted subset,
//                                    not every key dropped while
//                                    offline. AI-assisted transformation
//                                    via Anthropic Claude Code.
//   2026-08-08  J.J. Boyd / KG4VCF  Task 20 fix round 2 (review,
//                                    Important 2): added
//                                    setupDialogAllowedForCurrentBackend()
//                                    and corrected the Setup-gate section,
//                                    which described a caller that did not
//                                    exist. AI-assisted transformation via
//                                    Anthropic Claude Code.
//   2026-08-09  J.J. Boyd / KG4VCF  Whole-branch review, Important 4:
//                                    applyRemoteRemoval(), so a station
//                                    removal is cached as absence rather
//                                    than as an empty string. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
//   2026-08-09  J.J. Boyd / KG4VCF  Whole-branch review, Important 2:
//                                    offlineEditsSuperseded(), so the
//                                    offline-edit bookkeeping finally has
//                                    a production consumer. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
//   2026-09-28  J.J. Boyd / KG4VCF  hasNonEmptySnapshot() counts only
//                                    keys the Core sent (m_coreKeys), not
//                                    this window's own writes. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
// =================================================================

#include "core/NereusCoreExport.h"
#include <QMap>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariant>

#include "core/settings/ISettingsBackend.h"

namespace NereusSDR {

class NEREUS_CORE_EXPORT SettingsProxy : public QObject, public ISettingsBackend {
    Q_OBJECT

public:
    explicit SettingsProxy(QObject* parent = nullptr);
    ~SettingsProxy() override = default;

    SettingsProxy(const SettingsProxy&) = delete;
    SettingsProxy& operator=(const SettingsProxy&) = delete;

    // ---- ISettingsBackend ----
    bool handlesKey(const QString& key) const override;
    QVariant value(const QString& key, const QVariant& defaultValue) const override;
    void setValue(const QString& key, const QVariant& val) override;
    bool contains(const QString& key) const override;
    void remove(const QString& key) override;
    QStringList handledKeys() const override;

    // ---- Session identity (Task 18 sets this once it knows) ----
    void setLocalOriginTag(const QString& tag);
    QString localOriginTag() const { return m_localOriginTag; }

    // ---- Handshake / link state ----
    // False until Task 18's session completes its handshake (or after a
    // link drop, until reconnect completes again). Gates the OUTBOUND
    // side of writes/removes only -- see the class comment's "Offline
    // behaviour" paragraph. Reads always keep serving the cache
    // regardless of this flag, which is what "reads never touch the
    // network" (ISettingsBackend.h) means in practice: there is no
    // "ready" branch in value() at all.
    void setReady(bool ready);
    bool ready() const { return m_ready; }

    // ---- Connect-time (and reconnect-time) snapshot ingestion ----
    // Merges `data` into m_cache -- NEVER replaces it (Step 3: a second,
    // narrower-scoped snapshot must not blow away keys outside its own
    // scope). Marks m_snapshotEverApplied even when `data` is empty (an
    // empty snapshot is real information: see setupDialogAllowed()).
    // Clears any m_provenUnset entry a newly-arrived real value
    // contradicts.
    //
    // Whole-branch review, Minor 6: entries are filtered through
    // handlesKey() on the way in, so ISettingsBackend's
    // handledKeys() subset-of handlesKey() contract is structural rather
    // than a convention that happens to hold while both binaries share a
    // classifier build. A dropped entry is counted and logged once per
    // snapshot, not per key.
    void applySnapshot(const QMap<QString, QString>& data);

    bool hasReceivedSnapshot() const { return m_snapshotEverApplied; }

    // True once at least one snapshot has been applied AND the Core has
    // sent at least one key besides AppSettings::kDaemonProfileSeededKey
    // (in a snapshot, a remote value or a rejection's restored value).
    // This window's own writes never count. See the class comment: the
    // seed marker alone does not count as "real" station content, which
    // is exactly what lets the OR-fallback in setupDialogAllowed() below
    // do anything.
    bool hasNonEmptySnapshot() const;

    // Step 7's Setup-dialog gate: ready() AND (hasNonEmptySnapshot() OR
    // the seed marker is present). See the class comment.
    bool setupDialogAllowed() const;

    // Fix round 1 (review, Important 2) -- see the class comment's
    // "Offline behaviour" section for the full contract.

    // Keys whose setValue()/remove() call was dropped (not queued)
    // because !ready() at the time. Grows while offline; fully cleared
    // by the NEXT applySnapshot() call, regardless of which members that
    // snapshot actually covered.
    QSet<QString> droppedWhileOffline() const { return m_droppedWhileOffline; }

    // The subset of droppedWhileOffline() -- as it stood immediately
    // before the MOST RECENT applySnapshot() call -- that snapshot also
    // reported a value for. Recomputed (and everything else discarded)
    // on every applySnapshot() call; empty before the first one.
    QSet<QString> keysContradictedByLastSnapshot() const { return m_lastSnapshotContradictions; }

    // ---- Inbound from the daemon (Task 18 calls these per decoded wire
    // message) ----

    // A settings value the daemon reports as current for `key`, whether
    // a genuine third-party/daemon-local change or the echo of this
    // client's own write (see the class comment's origin-tag paragraph).
    // Applied to m_cache unconditionally.
    void applyRemoteValue(const QString& key, const QVariant& value, const QString& originTag);

    // The daemon reports `key` as GONE from the station's store, whether
    // a daemon-local removal or the echo of this client's own remove().
    //
    // Whole-branch review, Important 4. Until this existed there was no
    // way to represent "removed" as distinct from "empty" on this side:
    // the daemon's removal broadcast carried an invalid QVariant that the
    // relay flattened to "", applyRemoteValue() cached that, and the key
    // came back from the dead as an empty string -- contains() true here
    // and false on the daemon, value(key, someDefault) returning "" where
    // the caller's default was the whole contract (SettingsProxy's own
    // "Absent keys must stay absent"). Leaves the key PROVEN UNSET, which
    // is exactly what the daemon just asserted, the same way
    // applyRejection() below treats an invalid restored value.
    void applyRemoteRemoval(const QString& key);

    // The daemon rejected a write this client sent for `key`.
    // `restoredValue`, if valid, becomes the new cached value; an
    // invalid QVariant means the daemon has nothing for this key either
    // (reverts to proven-unset, not to an empty string). Always emits
    // valueRejected().
    void applyRejection(const QString& key, const QVariant& restoredValue);

    // ---- Test / diagnostic introspection ----
    QSet<QString> provenUnsetKeys() const { return m_provenUnset; }
    int cacheSize() const { return m_cache.size(); }

signals:
    /// setValue() produced a NEW outbound write to relay, while ready().
    /// Never fired while !ready() (Step 8: dropped, not queued).
    void outboundWriteRequested(const QString& key, const QVariant& value);

    /// remove() produced a new outbound removal to relay, while ready().
    void outboundRemoveRequested(const QString& key);

    /// The daemon rejected one of this client's writes. See
    /// applyRejection().
    void valueRejected(const QString& key, const QVariant& restored);

    /// A snapshot (connect-time or reconnect) was applied. `keyCount` is
    /// data.size() from THIS call, not the cache total.
    void snapshotApplied(int keyCount);

    /// Whole-branch review, Important 2. The just-applied snapshot
    /// overwrote at least one key this client had edited while offline:
    /// `keys` is keysContradictedByLastSnapshot() as a sorted list, and
    /// the values those edits carried are gone. Fired at most once per
    /// applySnapshot(), and only when there is something to report.
    ///
    /// This exists because value() keeps returning the offline value
    /// until the snapshot lands, so the control the operator moved reads
    /// back as APPLIED the whole time -- the failure is invisible from
    /// the widget, and "Link to the Core lost" tells them about the link, not
    /// about their edit. MainWindow::connectToStation() hangs a toast on
    /// this; applySnapshot() also logs the key names at warning level, so
    /// the toast can stay a count and the log carries the detail.
    ///
    /// It reports only the CONTRADICTED subset, for the reason the class
    /// comment's "Offline behaviour" section gives: an offline edit whose
    /// key the snapshot did not cover leaves no record in either set, and
    /// this class cannot honestly claim to know whether it reached the
    /// station.
    void offlineEditsSuperseded(const QStringList& keys);

private:
    void logProxiedRead(const QString& key, const QString& outcome,
                        const QVariant& valueOrDefault) const;

    QMap<QString, QString> m_cache;

    /// See the class comment's "three-state read". Mutable: populated
    /// lazily from value(), a const method, exactly like AppSettings's
    /// own const accessors reading a mutable cache is not needed for
    /// (AppSettings has no such laziness) but this class does because the
    /// set is diagnostic-only and must not change what any const call
    /// returns.
    mutable QSet<QString> m_provenUnset;

    /// Fix round 1 (review, Important 2). See droppedWhileOffline()'s
    /// and keysContradictedByLastSnapshot()'s doc comments above and the
    /// class comment's "Offline behaviour" section.
    QSet<QString> m_droppedWhileOffline;
    QSet<QString> m_lastSnapshotContradictions;

    /// Keys the Core itself has sent a value for (snapshot, remote value,
    /// a rejection's restored value) and not since removed. setValue() and
    /// remove() never touch it, so hasNonEmptySnapshot() is not satisfied
    /// by this window's own writes.
    QSet<QString> m_coreKeys;

    bool m_ready = false;
    bool m_snapshotEverApplied = false;
    QString m_localOriginTag;
};

// ---------------------------------------------------------------------------
// The Setup gate, as production actually asks the question.
//
// Task 20 fix round 2, Important 2. setupDialogAllowed() above is a method
// on this class, so a caller has to already HAVE a SettingsProxy to ask it,
// and in local direct mode there is no proxy at all -- AppSettings holds a
// null remote backend. Every caller would therefore repeat the same
// "cross-cast the installed backend, allow unconditionally if it is not a
// SettingsProxy" preamble, and the first one to get it wrong would refuse
// Setup on a local radio. One function, one place to be right.
//
// Local direct mode: AppSettings::remoteBackend() is nullptr, the cast
// yields nullptr, this returns true, and nothing changes. The same is true
// of any OTHER ISettingsBackend implementation that might be installed:
// this gate is specifically about a station whose settings have not landed
// yet, and it has no opinion about backends it does not recognise.
bool setupDialogAllowedForCurrentBackend();

} // namespace NereusSDR
