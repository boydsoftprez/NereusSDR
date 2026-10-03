// =================================================================
// src/core/session/ObjectRegistry.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Remote-daemon R2 Task 9.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-05  J.J. Boyd / KG4VCF  Remote daemon R2 Task 9: slice
//                                    lifecycle object registry. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
// =================================================================

#include "core/session/ObjectRegistry.h"

#include "core/session/StateMirror.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcObjectRegistry, "nereus.mirror.objects")

namespace NereusSDR {

ObjectRegistry::ObjectRegistry(RadioModel* radioModel, StateMirror* mirror,
                               QObject* parent)
    : QObject(parent)
    , m_radioModel(radioModel)
    , m_mirror(mirror)
{
    if (radioModel == nullptr || mirror == nullptr) {
        qCWarning(lcObjectRegistry)
            << "constructed with a null RadioModel or StateMirror; no "
               "slice lifecycle will be tracked";
        return;
    }

    connect(radioModel, &RadioModel::sliceAdded, this,
            &ObjectRegistry::onSliceAdded, Qt::UniqueConnection);
    connect(radioModel, &RadioModel::sliceRemoved, this,
            &ObjectRegistry::onSliceRemoved, Qt::UniqueConnection);
}

ObjectRegistry::~ObjectRegistry()
{
    // Defensive symmetry with StateMirror::~StateMirror()'s unwatchAll():
    // if this registry is torn down while the mirror outlives it, nothing
    // it ever watched is left stranded in the mirror's watch list.
    // m_mirror is a QPointer, so if the MIRROR went first instead, this is
    // simply a no-op rather than a use-after-free.
    if (m_mirror) {
        for (auto it = m_live.constBegin(); it != m_live.constEnd(); ++it) {
            m_mirror->unwatch(keyForSlice(it.key()));
        }
    }
    m_live.clear();
}

QByteArray ObjectRegistry::keyForSlice(int sliceId)
{
    return QByteArray("slice:") + QByteArray::number(sliceId);
}

bool ObjectRegistry::isLive(int sliceId) const
{
    const auto it = m_live.constFind(sliceId);
    // A key can outlive its object's validity: SliceModel is parented to
    // RadioModel (RadioModel.cpp, new SliceModel(this) inside addSlice()),
    // so if RadioModel is destroyed while this registry is still alive,
    // Qt's parent-child cleanup deletes every surviving SliceModel
    // directly, without RadioModel::removeSlice() running and without a
    // sliceRemoved signal to prompt onSliceRemoved() to clear the entry.
    // StateMirror stays correct regardless (it has its own, independent
    // QObject::destroyed cleanup), but this accessor would otherwise keep
    // reporting a dead id as live.
    return it != m_live.constEnd() && !it.value().isNull();
}

QList<int> ObjectRegistry::liveSliceIds() const
{
    QList<int> ids;
    ids.reserve(m_live.size());
    for (auto it = m_live.constBegin(); it != m_live.constEnd(); ++it) {
        if (!it.value().isNull()) {
            ids.append(it.key());
        }
    }
    return ids;
}

void ObjectRegistry::onSliceAdded(int sliceId)
{
    if (!m_mirror) { return; }

    if (m_live.contains(sliceId)) {
        // Protocol error. RadioModel::addSlice() always scans for the
        // lowest id NOT currently in m_slices (sliceById(index) !=
        // nullptr), and this registry only ever clears an id from m_live
        // inside onSliceRemoved(), synchronously, before that call
        // returns -- so a second sliceAdded() for an id this registry
        // still considers live should be unreachable through normal
        // operation. Refused rather than silently clobbered:
        // StateMirror::watch() is idempotent for the SAME key and object
        // (Task 7), so overwriting m_live's entry here would make this
        // registry emit a SECOND object.create while StateMirror itself
        // stayed silent -- the two would disagree about what "slice:N"
        // means having happened.
        qCCritical(lcObjectRegistry).nospace()
            << "sliceAdded(" << sliceId << ") for an id this registry "
               "already considers live; refusing the second create "
               "(protocol error)";
        return;
    }

    SliceModel* slice = m_radioModel ? m_radioModel->sliceById(sliceId) : nullptr;
    if (slice == nullptr) {
        qCWarning(lcObjectRegistry)
            << "sliceAdded(" << sliceId << ") but RadioModel::sliceById() "
               "found nothing; nothing to watch";
        return;
    }

    createForSlice(slice);
}

void ObjectRegistry::backfillExistingSlices()
{
    if (!m_radioModel || !m_mirror) { return; }

    // Every slice RadioModel already holds, regardless of when each of
    // them was actually added relative to this registry's own
    // construction. Order is RadioModel::slices()'s own list order, not
    // necessarily ascending id (removeSlice() never renumbers survivors),
    // but nothing here depends on that ordering.
    for (SliceModel* slice : m_radioModel->slices()) {
        if (slice == nullptr) { continue; }
        if (m_live.contains(slice->sliceIndex())) {
            // Already watched: a real sliceAdded() already ran for it (this
            // registry existed before this particular slice did), or an
            // earlier call to this same method already caught it. Expected
            // and silent, unlike onSliceAdded()'s duplicate guard, which
            // treats the same condition as a bug: re-entering this method
            // is the whole point of it being idempotent.
            continue;
        }
        createForSlice(slice);
    }
}

void ObjectRegistry::createForSlice(SliceModel* slice)
{
    const int sliceId = slice->sliceIndex();
    const QByteArray key = keyForSlice(sliceId);
    if (!m_mirror->watch(key, slice)) {
        // StateMirror::watch() already logged the specific reason (not
        // mirrorable, or the key/object pairing conflicts with something
        // already watched). No object.create for a slice nothing is
        // actually being forwarded for -- that would promise a remote peer
        // updates it will never receive.
        return;
    }

    m_live.insert(sliceId, QPointer<SliceModel>(slice));

    const QByteArray className =
        MirrorSchema::shortClassName(MirrorSchema::forObject(slice).className());

    // The FULL settled snapshot, taken now. Whether `slice` just finished
    // addSlice()'s TX arbiter resync, frequency/mode seed, stream bind and
    // wireSliceSignals (the onSliceAdded() caller) or has been sitting
    // fully wired for however long (the backfillExistingSlices() caller),
    // this reads its CURRENT state, which is settled either way. Watching
    // begins only here, so no intermediate construction-time change was
    // ever observed, and this create carries one settled state rather than
    // a replay of history.
    const QList<MirrorUpdate> snapshot = m_mirror->snapshot(key);
    emit objectCreated(key, className, sliceId, snapshot);
}

void ObjectRegistry::onSliceRemoved(int sliceId)
{
    if (!m_mirror) { return; }

    const auto it = m_live.find(sliceId);
    if (it == m_live.end()) {
        // Never watched under this id: either the create that would have
        // produced it was itself rejected (RadioModel::addSlice() never
        // emits sliceAdded for a refused placement, so onSliceAdded() was
        // never called for it at all), or it has already been removed
        // once.
        return;
    }

    // The QPointer this registry has carried since the create. removeSlice()
    // calls deleteLater() BEFORE emitting sliceRemoved(), so the object it
    // names is still fully alive at this exact point in the call stack --
    // only the deferred-delete event, not yet processed, will end it.
    // Reading through it now, while that is still true, is safe; holding
    // onto it past this function is not attempted anywhere here.
    const QPointer<SliceModel> slice = it.value();
    const QByteArray key = keyForSlice(sliceId);
    const QByteArray className = slice
        ? MirrorSchema::shortClassName(MirrorSchema::forObject(slice).className())
        : QByteArray("SliceModel");

    // Detach FIRST, synchronously, inside this handler. This is the entire
    // mechanism the id-reuse hazard turns on: unwatch() disconnects every
    // signal StateMirror has to the dying object right now, so nothing it
    // still emits before its actual deletion can be forwarded, and it frees
    // the key immediately -- so a same-event-loop-turn addSlice() reusing
    // this id can watch a DIFFERENT object under the SAME key without
    // StateMirror::watch() refusing the rebind. Waiting on
    // QObject::destroyed instead would miss both: that signal does not
    // arrive until the event loop next spins, by which point a same-turn
    // re-add would already have collided with the still-watched corpse.
    m_mirror->unwatch(key);
    m_live.erase(it);

    emit objectDestroyed(key, className, sliceId);
}

} // namespace NereusSDR
