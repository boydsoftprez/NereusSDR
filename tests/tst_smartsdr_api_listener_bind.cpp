// no-port-check: NereusSDR-original. R-R3-22 / R-R3-47: the Core's station
// listeners accept connections on the station network only, by one rule
// (StationNetwork::StationBind): nereusd.conf's station_bind, else the
// radio's subnet, 127.0.0.1 always, 127.0.0.1 alone before a radio, and a
// move when the radio's address changes. A desktop window binds as before.
//
// Every listener here binds to loopback (127.0.0.1, and ::1 standing in
// for a second network); other networks are injected interface lists.
// The machine's network configuration is never touched and no radio or
// accessory is contacted.
// J.J. Boyd (KG4VCF), September 2026; AI-assisted via Anthropic Claude Code.
// 2026-09-24: R-R3-26: station_bind = "::" takes IPv4 and IPv6 clients, as
// remote_bind does. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
#include <QtTest/QtTest>
#include <QNetworkAddressEntry>
#include <QTcpServer>
#include <QTcpSocket>

#include "core/AppSettings.h"
#include "core/SmartSdrApiListener.h"
#include "core/StationNetwork.h"
#include "core/StationTciController.h"
#include "models/RadioModel.h"

using namespace NereusSDR;
using StationNetwork::StationBind;

namespace {

QNetworkAddressEntry entry(const char* ip, int prefix)
{
    QNetworkAddressEntry e;
    e.setIp(QHostAddress(QString::fromLatin1(ip)));
    e.setPrefixLength(prefix);
    return e;
}

// Two networks on one Core: the station (radio) network and the house LAN.
QList<QNetworkAddressEntry> twoNetworks()
{
    return {entry("10.0.0.5", 24), entry("192.168.1.20", 24)};
}

const QHostAddress kLoopback(QHostAddress::LocalHost);
const QHostAddress kLoopback6(QHostAddress::LocalHostIPv6);

bool ipv6LoopbackAvailable()
{
    QTcpServer probe;
    return probe.listen(kLoopback6, 0);
}

// Connects to `address`:`port` and returns the first line the listener
// sends (the SmartSDR "V<version>" banner), or empty when nothing answers.
QByteArray bannerFrom(const QHostAddress& address, quint16 port)
{
    QTcpSocket client;
    client.connectToHost(address, port);
    if (!client.waitForConnected(2000)) { return {}; }
    if (!QTest::qWaitFor([&] { return client.canReadLine(); }, 2000)) { return {}; }
    return client.readLine().trimmed();
}

} // namespace

class SmartSdrApiListenerBindTest : public QObject {
    Q_OBJECT

private slots:
    void init()
    {
        AppSettings::instance().clear();
        AppSettings::instance().setValue(QStringLiteral("PeripheralsMigrationDone"),
                                         QStringLiteral("True"));
    }
    void cleanup() { AppSettings::instance().clear(); }

    // The rule itself, on a Core with two networks.
    void stationRuleFollowsTheRadiosNetwork()
    {
        StationBind bind;
        bind.entriesForTest = twoNetworks();
        // Before a radio: this computer only, and only this computer heard.
        QCOMPARE(bind.listenAddresses(), QList<QHostAddress>{kLoopback});
        QVERIFY(bind.stationAddress().isNull());
        QVERIFY(bind.acceptsPeer(kLoopback));
        QVERIFY(!bind.acceptsPeer(QHostAddress(QStringLiteral("192.168.1.43"))));

        // The radio on the house LAN: that network, not the other.
        bind.radio = QHostAddress(QStringLiteral("192.168.1.50"));
        QCOMPARE(bind.listenAddresses(),
                 (QList<QHostAddress>{QHostAddress(QStringLiteral("192.168.1.20")), kLoopback}));
        QVERIFY(bind.acceptsPeer(QHostAddress(QStringLiteral("192.168.1.43"))));
        QVERIFY(bind.acceptsPeer(QHostAddress(QStringLiteral("::ffff:192.168.1.43"))));
        QVERIFY(!bind.acceptsPeer(QHostAddress(QStringLiteral("10.0.0.9"))));
        QVERIFY(bind.acceptsPeer(kLoopback));
        QVERIFY(!bind.acceptsPeer(QHostAddress()));

        // The radio moves to the other network: so does everything.
        bind.radio = QHostAddress(QStringLiteral("10.0.0.77"));
        QCOMPARE(bind.listenAddresses(),
                 (QList<QHostAddress>{QHostAddress(QStringLiteral("10.0.0.5")), kLoopback}));
        QVERIFY(bind.acceptsPeer(QHostAddress(QStringLiteral("10.0.0.9"))));
        QVERIFY(!bind.acceptsPeer(QHostAddress(QStringLiteral("192.168.1.43"))));

        // A radio on no network of this computer: this computer only.
        bind.radio = QHostAddress(QStringLiteral("172.16.0.9"));
        QCOMPARE(bind.listenAddresses(), QList<QHostAddress>{kLoopback});
        QVERIFY(!bind.acceptsPeer(QHostAddress(QStringLiteral("172.16.0.1"))));
    }

