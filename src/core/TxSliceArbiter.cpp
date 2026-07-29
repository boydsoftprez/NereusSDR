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
    if (!m_slices) { return nullptr; }
    for (SliceModel* slice : *m_slices) {
        if (slice && slice->sliceIndex() == m_txBoundSliceId) {
            return slice;
        }
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// syncToSliceList: the initial binding, and the invariant's repair arm.
//
// The defect this closes: requestHandoff was the ONLY writer of
// SliceModel::txSlice, and it early-returns when the requested index already
// equals m_txBoundSliceId. A default binding therefore has to be established
// took that arm and never raised the flag. A single-slice session, and any
// session where the operator never explicitly moved TX, therefore ran with
// isTxSlice() false on every slice for the life of the process, silently
// disabling every consumer that asks which slice transmits.
//
// The flag on the SliceModel is the authority for WHICH slice transmits;
// m_txBoundSliceId is a cache of that slice's stable identity. Deriving the
// ID from the flag (rather than the other way round) is what makes
// this safe to call after a removal: the flagged OBJECT survives the list
// mutation, only its position moves.
// ---------------------------------------------------------------------------
void TxSliceArbiter::syncToSliceList()
{
    if (!m_slices || m_slices->isEmpty()) {
        // No slices, so no binding to hold. The restored ID is left alone: it
        // may be carrying a value load() restored, and the first slice to
        // arrive should honour it.
        return;
    }

    SliceModel* firstFlagged = nullptr;
    int flaggedCount = 0;
    for (SliceModel* s : *m_slices) {
        if (s && s->isTxSlice()) {
            if (!firstFlagged) { firstFlagged = s; }
            ++flaggedCount;
        }
    }

    if (flaggedCount == 1) {
        // The transmitter is where it was; silently adopt its stable ID.
        // Deliberately silent: txBoundSliceChanged means "the transmitter
        // moved to a different slice", and subscribers act on it (re-badge
        // every VFO flag, re-push the transmit frequency, toast the
        // operator). Announcing a re-index would tell them a handoff
        // happened that did not.
        m_txBoundSliceId = firstFlagged->sliceIndex();
        return;
    }

    if (flaggedCount > 1) {
        // Defensive: nothing outside this class writes the flag today, so
        // this is a guard against a future second writer rather than a live
        // path. Keep the slice the stable ID already names if it is flagged,
        // otherwise the first flagged slice, and clear the rest.
        SliceModel* keep = txBoundSlice();
        if (!keep || !keep->isTxSlice()) {
            keep = firstFlagged;
        }
        for (SliceModel* slice : *m_slices) {
            if (slice && slice != keep) {
                slice->setTxSlice(false);
            }
        }
        m_txBoundSliceId = keep->sliceIndex();
        return;
    }

    // ── Initial bind ────────────────────────────────────────────────────
    // Nothing is flagged and slices exist, so the transmitter has no home.
    // Give it one. Honour the current ID (which load() may have restored
    // from AppSettings) when it names a live slice; otherwise fall back to
    // slice A, per design §6 "Restore on launch": "If that slice doesn't
    // exist post-restore (e.g. operator deleted it last session), default to
    // Slice A."
    SliceModel* slice = txBoundSlice();
    if (!slice) {
        for (SliceModel* candidate : *m_slices) {
            if (candidate) {
                slice = candidate;
                break;
            }
        }
    }
    if (!slice) { return; }

    // RF-safe, same guard requestHandoff uses. Unreachable on the true first
    // bind (nothing to key from before a slice exists); present for the
    // degenerate keyed-but-unbound case.
    if (m_mox && m_mox->isMox()) {
        m_mox->setMox(false);
    }

    slice->setTxSlice(true);
    m_txBoundSliceId = slice->sliceIndex();
    // oldId -1: there was no previous binding, so this is not a handoff.
    // Subscribers that treat it as one (status-bar "TX > Slice X" toast)
    // check for the sentinel.
    emit txBoundSliceChanged(-1, m_txBoundSliceId);
}

bool TxSliceArbiter::requestHandoff(int sliceId)
{
    SliceModel* target = nullptr;
    if (m_slices) {
        for (SliceModel* slice : *m_slices) {
            if (slice && slice->sliceIndex() == sliceId) {
                target = slice;
                break;
            }
        }
    }
    if (!target) {
        emit handoffBlocked(sliceId, QStringLiteral("Slice ID not found"));
        return false;
    }

    if (target->isTxSlice()) {
        m_txBoundSliceId = sliceId;
        return true;  // already TX-bound, no-op
    }

    // RF-safe handoff: drop MOX before changing TX-bound slice.
    if (m_mox && m_mox->isMox()) {
        m_mox->setMox(false);
        // MoxController::setMox is synchronous on the moxChanged side; if it ever
        // becomes async, switch to a QEventLoop wait on moxChanged here.
    }

    int oldId = m_txBoundSliceId;
    if (!txBoundSlice()) {
        for (SliceModel* slice : *m_slices) {
            if (slice && slice->isTxSlice()) {
                oldId = slice->sliceIndex();
                break;
            }
        }
    }

    // The ID cache may have been restored before the live flags were
    // normalised, so clear every other slice rather than trusting one
    // positional predecessor.
    for (SliceModel* slice : *m_slices) {
        if (slice && slice != target) {
            slice->setTxSlice(false);
        }
    }
    target->setTxSlice(true);

    m_txBoundSliceId = sliceId;
    emit txBoundSliceChanged(oldId, sliceId);
    return true;
}

void TxSliceArbiter::save()
{
    if (m_mac.isEmpty()) { return; }
    auto& s = AppSettings::instance();
    const QString key = QStringLiteral("hardware/%1/TxBoundSliceId").arg(m_mac);
    s.setValue(key, m_txBoundSliceId);
}

void TxSliceArbiter::load()
{
    if (m_mac.isEmpty()) { return; }
    auto& s = AppSettings::instance();
    const QString idKey = QStringLiteral("hardware/%1/TxBoundSliceId").arg(m_mac);
    const QString legacyKey =
        QStringLiteral("hardware/%1/TxBoundSliceIndex").arg(m_mac);

    int restoredId = -1;
    if (s.contains(idKey)) {
        restoredId = s.value(idKey, -1).toInt();
    } else if (s.contains(legacyKey) && m_slices) {
        const int legacyPosition = s.value(legacyKey, -1).toInt();
        if (legacyPosition >= 0 && legacyPosition < m_slices->size()) {
            if (SliceModel* legacySlice = m_slices->at(legacyPosition)) {
                restoredId = legacySlice->sliceIndex();
                s.setValue(idKey, restoredId);
            }
        }
    }

    if (restoredId >= 0) {
        bool liveId = false;
        if (m_slices) {
            for (SliceModel* slice : *m_slices) {
                if (slice && slice->sliceIndex() == restoredId) {
                    liveId = true;
                    break;
                }
            }
        }
        if (liveId) {
            requestHandoff(restoredId);
        } else {
            m_txBoundSliceId = restoredId;
        }
    }
    syncToSliceList();
}

} // namespace NereusSDR
