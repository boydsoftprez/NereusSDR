// =================================================================
// tests/tst_tgxl_connection_ping.cpp  (NereusSDR)
// =================================================================
// NereusSDR-native test. No AetherSDR equivalent (ping RTT measurement
// is a NereusSDR Tier 2 addition per design doc §2 and §6.4).
// Wire format from FlexRadio TunerGenius Ethernet API (TGXL protocol,
// parallel to PgxlConnection ping path).
// =================================================================
// Modification history (NereusSDR):
//   2026-05-19  Created by J.J. Boyd (KG4VCF), with AI-assisted
//                 transformation via Anthropic Claude Code.
//                 Phase 3P-II Task 67.
//                 Tests: ping emits pongReceived on R-frame reply;
//                 ping timeout via testFlushPingTimeouts() seam.
// =================================================================

#include <QtTest/QtTest>
#include "core/TgxlConnection.h"

class TgxlConnectionPingTest : public QObject {
    Q_OBJECT
private slots:
    void pingEmitsPongReceivedOnReply();
    void pingTimesOutAfterFiveSeconds();
};

void TgxlConnectionPingTest::pingEmitsPongReceivedOnReply()
{
    NereusSDR::TgxlConnection conn;
    QSignalSpy spy(&conn, &NereusSDR::TgxlConnection::pongReceived);
    // Establish version handshake so injectLineForTesting routes R-frames.
    conn.injectLineForTesting("V1.2.17");
    quint32 seq = conn.ping("test");
    // Inject the R-frame reply: R<seq>|0| (TGXL ping response has empty body).
    conn.injectLineForTesting(QString("R%1|0|").arg(seq));
    QCOMPARE(spy.count(), 1);
    QList<QVariant> args = spy.takeFirst();
    QCOMPARE(args.at(0).toUInt(), seq);
    // RTT in-process: must be non-negative and well below 1000 ms.
    qint64 rttMs = args.at(1).toLongLong();
    QVERIFY(rttMs >= 0);
    QVERIFY(rttMs < 1000);
    QCOMPARE(args.at(2).toString(), QString("test"));
}

void TgxlConnectionPingTest::pingTimesOutAfterFiveSeconds()
{
    NereusSDR::TgxlConnection conn;
    QSignalSpy spy(&conn, &NereusSDR::TgxlConnection::pingTimedOut);
    conn.injectLineForTesting("V1.2.17");
    quint32 seq = conn.ping();
    // Use the test seam to evict all pending pings immediately (bypasses 5 s).
    conn.testFlushPingTimeouts();
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toUInt(), seq);
}

QTEST_GUILESS_MAIN(TgxlConnectionPingTest)
#include "tst_tgxl_connection_ping.moc"
