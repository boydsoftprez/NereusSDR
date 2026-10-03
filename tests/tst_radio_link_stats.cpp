// no-port-check: NereusSDR-original test. The radio link's datagram
// counters (R-R3-32, R-R3-49, remote-window parity Task 6): UDP packets
// seen, packet loss from sequence gaps, RFC 3550 interarrival jitter and
// the packet gap, as a unit and from fake radios on both protocols.

#include <QtTest/QtTest>

#include <QSignalSpy>

#include "core/P1RadioConnection.h"
#include "core/P2RadioConnection.h"
#include "core/RadioLinkStats.h"
#include "fakes/P1FakeRadio.h"
#include "fakes/P2FakeRadio.h"

using namespace NereusSDR;
using NereusSDR::Test::P1FakeRadio;
using NereusSDR::Test::P2FakeRadio;

namespace {

// A base far from zero so the first bucket index is not a special case.
constexpr qint64 kBaseUs = 100'000'000;

RadioInfo p1Info(const P1FakeRadio& fake)
{
    RadioInfo info;
    info.address = fake.localAddress();
    info.port = fake.localPort();
    info.boardType = HPSDRHW::HermesLite;
    info.protocol = ProtocolVersion::Protocol1;
    info.macAddress = QStringLiteral("aa:bb:cc:11:22:33");
    return info;
}

} // namespace

class TstRadioLinkStats : public QObject {
    Q_OBJECT

private slots:
    // RFC 3550 section 6.4.1, worked by hand for a 1 ms stream:
    //   arrivals 0, 1000, 2100, 3000 us, sequence 0..3:
    //     D = 0      -> J = 0
    //     D = +100   -> J = 100/16            = 6.25 us
    //     D = -100   -> J = 6.25 + 93.75/16   = 12.109375 us
    //   then sequence 5 at 5000 us (one datagram missing):
    //     D = 2000 - 2*1000 = 0 -> J = 12.109375 * 15/16 = 11.352539 us
    void jitterFollowsRfc3550ByHand()
    {
        QCOMPARE(RadioLinkStats::nextJitter(0.0, 100.0), 6.25);
        QCOMPARE(RadioLinkStats::nextJitter(6.25, -100.0), 12.109375);

        RadioLinkStats stats;
        const double spacing = 1000.0;
        stats.noteStreamArrival(2, 0, kBaseUs + 0, spacing);
        QVERIFY(!stats.snapshot(kBaseUs).jitterMs);   // one arrival: none yet
        stats.noteStreamArrival(2, 1, kBaseUs + 1000, spacing);
        QCOMPARE(*stats.snapshot(kBaseUs + 1000).jitterMs, 0.0);
        stats.noteStreamArrival(2, 2, kBaseUs + 2100, spacing);
        QCOMPARE(*stats.snapshot(kBaseUs + 2100).jitterMs, 0.00625);
        stats.noteStreamArrival(2, 3, kBaseUs + 3000, spacing);
        QCOMPARE(*stats.snapshot(kBaseUs + 3000).jitterMs, 0.012109375);
        stats.noteStreamArrival(2, 5, kBaseUs + 5000, spacing);
        QVERIFY(qAbs(*stats.snapshot(kBaseUs + 5000).jitterMs - 0.011352539) < 1e-9);

        // A higher stream is ignored while the lowest is active.
        stats.noteStreamArrival(5, 7, kBaseUs + 5100, spacing);
        QVERIFY(qAbs(*stats.snapshot(kBaseUs + 5100).jitterMs - 0.011352539) < 1e-9);
        // A lower stream takes over and starts its estimate afresh.
        stats.noteStreamArrival(1, 0, kBaseUs + 5200, spacing);
        QVERIFY(!stats.snapshot(kBaseUs + 5200).jitterMs);
        // A stream silent for the whole loss window reports no jitter.
        stats.noteStreamArrival(1, 1, kBaseUs + 6200, spacing);
        QVERIFY(stats.snapshot(kBaseUs + 6200).jitterMs);
        QVERIFY(!stats.snapshot(kBaseUs + 6200 + RadioLinkStats::kLossWindowUs + 1).jitterMs);
    }

