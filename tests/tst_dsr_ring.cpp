// =================================================================
// tests/tst_dsr_ring.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test. The Digital Spectrum Recorder's
// ring file (spec 3, 6.7, A10): one-second blocks of 16-bit I/Q, about
// 2.8 GB an hour per receiver at 192 kHz and in step at 384, 768 and
// 1536 kHz; the oldest erased at the size limit; the same samples read
// back at the right time and centre; your transmit marked with its audio
// beside it; and a restart that finds the oldest, the newest and where
// to write next.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-16 - Created. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code. Recording plan Task 21.
//   2026-11-16 - J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//                 Code. Final review fix: a write cut short before its
//                 header leaves no block a restart takes.
// =================================================================

#include <QtTest/QtTest>

#include <QFileInfo>
#include <QTemporaryDir>

#include "core/recordings/dsr/DsrRingFile.h"

#include <cmath>
#include <vector>

using namespace NereusSDR;

namespace {

constexpr qint64 kNs = DsrRingFile::kNsPerSecond;
// 2025-11-16 12:53:20 UTC.
constexpr qint64 kT0 = 1763297600LL * kNs;
constexpr qint64 kCentreHz = 7100000;

// A sample that names its block and frame, so a read from the wrong place
// shows.
qint16 sampleAt(int block, int frame, int part)
{
    const int v = (block * 7919 + frame * 3 + part) % 32767;
    return static_cast<qint16>(part == 0 ? v : -v);
}

void fillBlock(std::vector<qint16>& iq, int block, int frames)
{
    iq.resize(static_cast<size_t>(frames) * 2);
    for (int f = 0; f < frames; ++f) {
        iq[static_cast<size_t>(f) * 2] = sampleAt(block, f, 0);
        iq[static_cast<size_t>(f) * 2 + 1] = sampleAt(block, f, 1);
    }
}

DsrBlockHeader headerFor(int block, int rateHz, int receiver = 0, bool transmit = false)
{
    DsrBlockHeader h;
    h.receiver = static_cast<quint16>(receiver);
    h.utcNs = kT0 + qint64(block) * kNs;
    h.sampleRateHz = rateHz;
    h.centreHz = kCentreHz + receiver * 1000000;
    h.transmit = transmit ? 1 : 0;
    h.frames = static_cast<quint32>(rateHz);
    return h;
}

bool appendSeconds(DsrRingFile& ring, int from, int count, int rateHz, int receiver = 0)
{
    std::vector<qint16> iq;
    for (int b = from; b < from + count; ++b) {
        fillBlock(iq, b, rateHz);
        if (!ring.append(headerFor(b, rateHz, receiver), iq.data(), rateHz)) {
            return false;
        }
    }
    return true;
}

}  // namespace

class TestDsrRing : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    QString ringPath(const QString& name) const { return m_dir.filePath(name + QStringLiteral(".ndsr")); }

