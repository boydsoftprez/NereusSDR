// =================================================================
// tests/tst_lan_discovery_regex.cpp  (NereusSDR)
// =================================================================
//
// Unit tests for LanDiscovery regex parsing and deduplication.
// Verifies that the official FlexRadio announcement format is
// parsed correctly, malformed lines are rejected, and duplicate
// serials are deduplicated.
//
// R-R3-22 / R-R3-47 (2026-09-24): on the Core only announcements from the
// station network (or this computer) are heard; a desktop window hears
// every one. Senders are injected; no socket is opened.

#include <QtTest>
#include <QNetworkAddressEntry>
#include "core/LanDiscovery.h"
#include "core/StationNetwork.h"

class LanDiscoveryRegexTest : public QObject {
    Q_OBJECT
private slots:
    void parsesValidAnnouncement();
    void rejectsMalformedLine();
    void dedupsBySerial();
    void desktopHearsEveryNetwork();
    void coreHearsOnlyTheStationNetwork();
    void coreBeforeARadioHearsThisComputerOnly();
    void coreOverrideSelectsAnotherNetwork();
};

namespace {
QNetworkAddressEntry entry(const char* ip, int prefix)
{
    QNetworkAddressEntry e;
    e.setIp(QHostAddress(QString::fromLatin1(ip)));
    e.setPrefixLength(prefix);
    return e;
}

// A Core with two networks: the station network (radio at 10.0.0.77) and
// the house LAN.
NereusSDR::StationNetwork::StationBind twoNetworkCore()
{
    NereusSDR::StationNetwork::StationBind bind;
    bind.entriesForTest = QList<QNetworkAddressEntry>{entry("10.0.0.5", 24),
                                                      entry("192.168.1.20", 24)};
    bind.radio = QHostAddress(QStringLiteral("10.0.0.77"));
    return bind;
}

QString announcement(const char* ip, const char* serial)
{
    return QStringLiteral("PowerGeniusXL ip=%1 v=3.8.9 serial=%2 nickname=ShackAmp")
        .arg(QString::fromLatin1(ip), QString::fromLatin1(serial));
}
} // namespace

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

void LanDiscoveryRegexTest::desktopHearsEveryNetwork() {
    NereusSDR::LanDiscovery d;
    QSignalSpy spy(&d, &NereusSDR::LanDiscovery::deviceDiscovered);
    d.injectDatagramForTesting(announcement("192.168.1.43", "A"), 9008,
                               QHostAddress(QStringLiteral("192.168.1.43")));
    d.injectDatagramForTesting(announcement("10.0.0.43", "B"), 9008,
                               QHostAddress(QStringLiteral("10.0.0.43")));
    d.injectDatagramForTesting(announcement("172.16.0.43", "C"), 9010);
    QCOMPARE(spy.count(), 3);
}

void LanDiscoveryRegexTest::coreHearsOnlyTheStationNetwork() {
    NereusSDR::LanDiscovery d;
    d.setStationBind(twoNetworkCore());
    QSignalSpy spy(&d, &NereusSDR::LanDiscovery::deviceDiscovered);
    // From the house LAN: ignored.
    d.injectDatagramForTesting(announcement("192.168.1.43", "HOUSE"), 9008,
                               QHostAddress(QStringLiteral("192.168.1.43")));
    // From the station network, but announcing a house LAN address: ignored.
    d.injectDatagramForTesting(announcement("192.168.1.43", "STEER"), 9008,
                               QHostAddress(QStringLiteral("10.0.0.43")));
    // No known sender: ignored.
    d.injectDatagramForTesting(announcement("10.0.0.43", "UNKNOWN"), 9008);
    QCOMPARE(spy.count(), 0);
    // From the station network (as an IPv4-mapped sender too): heard.
    d.injectDatagramForTesting(announcement("10.0.0.43", "PGXL"), 9008,
                               QHostAddress(QStringLiteral("::ffff:10.0.0.43")));
    d.injectDatagramForTesting(QStringLiteral("TunerGeniusXL ip=10.0.0.44 v=1.2.17 serial=TG nickname=Tuner"),
                               9010, QHostAddress(QStringLiteral("10.0.0.44")));
    // From this computer: heard.
    d.injectDatagramForTesting(announcement("127.0.0.1", "LOCAL"), 9008,
                               QHostAddress(QHostAddress::LocalHost));
    QCOMPARE(spy.count(), 3);
    QCOMPARE(spy.at(0).at(4).toString(), QStringLiteral("PGXL"));
    QCOMPARE(spy.at(1).at(4).toString(), QStringLiteral("TG"));
    QCOMPARE(spy.at(2).at(4).toString(), QStringLiteral("LOCAL"));
}

void LanDiscoveryRegexTest::coreBeforeARadioHearsThisComputerOnly() {
    NereusSDR::LanDiscovery d;
    auto bind = twoNetworkCore();
    bind.radio = QHostAddress();
    d.setStationBind(bind);
    QSignalSpy spy(&d, &NereusSDR::LanDiscovery::deviceDiscovered);
    d.injectDatagramForTesting(announcement("10.0.0.43", "PGXL"), 9008,
                               QHostAddress(QStringLiteral("10.0.0.43")));
    QCOMPARE(spy.count(), 0);
    d.injectDatagramForTesting(announcement("127.0.0.1", "LOCAL"), 9008,
                               QHostAddress(QHostAddress::LocalHost));
    QCOMPARE(spy.count(), 1);
}

void LanDiscoveryRegexTest::coreOverrideSelectsAnotherNetwork() {
    NereusSDR::LanDiscovery d;
    auto bind = twoNetworkCore();
    bind.bindOverride = QStringLiteral("192.168.1.20");
    d.setStationBind(bind);
    QSignalSpy spy(&d, &NereusSDR::LanDiscovery::deviceDiscovered);
    d.injectDatagramForTesting(announcement("10.0.0.43", "RADIONET"), 9008,
                               QHostAddress(QStringLiteral("10.0.0.43")));
    QCOMPARE(spy.count(), 0);
    d.injectDatagramForTesting(announcement("192.168.1.43", "HOUSE"), 9008,
                               QHostAddress(QStringLiteral("192.168.1.43")));
    QCOMPARE(spy.count(), 1);

    // Every address: every network heard.
    NereusSDR::LanDiscovery every;
    bind.bindOverride = QStringLiteral("0.0.0.0");
    every.setStationBind(bind);
    QSignalSpy everySpy(&every, &NereusSDR::LanDiscovery::deviceDiscovered);
    every.injectDatagramForTesting(announcement("172.16.0.43", "FAR"), 9008,
                                   QHostAddress(QStringLiteral("172.16.0.43")));
    QCOMPARE(everySpy.count(), 1);
}

QTEST_GUILESS_MAIN(LanDiscoveryRegexTest)
#include "tst_lan_discovery_regex.moc"
