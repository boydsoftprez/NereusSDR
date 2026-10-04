// no-port-check: NereusSDR-original test. Cites Thetis and WDSP lines as the
// oracle for the analyzer set-up and the TUNE tone; no upstream source is
// reproduced here.
//
// tst_tx_analyzer_skirt.cpp
//
// Row 15 of docs/architecture/tx-display-verification/README.md: the
// transmit display showed a skirt about 35 dB down 66 Hz from a TUNE tone's
// peak with Blackman-Harris 4T selected, where the window's sidelobes are
// more than 90 dB down. The skirt was already in WDSP's raw GetPixels output.
//
// Two cases, both with the arguments TxAnalyzer::currentArgs() gives WDSP at
// TUNE (the MOX edge's +/-4 kHz window, 1200 pixels, the TX channel's DSP
// block size, 96 kHz, 15 frames a second) and window 1 (Blackman-Harris 4T):
//
//  1. A synthetic tone fed straight into Spectrum0, as the TX siphon does
//     (wdsp/siphon.c: Spectrum0(1, disp, 0, 0, in)), in blocks of the TX
//     channel's dsp_size. The analyzer is created at display 5 with
//     Thetis's arguments (ChannelMaster/cmaster.c:192-198 [v2.10.3.15]:
//     XCreateAnalyzer(in_id, &rc, 262144, 1, 1, "")).
//
//  2. A real TX channel on the transmit lane (no radio), the TUNE generator
//     set as RadioModel's TUNE path sets it (setTuneTone(true, -600,
//     kMaxToneMag), Thetis console.cs:30031-30040 [v2.10.3.15] in LSB), and
//     64-frame blocks pumped from a worker thread the way TxWorkerThread
//     pumps them. The samples the siphon handed the analyzer are read back
//     out of its input ring and every block must be there once, in order.
//
// The synthetic tones are +/-600 Hz to verify the carrier-relative axis
// for USB and LSB; the TX channel tone is -600 Hz (LSB). Both use 0.99999,
// Thetis's MAX_TONE_MAG. The pixel 66 Hz either side of the peak, and every pixel
// beyond it out to the window's edge, must be at least 90 dB down.
//
// Modification history (NereusSDR):
//   2026-10-04 — J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//                 Verify both USB and LSB analyzer frequency orientation.

#include <QtTest/QtTest>
#include "RealtimeTestLoad.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <complex>
#include <functional>
#include <memory>
#include <numbers>
#include <thread>
#include <vector>

#include "core/AppSettings.h"
#include "core/DspControlThread.h"
#include "core/RadioConnection.h"
#include "core/TxAnalyzer.h"
#include "core/TxChannel.h"
#include "core/WdspEngine.h"
#include "core/wdsp_api.h"

// WDSP's analyzer table and its per-display state (analyzer.h), read to
// count what the siphon pushed.
extern "C" {
#include "../third_party/wdsp/src/comm.h"
}
// linux_port.h defines min/max as macros; this file uses std::min/std::max.
#undef min
#undef max

using namespace NereusSDR;