    // nereusd.conf's station_bind selects another network, whatever the radio.
    void overrideSelectsAnotherNetwork()
    {
        StationBind bind;
        bind.entriesForTest = twoNetworks();
        bind.radio = QHostAddress(QStringLiteral("192.168.1.50"));
        bind.bindOverride = QStringLiteral("10.0.0.5");
        QCOMPARE(bind.listenAddresses(),
                 (QList<QHostAddress>{QHostAddress(QStringLiteral("10.0.0.5")), kLoopback}));
        QVERIFY(bind.acceptsPeer(QHostAddress(QStringLiteral("10.0.0.9"))));
        QVERIFY(!bind.acceptsPeer(QHostAddress(QStringLiteral("192.168.1.43"))));

        // Before any radio the override already applies.
        bind.radio = QHostAddress();
        QCOMPARE(bind.stationAddress(), QHostAddress(QStringLiteral("10.0.0.5")));

        // Every address: used alone, every network heard.
        bind.bindOverride = QStringLiteral("0.0.0.0");
        QVERIFY(bind.everyAddress());
        QCOMPARE(bind.listenAddresses(), QList<QHostAddress>{QHostAddress(QHostAddress::AnyIPv4)});
        QVERIFY(bind.acceptsPeer(QHostAddress(QStringLiteral("172.16.0.1"))));

        // This computer only.
        bind.bindOverride = QStringLiteral("127.0.0.1");
        QCOMPARE(bind.listenAddresses(), QList<QHostAddress>{kLoopback});
        QVERIFY(!bind.acceptsPeer(QHostAddress(QStringLiteral("10.0.0.9"))));

        // An address that is not this computer's: only that address heard.
        bind.bindOverride = QStringLiteral("10.9.9.9");
        QVERIFY(bind.acceptsPeer(QHostAddress(QStringLiteral("10.9.9.9"))));
        QVERIFY(!bind.acceptsPeer(QHostAddress(QStringLiteral("10.9.9.8"))));
    }

    // station_bind = "::" asks for every address of both families, the way
    // remote_bind = "::" does. Qt binds a parsed "::" IPv6-only, which would
    // refuse the station network's IPv4 amplifiers and tuners; the rule maps
    // it to Qt's dual-stack any-address, and one listener takes both.
    void everyAddressOverrideTakesIpv4AndIpv6()
    {
        StationBind bind;
        bind.entriesForTest = twoNetworks();
        bind.bindOverride = QStringLiteral("::");
        QVERIFY(bind.everyAddress());
        QCOMPARE(bind.listenAddresses(), QList<QHostAddress>{QHostAddress(QHostAddress::Any)});
        QVERIFY(bind.acceptsPeer(QHostAddress(QStringLiteral("172.16.0.1"))));

        SmartSdrApiListener listener;
        listener.setListenEndpointForTesting(QHostAddress(QHostAddress::AnyIPv4), 0);
        listener.setStationBind(bind);
        QVERIFY(listener.start());
        const quint16 port = listener.serverPort();
        QVERIFY2(bannerFrom(kLoopback, port).startsWith('V'),
                 "IPv4 client refused by the \"::\" station listener");
        if (!ipv6LoopbackAvailable()) {
            listener.stop();
            QSKIP("This host has no IPv6 loopback; the IPv4 half passed.");
        }
        QVERIFY2(bannerFrom(kLoopback6, port).startsWith('V'),
                 "IPv6 client refused by the \"::\" station listener");
        listener.stop();
    }

    // A desktop window never sets the rule: the listener listens where it
    // always has (the endpoint it was given), whatever a Core would choose.
    void desktopListenerBindsAsBefore()
    {
        SmartSdrApiListener listener;
        listener.setListenEndpointForTesting(kLoopback, 0);
        QVERIFY(listener.start());
        QCOMPARE(listener.listenAddresses(), QList<QHostAddress>{kLoopback});
        QVERIFY(bannerFrom(kLoopback, listener.serverPort()).startsWith('V'));
        listener.stop();
        QVERIFY(listener.listenAddresses().isEmpty());
    }

