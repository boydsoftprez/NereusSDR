// =================================================================
// src/core/settings/SettingsProxy.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Remote-daemon R2 Task 15.
//
// See SettingsProxy.h for the full design.
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
//                                    Minor 8 (remove() no longer marks
//                                    proven-unset before any snapshot has
//                                    landed). AI-assisted transformation
//                                    via Anthropic Claude Code.
//   2026-08-06  J.J. Boyd / KG4VCF  Fix round 2 (review): no code change
//                                    in this file; see the .h for the
//                                    documentation-only fix (Task 19 is
//                                    told only the contradicted subset).
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
//   2026-08-08  J.J. Boyd / KG4VCF  Task 20 fix round 2 (review,
//                                    Important 2):
//                                    setupDialogAllowedForCurrentBackend(),
//                                    so the Setup gate has a production
//                                    caller. AI-assisted transformation via
//                                    Anthropic Claude Code.
//   2026-08-09  J.J. Boyd / KG4VCF  Whole-branch review, Important 4:
//                                    applyRemoteRemoval(). AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
//   2026-08-09  J.J. Boyd / KG4VCF  Whole-branch review, Important 2:
//                                    applySnapshot() logs and announces
//                                    superseded offline edits. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
//   2026-08-09  J.J. Boyd / KG4VCF  Whole-branch review, Minors 5 and 6:
//                                    hasNonEmptySnapshot() consults
//                                    m_snapshotEverApplied as its own doc
//                                    always claimed, and applySnapshot()
//                                    filters through handlesKey() so
//                                    handledKeys() subset-of handlesKey()
//                                    is structural. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
//   2026-09-28  J.J. Boyd / KG4VCF  hasNonEmptySnapshot() counts only
//                                    keys the Core sent (m_coreKeys), not
//                                    this window's own writes. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
// =================================================================

#include "core/settings/SettingsProxy.h"

#include "core/AppSettings.h"
#include "core/settings/SettingsScope.h"

#include <QLoggingCategory>

