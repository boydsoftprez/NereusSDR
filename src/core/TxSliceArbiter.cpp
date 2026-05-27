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

bool TxSliceArbiter::requestHandoff(int /*newSliceIndex*/)
{
    // Stub: filled in Task 3
    return false;
}

} // namespace NereusSDR
