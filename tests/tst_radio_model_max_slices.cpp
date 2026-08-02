// =================================================================
// tests/tst_radio_model_max_slices.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Phase 3F Sub-Epic A Task 11: verify RadioModel::maxSlices() exposes
// the connected SKU's slice cap. Disconnected returns 1 (single-slice fallback).
// =================================================================

#include <QtTest/QtTest>
#include "models/RadioModel.h"

using namespace NereusSDR;

class TestRadioModelMaxSlices : public QObject {
    Q_OBJECT
private slots:
    void disconnected_max_slices_is_1()
    {
        RadioModel radio;
        QCOMPARE(radio.maxSlices(), 1);  // safe default when not connected
    }
};

QTEST_MAIN(TestRadioModelMaxSlices)
#include "tst_radio_model_max_slices.moc"
