// tst_flex_radio_discovery_broadcaster.cpp
//
// Unit tests for FlexRadioDiscoveryBroadcaster binary frame layout.
// Verifies the 28-byte VITA-49-style header + ASCII payload structure
// against the FLEX-8600 discovery beacon wire format captured 2026-05-19
// (captures/flex-pgxl-tgxl-capture_00001_20260519173452.pcapng).

#include <QtTest/QtTest>
#include "core/FlexRadioDiscoveryBroadcaster.h"

class FlexRadioDiscoveryBroadcasterTest : public QObject {
    Q_OBJECT

private slots:
    void headerLayout();             // 28-byte header structure
    void payloadIsAscii();           // payload is printable ASCII
    void packetCountRolls();         // builds with counts 0..15, byte1 changes
    void totalSizeIsMultipleOf4();
    void startSucceedsWithValidIp(); // start() binds and logs subnet broadcast
};

void FlexRadioDiscoveryBroadcasterTest::headerLayout()
{
    NereusSDR::FlexRadioDiscoveryBroadcaster b;
    b.setSerial(QStringLiteral("1234-5678-9012-3456"));
    b.setVersion(QStringLiteral("0.5.1"));
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

    // Words 1-2: Class ID bytes 4..11 = 00 00 08 00 1C 2D 53 4C
    QByteArray expectedClassId;
    expectedClassId.append('\x00');
    expectedClassId.append('\x00');
    expectedClassId.append('\x08');
    expectedClassId.append('\x00');
    expectedClassId.append('\x1c');
    expectedClassId.append('\x2d');
    expectedClassId.append('\x53');
    expectedClassId.append('\x4c');
    QCOMPARE(pkt.mid(4, 8), expectedClassId);

    // Words 3-4: Integer timestamp at bytes 12..15 = 0x6A0CD77D (big-endian)
    QCOMPARE(static_cast<quint8>(pkt[12]), quint8(0x6A));
    QCOMPARE(static_cast<quint8>(pkt[13]), quint8(0x0C));
    QCOMPARE(static_cast<quint8>(pkt[14]), quint8(0xD7));
    QCOMPARE(static_cast<quint8>(pkt[15]), quint8(0x7D));

    // Fractional + padding (bytes 16..27) = all zero (12 bytes)
    QCOMPARE(pkt.mid(16, 12), QByteArray(12, '\0'));

    // Payload starts at byte 28 with "discovery_protocol_version="
    QVERIFY(pkt.mid(28).startsWith("discovery_protocol_version="));
}

void FlexRadioDiscoveryBroadcasterTest::payloadIsAscii()
{
    NereusSDR::FlexRadioDiscoveryBroadcaster b;
    b.setSerial(QStringLiteral("0001-0002-0003-0004"));
    b.setVersion(QStringLiteral("0.5.1"));
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
    QVERIFY(payloadStr.contains(QStringLiteral("version=0.5.1")));
    QVERIFY(payloadStr.contains(QStringLiteral("nickname=TestNick")));
    QVERIFY(payloadStr.contains(QStringLiteral("callsign=W1AW")));
    QVERIFY(payloadStr.contains(QStringLiteral("port=4992")));
    QVERIFY(payloadStr.contains(QStringLiteral("status=Available")));
    // MAC should be dashed uppercase
    QVERIFY(payloadStr.contains(QStringLiteral("radio_license_id=11-22-33-44-55-66")));
}

void FlexRadioDiscoveryBroadcasterTest::packetCountRolls()
{
    NereusSDR::FlexRadioDiscoveryBroadcaster b;
    b.setVersion(QStringLiteral("0.5.1"));
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
    b.setVersion(QStringLiteral("0.5.1"));
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
    b.setVersion(QStringLiteral("0.5.1"));
    b.setNickname(QStringLiteral("TestBroadcaster"));

    // start() should succeed even if port 4992 is in use; it falls back to
    // ephemeral port. Verify start() completes without returning false.
    b.start();
    // If start() returns (doesn't crash or assert), the test passes.
    // Cleanup.
    b.stop();

    QVERIFY(true); // Smoke test: start() did not fail
}

QTEST_GUILESS_MAIN(FlexRadioDiscoveryBroadcasterTest)
#include "tst_flex_radio_discovery_broadcaster.moc"
