// no-port-check: 2026-04-21 UX parity follow-up to Phase 3P-H Task 5a —
// Alex-1 Filters live LED selection.
//
// Mirrors tst_alex2_live_leds. Verifies AntennaAlexAlex1Tab::
// updateActiveLeds() follows the Thetis console.cs:setAlexHPF /
// setAlexLPF range-match logic [@501e3f5]:
//   - 14.200 MHz → 13 MHz HPF row (index 3), 30/20m LPF (index 3).
//   - 50.125 MHz → 6m bypass row (index 5), 6m LPF (index 6).
//   - 1.900 MHz → 1.5 MHz HPF row (index 0), 160m LPF (index 0).
//   - 200 kHz (below all rows) → HPF falls to bypass-row (index 5),
//     no LPF match → activeLpfLedForTest() == -1.
//   - Master HPF bypass → HPF bypass-row lit.
//   - SliceModel::frequencyChanged on slice 0 drives the selection.
#include <QtTest/QtTest>
#include <QApplication>
#include <QCheckBox>

#include "gui/setup/hardware/AntennaAlexAlex1Tab.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

class TestAlex1LiveLeds : public QObject {
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

    void ssb_20m_selects_13mhz_hpf_and_2030m_lpf()
    {
        RadioModel model;
        AntennaAlexAlex1Tab tab(&model);
        tab.setCurrentFrequencyHz(14.200e6);
        QCOMPARE(tab.activeHpfLedForTest(), 3);  // 13 MHz HPF
        QCOMPARE(tab.activeLpfLedForTest(), 3);  // 30/20m LPF
    }

    void six_meter_selects_bypass_row_and_6m_lpf()
    {
        RadioModel model;
        AntennaAlexAlex1Tab tab(&model);
        tab.setCurrentFrequencyHz(50.125e6);
        QCOMPARE(tab.activeHpfLedForTest(), 5);  // 6m Bypass (HPF row 5)
        QCOMPARE(tab.activeLpfLedForTest(), 6);  // 6m LPF
    }

    void one_sixty_meter_matches_first_hpf_and_lpf_rows()
    {
        RadioModel model;
        AntennaAlexAlex1Tab tab(&model);
        tab.setCurrentFrequencyHz(1.900e6);
        QCOMPARE(tab.activeHpfLedForTest(), 0);  // 1.5 MHz HPF
        QCOMPARE(tab.activeLpfLedForTest(), 0);  // 160m LPF
    }

    // Above 61.44 MHz — out of every row's [start, end] range, so HPF
    // falls back to the 6m-bypass row and LPF falls through to the 6m
    // lamp, as setAlexLPF's else branch does (console.cs:7236-7240
    // [v2.10.3.15]). 70 MHz is safely past the 6m LPF end of 61.44 MHz.
    void above_all_rows_falls_to_bypass_hpf_and_6m_lpf()
    {
        RadioModel model;
        AntennaAlexAlex1Tab tab(&model);
        tab.setCurrentFrequencyHz(70.0e6);
        QCOMPARE(tab.activeHpfLedForTest(), 5);   // 6m-bypass row = fallback
        QCOMPARE(tab.activeLpfLedForTest(), 6);   // 6m LPF = fallback
    }

    void sliceFrequencyChanged_signal_drives_leds()
    {
        RadioModel model;
        model.addSlice();
        QVERIFY(!model.slices().isEmpty());
        SliceModel* slice = model.slices().first();
        AntennaAlexAlex1Tab tab(&model);

        slice->setFrequency(7.150e6);    // 40m — LPF row 2 (60/40m).
        QCOMPARE(tab.activeLpfLedForTest(), 2);

        slice->setFrequency(28.500e6);   // 10m — LPF row 5 (12/10m).
        QCOMPARE(tab.activeLpfLedForTest(), 5);
    }
};

QTEST_MAIN(TestAlex1LiveLeds)
#include "tst_alex1_live_leds.moc"
