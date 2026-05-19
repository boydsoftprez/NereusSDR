// =================================================================
// tests/tst_pgxl_connection_ping.cpp  (NereusSDR)
// =================================================================
// NereusSDR-native test. No AetherSDR equivalent (ping RTT measurement
// is a NereusSDR Tier 2 addition per design doc §2 and §6.4).
// Wire format from FlexRadio PowerGenius Ethernet API wiki spec.
// =================================================================
// Modification history (NereusSDR):
//   2026-05-19  Created by J.J. Boyd (KG4VCF), with AI-assisted
//                 transformation via Anthropic Claude Code.
//                 Tests: ping emits pongReceived on R-frame reply;
//                 ping timeout via testFlushPingTimeouts() seam.
// =================================================================

#include <QtTest/QtTest>
#include "core/PgxlConnection.h"

class PgxlConnectionPingTest : public QObject {
    Q_OBJECT
private slots:
    void pingEmitsPongReceivedOnReply();
    void pingTimesOutViaFlushSeam();
};

void PgxlConnectionPingTest::pingEmitsPongReceivedOnReply() {
    NereusSDR::PgxlConnection conn;
    QSignalSpy pongSpy(&conn, &NereusSDR::PgxlConnection::pongReceived);
    // Establish version handshake so injectLineForTesting routes R-frames.
    conn.injectLineForTesting("V3.8.9");
    quint32 seq = conn.ping("test-tag");
    // Inject the R-frame reply: R<seq>|0| (PGXL ping response has empty body).
    conn.injectLineForTesting(QString("R%1|0|").arg(seq));
    QCOMPARE(pongSpy.count(), 1);
    QList<QVariant> args = pongSpy.takeFirst();
    QCOMPARE(args.at(0).toUInt(), seq);
    // RTT in-process: must be non-negative and well below 100 ms.
    qint64 rttMs = args.at(1).toLongLong();
    QVERIFY(rttMs >= 0);
    QVERIFY(rttMs < 100);
    QCOMPARE(args.at(2).toString(), QString("test-tag"));
}

void PgxlConnectionPingTest::pingTimesOutViaFlushSeam() {
    NereusSDR::PgxlConnection conn;
    QSignalSpy timeoutSpy(&conn, &NereusSDR::PgxlConnection::pingTimedOut);
    conn.injectLineForTesting("V3.8.9");
    quint32 seq = conn.ping("timeout-tag");
    // Use the test seam to evict all pending pings immediately (bypasses 5 s).
    conn.testFlushPingTimeouts();
    QCOMPARE(timeoutSpy.count(), 1);
    QCOMPARE(timeoutSpy.takeFirst().at(0).toUInt(), seq);
}

QTEST_GUILESS_MAIN(PgxlConnectionPingTest)
#include "tst_pgxl_connection_ping.moc"
