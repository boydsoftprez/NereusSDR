// Focused unit tests for untrusted Core LAN announcement codec/cache.
#include <QtTest>

#include "core/session/StationLanAnnouncement.h"
#include "core/session/StationLanCache.h"

using namespace NereusSDR;

namespace {

QString pin(int seed = 0)
{
    QString out;
    for (int i = 0; i < 32; ++i) {
        if (i) {
            out += QLatin1Char(':');
        }
        out += QString::number((seed + i) & 0xff, 16).rightJustified(2, QLatin1Char('0')).toUpper();
    }
    return out;
}

StationLanAnnouncement value(int seed = 0)
{
    return {47910, pin(seed), QStringLiteral("Rock 5C"), QStringLiteral("Saturn G2"),
            QStringLiteral("AA:BB:CC:DD:EE:FF"), true};
}

QByteArray wire(const StationLanAnnouncement& announcement)
{
    QString error;
    const QByteArray result = encodeStationLanAnnouncement(announcement, &error);
    Q_ASSERT(!result.isEmpty());
    Q_ASSERT(error.isEmpty());
    return result;
}

// iPhone app Task 16: the same Core, announcing schema 2.
StationLanAnnouncement valueV2(int seed = 0)
{
    StationLanAnnouncement announcement = value(seed);
    announcement.schema = kStationLanAnnouncementSchema2;
    announcement.claimed = true;
    for (int i = 0; i < kStationLanIdentityBytes; ++i) {
        announcement.identity.append(static_cast<char>((seed + 0x40 + i) & 0xff));
    }
    announcement.label = QStringLiteral("KG4VCF/shack");
    announcement.pairing = StationLanPairing::Code;
    return announcement;
}

int connectionOffset(const QByteArray& bytes)
{
    return 8 + 95 + 1 + static_cast<quint8>(bytes.at(8 + 95));
}

} // namespace

