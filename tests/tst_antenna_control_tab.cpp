// no-port-check: smoke test for Antenna Control sub-sub-tab UI construction + AlexController wiring
#include <QtTest/QtTest>
#include <QApplication>
#include <QCheckBox>
#include <QLabel>
#include <QAbstractButton>
#include <QSignalSpy>

#include "gui/setup/hardware/AntennaAlexAntennaControlTab.h"
#include "models/RadioModel.h"
#include "core/accessories/AlexController.h"
#include "models/Band.h"
#include "core/AppSettings.h"

#include "GridHeaderAlignment.h"
#include "core/RadioDiscovery.h"
#include "gui/setup/HardwarePage.h"

using namespace NereusSDR;

class TestAntennaControlTab : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        if (!qApp) {
            static int   argc = 0;
            static char* argv = nullptr;
            new QApplication(argc, &argv);
        }
        AppSettings::instance().clear();
    }

    // Widget constructs without crashing.
    void construct_basic()
    {
        RadioModel model;
        AntennaAlexAntennaControlTab tab(&model);
        QVERIFY(true);  // smoke — construction succeeds
    }

    // The controller() accessor returns the model's AlexController.
    void controller_accessor_returns_model_controller()
    {
        RadioModel model;
        AntennaAlexAntennaControlTab tab(&model);
        // pointer equality: tab's controller IS the model's controller
        QCOMPARE(&tab.controller(), &model.alexControllerMutable());
    }

    // Setting an antenna in the model propagates via the signal (signal-driven sync).
    void model_change_propagates_via_signal()
    {
        RadioModel model;
        AntennaAlexAntennaControlTab tab(&model);

        QSignalSpy spy(&model.alexControllerMutable(), &AlexController::antennaChanged);
        model.alexControllerMutable().setTxAnt(Band::Band20m, 2);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(model.alexController().txAnt(Band::Band20m), 2);
    }

    // Block-TX safety on UI: when blockTxAnt2 is set, attempting setTxAnt(2)
    // is rejected by the controller; TX port stays at default (1).
    void block_tx_ant2_rejects_tx_assignment()
    {
        RadioModel model;
        AntennaAlexAntennaControlTab tab(&model);

        model.alexControllerMutable().setBlockTxAnt2(true);
        model.alexControllerMutable().setTxAnt(Band::Band20m, 2);
        QCOMPARE(model.alexController().txAnt(Band::Band20m), 1);  // stayed at default
    }

    // Block-TX Ant3 rejection works independently.
    void block_tx_ant3_rejects_tx_assignment()
    {
        RadioModel model;
        AntennaAlexAntennaControlTab tab(&model);

        model.alexControllerMutable().setBlockTxAnt3(true);
        model.alexControllerMutable().setTxAnt(Band::Band6m, 3);
        QCOMPARE(model.alexController().txAnt(Band::Band6m), 1);  // stayed at default
    }

    // Block-TX does not prevent RX assignment.
    void block_tx_does_not_block_rx_assignment()
    {
        RadioModel model;
        AntennaAlexAntennaControlTab tab(&model);

        model.alexControllerMutable().setBlockTxAnt2(true);
        model.alexControllerMutable().setRxAnt(Band::Band20m, 2);
        QCOMPARE(model.alexController().rxAnt(Band::Band20m), 2);
    }

    // blockTxChanged signal fires when block-TX state changes.
    void block_tx_changed_signal_fires()
    {
        RadioModel model;
        AntennaAlexAntennaControlTab tab(&model);

        QSignalSpy spy(&model.alexControllerMutable(), &AlexController::blockTxChanged);
        model.alexControllerMutable().setBlockTxAnt2(true);
        QCOMPARE(spy.count(), 1);
    }

    // RX-only antenna assignment works per-band.
    void rx_only_ant_assignment_per_band()
    {
        RadioModel model;
        AntennaAlexAntennaControlTab tab(&model);

        model.alexControllerMutable().setRxOnlyAnt(Band::Band40m, 3);
        QCOMPARE(model.alexController().rxOnlyAnt(Band::Band40m), 3);
        // Phase 3P-I-b T3: default rxOnlyAnt is now 0 ("none selected") per
        // Thetis Alex.cs:58 — was 1 under the 3P-I-a NereusSDR-native default.
        QCOMPARE(model.alexController().rxOnlyAnt(Band::Band20m), 0);  // unaffected (default)
    }
    // Each column label ("1", "2", "3" under RX1, the RX-only labels and
    // TX "Ant 1".."Ant 3") sits over its column of radio buttons, every
    // band row, in today's look and the workspace look (the RX "1" and "2"
    // sat off their columns).
    // RX-only inputs are Thetis checkboxes (setup.cs ProcessAlexAntCheckBox):
    // RX1 and RX2 rule each other out, XVTR stands alone, and the stored
    // value is the sum of the checked boxes' numbers (0..5).
    void rx_only_checkboxes_follow_thetis_sum()
    {
        RadioModel model;
        AntennaAlexAntennaControlTab tab(&model);
        const auto value = [&model]() { return model.alexController().rxOnlyAnt(Band::Band40m); };
        QCheckBox* r1 = tab.rxOnlyButtonForTest(Band::Band40m, 1);
        QCheckBox* r2 = tab.rxOnlyButtonForTest(Band::Band40m, 2);
        QCheckBox* xv = tab.rxOnlyButtonForTest(Band::Band40m, 3);
        QVERIFY(r1 && r2 && xv);
        QCOMPARE(value(), 0);

        xv->click();
        QCOMPARE(value(), 3);
        xv->click();  // XVTR can be turned off again
        QCOMPARE(value(), 0);
        QVERIFY(!xv->isChecked());

        r1->click();
        QCOMPARE(value(), 1);
        r2->click();  // RX2 clears RX1
        QCOMPARE(value(), 2);
        QVERIFY(!r1->isChecked());
        QVERIFY(r2->isChecked());
        xv->click();
        QCOMPARE(value(), 5);
        r1->click();
        QCOMPARE(value(), 4);
        QVERIFY(!r2->isChecked());
        r1->click();
        xv->click();
        QCOMPARE(value(), 0);
        QVERIFY(!r1->isChecked() && !r2->isChecked() && !xv->isChecked());
        QCOMPARE(model.alexController().rxOnlyAnt(Band::Band20m), 0);
    }

    void rx_only_checkboxes_show_the_model_value()
    {
        RadioModel model;
        AntennaAlexAntennaControlTab tab(&model);
        QCheckBox* r1 = tab.rxOnlyButtonForTest(Band::Band20m, 1);
        QCheckBox* r2 = tab.rxOnlyButtonForTest(Band::Band20m, 2);
        QCheckBox* xv = tab.rxOnlyButtonForTest(Band::Band20m, 3);
        const struct { int value; bool one; bool two; bool three; } cases[] = {
            {0, false, false, false}, {1, true, false, false}, {2, false, true, false},
            {3, false, false, true},  {4, true, false, true},  {5, false, true, true},
        };
        for (const auto& c : cases) {
            model.alexControllerMutable().setRxOnlyAnt(Band::Band20m, c.value);
            QCOMPARE(model.alexController().rxOnlyAnt(Band::Band20m), c.value);
            QCOMPARE(r1->isChecked(), c.one);
            QCOMPARE(r2->isChecked(), c.two);
            QCOMPARE(xv->isChecked(), c.three);
        }
        // The clamp keeps Thetis's whole range and nothing past it.
        model.alexControllerMutable().setRxOnlyAnt(Band::Band20m, 6);
        QCOMPARE(model.alexController().rxOnlyAnt(Band::Band20m), 5);
        model.alexControllerMutable().setRxOnlyAnt(Band::Band20m, -1);
        QCOMPARE(model.alexController().rxOnlyAnt(Band::Band20m), 0);
        QVERIFY(!r1->isChecked() && !r2->isChecked() && !xv->isChecked());
    }

    void column_labels_sit_over_their_columns_data()
    {
        QTest::addColumn<bool>("workspace");
        QTest::newRow("today") << false;
        QTest::newRow("look") << true;
    }

    void column_labels_sit_over_their_columns()
    {
        QFETCH(bool, workspace);
        const GridHeaderAlignment::AppLook look(workspace);
        RadioModel model;
        model.setBoardForTest(HPSDRHW::Saturn);
        HardwarePage page(&model);
        RadioInfo info;
        info.boardType  = HPSDRHW::Saturn;
        info.protocol   = ProtocolVersion::Protocol2;
        info.macAddress = QStringLiteral("aa:bb:cc:11:22:33");
        page.onCurrentRadioChanged(info);
        QVERIFY(page.showAntennaTab());
        auto* tab = page.findChild<AntennaAlexAntennaControlTab*>();
        QVERIFY(tab != nullptr);
        look.apply(&page);
        page.resize(1400, 900);
        page.show();
        QVERIFY(QTest::qWaitForWindowExposed(&page));
        int columns = 0;
        for (QLabel* header : tab->findChildren<QLabel*>()) {
            const QString column = header->property("nereusAntennaColumn").toString();
            if (column.isEmpty()) {
                continue;
            }
            ++columns;
            const qreal headerX = GridHeaderAlignment::labelCentreX(header, &page);
            int buttons = 0;
            for (QAbstractButton* button : tab->findChildren<QAbstractButton*>()) {
                if (button->property("nereusAntennaColumn").toString() != column) {
                    continue;
                }
                ++buttons;
                const qreal buttonX = GridHeaderAlignment::indicatorCentreX(button, &page);
                QVERIFY2(qAbs(buttonX - headerX) <= 1.0,
                         qPrintable(QStringLiteral("%1: label centre %2, button centre %3 (band %4)")
                                        .arg(column).arg(headerX).arg(buttonX)
                                        .arg(button->property("nereusAntennaBand").toInt())));
            }
            QVERIFY(buttons > 0);
        }
        QCOMPARE(columns, 9);
    }
};

QTEST_MAIN(TestAntennaControlTab)
#include "tst_antenna_control_tab.moc"
