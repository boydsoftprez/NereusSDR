// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/SliceAccessSet.cpp  (NereusSDR)
// =================================================================
//
// Slice control and shared listening plan Task 4: who controls and who
// listens to each slice, one mirrored object per slice. See
// SliceAccessSet.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-28: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), slice control and shared listening plan Task 4,
//               with AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include "core/session/SliceAccessSet.h"

#include "core/SliceOwnership.h"
#include "core/session/StateMirror.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

namespace NereusSDR {

// ── SliceAccess ──────────────────────────────────────────────────────────

SliceAccess::SliceAccess(int sliceId, qint64 incarnation, QObject* parent)
    : QObject(parent)
    , m_sliceId(sliceId)
    , m_incarnation(incarnation)
{
}

void SliceAccess::setFields(const Fields& fields)
{
    const Fields before = m_fields;
    m_fields = fields;
    if (before.controllerDeviceId != fields.controllerDeviceId) {
        emit controllerDeviceIdChanged();
    }
    if (before.controlRevision != fields.controlRevision) {
        emit controlRevisionChanged();
    }
    if (before.listenerDeviceIds != fields.listenerDeviceIds) {
        emit listenerDeviceIdsChanged();
    }
    if (before.activeRxDeviceIds != fields.activeRxDeviceIds) {
        emit activeRxDeviceIdsChanged();
    }
    if (before.txSelected != fields.txSelected) {
        emit txSelectedChanged();
    }
    if (before.onAir != fields.onAir) {
        emit onAirChanged();
    }
}

// ── SliceAccessSet ───────────────────────────────────────────────────────

SliceAccessSet::SliceAccessSet(RadioModel* radio, StateMirror* mirror, Resolver resolver,
                               QObject* parent)
    : QObject(parent)
    , m_radio(radio)
    , m_mirror(mirror)
    , m_resolver(std::move(resolver))
{
    if (radio == nullptr || mirror == nullptr) {
        return;
    }
    // After ObjectRegistry's and the markers' own connections (the Core
    // makes this set after both), so a slice's object.create and its
    // marker's go out before its access object's, and the same for their
    // object.destroy.
    connect(radio, &RadioModel::sliceAdded, this, [this](int sliceId) { createFor(sliceId); });
    connect(radio, &RadioModel::sliceRemoved, this, [this](int sliceId) { destroyFor(sliceId); });
    SliceOwnership* ownership = radio->sliceOwnership();
    if (ownership == nullptr) {
        return;
    }
    connect(ownership, &SliceOwnership::markChanged, this,
            [this](int sliceId, const QByteArray&, const QByteArray&) { refreshOne(sliceId); });
    connect(ownership, &SliceOwnership::controlRevisionChanged, this,
            [this](int sliceId, quint64) { refreshOne(sliceId); });
    connect(ownership, &SliceOwnership::listenersChanged, this,
            [this](int sliceId) { refreshOne(sliceId); });
    // A device's receive choice may move to or from any of its slices.
    connect(ownership, &SliceOwnership::activeRxChanged, this,
            [this](const QByteArray&) { refresh(); });
}

SliceAccessSet::~SliceAccessSet()
{
    if (m_mirror) {
        for (auto it = m_objects.cbegin(); it != m_objects.cend(); ++it) {
            m_mirror->unwatch(keyFor(it.key()));
        }
    }
    // The objects are this set's Qt children and go with it.
    m_objects.clear();
}

QByteArray SliceAccessSet::keyFor(int sliceId)
{
    return QByteArrayLiteral("access:") + QByteArray::number(sliceId);
}

int SliceAccessSet::sliceIdOf(const QByteArray& key)
{
    if (!key.startsWith("access:")) {
        return -1;
    }
    bool ok = false;
    const int id = key.mid(7).toInt(&ok);
    return ok && id >= 0 ? id : -1;
}

void SliceAccessSet::backfill()
{
    if (!m_radio) {
        return;
    }
    for (SliceModel* slice : m_radio->slices()) {
        if (slice != nullptr && !m_objects.contains(slice->sliceIndex())) {
            createFor(slice->sliceIndex());
        }
    }
}

void SliceAccessSet::refresh()
{
    const QList<int> ids = m_objects.keys();
    for (int sliceId : ids) {
        refreshOne(sliceId);
    }
}

SliceAccess* SliceAccessSet::access(int sliceId) const
{
    return m_objects.value(sliceId, nullptr);
}

void SliceAccessSet::refreshOne(int sliceId)
{
    SliceAccess* object = m_objects.value(sliceId, nullptr);
    // A slice being removed keeps the fields it had until its object goes:
    // its control revision reads 0 from then on, and that is not news.
    // A synchronously initialized replacement may already own this ID while
    // the old removal dispatch is unfinished. Its fields belong only to the
    // replacement access object, after destroy-before-create publication.
    const SliceOwnership* ownership = m_radio ? m_radio->sliceOwnership() : nullptr;
    if (object != nullptr && m_resolver && ownership != nullptr
        && ownership->matches({sliceId, static_cast<quint64>(object->incarnation())})) {
        object->setFields(m_resolver(sliceId));
    }
}

void SliceAccessSet::createFor(int sliceId)
{
    if (!m_radio || !m_mirror || m_objects.contains(sliceId)) {
        return;
    }
    SliceModel* slice = m_radio->sliceById(sliceId);
    const SliceOwnership* ownership = m_radio->sliceOwnership();
    if (slice == nullptr || ownership == nullptr) {
        return;
    }
    auto* object = new SliceAccess(
        sliceId, static_cast<qint64>(ownership->incarnation(sliceId)), this);
    if (m_resolver) {
        object->setFields(m_resolver(sliceId));
    }
    const QByteArray key = keyFor(sliceId);
    if (!m_mirror->watch(key, object)) {
        object->deleteLater();
        return;
    }
    m_objects.insert(sliceId, object);
    // The TX mark follows the slice's own flag and its controller's hold.
    m_txMarkWatches.insert(sliceId, connect(slice, &SliceModel::txSliceChanged, this,
                                            [this, sliceId]() { refreshOne(sliceId); }));
    emit accessCreated(key,
                       MirrorSchema::shortClassName(MirrorSchema::forObject(object).className()),
                       m_mirror->snapshot(key));
}

void SliceAccessSet::destroyFor(int sliceId)
{
    SliceAccess* object = m_objects.take(sliceId);
    if (object == nullptr) {
        return;
    }
    const QByteArray key = keyFor(sliceId);
    if (m_mirror) {
        m_mirror->unwatch(key);
    }
    disconnect(m_txMarkWatches.take(sliceId));
    emit accessDestroyed(key, QByteArrayLiteral("SliceAccess"));
    object->deleteLater();
}

} // namespace NereusSDR
