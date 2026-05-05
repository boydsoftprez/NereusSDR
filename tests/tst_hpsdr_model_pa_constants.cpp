// no-port-check: test fixture; cite comments reference upstream setup.cs /
// setup.designer.cs as documentation of the constants under test (no logic
// is ported here, only constant values are asserted).
//
// Per-SKU PA constants assertion suite for mi0bot-Thetis HL2 parity.
// Spec: docs/architecture/2026-05-04-hl2-tx-mi0bot-parity-design.md §5.1

#include <QtTest/QtTest>
#include "core/HpsdrModel.h"

using NereusSDR::HPSDRModel;

class TestHpsdrModelPaConstants : public QObject {
    Q_OBJECT
private slots:
    void hl2_rfPowerSlider() {
        QCOMPARE(NereusSDR::rfPowerSliderMaxFor(HPSDRModel::HERMESLITE),  90);
        QCOMPARE(NereusSDR::rfPowerSliderStepFor(HPSDRModel::HERMESLITE),  6);
    }
    void hl2_tuneSlider() {
        QCOMPARE(NereusSDR::tuneSliderMaxFor(HPSDRModel::HERMESLITE),  99);
        QCOMPARE(NereusSDR::tuneSliderStepFor(HPSDRModel::HERMESLITE),  3);
    }
    void hl2_fixedTuneSpinbox() {
        QCOMPARE(NereusSDR::fixedTuneSpinboxMinFor(HPSDRModel::HERMESLITE), -16.5f);
        QCOMPARE(NereusSDR::fixedTuneSpinboxMaxFor(HPSDRModel::HERMESLITE),   0.0f);
        QCOMPARE(NereusSDR::fixedTuneSpinboxStepFor(HPSDRModel::HERMESLITE), 0.5f);
        QCOMPARE(NereusSDR::fixedTuneSpinboxDecimalsFor(HPSDRModel::HERMESLITE), 1);
        QCOMPARE(QString(NereusSDR::fixedTuneSpinboxSuffixFor(HPSDRModel::HERMESLITE)),
                 QStringLiteral(" dB"));
    }
    void hl2_attenuator() {
        QCOMPARE(NereusSDR::hl2AttenuatorDbPerStep(),  0.5f);
        QCOMPARE(NereusSDR::hl2AttenuatorStepCount(), 16);
    }
    void anan100_default_100W() {
        // NOTE: enum is ANAN100 (no underscore), not ANAN_100
        QCOMPARE(NereusSDR::rfPowerSliderMaxFor(HPSDRModel::ANAN100), 100);
        QCOMPARE(NereusSDR::rfPowerSliderStepFor(HPSDRModel::ANAN100), 1);
        QCOMPARE(NereusSDR::tuneSliderMaxFor(HPSDRModel::ANAN100),    100);
        QCOMPARE(NereusSDR::tuneSliderStepFor(HPSDRModel::ANAN100),     1);
        QCOMPARE(NereusSDR::fixedTuneSpinboxMinFor(HPSDRModel::ANAN100),  0.0f);
        QCOMPARE(NereusSDR::fixedTuneSpinboxMaxFor(HPSDRModel::ANAN100), 100.0f);
        QCOMPARE(QString(NereusSDR::fixedTuneSpinboxSuffixFor(HPSDRModel::ANAN100)),
                 QStringLiteral(" W"));
    }
    void ananG2_1k_default_100W_slider() {
        // Slider range stays 0-100 on every SKU - the per-band PA gain table
        // does the actual scaling. mi0bot only deviates for HERMESLITE.
        QCOMPARE(NereusSDR::rfPowerSliderMaxFor(HPSDRModel::ANAN_G2_1K), 100);
        QCOMPARE(NereusSDR::tuneSliderMaxFor(HPSDRModel::ANAN_G2_1K),    100);
    }
};

QTEST_MAIN(TestHpsdrModelPaConstants)
#include "tst_hpsdr_model_pa_constants.moc"
