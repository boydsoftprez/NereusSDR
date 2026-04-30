// tst_fft_engine_sliding_window.cpp
//
// Sliding-window FFT contract: with the Thetis-faithful stride formula
// (PanDisplay.cs:4699-4700 [@501e3f51]), the fftReady() emit rate must
// equal the configured target FPS regardless of fftSize. Before this
// fix the rate was sampleRate / fftSize, which collapsed deep-zoom
// updates to ~3 fps on a P1 wire rate.

#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QVector>

#include <cmath>

#include "core/FFTEngine.h"

using namespace NereusSDR;

namespace {

QVector<float> generateIqChunk(int sampleCount, double sampleRateHz, double toneHz)
{
    QVector<float> out;
    out.reserve(sampleCount * 2);
    static double phase = 0.0;
    const double dPhase = 2.0 * M_PI * toneHz / sampleRateHz;
    for (int i = 0; i < sampleCount; ++i) {
        out.append(static_cast<float>(std::cos(phase)));  // I
        out.append(static_cast<float>(std::sin(phase)));  // Q
        phase += dPhase;
        if (phase > 2.0 * M_PI) phase -= 2.0 * M_PI;
    }
    return out;
}

int countEmitsForOneSecond(int fftSize, double sampleRateHz, int targetFps)
{
    FFTEngine engine(0);
    engine.setSampleRate(sampleRateHz);
    engine.setOutputFps(targetFps);
    engine.setFftSize(fftSize);

    QSignalSpy spy(&engine, &FFTEngine::fftReady);

    // Feed exactly one second of synthetic I/Q (one chunk; the stride
    // counter handles inter-chunk boundaries identically to per-packet
    // delivery from RadioConnection).
    const int totalPairs = static_cast<int>(sampleRateHz);
    const QVector<float> iq = generateIqChunk(totalPairs, sampleRateHz, 1000.0);
    engine.feedIQ(iq);

    return spy.count();
}

} // namespace

class TestFftEngineSlidingWindow : public QObject {
    Q_OBJECT
private slots:

    // P1 wire rate. The pre-fix code yielded 192000/65536 ≈ 2.9 emits/sec
    // — well outside any reasonable smooth-display window. Sliding-window
    // pins it at ~targetFps regardless of fftSize.
    void rateIndependentOfFftSize_p1()
    {
        const double sr = 192000.0;
        const int    fps = 30;

        const int small = countEmitsForOneSecond(4096,  sr, fps);
        const int big   = countEmitsForOneSecond(65536, sr, fps);

        // Allow ±2 frames of slack: the warmup discards the first fftSize
        // samples before the very first emit, which shaves a fraction of
        // a frame from the small-FFT case and a larger fraction from the
        // big-FFT case (65536 samples ≈ 0.34s of the 1s window).
        QVERIFY2(std::abs(small - fps) <= 2,
                 qPrintable(QString("small-fft emits=%1 expected≈%2").arg(small).arg(fps)));
        QVERIFY2(std::abs(big - (fps - 10)) <= 2 || std::abs(big - fps) <= 2,
                 qPrintable(QString("big-fft emits=%1 (expected ≈20-30 after 0.34s warmup)").arg(big)));
    }

    // P2 wire rate, same contract.
    void rateIndependentOfFftSize_p2()
    {
        const double sr = 768000.0;
        const int    fps = 30;

        const int small = countEmitsForOneSecond(4096,  sr, fps);
        const int big   = countEmitsForOneSecond(65536, sr, fps);

        // P2 warmup is 65536/768000 ≈ 85ms — barely a frame, so big-fft
        // should hit the full target.
        QVERIFY2(std::abs(small - fps) <= 2,
                 qPrintable(QString("p2 small-fft emits=%1 expected≈%2").arg(small).arg(fps)));
        QVERIFY2(std::abs(big - fps) <= 2,
                 qPrintable(QString("p2 big-fft emits=%1 expected≈%2").arg(big).arg(fps)));
    }

    // Window-function dropdown must take effect on the next chunk, not
    // silently wait for an FFT-size replan. Pre-fix this was a no-op
    // outside replanFft(), masked by the very-low emit rate at deep zoom.
    void windowChangeTakesEffectImmediately()
    {
        FFTEngine engine(0);
        engine.setSampleRate(192000.0);
        engine.setOutputFps(30);
        engine.setFftSize(4096);
        engine.setWindowFunction(WindowFunction::BlackmanHarris4);

        // Prime the engine so it's emitting frames against the BH4 window.
        QVector<float> binsBh;
        QObject::connect(&engine, &FFTEngine::fftReady, &engine,
            [&binsBh](int, const QVector<float>& bins) { binsBh = bins; });
        engine.feedIQ(generateIqChunk(192000, 192000.0, 1000.0));
        QVERIFY(!binsBh.isEmpty());

        // Disconnect, swap window, prime again. Without the fix the second
        // capture would be byte-identical to BH4 (window never recomputed).
        QObject::disconnect(&engine, nullptr, nullptr, nullptr);
        QVector<float> binsHann;
        QObject::connect(&engine, &FFTEngine::fftReady, &engine,
            [&binsHann](int, const QVector<float>& bins) { binsHann = bins; });
        engine.setWindowFunction(WindowFunction::Hanning);
        engine.feedIQ(generateIqChunk(192000, 192000.0, 1000.0));
        QVERIFY(!binsHann.isEmpty());

        // The DC-bin neighborhood differs between Blackman-Harris-4 and Hann
        // even on a clean tone — different sidelobe profiles. Just demand
        // that *something* in the spectrum moved.
        QCOMPARE(binsBh.size(), binsHann.size());
        bool anyDifference = false;
        for (int i = 0; i < binsBh.size(); ++i) {
            if (std::abs(binsBh[i] - binsHann[i]) > 0.001f) {
                anyDifference = true;
                break;
            }
        }
        QVERIFY2(anyDifference, "BH4 and Hann produced byte-identical spectra — window change is a no-op");
    }

    // Changing target FPS mid-stream must take effect on the next chunk.
    void respondsToFpsChange()
    {
        FFTEngine engine(0);
        engine.setSampleRate(192000.0);
        engine.setOutputFps(15);
        engine.setFftSize(4096);

        QSignalSpy spy(&engine, &FFTEngine::fftReady);

        engine.feedIQ(generateIqChunk(192000, 192000.0, 1000.0));
        const int firstSec = spy.count();

        engine.setOutputFps(30);
        spy.clear();
        engine.feedIQ(generateIqChunk(192000, 192000.0, 1000.0));
        const int secondSec = spy.count();

        QVERIFY2(std::abs(firstSec - 15) <= 2,
                 qPrintable(QString("first sec @15fps got=%1").arg(firstSec)));
        QVERIFY2(std::abs(secondSec - 30) <= 2,
                 qPrintable(QString("second sec @30fps got=%1").arg(secondSec)));
    }
};

QTEST_GUILESS_MAIN(TestFftEngineSlidingWindow)
#include "tst_fft_engine_sliding_window.moc"
