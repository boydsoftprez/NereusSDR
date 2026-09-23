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
    // The engine consumes one capture (16384 samples, all the FPGA's FIFO
    // holds) and zero-pads it to a longer transform, so the bin COUNT is set
    // by the padded length, not the capture length.
    void fft_output_bin_count_follows_the_padded_transform()
    {
        WidebandFftEngine engine;
        engine.setAdcSampleRateHz(122880000.0);

        QVector<float> samples(WidebandFftEngine::kCaptureSamples, 0.5f);
        QVector<float> bins;
        engine.computeFft(samples, bins);

        // Real-to-complex gives N/2 + 1; DC is dropped, leaving N/2.
        QCOMPARE(bins.size(), WidebandFftEngine::kFftSize / 2);
    }

    // A short or long block is a caller bug, not something to pad silently:
    // the window is built for exactly one length.
    void wrong_capture_length_is_rejected()
    {
        WidebandFftEngine engine;
        QVector<float> bins;

        QVector<float> tooShort(WidebandFftEngine::kCaptureSamples - 1, 0.5f);
        engine.computeFft(tooShort, bins);
        QVERIFY2(bins.isEmpty(), "short block should have been refused");

        QVector<float> tooLong(WidebandFftEngine::kCaptureSamples + 1, 0.5f);
        engine.computeFft(tooLong, bins);
        QVERIFY2(bins.isEmpty(), "long block should have been refused");
    }

    // Bin SPACING is the padded figure. This is where a bin sits, and it is
    // deliberately finer than what the engine can resolve.
    void bin_spacing_is_the_padded_figure()
    {
        WidebandFftEngine engine;
        engine.setAdcSampleRateHz(122880000.0);
        // 122,880,000 / 65536 = 1875 Hz
        const double expected = 122880000.0 / WidebandFftEngine::kFftSize;
        QVERIFY(qAbs(engine.binSpacingHz() - expected) < 1.0);
        QVERIFY(qAbs(engine.binWidthHz() - expected) < 1.0);
    }

    // TRUE resolution is set by the contiguous capture, and no amount of
    // padding changes it. 16384 samples at 122.88 MHz is 133 us of signal,
    // so 1/133us = 7.5 kHz, times the window's ENB. The gateware fixes the
    // capture at 16k (n1gp-Anvelina_PROIII Orion.v:1503 [@8e86a61]), which is
    // why this number cannot be improved from the client side.
    //
    // Guards against anyone reading the 1875 Hz spacing as a resolution
    // claim: the noise bandwidth must stay several times larger.
    void noise_bandwidth_reflects_the_capture_not_the_padding()
    {
        WidebandFftEngine engine;
        engine.setAdcSampleRateHz(122880000.0);

        const double rayleigh =
            122880000.0 / WidebandFftEngine::kCaptureSamples;   // 7500 Hz
        QVERIFY2(qAbs(rayleigh - 7500.0) < 1.0,
                 "capture-length resolution is no longer 7.5 kHz");

        // Blackman-Harris 7-term has an ENB comfortably above 1 bin, so the
        // noise bandwidth exceeds the Rayleigh figure.
        QVERIFY(engine.noiseBandwidthHz() > rayleigh);
        QVERIFY2(engine.noiseBandwidthHz() > 4.0 * engine.binSpacingHz(),
                 "noise bandwidth collapsed toward the padded bin spacing — "
                 "padding is being mistaken for resolution");
    }

    // Windowing costs coherent gain; the published sum is what the display
    // side references amplitudes against, so it must reflect the real window
    // rather than a bare sample count.
    void window_sum_is_below_an_unwindowed_block()
    {
        const double sum = WidebandFftEngine::windowSum();
        QVERIFY(sum > 0.0);
        QVERIFY2(sum < double(WidebandFftEngine::kCaptureSamples),
                 "window sum equals the sample count — no window is applied");
        QVERIFY(WidebandFftEngine::windowEnbBins() > 1.0);
    }
};

QTEST_MAIN(TestWidebandFftEngine)
#include "tst_wideband_fft_engine.moc"