namespace {

constexpr int kDisp = TxAnalyzer::kTxDispId;
constexpr int kTxId = WdspEngine::kTxChannelId;
constexpr int kInRate = 48000;
constexpr int kTxInSize = 64;                 // TxWorkerThread::kBlockFrames
constexpr double kDspRate = 96000.0;          // WdspEngine::kTxDspSampleRate
constexpr double kToneHz = -600.0;            // -cw_pitch, LSB
constexpr int kPixels = 1200;
constexpr int kWindowBh4 = 1;                 // Blackman-Harris 4T
constexpr int kTxWindowLowHz = -4000;         // MainWindow's MOX-edge window
constexpr int kTxWindowHighHz = +4000;
constexpr double kSkirtOffsetHz = 66.0;
constexpr double kRequiredDownDb = 90.0;
// The first SetAnalyzer in a process plans a 32768-point FFTW_PATIENT
// transform with no wisdom.
constexpr int kLaneIdleTimeoutMs = 600000;

// The analyzer's frequency per output pixel for these arguments
// (wdsp/analyzer.c SetAnalyzer: bin_per_pix).
double hzPerPixel(const TxAnalyzerArgs& a)
{
    const double binWidth = a.sampleRateHz / static_cast<double>(a.sz);
    const double binPerPix =
        (static_cast<double>(a.nStch * (a.sz - 1 - 2 * a.clp)) - 1.0 - a.fscLin - a.fscHin)
        / (static_cast<double>(a.nPix) - 1.0);
    return binPerPix * binWidth;
}

struct Skirt {
    int peakIndex{-1};
    double peakDb{0.0};
    double at66LowDb{0.0};    // pixel 66 Hz below the peak, relative to it
    double at66HighDb{0.0};   // pixel 66 Hz above the peak, relative to it
    double worstBeyondDb{0.0};  // highest pixel at or beyond 66 Hz, relative
    double peakHz{0.0};
};

Skirt measure(const std::vector<float>& pix, const TxAnalyzerArgs& args)
{
    Skirt s;
    const auto it = std::max_element(pix.begin(), pix.end());
    s.peakIndex = static_cast<int>(it - pix.begin());
    s.peakDb = *it;
    const double hzPix = hzPerPixel(args);
    s.peakHz = kTxWindowLowHz + s.peakIndex * hzPix;
    const int off = static_cast<int>(std::lround(kSkirtOffsetHz / hzPix));
    const int n = static_cast<int>(pix.size());
    const int lo = s.peakIndex - off;
    const int hi = s.peakIndex + off;
    s.at66LowDb = lo >= 0 ? pix[static_cast<std::size_t>(lo)] - s.peakDb : -999.0;
    s.at66HighDb = hi < n ? pix[static_cast<std::size_t>(hi)] - s.peakDb : -999.0;
    s.worstBeyondDb = -999.0;
    for (int i = 0; i < n; ++i) {
        if (std::abs(i - s.peakIndex) >= off) {
            s.worstBeyondDb = std::max(s.worstBeyondDb,
                                       static_cast<double>(pix[static_cast<std::size_t>(i)])
                                           - s.peakDb);
        }
    }
    return s;
}

QByteArray describe(const Skirt& s)
{
    return QStringLiteral("peak %1 dB at pixel %2 (%3 Hz); 66 Hz below %4 dB, above %5 dB; "
                          "worst at or beyond 66 Hz %6 dB")
        .arg(s.peakDb, 0, 'f', 1)
        .arg(s.peakIndex)
        .arg(s.peakHz, 0, 'f', 0)
        .arg(s.at66LowDb, 0, 'f', 1)
        .arg(s.at66HighDb, 0, 'f', 1)
        .arg(s.worstBeyondDb, 0, 'f', 1)
        .toUtf8();
}

// The latest pan frame (pixout 0) the analyzer has, after it has produced
// `frames` new ones.
bool grabFrame(int disp, int nPix, int frames, std::vector<float>& out,
               const std::function<void()>& keepFeeding)
{
    out.assign(static_cast<std::size_t>(nPix), 0.0f);
    int got = 0;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
    while (got < frames && std::chrono::steady_clock::now() < deadline) {
        keepFeeding();
        int flag = 0;
        GetPixels(disp, /*pixout=*/0, out.data(), &flag);
        if (flag != 0) {
            ++got;
        }
    }
    return got >= frames;
}

class MockConnection : public RadioConnection {
    Q_OBJECT
public:
    explicit MockConnection(QObject* parent = nullptr) : RadioConnection(parent)
    {
        setState(ConnectionState::Connected);
    }
    void init() override {}
    void connectToRadio(const NereusSDR::RadioInfo&) override {}
    void disconnect() override {}
    void setReceiverFrequency(int, quint64) override {}
    void setTxFrequency(quint64) override {}
    void setActiveReceiverCount(int) override {}
    void setSampleRate(int) override {}
    void setAttenuator(int) override {}
    void setPreamp(bool) override {}
    void setTxDrive(int) override {}
    void sendTxIq(const float*, int) override {}
    void setWatchdogEnabled(bool) override {}
    void setAntennaRouting(AntennaRouting) override {}
    void setMox(bool) override {}
    void setTrxRelay(bool) override {}
    void setMicBoost(bool) override {}
    void setLineIn(bool) override {}
    void setMicTipRing(bool) override {}
    void setMicBias(bool) override {}
    void setLineInGain(int) override {}
    void setUserDigOut(quint8) override {}
    void setPuresignalRun(bool) override {}
    void setMicPTTDisabled(bool) override {}
    void setMicXlr(bool) override {}
};

// The analyzer as the MOX edge configures it at TUNE, with BH-4T selected.
void configureForTune(TxAnalyzer& a, int blockFrames)
{
    a.applyStationRates();
    a.setWindowType(kWindowBh4);
    a.setBlockSize(blockFrames);
    a.setSpectrumWindow(kTxWindowLowHz, kTxWindowHighHz);
    a.setNumPixels(kPixels);
    a.start();   // the first start() applies SetAnalyzer (plans the FFT)
    a.stop();    // the test reads GetPixels itself
}

} // namespace

