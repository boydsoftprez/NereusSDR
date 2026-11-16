// =================================================================
// tests/tst_dsr_past_rows.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test. The past picture (spec 3
// "Rewind picture", "Moving through hours", 6.7, A12; recording plan Task
// 23): past waterfall rows and a past spectrum frame drawn from the Digital
// Spectrum Recorder's ring file with the pan's own FFT settings put a kept
// carrier in the bin the live spectrum puts it in; rows come newest first,
// timestamped, only for what is kept, and in chunks; the hours map gives one
// entry per kept hour, silence near 0 and carriers higher; the drawing runs
// on the recordings worker; bad requests are refused in plain words.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-16 - Created. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code. Recording plan Task 23.
// =================================================================

#include <QtTest/QtTest>

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QLoggingCategory>
#include <QRandomGenerator>
#include <QTemporaryDir>
#include <QThread>
#include <QTimeZone>

#include "core/FFTEngine.h"
#include "core/recordings/RecordingService.h"
#include "core/recordings/RecordingSettings.h"
#include "core/recordings/dsr/DsrRecorder.h"
#include "core/recordings/dsr/DsrReplay.h"
#include "core/recordings/dsr/DsrRingFile.h"
#include "core/spectrum/SpectrumReducer.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <numbers>
#include <vector>

using namespace NereusSDR;

namespace {

constexpr qint64 kNs = DsrRingFile::kNsPerSecond;
const qint64 kStartNs = QDateTime(QDate(2026, 11, 16), QTime(10, 0, 0), QTimeZone::UTC).toMSecsSinceEpoch()
                        * 1000000LL;
constexpr int kRate = 192000;
constexpr qint64 kKeptCentreHz = 7095000;
constexpr qint64 kCarrierHz = 7100123;      // between bins of a 4096-point FFT
constexpr float kCarrierAmplitude = 0.5f;
constexpr int kFftSize = 4096;
constexpr int kWindow = 1;                   // WindowFunction::BlackmanHarris4

DsrPanView panView()
{
    DsrPanView v;
    v.fftSize = kFftSize;
    v.windowType = kWindow;
    v.centreHz = 7097000;
    v.spanHz = 48000;
    return v;
}

// `frames` of a carrier at `carrierHz` (0: none) on noise of `noise`, seen
// around `centreHz` at `rate`, as the ring keeps it (16-bit).
std::vector<qint16> keptIq(int frames, int rate, qint64 centreHz, qint64 carrierHz, float amplitude,
                           float noise, quint32 seed)
{
    QRandomGenerator rng(seed);
    std::vector<qint16> iq(static_cast<size_t>(frames) * 2);
    const double step = 2.0 * std::numbers::pi * double(carrierHz - centreHz) / double(rate);
    for (int k = 0; k < frames; ++k) {
        double i = noise * (rng.generateDouble() * 2.0 - 1.0);
        double q = noise * (rng.generateDouble() * 2.0 - 1.0);
        // In the order the spectrum shows it at carrierHz: FFTEngine::feedIQ
        // swaps I and Q for the display.
        if (carrierHz != 0) {
            i += amplitude * std::sin(step * k);
            q += amplitude * std::cos(step * k);
        }
        iq[size_t(2 * k)] = qint16(std::lround(std::clamp(i, -1.0, 1.0) * DsrReplay::kIqScale));
        iq[size_t(2 * k + 1)] = qint16(std::lround(std::clamp(q, -1.0, 1.0) * DsrReplay::kIqScale));
    }
    return iq;
}

bool appendBlock(DsrRingFile& ring, qint64 utcNs, int rate, qint64 centreHz, const std::vector<qint16>& iq)
{
    DsrBlockHeader h;
    h.receiver = 0;
    h.utcNs = utcNs;
    h.sampleRateHz = rate;
    h.centreHz = centreHz;
    return ring.append(h, iq.data(), int(iq.size() / 2));
}

int peakIndex(const QVector<float>& bins)
{
    return int(std::max_element(bins.cbegin(), bins.cend()) - bins.cbegin());
}

// The live spectrum: the spectrum thread's engine fed the same I/Q.
QVector<float> liveFrame(const std::vector<qint16>& iq, int rate)
{
    FFTEngine engine(0);
    QVector<float> frame;
    QObject::connect(&engine, &FFTEngine::fftReady, &engine,
                     [&frame](int, const QVector<float>& bins) { frame = bins; }, Qt::DirectConnection);
    engine.setFftSize(kFftSize);
    engine.setWindowFunction(static_cast<WindowFunction>(kWindow));
    engine.setSampleRate(double(rate));
    QVector<float> feed((kFftSize + 1) * 2);
    for (int k = 0; k < feed.size(); ++k) {
        feed[k] = float(iq[size_t(k)]) / DsrReplay::kIqScale;
    }
    engine.feedIQ(feed);
    return frame;
}

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

// The radio as the past picture sees it: slice A on receiver 0.
class FakeRadio final : public DsrRewindHost {
public:
    bool sliceOpen(int slice) const override { return slice == 0; }
    qint64 sliceFrequencyHz(int) const override { return kCarrierHz; }
    void setSliceFrequencyHz(int, qint64) override {}
    int sliceReceiver(int slice) const override { return slice == 0 ? 0 : -1; }
    qint64 receiverCentreHz(int receiver) const override { return receiver == 0 ? kKeptCentreHz : 0; }
    int receiverRateHz(int receiver) const override { return receiver == 0 ? kRate : 0; }
    bool sliceOnDiversity(int) const override { return false; }
    bool feedSlice(int, std::shared_ptr<DsrReplay>) override { return false; }
};

}  // namespace

