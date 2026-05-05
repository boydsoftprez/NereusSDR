// no-port-check: NereusSDR-original test file.  Exercises the per-SKU
// power-slider rescale + dB-label formula + Thetis-faithful tooltips.
// All Thetis source cites for the underlying formulae live in
// TxApplet.cpp (rescalePowerSlidersForModel + updatePowerSliderLabels)
// and HpsdrModel.h (per-SKU constants).
//
// =================================================================
// tests/tst_tx_applet_hl2_slider.cpp  (NereusSDR)
// =================================================================
//
// Issue #175 / mi0bot HL2 TX power port - Task 7.
//
// On HL2 (HPSDRModel::HERMESLITE), TxApplet's RF Power and Tune Power
// sliders rescale to mirror Thetis HL2 behaviour:
//   - RF Power slider:    Maximum 90, single/page step 6
//                          (mi0bot console.cs:2098-2108 [v2.10.3.13-beta2]
//                           "MI0BOT: Changes for HL2 only having a 16 step
//                           output attenuator")
//   - Tune Power slider:  Maximum 99, single/page step 3
//                          (33 sub-steps for fine TUNE drive control)
//   - RF Power label:     -X.X dB via mi0bot formula
//                            (round(drv/6.0)/2) - 7.5
//                          (mi0bot console.cs:29245-29274 [v2.10.3.13-beta2])
//   - Tune Power label:   -X.X dB via inverse persistence formula
//                            (slider/3.0 - 33.0) / 2.0
//   - Tooltip on both:    Thetis-faithful "Transmit Drive - relative
//                         value, not watts unless the PA profile is
//                         configured with MAX watts @ 100%"
//                         (Thetis console.resx + mi0bot console.resx
//                          [v2.10.3.13-beta2]).
//
// On non-HL2 SKUs, sliders stay at canonical Thetis 0..100 step 1, and
// the labels display the bare integer value (no dB conversion).
//
// =================================================================

#include <QtTest/QtTest>
#include <QApplication>
#include <QSlider>
#include <QLabel>

#include "core/HpsdrModel.h"
#include "gui/applets/TxApplet.h"
#include "models/RadioModel.h"

using NereusSDR::HPSDRModel;

class TestTxAppletHl2Slider : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        if (!qApp) {
            static int argc = 0;
            new QApplication(argc, nullptr);
        }
    }

    // ── 1. HL2: RF Power slider rescales to 0..90 step 6 ────────────────
    void hl2_rfSlider_rescales()
    {
        NereusSDR::RadioModel rm;
        NereusSDR::TxApplet ta(&rm);
        ta.rescalePowerSlidersForModel(HPSDRModel::HERMESLITE);
        QCOMPARE(ta.rfPowerSlider()->maximum(),    90);
        QCOMPARE(ta.rfPowerSlider()->singleStep(),  6);
        QCOMPARE(ta.rfPowerSlider()->pageStep(),    6);
    }

    // ── 2. HL2: Tune Power slider rescales to 0..99 step 3 ─────────────
    void hl2_tuneSlider_rescales()
    {
        NereusSDR::RadioModel rm;
        NereusSDR::TxApplet ta(&rm);
        ta.rescalePowerSlidersForModel(HPSDRModel::HERMESLITE);
        QCOMPARE(ta.tunePowerSlider()->maximum(),    99);
        QCOMPARE(ta.tunePowerSlider()->singleStep(),  3);
    }

    // ── 3. SKU swap-back: HL2 → ANAN-100 returns to canonical 0..100 ───
    void anan100_returnsToDefault()
    {
        NereusSDR::RadioModel rm;
        NereusSDR::TxApplet ta(&rm);
        ta.rescalePowerSlidersForModel(HPSDRModel::HERMESLITE);
        ta.rescalePowerSlidersForModel(HPSDRModel::ANAN100);
        QCOMPARE(ta.rfPowerSlider()->maximum(),     100);
        QCOMPARE(ta.rfPowerSlider()->singleStep(),    1);
        QCOMPARE(ta.tunePowerSlider()->maximum(),   100);
    }

    // ── 4. HL2 RF Power label formula at slider=60 ─────────────────────
    //   (round(60/6.0)/2) - 7.5 = (10/2) - 7.5 = 5 - 7.5 = -2.5
    void hl2_label_formula_at_60()
    {
        NereusSDR::RadioModel rm;
        NereusSDR::TxApplet ta(&rm);
        ta.rescalePowerSlidersForModel(HPSDRModel::HERMESLITE);
        ta.rfPowerSlider()->setValue(60);
        ta.updatePowerSliderLabels();
        QCOMPARE(ta.rfPowerLabel()->text(), QStringLiteral("-2.5"));
    }

    // ── 5. HL2 RF Power label formula at slider=90 (max → 0.0 dB) ──────
    //   (round(90/6.0)/2) - 7.5 = (15/2) - 7.5 = 7.5 - 7.5 = 0.0
    void hl2_label_at_max()
    {
        NereusSDR::RadioModel rm;
        NereusSDR::TxApplet ta(&rm);
        ta.rescalePowerSlidersForModel(HPSDRModel::HERMESLITE);
        ta.rfPowerSlider()->setValue(90);
        ta.updatePowerSliderLabels();
        QCOMPARE(ta.rfPowerLabel()->text(), QStringLiteral("0.0"));
    }

    // ── 6. Non-HL2 label is plain integer (no dB conversion) ───────────
    void nonHl2_label_is_int()
    {
        NereusSDR::RadioModel rm;
        NereusSDR::TxApplet ta(&rm);
        ta.rescalePowerSlidersForModel(HPSDRModel::ANAN100);
        ta.rfPowerSlider()->setValue(75);
        ta.updatePowerSliderLabels();
        QCOMPARE(ta.rfPowerLabel()->text(), QStringLiteral("75"));
    }

    // ── 7. Thetis-faithful tooltip on RF Power slider ──────────────────
    void tooltip_thetis_faithful()
    {
        NereusSDR::RadioModel rm;
        NereusSDR::TxApplet ta(&rm);
        ta.rescalePowerSlidersForModel(HPSDRModel::HERMESLITE);
        QVERIFY(ta.rfPowerSlider()->toolTip().contains("relative value"));
        QVERIFY(ta.rfPowerSlider()->toolTip().contains("MAX watts"));
    }
};

QTEST_MAIN(TestTxAppletHl2Slider)
#include "tst_tx_applet_hl2_slider.moc"
