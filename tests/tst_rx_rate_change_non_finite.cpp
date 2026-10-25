// =================================================================
// tests/tst_rx_rate_change_non_finite.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original linked-WDSP test. Synthetic tones prove
// the input contract and WDSP's recovery, not listening quality.
//
// 2026-10-23: a slice moved to a slower stream is fed before its channel's
// rate change lands. On the Zephyrus G2E Core (combined-20261022d) slice B
// moved from stream 0 (1536 kHz, in_size 2048) to a new stream 1 (192 kHz,
// in_size 256). The DSP worker drained the new stream's 256-sample chunks at
// once, while the channel's SetInputBuffsize waited 27 ms on the receive
// lane. fexchange2 copies ch[].in_size samples from the caller's legs
// whatever count it is given (third_party/wdsp/src/iobuffs.c:543-547), so it
// read 1792 samples of whatever memory followed each chunk. The Core logged
// 28544 non-finite output samples and the S-meter stuck at +3086 dBm.
//
// The first cases stand in for that memory with NaN after each short chunk,
// so the over-read is seen every run; with NB1 on, the blanker used to write
// its old block size back into the caller's legs as well, so that case also
// checks nothing past the chunk is written. A chunk longer than the channel's
// block (the move the other way) is silenced too. The last case measures
// what a NaN burst that does reach WDSP leaves behind: the audio flushes
// out, and the log line records the meters.
//
// J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================
#include <QtTest>
#include <QTemporaryDir>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <numbers>
#include <vector>

#include "core/RxChannel.h"
#include "core/SampleRateCatalog.h"
#include "core/WdspEngine.h"
#include "core/WdspTypes.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

namespace {

constexpr int kOldRateHz = 1536000;
constexpr int kNewRateHz = 192000;
constexpr int kDspSize = 1024;
constexpr int kDspRateHz = 48000;
constexpr int kOutRateHz = 48000;
// The largest block either rate uses, so every leg can hold the over-read.
constexpr int kLegLength = bufferSizeForRate(kOldRateHz);   // 2048
// Above any reading a finite signal gives (a NaN reads 3084 dB).
constexpr double kMeterCeilingDb = 100.0;

struct Tally {
    long nonFinite = 0;
    long lastNonFiniteBlock = -1;
    double peak = 0.0;
    long tailWrites = 0;   // blocks that wrote into the leg past the chunk
};

// Feeds `blocks` chunks of `chunk` samples of a 1 kHz tone at `rateHz` to
// rx, the call RxDspWorker makes. Each leg is kLegLength long and holds NaN
// past the chunk: what a read past the chunk would find. Every output sample
// fexchange2 can write is checked; the peak counts from block `peakFrom`.
// A real-valued tone lands in both sidebands, so the mode does not matter.
void feed(RxChannel* rx, int rateHz, int chunk, int blocks, long& n, Tally& t,
          int peakFrom = 0, bool nanChunk = false)
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    std::vector<float> inI(kLegLength, nan), inQ(kLegLength, nan);
    std::vector<float> outI(kLegLength), outQ(kLegLength);
    const int outSize = chunk * kOutRateHz / rateHz;
    for (int block = 0; block < blocks; ++block) {
        for (int i = 0; i < chunk; ++i, ++n) {
            inI[static_cast<std::size_t>(i)] = nanChunk ? nan
                : static_cast<float>(0.01 * std::cos(2.0 * std::numbers::pi * 1000.0
                                                     * static_cast<double>(n) / rateHz));
            inQ[static_cast<std::size_t>(i)] = nanChunk ? nan : 0.0f;
        }
        std::fill(outI.begin(), outI.end(), 0.0f);
        std::fill(outQ.begin(), outQ.end(), 0.0f);
        rx->processIq(inI.data(), inQ.data(), outI.data(), outQ.data(), chunk, outSize);
        bool wrote = false;
        for (int i = chunk; i < kLegLength; ++i) {
            const auto k = static_cast<std::size_t>(i);
            wrote = wrote || !std::isnan(inI[k]) || !std::isnan(inQ[k]);
            inI[k] = nan;
            inQ[k] = nan;
        }
        t.tailWrites += wrote ? 1 : 0;
        for (int i = 0; i < kLegLength; ++i) {
            const float a = outI[static_cast<std::size_t>(i)];
            const float b = outQ[static_cast<std::size_t>(i)];
            if (!std::isfinite(a) || !std::isfinite(b)) {
                ++t.nonFinite;
                t.lastNonFiniteBlock = block;
            } else if (block >= peakFrom) {
                t.peak = std::max(t.peak, static_cast<double>(std::abs(a)));
            }
        }
    }
}