class TstStationLanCache : public QObject {
    Q_OBJECT

private slots:
    void codecRoundTripAndStrictRejections()
    {
        const StationLanAnnouncement source = value();
        QString error;
        const QByteArray encoded = wire(source);
        const auto decoded = decodeStationLanAnnouncement(encoded, &error);
        QVERIFY2(decoded.has_value(), qPrintable(error));
        QCOMPARE(*decoded, source);

        StationLanAnnouncement nonBmp = source;
        nonBmp.coreName = QString::fromUtf8("Core \xF0\x9F\x9A\x80");
        const QByteArray nonBmpWire = encodeStationLanAnnouncement(nonBmp, &error);
        QVERIFY2(!nonBmpWire.isEmpty(), qPrintable(error));
        const auto nonBmpDecoded = decodeStationLanAnnouncement(nonBmpWire, &error);
        QVERIFY2(nonBmpDecoded.has_value(), qPrintable(error));
        QCOMPARE(*nonBmpDecoded, nonBmp);

        StationLanAnnouncement malformedSurrogate = source;
        malformedSurrogate.coreName = QString(QChar(0xd800));
        QVERIFY(encodeStationLanAnnouncement(malformedSurrogate, &error).isEmpty());
        QCOMPARE(error, QStringLiteral("Station LAN announcement has an invalid Core name."));
        malformedSurrogate.coreName = QString(QChar(0xdc00));
        QVERIFY(encodeStationLanAnnouncement(malformedSurrogate, &error).isEmpty());
        QCOMPARE(error, QStringLiteral("Station LAN announcement has an invalid Core name."));

        StationLanAnnouncement maximum = source;
        maximum.coreName = QString(kStationLanMaxCoreNameBytes, QLatin1Char('C'));
        maximum.radioName = QString(kStationLanMaxRadioNameBytes, QLatin1Char('R'));
        QVERIFY(!encodeStationLanAnnouncement(maximum, &error).isEmpty());
        maximum.coreName.append(QLatin1Char('C'));
        QVERIFY(encodeStationLanAnnouncement(maximum, &error).isEmpty());
        QCOMPARE(error, QStringLiteral("Station LAN announcement has an invalid Core name."));

        StationLanAnnouncement offline = source;
        offline.radioConnected = false;
        offline.radioName.clear();
        offline.radioMac = QStringLiteral("00:00:00:00:00:00");
        QVERIFY(!encodeStationLanAnnouncement(offline, &error).isEmpty());
        offline.radioConnected = true;
        QVERIFY(encodeStationLanAnnouncement(offline, &error).isEmpty());
        QCOMPARE(error, QStringLiteral("Station LAN announcement has an invalid radio name."));
        offline.radioName = QStringLiteral("Radio");
        QVERIFY(encodeStationLanAnnouncement(offline, &error).isEmpty());
        QCOMPARE(error, QStringLiteral("Station LAN announcement has an invalid radio MAC."));

        QByteArray invalid = encoded;
        invalid[4] = '\x03';
        QVERIFY(!decodeStationLanAnnouncement(invalid, &error));
        QCOMPARE(error, QStringLiteral("Station LAN announcement has an unsupported schema."));
        // iPhone app Task 16: schema 2 is known, but these bytes stop where
        // schema 1 does, short of schema 2's fields.
        invalid = encoded;
        invalid[4] = '\x02';
        QVERIFY(!decodeStationLanAnnouncement(invalid, &error));
        QCOMPARE(error, QStringLiteral("Station LAN announcement is malformed."));
        invalid = encoded;
        invalid[5] = '\x02';
        QVERIFY(!decodeStationLanAnnouncement(invalid, &error));
        QCOMPARE(error, QStringLiteral("Station LAN announcement has an unsupported service."));
        invalid = encoded;
        invalid[6] = invalid[7] = '\0';
        QVERIFY(!decodeStationLanAnnouncement(invalid, &error));
        QCOMPARE(error, QStringLiteral("Station LAN announcement has an invalid control port."));
        invalid = encoded;
        invalid[8] = 'a';
        QVERIFY(!decodeStationLanAnnouncement(invalid, &error));
        QCOMPARE(error, QStringLiteral("Station LAN announcement has an invalid fingerprint."));
        invalid = encoded;
        invalid[8 + 95 + 1] = '\n';
        QVERIFY(!decodeStationLanAnnouncement(invalid, &error));
        QCOMPARE(error, QStringLiteral("Station LAN announcement has an invalid Core name."));
        invalid = encoded;
        invalid[8 + 95 + 1] = static_cast<char>(0xff);
        QVERIFY(!decodeStationLanAnnouncement(invalid, &error));
        QCOMPARE(error, QStringLiteral("Station LAN announcement has an invalid Core name."));
        invalid = encoded;
        invalid[connectionOffset(invalid)] = '\x02';
        QVERIFY(!decodeStationLanAnnouncement(invalid, &error));
        QCOMPARE(error, QStringLiteral("Station LAN announcement has an invalid radio connection state."));
        invalid = encoded;
        const int macOffset = connectionOffset(invalid) + 2
            + static_cast<quint8>(invalid.at(connectionOffset(invalid) + 1));
        invalid[macOffset] = 'a';
        QVERIFY(!decodeStationLanAnnouncement(invalid, &error));
        QCOMPARE(error, QStringLiteral("Station LAN announcement has an invalid radio MAC."));
        invalid = encoded;
        invalid.append('x');
        QVERIFY(!decodeStationLanAnnouncement(invalid, &error));
        QCOMPARE(error, QStringLiteral("Station LAN announcement is malformed."));
        invalid = encoded;
        invalid.append(QByteArray(kStationLanMaxDatagramBytes, 'x'));
        QVERIFY(!decodeStationLanAnnouncement(invalid, &error));
        QCOMPARE(error, QStringLiteral("Station LAN announcement is too large."));
    }

