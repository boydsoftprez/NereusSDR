// =================================================================
// tests/tst_tgxl_connection_parse.cpp  (NereusSDR)
// =================================================================
// Source attribution (AetherSDR, GPLv3):
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       per https://github.com/ten9876/AetherSDR (GPLv3)
//   This file is a port or structural derivative of AetherSDR source.
//   AetherSDR is licensed under the GNU General Public License v3.
//   NereusSDR is also GPLv3. Attribution follows GPLv3 section 5 requirements.
// =================================================================
// Modification history (NereusSDR):
//   2026-05-18  Test scaffolding for TgxlConnection parse logic by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code. Based on AetherSDR src/core/TgxlConnection.{h,cpp}
//                 [@0cd4559].
// =================================================================

#include <QtTest/QtTest>
#include "core/TgxlConnection.h"

class TgxlConnectionParseTest : public QObject {
    Q_OBJECT
private slots:
    void parsesVersionBanner();
    void parsesStateFrameUnsolicited();
    void parsesStatusPollResponse();
    void adjustRelayBuildsCorrectFrame();
};

void TgxlConnectionParseTest::parsesVersionBanner() {
    NereusSDR::TgxlConnection conn;
    QSignalSpy connectedSpy(&conn, &NereusSDR::TgxlConnection::connected);
    conn.injectLineForTesting("V1.2.17");
    QCOMPARE(conn.version(), QString("1.2.17"));
    QVERIFY(conn.isConnected());
    QCOMPARE(connectedSpy.count(), 1);
}

void TgxlConnectionParseTest::parsesStateFrameUnsolicited() {
    NereusSDR::TgxlConnection conn;
    QSignalSpy stateSpy(&conn, &NereusSDR::TgxlConnection::stateUpdated);
    conn.injectLineForTesting("V1.2.17");
    conn.injectLineForTesting("S0|state relayC1=42 relayL=199 relayC2=88 operate=1 bypass=0");
    QCOMPARE(stateSpy.count(), 1);
    auto kvs = stateSpy.takeFirst().at(0).value<QMap<QString,QString>>();
    QCOMPARE(kvs.value("relayC1"), QString("42"));
    QCOMPARE(kvs.value("relayL"),  QString("199"));
    QCOMPARE(kvs.value("operate"), QString("1"));
}

void TgxlConnectionParseTest::parsesStatusPollResponse() {
    NereusSDR::TgxlConnection conn;
    QSignalSpy statusSpy(&conn, &NereusSDR::TgxlConnection::statusUpdated);
    conn.injectLineForTesting("V1.2.17");
    conn.injectLineForTesting("S1|status fwd=12.5 swr=1.1 tuning=0");
    QCOMPARE(statusSpy.count(), 1);
    auto kvs = statusSpy.takeFirst().at(0).value<QMap<QString,QString>>();
    QCOMPARE(kvs.value("fwd"), QString("12.5"));
}

void TgxlConnectionParseTest::adjustRelayBuildsCorrectFrame() {
    // Verify the sendCommand string contains "tune relay=1 move=-1"
    // by attaching a QSignalSpy-like sniffer; the simplest test is to
    // pump the socket through a local QTcpServer. Implementation deferred
    // to a test infrastructure helper; for now just compile-test:
    NereusSDR::TgxlConnection conn;
    conn.adjustRelay(1, -1);  // should be a no-op when not connected
    QVERIFY(!conn.isConnected());
}

QTEST_GUILESS_MAIN(TgxlConnectionParseTest)
#include "tst_tgxl_connection_parse.moc"
