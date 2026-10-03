// =================================================================
// src/core/session/StateMirror.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Remote-daemon R2 Task 7.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-05  J.J. Boyd / KG4VCF  Remote daemon R2 Task 7: outbound
//                                    state forwarder. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
//   2026-08-05  J.J. Boyd / KG4VCF  Remote daemon R2 Task 8: inbound apply
//                                    (applyInbound, the per-model
//                                    applyMirroredValue hook dispatch, and
//                                    the m_applying forwarding guard). AI-
//                                    assisted transformation via Anthropic
//                                    Claude Code.
//   2026-08-05  J.J. Boyd / KG4VCF  Remote daemon R2 Task 10: connect-time
//                                    snapshot (attachSession), the
//                                    outbound coalescer (MirrorCoalescer),
//                                    and flushCoalescedDeltas. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
//   2026-08-05  J.J. Boyd / KG4VCF  Remote daemon R2 Task 11: m_applying
//                                    save/restore fix (ApplyingGuard) for
//                                    nested applyInbound() reentrancy. AI-
//                                    assisted transformation via Anthropic
//                                    Claude Code.
//   2026-09-24 - iPhone app Task 4b (R-IOS-01, R-R3-21): the reasons this
//                file sends an app are in operator words. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  iPhone app Task 72 (R-IOS-02): changes
//                                    go to every MirrorView but the
//                                    writer's (WriterScope replaces
//                                    ApplyingGuard); the single-session API
//                                    is served by a view of its own. AI-
//                                    assisted via Anthropic Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  Inbound sibling fix round 1:
//                                    MirrorCoalescer::remove drops one
//                                    pending property. AI-assisted via
//                                    Anthropic Claude Code.
// =================================================================

#include "core/session/StateMirror.h"

#include "core/session/MirrorPolicy.h"
#include "core/session/MirrorView.h"

#include <QLoggingCategory>
#include <QMetaMethod>
#include <QMetaObject>
#include <QMutexLocker>
#include <QSet>

Q_LOGGING_CATEGORY(lcStateMirror, "nereus.mirror")