    // The Core before a radio connects: this computer only.
    void coreListenerBeforeARadioListensOnThisComputerOnly()
    {
        SmartSdrApiListener listener;
        listener.setListenEndpointForTesting(QHostAddress(QHostAddress::AnyIPv4), 0);
        StationBind bind;
        bind.entriesForTest = twoNetworks();
        listener.setStationBind(bind);
        QVERIFY(listener.start());
        QCOMPARE(listener.listenAddresses(), QList<QHostAddress>{kLoopback});
        QVERIFY(bannerFrom(kLoopback, listener.serverPort()).startsWith('V'));
    }

    // The station address and this computer, each answering on one port.
    void coreListenerAnswersOnTheStationAddressAndThisComputer()
    {
        if (!ipv6LoopbackAvailable()) { QSKIP("No IPv6 loopback on this machine."); }
        SmartSdrApiListener listener;
        listener.setListenEndpointForTesting(QHostAddress(QHostAddress::AnyIPv4), 0);
        StationBind bind;
        bind.bindOverride = QStringLiteral("::1"); // The station network's stand-in.
        listener.setStationBind(bind);
        QVERIFY(listener.start());
        QCOMPARE(listener.listenAddresses(), (QList<QHostAddress>{kLoopback6, kLoopback}));
        QVERIFY(!listener.listenAddresses().contains(QHostAddress(QHostAddress::AnyIPv4)));
        const quint16 port = listener.serverPort();
        QVERIFY(bannerFrom(kLoopback6, port).startsWith('V'));
        QVERIFY(bannerFrom(kLoopback, port).startsWith('V'));
    }

    // A station address this computer does not have: refused plainly, and
    // the listener listens nowhere rather than on this computer alone.
    void coreListenerOnAMissingAddressListensNowhere()
    {
        SmartSdrApiListener listener;
        listener.setListenEndpointForTesting(QHostAddress(QHostAddress::AnyIPv4), 0);
        StationBind bind;
        bind.entriesForTest = QList<QNetworkAddressEntry>{entry("192.0.2.10", 24)};
        bind.radio = QHostAddress(QStringLiteral("192.0.2.50")); // TEST-NET-1, never local.
        listener.setStationBind(bind);
        QCOMPARE(bind.listenAddresses(),
                 (QList<QHostAddress>{QHostAddress(QStringLiteral("192.0.2.10")), kLoopback}));
        QVERIFY(!listener.start());
        QVERIFY(!listener.isListening());
        QVERIFY(listener.listenAddresses().isEmpty());
        QVERIFY(!listener.lastListenError().isEmpty());
    }

    // The radio's address changes: the running listener moves, and the
    // amplifier connected on the old network is dropped (it reconnects).
    // The same rule again changes nothing.
    void coreListenerMovesWhenTheRadioMoves()
    {
        if (!ipv6LoopbackAvailable()) { QSKIP("No IPv6 loopback on this machine."); }
        SmartSdrApiListener listener;
        listener.setListenEndpointForTesting(QHostAddress(QHostAddress::AnyIPv4), 0);
        StationBind bind;
        listener.setStationBind(bind);
        QVERIFY(listener.start());
        QCOMPARE(listener.listenAddresses(), QList<QHostAddress>{kLoopback});

        QTcpSocket amp;
        amp.connectToHost(kLoopback, listener.serverPort());
        QVERIFY(amp.waitForConnected(2000));
        QVERIFY(QTest::qWaitFor([&] { return amp.canReadLine(); }, 2000));

        listener.setStationBind(bind); // Unchanged: the amp stays.
        QTest::qWait(50);
        QCOMPARE(amp.state(), QAbstractSocket::ConnectedState);

        bind.bindOverride = QStringLiteral("::1");
        listener.setStationBind(bind);
        QVERIFY(listener.isListening());
        QCOMPARE(listener.listenAddresses(), (QList<QHostAddress>{kLoopback6, kLoopback}));
        QVERIFY(QTest::qWaitFor([&] { return amp.state() == QAbstractSocket::UnconnectedState; },
                                2000));
        QVERIFY(bannerFrom(kLoopback6, listener.serverPort()).startsWith('V'));

        // Moving again retires the extra server that accepted a client,
        // without touching the client twice.
        bind.bindOverride.clear();
        listener.setStationBind(bind);
        QCOMPARE(listener.listenAddresses(), QList<QHostAddress>{kLoopback});
        listener.stop();
    }

