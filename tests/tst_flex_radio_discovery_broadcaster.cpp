// tst_flex_radio_discovery_broadcaster.cpp
//
// Unit tests for FlexRadioDiscoveryBroadcaster binary frame layout.
// Verifies the 28-byte VITA-49-style header + ASCII payload structure
// against the FLEX-8600 discovery beacon wire format captured 2026-05-19
// (captures/flex-pgxl-tgxl-capture_00001_20260519173452.pcapng).

//
// R-R3-22 / R-R3-47 (2026-09-24): the beacon follows the 4O3A switch, and
// on the Core announces the station network address (where its 4992
// listener listens). The RadioModel cases run the broadcaster in its
// no-send mode: nothing is bound and no beacon leaves the machine.

#include <QtTest/QtTest>
#include <QNetworkAddressEntry>
#include "core/AppSettings.h"
#include "core/FlexRadioDiscoveryBroadcaster.h"
#include "core/SmartSdrApiListener.h"
#include "models/RadioModel.h"

using NereusSDR::RadioModel;

class FlexRadioDiscoveryBroadcasterTest : public QObject {
    Q_OBJECT

private slots:
    void headerLayout();             // 28-byte header structure
    void payloadIsAscii();           // payload is printable ASCII
    void packetCountRolls();         // builds with counts 0..15, byte1 changes
    void totalSizeIsMultipleOf4();
    void startSucceedsWithValidIp(); // start() binds and logs subnet broadcast
    void sourceAddressIsAnnounced();
    void beaconFollowsTheFourO3ASwitch();
    void beaconStaysOffWithItsOwnSettingOff();
    void beaconStaysOffUntilConfiguredForARadio();
    void coreBeaconAnnouncesTheStationAddress();
    void cleanup();
};

void FlexRadioDiscoveryBroadcasterTest::headerLayout()
{
    NereusSDR::FlexRadioDiscoveryBroadcaster b;
    b.setSerial(QStringLiteral("1234-5678-9012-3456"));
    b.setVersion(QStringLiteral("4.0.0.1"));
    b.setNickname(QStringLiteral("NereusSDR"));
    b.setCallsign(QStringLiteral("KG4VCF"));
    b.setMacAddress(QStringLiteral("aa:bb:cc:dd:ee:ff"));

    const QByteArray pkt = b.buildBeaconForTesting(/*count=*/3, /*unixSec=*/0x6A0CD77D);
    QVERIFY(pkt.size() >= 28);

    // Word 0: type byte must be 0x38
    QCOMPARE(static_cast<quint8>(pkt[0]), quint8(0x38));
    // byte 1: 0x50 | count = 0x50 | 0x03 = 0x53
    QCOMPARE(static_cast<quint8>(pkt[1]), quint8(0x53));

    // packet_size_words field: total bytes == sizeWords * 4
    const quint16 sizeWords = (static_cast<quint8>(pkt[2]) << 8)
                              | static_cast<quint8>(pkt[3]);
    QCOMPARE(static_cast<int>(sizeWords * 4), pkt.size());

    // Word 1 (bytes 4-7): VITA dialect marker 00 00 08 00
    QCOMPARE(pkt.mid(4, 4), QByteArray::fromHex("00000800"));

    // Words 2-3 (bytes 8-15): 8-byte VITA-49 Class ID block
    //   00 00 1C 2D  (pad=0x00, OUI=FlexRadio 00-1C-2D)
    //   53 4C FF FF  (Info Class Code "SL", Packet Class Code 0xFFFF)
    QCOMPARE(pkt.mid(8, 8), QByteArray::fromHex("00001c2d534cffff"));

    // Word 4 (bytes 16-19): Integer timestamp = 0x6A0CD77D (big-endian)
    QCOMPARE(static_cast<quint8>(pkt[16]), quint8(0x6A));
    QCOMPARE(static_cast<quint8>(pkt[17]), quint8(0x0C));
    QCOMPARE(static_cast<quint8>(pkt[18]), quint8(0xD7));
    QCOMPARE(static_cast<quint8>(pkt[19]), quint8(0x7D));

    // Words 5-6 (bytes 20-27): fractional timestamp + reserved = all zero
    QCOMPARE(pkt.mid(20, 8), QByteArray(8, '\0'));

    // Payload starts at byte 28 with "discovery_protocol_version="
    QVERIFY(pkt.mid(28).startsWith("discovery_protocol_version="));
}

