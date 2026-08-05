// SPDX-License-Identifier: GPL-3.0-or-later
//
// no-port-check: This test contains fabricated PSK Reporter IPFIX
// fixtures (synthetic callsigns, gridsquares, frequencies, SNR values
// inside binary datagram envelopes). They are test fixtures, not
// ported user data. Precedent: B2 (commit f43582e), B3 (commit
// 9aa4202), B4 (commit fce661fd), B5 (commit d5faab1).
//
// NereusSDR - PskReporterClient IPFIX protocol tests
//
// Phase 3J-2 Task B6. Pins the wire-protocol contract that
// PskReporterClient implements the IPFIX (RFC 5101) v0.1 framing used
// by report.pskreporter.info:
//   - 16-byte IPFIX header: version=10 (0x000A), length, exportTime,
//     sequenceNumber, observationDomain (randomIdentifier).
//   - Receiver data set (template ID 0x9992) carries
//     receiverCallsign / receiverLocator / decodingSoftware, each
//     length-prefixed (1 byte length + N chars), 4-byte aligned.
//   - Sender data set (template ID 0x9993) carries N records of
//     senderCallsign(1+len) + frequency(5 big-endian) + SNR(1 signed) +
//     mode(1+len) + informationSource(1) + flowStartSeconds(4 BE),
//     4-byte aligned.
//   - Both the RX-only datagram (no sender records) and the
//     RX+TX datagram (with one or more sender records) layouts are
//     covered, plus the parser side for round-tripping a canned
//     RX+TX datagram back into DxSpot signals.

#include <QtTest>
#include <QSignalSpy>

#include "core/PskReporterClient.h"
#include "core/DxSpot.h"

using namespace NereusSDR;

// Big-endian uint16 / uint32 read helpers. PSK Reporter IPFIX is
// network byte order on every numeric field.
static quint16 beU16(const QByteArray& dg, int off) {
    return (quint16(quint8(dg[off])) << 8) | quint16(quint8(dg[off + 1]));
}
static quint32 beU32(const QByteArray& dg, int off) {
    return (quint32(quint8(dg[off])) << 24) |
           (quint32(quint8(dg[off + 1])) << 16) |
           (quint32(quint8(dg[off + 2])) << 8) |
           quint32(quint8(dg[off + 3]));
}

class TestPskReporterProtocol : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void buildsValidIpfixHeader();
    void includesReceiverTemplateAndDataSet();
    void senderRecordSizeMatchesUpstream();
    void encodesSenderRecordFields();
    void buildsDatagramWithSenderRecords();
    void parsesIncomingSpotDatagram();
    void setIdentityAffectsOutgoingPackets();
};

void TestPskReporterProtocol::initTestCase() {
    qRegisterMetaType<NereusSDR::DxSpot>("NereusSDR::DxSpot");
}

void TestPskReporterProtocol::buildsValidIpfixHeader() {
    PskReporterClient c;
    c.setIdentity(QStringLiteral("K6AQ"), QStringLiteral("DM12kw"),
                  QStringLiteral("NereusSDR 0.4.0"));

    // Build an RX-only datagram (no sender records queued).
    QByteArray dg = c.buildDatagramForTest();

    QVERIFY(dg.size() >= 16);
    // Version field: 0x000A (IPFIX v10).
    QCOMPARE(beU16(dg, 0), quint16(0x000A));
    // Length field: equals total datagram size.
    QCOMPARE(beU16(dg, 2), quint16(dg.size()));
    // exportTime + sequenceNumber + observationDomain are all 4-byte BE
    // ints. We just verify they are present at the expected offsets
    // (exact values are runtime-dependent).
    (void)beU32(dg, 4);   // exportTime
    QCOMPARE(beU32(dg, 8), quint32(0));  // first datagram, seq=0
    (void)beU32(dg, 12);  // observationDomain (randomIdentifier)
}

