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

    // HermesC10 (ANAN-G2E) uses the OrionII/Saturn BPF1 algorithm.
    // From Thetis console.cs:6829-6834 [v2.10.3.15] //N1GP G2E added (HermesC10) //DK1HLM —
    // setAlex1HPF dispatches setBPF1ForOrionIISaturn for OrionMKII || Saturn || HermesC10.
    // AntennaAlexTab::populate() gates the BPF1 column on (board==Saturn||SaturnMKII||HermesC10).
    // This test exercises the true-path for HermesC10 directly via updateBoardCapabilities.
    void hermesC10_showsBpf1Column()
    {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::HermesC10);
        AntennaAlexAlex1Tab tab(&model);

        // HermesC10 joins OrionMKII + Saturn for the BPF1 algorithm path.
        // AntennaAlexTab::populate() computes:
        //   isSaturn = (caps.board == Saturn || caps.board == SaturnMKII || caps.board == HermesC10)
        // and calls updateBoardCapabilities(isSaturn=true).
        tab.updateBoardCapabilities(true);
        QVERIFY(tab.isSaturnBpf1Visible());
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
