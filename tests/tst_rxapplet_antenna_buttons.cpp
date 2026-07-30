// no-port-check: test fixture asserts UI behavior, no Thetis port
//
// Phase 3P-F Task 4: verify RxApplet antenna buttons populate per-band from
// AlexController, and auto-switch on band change.
//
//  Scenarios covered:
//   1. Construction with Ant 2 on 20m → RX button shows "ANT2" on 20m.
//   2. Construction with bound-slice TX intent Ant 3 on 20m → TX button
//      shows "ANT3" on 20m.
//   3. Band change from 20m (Ant 2) to 40m (Ant 1) → button label switches.
//   4. AlexController::antennaChanged for current band → button repopulates.
//   5. No panadapter (nullptr) → no crash; buttons default to "ANT1".

#include <QtTest/QtTest>
#include <QApplication>

#include "core/accessories/AlexController.h"
#include "gui/applets/RxApplet.h"
#include "models/Band.h"
#include "models/PanadapterModel.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

class TestRxAppletAntennaButtons : public QObject {
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

    // Scenario 1: RX Ant 2 on 20m → RX button shows "ANT2" after construction.
    void rx_ant2_on_20m_shows_ant2_label()
    {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::Hermes);
        model.alexControllerMutable().setRxAnt(Band::Band20m, 2);

        // Add a panadapter and set it to 20m.
        model.addPanadapter();
        model.panadapters().first()->setBand(Band::Band20m);

        // Add a slice so RxApplet has a non-null slice.
        model.addSlice();
        SliceModel* slice = model.sliceById(0);

        RxApplet applet(slice, &model);
        QCOMPARE(applet.activeRxAntennaForTest(), 2);
    }

    // Scenario 2: bound-slice TX Ant 3 intent on 20m → TX button shows
    // "ANT3" after construction.
    void tx_ant3_on_20m_shows_ant3_label()
    {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::Hermes);

        model.addPanadapter();
        model.panadapters().first()->setBand(Band::Band20m);

        model.addSlice();
        SliceModel* slice = model.sliceById(0);
        slice->setFrequency(14'200'000.0);
        slice->setTxAntenna(QStringLiteral("ANT3"));
        QVERIFY(slice->isTxSlice());
        QCOMPARE(slice->txAntenna(), QStringLiteral("ANT3"));

        RxApplet applet(slice, &model);
        QCOMPARE(applet.activeTxAntennaForTest(), 3);
    }

    // Scenario 3: band change from 20m (Ant 2) to 40m (Ant 1).
    void band_change_repopulates_rx_button()
    {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::Hermes);
        // 20m → Ant 2 (RX), 40m → Ant 1 (default)
        model.alexControllerMutable().setRxAnt(Band::Band20m, 2);
        // 40m stays at default Ant 1 (AlexController default)

        model.addPanadapter();
        PanadapterModel* pan = model.panadapters().first();
        pan->setBand(Band::Band20m);

        model.addSlice();
        SliceModel* slice = model.sliceById(0);

        RxApplet applet(slice, &model);
        QCOMPARE(applet.activeRxAntennaForTest(), 2);   // on 20m → Ant 2

        // Now switch pan to 40m.
        pan->setBand(Band::Band40m);
        QCoreApplication::processEvents();

        QCOMPARE(applet.activeRxAntennaForTest(), 1);   // on 40m → Ant 1 (default)
    }

    // Scenario 4: antennaChanged for current band → button repopulates.
    void antenna_changed_signal_repopulates_button()
    {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::Hermes);
        model.alexControllerMutable().setRxAnt(Band::Band20m, 1);

        model.addPanadapter();
        model.panadapters().first()->setBand(Band::Band20m);

        model.addSlice();
        SliceModel* slice = model.sliceById(0);

        RxApplet applet(slice, &model);
        QCOMPARE(applet.activeRxAntennaForTest(), 1);

        // Mutate the controller and emit — RxApplet should observe it.
        model.alexControllerMutable().setRxAnt(Band::Band20m, 3);
        QCoreApplication::processEvents();

        QCOMPARE(applet.activeRxAntennaForTest(), 3);
    }

    // Scenario 5: no panadapter → no crash, buttons default to "ANT1".
    void no_panadapter_no_crash()
    {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::Hermes);
        // No addPanadapter() call — model.panadapters() is empty.

        model.addSlice();
        SliceModel* slice = model.sliceById(0);

        RxApplet applet(slice, &model);
        // Buttons default to whatever SliceModel initialises them to ("ANT1").
        QCOMPARE(applet.activeRxAntennaForTest(), 1);
        QCOMPARE(applet.activeTxAntennaForTest(), 1);
    }
};

QTEST_MAIN(TestRxAppletAntennaButtons)
#include "tst_rxapplet_antenna_buttons.moc"
