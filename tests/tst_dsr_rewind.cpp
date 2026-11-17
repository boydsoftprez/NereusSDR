// =================================================================
// tests/tst_dsr_rewind.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test. Rewind sound (spec 3, 6.7,
// recording plan Task 22): a rewound slice's demodulator is fed from the
// Digital Spectrum Recorder's ring file through RxDspWorker's one switch,
// in real time, at the clicked frequency; the other slices keep the radio's
// I/Q unchanged; LIVE returns to the radio and the frequency before
// rewinding; PTT while rewound goes live on the frequency being played;
// the newest second kept plays on into live; a moment no longer kept is
// refused; the recorder keeps the live I/Q meanwhile. A past carrier at
// 7.100 MHz, demodulated in USB at 7.099 MHz by a real WDSP channel, is a
// 1 kHz tone.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-16 - Created. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code. Recording plan Task 22.
//   2026-11-16 - J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//                 Code. Final review fix: a closed slice's replay is
//                 switched out.
//   2026-11-16 - J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//                 Code. The rewound sound stays read ahead while the past
//                 picture is drawn on the same worker.
// =================================================================

#include <QtTest/QtTest>

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QTemporaryDir>
#include <QTimeZone>

#include "core/RxChannel.h"
#include "core/SampleRateCatalog.h"
#include "core/WdspEngine.h"
#include "core/WdspTypes.h"
#include "core/recordings/RecordingService.h"
#include "core/recordings/RecordingSettings.h"
#include "core/recordings/dsr/DsrRecorder.h"
#include "core/recordings/dsr/DsrReplay.h"
#include "models/RxDspWorker.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <numbers>
#include <vector>

using namespace NereusSDR;

namespace {

const QDateTime kStartUtc(QDate(2026, 11, 16), QTime(14, 32, 0), QTimeZone::UTC);
constexpr int kRate = 192000;
constexpr int kChunkFrames = 4800;          // what the tap takes at once, 25 ms
constexpr int kInSize = bufferSizeForRate(kRate);   // 256: one demodulator block
constexpr qint64 kKeptCentreHz = 7095000;   // the pan's centre when it was kept
constexpr qint64 kCarrierHz = 7100000;      // the past signal
constexpr qint64 kPlayHz = 7099000;         // the clicked frequency, USB
constexpr qint64 kSliceAHz = 7150000;       // slice A before rewinding
constexpr qint64 kSliceBHz = 7120000;
constexpr float kCarrierAmplitude = 0.5f;
constexpr float kLiveI = 0.125f;            // the radio's I/Q now: a pattern
constexpr float kLiveQ = -0.25f;            // nothing kept looks like
constexpr qint64 kNs = DsrRingFile::kNsPerSecond;

RecordingVolume volume(const QString& root, bool own)
{
    RecordingVolume v;
    v.rootPath = root;
    v.displayName = QFileInfo(root).fileName();
    v.freeBytes = 500LL * 1000000000LL;
    v.totalBytes = 1000LL * 1000000000LL;
    v.isCoreOwnDisk = own;
    v.isSdCard = false;
    return v;
}

// What each slice's demodulator was handed, block by block.
struct SliceInputs {
    QHash<int, QList<std::vector<float>>> i;
    QHash<int, QList<std::vector<float>>> q;
    void clear()
    {
        i.clear();
        q.clear();
    }
};
SliceInputs* g_inputs = nullptr;

void captureSliceInput(int slice, const float* i, const float* q, int samples)
{
    g_inputs->i[slice].append(std::vector<float>(i, i + samples));
    g_inputs->q[slice].append(std::vector<float>(q, q + samples));
}

bool isLiveBlock(const std::vector<float>& i, const std::vector<float>& q)
{
    for (size_t k = 0; k < i.size(); ++k) {
        if (i[k] != kLiveI || q[k] != kLiveQ) {
            return false;
        }
    }
    return true;
}

// The radio as rewind sees it: slices A and B on receiver 0, and a DSP
// worker whose switch it sets, keeping each replay alive like RadioModel.
class FakeRadio final : public DsrRewindHost {
public:
    QHash<int, qint64> frequency{{0, kSliceAHz}, {1, kSliceBHz}};
    qint64 centreHz = kKeptCentreHz;
    int rateHz = kRate;
    bool diversity = false;
    RxDspWorker* worker = nullptr;
    std::shared_ptr<DsrReplay> held[2];