    void schemaTwoRoundTripSizesAndStrictRejections()
    {
        QString error;
        const StationLanAnnouncement source = valueV2();
        const QByteArray encoded = wire(source);
        QCOMPARE(static_cast<quint8>(encoded.at(4)), kStationLanAnnouncementSchema2);
        const auto decoded = decodeStationLanAnnouncement(encoded, &error);
        QVERIFY2(decoded, qPrintable(error));
        QCOMPARE(*decoded, source);
        QCOMPARE(decoded->displayName(), QStringLiteral("KG4VCF/shack"));

        // Schema 1 still decodes, with none of schema 2's fields.
        const auto older = decodeStationLanAnnouncement(wire(value()), &error);
        QVERIFY2(older, qPrintable(error));
        QCOMPARE(older->schema, kStationLanAnnouncementSchema1);
        QVERIFY(older->identity.isEmpty());
        QVERIFY(older->label.isEmpty());
        QCOMPARE(older->pairing, StationLanPairing::Closed);
        QCOMPARE(older->displayName(), QStringLiteral("Rock 5C"));

        // Every field at its limit: 481 bytes (iPhone app Task 71: with the
        // device count; iPhone app plan Task 25: and the radio state), and a
        // label is never cut.
        StationLanAnnouncement largest = valueV2();
        largest.devicesConnected = kStationLanMaxDevicesConnected;
        largest.radio = largest.radioConnected ? StationLanRadio::Connected
                                               : StationLanRadio::Waiting;
        largest.coreName = QString(kStationLanMaxCoreNameBytes, QLatin1Char('C'));
        largest.radioName = QString(kStationLanMaxRadioNameBytes, QLatin1Char('R'));
        largest.label = QString(32, QLatin1Char('K')) + QLatin1Char('/')
            + QString(32, QLatin1Char('s'));
        QCOMPARE(largest.label.size(), kStationLanMaxLabelBytes);
        const QByteArray largestWire = wire(largest);
        QCOMPARE(largestWire.size(), kStationLanMaxSchema2DatagramBytes);
        QCOMPARE(*decodeStationLanAnnouncement(largestWire, &error), largest);
        largest.label.append(QLatin1Char('x'));
        QVERIFY(encodeStationLanAnnouncement(largest, &error).isEmpty());
        QCOMPARE(error, QStringLiteral("Station LAN announcement has an invalid label."));

        for (const QString& label : {QStringLiteral("KG4VCF shack"), QStringLiteral("KG4VCF/sh\u00e4ck"),
                                     QStringLiteral("KG4VCF.shack")}) {
            StationLanAnnouncement bad = valueV2();
            bad.label = label;
            QVERIFY2(encodeStationLanAnnouncement(bad, &error).isEmpty(), qPrintable(label));
        }
        StationLanAnnouncement shortIdentity = valueV2();
        shortIdentity.identity.chop(1);
        QVERIFY(encodeStationLanAnnouncement(shortIdentity, &error).isEmpty());
        QCOMPARE(error, QStringLiteral("Station LAN announcement has an invalid identity."));
        StationLanAnnouncement mixed = value();
        mixed.label = QStringLiteral("KG4VCF");
        QVERIFY(encodeStationLanAnnouncement(mixed, &error).isEmpty());
        StationLanAnnouncement unknownSchema = valueV2();
        unknownSchema.schema = 3;
        QVERIFY(encodeStationLanAnnouncement(unknownSchema, &error).isEmpty());

        // The schema-2 tail: claimed, 32 identity bytes, label, pairing.
        const int tail = encoded.size() - 1 - static_cast<int>(source.label.size()) - 1
            - kStationLanIdentityBytes - 1;
        QByteArray invalid = encoded;
        invalid[tail] = '\x02';
        QVERIFY(!decodeStationLanAnnouncement(invalid, &error));
        QCOMPARE(error, QStringLiteral("Station LAN announcement has an invalid claimed state."));
        invalid = encoded;
        invalid[invalid.size() - 1] = '\x03';
        QVERIFY(!decodeStationLanAnnouncement(invalid, &error));
        QCOMPARE(error, QStringLiteral("Station LAN announcement has an invalid pairing state."));
        invalid = encoded;
        invalid[tail + 1 + kStationLanIdentityBytes + 1] = ' ';
        QVERIFY(!decodeStationLanAnnouncement(invalid, &error));
        QCOMPARE(error, QStringLiteral("Station LAN announcement has an invalid label."));
        QVERIFY(!decodeStationLanAnnouncement(encoded.chopped(1), &error));
        // Schema 2 extends by appending: bytes after the known fields are
        // ignored, up to the datagram bound. iPhone app Task 71: the device
        // count is a known field now, so the bytes go after it; the first
        // byte after Pairing is read as the count.
        StationLanAnnouncement countedSource = source;
        countedSource.devicesConnected = 1;
        const QByteArray counted = wire(countedSource);
        QCOMPARE(counted, encoded + '\x01');
        // iPhone app plan Task 25: so is the radio state, the byte after the
        // count, which agrees with Radio connected; the bytes after it are
        // ignored.
        QVERIFY(countedSource.radioConnected);
        const auto extended = decodeStationLanAnnouncement(encoded + QByteArray("\x01\x01\x03", 3), &error);
        QVERIFY2(extended, qPrintable(error));
        StationLanAnnouncement statedSource = countedSource;
        statedSource.radio = StationLanRadio::Connected;
        QCOMPARE(*extended, statedSource);
        // A state that disagrees with Radio connected is refused.
        QVERIFY(!decodeStationLanAnnouncement(encoded + QByteArray("\x01\x02", 2), &error));
        QCOMPARE(error, QStringLiteral("Station LAN announcement has an invalid radio state."));
        QVERIFY(decodeStationLanAnnouncement(
            counted + '\x01' + QByteArray(kStationLanMaxDatagramBytes - counted.size() - 1, '\x7f'),
            &error));
        QVERIFY(!decodeStationLanAnnouncement(
            counted + QByteArray(kStationLanMaxDatagramBytes - counted.size() + 1, '\x7f'), &error));
        QCOMPARE(error, QStringLiteral("Station LAN announcement is too large."));
        // Schema 1 stays exact.
        QVERIFY(!decodeStationLanAnnouncement(wire(value()) + '\x00', &error));
        QCOMPARE(error, QStringLiteral("Station LAN announcement is malformed."));
        invalid = encoded;
        invalid[4] = '\x01'; // schema 1 with a schema-2 tail: bytes left over
        QVERIFY(!decodeStationLanAnnouncement(invalid, &error));
        QCOMPARE(error, QStringLiteral("Station LAN announcement is malformed."));
        invalid = encoded;
        invalid[4] = '\x03';
        QVERIFY(!decodeStationLanAnnouncement(invalid, &error));
        QCOMPARE(error, QStringLiteral("Station LAN announcement has an unsupported schema."));

        QCOMPARE(stationLanPairingName(StationLanPairing::Click), QStringLiteral("click"));
        QCOMPARE(stationLanPairingName(StationLanPairing::Code), QStringLiteral("code"));
        QCOMPARE(stationLanPairingName(StationLanPairing::Closed), QStringLiteral("closed"));
        QCOMPARE(stationLanPairingFromName(QStringLiteral("code")), StationLanPairing::Code);
        QVERIFY(!stationLanPairingFromName(QStringLiteral("open")));
    }

