// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - SpotCollectorClient DX-line parser tests
//
// Phase 3J-2 Task B1. Pins the contract that SpotCollectorClient parses
// the standard DXLab "DX de" UDP line format and tags RBN-suffixed
// spotters as source="RBN".
//
// no-port-check: VK6APH appears in a fabricated DX-cluster line used as
// a test fixture (RBN-suffixed spotter regression). It is not a Thetis
// derivation and does not require a PROVENANCE entry. The
// SpotCollectorClient code itself is registered in
// docs/attribution/aethersdr-reconciliation.md (Phase 3J-2 Task B1).

#include <QtTest>

#include "core/SpotCollectorClient.h"
#include "core/DxSpot.h"

using namespace NereusSDR;

class TestSpotCollectorParser : public QObject {
    Q_OBJECT
private slots:
    void parsesStandardDxLine();
    void parsesRbnSuffixedSpotter();
    void rejectsMalformedLine();
};

void TestSpotCollectorParser::parsesStandardDxLine() {
    SpotCollectorClient client;
    DxSpot spot;
    QVERIFY(client.parseDxSpotLineForTest(
        "DX de W1AW:    14250.0  9V1XX      USB Singapore   1830Z",
        spot
    ));
    QCOMPARE(spot.spotterCall, QStringLiteral("W1AW"));
    QCOMPARE(spot.freqMhz, 14.250);
    QCOMPARE(spot.dxCall, QStringLiteral("9V1XX"));
    QCOMPARE(spot.comment, QStringLiteral("USB Singapore"));
    QCOMPARE(spot.utcTime, QTime(18, 30));
    QCOMPARE(spot.source, QStringLiteral("SpotCollector"));
}

void TestSpotCollectorParser::parsesRbnSuffixedSpotter() {
    SpotCollectorClient client;
    DxSpot spot;
    QVERIFY(client.parseDxSpotLineForTest(
        "DX de N5XO-#:    7041.5  VK6APH    RTTY +5dB    1825Z",
        spot
    ));
    QCOMPARE(spot.source, QStringLiteral("RBN"));  // -# suffix promotes to RBN
}

void TestSpotCollectorParser::rejectsMalformedLine() {
    SpotCollectorClient client;
    DxSpot spot;
    QVERIFY(!client.parseDxSpotLineForTest(QStringLiteral("not a spot"), spot));
    QVERIFY(!client.parseDxSpotLineForTest(QString(), spot));
    QVERIFY(!client.parseDxSpotLineForTest(QStringLiteral("DX de FOO:"), spot));
}

QTEST_GUILESS_MAIN(TestSpotCollectorParser)
#include "tst_spot_collector_parser.moc"
