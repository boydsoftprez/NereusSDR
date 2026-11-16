// =================================================================
// tests/tst_dsr_recorder.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test. The Digital Spectrum Recorder on
// the Core (spec 3, 6.7, section 7, A10, A14, A17): the I/Q tap copies
// into a lock-free ring and returns, with no allocation and no disk, and
// a stalled writer drops with a counted overflow; the recordings worker
// writes one-second blocks; your transmit is marked with its audio; it is
// on by default, never on an SD card, and stops when its disk goes while
// recordings go on; RadioModel::forkIqToTaps reaches it.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-16 - Created. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code. Recording plan Task 21.
//   2026-11-16 - The service's default by host: off in a desktop window
//                 running its own radio, on once it is told it is a Core,
//                 a saved value kept. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code. Recording plan Task 10b.
// =================================================================

#include <QtTest/QtTest>

#include <QDir>
#include <QFileInfo>
#include <QScopeGuard>
#include <QSemaphore>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimeZone>

#include "core/AppSettings.h"
#include "core/recordings/RecordingService.h"
#include "core/recordings/RecordingSettings.h"
#include "core/recordings/dsr/DsrRecorder.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <atomic>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <new>
#include <vector>

// ---- An allocation counter for the tap's no-allocation check (A17). The
// tap is inline in DsrRecorder.h, so it is compiled here and its
// allocations, if any, come through these.
namespace {
thread_local bool g_countAllocations = false;
std::atomic<int> g_allocations{0};

void* countedAlloc(std::size_t n)
{
    if (g_countAllocations) {
        g_allocations.fetch_add(1, std::memory_order_relaxed);
    }
    void* p = std::malloc(n == 0 ? 1 : n);
    if (p == nullptr) {
        std::abort();
    }
    return p;
}
}  // namespace