    void schemaOneNeverOverwritesSchemaTwoFields()
    {
        StationLanCache cache;
        const QHostAddress source(QStringLiteral("192.0.2.10"));
        QString error;
        QVERIFY2(cache.ingest(wire(valueV2()), source, 1, 100, &error), qPrintable(error));

        // The same endpoint, now in schema 1 with a new radio name: the
        // schema-1 fields move, schema 2's stay.
        StationLanAnnouncement older = value();
        older.radioName = QStringLiteral("Saturn G2E");
        QVERIFY(cache.ingest(wire(older), source, 1, 200, &error));
        QCOMPARE(cache.endpoints().size(), 1);
        StationLanAnnouncement expected = valueV2();
        expected.radioName = QStringLiteral("Saturn G2E");
        QCOMPARE(cache.endpoints().first().announcement, expected);
        QCOMPARE(cache.endpoints().first().lastSeenMs, 200);
        // Nothing new: no change reported.
        QVERIFY(!cache.ingest(wire(older), source, 1, 300, &error));
        QCOMPARE(cache.endpoints().first().announcement, expected);

        // Schema 2 replaces schema 2.
        StationLanAnnouncement renamed = valueV2();
        renamed.label = QStringLiteral("KG4VCF/portable");
        renamed.claimed = false;
        renamed.pairing = StationLanPairing::Click;
        QVERIFY(cache.ingest(wire(renamed), source, 1, 400, &error));
        QCOMPARE(cache.endpoints().first().announcement, renamed);

        // Another endpoint of the same Core that only ever sent schema 1
        // keeps schema 1.
        const QHostAddress other(QStringLiteral("192.0.2.11"));
        QVERIFY(cache.ingest(wire(value()), other, 1, 400, &error));
        QCOMPARE(cache.endpoints().size(), 2);
        QCOMPARE(cache.endpoints().last().announcement.schema, kStationLanAnnouncementSchema1);
    }

