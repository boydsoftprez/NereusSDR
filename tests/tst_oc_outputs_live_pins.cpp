// no-port-check: Phase 3P-H Task 5b — OC Outputs live pin-state LED row.
//
// Plan Task 14 fix wave (R-R3-49): the row shows the OC byte the connection
// composed (RadioModel::bandOutputsByte), not a byte computed here from
// OcMatrix::maskFor(pan 1's band, MOX), which in a cross-band split showed
// the other slice's pins:
//   - nothing composed yet → byte 0, no LEDs lit, whatever the matrix holds.
//   - a reported byte lights its LEDs.
//   - a band change, a MOX change or a matrix edit on its own changes
//     nothing shown; only the next report does.
// The wire-level cross-band split, locally and in a remote window, is
// tst_band_outputs_display.
#include <QtTest/QtTest>
#include <QApplication>

#include "core/OcMatrix.h"
#include "gui/setup/hardware/OcOutputsHfTab.h"
#include "models/Band.h"
#include "models/PanadapterModel.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

class TestOcOutputsLivePins : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        if (!qApp) {
            static int   argc = 0;
            static char* argv = nullptr;
            new QApplication(argc, &argv);
        }
    }

    // Nothing composed yet: nothing lit, even with a pin set for pan 1's band.
    void nothing_composed_yields_zero_byte()
    {
        RadioModel model;
        model.addPanadapter();
        model.panadapters().first()->setCenterFrequency(14.200e6);
        model.ocMatrixMutable().setPin(Band::Band20m, /*pin=*/0,
                                        /*tx=*/false, /*enabled=*/true);
        OcOutputsHfTab tab(&model, &model.ocMatrixMutable());
        QCOMPARE(int(tab.currentOcByteForTest()), 0);
        for (int pin = 0; pin < 7; ++pin) {
            QVERIFY(!tab.livePinLitForTest(pin));
        }
    }

    // A reported byte lights exactly its pins.
    void reported_byte_lights_its_leds()
    {
        RadioModel model;
        OcOutputsHfTab tab(&model, &model.ocMatrixMutable());
        model.reportBandOutputsForTest(0x09, int(Band::Band40m), /*keyed=*/false);
        QCOMPARE(int(tab.currentOcByteForTest()), 0x09);
        QVERIFY(tab.livePinLitForTest(0));
        QVERIFY(!tab.livePinLitForTest(1));
        QVERIFY(tab.livePinLitForTest(3));
    }

    // A band change, a MOX change or a matrix edit on its own does not
    // change what is shown: the connection's next report does.
    void band_mox_and_matrix_do_not_recompute()
    {
        RadioModel model;
        model.addPanadapter();
        model.panadapters().first()->setCenterFrequency(14.200e6);
        OcMatrix& m = model.ocMatrixMutable();
        m.setPin(Band::Band20m, /*pin=*/0, /*tx=*/false, true);
        m.setPin(Band::Band20m, /*pin=*/1, /*tx=*/true,  true);
        OcOutputsHfTab tab(&model, &m);
        model.reportBandOutputsForTest(0x04, int(Band::Band40m), /*keyed=*/true);
        QCOMPARE(int(tab.currentOcByteForTest()), 0x04);

        model.panadapters().first()->setCenterFrequency(7.150e6);
        model.transmitModel().setMox(true);
        m.setPin(Band::Band40m, /*pin=*/3, /*tx=*/false, true);
        QCOMPARE(int(tab.currentOcByteForTest()), 0x04);

        model.reportBandOutputsForTest(0x01, int(Band::Band20m), /*keyed=*/false);
        QCOMPARE(int(tab.currentOcByteForTest()), 0x01);
        model.transmitModel().setMox(false);
    }

    // setCurrentOcByte direct test — repaints 7 LEDs.
    void set_current_oc_byte_lights_matching_leds()
    {
        RadioModel model;
        OcOutputsHfTab tab(&model, &model.ocMatrixMutable());
        tab.setCurrentOcByte(0x55);  // pins 0, 2, 4, 6
        QVERIFY(tab.livePinLitForTest(0));
        QVERIFY(!tab.livePinLitForTest(1));
        QVERIFY(tab.livePinLitForTest(2));
        QVERIFY(tab.livePinLitForTest(4));
        QVERIFY(tab.livePinLitForTest(6));
    }
};

QTEST_MAIN(TestOcOutputsLivePins)
#include "tst_oc_outputs_live_pins.moc"
