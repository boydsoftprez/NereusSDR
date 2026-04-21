// no-port-check: smoke test for Alex-1 Filters sub-sub-tab UI construction + persistence
#include <QtTest/QtTest>
#include <QApplication>

#include "gui/setup/hardware/AntennaAlexAlex1Tab.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

class TestAlex1FiltersTab : public QObject {
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

    // Construction succeeds for a Saturn board (Saturn BPF1 visible).
    void construct_saturn_board()
    {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::Saturn);
        AntennaAlexAlex1Tab tab(&model);

        // Widget must construct without crashing.
        QVERIFY(true);

        // updateBoardCapabilities(true) shows the BPF1 column.
        tab.updateBoardCapabilities(true);
        QVERIFY(tab.isSaturnBpf1Visible());
    }

    // Construction succeeds for a non-Saturn board (Saturn BPF1 hidden).
    void construct_orionmkii_board()
    {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::OrionMKII);
        AntennaAlexAlex1Tab tab(&model);

        // Widget must construct without crashing.
        QVERIFY(true);

        // updateBoardCapabilities(false) hides the BPF1 column.
        tab.updateBoardCapabilities(false);
        QVERIFY(!tab.isSaturnBpf1Visible());
    }

    // Default state: BPF1 is hidden until updateBoardCapabilities is called.
    void bpf1_hidden_by_default()
    {
        RadioModel model;
        AntennaAlexAlex1Tab tab(&model);
        // Before any populate call the BPF1 group should be hidden.
        QVERIFY(!tab.isSaturnBpf1Visible());
    }

    // restoreSettings with empty MAC is a no-op (no crash).
    void restore_empty_mac_noop()
    {
        RadioModel model;
        AntennaAlexAlex1Tab tab(&model);
        tab.restoreSettings(QString());  // must not crash
        QVERIFY(true);
    }
};

QTEST_MAIN(TestAlex1FiltersTab)
#include "tst_alex1_filters_tab.moc"