void TestPskReporterProtocol::includesReceiverTemplateAndDataSet() {
    PskReporterClient c;
    c.setIdentity(QStringLiteral("K6AQ"), QStringLiteral("DM12kw"),
                  QStringLiteral("NereusSDR 0.4.0"));

    QByteArray dg = c.buildDatagramForTest();

    // After the 16-byte header, the rxFormatHeader (36 bytes) must
    // appear, then the RX data set must start with setID 0x9992.
    // rxFormatHeader byte 0..1 is the template set ID (0x0003), so we
    // look for the data set header (0x9992) at offset 16 + 36 = 52.
    QVERIFY(dg.size() >= 52 + 4);
    QCOMPARE(beU16(dg, 16), quint16(0x0003));  // template set
    QCOMPARE(beU16(dg, 52), quint16(0x9992));  // RX data set

    // RX data set length includes the 4-byte header + the three
    // length-prefixed fields, padded to 4-byte boundary.
    // Per upstream: 4 + (1 + cs.size) + (1 + loc.size) + (1 + sw.size)
    // For "K6AQ" (4) + "DM12kw" (6) + "NereusSDR 0.4.0" (15):
    //   4 + 5 + 7 + 16 = 32 -> padded to 32 (already aligned).
    int expectedRxSize = 4 + (1 + 4) + (1 + 6) + (1 + 15);
    if (expectedRxSize % 4) {
        expectedRxSize += 4 - (expectedRxSize % 4);
    }
    QCOMPARE(beU16(dg, 54), quint16(expectedRxSize));
}

void TestPskReporterProtocol::senderRecordSizeMatchesUpstream() {
    // From freedv-gui pskreporter.cpp:113-116 [@77e793a]:
    //   (1 + callsign.size()) + 5 + 1 + (1 + mode.size()) + 1 + 4
    // For dxCall="N1DQ" (4 chars) + mode="FT8" (3 chars):
    //   (1+4) + 5 + 1 + (1+3) + 1 + 4 = 5+5+1+4+1+4 = 20
    int sz = PskReporterClient::senderRecordSizeForTest(
        QStringLiteral("N1DQ"), QStringLiteral("FT8"));
    QCOMPARE(sz, 20);
}

void TestPskReporterProtocol::encodesSenderRecordFields() {
    // Per freedv-gui pskreporter.cpp:118-146 [@77e793a]:
    //   length-prefixed callsign, 5-byte BE frequency, signed SNR byte,
    //   length-prefixed mode, 1-byte infoSource, 4-byte BE flowTime.
    QByteArray rec = PskReporterClient::encodeSenderRecordForTest(
        QStringLiteral("N1DQ"), 14236000ULL, -5,
        QStringLiteral("FT8"), /*flowTimeSeconds=*/0x12345678);

    QVERIFY(rec.size() == 20);

    int off = 0;
    QCOMPARE(quint8(rec[off]), quint8(4));            // callsign length
    QCOMPARE(rec.mid(off + 1, 4), QByteArray("N1DQ"));
    off += 1 + 4;

    // 5-byte big-endian frequency. 14236000 = 0x00D93960
    // (verify: 0xD9*65536 + 0x39*256 + 0x60 =
    //  14221312 + 14592 + 96 = 14236000).
    QCOMPARE(quint8(rec[off + 0]), quint8(0x00));
    QCOMPARE(quint8(rec[off + 1]), quint8(0x00));
    QCOMPARE(quint8(rec[off + 2]), quint8(0xD9));
    QCOMPARE(quint8(rec[off + 3]), quint8(0x39));
    QCOMPARE(quint8(rec[off + 4]), quint8(0x60));
    off += 5;

    QCOMPARE(qint8(rec[off]), qint8(-5));             // SNR signed
    off += 1;

    QCOMPARE(quint8(rec[off]), quint8(3));            // mode length
    QCOMPARE(rec.mid(off + 1, 3), QByteArray("FT8"));
    off += 1 + 3;

    QCOMPARE(quint8(rec[off]), quint8(1));            // infoSource
    off += 1;

    // 4-byte big-endian flowTime
    QCOMPARE(quint8(rec[off + 0]), quint8(0x12));
    QCOMPARE(quint8(rec[off + 1]), quint8(0x34));
    QCOMPARE(quint8(rec[off + 2]), quint8(0x56));
    QCOMPARE(quint8(rec[off + 3]), quint8(0x78));
}

