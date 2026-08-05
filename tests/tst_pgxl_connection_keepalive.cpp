// =================================================================
// tests/tst_pgxl_connection_keepalive.cpp  (NereusSDR)
// =================================================================
// NereusSDR-native test. No AetherSDR equivalent (keepalive is a
// NereusSDR Tier 2 addition per design doc §2 and §6.4).
// Wire format from FlexRadio PowerGenius Ethernet API wiki spec.
// =================================================================
// Modification history (NereusSDR):
//   2026-05-19  Created by J.J. Boyd (KG4VCF), with AI-assisted
//                 transformation via Anthropic Claude Code.
//                 Tests: enableKeepalive frame format, keepalive timer
//                 activation via testKeepaliveTimerActive() seam.
// =================================================================

#include <QtTest/QtTest>
#include "core/PgxlConnection.h"

class PgxlConnectionKeepaliveTest : public QObject {
    Q_OBJECT
private slots:
    void enableKeepaliveSendsKeepaliveEnable();
    void keepaliveTimerStartsAfterEnable();
};

void PgxlConnectionKeepaliveTest::enableKeepaliveSendsKeepaliveEnable() {
    NereusSDR::PgxlConnection conn;
    QSignalSpy frameSpy(&conn, &NereusSDR::PgxlConnection::testFrameWrittenForTesting);
    conn.enableKeepalive();
    // At least one frame must have been written (the keepalive enable command).
    QVERIFY(frameSpy.count() >= 1);
    bool foundKeepalive = false;
    for (const auto& call : frameSpy) {
        if (call.at(0).toString().contains("keepalive enable")) {
            foundKeepalive = true;
            break;
        }
    }
    QVERIFY(foundKeepalive);
}

void PgxlConnectionKeepaliveTest::keepaliveTimerStartsAfterEnable() {
    NereusSDR::PgxlConnection conn;
    QVERIFY(!conn.testKeepaliveTimerActive());
    conn.enableKeepalive();
    QVERIFY(conn.testKeepaliveTimerActive());
}

QTEST_GUILESS_MAIN(PgxlConnectionKeepaliveTest)
#include "tst_pgxl_connection_keepalive.moc"
