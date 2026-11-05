// no-port-check: NereusSDR-original test; cites Thetis and WDSP for the
// convention it checks, no logic ported.
//
// =================================================================
// tests/tst_auto_agc_floor_reference.cpp  (NereusSDR)
// =================================================================
//
// Auto AGC-T reads a noise floor referred to a 4096-point FFT.
//
// WDSP reads the AGC threshold as the noise in one bin of width
// rate / size (third_party/wdsp/src/wcpAGC.c:504-515) and RxChannel always
// gives it size 4096. FFTEngine normalizes by the window's coherent gain
// only, so the noise in each bin falls 3.01 dB when the FFT size doubles,
// and the running size follows the zoom. NoiseFloorTracker therefore keeps
// a second floor, each frame's mean plus 10 * log10(N / 4096), and
// RadioModel::updateAutoAgc reads that one with a cal offset of 2.0 alone:
// no term for the FFT Size slider.
//
// Thetis differs: it gives WDSP the real display FFT size
// (console.cs:46055-46056 [v2.10.3.15]) and keeps
// `- Display.RX1FFTSizeOffset` in agc_cal_offset (console.cs:33357-33358
// [v2.10.3.15]).
//
// Every frame here comes out of a real FFTEngine fed white noise.
// =================================================================

#include <QtTest>

#include "core/ControlRanges.h"
#include "core/FFTEngine.h"
#include "core/HpsdrModel.h"
#include "core/NoiseFloorTracker.h"
#include "core/daemon/DaemonAgcSource.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>

using namespace NereusSDR;

namespace {

constexpr float kFrameIntervalMs = 33.0f;   // what MainWindow's feed passes
constexpr float kDbPerDoubling = 3.0103f;
constexpr int kRateHz = 192000;
constexpr float kAmplitude = 0.15f;

// Uniform white noise of fixed power: I/Q pairs, each component uniform in
// [-amplitude, amplitude]. The same generator as tst_daemon_agc_source,
// carried on across calls so no two frames match.
class Noise {
public:
    QVector<float> pairs(int pairCount, float amplitude)
    {
        QVector<float> samples;
        samples.reserve(pairCount * 2);
        for (int i = 0; i < pairCount * 2; ++i) {
            m_state = m_state * 1664525U + 1013904223U;
            const float unit = static_cast<float>((m_state >> 8) & 0xffffU) / 32767.5f - 1.0f;
            samples.append(unit * amplitude);
        }
        return samples;
    }

private:
    quint32 m_state{0x4d595df4U};
};

// One stream as a local window wires it: an FFTEngine whose every frame
// feeds that stream's NoiseFloorTracker (MainWindow::ensureStreamWired).
// The engine lives on the test's thread, so a frame is fed before feedIQ
// returns.
struct LocalStream {
    NereusSDR::FFTEngine engine{0};
    NoiseFloorTracker tracker;
    Noise noise;
    int frames{0};

    LocalStream()
    {
        engine.setSampleRate(static_cast<double>(kRateHz));
        // An engine plans its first transform when it is given a size.
        engine.setFftSize(4096);
        QObject::connect(&engine, &NereusSDR::FFTEngine::fftReady, &engine,
                         [this](int, const QVector<float>& binsDbm) {
            tracker.feed(binsDbm, kFrameIntervalMs);
            ++frames;
        });
    }

    // Exactly `count` transforms of fresh noise at the engine's size. Each
    // starts from an empty input buffer, which also clears the engine's
    // 5 ms guard between two transforms.
    bool run(int count)
    {
        const int before = frames;
        for (int i = 0; i < count; ++i) {
            engine.resetInputHistory();
            // One pair past a full buffer starts the transform.
            engine.feedIQ(noise.pairs(engine.fftSize() * engine.decimation() + 1, kAmplitude));
        }
        return frames - before == count;
    }
};

// A local station with one slice on a stream, connected, no WDSP: the tick
// then moves the slice's AGC threshold and nothing else.
struct Station {
    RadioModel radio;
    SliceModel* slice{nullptr};

    Station()
    {
        radio.setBoardForTest(HPSDRHW::Saturn);
        radio.configureStreamPool(/*userDdcCount=*/5, /*maxSlices=*/5,
                                  /*defaultRateHz=*/kRateHz);
        radio.setConnectionStateForTest(ConnectionState::Connected);
        slice = radio.sliceById(radio.addSlice(QStringLiteral("agc-floor-pan")));
        Q_ASSERT(slice && slice->streamIndex() >= 0);
    }

    // The tick's result for `target`, from a known starting point so a tick
    // that did nothing is seen.
    int tick(SliceModel* target = nullptr)
    {
        SliceModel* const s = target ? target : slice;
        s->setAgcThreshold(kUntouched);
        s->setAutoAgcEnabled(true);
        radio.runAutoAgcTickForTest();
        return s->agcThreshold();
    }

