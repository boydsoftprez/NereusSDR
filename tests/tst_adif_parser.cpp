// SPDX-License-Identifier: GPL-3.0-or-later
//
// no-port-check: This test references real DXCC entity callsigns
// (W1AW, JA1ABC, VK6APH, G3OCA, DL1ABCD) as fixtures in a sample
// ADIF log. They are well-known callsigns used as test inputs,
// not ported callsigns. Precedent: B2-B6, C1.
//
// NereusSDR - AdifParser tests
//
// Phase 3J-2 Task C2. Pins the contract that AdifParser parses an
// ADIF (Amateur Data Interchange Format) log file into a vector
// of QsoRecord (callsign + band + modeGroup). Five tests:
//   - parsesSampleAdif: synchronous parseFile() against the
//     bundled 10-QSO sample fixture returns 10 records with the
//     expected first-record callsign / band / modeGroup values.
//   - emitsFinishedSignal: parseFileAsync() emits finished()
//     with the parsed records (QSignalSpy wait).
//   - skipsHeaderSection: header (<adif_ver>, <created_timestamp>,
//     <programid>) before <eoh> contributes zero records.
//   - handlesCaseInsensitiveTags: <CALL:> / <Call:> / <call:> /
//     <eor> / <EOR> / <Eor> all parse the same.
//   - rejectsMalformedAdif: arbitrary garbage bytes yield an
//     empty record vector.

#include <QtTest>
#include <QSignalSpy>
#include <QFileInfo>
#include <QDir>

#include "core/AdifParser.h"

using namespace NereusSDR;

// Path resolver: the fixture lives in tests/fixtures/adif/sample.adi.
// __FILE__ resolves to .../tests/tst_adif_parser.cpp inside the
// source tree (same approach as tst_cty_dat_parser).
static QString resolveFixturePath()
{
    const QString file = QString::fromUtf8(__FILE__);
    const QString testsDir = QFileInfo(file).dir().path();
    return testsDir + "/fixtures/adif/sample.adi";
}

class TestAdifParser : public QObject {
    Q_OBJECT
private slots:
    void parsesSampleAdif();
    void emitsFinishedSignal();
    void skipsHeaderSection();
    void handlesCaseInsensitiveTags();
    void rejectsMalformedAdif();
};

void TestAdifParser::parsesSampleAdif()
{
    const QString fixturePath = resolveFixturePath();
    QVERIFY2(QFileInfo::exists(fixturePath),
             qPrintable(QString("fixture missing: %1").arg(fixturePath)));

    const QVector<QsoRecord> records = AdifParser::parseFile(fixturePath);
    QCOMPARE(records.size(), 10);
    QCOMPARE(records[0].callsign, QStringLiteral("W1AW"));
    QCOMPARE(records[0].band, QStringLiteral("20m"));
    // AetherSDR's QsoRecord uses modeGroup (PHONE / CW / DATA),
    // not raw mode. SSB normalises to PHONE.
    QCOMPARE(records[0].modeGroup, QStringLiteral("PHONE"));
    // Second record: same callsign, CW band -> CW group.
    QCOMPARE(records[1].callsign, QStringLiteral("W1AW"));
    QCOMPARE(records[1].modeGroup, QStringLiteral("CW"));
    // Last record: DL1ABCD on 15m CW.
    QCOMPARE(records[9].callsign, QStringLiteral("DL1ABCD"));
    QCOMPARE(records[9].band, QStringLiteral("15m"));
    QCOMPARE(records[9].modeGroup, QStringLiteral("CW"));
}

void TestAdifParser::emitsFinishedSignal()
{
    // Register meta-type so QSignalSpy can serialize the QVector<QsoRecord>.
    qRegisterMetaType<QVector<QsoRecord>>("QVector<QsoRecord>");

    AdifParser parser;
    QSignalSpy spy(&parser, &AdifParser::finished);

    const QString fixturePath = resolveFixturePath();
    QVERIFY(QFileInfo::exists(fixturePath));

    // parseFileAsync is "async" in the sense that the caller can
    // invoke it via QMetaObject::invokeMethod on a worker thread;
    // upstream's body is synchronous (open + parse + emit) and
    // returns after the signal has fired. The spy therefore has
    // count == 1 immediately after the call returns; QSignalSpy::wait
    // is unnecessary but harmless.
    parser.parseFileAsync(fixturePath);

    QCOMPARE(spy.count(), 1);
    const auto records = spy.first().first().value<QVector<QsoRecord>>();
    QCOMPARE(records.size(), 10);
    QCOMPARE(records[0].callsign, QStringLiteral("W1AW"));
}

void TestAdifParser::skipsHeaderSection()
{
    // The bundled fixture has <adif_ver> / <created_timestamp> /
    // <programid> ahead of <eoh>. The parser must skip everything
    // before <eoh> so the record count stays at the 10 body QSOs.
    const QVector<QsoRecord> records =
        AdifParser::parseFile(resolveFixturePath());
    QCOMPARE(records.size(), 10);

    // No header tags leaked into a record (no record should have
    // a callsign matching a header tag value).
    for (const QsoRecord& r : records) {
        QVERIFY2(r.callsign != QStringLiteral("3.1.4"),
                 "header adif_ver leaked as a callsign");
        QVERIFY2(r.callsign != QStringLiteral("NEREUSSDR"),
                 "header programid leaked as a callsign");
    }
}

void TestAdifParser::handlesCaseInsensitiveTags()
{
    // Construct a tiny in-memory ADIF buffer with mixed-case tag
    // names. The parser's regex is CaseInsensitiveOption, and the
    // <EOR> / <EOH> sentinel lookups use Qt::CaseInsensitive, so
    // all three forms must yield the same two records.
    const QByteArray adif =
        "<EOH>\n"
        "<CALL:4>W1AW <BAND:3>20m <MODE:3>SSB <eor>\n"
        "<Call:6>JA1ABC <Band:3>40m <Mode:2>CW <Eor>\n";

    const QVector<QsoRecord> records =
        AdifParser::parseBytesForTest(adif);
    QCOMPARE(records.size(), 2);
    QCOMPARE(records[0].callsign, QStringLiteral("W1AW"));
    QCOMPARE(records[0].band, QStringLiteral("20m"));
    QCOMPARE(records[0].modeGroup, QStringLiteral("PHONE"));
    QCOMPARE(records[1].callsign, QStringLiteral("JA1ABC"));
    QCOMPARE(records[1].band, QStringLiteral("40m"));
    QCOMPARE(records[1].modeGroup, QStringLiteral("CW"));
}

void TestAdifParser::rejectsMalformedAdif()
{
    const QByteArray garbage = "this is not adif";
    const QVector<QsoRecord> records =
        AdifParser::parseBytesForTest(garbage);
    QVERIFY(records.isEmpty());
}

QTEST_GUILESS_MAIN(TestAdifParser)
#include "tst_adif_parser.moc"