private slots:
    void initTestCase() { QVERIFY(m_dir.isValid()); }

    // A10, spec 6.7: one receiver at 192 kHz for 60 seconds is 60 blocks
    // of 192000 frames, about 46 MB, so about 2.8 GB an hour.
    void sixtySecondsAt192kHzIsSixtyBlocks()
    {
        DsrRingFile ring;
        QString error;
        QVERIFY2(ring.open(ringPath(QStringLiteral("sixty")), 10LL * 1000000000LL, &error), qPrintable(error));
        QCOMPARE(ring.fileBytes(), qint64(0));
        QVERIFY(appendSeconds(ring, 0, 60, 192000));
        QCOMPARE(ring.blockCount(), 60);
        const qint64 block = DsrRingFile::blockBytes(192000);
        QCOMPARE(block, qint64(770048));   // 64 + 192000 * 4, to the next 4096
        QCOMPARE(ring.fileBytes(), 59 * block + DsrRingFile::kHeaderBytes + 192000 * 4);
        QVERIFY(ring.fileBytes() > 46000000 && ring.fileBytes() < 46300000);
        const double gbPerHour = double(block) * 3600.0 / 1e9;
        QVERIFY2(gbPerHour > 2.7 && gbPerHour < 2.9, qPrintable(QString::number(gbPerHour)));
        QCOMPARE(ring.oldestUtcNs(0), kT0);
        QCOMPARE(ring.newestUtcNs(0), kT0 + 60 * kNs);
        QCOMPARE(ring.oldestUtcNs(1), qint64(0));
    }

    // Spec 6.7: 5.5, 11 and 22 GB an hour at 384, 768 and 1536 kHz; the
    // file grows by one block a second at each.
    void higherRatesScaleTheSame_data()
    {
        QTest::addColumn<int>("rateHz");
        QTest::addColumn<double>("gbPerHour");
        QTest::newRow("384 kHz") << 384000 << 5.5;
        QTest::newRow("768 kHz") << 768000 << 11.0;
        QTest::newRow("1536 kHz") << 1536000 << 22.0;
    }

    void higherRatesScaleTheSame()
    {
        QFETCH(int, rateHz);
        QFETCH(double, gbPerHour);
        DsrRingFile ring;
        QString error;
        QVERIFY2(ring.open(ringPath(QString::number(rateHz)), 10LL * 1000000000LL, &error), qPrintable(error));
        QVERIFY(appendSeconds(ring, 0, 3, rateHz));
        QCOMPARE(ring.blockCount(), 3);
        const qint64 block = DsrRingFile::blockBytes(rateHz);
        QCOMPARE(ring.fileBytes(), 2 * block + DsrRingFile::kHeaderBytes + qint64(rateHz) * 4);
        const double ratio = double(block) / double(DsrRingFile::blockBytes(192000));
        QVERIFY2(std::abs(ratio - rateHz / 192000.0) < 0.01 * rateHz / 192000.0, qPrintable(QString::number(ratio)));
        const double perHour = double(block) * 3600.0 / 1e9;
        QVERIFY2(std::abs(perHour - gbPerHour) < 0.04 * gbPerHour, qPrintable(QString::number(perHour)));
    }

    // A10: at a limit that holds 60 blocks, block 61 is written over block
    // 1, and the oldest moves on by one second.
    void block61WritesOverBlock1()
    {
        const qint64 block = DsrRingFile::blockBytes(192000);
        DsrRingFile ring;
        QString error;
        QVERIFY2(ring.open(ringPath(QStringLiteral("wrap60")), 60 * block, &error), qPrintable(error));
        QVERIFY(appendSeconds(ring, 0, 60, 192000));
        QCOMPARE(ring.blockCount(), 60);
        QCOMPARE(ring.oldestUtcNs(0), kT0);
        QCOMPARE(ring.writePosition(), 60 * block);

        QVERIFY(appendSeconds(ring, 60, 1, 192000));
        QCOMPARE(ring.blockCount(), 60);
        QCOMPARE(ring.oldestUtcNs(0), kT0 + kNs);
        QCOMPARE(ring.newestUtcNs(0), kT0 + 61 * kNs);
        QCOMPARE(ring.writePosition(), block);
        QVERIFY(ring.fileBytes() <= 60 * block);
        // Block 61 is where block 1 was: its samples, its time.
        std::vector<qint16> iq(16);
        DsrBlockHeader hdr;
        QCOMPARE(ring.read(0, kT0 + 60 * kNs, iq.data(), 8, &hdr), 8);
        QCOMPARE(iq[0], sampleAt(60, 0, 0));
        QCOMPARE(ring.read(0, kT0, iq.data(), 8, &hdr), 0);
    }

    // A10 at a 100 MB limit: it fills to the limit, then each new second
    // erases the oldest.
    void aHundredMegabyteLimitErasesTheOldest()
    {
        constexpr qint64 kLimit = 100000000;
        const qint64 block = DsrRingFile::blockBytes(192000);
        const int fits = static_cast<int>((kLimit / DsrRingFile::kAlignBytes * DsrRingFile::kAlignBytes) / block);
        QCOMPARE(fits, 129);
        DsrRingFile ring;
        QString error;
        QVERIFY2(ring.open(ringPath(QStringLiteral("hundred")), kLimit, &error), qPrintable(error));
        QVERIFY(appendSeconds(ring, 0, fits, 192000));
        QCOMPARE(ring.oldestUtcNs(0), kT0);
        QVERIFY(appendSeconds(ring, fits, 1, 192000));
        QCOMPARE(ring.oldestUtcNs(0), kT0 + kNs);
        QVERIFY(appendSeconds(ring, fits + 1, 1, 192000));
        QCOMPARE(ring.oldestUtcNs(0), kT0 + 2 * kNs);
        QCOMPARE(ring.blockCount(), fits);
        QVERIFY(ring.fileBytes() <= kLimit);
        QVERIFY(ring.usedBytes() <= kLimit);
    }

    // A10: one second read from the middle is the same 16-bit samples
    // written, at the right time and centre frequency.
    void oneSecondFromTheMiddleReadsBack()
    {
        DsrRingFile ring;
        QString error;
        QVERIFY2(ring.open(ringPath(QStringLiteral("readback")), 1000000000LL, &error), qPrintable(error));
        QVERIFY(appendSeconds(ring, 0, 60, 192000));

        // From 30.5 s: the second half of block 30, then the first half of
        // block 31.
        std::vector<qint16> iq(192000 * 2);
        qint64 at = kT0 + 30 * kNs + kNs / 2;
        int got = 0;
        QList<DsrBlockHeader> headers;
        while (got < 192000) {
            DsrBlockHeader hdr;
            const int n = ring.read(0, at, iq.data() + got * 2, 192000 - got, &hdr);
            QVERIFY(n > 0);
            QCOMPARE(hdr.utcNs, at);
            QCOMPARE(hdr.centreHz, kCentreHz);
            QCOMPARE(hdr.sampleRateHz, 192000);
            QCOMPARE(int(hdr.frames), n);
            headers.append(hdr);
            got += n;
            at += DsrRingFile::durationNs(n, 192000);
        }
        QCOMPARE(headers.size(), 2);
        QCOMPARE(int(headers.at(0).frames), 96000);
        QCOMPARE(at, kT0 + 31 * kNs + kNs / 2);
        for (int f = 0; f < 192000; ++f) {
            const int block = f < 96000 ? 30 : 31;
            const int frame = f < 96000 ? 96000 + f : f - 96000;
            if (iq[size_t(f) * 2] != sampleAt(block, frame, 0) || iq[size_t(f) * 2 + 1] != sampleAt(block, frame, 1)) {
                QFAIL(qPrintable(QStringLiteral("frame %1 differs").arg(f)));
            }
        }
        // Before the oldest and after the newest: nothing.
        QCOMPARE(ring.read(0, kT0 - 1, iq.data(), 10, nullptr), 0);
        QCOMPARE(ring.read(0, kT0 + 60 * kNs, iq.data(), 10, nullptr), 0);
        QCOMPARE(ring.read(1, kT0 + 30 * kNs, iq.data(), 10, nullptr), 0);
    }

    // Two receivers in one file: each reads its own.
    void eachReceiverReadsItsOwn()
    {
        DsrRingFile ring;
        QString error;
        QVERIFY2(ring.open(ringPath(QStringLiteral("two")), 1000000000LL, &error), qPrintable(error));
        std::vector<qint16> iq;
        for (int b = 0; b < 4; ++b) {
            fillBlock(iq, b, 192000);
            QVERIFY(ring.append(headerFor(b, 192000, 0), iq.data(), 192000));
            fillBlock(iq, 100 + b, 48000);
            QVERIFY(ring.append(headerFor(b, 48000, 1), iq.data(), 48000));
        }
        QCOMPARE(ring.receivers(), QList<int>({0, 1}));
        std::vector<qint16> out(8);
        DsrBlockHeader hdr;
        QCOMPARE(ring.read(1, kT0 + 2 * kNs, out.data(), 4, &hdr), 4);
        QCOMPARE(out[0], sampleAt(102, 0, 0));
        QCOMPARE(hdr.centreHz, kCentreHz + 1000000);
        QCOMPARE(ring.read(0, kT0 + 2 * kNs, out.data(), 4, &hdr), 4);
        QCOMPARE(out[0], sampleAt(2, 0, 0));
        QCOMPARE(ring.newestUtcNs(1), kT0 + 4 * kNs);
    }

    // Spec 3: your transmit seconds carry the flag and your audio beside
    // them; transmitMarks says where it starts and stops.
    void transmitSecondsAreMarkedWithTheirAudio()
    {
        DsrRingFile ring;
        QString error;
        QVERIFY2(ring.open(ringPath(QStringLiteral("transmit")), 1000000000LL, &error), qPrintable(error));
        std::vector<qint16> iq;
        std::vector<qint16> audio(48000);
        for (int b = 0; b < 10; ++b) {
            fillBlock(iq, b, 192000);
            const bool tx = b >= 3 && b <= 5;
            if (tx) {
                for (int i = 0; i < 48000; ++i) {
                    audio[size_t(i)] = static_cast<qint16>((b * 1000 + i) % 30000);
                }
                QVERIFY(ring.append(headerFor(b, 192000, 0, true), iq.data(), 192000, audio.data(), 48000, 48000));
            } else {
                QVERIFY(ring.append(headerFor(b, 192000), iq.data(), 192000));
            }
        }
        QCOMPARE(DsrRingFile::blockBytes(192000, 48000), DsrRingFile::blockBytes(192000 + 24000));
        const QList<QPair<qint64, bool>> marks = ring.transmitMarks(kT0, kT0 + 10 * kNs);
        QCOMPARE(marks.size(), 2);
        QCOMPARE(marks.at(0), qMakePair(kT0 + 3 * kNs, true));
        QCOMPARE(marks.at(1), qMakePair(kT0 + 6 * kNs, false));
        // Only the stretch asked for.
        QCOMPARE(ring.transmitMarks(kT0 + 7 * kNs, kT0 + 10 * kNs).size(), 0);
        QCOMPARE(ring.transmitMarks(kT0 + 4 * kNs, kT0 + 10 * kNs).first(), qMakePair(kT0 + 4 * kNs, true));

        std::vector<qint16> back(100);
        int rate = 0;
        QCOMPARE(ring.readTransmitAudio(kT0 + 4 * kNs + kNs / 2, back.data(), 100, &rate), 100);
        QCOMPARE(rate, 48000);
        QCOMPARE(back[0], static_cast<qint16>((4000 + 24000) % 30000));
        QCOMPARE(ring.readTransmitAudio(kT0 + 7 * kNs, back.data(), 100, &rate), 0);
        // The I/Q beside the audio is unchanged.
        DsrBlockHeader hdr;
        QCOMPARE(ring.read(0, kT0 + 4 * kNs, back.data(), 50, &hdr), 50);
        QCOMPARE(back[0], sampleAt(4, 0, 0));
        QCOMPARE(int(hdr.transmit), 1);
    }

    // A write cut short (a crash) after a block's samples and before its
    // header: the restart finds neither the new block nor the old one it
    // landed on, and every other block reads back as kept.
    void aWriteCutShortLeavesNoTornBlock()
    {
        const QString path = ringPath(QStringLiteral("torn"));
        const qint64 block = DsrRingFile::blockBytes(48000);
        {
            DsrRingFile ring;
            QString error;
            QVERIFY2(ring.open(path, 10 * block, &error), qPrintable(error));
            QVERIFY(appendSeconds(ring, 0, 10, 48000));
            QCOMPARE(ring.writePosition(), 10 * block);
            ring.stopBeforeHeaderForTest(true);
            // Second 10 lands where second 0 was, at the front.
            QVERIFY(!appendSeconds(ring, 10, 1, 48000));
        }
        DsrRingFile again;
        QString error;
        QVERIFY2(again.open(path, 10 * block, &error), qPrintable(error));
        QCOMPARE(again.blockCount(), 9);
        QCOMPARE(again.oldestUtcNs(0), kT0 + kNs);
        QCOMPARE(again.newestUtcNs(0), kT0 + 10 * kNs);
        std::vector<qint16> iq(4);
        QCOMPARE(again.read(0, kT0, iq.data(), 2, nullptr), 0);
        QCOMPARE(again.read(0, kT0 + kNs, iq.data(), 2, nullptr), 2);
        QCOMPARE(iq[0], sampleAt(1, 0, 0));
        QCOMPARE(again.read(0, kT0 + 9 * kNs, iq.data(), 2, nullptr), 2);
        QCOMPARE(iq[0], sampleAt(9, 0, 0));
    }

    // A restart scans the headers and takes up where it was: oldest,
    // newest and write position, after the ring has wrapped twice.
    void aRestartFindsItsPlace()
    {
        const QString path = ringPath(QStringLiteral("restart"));
        const qint64 block = DsrRingFile::blockBytes(48000);
        qint64 oldest = 0;
        qint64 newest = 0;
        qint64 position = 0;
        int count = 0;
        {
            DsrRingFile ring;
            QString error;
            QVERIFY2(ring.open(path, 10 * block, &error), qPrintable(error));
            QVERIFY(appendSeconds(ring, 0, 25, 48000));
            oldest = ring.oldestUtcNs(0);
            newest = ring.newestUtcNs(0);
            position = ring.writePosition();
            count = ring.blockCount();
            QCOMPARE(oldest, kT0 + 15 * kNs);
            QCOMPARE(count, 10);
        }
        DsrRingFile again;
        QString error;
        QVERIFY2(again.open(path, 10 * block, &error), qPrintable(error));
        QCOMPARE(again.blockCount(), count);
        QCOMPARE(again.oldestUtcNs(0), oldest);
        QCOMPARE(again.newestUtcNs(0), newest);
        QCOMPARE(again.writePosition(), position);
        std::vector<qint16> iq(4);
        QCOMPARE(again.read(0, kT0 + 20 * kNs, iq.data(), 2, nullptr), 2);
        QCOMPARE(iq[0], sampleAt(20, 0, 0));
        // The next second goes where it should: the oldest moves on by one.
        QVERIFY(appendSeconds(again, 25, 1, 48000));
        QCOMPARE(again.oldestUtcNs(0), oldest + kNs);
        QCOMPARE(again.newestUtcNs(0), newest + kNs);

        // A smaller limit on the next start keeps only what fits under it.
        again.close();
        DsrRingFile smaller;
        QVERIFY2(smaller.open(path, 4 * block, &error), qPrintable(error));
        QVERIFY(smaller.blockCount() <= 4);
        QVERIFY(smaller.fileBytes() <= 4 * block);
        QVERIFY(appendSeconds(smaller, 26, 6, 48000));
        QCOMPARE(smaller.blockCount(), 4);
        QCOMPARE(smaller.oldestUtcNs(0), kT0 + 28 * kNs);
    }
};

QTEST_GUILESS_MAIN(TestDsrRing)
#include "tst_dsr_ring.moc"