    static constexpr int kUntouched = -42;
};

// The threshold the tick must give for a floor per (rate / 4096) Hz.
int thresholdFor(double floorAt4096, const SliceModel& slice)
{
    const double calOffset = slice.agcMode() == AGCMode::Off ? 0.0 : 2.0;
    const double threshold = floorAt4096 + slice.autoAgcOffset() - calOffset;
    return static_cast<int>(std::round(std::clamp(threshold, -160.0, 2.0)));
}

} // namespace

class TstAutoAgcFloorReference final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
#ifndef HAVE_FFTW3
        QSKIP("FFTEngine has no FFTW3 backend in this build");
#endif
        qRegisterMetaType<QVector<float>>();
        qRegisterMetaType<MediaSourceKey>();
    }

    // White noise of one power through a real FFTEngine at four sizes: the
    // floor per bin falls about 3.01 dB per doubling and the floor Auto
    // AGC-T reads agrees within 0.3 dB.
    void theReferredFloorAgreesAcrossFftSizes()
    {
        float rawAt4096 = 0.0f;
        float agcAt4096 = 0.0f;
        for (const int size : {4096, 8192, 32768, 131072}) {
            LocalStream stream;
            stream.tracker.setAttackTimeMs(100.0f);
            stream.engine.setFftSize(size);
            QVERIFY(stream.run(8));
            QCOMPARE(stream.engine.fftSize(), size);
            const float raw = stream.tracker.noiseFloor();
            const float agc = stream.tracker.agcNoiseFloor();
            if (size == 4096) {
                rawAt4096 = raw;
                agcAt4096 = agc;
                QCOMPARE(agc, raw);
                QVERIFY(raw > -120.0f && raw < -20.0f);
                continue;
            }
            const float doublings = std::log2(static_cast<float>(size) / 4096.0f);
            QVERIFY2(std::abs(agc - agcAt4096) < 0.3f,
                     qPrintable(QStringLiteral("size %1: referred floor %2 against %3 at 4096")
                                    .arg(size).arg(agc).arg(agcAt4096)));
            QVERIFY2(std::abs((rawAt4096 - raw) - kDbPerDoubling * doublings) < 0.3f,
                     qPrintable(QStringLiteral("size %1: floor per bin %2 against %3 at 4096")
                                    .arg(size).arg(raw).arg(rawAt4096)));
        }
    }

    // The display's decimation keeps every Nth sample with no filter, so
    // the whole band's noise folds into the bins that remain: the level in
    // each bin depends on the FFT size alone and the reference needs no
    // decimation term. A reference worked out from the bin width in Hz
    // (rate / (decimation x N)) would come out 3.01 dB high per doubling of
    // the decimation, and this test would fail it.
    void theDisplayDecimationPlaysNoPart()
    {
        LocalStream plain;
        plain.tracker.setAttackTimeMs(100.0f);
        QVERIFY(plain.run(8));
        const float reference = plain.tracker.agcNoiseFloor();
        struct Row { int size; int decimation; };
        const Row rows[] = {{4096, 2}, {8192, 2}, {8192, 4}, {32768, 8}};
        for (const Row& row : rows) {
            LocalStream stream;
            stream.tracker.setAttackTimeMs(100.0f);
            stream.engine.setFftSize(row.size);
            stream.engine.setDecimation(row.decimation);
            QCOMPARE(stream.engine.decimation(), row.decimation);
            QVERIFY(stream.run(8));
            QCOMPARE(stream.engine.fftSize(), row.size);
            const float agc = stream.tracker.agcNoiseFloor();
            QVERIFY2(std::abs(agc - reference) < 0.3f,
                     qPrintable(QStringLiteral("size %1 decimation %2: %3 against %4")
                                    .arg(row.size).arg(row.decimation).arg(agc).arg(reference)));
        }
    }

    void theThresholdIsTheSameAtEveryRunningSize_data()
    {
        // Each sizing mode as FftEnginePool::setSizing leaves an engine: the
        // baseline is the stored size in Fixed and 4096 otherwise, and the
        // Hz/bin target is above 0 in Hz/bin only. `running` is the size
        // the pan's plan then asks for.
        QTest::addColumn<int>("baseline");
        QTest::addColumn<double>("hzPerBinTarget");
        QTest::addColumn<int>("running");
        QTest::newRow("Auto, full span") << 4096 << 0.0 << 4096;
        QTest::newRow("Auto, zoomed to 16384") << 4096 << 0.0 << 16384;
        QTest::newRow("Auto, zoomed to 131072") << 4096 << 0.0 << 131072;
        QTest::newRow("Hz/bin, 5.86 Hz") << 4096 << 5.86 << 32768;
        QTest::newRow("Hz/bin, 1.46 Hz") << 4096 << 1.46 << 131072;
        QTest::newRow("Fixed 8192") << 8192 << 0.0 << 8192;
        QTest::newRow("Fixed 16384") << 16384 << 0.0 << 16384;
        QTest::newRow("Fixed 65536") << 65536 << 0.0 << 65536;
        QTest::newRow("Fixed 262144") << 262144 << 0.0 << 262144;
    }

    // The tick, with an engine attached as a local window attaches stream
    // 0's: the threshold is the one a 4096-point FFT gives, whatever size
    // runs and whatever the FFT Size slider's offset says.
    void theThresholdIsTheSameAtEveryRunningSize()
    {
        QFETCH(int, baseline);
        QFETCH(double, hzPerBinTarget);
        QFETCH(int, running);

        // The same noise at 4096, Auto: the reference.
        Station referenceStation;
        LocalStream referenceStream;
        referenceStream.tracker.setAttackTimeMs(100.0f);
        referenceStation.radio.setFftEngine(&referenceStream.engine);
        referenceStation.radio.setStreamNoiseFloorTracker(
            referenceStation.slice->streamIndex(), &referenceStream.tracker);
        QVERIFY(referenceStream.run(8));
        QVERIFY(referenceStream.tracker.isGood());
        const int reference = referenceStation.tick();
        QCOMPARE(reference,
                 thresholdFor(referenceStream.tracker.noiseFloor(), *referenceStation.slice));
        QVERIFY(reference != Station::kUntouched);

        Station station;
        LocalStream stream;
        stream.tracker.setAttackTimeMs(100.0f);
        stream.engine.setFftSizeBaseline(baseline);
        stream.engine.setHzPerBinTarget(hzPerBinTarget);
        stream.engine.setFftSize(running);
        station.radio.setFftEngine(&stream.engine);
        station.radio.setStreamNoiseFloorTracker(station.slice->streamIndex(), &stream.tracker);
        QVERIFY(stream.run(8));
        QCOMPARE(stream.engine.fftSize(), running);
        QVERIFY(stream.tracker.isGood());
        const int threshold = station.tick();
        QCOMPARE(threshold, thresholdFor(stream.tracker.agcNoiseFloor(), *station.slice));
        QVERIFY2(std::abs(threshold - reference) <= 1,
                 qPrintable(QStringLiteral("threshold %1 against %2 at 4096")
                                .arg(threshold).arg(reference)));
        // The floor per bin did move, so the agreement is the reference's
        // doing and not an idle engine's.
        if (running > 4096) {
            QVERIFY(stream.tracker.noiseFloor()
                    < referenceStream.tracker.noiseFloor() - 2.5f);
        }
    }

    // AGC mode Off (Thetis FIXD) has no cal offset, as before.
    void agcModeOffTakesNoCalOffset()
    {
        Station station;
        LocalStream stream;
        stream.tracker.setAttackTimeMs(100.0f);
        stream.engine.setFftSize(32768);
        station.radio.setStreamNoiseFloorTracker(station.slice->streamIndex(), &stream.tracker);
        QVERIFY(stream.run(8));
        QVERIFY(station.slice->agcMode() != AGCMode::Off);
        const int on = station.tick();
        QCOMPARE(on, thresholdFor(stream.tracker.agcNoiseFloor(), *station.slice));
        station.slice->setAgcMode(AGCMode::Off);
        const int off = station.tick();
        QCOMPARE(off, thresholdFor(stream.tracker.agcNoiseFloor(), *station.slice));
        QCOMPARE(off - on, 2);
    }

    // A slice on a stream other than 0 reads its own stream's frames. The
    // engine attached to the model is stream 0's, carries the largest FFT
    // Size offset and hears no noise at all: nothing of it reaches this
    // slice's threshold.
    void aSliceOnAnotherStreamReadsItsOwnFrames()
    {
        Station station;
        SliceModel* other = station.radio.sliceById(
            station.radio.addSlice(QStringLiteral("agc-floor-pan-b")));
        QVERIFY(other);
        QVERIFY(other->streamIndex() > 0);
        QVERIFY(other->streamIndex() != station.slice->streamIndex());

        LocalStream zero;
        zero.engine.setFftSizeBaseline(262144);
        station.radio.setFftEngine(&zero.engine);
        QCOMPARE(zero.engine.fftSizeOffsetDb(), 12.0);

        LocalStream own;
        own.tracker.setAttackTimeMs(100.0f);
        own.engine.setFftSize(16384);
        station.radio.setStreamNoiseFloorTracker(other->streamIndex(), &own.tracker);
        QVERIFY(own.run(8));

        const int threshold = station.tick(other);
        QCOMPARE(threshold, thresholdFor(own.tracker.agcNoiseFloor(), *other));
        QVERIFY(threshold != Station::kUntouched);
    }

    // A 192 kHz stream in Auto, zoomed from the full span to 6 kHz: the
    // plan takes the running size from 4096 to 131072. The tracker is not
    // restarted, stays good, and the threshold holds within 1 dB at every
    // step while the floor per bin glides down.
    void theThresholdHoldsThroughAZoomWithNoRestart()
    {
        Station station;
        LocalStream stream;
        station.radio.setFftEngine(&stream.engine);
        station.radio.setStreamNoiseFloorTracker(station.slice->streamIndex(), &stream.tracker);
        // The tracker's own 2 s attack: 61 frames at 33 ms after the snap.
        QVERIFY(stream.run(62));
        QVERIFY(stream.tracker.isGood());
        const int start = station.tick();
        QVERIFY(start != Station::kUntouched);
        const float rawAtStart = stream.tracker.noiseFloor();
        QCOMPARE(stream.tracker.agcNoiseFloor(), rawAtStart);

        int lastSize = 4096;
        for (const double spanHz : {96000.0, 48000.0, 24000.0, 12000.0, 6000.0}) {
            const int size = ControlRanges::planDisplayFft(
                ControlRanges::DisplayFftSizingMode::Auto, 4096,
                static_cast<double>(kRateHz), spanHz, 0.0).size;
            QVERIFY(size > lastSize);
            lastSize = size;
            stream.engine.setFftSize(size);
            for (int frame = 0; frame < 6; ++frame) {
                QVERIFY(stream.run(1));
                QCOMPARE(stream.engine.fftSize(), size);
                QVERIFY(!stream.tracker.fastAttackPending());
                QVERIFY(stream.tracker.isGood());
                const int threshold = station.tick();
                QVERIFY2(std::abs(threshold - start) <= 1,
                         qPrintable(QStringLiteral("span %1 Hz, size %2: threshold %3 against %4")
                                        .arg(spanHz).arg(size).arg(threshold).arg(start)));
            }
        }
        QCOMPARE(lastSize, 131072);
        // 30 frames into the 2 s attack the floor per bin is on its way to
        // 15 dB down; the tick no longer follows it.
        QVERIFY(stream.tracker.noiseFloor() < rawAtStart - 2.0f);
        QVERIFY(std::abs(stream.tracker.agcNoiseFloor() - rawAtStart) < 0.5f);
    }

    // The same noise heard by the Core (its own 4096-point FFT, through
    // DaemonAgcSource) and by a local window running 32768: one threshold.
    void aLocalWindowAndTheCoreGiveTheSameThreshold()
    {
        Station core;
        DaemonAgcSource source(&core.radio);
        QTRY_COMPARE(source.activeStreamCount(), 1);
        Noise coreNoise;
        // DaemonAgcSource's FFT runs on its own thread and its tracker
        // takes 2 s of 33 ms frames to settle: the same paced feed as
        // tst_daemon_agc_source's warm().
        for (int frame = 0; frame < 150 && !core.slice->stationAutoAgcNoiseFloorValid(); ++frame) {
            core.radio.forkIqToTapsForTest(core.slice->streamIndex(),
                                         coreNoise.pairs(4096, kAmplitude));
            QTest::qWait(35);
        }
        QVERIFY(core.slice->stationAutoAgcNoiseFloorValid());
        const int coreThreshold = core.tick();
        QCOMPARE(coreThreshold,
                 thresholdFor(core.slice->stationAutoAgcNoiseFloorDbm(), *core.slice));

        Station local;
        LocalStream stream;
        stream.engine.setFftSize(32768);
        local.radio.setFftEngine(&stream.engine);
        local.radio.setStreamNoiseFloorTracker(local.slice->streamIndex(), &stream.tracker);
        QVERIFY(stream.run(62));
        QVERIFY(stream.tracker.isGood());
        const int localThreshold = local.tick();
        QVERIFY2(std::abs(localThreshold - coreThreshold) <= 1,
                 qPrintable(QStringLiteral("local %1 against the Core's %2")
                                .arg(localThreshold).arg(coreThreshold)));
        // And the floor per bin the local window draws is 9 dB lower.
        QVERIFY(std::abs(stream.tracker.noiseFloor() + 3.0f * kDbPerDoubling
                         - static_cast<float>(core.slice->stationAutoAgcNoiseFloorDbm())) < 0.5f);
    }
};

QTEST_MAIN(TstAutoAgcFloorReference)
#include "tst_auto_agc_floor_reference.moc"