class TestTxAnalyzerSkirt : public QObject {
    Q_OBJECT

private slots:
    void init() { AppSettings::instance().clear(); }
    void cleanup() { RealtimeTestLoad::printLoadAverageIfFailed(); }

    // Case 1: a continuous synthetic tone straight into Spectrum0.
    void syntheticToneHasNoSkirt_data()
    {
        QTest::addColumn<double>("toneHz");
        QTest::newRow("USB above carrier") << 600.0;
        QTest::newRow("LSB below carrier") << -600.0;
    }

    void syntheticToneHasNoSkirt()
    {
        QFETCH(double, toneHz);
        const int block = WdspEngine::kTxDspBufferSize;   // the TX channel's dsp_size
        TxAnalyzer analyzer(kDisp);
        configureForTune(analyzer, block);
        const TxAnalyzerArgs args = analyzer.currentArgs();
        QCOMPARE(args.winType, kWindowBh4);
        QCOMPARE(args.bfSz, block);
        QCOMPARE(args.nPix, kPixels);

        // The TUNE generator's output (wdsp/gen.c xgen mode 0):
        //   out[2i] = +mag*cos(phs), out[2i+1] = -mag*sin(phs)
        std::vector<double> buf(static_cast<std::size_t>(2 * block));
        double phs = 0.0;
        const double delta = 2.0 * std::numbers::pi * toneHz / kDspRate;
        const auto feedOne = [&]() {
            for (int i = 0; i < block; ++i) {
                buf[static_cast<std::size_t>(2 * i)] = TxChannel::kMaxToneMag * std::cos(phs);
                buf[static_cast<std::size_t>(2 * i + 1)] = -TxChannel::kMaxToneMag * std::sin(phs);
                phs = std::fmod(phs + delta + 2.0 * std::numbers::pi, 2.0 * std::numbers::pi);
            }
            Spectrum0(1, kDisp, 0, 0, buf.data());
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        };

        std::vector<float> pix;
        QVERIFY(grabFrame(kDisp, args.nPix, 5, pix, feedOne));
        const Skirt s = measure(pix, args);
        qInfo("synthetic: %s", describe(s).constData());
        QVERIFY2(std::abs(s.peakHz - toneHz) < 3.0 * hzPerPixel(args), describe(s).constData());
        QVERIFY2(s.at66LowDb <= -kRequiredDownDb && s.at66HighDb <= -kRequiredDownDb,
                 describe(s).constData());
        QVERIFY2(s.worstBeyondDb <= -kRequiredDownDb, describe(s).constData());
    }

