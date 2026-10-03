// no-port-check: NereusSDR-original. RFC 3550 jitter/sequence math, R-R3-23.
#include <QtTest>
#include "core/session/media/RtpReceptionStats.h"
using namespace NereusSDR;
class TstRtpReceptionStats : public QObject {
    Q_OBJECT
private slots:
    void emptyThenSinglePacketThenReset()
    {
        RtpReceptionStats stats;
        QVERIFY(!stats.jitterMs());
        QCOMPARE(stats.receivedPackets(), quint64(0));
        QCOMPARE(stats.expectedPackets(), quint64(0));
        QCOMPARE(stats.missingPackets(), quint64(0));

        stats.observe(0, 0, 0);
        QVERIFY(!stats.jitterMs());
        QCOMPARE(stats.receivedPackets(), quint64(1));
        QCOMPARE(stats.expectedPackets(), quint64(1));
        QCOMPARE(stats.missingPackets(), quint64(0));

        stats.observe(1, 1920, 40'000'000);
        QVERIFY(stats.jitterMs());
        QCOMPARE(*stats.jitterMs(), 0.0);
        QCOMPARE(stats.receivedPackets(), quint64(2));
        QCOMPARE(stats.expectedPackets(), quint64(2));

        stats.reset();
        QVERIFY(!stats.jitterMs());
        QCOMPARE(stats.receivedPackets(), quint64(0));
        QCOMPARE(stats.expectedPackets(), quint64(0));
        QCOMPARE(stats.missingPackets(), quint64(0));
    }
    void regularSpacingHasZeroJitterAndNoMissing()
    {
        RtpReceptionStats stats;
        for (int i = 0; i < 100; ++i) {
            stats.observe(quint16(i), quint32(i) * 1920u, qint64(i) * 40'000'000LL);
        }
        QCOMPARE(stats.receivedPackets(), quint64(100));
        QCOMPARE(stats.expectedPackets(), quint64(100));
        QCOMPARE(stats.missingPackets(), quint64(0));
        QVERIFY(stats.jitterMs());
        QCOMPARE(*stats.jitterMs(), 0.0);
    }
    void withheldPacketIsMissingWithoutDisturbingJitter()
    {
        RtpReceptionStats stats;
        for (int i = 0; i < 100; ++i) {
            if (i == 50) { continue; } // never observed: a real gap, not reordering
            stats.observe(quint16(i), quint32(i) * 1920u, qint64(i) * 40'000'000LL);
        }
        QCOMPARE(stats.receivedPackets(), quint64(99));
        QCOMPARE(stats.expectedPackets(), quint64(100));
        QCOMPARE(stats.missingPackets(), quint64(1));
        QVERIFY(stats.jitterMs());
        QCOMPARE(*stats.jitterMs(), 0.0);
    }
    void coincidentArrivalsProduceRfc3550JitterRamp()
    {
        // RFC 3550 appendix A.8: J += (|D| - J) / 16. Three packets 1920
        // ticks (40 ms at 48 kHz) apart by timestamp but arriving at the
        // same instant put the full 40 ms deviation into D every time.
        RtpReceptionStats stats;
        constexpr qint64 kArrivalNs = 5'000'000'000;
        stats.observe(0, 0, kArrivalNs);
        QVERIFY(!stats.jitterMs());
        stats.observe(1, 1920, kArrivalNs);
        QVERIFY(stats.jitterMs());
        QCOMPARE(*stats.jitterMs(), 2.5);
        stats.observe(2, 3840, kArrivalNs);
        QVERIFY(stats.jitterMs());
        QCOMPARE(*stats.jitterMs(), 4.84375);
    }
    void reorderedPacketsAreReceivedWithoutMovingMaxBackward()
    {
        RtpReceptionStats stats;
        const quint16 order[] = {0, 1, 3, 2, 4};
        for (int i = 0; i < 5; ++i) {
            // Arrival always advances with observation order; only the
            // RTP sequence itself is reordered.
            stats.observe(order[i], quint32(order[i]) * 1920u, qint64(i) * 40'000'000LL);
        }
        QCOMPARE(stats.receivedPackets(), quint64(5));
        QCOMPARE(stats.expectedPackets(), quint64(5));
        QCOMPARE(stats.missingPackets(), quint64(0));
    }
    void sequenceWrapAcrossTwelvePackets()
    {
        RtpReceptionStats stats;
        for (int i = 0; i < 12; ++i) {
            const quint16 sequence = quint16(65530 + i); // wraps 65535 -> 0
            stats.observe(sequence, quint32(i) * 1920u, qint64(i) * 40'000'000LL);
        }
        QCOMPARE(stats.receivedPackets(), quint64(12));
        QCOMPARE(stats.expectedPackets(), quint64(12));
        QCOMPARE(stats.missingPackets(), quint64(0));
    }
    void timestampWrapNearMaxKeepsZeroJitter()
    {
        RtpReceptionStats stats;
        const quint32 base = 0xFFFFFFFFu - 3u * 1920u;
        for (int i = 0; i < 6; ++i) {
            const quint32 timestamp = quint32(base + quint32(i) * 1920u); // wraps past 0xFFFFFFFF
            stats.observe(quint16(i), timestamp, qint64(i) * 40'000'000LL);
        }
        QVERIFY(stats.jitterMs());
        QCOMPARE(*stats.jitterMs(), 0.0);
    }
};
QTEST_APPLESS_MAIN(TstRtpReceptionStats)
#include "tst_rtp_reception_stats.moc"