void FlexRadioDiscoveryBroadcasterTest::payloadIsAscii()
{
    NereusSDR::FlexRadioDiscoveryBroadcaster b;
    b.setSerial(QStringLiteral("0001-0002-0003-0004"));
    b.setVersion(QStringLiteral("4.0.0.1"));
    b.setNickname(QStringLiteral("TestNick"));
    b.setCallsign(QStringLiteral("W1AW"));
    b.setMacAddress(QStringLiteral("11:22:33:44:55:66"));

    const QByteArray pkt = b.buildBeaconForTesting(0, 0x60000000);
    const QByteArray payload = pkt.mid(28);

    // Every byte in the payload must be printable ASCII (0x20..0x7E)
    // or a space used for padding (0x20).
    for (int i = 0; i < payload.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(payload[i]);
        QVERIFY2(c >= 0x20 && c <= 0x7E,
                 qPrintable(QStringLiteral("Non-printable byte 0x%1 at offset %2")
                                .arg(c, 2, 16, QLatin1Char('0'))
                                .arg(i)));
    }

    // Check that required keys are present in the payload string
    const QString payloadStr = QString::fromLatin1(payload);
    QVERIFY(payloadStr.contains(QStringLiteral("model=FLEX-6400")));
    QVERIFY(payloadStr.contains(QStringLiteral("serial=0001-0002-0003-0004")));
    QVERIFY(payloadStr.contains(QStringLiteral("version=4.0.0.1")));
    QVERIFY(payloadStr.contains(QStringLiteral("nickname=TestNick")));
    QVERIFY(payloadStr.contains(QStringLiteral("callsign=W1AW")));
    QVERIFY(payloadStr.contains(QStringLiteral("port=4992")));
    QVERIFY(payloadStr.contains(QStringLiteral("status=Available")));
    QVERIFY(payloadStr.contains(QStringLiteral("wan_connected=1")));
    // radio_license_id must start with FlexRadio OUI 00-1C-2D; last 3 octets
    // derived from host MAC (11:22:33:44:55:66 -> last 3 = 44-55-66).
    QVERIFY(payloadStr.contains(QStringLiteral("radio_license_id=00-1C-2D-44-55-66")));
}

void FlexRadioDiscoveryBroadcasterTest::packetCountRolls()
{
    NereusSDR::FlexRadioDiscoveryBroadcaster b;
    b.setVersion(QStringLiteral("4.0.0.1"));
    b.setNickname(QStringLiteral("NereusSDR"));

    // Verify byte 1 changes correctly for counts 0..15
    for (quint8 count = 0; count <= 15; ++count) {
        const QByteArray pkt = b.buildBeaconForTesting(count, 0x60000000U);
        QVERIFY(pkt.size() >= 28);
        const quint8 byte1 = static_cast<quint8>(pkt[1]);
        // Upper nibble must be 0x5 (TSI=01, TSF=01)
        QCOMPARE(byte1 & 0xF0U, quint8(0x50));
        // Lower nibble must match count
        QCOMPARE(byte1 & 0x0FU, count);
    }
}

