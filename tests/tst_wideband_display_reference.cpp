// =================================================================
// tests/tst_wideband_display_reference.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original regression coverage for the shared
// local/remote wing reference. No antenna-calibration claim is made.
// 2026-09-22 J.J. Boyd / KG4VCF, AI-assisted via OpenAI Codex.

#include <QtTest/QtTest>

#include "core/WidebandFftEngine.h"
#include "core/spectrum/WidebandDisplayReference.h"

#include <cmath>
#include <numbers>

using namespace NereusSDR;

class TestWidebandDisplayReference : public QObject {
    Q_OBJECT

private slots:
    void actual_windowed_real_fft_preserves_known_tone_level_data()
    {
        QTest::addColumn<float>("amplitude");
        QTest::addColumn<float>("expectedDbfs");
        QTest::newRow("full scale") << 1.0f << 0.0f;
        QTest::newRow("quarter scale") << 0.25f << -12.0412f;
    }

    void actual_windowed_real_fft_preserves_known_tone_level()
    {
        QFETCH(float, amplitude);
        QFETCH(float, expectedDbfs);
        WidebandFftEngine engine;
        constexpr int captureBin = 128;
        constexpr int fftBin = captureBin * WidebandFftEngine::kFftSize
            / WidebandFftEngine::kCaptureSamples;
        QVector<float> samples(WidebandFftEngine::kCaptureSamples);
        for (int i = 0; i < samples.size(); ++i) {
            samples[i] = amplitude * static_cast<float>(std::sin(
                2.0 * std::numbers::pi * captureBin * i / samples.size()));
        }
        QVector<float> bins;
        engine.computeFft(samples, bins);
        QCOMPARE(bins.size(), WidebandFftEngine::kOutputBins);
        // The positive-frequency output drops DC. This fixture catches a
        // wrong real/complex factor, omitted Hann gain or wrong FFT index.
        const float dbfs = bins.at(fftBin - 1) + widebandFftNormalisationDb();
        QVERIFY2(std::abs(dbfs - expectedDbfs) < 0.005f,
                 qPrintable(QStringLiteral("tone was %1 dBFS, expected %2")
                                .arg(dbfs).arg(expectedDbfs)));
    }

    void detector_reference_tracks_independent_adc_and_ddc_rates()
    {
        constexpr double adcRateHz = 122880000.0;
        constexpr double ddcBinHz = 192000.0 / 4096;
        constexpr double ddcEnb = 1.5;
        const float peak = widebandBandwidthNormalisationDb(
            adcRateHz, ddcBinHz, ddcEnb, SpectrumDetectorMode::Peak);
        const float average = widebandBandwidthNormalisationDb(
            adcRateHz, ddcBinHz, ddcEnb, SpectrumDetectorMode::Average);
        QVERIFY(std::abs((peak - average) - 1.76091f) < 0.001f);
        QCOMPARE(widebandBandwidthNormalisationDb(
                     adcRateHz, ddcBinHz, ddcEnb, SpectrumDetectorMode::Rosenfell), peak);
        QCOMPARE(widebandBandwidthNormalisationDb(
                     adcRateHz, ddcBinHz, ddcEnb, SpectrumDetectorMode::Sample), average);
        QCOMPARE(widebandBandwidthNormalisationDb(
                     adcRateHz, ddcBinHz, ddcEnb, SpectrumDetectorMode::RMS), average);

        for (int value = 0; value < static_cast<int>(SpectrumDetectorMode::Count); ++value) {
            const SpectrumDetectorMode detector = static_cast<SpectrumDetectorMode>(value);
            const float initial = widebandRelativeReferenceDb(
                adcRateHz, ddcBinHz, ddcEnb, detector);
            const float doubleDdc = widebandRelativeReferenceDb(
                adcRateHz, ddcBinHz * 2, ddcEnb, detector);
            const float doubleAdc = widebandRelativeReferenceDb(
                adcRateHz * 2, ddcBinHz, ddcEnb, detector);
            QVERIFY(std::abs((doubleDdc - initial) - 3.01030f) < 0.001f);
            QVERIFY(std::abs((doubleAdc - initial) + 3.01030f) < 0.001f);
        }
    }
};

QTEST_GUILESS_MAIN(TestWidebandDisplayReference)
#include "tst_wideband_display_reference.moc"
