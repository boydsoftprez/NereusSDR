// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/SliceMarker.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 73 (R-IOS-02): the marker another device sees for a
// slice, and the set of them. See SliceMarker.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 73 (R-IOS-02), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-26: iPhone app plan Task 77 (R-IOS-02, R-IOS-03, R-IOS-13):
//               txSlice by ruling 5.4a. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
// =================================================================

#include "core/session/SliceMarker.h"

#include "core/session/StateMirror.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

namespace NereusSDR {

// ── SliceMarker ──────────────────────────────────────────────────────────

SliceMarker::SliceMarker(SliceModel* slice, QObject* parent)
    : QObject(parent)
    , m_slice(slice)
    , m_sliceId(slice != nullptr ? slice->sliceIndex() : -1)
{
    if (slice == nullptr) {
        return;
    }
    // Relayed with no argument: the mirror reads the property again.
    connect(slice, &SliceModel::frequencyChanged, this, &SliceMarker::frequencyChanged);
    connect(slice, &SliceModel::dspModeChanged, this, &SliceMarker::dspModeChanged);
    connect(slice, &SliceModel::filterChanged, this, &SliceMarker::filterChanged);
    connect(slice, &SliceModel::txSliceChanged, this, &SliceMarker::txSliceChanged);
    connect(slice, &SliceModel::bandChanged, this, &SliceMarker::bandChanged);
    connect(slice, &SliceModel::streamIndexChanged, this, &SliceMarker::streamIndexChanged);
    connect(slice, &SliceModel::psPausedChanged, this, &SliceMarker::psPausedChanged);
}

double SliceMarker::frequency() const
{
    return m_slice ? m_slice->frequency() : 0.0;
}

DSPMode SliceMarker::dspMode() const
{
    return m_slice ? m_slice->dspMode() : DSPMode::USB;
}

int SliceMarker::filterLow() const
{
    return m_slice ? m_slice->filterLow() : 0;
}

int SliceMarker::filterHigh() const
{
    return m_slice ? m_slice->filterHigh() : 0;
}

bool SliceMarker::txSlice() const
{
    // Ruling 5.4a (Task 77): TX only while the slice's owner holds transmit.
    return m_slice && m_slice->txSliceMarked();
}

Band SliceMarker::band() const
{
    return m_slice ? m_slice->band() : Band::GEN;
}

int SliceMarker::streamIndex() const
{
    return m_slice ? m_slice->streamIndex() : -1;
}

bool SliceMarker::psPaused() const
{
    return m_slice && m_slice->psPaused();
}

void SliceMarker::setOwner(const Owner& owner)
{
    if (owner == m_owner) {
        return;
    }
    m_owner = owner;
    emit ownerChanged();
}

// ── SliceMarkerSet ───────────────────────────────────────────────────────

SliceMarkerSet::SliceMarkerSet(RadioModel* radio, StateMirror* mirror, OwnerResolver resolver,
                               QObject* parent)
    : QObject(parent)
    , m_radio(radio)
    , m_mirror(mirror)
    , m_resolver(std::move(resolver))
{
    if (radio == nullptr || mirror == nullptr) {
        return;
    }
    // After ObjectRegistry's own connections (the Core makes this set after
    // it), so a slice's object.create goes out before its marker's and its
    // object.destroy before its marker's.
    connect(radio, &RadioModel::sliceAdded, this, [this](int sliceId) { createFor(sliceId); });
    connect(radio, &RadioModel::sliceRemoved, this, [this](int sliceId) { destroyFor(sliceId); });
}

SliceMarkerSet::~SliceMarkerSet()
{
    if (m_mirror) {
        for (auto it = m_markers.cbegin(); it != m_markers.cend(); ++it) {
            m_mirror->unwatch(keyFor(it.key()));
        }
    }
    // The markers are this set's Qt children and go with it.
    m_markers.clear();
}

QByteArray SliceMarkerSet::keyFor(int sliceId)
{
    return QByteArrayLiteral("marker:") + QByteArray::number(sliceId);
}

int SliceMarkerSet::sliceIdOf(const QByteArray& key)
{
    if (!key.startsWith("marker:")) {
        return -1;
    }
    bool ok = false;
    const int id = key.mid(7).toInt(&ok);
    return ok && id >= 0 ? id : -1;
}

void SliceMarkerSet::backfill()
{
    if (!m_radio) {
        return;
    }
    for (SliceModel* slice : m_radio->slices()) {
        if (slice != nullptr && !m_markers.contains(slice->sliceIndex())) {
            createFor(slice->sliceIndex());
        }
    }
}

void SliceMarkerSet::refreshOwners()
{
    for (auto it = m_markers.cbegin(); it != m_markers.cend(); ++it) {
        it.value()->setOwner(m_resolver ? m_resolver(it.key()) : SliceMarker::Owner{});
    }
}

SliceMarker* SliceMarkerSet::marker(int sliceId) const
{
    return m_markers.value(sliceId, nullptr);
}

void SliceMarkerSet::createFor(int sliceId)
{
    if (!m_radio || !m_mirror || m_markers.contains(sliceId)) {
        return;
    }
    SliceModel* slice = m_radio->sliceById(sliceId);
    if (slice == nullptr) {
        return;
    }
    auto* marker = new SliceMarker(slice, this);
    marker->setOwner(m_resolver ? m_resolver(sliceId) : SliceMarker::Owner{});
    const QByteArray key = keyFor(sliceId);
    if (!m_mirror->watch(key, marker)) {
        marker->deleteLater();
        return;
    }
    m_markers.insert(sliceId, marker);
    emit markerCreated(key,
                       MirrorSchema::shortClassName(MirrorSchema::forObject(marker).className()),
                       m_mirror->snapshot(key));
}

void SliceMarkerSet::destroyFor(int sliceId)
{
    SliceMarker* marker = m_markers.take(sliceId);
    if (marker == nullptr) {
        return;
    }
    const QByteArray key = keyFor(sliceId);
    if (m_mirror) {
        m_mirror->unwatch(key);
    }
    emit markerDestroyed(key, QByteArrayLiteral("SliceMarker"));
    marker->deleteLater();
}

} // namespace NereusSDR
