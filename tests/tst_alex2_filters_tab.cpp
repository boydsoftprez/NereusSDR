// no-port-check: smoke test for Alex-2 Filters sub-sub-tab UI construction + persistence
#include <QtTest/QtTest>
#include <QApplication>

#include "gui/setup/hardware/AntennaAlexAlex2Tab.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

class TestAlex2FiltersTab : public QObject {
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

    // Construction succeeds for an OrionMKII board (Alex-2 Active).
    void construct_orionmkii_board_active()
    {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::OrionMKII);
        AntennaAlexAlex2Tab tab(&model);

        // Widget must construct without crashing.
        QVERIFY(true);

        // updateBoardCapabilities(true) sets status to "Active".
        tab.updateBoardCapabilities(true);
        QVERIFY(tab.isAlex2Active());
    }

    // Construction succeeds for a Saturn board (Alex-2 Active).
    void construct_saturn_board_active()
    {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::Saturn);
        AntennaAlexAlex2Tab tab(&model);

        QVERIFY(true);
        tab.updateBoardCapabilities(true);
        QVERIFY(tab.isAlex2Active());
    }

    // Construction succeeds for a HermesLite board (no Alex-2).
    void construct_hl2_board_no_alex2()
    {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::HermesLite);
        AntennaAlexAlex2Tab tab(&model);

        // Widget must construct without crashing.
        QVERIFY(true);

        // updateBoardCapabilities(false) leaves status as "Not detected".
        tab.updateBoardCapabilities(false);
        QVERIFY(!tab.isAlex2Active());
    }

    // Default state: Alex-2 not active until updateBoardCapabilities is called.
    void not_active_by_default()
    {
        RadioModel model;
        AntennaAlexAlex2Tab tab(&model);
        // Before any populate call the status should not be active.
        QVERIFY(!tab.isAlex2Active());
    }

    // restoreSettings with empty MAC is a no-op (no crash).
    void restore_empty_mac_noop()
    {
        RadioModel model;
        AntennaAlexAlex2Tab tab(&model);
        tab.restoreSettings(QString());  // must not crash
        QVERIFY(true);
    }
};

QTEST_MAIN(TestAlex2FiltersTab)
#include "tst_alex2_filters_tab.moc"