void TestPskReporterProtocol::buildsDatagramWithSenderRecords() {
    PskReporterClient c;
    c.setIdentity(QStringLiteral("K6AQ"), QStringLiteral("DM12kw"),
                  QStringLiteral("NereusSDR 0.4.0"));

    // Queue one sender record.
    c.reportDecode(QStringLiteral("N1DQ"), QStringLiteral("FT8"),
                   14.236, /*snr=*/-5);
    QByteArray dg = c.buildDatagramForTest();

    QCOMPARE(beU16(dg, 0), quint16(0x000A));        // IPFIX v10
    QCOMPARE(beU16(dg, 2), quint16(dg.size()));     // self-length

    // After 16-byte header + 36-byte rxFormatHeader, the txFormatHeader
    // (52 bytes) must appear next when sender records are queued.
    QCOMPARE(beU16(dg, 16), quint16(0x0003));       // RX template set
    QCOMPARE(beU16(dg, 52), quint16(0x0002));       // TX template set

    // After the TX template (52 bytes), the RX data set then TX data
    // set: 16 + 36 + 52 = 104. RX data set starts at 104, then TX set
    // starts after its padded length.
    QCOMPARE(beU16(dg, 104), quint16(0x9992));      // RX data set ID
    int rxLen = beU16(dg, 106);
    QCOMPARE(beU16(dg, 104 + rxLen), quint16(0x9993));  // TX data set
}

void TestPskReporterProtocol::parsesIncomingSpotDatagram() {
    PskReporterClient c;
    c.setIdentity(QStringLiteral("K6AQ"), QStringLiteral("DM12kw"),
                  QStringLiteral("NereusSDR 0.4.0"));

    // Build a self-consistent datagram with one sender record so we can
    // round-trip it through the parser. PSK Reporter parsers in the
    // wild treat incoming sender records as spots.
    c.reportDecode(QStringLiteral("N1DQ"), QStringLiteral("FT8"),
                   14.236, /*snr=*/-5);
    QByteArray dg = c.buildDatagramForTest();

    QSignalSpy spotSpy(&c, &PskReporterClient::spotReceived);
    QVector<DxSpot> spots = c.parseDatagramForTest(dg);

    QCOMPARE(spots.size(), 1);
    QCOMPARE(spots[0].dxCall, QStringLiteral("N1DQ"));
    QCOMPARE(spots[0].source, QStringLiteral("PSK"));
    QCOMPARE(spots[0].snr, -5);
    // Frequency was sent in Hz (14236000) but DxSpot.freqMhz is MHz.
    QVERIFY(qAbs(spots[0].freqMhz - 14.236) < 1e-6);
    QCOMPARE(spots[0].comment, QStringLiteral("FT8"));

    // parseDatagramForTest also fires the spotReceived signal.
    QCOMPARE(spotSpy.count(), 1);
}

void TestPskReporterProtocol::setIdentityAffectsOutgoingPackets() {
    PskReporterClient c;

    c.setIdentity(QStringLiteral("AA1AA"), QStringLiteral("FN42"),
                  QStringLiteral("TestSW 1.0"));
    QByteArray dg1 = c.buildDatagramForTest();
    // The RX data set encodes our identity; find the receiver callsign
    // by scanning past the 16+36 = 52-byte template/header for the
    // 0x9992 data-set ID, skipping the 4-byte set header.
    int rxOff = 52;
    QCOMPARE(beU16(dg1, rxOff), quint16(0x9992));
    int csLen = quint8(dg1[rxOff + 4]);
    QByteArray cs = dg1.mid(rxOff + 5, csLen);
    QCOMPARE(cs, QByteArray("AA1AA"));

    // Now change identity and re-encode.
    c.setIdentity(QStringLiteral("ZZ9ZZ"), QStringLiteral("RR99"),
                  QStringLiteral("TestSW 2.0"));
    QByteArray dg2 = c.buildDatagramForTest();
    int csLen2 = quint8(dg2[rxOff + 4]);
    QByteArray cs2 = dg2.mid(rxOff + 5, csLen2);
    QCOMPARE(cs2, QByteArray("ZZ9ZZ"));
}

QTEST_GUILESS_MAIN(TestPskReporterProtocol)
#include "tst_psk_reporter_protocol.moc"