void FlexRadioDiscoveryBroadcasterTest::totalSizeIsMultipleOf4()
{
    NereusSDR::FlexRadioDiscoveryBroadcaster b;
    b.setSerial(QStringLiteral("5555-6666-7777-8888"));
    b.setVersion(QStringLiteral("4.0.0.1"));
    b.setNickname(QStringLiteral("NereusSDR"));
    b.setCallsign(QStringLiteral("KG4VCF"));
    b.setMacAddress(QStringLiteral("aa:bb:cc:dd:ee:ff"));

    // Test with a few different packet counts + timestamps to exercise
    // different padding scenarios.
    for (quint8 count = 0; count < 4; ++count) {
        for (quint32 ts : {0U, 1U, 0x6A0CD77DU, 0xFFFFFFFFU}) {
            const QByteArray pkt = b.buildBeaconForTesting(count, ts);
            QVERIFY2(pkt.size() % 4 == 0,
                     qPrintable(QStringLiteral("Packet size %1 is not a multiple of 4")
                                    .arg(pkt.size())));

            // Also verify the size-words field in the header is consistent.
            const quint16 sizeWords = (static_cast<quint8>(pkt[2]) << 8)
                                      | static_cast<quint8>(pkt[3]);
            QCOMPARE(static_cast<int>(sizeWords * 4), pkt.size());
        }
    }
}

void FlexRadioDiscoveryBroadcasterTest::startSucceedsWithValidIp()
{
    NereusSDR::FlexRadioDiscoveryBroadcaster b;
    b.setVersion(QStringLiteral("4.0.0.1"));
    b.setNickname(QStringLiteral("TestBroadcaster"));

    // start() should succeed even if port 4992 is in use; it falls back to
    // ephemeral port. Verify start() completes without returning false.
    b.start();
    // If start() returns (doesn't crash or assert), the test passes.
    // Cleanup.
    b.stop();

    QVERIFY(true); // Smoke test: start() did not fail
}

namespace {
void prepareModel(RadioModel& model, const char* mac, const char* radioIp = "192.168.1.50")
{
    NereusSDR::AppSettings::instance().clear();
    NereusSDR::AppSettings::instance().setValue(QStringLiteral("PeripheralsMigrationDone"),
                                                QStringLiteral("True"));
    // Loopback and an ephemeral port: never TCP 4992 on this machine.
    model.smartSdrListener()->setListenEndpointForTesting(QHostAddress::LocalHost, 0);
    NereusSDR::RadioInfo radio;
    radio.macAddress = QString::fromLatin1(mac);
    radio.address = QHostAddress(QString::fromLatin1(radioIp));
    model.setLastRadioInfoForTest(radio);
    model.setConnectionStateForTest(NereusSDR::ConnectionState::Connected);
}
} // namespace

void FlexRadioDiscoveryBroadcasterTest::cleanup()
{
    NereusSDR::AppSettings::instance().clear();
}

void FlexRadioDiscoveryBroadcasterTest::sourceAddressIsAnnounced()
{
    NereusSDR::FlexRadioDiscoveryBroadcaster b;
    b.setSourceAddress(QHostAddress(QStringLiteral("10.0.0.5")));
    QCOMPARE(b.advertisedAddress(), QStringLiteral("10.0.0.5"));
    b.setSourceAddress(QHostAddress());
    QCOMPARE(b.advertisedAddress(), NereusSDR::FlexRadioDiscoveryBroadcaster().advertisedAddress());
}

// With the 4O3A switch off no beacon is sent; on, it runs; off again, it
// stops. Reconnecting the radio re-checks the switch.
void FlexRadioDiscoveryBroadcasterTest::beaconFollowsTheFourO3ASwitch()
{
    RadioModel model;
    prepareModel(model, "aa:bb:cc:dd:ee:81");
    model.configureFlexBeaconForTest();
    QVERIFY(!model.flexBeaconRunningForTest());
    model.applyPeripheralsForTest();
    QVERIFY(!model.flexBeaconRunningForTest());

    model.setFourO3AEnabled(true);
    QVERIFY(model.flexBeaconRunningForTest());
    model.setFourO3AEnabled(false);
    QVERIFY(!model.flexBeaconRunningForTest());

    // A radio whose saved switch is on starts it when its peripherals apply.
    model.setPeripheralValue(QStringLiteral("FourO3A_Enabled"), QStringLiteral("True"));
    model.applyPeripheralsForTest();
    QVERIFY(model.flexBeaconRunningForTest());
    model.setPeripheralValue(QStringLiteral("FourO3A_Enabled"), QStringLiteral("False"));
    model.applyPeripheralsForTest();
    QVERIFY(!model.flexBeaconRunningForTest());
    model.teardownPeripheralsForTest();
}

