// =================================================================
// tests/tst_extended_spectrum_reducer.cpp  (NereusSDR)
// =================================================================
//
// R3 Task 4b -- Core coverage for the local extended spectrum composition.
// These tests pin the clipped DDC island, the physical ADC wing geometry,
// calibration reference, detector/avenger ordering, and remote-input guards
// before media or GUI wiring is introduced.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-22  J.J. Boyd / KG4VCF  Remote daemon R3, Core extraction.
//                                    AI-assisted transformation via
//                                    OpenAI Codex.
// =================================================================

#include <QtTest>

#include <cmath>
#include <limits>

#include "core/FFTEngine.h"
#include "core/WidebandFftEngine.h"
#include "core/spectrum/ExtendedSpectrumReducer.h"
#include "core/spectrum/WidebandDisplayReference.h"

using namespace NereusSDR;

namespace {

ReducerConfig extendedConfig()
{
    ReducerConfig cfg;
    cfg.pixels = 100;
    cfg.centreHz = 500'000.0;
    cfg.spanHz = 2'000'000.0;
    cfg.streamCentreHz = 500'000.0;
    cfg.sampleRateHz = 1'000'000.0;
    cfg.detector = SpectrumDetectorMode::Peak;
    cfg.averageMode = 0;
    return cfg;
}

QVector<float> ddcBins(int count = 1024, float value = 1.0e-10f)
{
    return QVector<float>(count, value);
}

QVector<float> adcBins(float value = -140.0f)
{
    return QVector<float>(WidebandFftEngine::kOutputBins, value);
}

bool allNear(const QVector<float>& values, double expected, double tolerance = 0.01)
{
    for (float value : values) {
        if (std::abs(static_cast<double>(value) - expected) > tolerance) {
            return false;
        }
    }
    return true;
}

} // namespace

class TstExtendedSpectrumReducer : public QObject
{
    Q_OBJECT

private slots:
    void clippedIslandAndNoIsland()
    {
        const ReducerConfig cfg = extendedConfig();
        QCOMPARE(ExtendedSpectrumReducer::listenableIslandPixels(cfg),
                 std::make_pair(27, 72));

        ReducerConfig noIsland = cfg;
        noIsland.centreHz = 3'000'000.0;
        noIsland.spanHz = 1'000'000.0;
        QCOMPARE(ExtendedSpectrumReducer::listenableIslandPixels(noIsland),
                 std::make_pair(0, -1));
    }

    void ddcCarrierStaysInItsRfIsland()
    {
        ExtendedSpectrumReducer reducer;
        reducer.setConfig(extendedConfig());
        QVector<float> bins = ddcBins();
        bins[512] = 1.0f;
        QVector<float> out;
        QVERIFY(reducer.reduce(bins, 1.0, 0.0, {}, 4'000'000.0, 0.0,
                                -180.0, out));
        QCOMPARE(out.size(), 100);
        QVERIFY(out[50] > -0.1f);
        QCOMPARE(out[0], -180.0f);
        QCOMPARE(out[99], -180.0f);
    }