    // Case 2: through a real TX channel, TUNE generator on, blocks pumped
    // as TxWorkerThread pumps them.
    void tuneThroughTxChannelHasNoSkirt()
    {
        DspControlThread lane(DspLane::Transmit);
        lane.start();
        WdspEngine engine;
        engine.m_initialized = true;   // friend access (NEREUS_BUILD_TESTS)
        engine.setTransmitLane(&lane);
        TxChannel* tx = engine.createTxChannel(kTxId, kTxInSize, WdspEngine::kTxDspBufferSize,
                                               kInRate, WdspEngine::kTxDspSampleRate, kInRate);
        QVERIFY(tx);
        QVERIFY(lane.waitIdleForTest(kLaneIdleTimeoutMs));
        MockConnection conn;
        tx->setConnection(&conn);

        const int block = tx->dspBlockFrames();
        QCOMPARE(block, WdspEngine::kTxDspBufferSize);
        auto analyzer = std::make_unique<TxAnalyzer>(kDisp, nullptr, &lane);
        configureForTune(*analyzer, block);
        QVERIFY(lane.waitIdleForTest(kLaneIdleTimeoutMs));
        const TxAnalyzerArgs args = analyzer->currentArgs();
        QCOMPARE(args.bfSz, block);   // bf_sz equals the TX channel's dsp_size

        // RadioModel's TUNE path: setTuneTone(true, -cw_pitch, kMaxToneMag)
        // for an LSB slice, then the channel on.
        tx->setTuneTone(true, kToneHz, TxChannel::kMaxToneMag);
        tx->setRunningAsync(true);
        QTRY_VERIFY_WITH_TIMEOUT(tx->isRunning(), kLaneIdleTimeoutMs);
        QVERIFY(lane.waitIdleForTest(kLaneIdleTimeoutMs));

        auto* a = static_cast<DP>(pdisp[kDisp]);
        QVERIFY(a != nullptr);
        const int inStart = a->IQin_index[0][0];

        // TxWorkerThread's pump: a zero microphone block, DEXP, fexchange0.
        std::vector<double> mic(static_cast<std::size_t>(2 * kTxInSize), 0.0);
        std::atomic<long> calls{0};
        const auto pumpOne = [&]() {
            tx->pumpDexp(mic.data());
            tx->driveOneTxBlockFromInterleaved(mic.data());
            calls.fetch_add(1);
        };

        std::vector<float> pix;
        QVERIFY(grabFrame(kDisp, args.nPix, 5, pix, [&]() {
            for (int i = 0; i < 16; ++i) {
                pumpOne();
            }
        }));
        const Skirt s = measure(pix, args);
        qInfo("through the TX channel: %s", describe(s).constData());

        // Continuity: the samples the siphon handed the analyzer, read back
        // from its input ring, are one unbroken tone. A skipped or repeated
        // block breaks the phase step at its edge.
        const int inEnd = a->IQin_index[0][0];
        const int pushed = ((inEnd - inStart + a->bsize) % a->bsize) / a->buff_size;
        const long dspBlocksFed = calls.load() * kTxInSize * 2 / block;
        qInfo("fexchange0 calls %ld, DSP blocks fed %ld, siphon pushes %d",
              calls.load(), dspBlocksFed, pushed);
        QVERIFY2(pushed >= dspBlocksFed - 2 && pushed <= dspBlocksFed,
                 "every DSP block must reach the siphon once");

        const std::complex<double> step =
            std::polar(1.0, -2.0 * std::numbers::pi * kToneHz / kDspRate);
        int breaks = 0;
        int firstBreak = -1;
        int checked = 0;
        std::complex<double> prev;
        bool havePrev = false;
        for (int k = 0; k < pushed * a->buff_size; ++k) {
            const int idx = (inStart + k) % a->bsize;
            // Spectrum0 stores I = in[2i+1], Q = in[2i] (analyzer.c:1741-1742).
            const std::complex<double> z(a->Q_samples[0][0][idx], a->I_samples[0][0][idx]);
            if (std::abs(z) < 0.5) {   // the up-slew ramp at key-up
                havePrev = false;
                continue;
            }
            if (havePrev) {
                // z_n = mag * exp(-j * phs_n); the step between samples is
                // exp(-j * delta).
                const double err = std::abs(std::arg((z / prev) / step));
                ++checked;
                if (err > 1e-3) {
                    if (firstBreak < 0) {
                        firstBreak = k;
                    }
                    ++breaks;
                }
            }
            prev = z;
            havePrev = true;
        }
        qInfo("continuity: %d sample steps checked, %d breaks, first at sample %d "
              "(block %d)", checked, breaks, firstBreak,
              firstBreak < 0 ? -1 : firstBreak / a->buff_size);

        tx->setTuneTone(false, kToneHz, TxChannel::kMaxToneMag);
        tx->setRunningAsync(false);
        QVERIFY(lane.waitIdleForTest(kLaneIdleTimeoutMs));
        analyzer.reset();
        QVERIFY(lane.waitIdleForTest(kLaneIdleTimeoutMs));
        engine.shutdown();
        engine.setTransmitLane(nullptr);
        lane.stop();

        QVERIFY2(checked > 10 * block, "too few tone samples reached the analyzer");
        QVERIFY2(breaks == 0, "a block reached the siphon out of order, twice or not at all");
        QVERIFY2(std::abs(s.peakHz - kToneHz) < 3.0 * hzPerPixel(args), describe(s).constData());
        QVERIFY2(s.at66LowDb <= -kRequiredDownDb && s.at66HighDb <= -kRequiredDownDb,
                 describe(s).constData());
        QVERIFY2(s.worstBeyondDb <= -kRequiredDownDb, describe(s).constData());
    }
};

QTEST_MAIN(TestTxAnalyzerSkirt)
#include "tst_tx_analyzer_skirt.moc"