bool meterSane(double db)
{
    return std::isfinite(db) && db < kMeterCeilingDb;
}

} // namespace

class TestRxRateChangeNonFinite : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QVERIFY(m_directory.isValid());
        m_engine = std::make_unique<WdspEngine>();
        m_engine->setSynchronousInitForTest(true);
        QVERIFY(m_engine->initialize(m_directory.path() + '/'));
    }

    // The Core's sequence on one channel: running at 1536 kHz, then handed
    // the new stream's 256-sample chunks before its rate change, then
    // re-rated to 192 kHz and fed on. No output sample and no S-meter
    // reading may be non-finite, and the tone plays after the change.
    void aChunkShorterThanTheChannelsInputNeverReachesWdsp()
    {
        moveToASlowerStream(0, NbMode::Off);
    }

    // The same with NB1 on. The blanker runs before fexchange2, in place on
    // the caller's legs, over its own buffsize (nob.c:437-446): a short chunk
    // used to be blanked and written 1792 samples past its end.
    void withNb1OnNothingPastTheChunkIsReadOrWritten()
    {
        moveToASlowerStream(4, NbMode::NB);
    }

    // The move the other way: a 1536 kHz stream's 2048-sample chunks reach a
    // channel still built for 256. In bounds, but at the wrong rate, so the
    // chunks are silenced and counted rather than played.
    void aChunkLongerThanTheChannelsInputIsSilenced()
    {
        RxChannel* rx = m_engine->createRxChannel(5, bufferSizeForRate(kNewRateHz), kDspSize,
                                                  kNewRateHz, kDspRateHz, kOutRateHz);
        QVERIFY(rx);
        rx->setActive(true);
        long n = 0;
        Tally before;
        feed(rx, kNewRateHz, bufferSizeForRate(kNewRateHz), 300, n, before, 200);
        QVERIFY(before.peak > 0.0);
        QCOMPARE(mismatchChunks(rx), 0LL); // new API
        Tally window;
        long m = 0;
        feed(rx, kOldRateHz, bufferSizeForRate(kOldRateHz), 40, m, window);
        expectMismatch(rx, 40, bufferSizeForRate(kOldRateHz), bufferSizeForRate(kNewRateHz)); // new API
        qInfo("longer chunks: output peak %.6f, non-finite %ld", window.peak, window.nonFinite);
        QCOMPARE(window.nonFinite, 0L);
        QCOMPARE(window.peak, 0.0);
        m_engine->destroyRxChannel(5);
    }

    void theCoreLogLineCountsTheSilencedChunks()
    {
        QCOMPARE(RadioModel::inputSizeMismatchLogLine(1, 20, 256, 2048), QStringLiteral("Slice B (index 1): 20 I/Q chunk(s) silenced since the last report (26.7 ms of audio): chunks of 256 samples, WDSP's input block 2048")); // new API
    }

    // A rate move's window (about 20 chunks on the Core) logs at info; more
    // than 100 ms of silenced audio in an interval is a lasting mismatch.
    void aLastingMismatchIsTheOnlyWarning()
    {
        QCOMPARE(RadioModel::inputSizeMismatchAudioMs(75), 100.0); // new API
        QVERIFY(!RadioModel::inputSizeMismatchIsLasting(20)); // new API
        QVERIFY(!RadioModel::inputSizeMismatchIsLasting(75)); // new API
        QVERIFY(RadioModel::inputSizeMismatchIsLasting(76)); // new API
    }

    // What NaN does once it is inside a channel. A burst of whole NaN
    // chunks, then the tone again: the audio is finite again and as loud as
    // before within two seconds, so the filters and AGC flush it. The meter
    // readings are logged, not asserted: WDSP's meter keeps a NaN for good
    // until its own fix (draft PR 629) lands.
    void aNanBurstInsideWdspFlushesOutOfTheAudio()
    {
        RxChannel* rx = m_engine->createRxChannel(2, bufferSizeForRate(kNewRateHz), kDspSize,
                                                  kNewRateHz, kDspRateHz, kOutRateHz);
        QVERIFY(rx);
        rx->setActive(true);
        const int chunk = bufferSizeForRate(kNewRateHz);

        long n = 0;
        Tally before;
        feed(rx, kNewRateHz, chunk, 600, n, before, 300);
        QCOMPARE(before.nonFinite, 0L);
        QVERIFY(before.peak > 0.0);
        const double agcBefore = rx->getMeter(RxMeterType::AgcGain);

        Tally burst;
        feed(rx, kNewRateHz, chunk, 20, n, burst, 0, /*nanChunk=*/true);

        // 1500 chunks of 64 output samples: two seconds of audio.
        Tally after;
        feed(rx, kNewRateHz, chunk, 1500, n, after, 1200);

        qInfo("NaN burst: %ld non-finite output samples after it, the last in block %ld "
              "(%.0f ms of audio); peak %.4f before, %.4f after; AGC gain %.1f dB before, "
              "%.1f dB after; S-meter peak %.1f dB, average %.1f dB",
              after.nonFinite, after.lastNonFiniteBlock,
              1000.0 * (after.lastNonFiniteBlock + 1) * 64 / kOutRateHz,
              before.peak, after.peak, agcBefore, rx->getMeter(RxMeterType::AgcGain),
              rx->getMeter(RxMeterType::SignalPeak), rx->getMeter(RxMeterType::SignalAvg));

        QVERIFY2(after.lastNonFiniteBlock < 1200, "the audio was still non-finite after 1.6 s");
        QVERIFY2(after.peak > 0.5 * before.peak && after.peak < 2.0 * before.peak,
                 "the tone did not come back at its level");

        m_engine->destroyRxChannel(2);
    }

    void cleanupTestCase()
    {
        m_engine->shutdown();
        m_engine.reset();
    }

