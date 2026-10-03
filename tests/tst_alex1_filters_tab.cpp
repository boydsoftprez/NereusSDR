// no-port-check: smoke test for Alex-1 Filters sub-sub-tab UI construction + persistence
#include <QtTest/QtTest>
#include <QApplication>
#include <QGroupBox>

#include "core/BoardCapabilities.h"
#include "core/HpsdrModel.h"
#include "core/codec/AlexFilterMap.h"
#include "core/RadioDiscovery.h"
#include "gui/setup/hardware/AntennaAlexAlex1Tab.h"
#include "gui/setup/hardware/AntennaAlexTab.h"
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

    // Thetis swaps the Alex-1 tab's panels by model: on the 7000D, 8000D,
    // AnvelinaPro3, G2E, G2, G2-1K and RedPitaya it hides the Alex HPF panel,
    // shows the BPF panel and moves the five switches into it; on every
    // other model the HPF panel shows with the switches and BPF is hidden.
    // From Thetis setup.cs:6336-6360 [v2.10.3.15] (//N1GP G2E added, //DH1KLM)
    // and setup.cs:20208-20220 [v2.10.3.15] for the 7000D (8000D 20260-20272).
    // (console.cs:6827-6837 [v2.10.3.15] routes the filters by board:
    // //N1GP G2E added (HermesC10) //DK1HLM.)
    static QString switchGroupTitle(AntennaAlexAlex1Tab& tab)
    {
        auto* box = tab.findChild<QWidget*>(QStringLiteral("alexHpfBypassOnPs"));
        for (QWidget* up = box ? box->parentWidget() : nullptr; up; up = up->parentWidget()) {
            if (auto* group = qobject_cast<QGroupBox*>(up)) {
                return group->title();
            }
        }
        return {};
    }

    void bpfPanelModels_populateSwapsHpfForBpf1()
    {
        const HPSDRModel bpfModels[] = {
            HPSDRModel::ANAN7000D, HPSDRModel::ANAN8000D, HPSDRModel::ANVELINAPRO3,
            HPSDRModel::ANAN_G2E, HPSDRModel::ANAN_G2, HPSDRModel::ANAN_G2_1K,
            HPSDRModel::REDPITAYA, HPSDRModel::ORIONMKII};
        for (const HPSDRModel sku : bpfModels) {
            RadioModel model;
            model.setHpsdrModelForTest(sku);
            AntennaAlexTab tab(&model);
            RadioInfo info;
            info.boardType = boardForModel(sku);
            tab.populate(info, BoardCapsTable::forBoard(info.boardType));
            auto* alex1 = tab.findChild<AntennaAlexAlex1Tab*>();
            QVERIFY(alex1);
            QVERIFY2(alex1->isSaturnBpf1Visible(), qPrintable(QString::number(int(sku))));
            QVERIFY2(!alex1->isAlexHpfVisible(), qPrintable(QString::number(int(sku))));
            QCOMPARE(switchGroupTitle(*alex1), QStringLiteral("Saturn BPF1 Bands"));
        }
        // The HPF-ladder models keep the HPF panel, with the switches in it,
        // and hide BPF. The plain ORION MKII model is on the OrionMKII board,
        // which the Core and Thetis program through BPF1 (console.cs:6827-6837
        // [v2.10.3.15]), so it is in the BPF list above.
        for (const HPSDRModel sku : {HPSDRModel::ANAN200D,
                                     HPSDRModel::ANAN100D, HPSDRModel::ANAN100}) {
            RadioModel model;
            model.setHpsdrModelForTest(sku);
            AntennaAlexTab tab(&model);
            RadioInfo info;
            info.boardType = boardForModel(sku);
            tab.populate(info, BoardCapsTable::forBoard(info.boardType));
            auto* alex1 = tab.findChild<AntennaAlexAlex1Tab*>();
            QVERIFY(alex1);
            QVERIFY2(!alex1->isSaturnBpf1Visible(), qPrintable(QString::number(int(sku))));
            QVERIFY2(alex1->isAlexHpfVisible(), qPrintable(QString::number(int(sku))));
            QCOMPARE(switchGroupTitle(*alex1), QStringLiteral("Alex HPF Bands"));
        }
    }

    // Every Alex model: the bank the tab shows is the bank the Core programs
    // for that model's board. Bypassing every row of the shown bank changes
    // the receive filter computeRxPreselector selects; bypassing every row
    // of the hidden bank changes nothing.
    void everyModel_panelMatchesAppliedFilterBank()
    {
        using namespace NereusSDR::codec::alex;
        int checked = 0;
        for (int m = int(HPSDRModel::FIRST) + 1; m < int(HPSDRModel::LAST); ++m) {
            const HPSDRModel sku = HPSDRModel(m);
            const HPSDRHW board = boardForModel(sku);
            const BoardCapabilities caps = BoardCapsTable::forBoard(board);
            if (!caps.hasAlexFilters) {
                continue;
            }
            RadioModel model;
            model.setHpsdrModelForTest(sku);
            AntennaAlexTab tab(&model);
            RadioInfo info;
            info.boardType = board;
            tab.populate(info, caps);
            auto* alex1 = tab.findChild<AntennaAlexAlex1Tab*>();
            QVERIFY(alex1);
            const bool bpf1Applied = usesBpf1Preselector(board);
            const QString tag = QString::number(m);
            QVERIFY2(alex1->isSaturnBpf1Visible() == bpf1Applied, qPrintable(tag));
            QVERIFY2(alex1->isAlexHpfVisible() == !bpf1Applied, qPrintable(tag));

            const AlexHpfEdges defaults = AlexHpfEdges::thetisDefaults();
            AlexHpfEdges shownBypassed = defaults;
            AlexHpfEdges hiddenBypassed = defaults;
            AlexHpfRows& shown = bpf1Applied ? shownBypassed.bpf1 : shownBypassed.hpf;
            AlexHpfRows& hidden = bpf1Applied ? hiddenBypassed.hpf : hiddenBypassed.bpf1;
            for (AlexHpfRow& row : shown) {
                row.bypass = true;
            }
            for (AlexHpfRow& row : hidden) {
                row.bypass = true;
            }
            const double freqMhz = 14.1;
            QVERIFY2(computeRxPreselector(freqMhz, board, shownBypassed)
                         != computeRxPreselector(freqMhz, board, defaults), qPrintable(tag));
            QVERIFY2(computeRxPreselector(freqMhz, board, hiddenBypassed)
                         == computeRxPreselector(freqMhz, board, defaults), qPrintable(tag));
            ++checked;
        }
        QVERIFY(checked > 0);
    }

    // The swap goes back: a later populate for an HPF model returns the
    // switches to the HPF panel.
    void bpfPanel_swapIsReversible()
    {
        RadioModel model;
        AntennaAlexAlex1Tab tab(&model);
        tab.updateBoardCapabilities(true);
        QVERIFY(!tab.isAlexHpfVisible());
        QCOMPARE(switchGroupTitle(tab), QStringLiteral("Saturn BPF1 Bands"));
        tab.updateBoardCapabilities(false);
        QVERIFY(tab.isAlexHpfVisible());
        QVERIFY(!tab.isSaturnBpf1Visible());
        QCOMPARE(switchGroupTitle(tab), QStringLiteral("Alex HPF Bands"));
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