    // A stopped listener only remembers the rule; it starts on it later.
    void stoppedListenerKeepsTheRuleForItsStart()
    {
        SmartSdrApiListener listener;
        listener.setListenEndpointForTesting(QHostAddress(QHostAddress::AnyIPv4), 0);
        StationBind bind;
        bind.bindOverride = QStringLiteral("127.0.0.1");
        listener.setStationBind(bind);
        QVERIFY(!listener.isListening());
        QVERIFY(listener.listenAddresses().isEmpty());
        QVERIFY(listener.start());
        QCOMPARE(listener.listenAddresses(), QList<QHostAddress>{kLoopback});
    }

    // The Core's model: one rule reaches the 4992 listener and the station
    // TCI server, before and after the radio connects, and moves both when
    // the radio's address changes.
    void coreModelAppliesOneRuleToEveryStationListener()
    {
        RadioModel model;
        model.setStationBind(QString());
        model.setStationInterfaceEntriesForTest(twoNetworks());
        model.enableStationAccessoryIdentity();
        model.enableStationTci(QString());
        model.smartSdrListener()->setListenEndpointForTesting(QHostAddress(QHostAddress::AnyIPv4), 0);

        QVERIFY(model.stationBind().has_value());
        QCOMPARE(model.stationBind()->listenAddresses(), QList<QHostAddress>{kLoopback});
        QCOMPARE(model.stationTciController()->wantedAddresses(), QList<QHostAddress>{kLoopback});

        RadioInfo radio;
        radio.macAddress = QStringLiteral("aa:bb:cc:dd:ee:72");
        radio.address = QHostAddress(QStringLiteral("192.168.1.50"));
        model.setLastRadioInfoForTest(radio);
        model.setConnectionStateForTest(ConnectionState::Connected);
        model.applyPeripheralsForTest();
        const QList<QHostAddress> houseLan{QHostAddress(QStringLiteral("192.168.1.20")), kLoopback};
        QCOMPARE(model.stationBind()->listenAddresses(), houseLan);
        QCOMPARE(model.stationTciController()->wantedAddresses(), houseLan);

        radio.address = QHostAddress(QStringLiteral("10.0.0.77"));
        model.setLastRadioInfoForTest(radio);
        model.applyPeripheralsForTest();
        const QList<QHostAddress> stationLan{QHostAddress(QStringLiteral("10.0.0.5")), kLoopback};
        QCOMPARE(model.stationBind()->listenAddresses(), stationLan);
        QCOMPARE(model.stationTciController()->wantedAddresses(), stationLan);

        // The 4O3A switch on: the listener starts on the rule. The injected
        // networks are not this computer's, so the override puts it on
        // this computer, where the test can reach it.
        model.setStationBind(QStringLiteral("127.0.0.1"));
        model.setFourO3AEnabled(true);
        QVERIFY(model.smartSdrListener()->isListening());
        QCOMPARE(model.smartSdrListener()->listenAddresses(), QList<QHostAddress>{kLoopback});
        QCOMPARE(model.stationTciController()->wantedAddresses(), QList<QHostAddress>{kLoopback});
        model.setFourO3AEnabled(false);
        model.teardownPeripheralsForTest();
    }

    // A desktop window's model has no rule: its listener binds as before.
    void desktopModelHasNoStationRule()
    {
        RadioModel model;
        QVERIFY(!model.stationBind().has_value());
        model.setStationInterfaceEntriesForTest(twoNetworks()); // Ignored.
        QVERIFY(!model.stationBind().has_value());
        model.smartSdrListener()->setListenEndpointForTesting(kLoopback, 0);
        RadioInfo radio;
        radio.macAddress = QStringLiteral("aa:bb:cc:dd:ee:73");
        radio.address = QHostAddress(QStringLiteral("192.168.1.50"));
        model.setLastRadioInfoForTest(radio);
        model.setConnectionStateForTest(ConnectionState::Connected);
        model.setFourO3AEnabled(true);
        QVERIFY(model.smartSdrListener()->isListening());
        QCOMPARE(model.smartSdrListener()->listenAddresses(), QList<QHostAddress>{kLoopback});
        model.setFourO3AEnabled(false);
        model.teardownPeripheralsForTest();
    }
};

QTEST_GUILESS_MAIN(SmartSdrApiListenerBindTest)
#include "tst_smartsdr_api_listener_bind.moc"
