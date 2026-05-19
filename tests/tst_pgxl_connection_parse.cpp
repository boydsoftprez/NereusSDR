// =================================================================
// tests/tst_pgxl_connection_parse.cpp  (NereusSDR)
// =================================================================
// Source attribution (AetherSDR, GPLv3):
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       per https://github.com/ten9876/AetherSDR (GPLv3)
//   This file is a port or structural derivative of AetherSDR source.
//   AetherSDR is licensed under the GNU General Public License v3.
//   NereusSDR is also GPLv3. Attribution follows GPLv3 section 5 requirements.
// =================================================================
// Modification history (NereusSDR):
//   2026-05-18  Test scaffolding for PgxlConnection parse logic by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code. Based on AetherSDR src/core/PgxlConnection.{h,cpp}
//                 [@0cd4559]. Real assertions land in Tasks 6+7.
// =================================================================

#include <QtTest/QtTest>
#include "core/PgxlConnection.h"

class PgxlConnectionParseTest : public QObject {
    Q_OBJECT
private slots:
    void parsesVersionBanner();
    void parsesResponseFrameWithKvBody();
    void parsesUnsolicitedStatusPush();
};

void PgxlConnectionParseTest::parsesVersionBanner() {
    NereusSDR::PgxlConnection conn;
    QSignalSpy connectedSpy(&conn, &NereusSDR::PgxlConnection::connected);

    conn.injectLineForTesting("V3.8.9");

    QCOMPARE(conn.version(), QString("3.8.9"));
    QVERIFY(conn.isConnected());
    QCOMPARE(connectedSpy.count(), 1);
}

void PgxlConnectionParseTest::parsesResponseFrameWithKvBody() {
    NereusSDR::PgxlConnection conn;
    QSignalSpy statusSpy(&conn, &NereusSDR::PgxlConnection::statusUpdated);

    conn.injectLineForTesting("V3.8.9");  // need handshake first
    conn.injectLineForTesting("R1|0|state=OPERATE temp=42.5 vac=240 fwd=1480.0 swr=2.1");

    QCOMPARE(statusSpy.count(), 1);
    auto kvs = statusSpy.takeFirst().at(0).value<QMap<QString,QString>>();
    QCOMPARE(kvs.value("state"), QString("OPERATE"));
    QCOMPARE(kvs.value("temp"),  QString("42.5"));
    QCOMPARE(kvs.value("vac"),   QString("240"));
    QCOMPARE(kvs.value("fwd"),   QString("1480.0"));
    QCOMPARE(kvs.value("swr"),   QString("2.1"));
}

void PgxlConnectionParseTest::parsesUnsolicitedStatusPush()    { QVERIFY(true); }

QTEST_GUILESS_MAIN(PgxlConnectionParseTest)
#include "tst_pgxl_connection_parse.moc"