    void lossIsLostOverReceivedPlusLostInTheLastFiveSeconds()
    {
        RadioLinkStats stats;
        QVERIFY(!stats.snapshot(kBaseUs).packetLossPercent);   // nothing yet: absent, not 0
        for (int i = 0; i < 10; ++i) {
            stats.noteSequenced(kBaseUs + i * 1000, i == 4 ? 1U : 0U);
        }
        QVERIFY(qAbs(*stats.snapshot(kBaseUs + 10'000).packetLossPercent - 100.0 / 11.0) < 1e-9);
        // In-order datagrams later in the window dilute it.
        for (int i = 0; i < 9; ++i) {
            stats.noteSequenced(kBaseUs + 1'000'000 + i * 1000, 0U);
        }
        QCOMPARE(*stats.snapshot(kBaseUs + 1'100'000).packetLossPercent, 5.0);
        // Six seconds on, the loss has left the window.
        QVERIFY(!stats.snapshot(kBaseUs + 7'000'000).packetLossPercent);
        stats.noteSequenced(kBaseUs + 7'000'000, 0U);
        QCOMPARE(*stats.snapshot(kBaseUs + 7'000'000).packetLossPercent, 0.0);
    }

    void gapIsTheLongestIntervalInTheLastSecond()
    {
        RadioLinkStats stats;
        QVERIFY(!stats.snapshot(kBaseUs).packetGapMs);          // before the first datagram
        QCOMPARE(stats.snapshot(kBaseUs).udpPacketsSeen, quint64(0));
        stats.noteDatagram(kBaseUs);
        stats.noteDatagram(kBaseUs + 100'000);
        stats.noteDatagram(kBaseUs + 400'000);
        const RadioLinkStats::Snapshot s = stats.snapshot(kBaseUs + 450'000);
        QCOMPARE(s.udpPacketsSeen, quint64(3));
        QCOMPARE(*s.packetGapMs, 300.0);
        // The interval still open counts: a radio gone quiet shows it.
        QCOMPARE(*stats.snapshot(kBaseUs + 2'000'000).packetGapMs, 1600.0);
        // A later burst: the 300 ms gap has left the one-second window.
        stats.noteDatagram(kBaseUs + 3'000'000);
        stats.noteDatagram(kBaseUs + 3'010'000);
        QCOMPARE(*stats.snapshot(kBaseUs + 3'010'000).packetGapMs, 2600.0);
        stats.noteDatagram(kBaseUs + 4'300'000);
        stats.noteDatagram(kBaseUs + 4'310'000);
        QCOMPARE(*stats.snapshot(kBaseUs + 4'310'000).packetGapMs, 1290.0);
        stats.noteDatagram(kBaseUs + 5'600'000);
        stats.noteDatagram(kBaseUs + 5'620'000);
        QCOMPARE(*stats.snapshot(kBaseUs + 5'620'000).packetGapMs, 1290.0);

        stats.reset();
        QCOMPARE(stats.snapshot(kBaseUs + 5'620'000).udpPacketsSeen, quint64(0));
        QVERIFY(!stats.snapshot(kBaseUs + 5'620'000).packetGapMs);
    }

    // Protocol 1: EP6 frames from a fake radio, with a gap in their
    // sequence numbers. One mismatch counts one loss, as Thetis counts it.
    void p1FakeRadioCountsDatagramsAndOneGap()
    {
        P1FakeRadio fake;
        fake.setAutoStreamEnabled(false);
        fake.start();

        P1RadioConnection conn;
        conn.init();
        QSignalSpy frames(&conn, &RadioConnection::frameReceived);
        conn.connectToRadio(p1Info(fake));
        QTRY_VERIFY_WITH_TIMEOUT(fake.isRunning(), 3000);

        fake.sendEp6Frames(5);
        QTRY_COMPARE_WITH_TIMEOUT(frames.count(), 5, 3000);
        RadioLinkStats::Snapshot s = conn.linkStats();
        QVERIFY(s.udpPacketsSeen >= 5);
        QCOMPARE(*s.packetLossPercent, 0.0);
        QVERIFY(s.jitterMs.has_value());
        QVERIFY(s.packetGapMs.has_value());

        fake.skipEp6Sequence(4);
        fake.sendEp6Frames(2);
        QTRY_COMPARE_WITH_TIMEOUT(frames.count(), 7, 3000);
        s = conn.linkStats();
        QVERIFY(s.udpPacketsSeen >= 7);
        QCOMPARE(*s.packetLossPercent, 100.0 / 8.0);

        conn.disconnect();
        fake.stop();
    }

    // Protocol 2: DDC I/Q datagrams from a fake radio, with a gap in the
    // DDC's sequence numbers (rx_in_seq_err counts one).
    void p2FakeRadioCountsDatagramsAndOneGap()
    {
        P2FakeRadio fake;
        QVERIFY(fake.start());
        P2RadioConnection conn;
        conn.setPortBasesForTest(fake.outboundPortBase(), fake.inputRolePortBase());
        conn.init();
        QSignalSpy iq(&conn, &RadioConnection::iqDataReceived);
        conn.connectToRadio(fake.radioInfo());
        QTRY_VERIFY_WITH_TIMEOUT(fake.hasClient(), 3000);

        for (int i = 0; i < 5; ++i) {
            fake.sendDdc(2);
        }
        QTRY_VERIFY_WITH_TIMEOUT(iq.count() >= 5, 3000);
        RadioLinkStats::Snapshot s = conn.linkStats();
        QVERIFY(s.udpPacketsSeen >= 5);
        QCOMPARE(*s.packetLossPercent, 0.0);
        QVERIFY(s.jitterMs.has_value());

        fake.skipDdcSequence(3);
        fake.sendDdc(2);
        fake.sendDdc(2);
        QTRY_VERIFY_WITH_TIMEOUT(iq.count() >= 7, 3000);
        s = conn.linkStats();
        QVERIFY(s.udpPacketsSeen >= 7);
        QCOMPARE(*s.packetLossPercent, 100.0 / 8.0);

        conn.disconnect();
        fake.stop();
    }
};

QTEST_MAIN(TstRadioLinkStats)
#include "tst_radio_link_stats.moc"
