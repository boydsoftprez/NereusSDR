// =================================================================
// tests/tst_codec_5_slice_assignment.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Phase 3F Sub-Epic B Tasks 1-7: verify per-board codec emits correct
// multi-slice DDC assignments per design §4. Byte-faithful for the
// RX1/RX2 case (1-2 slices) preserves Thetis console.cs:8186-8538 [v2.10.3.15]
// behaviour; slices C/D/E fill Thetis's idle DDC4/5/6 slots additively.
// =================================================================

#include <QtTest/QtTest>
#include "core/DdcAssignment.h"
#include "core/codec/CodecContext.h"

using namespace NereusSDR;

class TestCodec5SliceAssignment : public QObject {
    Q_OBJECT
private slots:
    void slice_config_struct_has_required_fields()
    {
        SliceConfig sc{};
        sc.frequencyHz = 14225000;
        sc.bandIndex = 5;  // 20m
        sc.sampleRateHz = 192000;
        sc.antennaIndex = 1;
        sc.txBound = true;
        sc.diversityRequested = false;
        sc.live = true;
        QCOMPARE(sc.frequencyHz, qint64(14225000));
        QCOMPARE(sc.live, true);
    }

    void ddc_assignment_struct_has_required_fields()
    {
        DdcAssignment d{};
        d.rate[2] = 192000;
        d.ddcEnable = 0x04;  // DDC2 bit
        QCOMPARE(d.rate[2], 192000);
        QCOMPARE(d.ddcEnable, 0x04);
    }
};

QTEST_MAIN(TestCodec5SliceAssignment)
#include "tst_codec_5_slice_assignment.moc"
