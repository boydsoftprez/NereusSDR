// =================================================================
// tests/tst_lan_discovery_regex.cpp  (NereusSDR)
// =================================================================
//
// Unit tests for LanDiscovery regex parsing and deduplication.
// Verifies that the official FlexRadio announcement format is
// parsed correctly, malformed lines are rejected, and duplicate
// serials are deduplicated.

#include <QtTest>
#include "core/LanDiscovery.h"

class LanDiscoveryRegexTest : public QObject {
    Q_OBJECT
private slots:
    void parsesValidAnnouncement();
    void rejectsMalformedLine();
    void dedupsBySerial();
};

void LanDiscoveryRegexTest::parsesValidAnnouncement() {
    NereusSDR::LanDiscovery d;
    QSignalSpy spy(&d, &NereusSDR::LanDiscovery::deviceDiscovered);
    d.injectDatagramForTesting("PowerGeniusXL ip=192.168.1.43 v=3.8.9 serial=PGXL5678 nickname=ShackAmp");
    QCOMPARE(spy.count(), 1);
    auto args = spy.takeFirst();
    QCOMPARE(args.at(0).toString(), QString("PowerGeniusXL"));
    QCOMPARE(args.at(1).toString(), QString("192.168.1.43"));
    QCOMPARE(args.at(3).toString(), QString("3.8.9"));
    QCOMPARE(args.at(4).toString(), QString("PGXL5678"));
    QCOMPARE(args.at(5).toString(), QString("ShackAmp"));
}

void LanDiscoveryRegexTest::rejectsMalformedLine() {
    NereusSDR::LanDiscovery d;
    QSignalSpy spy(&d, &NereusSDR::LanDiscovery::deviceDiscovered);
    d.injectDatagramForTesting("garbage with no fields");
    d.injectDatagramForTesting("PowerGeniusXL ip=invalid v=3.8.9 serial=X nickname=Y");
    QCOMPARE(spy.count(), 0);
}

void LanDiscoveryRegexTest::dedupsBySerial() {
    NereusSDR::LanDiscovery d;
    QSignalSpy spy(&d, &NereusSDR::LanDiscovery::deviceDiscovered);
    QString line = "PowerGeniusXL ip=192.168.1.43 v=3.8.9 serial=PGXL5678 nickname=ShackAmp";
    d.injectDatagramForTesting(line);
    d.injectDatagramForTesting(line);
    d.injectDatagramForTesting(line);
    QCOMPARE(spy.count(), 1);
}

QTEST_GUILESS_MAIN(LanDiscoveryRegexTest)
#include "tst_lan_discovery_regex.moc"
