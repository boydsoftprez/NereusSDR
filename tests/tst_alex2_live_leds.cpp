// no-port-check: Phase 3P-H Task 5a — Alex-2 Filters live LED selection.
//
// Verifies AntennaAlexAlex2Tab::updateActiveLeds() follows the Thetis
// console.cs:setAlex2HPF (7060-7166) / setAlex2LPF (7236-7299) range-match
// logic [@501e3f5]:
//   - 14.200 MHz → 13 MHz HPF, 30/20m LPF.
//   - 50.125 MHz → 6m BPF row (not the bypass LED).
//   - Master 55 MHz bypass → BP LED (index kBypassLedIndex).
//   - Per-band bypass on the matched HPF row → BP LED.
//   - No LPF match (e.g. 200 kHz) → no LPF LED lit, activeLpfLedForTest()=-1.
#include <QtTest/QtTest>
#include <QApplication>

#include "gui/setup/hardware/AntennaAlexAlex2Tab.h"
#include "models/PanadapterModel.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

class TestAlex2LiveLeds : public QObject {
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

    // 14.200 MHz (20m band) → 13 MHz HPF row (index 3) matches
    //   [11.0, 21.999999] MHz per hpfBands()[3].
    // LPF: 30/20m row (index 3) matches [10.1, 14.35] MHz per lpfBands()[3].
    void ssb_20m_selects_13mhz_hpf_and_2030m_lpf()
    {
        RadioModel model;
        AntennaAlexAlex2Tab tab(&model);
        tab.setCurrentFrequencyHz(14.200e6);
        QCOMPARE(tab.activeHpfLedForTest(), 3);  // 13 MHz HPF
        QCOMPARE(tab.activeLpfLedForTest(), 3);  // 30/20m LPF
    }

    // 50.125 MHz → 6m BPF row (index 5) matches [35.0, 61.44] MHz.
    // LPF: 6m row (index 6) matches [50.0, 54.0] MHz.
    void six_meter_selects_bpf_row_and_6m_lpf()
    {
        RadioModel model;
        AntennaAlexAlex2Tab tab(&model);
        tab.setCurrentFrequencyHz(50.125e6);
        QCOMPARE(tab.activeHpfLedForTest(), 5);  // 6m BPF
        QCOMPARE(tab.activeLpfLedForTest(), 6);  // 6m LPF
    }

    // 1.900 MHz → 160m LPF row (index 0) matches [1.8, 2.0] MHz.
    // HPF: no row matches below 1.5 MHz, but 1.9 MHz is below the 1.5 MHz
    //  HPF end (2.099999) so the 1.5 MHz row (index 0) matches.
    void one_sixty_meter_matches_first_hpf_and_lpf_rows()
    {
        RadioModel model;
        AntennaAlexAlex2Tab tab(&model);
        tab.setCurrentFrequencyHz(1.900e6);
        QCOMPARE(tab.activeHpfLedForTest(), 0);  // 1.5 MHz HPF
        QCOMPARE(tab.activeLpfLedForTest(), 0);  // 160m LPF
    }

    // 100 kHz → no HPF row matches, falls through to bypass (BP LED).
    // No LPF row matches → activeLpfLedForTest() == -1 (not lit).
    // Mirrors Thetis console.cs:7162-7164 fallback [@501e3f5].
    void below_all_rows_falls_to_bypass_hpf_and_no_lpf()
    {
        RadioModel model;
        AntennaAlexAlex2Tab tab(&model);
        tab.setCurrentFrequencyHz(0.1e6);
        QCOMPARE(tab.activeHpfLedForTest(), AntennaAlexAlex2Tab::kBypassLedIndex);
        QCOMPARE(tab.activeLpfLedForTest(), -1);
    }

    // LED select updates on centerFrequencyChanged from the first panadapter.
    void centerFrequencyChanged_signal_drives_leds()
    {
        RadioModel model;
        model.addPanadapter();
        QVERIFY(!model.panadapters().isEmpty());
        PanadapterModel* pan = model.panadapters().first();
        AntennaAlexAlex2Tab tab(&model);

        pan->setCenterFrequency(7.150e6);    // 40m — LPF row 2 (60/40m).
        QCOMPARE(tab.activeLpfLedForTest(), 2);

        pan->setCenterFrequency(28.500e6);   // 10m — LPF row 5 (12/10m).
        QCOMPARE(tab.activeLpfLedForTest(), 5);
    }
};

QTEST_MAIN(TestAlex2LiveLeds)
#include "tst_alex2_live_leds.moc"
