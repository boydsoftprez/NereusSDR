// SPDX-License-Identifier: GPL-3.0-or-later
//
// no-port-check: This test contains fabricated DX-cluster fixture
// strings (spotter + DX callsigns) that pre-commit's
// check-new-ports.py may flag as upstream-contributor callsigns.
// They are test fixtures, not ported callsigns. See B2 (commit
// f43582e) for precedent.
//
// NereusSDR - DxClusterClient telnet parser tests
//
// Phase 3J-2 Task B3. Pins the contract that DxClusterClient parses
// the standard DX cluster "DX de" telnet format, detects multi-flavor
// login prompts, strips telnet IAC bytes, and tags RBN-suffixed
// spotters as source="RBN".

#include <QtTest>

#include "core/DxClusterClient.h"
#include "core/DxSpot.h"

using namespace NereusSDR;

class TestDxClusterParser : public QObject {
    Q_OBJECT
private slots:
    void parsesStandardDxLine();
    void detectsLoginPrompts_data();
    void detectsLoginPrompts();
    void stripsTelnetIAC();
    void tagsRbnSpottersByPrefix();
    void tagsRbnSpottersBySuffix();
    void rejectsMalformedLine();
};

void TestDxClusterParser::parsesStandardDxLine() {
    DxClusterClient c;
    DxSpot s;
    QVERIFY(c.parseDxSpotLineForTest(
        QStringLiteral("DX de W3LPL:    14025.0  JA1ABC       CW big signal       1824Z"),
        s
    ));
    QCOMPARE(s.spotterCall, QStringLiteral("W3LPL"));
    QCOMPARE(s.freqMhz, 14.025);
    QCOMPARE(s.dxCall, QStringLiteral("JA1ABC"));
    QCOMPARE(s.comment, QStringLiteral("CW big signal"));
    QCOMPARE(s.utcTime, QTime(18, 24));
    QCOMPARE(s.source, QStringLiteral("Cluster"));
}

void TestDxClusterParser::detectsLoginPrompts_data() {
    QTest::addColumn<QString>("input");
    QTest::addColumn<bool>("expected");
    QTest::newRow("Please enter your call:")  << "Please enter your call:" << true;
    QTest::newRow("call:")                     << "call:" << true;
    QTest::newRow("callsign:")                 << "callsign:" << true;
    QTest::newRow("login:")                    << "login:" << true;
    QTest::newRow("ar-cluster phrase")         << "Please enter your call to login:" << true;
    QTest::newRow("your call")                 << "Enter your call sign>" << true;
    QTest::newRow("DX line")                   << "DX de W1AW:" << false;
    QTest::newRow("empty")                     << "" << false;
}

void TestDxClusterParser::detectsLoginPrompts() {
    QFETCH(QString, input);
    QFETCH(bool, expected);
    DxClusterClient c;
    QCOMPARE(c.isLoginPromptForTest(input), expected);
}

void TestDxClusterParser::stripsTelnetIAC() {
    DxClusterClient c;
    QByteArray buf;
    buf.append(static_cast<char>(0xFF));
    buf.append(static_cast<char>(0xFD));
    buf.append(static_cast<char>(0x18));  // IAC DO TERMINAL-TYPE
    buf.append("DX de W1AW: 14000.0 K1ABC      CW   1830Z\n");
    c.stripTelnetIACForTest(buf);
    QVERIFY(buf.startsWith("DX de"));
}

void TestDxClusterParser::tagsRbnSpottersByPrefix() {
    DxClusterClient c;
    DxSpot s;
    QVERIFY(c.parseDxSpotLineForTest(
        QStringLiteral("DX de RBN-WK1S:  14025.0  JA1ABC     CW 28 dB 25 wpm    1825Z"),
        s
    ));
    QCOMPARE(s.source, QStringLiteral("RBN"));
}

void TestDxClusterParser::tagsRbnSpottersBySuffix() {
    DxClusterClient c;
    DxSpot s;
    QVERIFY(c.parseDxSpotLineForTest(
        QStringLiteral("DX de DJ4DN-#:   14074.3  DL2RD      FT8  -3 dB         1825Z"),
        s
    ));
    QCOMPARE(s.source, QStringLiteral("RBN"));
}

void TestDxClusterParser::rejectsMalformedLine() {
    DxClusterClient c;
    DxSpot s;
    QVERIFY(!c.parseDxSpotLineForTest(QStringLiteral("not a spot"), s));
    QVERIFY(!c.parseDxSpotLineForTest(QString(), s));
    QVERIFY(!c.parseDxSpotLineForTest(QStringLiteral("DX de FOO:"), s));
}

QTEST_GUILESS_MAIN(TestDxClusterParser)
#include "tst_dx_cluster_parser.moc"