    void adcPeakAndReferenceArePreserved()
    {
        ReducerConfig cfg = extendedConfig();
        cfg.centreHz = 1'500'000.0; // no DDC island: ADC owns the whole row
        cfg.spanHz = 1'000'000.0;
        ExtendedSpectrumReducer reducer;
        reducer.setConfig(cfg);
        QVector<float> adc = adcBins();
        // kOutputBins is 32768. This peak maps to 1.5 MHz at a 4 MHz ADC rate.
        adc[24'576] = -80.0f;
        QVector<float> out;
        QVERIFY(reducer.reduce(ddcBins(), 1.0, 0.0, adc, 4'000'000.0, 0.0,
                                -180.0, out));
        const double reference = widebandRelativeReferenceDb(
            4'000'000.0, 1'000'000.0 / 1024.0, 1.0,
            SpectrumDetectorMode::Peak);
        QVERIFY(std::abs(static_cast<double>(out[50]) - (-80.0 + reference)) < 0.02);
    }

    void everyDetectorFamilyProducesAComposedRow()
    {
        QVector<float> bins = ddcBins();
        for (int i = 0; i < bins.size(); ++i) {
            bins[i] = static_cast<float>((i % 17) + 1) * 1.0e-10f;
        }
        for (SpectrumDetectorMode detector : {SpectrumDetectorMode::Peak,
                                               SpectrumDetectorMode::Rosenfell,
                                               SpectrumDetectorMode::Average,
                                               SpectrumDetectorMode::Sample,
                                               SpectrumDetectorMode::RMS}) {
            ReducerConfig cfg = extendedConfig();
            cfg.detector = detector;
            ExtendedSpectrumReducer reducer;
            reducer.setConfig(cfg);
            QVector<float> out;
            QVERIFY(reducer.reduce(bins, 1.5, 0.0, adcBins(), 4'000'000.0,
                                    0.0, -180.0, out));
            QCOMPARE(out.size(), cfg.pixels);
            for (float value : out) {
                QVERIFY(std::isfinite(value));
            }
        }
    }

    void stationScalarAppliesOnceToIslandAndWing()
    {
        const ReducerConfig cfg = extendedConfig();
        QVector<float> adc = adcBins();
        ExtendedSpectrumReducer zero;
        zero.setConfig(cfg);
        ExtendedSpectrumReducer shifted;
        shifted.setConfig(cfg);
        QVector<float> base;
        QVector<float> plusSeven;
        QVERIFY(zero.reduce(ddcBins(), 1.0, -20.0, adc, 4'000'000.0, 0.0,
                             -180.0, base));
        QVERIFY(shifted.reduce(ddcBins(), 1.0, -20.0, adc, 4'000'000.0, 7.0,
                                -180.0, plusSeven));
        QVERIFY(std::abs((plusSeven[50] - base[50]) - 7.0f) < 0.01f);
        QVERIFY(std::abs((plusSeven[80] - base[80]) - 7.0f) < 0.01f);
    }

    void traceAndWaterfallKeepSeparateAveraging()
    {
        ReducerConfig cfg = extendedConfig();
        cfg.averageMode = 1;
        cfg.averageAlpha = 0.5;
        ExtendedSpectrumReducer trace;
        ExtendedSpectrumReducer waterfall;
        trace.setConfig(cfg);
        waterfall.setConfig(cfg);
        QVector<float> traceFirst;
        QVector<float> waterfallFirst;
        QVERIFY(trace.reduce(ddcBins(), 1.0, 0.0, {}, 4'000'000.0, 0.0,
                             -180.0, traceFirst));
        QVERIFY(waterfall.reduce(ddcBins(), 1.0, 0.0, {}, 4'000'000.0, 0.0,
                                 -180.0, waterfallFirst));
        QVector<float> loud = ddcBins();
        loud[512] = 1.0f;
        QVector<float> traceSecond;
        QVector<float> waterfallSecond;
        QVERIFY(trace.reduce(loud, 1.0, 0.0, {}, 4'000'000.0, 0.0,
                             -180.0, traceSecond));
        QVERIFY(waterfall.reduce(ddcBins(), 1.0, 0.0, {}, 4'000'000.0, 0.0,
                                 -180.0, waterfallSecond));
        QVERIFY(traceSecond[50] > waterfallSecond[50] + 20.0f);
    }

    void recursiveAveragingIncludesChangingAdcRow()
    {
        ReducerConfig cfg = extendedConfig();
        cfg.centreHz = 1'500'000.0;
        cfg.spanHz = 1'000'000.0;
        cfg.averageMode = 1;
        cfg.averageAlpha = 0.5;
        ExtendedSpectrumReducer reducer;
        reducer.setConfig(cfg);
        QVector<float> quiet = adcBins(-120.0f);
        QVector<float> loud = adcBins(-80.0f);
        QVector<float> settled;
        QVector<float> second;
        // SpectrumAvenger seeds recursive-linear state at 1e-12. Settle that
        // documented seed against a physical raw-FFT noise row before testing
        // the next composed ADC row; otherwise the seed, not ADC composition,
        // determines the first comparison.
        for (int frame = 0; frame < 32; ++frame) {
            QVERIFY(reducer.reduce(ddcBins(), 1.0, 0.0, quiet, 4'000'000.0,
                                    0.0, -240.0, settled));
        }
        const double priorLinear = std::pow(10.0,
            static_cast<double>(settled[50]) / 10.0);
        QVERIFY(reducer.reduce(ddcBins(), 1.0, 0.0, loud, 4'000'000.0, 0.0,
                                -240.0, second));
        const double reference = widebandRelativeReferenceDb(
            4'000'000.0, 1'000'000.0 / 1024.0, 1.0,
            SpectrumDetectorMode::Peak);
        const double loudLinear = std::pow(10.0, (-80.0 + reference) / 10.0);
        const double expected = 10.0 * std::log10(0.5 * priorLinear
                                                   + 0.5 * loudLinear);
        QVERIFY(std::abs(static_cast<double>(second[50]) - expected) < 0.03);
    }

    void emptyAdcAndOutOfNyquistUseFinalFloor()
    {
        ReducerConfig cfg = extendedConfig();
        cfg.centreHz = 3'000'000.0;
        cfg.spanHz = 1'000'000.0;
        ExtendedSpectrumReducer reducer;
        reducer.setConfig(cfg);
        QVector<float> emptyOut;
        QVERIFY(reducer.reduce(ddcBins(), 1.0, 0.0, {}, 4'000'000.0, 0.0,
                                -180.0, emptyOut));
        QVERIFY(allNear(emptyOut, -180.0));

        cfg.centreHz = 2'500'000.0;
        cfg.spanHz = 2'000'000.0; // pixels at the right extend beyond Nyquist
        reducer.setConfig(cfg);
        QVector<float> out;
        QVERIFY(reducer.reduce(ddcBins(), 1.0, 0.0, adcBins(), 4'000'000.0,
                                0.0, -180.0, out));
        QCOMPARE(out.last(), -180.0f);
    }

    void malformedAndExtremeInputLeaveOutputUnchanged()
    {
        ExtendedSpectrumReducer reducer;
        reducer.setConfig(extendedConfig());
        QVector<float> out{-12.0f, -34.0f};
        const QVector<float> sentinel = out;

        QVector<float> nanPower = ddcBins();
        nanPower[0] = std::numeric_limits<float>::quiet_NaN();
        QVERIFY(!reducer.reduce(nanPower, 1.0, 0.0, {}, 4'000'000.0, 0.0,
                                 -180.0, out));
        QCOMPARE(out, sentinel);

        QVERIFY(!reducer.reduce(QVector<float>(FFTEngine::maximumFftSize() + 1,
                                                1.0e-10f),
                                 1.0, 0.0, {}, 4'000'000.0, 0.0, -180.0, out));
        QCOMPARE(out, sentinel);

        QVERIFY(!reducer.reduce(ddcBins(), 1.0, 0.0, QVector<float>(3, -100.0f),
                                 4'000'000.0, 0.0, -180.0, out));
        QCOMPARE(out, sentinel);

        QVERIFY(!reducer.reduce(ddcBins(), 1.0,
                                 std::numeric_limits<double>::max(), {},
                                 4'000'000.0, 0.0, -180.0, out));
        QCOMPARE(out, sentinel);

        QVector<float> underflowAdc = adcBins();
        underflowAdc.fill(-std::numeric_limits<float>::max());
        ReducerConfig wingOnly = extendedConfig();
        wingOnly.centreHz = 1'500'000.0;
        wingOnly.spanHz = 1'000'000.0;
        reducer.setConfig(wingOnly);
        QVERIFY(!reducer.reduce(ddcBins(), 1.0, 0.0, underflowAdc,
                                 4'000'000.0, 0.0, -180.0, out));
        QCOMPARE(out, sentinel);

        reducer.setConfig(extendedConfig());
        QVERIFY(!reducer.reduce(ddcBins(1024, 1.0e20f), 1.0, 0.0, {},
                                 4'000'000.0, 3000.0, 2900.0, out));
        QCOMPARE(out, sentinel);

        ReducerConfig overflowGeometry = extendedConfig();
        overflowGeometry.centreHz = std::numeric_limits<double>::max();
        overflowGeometry.spanHz = std::numeric_limits<double>::max();
        reducer.setConfig(overflowGeometry);
        QVERIFY(!reducer.reduce(ddcBins(), 1.0, 0.0, {}, 4'000'000.0, 0.0,
                                 -180.0, out));
        QCOMPARE(out, sentinel);

        ReducerConfig overflowDdcEdge = extendedConfig();
        overflowDdcEdge.streamCentreHz = std::numeric_limits<double>::max();
        overflowDdcEdge.sampleRateHz = std::numeric_limits<double>::max();
        reducer.setConfig(overflowDdcEdge);
        QVERIFY(!reducer.reduce(ddcBins(), 1.0, 0.0, {}, 4'000'000.0, 0.0,
                                 -180.0, out));
        QCOMPARE(out, sentinel);

        ReducerConfig extreme = extendedConfig();
        extreme.pixels = 4097;
        reducer.setConfig(extreme);
        QVERIFY(!reducer.reduce(ddcBins(), 1.0, 0.0, {}, 4'000'000.0, 0.0,
                                 -180.0, out));
        QCOMPARE(out, sentinel);
    }

    void rejectedScaleDoesNotAlterAveragingHistory()
    {
        ReducerConfig cfg = extendedConfig();
        cfg.averageMode = 1;
        cfg.averageAlpha = 0.5;
        ExtendedSpectrumReducer subject;
        ExtendedSpectrumReducer control;
        subject.setConfig(cfg);
        control.setConfig(cfg);
        QVector<float> actual;
        QVector<float> expected;
        const QVector<float> loud = ddcBins(1024, 1.0e30f);
        QVERIFY(subject.reduce(loud, 1.0, 0.0, {}, 4'000'000.0,
                               0.0, -180.0, actual));
        QVERIFY(control.reduce(loud, 1.0, 0.0, {}, 4'000'000.0,
                               0.0, -180.0, expected));
        const QVector<float> prior = actual;
        // This scale is finite and safe for the new quiet row, but would
        // overflow the retained loud row. Rejection must precede averaging.
        QVERIFY(!subject.reduce(ddcBins(), 1.0, 0.0, {}, 4'000'000.0,
                                3000.0, 2900.0, actual));
        QCOMPARE(actual, prior);
        QVERIFY(subject.reduce(ddcBins(), 1.0, 0.0, {}, 4'000'000.0,
                               0.0, -180.0, actual));
        QVERIFY(control.reduce(ddcBins(), 1.0, 0.0, {}, 4'000'000.0,
                               0.0, -180.0, expected));
        QCOMPARE(actual, expected);
    }
};

QTEST_MAIN(TstExtendedSpectrumReducer)
#include "tst_extended_spectrum_reducer.moc"
