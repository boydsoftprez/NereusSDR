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

namespace NereusSDR {

TxSliceArbiter::TxSliceArbiter(QObject* parent) : QObject(parent) {}

void TxSliceArbiter::setMoxController(MoxController* mox) { m_mox = mox; }

void TxSliceArbiter::setSliceList(QVector<SliceModel*>* slices) { m_slices = slices; }

bool TxSliceArbiter::requestHandoff(int newSliceIndex)
{
    if (!m_slices || newSliceIndex < 0 || newSliceIndex >= m_slices->size()) {
        emit handoffBlocked(newSliceIndex, QStringLiteral("Slice index out of range"));
        return false;
    }

    if (newSliceIndex == m_txBoundIndex) {
        return true;  // already TX-bound, no-op
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

} // namespace NereusSDR
