// =================================================================
// src/core/TxSliceArbiter.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port. See TxSliceArbiter.h for header
// notes and design reference.
//
// =================================================================
#include "core/TxSliceArbiter.h"
#include "models/SliceModel.h"
#include "core/MoxController.h"
#include "core/AppSettings.h"

namespace NereusSDR {

TxSliceArbiter::TxSliceArbiter(QObject* parent) : QObject(parent) {}

void TxSliceArbiter::setMoxController(MoxController* mox) { m_mox = mox; }

// Pure wiring: deliberately does NOT sync. An initial bind carries an RF
// guard that needs the MoxController, so binding as a side effect of
// whichever setter happened to be called first would make RF safety depend
// on wiring order. Callers hand over the list, finish wiring, then call
// syncToSliceList() (RadioModel does it from addSlice / removeSlice).
void TxSliceArbiter::setSliceList(QVector<SliceModel*>* slices) { m_slices = slices; }

SliceModel* TxSliceArbiter::txBoundSlice() const
{
    if (!m_slices || m_txBoundIndex < 0 || m_txBoundIndex >= m_slices->size()) {
        return nullptr;
    }
    return m_slices->at(m_txBoundIndex);
}

// ---------------------------------------------------------------------------
// syncToSliceList: the initial binding, and the invariant's repair arm.
//
// The defect this closes: requestHandoff was the ONLY writer of
// SliceModel::txSlice, and it early-returns when the requested index already
// equals m_txBoundIndex. m_txBoundIndex defaults to 0, so requestHandoff(0)
// took that arm and never raised the flag. A single-slice session, and any
// session where the operator never explicitly moved TX, therefore ran with
// isTxSlice() false on every slice for the life of the process, silently
// disabling every consumer that asks which slice transmits.
//
// The flag on the SliceModel is the authority for WHICH slice transmits;
// m_txBoundIndex is a cache of that slice's position in the list. Deriving
// the index from the flag (rather than the other way round) is what makes
// this safe to call after a removal: the flagged OBJECT survives the list
// mutation, only its position moves.
// ---------------------------------------------------------------------------
void TxSliceArbiter::syncToSliceList()
{
    if (!m_slices || m_slices->isEmpty()) {
        // No slices, so no binding to hold. m_txBoundIndex is left alone: it
        // may be carrying a value load() restored, and the first slice to
        // arrive should honour it.
        return;
    }

    int firstFlagged = -1;
    int flaggedCount = 0;
    for (int i = 0; i < m_slices->size(); ++i) {
        SliceModel* s = m_slices->at(i);
        if (s && s->isTxSlice()) {
            if (firstFlagged < 0) { firstFlagged = i; }
            ++flaggedCount;
        }
    }

    if (flaggedCount == 1) {
        // The transmitter is where it was; only its position may have moved.
        // Deliberately silent: txBoundSliceChanged means "the transmitter
        // moved to a different slice", and subscribers act on it (re-badge
        // every VFO flag, re-push the transmit frequency, toast the
        // operator). Announcing a re-index would tell them a handoff
        // happened that did not.
        m_txBoundIndex = firstFlagged;
        return;
    }

    if (flaggedCount > 1) {
        // Defensive: nothing outside this class writes the flag today, so
        // this is a guard against a future second writer rather than a live
        // path. Keep the slice the index already names if it is one of the
        // flagged, otherwise the first flagged, and clear the rest.
        const bool indexInRange = m_txBoundIndex >= 0
                                  && m_txBoundIndex < m_slices->size();
        const bool indexIsFlagged = indexInRange
                                    && m_slices->at(m_txBoundIndex)
                                    && m_slices->at(m_txBoundIndex)->isTxSlice();
        const int keep = indexIsFlagged ? m_txBoundIndex : firstFlagged;
        for (int i = 0; i < m_slices->size(); ++i) {
            if (i != keep && m_slices->at(i)) {
                m_slices->at(i)->setTxSlice(false);
            }
        }
        m_txBoundIndex = keep;
        return;
    }

    // ── Initial bind ────────────────────────────────────────────────────
    // Nothing is flagged and slices exist, so the transmitter has no home.
    // Give it one. Honour the current index (which load() may have restored
    // from AppSettings) when it names a live slice; otherwise fall back to
    // slice A, per design §6 "Restore on launch": "If that slice doesn't
    // exist post-restore (e.g. operator deleted it last session), default to
    // Slice A."
    const int target = (m_txBoundIndex >= 0 && m_txBoundIndex < m_slices->size())
                           ? m_txBoundIndex
                           : 0;
    SliceModel* slice = m_slices->at(target);
    if (!slice) { return; }

    // RF-safe, same guard requestHandoff uses. Unreachable on the true first
    // bind (nothing to key from before a slice exists); present for the
    // degenerate keyed-but-unbound case.
    if (m_mox && m_mox->isMox()) {
        m_mox->setMox(false);
    }

    slice->setTxSlice(true);
    m_txBoundIndex = target;
    // oldIndex -1: there was no previous binding, so this is not a handoff.
    // Subscribers that treat it as one (status-bar "TX > Slice X" toast)
    // check for the sentinel.
    emit txBoundSliceChanged(-1, target);
}

bool TxSliceArbiter::requestHandoff(int newSliceIndex)
{
    if (!m_slices || newSliceIndex < 0 || newSliceIndex >= m_slices->size()) {
        emit handoffBlocked(newSliceIndex, QStringLiteral("Slice index out of range"));
        return false;
    }

    if (newSliceIndex == m_txBoundIndex) {
        return true;  // already TX-bound, no-op
    }

    // RF-safe handoff: drop MOX before changing TX-bound slice.
    if (m_mox && m_mox->isMox()) {
        m_mox->setMox(false);
        // MoxController::setMox is synchronous on the moxChanged side; if it ever
        // becomes async, switch to a QEventLoop wait on moxChanged here.
    }

    const int oldIndex = m_txBoundIndex;

    // Flip txSlice flags on the affected slices.
    if (oldIndex >= 0 && oldIndex < m_slices->size()) {
        m_slices->at(oldIndex)->setTxSlice(false);
    }
    m_slices->at(newSliceIndex)->setTxSlice(true);

    m_txBoundIndex = newSliceIndex;
    emit txBoundSliceChanged(oldIndex, newSliceIndex);
    return true;
}

void TxSliceArbiter::save()
{
    if (m_mac.isEmpty()) { return; }
    auto& s = AppSettings::instance();
    const QString key = QStringLiteral("hardware/%1/TxBoundSliceIndex").arg(m_mac);
    s.setValue(key, m_txBoundIndex);
}

void TxSliceArbiter::load()
{
    if (m_mac.isEmpty()) { return; }
    auto& s = AppSettings::instance();
    const QString key = QStringLiteral("hardware/%1/TxBoundSliceIndex").arg(m_mac);
    const int restored = s.value(key, 0).toInt();

    // A restored index that names a slice this session does not have is
    // DISCARDED, not remembered: design §6 "Restore on launch" says default
    // to Slice A when the persisted slice no longer exists, and remembering
    // it would leave txBoundSliceIndex() naming a slice nothing can resolve.
    // The syncToSliceList() below is what actually lands on Slice A in that
    // case (and what guarantees a binding exists at all, restore or not).
    //
    // requestHandoff's own "already TX-bound" arm handles restored ==
    // m_txBoundIndex, so there is no second equality check here.
    if (restored >= 0 && m_slices && restored < m_slices->size()) {
        requestHandoff(restored);
    }
    syncToSliceList();
}

} // namespace NereusSDR
