// =================================================================
// tests/tst_wideband_fft_engine.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Phase 3F Sub-Epic F Task 4: WidebandFftEngine wraps an FFTW3
// 16384-point real-to-complex plan. Output: 8192 dBm-style bins
// (real-to-complex produces N/2+1 = 8193 bins; we drop DC to leave
// 8192 covering 0..(adcRateHz / 2) less the DC bin width).
// Bin width = adcRateHz / kFftSize.
// =================================================================
#include <QtTest/QtTest>
#include "core/WidebandFftEngine.h"

using namespace NereusSDR;

class TestWidebandFftEngine : public QObject {
    Q_OBJECT
private slots:
    void fft_produces_8192_bins_for_16384_real_samples()
    {
        WidebandFftEngine engine;
        engine.setAdcSampleRateHz(122880000.0);

        QVector<float> samples(16384, 0.5f);
        QVector<float> bins;
        engine.computeFft(samples, bins);

        // Real-to-complex output is N/2 + 1 = 8193; we drop DC, so 8192.
        QCOMPARE(bins.size(), 8192);
    }

    void bin_width_is_7p5_kHz_for_122mhz_adc()
    {
        WidebandFftEngine engine;
        engine.setAdcSampleRateHz(122880000.0);
        // 122,880,000 / 16384 = 7500 Hz
        const double binWidth = engine.binWidthHz();
        QVERIFY(qAbs(binWidth - 7500.0) < 1.0);
    }
};

QTEST_MAIN(TestWidebandFftEngine)
#include "tst_wideband_fft_engine.moc"