namespace NereusSDR {

namespace {
Q_LOGGING_CATEGORY(lcSettingsProxy, "nereus.settingsproxy")
} // namespace

SettingsProxy::SettingsProxy(QObject* parent)
    : QObject(parent)
{
}

bool SettingsProxy::handlesKey(const QString& key) const
{
    // R2's own connect-time bookkeeping marker (AppSettings.h's doc
    // comment on kDaemonProfileSeededKey) is not a "setting" in
    // classifySettingsKey()'s sense at all -- no rule there names it, on
    // purpose, since it is protocol state rather than a station fact.
    // Special-cased here so it still reaches this cache (via
    // applySnapshot()/SettingsProxyServer::buildSnapshot(), which
    // includes it unconditionally) rather than falling through to
    // AppSettings's own local m_settings map, where a freshly-launched
    // remote-mode GUI would never find it. See SettingsProxy.h's
    // "Setup-dialog gate" paragraph.
    if (key == QLatin1String(AppSettings::kDaemonProfileSeededKey)) {
        return true;
    }
    return classifySettingsKey(key) == SettingsScope::Station;
}

QVariant SettingsProxy::value(const QString& key, const QVariant& defaultValue) const
{
    auto it = m_cache.constFind(key);
    if (it != m_cache.constEnd()) {
        logProxiedRead(key, QStringLiteral("CacheHit"), QVariant(it.value()));
        return QVariant(it.value());
    }
    if (m_snapshotEverApplied) {
        // See the class comment's "three-state read": absence after at
        // least one snapshot has landed reads as "the daemon's own
        // AppSettings::value() would also return the caller's default
        // for this key". Does not change the return value -- only what
        // gets logged/recorded -- but m_provenUnset is real state a test
        // (and Step 9's log) can inspect.
        m_provenUnset.insert(key);
        logProxiedRead(key, QStringLiteral("ProvenUnset"), defaultValue);
    } else {
        logProxiedRead(key, QStringLiteral("NoSnapshotYet"), defaultValue);
    }
    return defaultValue;
}

void SettingsProxy::setValue(const QString& key, const QVariant& val)
{
    m_cache.insert(key, val.toString());
    m_provenUnset.remove(key);
    if (m_ready) {
        emit outboundWriteRequested(key, val);
    } else {
        // Fix round 1 (review, Important 2). See the class comment's
        // "Offline behaviour" paragraph: the cache above is still
        // updated (the UI stays consistent) but nothing is emitted -- a
        // dropped write, not a queued one -- and THIS is what records
        // that the drop happened, so a later applySnapshot() can tell
        // Task 19 which keys it might be overwriting.
        m_droppedWhileOffline.insert(key);
    }
}

bool SettingsProxy::contains(const QString& key) const
{
    return m_cache.contains(key);
}

void SettingsProxy::remove(const QString& key)
{
    m_cache.remove(key);
    // Fix round 1 (review, Minor 8): only mark proven-unset once at
    // least one snapshot has landed, matching value()'s own gating and
    // the class comment's "three-state read" definition of PROVEN
    // UNSET -- a remove() called before this client has ever heard from
    // the daemon at all has nothing to base "proven" on; it is exactly
    // the NO-SNAPSHOT-YET state, not a confirmed absence.
    if (m_snapshotEverApplied) {
        m_provenUnset.insert(key);
    }
    if (m_ready) {
        emit outboundRemoveRequested(key);
    } else {
        // See setValue()'s matching branch and the class comment's
        // "Offline behaviour" paragraph.
        m_droppedWhileOffline.insert(key);
    }
}

QStringList SettingsProxy::handledKeys() const
{
    return m_cache.keys();
}

void SettingsProxy::setLocalOriginTag(const QString& tag)
{
    m_localOriginTag = tag;
}

void SettingsProxy::setReady(bool ready)
{
    m_ready = ready;
}

void SettingsProxy::applySnapshot(const QMap<QString, QString>& data)
{
    // Merge, never replace -- see the class comment's "Why the cache is
    // NOT AppSettings's own m_settings map" and the R2 Task 15 brief's
    // own Step 3. A key this snapshot reports real content for
    // supersedes anything m_provenUnset previously recorded for it (the
    // daemon evidently has it now, whatever this cache believed before).
    //
    // Fix round 1 (review, Important 2): before merging, note which
    // members of m_droppedWhileOffline this snapshot ALSO covers -- the
    // daemon has its own authoritative value for those keys now, so
    // whatever this client tried to set for them while offline did not
    // reach it. See the class comment's "Offline behaviour" section for
    // why m_droppedWhileOffline is cleared in full afterward regardless
    // of which members were contradicted, not just the contradicted
    // subset.
    m_lastSnapshotContradictions.clear();
    int notClaimed = 0;
    for (auto it = data.constBegin(); it != data.constEnd(); ++it) {
        // Whole-branch review, Minor 6. ISettingsBackend's contract is
        // handledKeys() subset-of handlesKey(), and every other writer
        // into m_cache honours it by construction: setValue()/remove()
        // are only reached through AppSettings, which asks handlesKey()
        // first. This method was the one un-gated writer, merging
        // whatever the station sent, so the invariant held only while
        // both binaries shared a classifier build. Not reachable at one
        // version, and this filter is what makes it structural instead of
        // conventional.
        //
        // No read changes: a key this client does not claim was never
        // routed here in the first place, so caching it only ever made
        // handledKeys() report a key handlesKey() denies. Dropping it is
        // also the safer half of the skew: the value stays readable from
        // this machine's own settings file, which is where a key this
        // build calls OperatorLocal belongs.
        if (!handlesKey(it.key())) {
            ++notClaimed;
            continue;
        }
        if (m_droppedWhileOffline.contains(it.key())) {
            m_lastSnapshotContradictions.insert(it.key());
        }
        m_cache.insert(it.key(), it.value());
        m_provenUnset.remove(it.key());
        m_coreKeys.insert(it.key());
    }
    if (notClaimed > 0) {
        qCWarning(lcSettingsProxy)
            << "Station snapshot carried" << notClaimed
            << "key(s) this build does not classify as station-scoped; they were "
               "not cached. This is settings-classifier skew between the two "
               "binaries, not a transport fault.";
    }
    m_droppedWhileOffline.clear();
    m_snapshotEverApplied = true;
    emit snapshotApplied(data.size());

    // Whole-branch review, Important 2. Until this existed the
    // contradiction set had no consumer anywhere in production, so the
    // operator was told the LINK dropped and never that a specific EDIT of
    // theirs had been overwritten -- and value() had been serving the
    // offline value back the whole time, so the control read as applied.
    // Sorted so the log line and any UI hung on the signal are stable and
    // diffable rather than QSet-hash-ordered.
    if (!m_lastSnapshotContradictions.isEmpty()) {
        QStringList keys(m_lastSnapshotContradictions.cbegin(),
                         m_lastSnapshotContradictions.cend());
        keys.sort();
        // The DETAIL lives here, at warning level, so a consumer can stay
        // proportionate (a count, and "see the log") instead of pasting a
        // wall of key names into a toast.
        qCWarning(lcSettingsProxy).noquote()
            << QStringLiteral("%1 setting(s) changed while the station link was "
                              "down did not reach the station and have been "
                              "replaced by its own values: %2")
                   .arg(keys.size())
                   .arg(keys.join(QStringLiteral(", ")));
        emit offlineEditsSuperseded(keys);
    }
}

bool SettingsProxy::hasNonEmptySnapshot() const
{
    // Whole-branch review, Minor 5. This method's own doc comment has
    // always said "once at least one snapshot has been applied AND it
    // carried at least one key besides the seed marker", and the body
    // inspected only m_cache -- so this client's own optimistic writes
    // (which land in m_cache whether or not a station has ever been heard
    // from) satisfied it too. Inert today, because the only consumer,
    // setupDialogAllowed() below, also requires m_ready, which nothing but
    // a completed handshake sets and which necessarily follows a snapshot.
    // Made structural rather than left resting on that ordering.
    //
    // The same holds after a snapshot: counting m_cache let this window's
    // own write, made over an empty or marker-only snapshot, read as station
    // content. Only keys the Core itself sent (m_coreKeys) count.
    if (!m_snapshotEverApplied) {
        return false;
    }
    // The seed marker alone does not count as "real" station content --
    // see the class comment and setupDialogAllowed()'s own doc comment.
    // Without this carve-out, EVERY snapshot (which always carries the
    // marker once the daemon has seeded it) would trivially satisfy
    // "non-empty" and the OR-fallback below would never do anything.
    for (const QString& key : m_coreKeys) {
        if (key != QLatin1String(AppSettings::kDaemonProfileSeededKey)) {
            return true;
        }
    }
    return false;
}

bool SettingsProxy::setupDialogAllowed() const
{
    if (!m_ready) {
        return false;
    }
    if (hasNonEmptySnapshot()) {
        return true;
    }
    return m_cache.contains(QLatin1String(AppSettings::kDaemonProfileSeededKey));
}

void SettingsProxy::applyRemoteValue(const QString& key, const QVariant& value, const QString& originTag)
{
    Q_UNUSED(originTag); // see the class comment's origin-tag paragraph: applied unconditionally here
    m_cache.insert(key, value.toString());
    m_provenUnset.remove(key);
    m_coreKeys.insert(key);
}

void SettingsProxy::applyRemoteRemoval(const QString& key)
{
    // Deliberately NOT this class's own remove(): that is the OUTBOUND
    // path and would emit outboundRemoveRequested() (or record a dropped
    // offline edit), sending the station a removal it just told us about.
    // Proven-unset unconditionally, without value()'s and remove()'s
    // m_snapshotEverApplied gate: the daemon has directly asserted this
    // key's absence, which is a stronger fact than the inference that
    // gate protects, and is the same reasoning applyRejection() below
    // uses for an invalid restored value.
    m_cache.remove(key);
    m_provenUnset.insert(key);
    m_coreKeys.remove(key);
}

void SettingsProxy::applyRejection(const QString& key, const QVariant& restoredValue)
{
    if (restoredValue.isValid()) {
        m_cache.insert(key, restoredValue.toString());
        m_provenUnset.remove(key);
        m_coreKeys.insert(key);
    } else {
        // The daemon has nothing for this key either -- revert to
        // proven-unset, not to an empty string (see the class comment).
        m_cache.remove(key);
        m_provenUnset.insert(key);
        m_coreKeys.remove(key);
    }
    emit valueRejected(key, restoredValue);
}

void SettingsProxy::logProxiedRead(const QString& key, const QString& outcome,
                                   const QVariant& valueOrDefault) const
{
    // Step 9: unconditional, one line per proxied read, literal
    // "proxied-read" first token so `grep proxied-read <log>` produces
    // Task 20's scannable list. Built as a single QString and logged via
    // .noquote() rather than streamed token-by-token through QDebug's
    // default operator<<, which would wrap each QString/QVariant in
    // its own quotes and print a QVariant via its verbose debug
    // representation (QVariant(QString, "...")), not the bare value --
    // neither is what a scannable, greppable line needs. See the class
    // comment for the exact shape and
    // tests/tst_settings_proxy.cpp's proxiedReadLogHasScannableShape for
    // the pinned format.
    const QString label = (outcome == QStringLiteral("CacheHit"))
        ? QStringLiteral("value")
        : QStringLiteral("default");
    qCDebug(lcSettingsProxy).noquote()
        << QStringLiteral("proxied-read %1 outcome=%2 %3=\"%4\"")
               .arg(key, outcome, label, valueOrDefault.toString());
}

bool setupDialogAllowedForCurrentBackend()
{
    // dynamic_cast, not qobject_cast: AppSettings holds the backend as an
    // ISettingsBackend*, and that interface deliberately is NOT a QObject
    // (see ISettingsBackend.h), so there is no meta-object for qobject_cast
    // to walk. The interface has a virtual destructor, which is what makes
    // this cross-cast well-formed. Same shape MainWindow::connectToStation()
    // uses to find the proxy at dial time.
    const auto* proxy = dynamic_cast<const SettingsProxy*>(
        AppSettings::instance().remoteBackend());
    if (proxy == nullptr) {
        return true;  // local direct mode, or a backend this gate has no opinion about
    }
    return proxy->setupDialogAllowed();
}

} // namespace NereusSDR