void* operator new(std::size_t n) { return countedAlloc(n); }
void* operator new[](std::size_t n) { return countedAlloc(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

using namespace NereusSDR;

namespace {

const QDateTime kStartUtc(QDate(2026, 11, 16), QTime(14, 32, 0), QTimeZone::UTC);
constexpr int kRate = 192000;
constexpr qint64 kCentreHz = 7100000;
constexpr int kChunkFrames = 4800;   // 25 ms at 192 kHz

RecordingVolume volume(const QString& root, bool own, bool sd)
{
    RecordingVolume v;
    v.rootPath = root;
    v.displayName = QFileInfo(root).fileName();
    v.freeBytes = 500LL * 1000000000LL;
    v.totalBytes = 1000LL * 1000000000LL;
    v.isCoreOwnDisk = own;
    v.isSdCard = sd;
    return v;
}

}  // namespace

class TestDsrRecorder : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    std::atomic<qint64> m_clockMs{0};
    bool m_ssdPresent = true;

    // Each test its own SSD, so no test takes up another's ring file.
    QString ssdRoot() const
    {
        return m_dir.filePath(QStringLiteral("ssd-") + QString::fromLatin1(QTest::currentTestFunction()));
    }

    // The Core's own disk and a USB SSD; the recorder on the SSD at 1 GB.
    void prepare(RecordingService& svc, bool ownDiskIsSd = false)
    {
        QDir().mkpath(ssdRoot());
        m_ssdPresent = true;
        m_clockMs = kStartUtc.toMSecsSinceEpoch();
        svc.setClockForTest([this] { return m_clockMs.load(); });
        svc.setDsrVolumesForTest([this, ownDiskIsSd] {
            QList<RecordingVolume> list{volume(QDir::rootPath(), true, ownDiskIsSd)};
            if (m_ssdPresent) {
                list.append(volume(ssdRoot(), false, false));
            }
            return list;
        });
        RecordingSettings s = svc.settings();
        s.disk = m_dir.filePath(QStringLiteral("recordings"));
        QDir().mkpath(s.disk);
        s.format = AudioFileFormat::Wav;
        s.dsrEnabled = true;
        s.dsrDisk = ssdRoot();
        s.dsrSizeLimitGb = 1;
        svc.setSettings(s);
    }

    QString ringPathOf(const RecordingService& svc) const
    {
        return QDir(svc.settings().dsrFolder()).filePath(QString::fromLatin1(RecordingService::kDsrFileName));
    }

    // One second of receiver 0 through the tap, in 25 ms pieces, then the
    // clock moves on and the worker writes.
    void feedSecond(RecordingService& svc, const std::vector<float>& chunk)
    {
        for (int i = 0; i < kRate / kChunkFrames; ++i) {
            svc.dsrRecorder()->consumeIq(0, chunk.data(), kChunkFrames);
        }
        m_clockMs += 1000;
        svc.pumpForTest();
    }

    static std::vector<float> chunkOf(float value)
    {
        return std::vector<float>(static_cast<size_t>(kChunkFrames) * 2, value);
    }

private slots:
    void initTestCase() { QVERIFY(m_dir.isValid()); }

    // Spec 6.7: on by default, 10 GB, on the Core's own disk.
    void onByDefaultAtTenGigabytesOnTheCoresOwnDisk()
    {
        const RecordingSettings fresh;
        QVERIFY(fresh.dsrEnabled);
        QCOMPARE(fresh.dsrSizeLimitGb, 10);
        QVERIFY(fresh.dsrDisk.isEmpty());

        RecordingService svc(nullptr);
        m_clockMs = kStartUtc.toMSecsSinceEpoch();
        svc.setClockForTest([this] { return m_clockMs.load(); });
        svc.setDsrVolumesForTest([] { return QList<RecordingVolume>{volume(QDir::rootPath(), true, false)}; });
        RecordingSettings s = svc.settings();
        s.dsrEnabled = true;
        s.dsrDisk.clear();
        s.dsrSizeLimitGb = 10;
        svc.setSettings(s);
        // Nothing runs until the radio streams.
        QVERIFY(!svc.dsrStatus().recording);
        svc.setRadioStreaming(true);
        const DsrStatus st = svc.dsrStatus();
        QVERIFY(st.enabled);
        QVERIFY(st.recording);
        QCOMPARE(st.reason, QString());
        QCOMPARE(st.sizeLimitBytes, 10LL * RecordingService::kBytesPerGb);
        svc.pumpForTest();
        QVERIFY(QFileInfo::exists(ringPathOf(svc)));
        svc.setRadioStreaming(false);
        QVERIFY(!svc.dsrStatus().recording);
        QCOMPARE(svc.dsrStatus().reason, QString());
    }

    // Recording plan Task 10b (JJ, 2026-11-16): a desktop window running
    // its own radio starts with it off; a Core starts with it on; a saved
    // value is kept.
    void theDefaultFollowsWhereItRuns()
    {
        AppSettings::instance().clear();
        {
            RecordingService svc(nullptr);
            svc.setDsrVolumesForTest([] { return QList<RecordingVolume>{volume(QDir::rootPath(), true, false)}; });
            QVERIFY(!svc.settings().dsrEnabled);
            svc.setRadioStreaming(true);
            QVERIFY(!svc.dsrStatus().enabled);
            QVERIFY(!svc.dsrStatus().recording);
            svc.setHost(RecordingSettings::Host::Core);
            QVERIFY(svc.settings().dsrEnabled);
            QVERIFY(svc.dsrStatus().enabled);
            QVERIFY(svc.dsrStatus().recording);
            svc.setRadioStreaming(false);
        }
        // Nothing was saved by either: a fresh file still says nothing.
        QVERIFY(!AppSettings::instance().contains(QStringLiteral("DsrEnabled")));
        AppSettings::instance().setValue(QStringLiteral("DsrEnabled"), QStringLiteral("True"));
        {
            RecordingService svc(nullptr);
            QVERIFY(svc.settings().dsrEnabled);
        }
        AppSettings::instance().clear();
    }

    // A10, spec 6.7: one receiver at 192 kHz for 60 fake seconds through
    // the tap is 60 blocks of 192000 frames, about 46 MB.
    void sixtyFakeSecondsAreSixtyBlocks()
    {
        RecordingService svc(nullptr);
        prepare(svc);
        svc.setRadioStreaming(true);
        QVERIFY(svc.dsrStatus().recording);
        svc.dsrRecorder()->setStream(0, kCentreHz, kRate);
        QSignalSpy changed(&svc, &RecordingService::dsrChanged);
        const std::vector<float> chunk = chunkOf(0.25f);
        for (int sec = 0; sec < 60; ++sec) {
            feedSecond(svc, chunk);
        }
        const DsrRingFile& ring = svc.dsrRecorder()->ring();
        QCOMPARE(ring.blockCount(), 60);
        QVERIFY(ring.fileBytes() > 46000000 && ring.fileBytes() < 46300000);
        QCOMPARE(ring.newestUtcNs(0) - ring.oldestUtcNs(0), 60LL * DsrRingFile::kNsPerSecond);
        QCOMPARE(svc.dsrRecorder()->overflows(), quint64(0));
        // 0.25 as 16-bit, and the centre the receiver streams at.
        std::vector<qint16> iq(4);
        DsrBlockHeader hdr;
        QCOMPARE(ring.read(0, ring.oldestUtcNs(0) + DsrRingFile::kNsPerSecond, iq.data(), 2, &hdr), 2);
        QCOMPARE(iq[0], qint16(8192));
        QCOMPARE(hdr.centreHz, kCentreHz);
        QCOMPARE(hdr.sampleRateHz, kRate);
        QCOMPARE(int(hdr.transmit), 0);
        // The figures reach the document, not every second.
        QVERIFY(changed.count() >= 1);
        QVERIFY(changed.count() < 60);

        svc.stopAll();
        const DsrStatus st = svc.dsrStatus();
        QVERIFY(!st.recording);
        QCOMPARE(st.newestUtcMs - st.oldestUtcMs, qint64(60000));
        QVERIFY(std::abs(st.hoursHeld() - 60.0 / 3600.0) < 1e-9);
        QVERIFY(st.usedBytes > 46000000);
        QVERIFY(QFileInfo(ringPathOf(svc)).size() > 46000000);
    }

    // A17: the tap copies and returns. It allocates nothing, touches no
    // disk while the writer is stalled, and drops what does not fit with a
    // counted overflow; the writer goes on once it is free.
    void theTapNeverWaitsForTheWriter()
    {
        RecordingService svc(nullptr);
        prepare(svc);
        svc.setRadioStreaming(true);
        svc.dsrRecorder()->setStream(0, kCentreHz, kRate);
        svc.pumpForTest();   // the file is open
        const qint64 fileBefore = svc.dsrRecorder()->ring().fileBytes();

        // Stall the writer: the worker waits here until released.
        QSemaphore entered;
        QSemaphore release;
        svc.runOnWorker([&entered, &release] {
            entered.release();
            release.acquire();
        });
        entered.acquire();
        // A failed check below still frees the worker, so the service can go.
        auto freeWorker = qScopeGuard([&release] { release.release(); });

        constexpr int kPiece = 1024;   // frames
        const std::vector<float> piece(static_cast<size_t>(kPiece) * 2, 0.1f);
        const int fits = DsrRecorder::kIqRingFloats / (kPiece * 2);
        g_allocations = 0;
        g_countAllocations = true;
        for (int i = 0; i < fits + 76; ++i) {
            svc.dsrRecorder()->consumeIq(0, piece.data(), kPiece);
        }
        g_countAllocations = false;
        QCOMPARE(g_allocations.load(), 0);
        QCOMPARE(svc.dsrRecorder()->overflows(0), quint64(76));
        QCOMPARE(svc.dsrRecorder()->ring().fileBytes(), fileBefore);
        QCOMPARE(svc.dsrRecorder()->ring().blockCount(), 0);

        freeWorker.dismiss();
        release.release();
        m_clockMs += 6000;
        svc.pumpForTest();
        // 1024 pieces of 1024 frames: five whole seconds, the rest held.
        QCOMPARE(svc.dsrRecorder()->ring().blockCount(), 5);
        QCOMPARE(svc.dsrStatus().overflows, quint64(76));
        QVERIFY(svc.dsrStatus().recording);
    }

    // Spec 3, "Your transmit in the recorder": a second you transmit is
    // marked, with your audio beside it.
    void yourTransmitIsMarkedWithItsAudio()
    {
        RecordingService svc(nullptr);
        prepare(svc);
        svc.setRadioStreaming(true);
        svc.dsrRecorder()->setStream(0, kCentreHz, kRate);
        const std::vector<float> chunk = chunkOf(0.1f);
        feedSecond(svc, chunk);
        svc.setTransmitting(true);
        const std::vector<float> voice(4800, 0.5f);
        for (int i = 0; i < 10; ++i) {
            svc.consumeMonitor(voice.data(), 4800, 48000);
        }
        feedSecond(svc, chunk);
        svc.setTransmitting(false);
        feedSecond(svc, chunk);

        const DsrRingFile& ring = svc.dsrRecorder()->ring();
        QCOMPARE(ring.blockCount(), 3);
        const qint64 first = ring.oldestUtcNs(0);
        const qint64 second = first + DsrRingFile::kNsPerSecond;
        const QList<QPair<qint64, bool>> marks = ring.transmitMarks(first, first + 3 * DsrRingFile::kNsPerSecond);
        QCOMPARE(marks.size(), 2);
        QCOMPARE(marks.at(0), qMakePair(second, true));
        QCOMPARE(marks.at(1), qMakePair(second + DsrRingFile::kNsPerSecond, false));
        std::vector<qint16> audio(48000);
        int rate = 0;
        QCOMPARE(ring.readTransmitAudio(second, audio.data(), 48000, &rate), 48000);
        QCOMPARE(rate, 48000);
        QCOMPARE(audio[100], qint16(16384));
    }

    // A14 at the Core: when every disk is an SD card the recorder stays
    // off and says so; a chosen SD card beside a better disk says which.
    void anSdCardIsNeverUsed()
    {
        bool ssdToo = false;
        const QString card = m_dir.filePath(QStringLiteral("card"));
        QDir().mkpath(card);
        RecordingService svc(nullptr);
        m_clockMs = kStartUtc.toMSecsSinceEpoch();
        svc.setClockForTest([this] { return m_clockMs.load(); });
        svc.setDsrVolumesForTest([&ssdToo, card, this] {
            QList<RecordingVolume> list{volume(QDir::rootPath(), true, true), volume(card, false, true)};
            if (ssdToo) {
                list.append(volume(ssdRoot(), false, false));
            }
            return list;
        });
        RecordingSettings s = svc.settings();
        s.dsrEnabled = true;
        s.dsrDisk.clear();
        svc.setSettings(s);
        svc.setRadioStreaming(true);
        DsrStatus st = svc.dsrStatus();
        QVERIFY(st.enabled);
        QVERIFY(!st.recording);
        QCOMPARE(st.reason, QStringLiteral("The only disk is an SD card."));

        s.dsrDisk = card;
        svc.setSettings(s);
        QCOMPARE(svc.dsrStatus().reason, QStringLiteral("The only disk is an SD card."));
        QVERIFY(!svc.dsrStatus().recording);
        // Nothing is written to the card.
        QVERIFY(!QFileInfo::exists(ringPathOf(svc)));

        ssdToo = true;
        QDir().mkpath(ssdRoot());
        svc.setSettings(s);
        st = svc.dsrStatus();
        QVERIFY(!st.recording);
        QCOMPARE(st.reason, QStringLiteral("The recorder's disk is an SD card. Choose a USB SSD or NVMe drive."));

        // Recordings may still use the card.
        QVERIFY(svc.store() != nullptr);
        s.dsrDisk = ssdRoot();
        svc.setSettings(s);
        QVERIFY(svc.dsrStatus().recording);
        QCOMPARE(svc.dsrStatus().reason, QString());
        svc.setRadioStreaming(false);
    }

    // Section 7: a recorder disk that goes away stops the recorder with
    // "The recorder's disk is missing."; recordings keep working, and the
    // recorder comes back with the disk. RadioModel::forkIqToTaps feeds it.
    void aMissingDiskStopsOnlyTheRecorder()
    {
        RadioModel radio(RadioModel::Role::Local);
        RecordingService* svc = radio.recordingService();
        QVERIFY(svc);
        SliceModel* a = radio.sliceById(radio.addSlice());
        QVERIFY(a);
        a->setFrequency(7236400.0);
        a->setDspMode(DSPMode::LSB);
        prepare(*svc);
        svc->setRadioStreaming(true);
        QVERIFY(svc->dsrStatus().recording);
        svc->dsrRecorder()->setStream(0, kCentreHz, kRate);

        // The radio's I/Q fork reaches the recorder: one second, one block.
        const QVector<float> second(kRate * 2, 0.5f);
        radio.forkIqToTapsForTest(0, second);
        m_clockMs += 1000;
        svc->pumpForTest();
        QCOMPARE(svc->dsrRecorder()->ring().blockCount(), 1);

        QSignalSpy changed(svc, &RecordingService::dsrChanged);
        m_ssdPresent = false;
        svc->checkDsrDiskForTest();
        QVERIFY(!svc->dsrStatus().recording);
        QCOMPARE(svc->dsrStatus().reason, QStringLiteral("The recorder's disk is missing."));
        QVERIFY(changed.count() >= 1);
        // The tap takes nothing while it is off.
        radio.forkIqToTapsForTest(0, second);
        QCOMPARE(svc->dsrRecorder()->overflows(), quint64(0));

        // A recording starts, stops and is kept as before.
        QSignalSpy kept(svc, &RecordingService::kept);
        svc->startRecording(a->sliceIndex(), QString());
        svc->pumpForTest();
        m_clockMs += 2000;
        svc->stopRecording(a->sliceIndex(), QString());
        svc->pumpForTest();
        QTRY_COMPARE(kept.count(), 1);
        QCOMPARE(kept.at(0).at(0).value<KeptEvent>().reason, KeptReason::StoppedHere);

        // The disk comes back: so does the recorder, taking up its file.
        m_ssdPresent = true;
        svc->checkDsrDiskForTest();
        QVERIFY(svc->dsrStatus().recording);
        QCOMPARE(svc->dsrStatus().reason, QString());
        svc->pumpForTest();
        QCOMPARE(svc->dsrRecorder()->ring().blockCount(), 1);
    }
};

QTEST_GUILESS_MAIN(TestDsrRecorder)
#include "tst_dsr_recorder.moc"