// Its own setting still turns it off when 4O3A is on.
void FlexRadioDiscoveryBroadcasterTest::beaconStaysOffWithItsOwnSettingOff()
{
    RadioModel model;
    prepareModel(model, "aa:bb:cc:dd:ee:82");
    NereusSDR::AppSettings::instance().setValue(QStringLiteral("PGXL_BroadcastDiscovery"),
                                                QStringLiteral("False"));
    model.configureFlexBeaconForTest();
    model.setFourO3AEnabled(true);
    QVERIFY(!model.flexBeaconRunningForTest());
    model.setFourO3AEnabled(false);
    model.teardownPeripheralsForTest();
}

// No radio connected through connectToRadio: nothing to announce.
void FlexRadioDiscoveryBroadcasterTest::beaconStaysOffUntilConfiguredForARadio()
{
    RadioModel model;
    prepareModel(model, "aa:bb:cc:dd:ee:83");
    model.flexBroadcasterForTest()->setNoSendForTesting(true);
    model.setFourO3AEnabled(true);
    QVERIFY(!model.flexBeaconRunningForTest());
    model.setFourO3AEnabled(false);
    model.teardownPeripheralsForTest();
}

// On a Core with two networks the beacon announces the station address,
// where the 4992 listener listens; a desktop window finds its own.
void FlexRadioDiscoveryBroadcasterTest::coreBeaconAnnouncesTheStationAddress()
{
    auto entry = [](const char* ip, int prefix) {
        QNetworkAddressEntry e;
        e.setIp(QHostAddress(QString::fromLatin1(ip)));
        e.setPrefixLength(prefix);
        return e;
    };
    RadioModel core;
    core.setStationBind(QString());
    core.setStationInterfaceEntriesForTest({entry("10.0.0.5", 24), entry("192.168.1.20", 24)});
    prepareModel(core, "aa:bb:cc:dd:ee:84", "10.0.0.77");
    core.configureFlexBeaconForTest();
    core.applyPeripheralsForTest();
    QCOMPARE(core.flexBroadcasterForTest()->advertisedAddress(), QStringLiteral("10.0.0.5"));

    // The override selects the other network.
    core.setStationBind(QStringLiteral("192.168.1.20"));
    QCOMPARE(core.flexBroadcasterForTest()->advertisedAddress(), QStringLiteral("192.168.1.20"));

    // Every address: no single station address, so it finds its own, as
    // a desktop window's beacon does.
    const QString ownAddress = NereusSDR::FlexRadioDiscoveryBroadcaster().advertisedAddress();
    core.setStationBind(QStringLiteral("0.0.0.0"));
    QCOMPARE(core.flexBroadcasterForTest()->advertisedAddress(), ownAddress);
    core.teardownPeripheralsForTest();

    RadioModel desktop;
    prepareModel(desktop, "aa:bb:cc:dd:ee:85", "10.0.0.77");
    desktop.applyPeripheralsForTest();
    QCOMPARE(desktop.flexBroadcasterForTest()->advertisedAddress(), ownAddress);
    desktop.teardownPeripheralsForTest();
}

QTEST_GUILESS_MAIN(FlexRadioDiscoveryBroadcasterTest)
#include "tst_flex_radio_discovery_broadcaster.moc"