private:
    static qint64 mismatchChunks(RxChannel* rx) // new API
    { // new API
        RxChannel::InputSizeMismatch taken; // new API
        rx->takeInputSizeMismatch(taken); // new API
        return taken.chunks; // new API
    } // new API

    static void expectMismatch(RxChannel* rx, qint64 chunks, int chunkSize, int wdspInputSize) // new API
    { // new API
        RxChannel::InputSizeMismatch taken; // new API
        QVERIFY(rx->takeInputSizeMismatch(taken)); // new API
        QCOMPARE(taken.chunks, chunks); // new API
        QCOMPARE(taken.chunkSize, chunkSize); // new API
        QCOMPARE(taken.wdspInputSize, wdspInputSize); // new API
    } // new API

    void moveToASlowerStream(int id, NbMode nb)
    {
        RxChannel* rx = m_engine->createRxChannel(id, bufferSizeForRate(kOldRateHz), kDspSize,
                                                  kOldRateHz, kDspRateHz, kOutRateHz);
        QVERIFY(rx);
        rx->setActive(true);
        rx->setNbMode(nb);
        QCOMPARE(rx->wdspInputSize(), bufferSizeForRate(kOldRateHz));

        long n = 0;
        Tally before;
        feed(rx, kOldRateHz, bufferSizeForRate(kOldRateHz), 200, n, before, 100);
        QCOMPARE(before.nonFinite, 0L);
        QVERIFY2(before.peak > 0.0, "the channel never played the tone at 1536 kHz");
        QCOMPARE(mismatchChunks(rx), 0LL); // new API

        // The window: the worker drains the 192 kHz stream into a channel
        // still built for 2048 samples (27 ms on the Core, about 20 chunks).
        Tally window;
        long m = 0;
        feed(rx, kNewRateHz, bufferSizeForRate(kNewRateHz), 40, m, window);
        expectMismatch(rx, 40, bufferSizeForRate(kNewRateHz), bufferSizeForRate(kOldRateHz)); // new API

        QVERIFY(m_engine->setRxChannelRate(id, kNewRateHz));
        QCOMPARE(rx->wdspInputSize(), bufferSizeForRate(kNewRateHz));

        Tally after;
        feed(rx, kNewRateHz, bufferSizeForRate(kNewRateHz), 800, m, after, 600);
        QCOMPARE(mismatchChunks(rx), 0LL); // new API

        const double sPeak = rx->getMeter(RxMeterType::SignalPeak);
        const double sAvg = rx->getMeter(RxMeterType::SignalAvg);
        qInfo("NB %d: non-finite output samples: %ld in the window, %ld after the rate change "
              "(last in block %ld); %ld window blocks wrote past the chunk; peak %.4f before, "
              "%.4f after; S-meter peak %.1f dB, average %.1f dB",
              static_cast<int>(nb), window.nonFinite, after.nonFinite, after.lastNonFiniteBlock,
              window.tailWrites, before.peak, after.peak, sPeak, sAvg);

        QCOMPARE(window.tailWrites, 0L);
        QCOMPARE(window.nonFinite + after.nonFinite, 0L);
        QVERIFY2(meterSane(sPeak) && meterSane(sAvg), "the S-meter holds a non-finite reading");
        QVERIFY2(after.peak > 0.0, "the channel never played the tone at 192 kHz");

        m_engine->destroyRxChannel(id);
    }

    QTemporaryDir m_directory;
    std::unique_ptr<WdspEngine> m_engine;
};

QTEST_MAIN(TestRxRateChangeNonFinite)
#include "tst_rx_rate_change_non_finite.moc"