    void sourceScopeAndUrlRoundTrip()
    {
        StationLanCache cache;
        QHostAddress source(QStringLiteral("fe80::1234"));
        source.setScopeId(QStringLiteral("en7"));
        QString error;
        QVERIFY2(cache.ingest(wire(value()), source, 7, 100, &error), qPrintable(error));
        const StationLanEndpoint endpoint = cache.endpoints().first();
        QCOMPARE(endpoint.address.scopeId(), QStringLiteral("en7"));
        QCOMPARE(endpoint.interfaceIndex, 7u);
        const QUrl url = endpoint.url();
        QCOMPARE(url.scheme(), QStringLiteral("wss"));
        QCOMPARE(url.port(), 47910);
        const QHostAddress recovered(url.host(QUrl::FullyDecoded));
        QCOMPARE(recovered.protocol(), QAbstractSocket::IPv6Protocol);
        QCOMPARE(recovered.scopeId(), QStringLiteral("en7"));

        StationLanCache indexed;
        QHostAddress unscoped(QStringLiteral("fe80::5678"));
        QVERIFY(indexed.ingest(wire(value(1)), unscoped, 42, 100, &error));
        QCOMPARE(indexed.endpoints().first().address.scopeId(), QStringLiteral("42"));
        StationLanCache missingScope;
        QVERIFY(!missingScope.ingest(wire(value(2)), unscoped, 0, 100, &error));
        QCOMPARE(error, QStringLiteral("Station LAN announcement has an unscoped link-local source."));
    }

    void cacheRefreshBoundsExpiryAndReadmission()
    {
        StationLanCache cache;
        const QHostAddress source(QStringLiteral("192.0.2.10"));
        QString error;
        QVERIFY(cache.ingest(wire(value()), source, 1, 100, &error));
        QVERIFY(!cache.ingest(wire(value()), source, 1, 200, &error));
        QCOMPARE(cache.endpoints().first().lastSeenMs, 200);
        StationLanAnnouncement changed = value();
        changed.radioName = QStringLiteral("Saturn G2E");
        QVERIFY(cache.ingest(wire(changed), source, 1, 300, &error));
        QCOMPARE(cache.endpoints().size(), 1);
        QCOMPARE(cache.endpoints().first().announcement.radioName, QStringLiteral("Saturn G2E"));

        for (int i = 1; i < StationLanCache::kMaxFingerprints; ++i) {
            QVERIFY2(cache.ingest(wire(value(i)), source, 1, 300, &error), qPrintable(error));
        }
        QCOMPARE(cache.endpoints().size(), StationLanCache::kMaxFingerprints);
        QVERIFY(!cache.ingest(wire(value(200)), source, 1, 300, &error));
        QCOMPARE(error, QStringLiteral("Station LAN announcement cache is full."));
        QVERIFY(cache.ingest(wire(value(200)), source, 1, 300 + kStationLanCacheTtlMs, &error));
        QCOMPARE(cache.endpoints().size(), 1);
        QCOMPARE(cache.endpoints().first().announcement.fingerprint, pin(200));
    }

    void endpointBoundAndSourceValidation()
    {
        StationLanCache cache;
        const QByteArray encoded = wire(value());
        QString error;
        for (int i = 1; i <= StationLanCache::kMaxEndpointsPerFingerprint; ++i) {
            QVERIFY2(cache.ingest(encoded, QHostAddress(QStringLiteral("192.0.2.%1").arg(i)),
                                  static_cast<uint>(i), 100, &error), qPrintable(error));
        }
        QVERIFY(!cache.ingest(encoded, QHostAddress(QStringLiteral("192.0.2.99")), 99, 100,
                              &error));
        QCOMPARE(error, QStringLiteral("Station LAN announcement endpoint limit reached."));

        StationLanCache invalidSource;
        QVERIFY(!invalidSource.ingest(encoded, QHostAddress::AnyIPv4, 0, 100, &error));
        QCOMPARE(error, QStringLiteral("Station LAN announcement has an invalid source address."));
        QVERIFY(!invalidSource.ingest(encoded, QHostAddress(QStringLiteral("239.1.1.1")), 0,
                                      100, &error));
        QCOMPARE(error, QStringLiteral("Station LAN announcement has an invalid source address."));
        QVERIFY(invalidSource.ingest(encoded, QHostAddress::LocalHost, 0, 100, &error));
    }
};

QTEST_GUILESS_MAIN(TstStationLanCache)
#include "tst_station_lan_cache.moc"