namespace NereusSDR {

namespace {

// Task 11 dispatches four client command verbs (requestSliceSampleRate,
// addSlice, removeSlice, addSliceOnPan; SessionCommandDispatcher.h). Only
// sampleRateHz maps onto one of them -- the other three are slice
// LIFECYCLE commands (create/destroy/place), not replacements for writing
// an EXISTING slice's placement fields directly. The other six writable-
// but-Outbound SliceModel properties (chainIndex, ddcIndex, streamIndex,
// shiftOffsetHz, widebandExtensionRequested, psPaused -- MirrorPolicy.cpp)
// are all codec-/coordinator-owned placement state that none of the four
// verbs writes directly either: chainIndex/ddcIndex/streamIndex/
// shiftOffsetHz are codec OUTPUTS, not inputs (nothing takes a "set this
// slice's DDC" request; a client asks for a rate or a placement and the
// allocator decides the rest), and widebandExtensionRequested/psPaused are
// each driven by something other than an explicit client verb (pan zoom
// state and the PureSignal coordinator, respectively -- SliceModel.h's own
// property comments). All six stay honestly unassigned rather than
// guessing at a name.
// R-IOS-01: each hint is the reason in operator words, naming the control
// that sends the verb (requestSliceSampleRate for sampleRateHz), never the
// verb itself.
struct VerbHint {
    const char* className;
    const char* property;
    const char* reason;
};
constexpr VerbHint kVerbHints[] = {
    { "SliceModel", "sampleRateHz",
      "Choose the receiver's sample rate with its own control; it cannot be set directly." },
};

QString writableButOutboundReason(const QByteArray& shortClassName,
                                  const QByteArray& property)
{
    for (const VerbHint& hint : kVerbHints) {
        if (shortClassName == hint.className && property == hint.property) {
            return QString::fromLatin1(hint.reason);
        }
    }
    return QStringLiteral("The Core sets this itself; it cannot be changed from here.");
}

// Task 11 review finding (Task 8's own review round flagged this Minor at
// the time, because nothing could re-enter applyInbound() yet -- Task 11's
// command dispatch is what makes it reachable): m_applying was set true
// and unconditionally cleared back to false around each of
// applyInboundToProperty()'s two write paths below. That is only correct
// for the OUTERMOST call. A command handler (SessionCommandDispatcher) can
// run a RadioModel entry point whose synchronous side effects lead back
// into a SECOND, nested applyInbound() call before the first one's write()
// returns -- SessionCommandDispatcher::handleRequestSliceSampleRate is one
// concrete shape, but the general hazard is any RadioModel call reachable
// from a command that also touches a mirrored property through this class.
// The nested call's own unconditional `m_applying = false` on return would
// clear the OUTER call's flag while the outer write is still unwinding, so
// any further notify produced by that SAME outer write -- after the nested
// call returns but before the outer write() call itself does -- stops
// being suppressed and leaks out as a delta the peer never asked for: an
// echo of its own inbound write, or worse, of unrelated state the outer
// write's cascade happened to touch afterward.
//
// The fix is the standard save/restore RAII shape: remember what
// m_applying was BEFORE this call (false for an outermost call, true for a
// nested one), set it true for the duration, and restore the REMEMBERED
// value on the way out rather than hardcoding false. An outermost call
// still leaves it false afterward (prev == false), and a nested call now
// leaves it true afterward -- exactly what lets the outer call's own
// cleanup, moments later, do the actual final reset.
// iPhone app Task 72: the same save/restore now lives in
// StateMirror::WriterScope, which saves the writer beside the flag.

// Resolved once. Renaming the slot without updating this string would
// strand every watched object silently, so the lookup is loud on failure
// and StateMirror::watch refuses to pretend it worked.
const QMetaMethod& watcherSlot()
{
    static const QMetaMethod slot = [] {
        const int index =
            StateMirror::staticMetaObject.indexOfSlot("onWatchedPropertyChanged()");
        if (index < 0) {
            qCCritical(lcStateMirror)
                << "onWatchedPropertyChanged() slot not found; no model state "
                   "will be mirrored";
            return QMetaMethod();
        }
        return StateMirror::staticMetaObject.method(index);
    }();
    return slot;
}

} // namespace

// ── MirrorCoalescer ─────────────────────────────────────────────────────

void MirrorCoalescer::update(const QByteArray& objectKey, const MirrorUpdate& changed)
{
    QMutexLocker locker(&m_mutex);
    auto valuesIt = m_values.find(objectKey);
    if (valuesIt == m_values.end()) {
        // First property to go dirty for this object since the last
        // flush: record its arrival order, mirroring TciVfoCoalescer's
        // own "first time we see this key" comment.
        m_objectOrder.enqueue(objectKey);
        valuesIt = m_values.insert(objectKey, {});
        m_ordinalOrder.insert(objectKey, {});
    }
    QHash<quint16, MirrorUpdate>& perObject = valuesIt.value();
    if (!perObject.contains(changed.ordinal)) {
        m_ordinalOrder[objectKey].append(changed.ordinal);
    }
    // Latest-wins: REPLACE (or insert) the value for this ordinal. The
    // insertion-order slot recorded above is untouched by a replacement,
    // exactly like TciVfoCoalescer::update()'s own m_frames.insert().
    perObject.insert(changed.ordinal, changed);
}

QList<QPair<QByteArray, QList<MirrorUpdate>>> MirrorCoalescer::flush()
{
    QMutexLocker locker(&m_mutex);
    QList<QPair<QByteArray, QList<MirrorUpdate>>> out;
    out.reserve(m_objectOrder.size());
    while (!m_objectOrder.isEmpty()) {
        const QByteArray key = m_objectOrder.dequeue();
        const QList<quint16> ordinals = m_ordinalOrder.take(key);
        const QHash<quint16, MirrorUpdate> values = m_values.take(key);
        QList<MirrorUpdate> batch;
        batch.reserve(ordinals.size());
        for (quint16 ordinal : ordinals) {
            batch.append(values.value(ordinal));
        }
        out.append(qMakePair(key, batch));
    }
    return out;
}

void MirrorCoalescer::remove(const QByteArray& objectKey, quint16 ordinal)
{
    QMutexLocker locker(&m_mutex);
    auto valuesIt = m_values.find(objectKey);
    if (valuesIt == m_values.end() || valuesIt.value().remove(ordinal) == 0) {
        return;
    }
    m_ordinalOrder[objectKey].removeAll(ordinal);
    if (valuesIt.value().isEmpty()) {
        // Nothing left for this object: it leaves the arrival order too,
        // so flush() does not hand back an empty batch.
        m_values.erase(valuesIt);
        m_ordinalOrder.remove(objectKey);
        m_objectOrder.removeAll(objectKey);
    }
}

void MirrorCoalescer::clear()
{
    QMutexLocker locker(&m_mutex);
    m_objectOrder.clear();
    m_ordinalOrder.clear();
    m_values.clear();
}

int MirrorCoalescer::pendingObjectCount() const
{
    QMutexLocker locker(&m_mutex);
    return m_objectOrder.size();
}

int MirrorCoalescer::pendingPropertyCount() const
{
    QMutexLocker locker(&m_mutex);
    int total = 0;
    for (auto it = m_values.constBegin(); it != m_values.constEnd(); ++it) {
        total += it.value().size();
    }
    return total;
}

StateMirror::StateMirror(QObject* parent)
    : QObject(parent)
{
}

StateMirror::~StateMirror()
{
    // The mirror's own view first, while the mirror is still whole; a view
    // owned elsewhere outlives nothing here (it holds a QPointer back).
    m_legacyView.reset();
    unwatchAll();
}

// ── Views (iPhone app Task 72) ───────────────────────────────────────────

StateMirror::WriterScope::WriterScope(StateMirror& mirror, const MirrorView* writer)
    : m_mirror(mirror)
    , m_previousApplying(mirror.m_applying)
    , m_previousWriter(mirror.m_applyingWriter)
{
    m_mirror.m_applying = true;
    m_mirror.m_applyingWriter = writer;
}

StateMirror::WriterScope::~WriterScope()
{
    m_mirror.m_applying = m_previousApplying;
    m_mirror.m_applyingWriter = m_previousWriter;
}

void StateMirror::addView(MirrorView* view)
{
    if (view != nullptr && !m_views.contains(QPointer<MirrorView>(view))) {
        m_views.append(QPointer<MirrorView>(view));
    }
}

void StateMirror::removeView(MirrorView* view)
{
    m_views.removeIf([view](const QPointer<MirrorView>& held) {
        return held.isNull() || held.data() == view;
    });
}

MirrorView* StateMirror::legacyView()
{
    if (!m_legacyView) {
        m_legacyView = std::make_unique<MirrorView>(
            this, [this](const SessionMessage& message) { emit sessionMessageReady(message); });
    }
    return m_legacyView.get();
}

QList<MirrorUpdate> StateMirror::currentValues(const QByteArray& objectKey,
                                               const QList<quint16>& ordinals) const
{
    const int index = indexOfKey(objectKey);
    if (index < 0) {
        return {};
    }
    const Watch& watch = m_watches.at(index);
    if (watch.object == nullptr || watch.schema == nullptr) {
        return {};
    }
    QList<MirrorUpdate> values;
    values.reserve(ordinals.size());
    for (quint16 ordinal : ordinals) {
        const MirrorProperty* prop = watch.schema->byOrdinal(ordinal);
        if (prop == nullptr) {
            continue;
        }
        const QVariant value = watch.schema->read(*prop, watch.object);
        if (!value.isValid()) {
            continue;
        }
        values.append(MirrorUpdate{ prop->ordinal, prop->name, prop->kind, value });
    }
    return values;
}

// ── Lookup helpers ────────────────────────────────────────────────────────

int StateMirror::indexOfKey(const QByteArray& objectKey) const
{
    for (int i = 0; i < m_watches.size(); ++i) {
        if (m_watches.at(i).key == objectKey) {
            return i;
        }
    }
    return -1;
}

int StateMirror::indexOfObject(const QObject* object) const
{
    for (int i = 0; i < m_watches.size(); ++i) {
        if (m_watches.at(i).object == object) {
            return i;
        }
    }
    return -1;
}

// ── Watch lifecycle ───────────────────────────────────────────────────────

bool StateMirror::watch(const QByteArray& objectKey, QObject* object)
{
    if (objectKey.isEmpty() || object == nullptr) {
        return false;
    }

    const MirrorSchema& schema = MirrorSchema::forObject(object);
    if (schema.size() == 0) {
        qCWarning(lcStateMirror)
            << "refusing to watch" << objectKey << "of class"
            << object->metaObject()->className()
            << "-- not part of the mirrored surface";
        return false;
    }

    // The binding must be one-to-one in BOTH directions.
    const int existing = indexOfKey(objectKey);
    if (existing >= 0 && m_watches.at(existing).object != object) {
        // Two objects under one wire identity would give the remote GUI an
        // interleaving of both, with no way to tell which it was looking at.
        qCWarning(lcStateMirror)
            << "refusing to rebind key" << objectKey << "to a different object";
        return false;
    }

    const int sameObject = indexOfObject(object);
    if (sameObject >= 0 && m_watches.at(sameObject).key != objectKey) {
        // One object under two keys is worse than useless. Every lookup
        // here is first-match, so the second record would forward nothing
        // while isWatching() reported true, unwatch() of the first key
        // would disconnect the object wholesale and mute the second
        // permanently, and destruction would remove only one record and
        // leave the other holding a freed pointer for snapshot() to read.
        qCWarning(lcStateMirror)
            << "refusing to watch an object already bound to key"
            << m_watches.at(sameObject).key << "under a second key" << objectKey;
        return false;
    }

    const QMetaMethod& slot = watcherSlot();
    if (!slot.isValid()) {
        return false;
    }

    // Connect the declared NOTIFY of every non-CONSTANT property, once per
    // DISTINCT signal. UniqueConnection makes the re-watch case (and the
    // shared-notifier case, already collapsed by the schema) a no-op rather
    // than a source of duplicate frames.
    for (int notifyMethodIndex : schema.notifyMethodIndices()) {
        const QMetaMethod signal = schema.metaObject()->method(notifyMethodIndex);
        if (!signal.isValid()) {
            continue;
        }
        connect(object, signal, this, slot, Qt::UniqueConnection);
    }

    if (existing >= 0) {
        return true;
    }

    // A slice removed from RadioModel is deleted underneath us; without
    // this the next notifier lookup would walk a dangling pointer.
    connect(object, &QObject::destroyed,
            this, &StateMirror::onWatchedObjectDestroyed,
            Qt::UniqueConnection);

    m_watches.append(Watch{ objectKey, object, &schema });
    return true;
}

void StateMirror::detach(const Watch& watch)
{
    if (watch.object != nullptr) {
        // Drops both the notify connections and the destroyed() hook, which
        // are the only connections this class ever makes from a watched
        // object to itself.
        disconnect(watch.object, nullptr, this, nullptr);
    }
}

void StateMirror::unwatch(const QByteArray& objectKey)
{
    const int index = indexOfKey(objectKey);
    if (index < 0) {
        return;
    }
    detach(m_watches.at(index));
    m_watches.removeAt(index);
}

void StateMirror::unwatchAll()
{
    for (const Watch& watch : m_watches) {
        detach(watch);
    }
    m_watches.clear();
}

void StateMirror::onWatchedObjectDestroyed(QObject* object)
{
    // Removes EVERY record naming this pointer, not just the first.
    // watch() refuses to create a second one, so this should only ever
    // remove a single record; it removes all of them anyway, because the
    // cost of being wrong is a record holding a freed pointer that
    // snapshot() would go on to read.
    //
    // No detach() here: the object is already in ~QObject, which has
    // dropped its connections itself.
    m_watches.removeIf([object](const Watch& watch) {
        return watch.object == object;
    });
}

bool StateMirror::isWatching(const QByteArray& objectKey) const
{
    return indexOfKey(objectKey) >= 0;
}

QList<QByteArray> StateMirror::watchedKeys() const
{
    QList<QByteArray> keys;
    keys.reserve(m_watches.size());
    for (const Watch& watch : m_watches) {
        keys.append(watch.key);
    }
    return keys;
}

QObject* StateMirror::watchedObject(const QByteArray& objectKey) const
{
    const int index = indexOfKey(objectKey);
    return index < 0 ? nullptr : m_watches.at(index).object;
}

QByteArray StateMirror::keyFor(const QObject* object) const
{
    const int index = indexOfObject(object);
    return index < 0 ? QByteArray() : m_watches.at(index).key;
}

// ── The forwarder ─────────────────────────────────────────────────────────

void StateMirror::onWatchedPropertyChanged()
{
    // iPhone app Task 72 (ruling 5.7): an inbound apply in flight no longer
    // ends this slot here. While applyInbound() (or a WriterScope) is on
    // the call stack, writing the property this notify is FOR directly, or
    // -- same-thread, synchronous, still nested inside that same write --
    // causing a side effect on a DIFFERENT watched object (RadioModel::
    // addSlice()'s co-hosted-slice NB-mode/NB-tuning peer mirrors:
    // peer->setNbMode() fires nbModeChanged on the PEER object), the change
    // is withheld from the WRITER's view alone, below, and still reaches
    // every other device's view. Whichever object fired, one check covers
    // both cases, as the single flag did.
    const bool applying = m_applying;
    const MirrorView* writer = m_applyingWriter;
    if (applying && m_views.isEmpty()) {
        // Nothing but propertiesChanged() could hear it, and that stays
        // silent while a write is applied.
        return;
    }

    QObject* source = sender();
    if (source == nullptr) {
        return;
    }

    // The meta-method index of the signal that got us here, which is
    // directly comparable with QMetaProperty::notifySignal().methodIndex()
    // as recorded in the schema.
    const int notifyMethodIndex = senderSignalIndex();
    if (notifyMethodIndex < 0) {
        return;
    }

    const int index = indexOfObject(source);
    if (index < 0) {
        return;
    }

    // Copied out of m_watches before the emit below: a receiver may watch
    // or unwatch in response, which would move the list out from under a
    // pointer into it.
    const QByteArray key = m_watches.at(index).key;
    const MirrorSchema* schema = m_watches.at(index).schema;
    if (schema == nullptr) {
        return;
    }

    const QList<quint16>& ordinals = schema->ordinalsForNotifySignal(notifyMethodIndex);
    if (ordinals.isEmpty()) {
        return;
    }

    QList<MirrorUpdate> updates;
    updates.reserve(ordinals.size());
    for (quint16 ordinal : ordinals) {
        const MirrorProperty* prop = schema->byOrdinal(ordinal);
        if (prop == nullptr) {
            continue;
        }
        // RE-READ. Never the signal's arguments: an arity-0 notifier has
        // none, and setDspMode moves filterLow/filterHigh behind a
        // separate signal.
        const QVariant value = schema->read(*prop, source);
        if (!value.isValid()) {
            continue;
        }
        updates.append(MirrorUpdate{ prop->ordinal, prop->name, prop->kind, value });
    }

    if (updates.isEmpty()) {
        return;
    }

    // Task 10, per view since Task 72: every change feeds each attached
    // view's outbound coalescer -- the WHOLE mechanism behind a view's
    // flush() reporting the same value a live re-read would (see
    // MirrorCoalescer's class comment), and what stops a change triggered
    // synchronously from inside a view's own attach burst from reaching
    // the wire ahead of its snapshot-complete marker: a Delta is only ever
    // sent from a flush, never from here. A view that has not attached
    // yet collects nothing (MirrorView::noteChange), which is what "the
    // coalescer must not change any behaviour when no session is
    // attached" rests on. The writer's own view is skipped (ruling 5.7).
    // Copied: a view may leave (its session ended) while this runs.
    const QList<QPointer<MirrorView>> views = m_views;
    for (const QPointer<MirrorView>& view : views) {
        if (view.isNull() || (applying && view.data() == writer)) {
            continue;
        }
        view->noteChange(key, updates);
    }

    if (!applying) {
        emit propertiesChanged(key, updates);
    }
}

// ── Snapshot ──────────────────────────────────────────────────────────────

QList<MirrorUpdate> StateMirror::snapshot(const QByteArray& objectKey) const
{
    const int index = indexOfKey(objectKey);
    if (index < 0) {
        return {};
    }
    const Watch& watch = m_watches.at(index);
    if (watch.object == nullptr || watch.schema == nullptr) {
        return {};
    }

    QList<MirrorUpdate> updates;
    updates.reserve(watch.schema->size());
    // Walks the WHOLE schema, not the notify map. That is the only way a
    // CONSTANT property such as sliceIndex -- the mirror's object identity
    // -- ever reaches the wire.
    for (const MirrorProperty& prop : watch.schema->properties()) {
        const QVariant value = watch.schema->read(prop, watch.object);
        if (!value.isValid()) {
            continue;
        }
        updates.append(MirrorUpdate{ prop.ordinal, prop.name, prop.kind, value });
    }
    return updates;
}

// O(n^2) in the watched-object count: each snapshot() re-resolves its key
// through the linear indexOfKey. Irrelevant at the five-ish slices this
// ever holds, and left simple on purpose, but Task 10 builds the
// connect-time burst on this call, so if the watched set ever grows past
// a handful, pass the index down instead of the key.
QList<QPair<QByteArray, QList<MirrorUpdate>>> StateMirror::snapshotAll() const
{
    QList<QPair<QByteArray, QList<MirrorUpdate>>> all;
    all.reserve(m_watches.size());
    for (const Watch& watch : m_watches) {
        all.append(qMakePair(watch.key, snapshot(watch.key)));
    }
    return all;
}

// ── Connect-time snapshot (Task 10) ─────────────────────────────────────

void StateMirror::attachSession()
{
    // iPhone app Task 72: Task 10's burst is MirrorView::attach(), here on
    // the mirror's own view, whose messages leave through
    // sessionMessageReady(). Its order and its coalescer rules are
    // unchanged: a schema per distinct class, an object.create per watched
    // object, the marker, then whatever the burst itself caused to change.
    legacyView()->attach();
}

bool StateMirror::hasAttachedSession() const
{
    return m_legacyView && m_legacyView->isAttached();
}

int StateMirror::flushCoalescedDeltas()
{
    // Re-resolved against the live watch list inside the view's flush (see
    // MirrorView::flush and currentValues()): a key no longer watched is
    // dropped, and a key still watched is read fresh.
    return m_legacyView ? m_legacyView->flush() : 0;
}

// ── Inbound apply ────────────────────────────────────────────────────────

MirrorApplyResult StateMirror::applyInbound(const QByteArray& objectKey,
                                            const QByteArray& propertyName,
                                            const QVariant& wireValue)
{
    MirrorApplyResult result;
    result.objectKey = objectKey;
    result.property = propertyName;

    const int index = indexOfKey(objectKey);
    if (index < 0 || m_watches.at(index).object == nullptr
        || m_watches.at(index).schema == nullptr) {
        result.reason = QStringLiteral("The Core does not have this setting.");
        return result;
    }
    const Watch& watch = m_watches.at(index);

    const MirrorProperty* prop = watch.schema->byName(propertyName);
    if (prop == nullptr) {
        result.reason = QStringLiteral("The Core does not have this setting.");
        return result;
    }
    return applyInboundToProperty(watch, *prop, wireValue, m_legacyView.get());
}

MirrorApplyResult StateMirror::applyInbound(const QByteArray& objectKey,
                                            const QByteArray& propertyName,
                                            const QVariant& wireValue,
                                            const MirrorView* writer)
{
    MirrorApplyResult result;
    result.objectKey = objectKey;
    result.property = propertyName;

    const int index = indexOfKey(objectKey);
    if (index < 0 || m_watches.at(index).object == nullptr
        || m_watches.at(index).schema == nullptr) {
        result.reason = QStringLiteral("The Core does not have this setting.");
        return result;
    }
    const Watch& watch = m_watches.at(index);

    const MirrorProperty* prop = watch.schema->byName(propertyName);
    if (prop == nullptr) {
        result.reason = QStringLiteral("The Core does not have this setting.");
        return result;
    }
    return applyInboundToProperty(watch, *prop, wireValue, writer);
}

MirrorApplyResult StateMirror::applyInbound(const QByteArray& objectKey,
                                            quint16 ordinal,
                                            const QVariant& wireValue)
{
    MirrorApplyResult result;
    result.objectKey = objectKey;

    const int index = indexOfKey(objectKey);
    if (index < 0 || m_watches.at(index).object == nullptr
        || m_watches.at(index).schema == nullptr) {
        result.reason = QStringLiteral("The Core does not have this setting.");
        return result;
    }
    const Watch& watch = m_watches.at(index);

    const MirrorProperty* prop = watch.schema->byOrdinal(ordinal);
    if (prop == nullptr) {
        result.reason = QStringLiteral("The Core does not have this setting.");
        return result;
    }
    return applyInboundToProperty(watch, *prop, wireValue, m_legacyView.get());
}

MirrorApplyResult StateMirror::applyInboundToProperty(const Watch& watch,
                                                       const MirrorProperty& prop,
                                                       const QVariant& wireValue,
                                                       const MirrorView* writer)
{
    MirrorApplyResult result;
    result.objectKey = watch.key;
    result.property = prop.name;

    // CONSTANT properties (sliceIndex, the mirror's object identity) have
    // no NOTIFY, travel only in the connect-time snapshot, and must never
    // be treated as a live write -- including through the hook below.
    if (prop.isConstant) {
        result.reason = QStringLiteral("The Core sets this itself; it cannot be changed from here.");
        return result;
    }

    const QByteArray className = watch.schema->className();

    if (prop.isWritable) {
        // The standard path: a real Q_PROPERTY WRITE, gated by MirrorPolicy.
        // Most of the mirrored surface is Bidirectional and passes; the
        // SliceModel properties that carry WRITE but are daemon-
        // authoritative (MirrorPolicy.cpp) are refused here, naming the
        // control to use instead where the R2 plan has assigned a command.
        //
        // Whole-branch review, Minor 1: this used to quote "115 of 145
        // properties" and "the seven SliceModel properties". Both had
        // rotted (146 and nine as counted mechanically), and both
        // contradicted MirrorPolicy.cpp, which already calls the plan's
        // seven "a floor, not a cap" and names the two later additions.
        // Removed rather than corrected, matching the earlier round on
        // this branch that deleted a literal count for the same reason: a
        // number that goes stale on every added property is the same
        // species of false claim, just deferred. MirrorPolicy.cpp's table
        // is the one place that can be counted, and it is one file away.
        if (!MirrorPolicy::inboundAllowed(className, prop.name)) {
            result.reason = writableButOutboundReason(
                MirrorSchema::shortClassName(className), prop.name);
            return result;
        }
        bool ok = false;
        {
            // See the save/restore comment (top of this file; WriterScope
            // since Task 72) for why this must restore rather than hardcode
            // m_applying back
            // to false: watch.schema->write() can synchronously re-enter
            // applyInbound() through a command handler's RadioModel call,
            // and the guard has to survive that nesting intact.
            WriterScope guard(*this, writer);
            ok = watch.schema->write(prop, watch.object, wireValue);
        }
        if (!ok) {
            result.reason = QStringLiteral(
                "value could not be decoded for this property's type");
            return result;
        }
        result.accepted = true;
        return result;
    }

    // No WRITE at all. MirrorPolicy always classifies this Outbound --
    // there is nothing standard for it to write through -- but that is not
    // the final word: the target model gets one more chance to translate
    // the intent through its own applyMirroredValue(name, value), called by
    // name via QMetaObject::invokeMethod so this file never needs to
    // #include a model header. Decoded through the same codec a normal
    // WRITE would use, so the hook receives a properly narrowed, natively
    // typed value rather than a raw wire one.
    const QVariant native = MirrorSchema::decode(prop, wireValue);
    if (!native.isValid()) {
        result.reason = QStringLiteral("The Core could not use this value for this setting.");
        return result;
    }

    QString hookReason;
    bool invoked = false;
    {
        // Same reentrancy hazard as the writable path above: the hook may
        // itself call a RadioModel entry point that leads back into
        // applyInbound() before invokeMethod() returns.
        WriterScope guard(*this, writer);
        invoked = QMetaObject::invokeMethod(
            watch.object, "applyMirroredValue", Qt::DirectConnection,
            Q_RETURN_ARG(QString, hookReason),
            Q_ARG(QByteArray, prop.name),
            Q_ARG(QVariant, native));
    }

    if (!invoked) {
        result.reason = QStringLiteral("The Core sets this itself; it cannot be changed from here.");
        return result;
    }
    if (!hookReason.isEmpty()) {
        result.reason = hookReason;
        return result;
    }
    result.accepted = true;
    return result;
}

} // namespace NereusSDR