    bool sliceOpen(int slice) const override { return frequency.contains(slice); }
    qint64 sliceFrequencyHz(int slice) const override { return frequency.value(slice); }
    void setSliceFrequencyHz(int slice, qint64 hz) override { frequency[slice] = hz; }
    int sliceReceiver(int slice) const override { return frequency.contains(slice) ? 0 : -1; }
    qint64 receiverCentreHz(int receiver) const override { return receiver == 0 ? centreHz : 0; }
    int receiverRateHz(int receiver) const override { return receiver == 0 ? rateHz : 0; }
    bool sliceOnDiversity(int) const override { return diversity; }
    bool feedSlice(int slice, std::shared_ptr<DsrReplay> replay) override
    {
        if (worker == nullptr || slice < 0 || slice > 1) {
            return replay == nullptr;
        }
        worker->setSliceReplay(slice, replay.get());
        held[slice] = std::move(replay);
        return true;
    }
};

}  // namespace

class TestDsrRewind : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    std::atomic<qint64> m_clockMs{0};
    double m_carrierPhase = 0.0;
    SliceInputs m_inputs;

    QString ssdRoot() const
    {
        return m_dir.filePath(QStringLiteral("ssd-") + QString::fromLatin1(QTest::currentTestFunction()));
    }

    // The recorder on a USB SSD at 1 GB, running, receiver 0 at 192 kHz
    // around kKeptCentreHz; slices A and B on the fake radio.
    FakeRadio* prepare(RecordingService& svc)
    {
        QDir().mkpath(ssdRoot());
        m_clockMs = kStartUtc.toMSecsSinceEpoch();
        m_carrierPhase = 0.0;
        svc.setClockForTest([this] { return m_clockMs.load(); });
        svc.setDsrVolumesForTest([this] {
            return QList<RecordingVolume>{volume(QDir::rootPath(), true), volume(ssdRoot(), false)};
        });
        RecordingSettings s = svc.settings();
        s.disk = m_dir.filePath(QStringLiteral("recordings"));
        QDir().mkpath(s.disk);
        s.format = AudioFileFormat::Wav;
        s.dsrEnabled = true;
        s.dsrDisk = ssdRoot();
        s.dsrSizeLimitGb = 1;
        svc.setSettings(s);
        auto radio = std::make_unique<FakeRadio>();
        FakeRadio* const raw = radio.get();
        svc.setDsrRewindHostForTest(std::move(radio));
        svc.setRadioStreaming(true);
        svc.dsrRecorder()->setStream(0, kKeptCentreHz, kRate);
        return raw;
    }

    // `seconds` of a carrier at kCarrierHz through the tap, kept around
    // kKeptCentreHz, one second at a time.
    void keepCarrier(RecordingService& svc, int seconds)
    {
        const double step = 2.0 * std::numbers::pi * double(kCarrierHz - kKeptCentreHz) / double(kRate);
        std::vector<float> chunk(static_cast<size_t>(kChunkFrames) * 2);
        for (int sec = 0; sec < seconds; ++sec) {
            for (int c = 0; c < kRate / kChunkFrames; ++c) {
                for (int k = 0; k < kChunkFrames; ++k) {
                    chunk[size_t(2 * k)] = kCarrierAmplitude * float(std::cos(m_carrierPhase));
                    chunk[size_t(2 * k + 1)] = kCarrierAmplitude * float(std::sin(m_carrierPhase));
                    m_carrierPhase = std::fmod(m_carrierPhase + step, 2.0 * std::numbers::pi);
                }
                svc.dsrRecorder()->consumeIq(0, chunk.data(), kChunkFrames);
            }
            m_clockMs += 1000;
            svc.pumpForTest();
        }
    }

    // One second of the radio's live pattern through the tap.
    void keepLiveSecond(RecordingService& svc)
    {
        std::vector<float> chunk(static_cast<size_t>(kChunkFrames) * 2);
        for (int k = 0; k < kChunkFrames; ++k) {
            chunk[size_t(2 * k)] = kLiveI;
            chunk[size_t(2 * k + 1)] = kLiveQ;
        }
        for (int c = 0; c < kRate / kChunkFrames; ++c) {
            svc.dsrRecorder()->consumeIq(0, chunk.data(), kChunkFrames);
        }
        m_clockMs += 1000;
        svc.pumpForTest();
    }

    // A worker fanning receiver 0 out to slices A and B, its inputs seen.
    static void prepareWorker(RxDspWorker& worker)
    {
        worker.setBufferSizes(kInSize, 64);
        worker.setStreamSlices(0, {0, 1});
        worker.setSliceInputHookForTest(&captureSliceInput);
    }

    // One demodulator block of the radio's live pattern on receiver 0.
    static void feedLiveBlock(RxDspWorker& worker)
    {
        QVector<float> live(kInSize * 2);
        for (int k = 0; k < kInSize; ++k) {
            live[2 * k] = kLiveI;
            live[2 * k + 1] = kLiveQ;
        }
        worker.processIqBatch(0, live);
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        g_inputs = &m_inputs;
    }

    void init() { m_inputs.clear(); }

    // From the next block the slice's demodulator is fed from the ring at
    // that time, its frequency is the clicked one, the past plays in real
    // time, slice B keeps the radio's I/Q unchanged, and the recorder keeps
    // the live I/Q meanwhile.
    void rewindFeedsTheSliceFromTheRingAndLeavesTheRestLive()
    {
        RecordingService svc(nullptr);
        FakeRadio* radio = prepare(svc);
        RxDspWorker worker;
        prepareWorker(worker);
        radio->worker = &worker;
        keepCarrier(svc, 10);
        const DsrRingFile& ring = svc.dsrRecorder()->ring();
        QCOMPARE(ring.newestUtcNs(0) - ring.oldestUtcNs(0), 10 * kNs);

        // Before: both slices hear the radio.
        feedLiveBlock(worker);
        QVERIFY(isLiveBlock(m_inputs.i[0].last(), m_inputs.q[0].last()));
        QVERIFY(isLiveBlock(m_inputs.i[1].last(), m_inputs.q[1].last()));

        const qint64 t = ring.oldestUtcNs(0) + 3 * kNs + 250000000;
        QString why;
        QVERIFY2(svc.rewind(0, t, kPlayHz, &why), qPrintable(why));
        QCOMPARE(radio->frequency.value(0), kPlayHz);
        QCOMPARE(radio->frequency.value(1), kSliceBHz);
        svc.pumpForTest();
        QVERIFY(worker.sliceReplay(0) != nullptr);
        QCOMPARE(worker.sliceReplay(1), nullptr);
        const QList<DsrRewoundSlice> rewound = svc.rewound();
        QCOMPARE(rewound.size(), 1);
        QCOMPARE(rewound.first().slice, 0);
        QCOMPARE(rewound.first().frequencyHz, kPlayHz);
        QCOMPARE(rewound.first().liveFrequencyHz, kSliceAHz);
        QCOMPARE(rewound.first().utcNs, t);

        // The kept I/Q from t, as the ring holds it (the centre has not
        // moved, so nothing is moved).
        std::vector<qint16> kept(static_cast<size_t>(kInSize) * 8 * 2);
        DsrBlockHeader hdr;
        QCOMPARE(ring.read(0, t, kept.data(), kInSize * 8, &hdr), kInSize * 8);
        m_inputs.clear();
        for (int block = 0; block < 8; ++block) {
            feedLiveBlock(worker);
        }
        QCOMPARE(m_inputs.i[0].size(), 8);
        QCOMPARE(m_inputs.i[1].size(), 8);
        for (int block = 0; block < 8; ++block) {
            const std::vector<float>& i = m_inputs.i[0][block];
            const std::vector<float>& q = m_inputs.q[0][block];
            for (int k = 0; k < kInSize; ++k) {
                const size_t at = size_t((block * kInSize + k) * 2);
                QCOMPARE(i[size_t(k)], float(kept[at]) / DsrReplay::kIqScale);
                QCOMPARE(q[size_t(k)], float(kept[at + 1]) / DsrReplay::kIqScale);
            }
            // Slice B: the radio's I/Q, unchanged.
            QVERIFY(isLiveBlock(m_inputs.i[1][block], m_inputs.q[1][block]));
        }
        // Real time: the position moved by what was taken, 8 blocks.
        const std::shared_ptr<DsrReplay> replay = svc.replayForTest(0);
        QVERIFY(replay);
        QCOMPARE(replay->framesPlayed(), qint64(8 * kInSize));
        QCOMPARE(replay->underruns(), quint64(0));
        const qint64 expected = hdr.utcNs + DsrRingFile::durationNs(8 * kInSize, kRate);
        QVERIFY2(std::llabs(replay->positionUtcNs() - expected) < 10000,
                 qPrintable(QStringLiteral("%1 vs %2").arg(replay->positionUtcNs()).arg(expected)));

        // The recorder keeps the radio's I/Q, not the past, while rewound.
        const qint64 before = ring.newestUtcNs(0);
        keepLiveSecond(svc);
        QCOMPARE(ring.newestUtcNs(0), before + kNs);
        std::vector<qint16> newest(4);
        QCOMPARE(ring.read(0, before + kNs / 2, newest.data(), 2, nullptr), 2);
        QCOMPARE(newest[0], qint16(std::lround(kLiveI * DsrRecorder::kIqScale)));
        QCOMPARE(newest[1], qint16(std::lround(kLiveQ * DsrRecorder::kIqScale)));

        // After LIVE, slice B was never touched either.
        QVERIFY(svc.goLive(0, &why));
        m_inputs.clear();
        feedLiveBlock(worker);
        QVERIFY(isLiveBlock(m_inputs.i[1].last(), m_inputs.q[1].last()));
        radio->worker = nullptr;
    }

    // LIVE returns the slice to the radio's I/Q and to the frequency it had
    // before rewinding; a second rewind keeps that frequency.
    void liveReturnsToTheRadioAndTheFrequencyBefore()
    {
        RecordingService svc(nullptr);
        FakeRadio* radio = prepare(svc);
        RxDspWorker worker;
        prepareWorker(worker);
        radio->worker = &worker;
        keepCarrier(svc, 5);
        const DsrRingFile& ring = svc.dsrRecorder()->ring();
        QString why;
        QVERIFY(svc.rewind(0, ring.oldestUtcNs(0) + kNs, kPlayHz, &why));
        svc.pumpForTest();
        // A second click moves within the past; LIVE still knows the start.
        QVERIFY(svc.rewind(0, ring.oldestUtcNs(0) + 2 * kNs, kCarrierHz - 500, &why));
        svc.pumpForTest();
        QCOMPARE(svc.rewound().first().liveFrequencyHz, kSliceAHz);
        QVERIFY(worker.sliceReplay(0) != nullptr);

        QVERIFY(svc.goLive(0, &why));
        QCOMPARE(worker.sliceReplay(0), nullptr);
        QCOMPARE(radio->frequency.value(0), kSliceAHz);
        QVERIFY(svc.rewound().isEmpty());
        QVERIFY(!radio->held[0]);
        m_inputs.clear();
        feedLiveBlock(worker);
        QVERIFY(isLiveBlock(m_inputs.i[0].last(), m_inputs.q[0].last()));
        // LIVE on a live slice changes nothing.
        QVERIFY(svc.goLive(0, &why));
        QCOMPARE(radio->frequency.value(0), kSliceAHz);
        radio->worker = nullptr;
    }

    // dsr.seek: a rewound slice moves to another moment, same frequency.
    void seekMovesTheRewoundSlice()
    {
        RecordingService svc(nullptr);
        FakeRadio* radio = prepare(svc);
        RxDspWorker worker;
        prepareWorker(worker);
        radio->worker = &worker;
        keepCarrier(svc, 5);
        const DsrRingFile& ring = svc.dsrRecorder()->ring();
        QString why;
        QVERIFY(!svc.seekRewind(0, ring.oldestUtcNs(0), &why));
        QCOMPARE(why, QString::fromLatin1(RecordingService::kRewindNotRewoundReason));
        QVERIFY(svc.rewind(0, ring.oldestUtcNs(0) + kNs, kPlayHz, &why));
        svc.pumpForTest();
        const qint64 to = ring.oldestUtcNs(0) + 3 * kNs;
        QVERIFY(svc.seekRewind(0, to, &why));
        svc.pumpForTest();
        QCOMPARE(svc.replayForTest(0)->fromUtcNs(), to);
        QCOMPARE(worker.sliceReplay(0), svc.replayForTest(0).get());
        QCOMPARE(radio->frequency.value(0), kPlayHz);
        QVERIFY(!svc.seekRewind(0, ring.newestUtcNs(0) + 60 * kNs, &why));
        QCOMPARE(why, QString::fromLatin1(RecordingService::kRewindGoneReason));
        svc.goLive(0, &why);
        radio->worker = nullptr;
    }

    // Spec 3: PTT while rewound takes the slice live on the frequency being
    // played, not the old one; LIVE afterwards still returns to the old one.
    void pttWhileRewoundGoesLiveOnThePlayedFrequency()
    {
        RecordingService svc(nullptr);
        FakeRadio* radio = prepare(svc);
        RxDspWorker worker;
        prepareWorker(worker);
        radio->worker = &worker;
        keepCarrier(svc, 5);
        const DsrRingFile& ring = svc.dsrRecorder()->ring();
        QString why;
        QVERIFY(svc.rewind(0, ring.oldestUtcNs(0) + kNs, kPlayHz, &why));
        svc.pumpForTest();
        QVERIFY(worker.sliceReplay(0) != nullptr);

        svc.rewoundSlicesLiveForTransmit();
        QCOMPARE(worker.sliceReplay(0), nullptr);
        QCOMPARE(radio->frequency.value(0), kPlayHz);
        QVERIFY(svc.rewound().isEmpty());
        m_inputs.clear();
        feedLiveBlock(worker);
        QVERIFY(isLiveBlock(m_inputs.i[0].last(), m_inputs.q[0].last()));

        QVERIFY(svc.goLive(0, &why));
        QCOMPARE(radio->frequency.value(0), kSliceAHz);
        radio->worker = nullptr;
    }

    // A rewound slice that closes is switched back to the radio: the next
    // slice on that id hears the live I/Q, and nothing is left rewound.
    void aClosedSliceIsSwitchedOutOfThePast()
    {
        RecordingService svc(nullptr);
        FakeRadio* radio = prepare(svc);
        RxDspWorker worker;
        prepareWorker(worker);
        radio->worker = &worker;
        keepCarrier(svc, 5);
        const DsrRingFile& ring = svc.dsrRecorder()->ring();
        QString why;
        QVERIFY(svc.rewind(1, ring.oldestUtcNs(0) + kNs, kPlayHz, &why));
        svc.pumpForTest();
        QVERIFY(worker.sliceReplay(1) != nullptr);

        radio->frequency.remove(1);
        svc.sliceRemovedForTest(1);
        QCOMPARE(worker.sliceReplay(1), nullptr);
        QVERIFY(!radio->held[1]);
        QVERIFY(svc.rewound().isEmpty());
        radio->frequency.insert(1, kSliceBHz);
        m_inputs.clear();
        feedLiveBlock(worker);
        QVERIFY(isLiveBlock(m_inputs.i[1].last(), m_inputs.q[1].last()));
        radio->worker = nullptr;
    }

    // Reaching the newest block plays on into live: every kept frame after
    // the moment, then the radio's I/Q in the next block, no silence
    // between; the slice stays on the frequency it plays.
    void theNewestSecondPlaysOnIntoLive()
    {
        RecordingService svc(nullptr);
        FakeRadio* radio = prepare(svc);
        RxDspWorker worker;
        prepareWorker(worker);
        radio->worker = &worker;
        keepCarrier(svc, 4);
        const DsrRingFile& ring = svc.dsrRecorder()->ring();
        const qint64 newest = ring.newestUtcNs(0);
        const qint64 t = newest - kNs / 2;   // 96000 frames, 375 blocks, left
        QString why;
        QVERIFY(svc.rewind(0, t, kPlayHz, &why));
        svc.pumpForTest();
        QVERIFY(worker.sliceReplay(0) != nullptr);
        const std::shared_ptr<DsrReplay> replay = svc.replayForTest(0);
        QVERIFY(replay);

        int pastBlocks = 0;
        int liveBlocks = 0;
        for (int block = 0; block < 400; ++block) {
            if (block % 50 == 0) {
                svc.pumpForTest();
            }
            m_inputs.clear();
            feedLiveBlock(worker);
            const bool live = isLiveBlock(m_inputs.i[0].last(), m_inputs.q[0].last());
            if (live) {
                ++liveBlocks;
            } else {
                QCOMPARE(liveBlocks, 0);   // never the past again once live
                ++pastBlocks;
            }
        }
        QCOMPARE(pastBlocks, 375);
        QCOMPARE(liveBlocks, 25);
        QCOMPARE(replay->underruns(), quint64(0));
        QVERIFY(replay->finished());
        // The service follows: no longer rewound, on the played frequency.
        svc.pumpForTest();
        QVERIFY(svc.rewound().isEmpty());
        QCOMPARE(worker.sliceReplay(0), nullptr);
        QCOMPARE(radio->frequency.value(0), kPlayHz);
        radio->worker = nullptr;
    }

    // Drawing past rows runs on the worker that reads the rewound sound
    // ahead, and can take longer than the read-ahead lasts. The drawing tops
    // the sound up as it goes, so the block after it is the past, not
    // silence. The worker's rounds are held: only the queued jobs run, in
    // order (empty the sound, draw, take one block), so nothing but the
    // drawing can have filled it.
    void theSoundStaysFedWhileThePastIsDrawn()
    {
        RecordingService svc(nullptr);
        FakeRadio* radio = prepare(svc);
        RxDspWorker worker;
        prepareWorker(worker);
        radio->worker = &worker;
        keepCarrier(svc, 10);
        const DsrRingFile& ring = svc.dsrRecorder()->ring();
        const qint64 oldest = ring.oldestUtcNs(0);
        QString why;
        QVERIFY2(svc.rewind(0, oldest + 3 * kNs, kPlayHz, &why), qPrintable(why));
        svc.pumpForTest();
        QVERIFY(worker.sliceReplay(0) != nullptr);
        const std::shared_ptr<DsrReplay> replay = svc.replayForTest(0);
        QVERIFY(replay);
        QVERIFY(replay->primed());
        svc.holdWorkerRoundsForTest(true);

        // The DSP thread's part, played on the worker between the jobs: take
        // everything read ahead, until a block comes up short.
        std::vector<float> i(static_cast<size_t>(kInSize));
        std::vector<float> q(static_cast<size_t>(kInSize));
        quint64 underrunsEmptied = 0;
        bool emptied = false;
        svc.runOnWorker([&] {
            const quint64 before = replay->underruns();
            for (int block = 0; block < 100000 && replay->underruns() == before; ++block) {
                if (!replay->pull(i.data(), q.data(), kInSize)) {
                    return;
                }
            }
            underrunsEmptied = replay->underruns();
            emptied = underrunsEmptied == before + 1;
        });

        DsrPanView view;
        view.fftSize = 4096;
        view.windowType = 1;
        view.centreHz = kKeptCentreHz;
        view.spanHz = 48000;
        bool drawn = false;
        svc.pastRows(0, view, oldest, oldest + 2 * kNs, 16,
                     [&](bool ok, const QString&, const DsrPastFrames& frames) {
                         drawn = ok && !frames.rows.isEmpty();
                     });

        bool fedAfter = false;
        quint64 underrunsAfter = 0;
        svc.runOnWorker([&] {
            fedAfter = replay->pull(i.data(), q.data(), kInSize);
            underrunsAfter = replay->underruns();
        });
        svc.waitForWorker();
        svc.holdWorkerRoundsForTest(false);
        QTRY_VERIFY(drawn);

        QVERIFY(emptied);
        QVERIFY(fedAfter);
        // The block after the drawing was the past in full: no silence.
        QCOMPARE(underrunsAfter, underrunsEmptied);
        QVERIFY(std::any_of(i.cbegin(), i.cend(), [](float v) { return v != 0.0f; }));
        QVERIFY(svc.goLive(0, &why));
        radio->worker = nullptr;
    }

    // A rewind outside the ring's range, or to a frequency no receiver kept
    // then, is refused with the brief's words, and nothing moves.
    void aMomentNoLongerKeptIsRefused()
    {
        RecordingService svc(nullptr);
        FakeRadio* radio = prepare(svc);
        RxDspWorker worker;
        prepareWorker(worker);
        radio->worker = &worker;
        keepCarrier(svc, 3);
        const DsrRingFile& ring = svc.dsrRecorder()->ring();
        const QString gone = QStringLiteral("That moment is no longer kept.");
        QString why;
        QVERIFY(!svc.rewind(0, ring.oldestUtcNs(0) - kNs, kPlayHz, &why));
        QCOMPARE(why, gone);
        why.clear();
        QVERIFY(!svc.rewind(0, ring.newestUtcNs(0) + 5 * kNs, kPlayHz, &why));
        QCOMPARE(why, gone);
        why.clear();
        QVERIFY(!svc.rewind(0, ring.oldestUtcNs(0) + kNs, kKeptCentreHz + kRate, &why));
        QCOMPARE(why, gone);
        QCOMPARE(radio->frequency.value(0), kSliceAHz);
        QVERIFY(svc.rewound().isEmpty());
        QCOMPARE(worker.sliceReplay(0), nullptr);
        // No such slice; diversity on.
        QVERIFY(!svc.rewind(3, ring.oldestUtcNs(0) + kNs, kPlayHz, &why));
        QCOMPARE(why, QString::fromLatin1(RecordingService::kRewindNoSliceReason));
        radio->diversity = true;
        QVERIFY(!svc.rewind(0, ring.oldestUtcNs(0) + kNs, kPlayHz, &why));
        QCOMPARE(why, QString::fromLatin1(RecordingService::kRewindDiversityReason));
        radio->diversity = false;
        // Kept at another rate than the slice's receiver runs now.
        radio->rateHz = 384000;
        QVERIFY(!svc.rewind(0, ring.oldestUtcNs(0) + kNs, kPlayHz, &why));
        QCOMPARE(why, QString::fromLatin1(RecordingService::kRewindRateReason));
        QCOMPARE(radio->frequency.value(0), kSliceAHz);
        radio->rateHz = kRate;
        // The recorder off: nothing is kept to play.
        svc.setRadioStreaming(false);
        QVERIFY(!svc.rewind(0, ring.oldestUtcNs(0) + kNs, kPlayHz, &why));
        QCOMPARE(why, gone);
        radio->worker = nullptr;
    }

    // The acceptance case: a carrier kept at 7.100 MHz, played in USB at
    // 7.099 MHz through a real WDSP receive channel, is a 1 kHz tone. The
    // pan has moved 5 kHz since (the live centre is 7.090 MHz), so the past
    // is moved to it, and the slice's own shift (its frequency less the live
    // centre, as RadioModel sets it) finds the carrier where it was.
    void aPastCarrierIsAOneKilohertzToneInUsb()
    {
#ifndef HAVE_WDSP
        QSKIP("Built without WDSP.");
#else
        RecordingService svc(nullptr);
        FakeRadio* radio = prepare(svc);
        keepCarrier(svc, 6);
        constexpr qint64 kLiveCentreHz = 7090000;
        radio->centreHz = kLiveCentreHz;
        const DsrRingFile& ring = svc.dsrRecorder()->ring();
        // The replay is read on the recordings worker and pulled here, block
        // by block, as RxDspWorker's switch pulls it.
        DsrReplay replay(0, 0, kRate, ring.oldestUtcNs(0) + kNs / 4, radio->receiverCentreHz(0));
        svc.runOnWorker([&replay, &svc] { replay.fill(svc.dsrRecorder()->ring()); });
        svc.waitForWorker();
        QVERIFY(replay.primed());

        WdspEngine engine;
        engine.m_initialized = true;   // friend access (NEREUS_BUILD_TESTS)
        RxChannel* rx = engine.createRxChannel(0, kInSize, 4096, kRate, 48000, 48000);
        QVERIFY(rx != nullptr);
        rx->setMode(DSPMode::USB);
        rx->setFilterFreqs(100.0, 3000.0);
        // From Thetis radio.cs:1419 [v2.10.3.15], as RadioModel pushes it:
        // SetRXAShiftFreq receives +(freq - center).
        rx->setShiftFrequency(double(kPlayHz - kLiveCentreHz));
        rx->setActive(true);

        constexpr int kOutSize = 64;
        constexpr int kBlocks = 3000;          // 4 s of the past
        constexpr int kSettleBlocks = 1500;    // 2 s for AGC and filters
        std::vector<float> inI(kInSize);
        std::vector<float> inQ(kInSize);
        std::vector<float> outI(kInSize);
        std::vector<float> outQ(kInSize);
        std::vector<float> audio;
        for (int block = 0; block < kBlocks; ++block) {
            if (block % 40 == 0) {
                svc.runOnWorker([&replay, &svc] { replay.fill(svc.dsrRecorder()->ring()); });
                svc.waitForWorker();
            }
            QVERIFY(replay.pull(inI.data(), inQ.data(), kInSize));
            std::fill(outI.begin(), outI.end(), 0.0f);
            std::fill(outQ.begin(), outQ.end(), 0.0f);
            rx->processIq(inI.data(), inQ.data(), outI.data(), outQ.data(), kInSize, kOutSize);
            if (block >= kSettleBlocks) {
                audio.insert(audio.end(), outI.begin(), outI.begin() + kOutSize);
            }
        }
        QCOMPARE(replay.underruns(), quint64(0));
        // The tone's frequency from its zero crossings, and its level.
        int crossings = 0;
        double energy = 0.0;
        for (size_t k = 1; k < audio.size(); ++k) {
            if ((audio[k - 1] < 0.0f) != (audio[k] < 0.0f)) {
                ++crossings;
            }
            energy += double(audio[k]) * double(audio[k]);
        }
        const double seconds = double(audio.size()) / 48000.0;
        const double toneHz = crossings / 2.0 / seconds;
        const double rms = std::sqrt(energy / double(audio.size()));
        QVERIFY2(rms > 1e-3, qPrintable(QStringLiteral("rms %1").arg(rms)));
        QVERIFY2(std::abs(toneHz - 1000.0) < 10.0, qPrintable(QStringLiteral("tone %1 Hz").arg(toneHz)));
        rx->setActive(false);
#endif
    }
};

QTEST_MAIN(TestDsrRewind)
#include "tst_dsr_rewind.moc"