class TestDsrPastRows : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    QString ringPath() const
    {
        return m_dir.filePath(QString::fromLatin1(QTest::currentTestFunction()) + QStringLiteral(".ndsr"));
    }

    // `seconds` one-second blocks of the carrier around kKeptCentreHz.
    void keepSeconds(DsrRingFile& ring, int seconds)
    {
        for (int s = 0; s < seconds; ++s) {
            const std::vector<qint16> iq =
                keptIq(kRate, kRate, kKeptCentreHz, kCarrierHz, kCarrierAmplitude, 0.001f, quint32(s + 1));
            QVERIFY(appendBlock(ring, kStartNs + qint64(s) * kNs, kRate, kKeptCentreHz, iq));
        }
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        // The FFT engine's plan notes are not this test's output.
        QLoggingCategory::setFilterRules(QStringLiteral("nereus.dsp.info=false\nnereus.memlock.info=false"));
    }

    // A kept carrier's peak lands in the bin the live spectrum puts it in
    // for the pan's span and FFT size; the layout is the live crop.
    void aPastCarrierPeaksInTheLiveSpectrumsBin()
    {
        DsrRingFile ring;
        QString error;
        QVERIFY2(ring.open(ringPath(), 64LL * 1000000LL, &error), qPrintable(error));
        keepSeconds(ring, 2);

        // The live spectrum of the same I/Q, cropped to the pan.
        const std::vector<qint16> iq =
            keptIq(kRate, kRate, kKeptCentreHz, kCarrierHz, kCarrierAmplitude, 0.001f, 1);
        const QVector<float> live = liveFrame(iq, kRate);
        QCOMPARE(live.size(), kFftSize);
        ReducerConfig cfg;
        cfg.centreHz = double(panView().centreHz);
        cfg.spanHz = double(panView().spanHz);
        cfg.streamCentreHz = double(kKeptCentreHz);
        cfg.sampleRateHz = double(kRate);
        const auto [first, last] = SpectrumReducer::visibleBinRange(kFftSize, cfg);
        const int livePeak = peakIndex(live);
        QVERIFY(livePeak >= first && livePeak <= last);

        DsrPastRenderer renderer;
        const DsrPastFrames rows =
            renderer.rows(ring, 0, kRate, kKeptCentreHz, panView(), kStartNs, kStartNs, 1);
        QCOMPARE(rows.binCount, last - first + 1);
        QCOMPARE(rows.rows.size(), 1);
        QCOMPARE(rows.rows.first().size(), rows.binCount);
        QCOMPARE(peakIndex(rows.rows.first()), livePeak - first);
        QCOMPARE(rows.hzPerBin, double(kRate) / kFftSize);
        QVERIFY(std::abs(rows.firstBinHz + peakIndex(rows.rows.first()) * rows.hzPerBin - double(kCarrierHz))
                <= rows.hzPerBin);

        // One spectrum frame at the same moment is the same row.
        DsrPastFrames frame;
        QVERIFY(renderer.spectrum(ring, 0, kRate, kKeptCentreHz, panView(), kStartNs, &frame));
        QCOMPARE(frame.rows.size(), 1);
        QCOMPARE(frame.utcNs.first(), kStartNs);
        QCOMPARE(peakIndex(frame.rows.first()), livePeak - first);

        // The receiver has moved since: the carrier is still drawn at its
        // own frequency in the pan's layout now.
        const DsrPastFrames moved =
            renderer.rows(ring, 0, kRate, kKeptCentreHz - 5000, panView(), kStartNs, kStartNs, 1);
        QCOMPARE(moved.rows.size(), 1);
        const int movedPeak = peakIndex(moved.rows.first());
        QVERIFY(std::abs(moved.firstBinHz + movedPeak * moved.hzPerBin - double(kCarrierHz))
                <= 1.5 * moved.hzPerBin);
    }

    // Rows come back newest first, each with its time, and a request that
    // reaches beyond the ring returns only the rows over what is kept.
    void rowsComeNewestFirstAndOnlyWhatIsKept()
    {
        DsrRingFile ring;
        QString error;
        QVERIFY2(ring.open(ringPath(), 64LL * 1000000LL, &error), qPrintable(error));
        keepSeconds(ring, 5);
        const qint64 oldest = ring.oldestUtcNs(0);
        const qint64 newest = ring.newestUtcNs(0);

        const qint64 from = kStartNs - 10 * kNs;
        const qint64 to = kStartNs + 9 * kNs;
        const int rowCount = 39;   // every half second
        DsrPastRenderer renderer;
        const DsrPastFrames rows = renderer.rows(ring, 0, kRate, kKeptCentreHz, panView(), from, to, rowCount);

        int keptRows = 0;
        const qint64 step = (to - from) / (rowCount - 1);
        for (int i = 0; i < rowCount; ++i) {
            const qint64 t = to - qint64(i) * step;
            if (t >= oldest && t < newest) {
                ++keptRows;
            }
        }
        QCOMPARE(keptRows, 10);
        QCOMPARE(rows.rows.size(), keptRows);
        QCOMPARE(rows.utcNs.size(), keptRows);
        QCOMPARE(rows.rowsLeft, 0);
        for (int i = 0; i < rows.utcNs.size(); ++i) {
            QVERIFY(rows.utcNs.at(i) >= oldest && rows.utcNs.at(i) < newest);
            if (i > 0) {
                QVERIFY(rows.utcNs.at(i) < rows.utcNs.at(i - 1));
            }
            QCOMPARE(rows.rows.at(i).size(), rows.binCount);
        }

        // Nothing kept at all in the stretch: no rows.
        const DsrPastFrames none =
            renderer.rows(ring, 0, kRate, kKeptCentreHz, panView(), kStartNs - 20 * kNs, kStartNs - 10 * kNs, 5);
        QVERIFY(none.rows.isEmpty());
        DsrPastFrames frame;
        QVERIFY(!renderer.spectrum(ring, 0, kRate, kKeptCentreHz, panView(), kStartNs - kNs, &frame));
    }

    // A large request comes in chunks, each within its byte budget; asking
    // on from nextToUtcNs gives the same rows as one request.
    void rowsComeInChunks()
    {
        DsrRingFile ring;
        QString error;
        QVERIFY2(ring.open(ringPath(), 64LL * 1000000LL, &error), qPrintable(error));
        keepSeconds(ring, 3);
        const qint64 from = kStartNs;
        const qint64 to = kStartNs + 2 * kNs;
        const int rowCount = 9;
        DsrPastRenderer renderer;
        const DsrPastFrames whole =
            renderer.rows(ring, 0, kRate, kKeptCentreHz, panView(), from, to, rowCount);
        QCOMPARE(whole.rows.size(), rowCount);

        const qint64 budget = qint64(whole.binCount) * 2 * 4;   // four rows an answer
        QList<qint64> times;
        QList<QVector<float>> chunked;
        qint64 chunkTo = to;
        int left = rowCount;
        int answers = 0;
        while (left > 0) {
            const DsrPastFrames part =
                renderer.rows(ring, 0, kRate, kKeptCentreHz, panView(), from, chunkTo, left, budget);
            QVERIFY(qint64(part.rows.size()) * part.binCount * 2 <= budget);
            times += part.utcNs;
            chunked += part.rows;
            left = part.rowsLeft;
            chunkTo = part.nextToUtcNs;
            ++answers;
            QVERIFY(answers <= rowCount);
        }
        QCOMPARE(answers, 3);
        QCOMPARE(times, whole.utcNs);
        QCOMPARE(chunked, whole.rows);
    }

    // Over a three-hour ring the hours map has three entries, newest first;
    // a silent hour is near 0 and an hour with carriers is higher.
    void threeKeptHoursShadedByActivity()
    {
        DsrRingFile ring;
        QString error;
        QVERIFY2(ring.open(ringPath(), 64LL * 1000000LL, &error), qPrintable(error));
        constexpr int kHoursRate = 48000;
        constexpr int kBlockFrames = 8192;
        constexpr qint64 kCentre = 14100000;
        quint32 seed = 100;
        for (int hour = 0; hour < 3; ++hour) {
            for (int quarter = 0; quarter < 4; ++quarter) {
                const qint64 t = kStartNs + qint64(hour) * DsrPastRenderer::kHourNs
                                 + qint64(quarter) * 15 * 60 * kNs;
                const qint64 carrier = hour == 1 ? kCentre + 3000 + quarter * 1000 : 0;
                const std::vector<qint16> iq =
                    keptIq(kBlockFrames, kHoursRate, kCentre, carrier, 0.3f, 0.01f, seed++);
                QVERIFY(appendBlock(ring, t, kHoursRate, kCentre, iq));
            }
        }
        DsrPastRenderer renderer;
        const QList<DsrHour> hours = renderer.hours(ring);
        QCOMPARE(hours.size(), 3);
        QCOMPARE(hours.at(0).startUtcNs, kStartNs + 2 * DsrPastRenderer::kHourNs);
        QCOMPARE(hours.at(1).startUtcNs, kStartNs + DsrPastRenderer::kHourNs);
        QCOMPARE(hours.at(2).startUtcNs, kStartNs);
        QVERIFY2(hours.at(0).activity < 0.1, qPrintable(QString::number(hours.at(0).activity)));
        QVERIFY2(hours.at(2).activity < 0.1, qPrintable(QString::number(hours.at(2).activity)));
        QVERIFY2(hours.at(1).activity > 0.5, qPrintable(QString::number(hours.at(1).activity)));

        DsrRingFile empty;
        QVERIFY(empty.open(m_dir.filePath(QStringLiteral("empty.ndsr")), 1000000, &error));
        QVERIFY(renderer.hours(empty).isEmpty());
    }

    // The wire form carries times exactly and bins to a tenth of a dB.
    void theWireFormRoundTrips()
    {
        const QList<qint64> times{kStartNs + 123456789, kStartNs, 1};
        QCOMPARE(DsrPastRenderer::unpackTimes(DsrPastRenderer::packTimes(times)), times);
        const QList<QVector<float>> rows{{-200.0f, -120.04f, -73.26f}, {0.0f, 12.5f, -3276.9f}};
        const QList<QVector<float>> back = DsrPastRenderer::unpackRows(DsrPastRenderer::packRows(rows), 3);
        QCOMPARE(back.size(), 2);
        QCOMPARE(back.at(0).at(0), -200.0f);
        QVERIFY(std::abs(back.at(0).at(1) - -120.0f) < 0.051f);
        QVERIFY(std::abs(back.at(0).at(2) - -73.3f) < 0.051f);
        QCOMPARE(back.at(1).at(1), 12.5f);
        QCOMPARE(back.at(1).at(2), -3276.8f);   // the wire's floor
    }

    // Through the service: checked on the main thread, drawn on the
    // recordings worker (never the caller's thread), answered here; bad
    // requests refused at once in plain words.
    void theServiceDrawsOnTheRecordingsWorker()
    {
        const QString ssd = m_dir.filePath(QStringLiteral("ssd-service"));
        QDir().mkpath(ssd);
        std::atomic<qint64> clockMs{kStartNs / 1000000};
        RecordingService svc(nullptr);
        svc.setClockForTest([&clockMs] { return clockMs.load(); });
        svc.setDsrVolumesForTest([&ssd] {
            return QList<RecordingVolume>{volume(QDir::rootPath(), true), volume(ssd, false)};
        });
        RecordingSettings s = svc.settings();
        s.disk = m_dir.filePath(QStringLiteral("recordings"));
        QDir().mkpath(s.disk);
        s.format = AudioFileFormat::Wav;
        s.dsrEnabled = true;
        s.dsrDisk = ssd;
        s.dsrSizeLimitGb = 1;
        svc.setSettings(s);
        svc.setDsrRewindHostForTest(std::make_unique<FakeRadio>());

        // Refused before the recorder keeps anything: nothing is kept.
        QString refusal;
        bool answered = false;
        const auto expectRefusal = [&](const char* words) {
            QVERIFY(answered);
            QCOMPARE(refusal, QString::fromLatin1(words));
            answered = false;
        };
        const RecordingService::PastFramesDone refused = [&](bool ok, const QString& why, const DsrPastFrames&) {
            answered = true;
            refusal = ok ? QString() : why;
        };
        svc.pastRows(0, panView(), kStartNs, kStartNs + kNs, 4, refused);
        expectRefusal(RecordingService::kRewindGoneReason);

        svc.setRadioStreaming(true);
        svc.dsrRecorder()->setStream(0, kKeptCentreHz, kRate);
        for (int sec = 0; sec < 3; ++sec) {
            const std::vector<qint16> iq =
                keptIq(kRate, kRate, kKeptCentreHz, kCarrierHz, kCarrierAmplitude, 0.001f, quint32(sec + 7));
            std::vector<float> chunk(iq.size());
            for (size_t k = 0; k < iq.size(); ++k) {
                chunk[k] = float(iq[k]) / DsrReplay::kIqScale;
            }
            constexpr int kChunkFrames = 4800;   // what the tap takes at once
            for (int at = 0; at < kRate; at += kChunkFrames) {
                svc.dsrRecorder()->consumeIq(0, chunk.data() + size_t(at) * 2, kChunkFrames);
            }
            clockMs += 1000;
            svc.pumpForTest();
        }
        const DsrRingFile& ring = svc.dsrRecorder()->ring();
        QVERIFY(ring.blockCount() >= 2);
        const qint64 oldest = ring.oldestUtcNs(0);

        // Bad requests, in plain words, at once.
        DsrPanView bad = panView();
        bad.fftSize = 3000;
        svc.pastRows(0, bad, oldest, oldest + kNs, 4, refused);
        expectRefusal(RecordingService::kPastSizeReason);
        bad = panView();
        bad.windowType = static_cast<int>(WindowFunction::Count);
        svc.pastSpectrum(0, bad, oldest, refused);
        expectRefusal(RecordingService::kPastWindowReason);
        bad = panView();
        bad.spanHz = kRate + 1;
        svc.pastRows(0, bad, oldest, oldest + kNs, 4, refused);
        expectRefusal(RecordingService::kPastSpanReason);
        svc.pastRows(0, panView(), oldest, oldest + kNs, 0, refused);
        expectRefusal(RecordingService::kPastRowsReason);
        svc.pastRows(3, panView(), oldest, oldest + kNs, 4, refused);
        expectRefusal(RecordingService::kRewindNoSliceReason);

        // Drawn on the worker, answered on this thread.
        bool done = false;
        DsrPastFrames got;
        QThread* answeredOn = nullptr;
        svc.pastRows(0, panView(), oldest, oldest + kNs, 3,
                     [&](bool ok, const QString&, const DsrPastFrames& rows) {
                         done = ok;
                         got = rows;
                         answeredOn = QThread::currentThread();
                     });
        QTRY_VERIFY(done);
        QCOMPARE(answeredOn, QThread::currentThread());
        QThread* const drawnOn = svc.pastRenderThreadForTest();
        QVERIFY(drawnOn != nullptr);
        QVERIFY(drawnOn != QThread::currentThread());
        QCOMPARE(drawnOn->objectName(), QStringLiteral("NereusRecordings"));
        QCOMPARE(got.slice, 0);
        QCOMPARE(got.rows.size(), 3);

        bool spectrumDone = false;
        svc.pastSpectrum(0, panView(), oldest, [&](bool ok, const QString&, const DsrPastFrames& frame) {
            spectrumDone = ok && frame.rows.size() == 1;
        });
        QTRY_VERIFY(spectrumDone);

        bool hoursDone = false;
        svc.pastHours([&](const QList<DsrHour>& hours) { hoursDone = hours.size() == 1; });
        QTRY_VERIFY(hoursDone);
    }
};

QTEST_MAIN(TestDsrPastRows)
#include "tst_dsr_past_rows.moc"
