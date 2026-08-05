// SPDX-License-Identifier: GPL-2.0-or-later
//
// NereusSDR - tst_rx_decode_model: bounded ring buffer of local decodes
// from MY radio's receivers (rade_text + WSJT-X UDP).
//
// NEW NereusSDR-native model. No upstream equivalent. Test fixtures
// reference plausible callsigns and SNRs as fixtures (no DXCC entity
// porting required).

#include <QtTest>
#include <QSignalSpy>

#include "models/RxDecodeModel.h"

using namespace NereusSDR;

class TestRxDecodeModel : public QObject {
    Q_OBJECT
private slots:
    void initialState();
    void addDecodeEmits();
    void ringBufferDropsOldest();
    void clearEmits();
    void addsFromMultipleSources();
};

static RxDecode makeDecode(const QString& call,
                           double freqMhz,
                           int snr,
                           const QString& mode,
                           const QString& source) {
    RxDecode d;
    d.callsign = call;
    d.freqMhz  = freqMhz;
    d.snr      = snr;
    d.mode     = mode;
    d.source   = source;
    d.utcTime  = QDateTime::currentDateTimeUtc();
    d.payload  = QStringLiteral("CQ %1 FN42").arg(call);
    return d;
}

void TestRxDecodeModel::initialState() {
    RxDecodeModel m;
    QCOMPARE(m.decodes().size(), 0);
}

void TestRxDecodeModel::addDecodeEmits() {
    RxDecodeModel m;
    QSignalSpy spy(&m, &RxDecodeModel::decodeAdded);
    m.addDecode(makeDecode("DL2RD", 14.074, 5, "FT8", "WSJT-X"));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(m.decodes().size(), 1);
    QCOMPARE(m.decodes().first().callsign, QStringLiteral("DL2RD"));
}

void TestRxDecodeModel::ringBufferDropsOldest() {
    RxDecodeModel m(/*maxSize=*/200);
    // Push 250 decodes. The first 50 should be evicted (ring buffer drop).
    for (int i = 0; i < 250; ++i) {
        m.addDecode(makeDecode(QStringLiteral("CALL%1").arg(i),
                               14.074,
                               i,
                               QStringLiteral("FT8"),
                               QStringLiteral("WSJT-X")));
    }
    QCOMPARE(m.decodes().size(), 200);
    // Oldest surviving entry is CALL50 (CALL0..CALL49 dropped).
    QCOMPARE(m.decodes().first().callsign, QStringLiteral("CALL50"));
    // Newest entry is CALL249.
    QCOMPARE(m.decodes().last().callsign, QStringLiteral("CALL249"));
}

void TestRxDecodeModel::clearEmits() {
    RxDecodeModel m;
    m.addDecode(makeDecode("DL2RD", 14.074, 5, "FT8", "WSJT-X"));
    m.addDecode(makeDecode("G4XXX", 14.074, 3, "FT8", "WSJT-X"));

    QSignalSpy spy(&m, &RxDecodeModel::cleared);
    m.clear();
    QCOMPARE(spy.count(), 1);
    QCOMPARE(m.decodes().size(), 0);
}

void TestRxDecodeModel::addsFromMultipleSources() {
    RxDecodeModel m;
    m.addDecode(makeDecode("DL2RD", 14.236, 8,  "RADE",  "rade_text"));
    m.addDecode(makeDecode("G4XXX", 14.074, 12, "FT8",   "WSJT-X"));
    m.addDecode(makeDecode("VK3FOO", 7.180, -3, "RADE",  "rade_text"));

    const auto all = m.decodes();
    QCOMPARE(all.size(), 3);
    QCOMPARE(all[0].source, QStringLiteral("rade_text"));
    QCOMPARE(all[1].source, QStringLiteral("WSJT-X"));
    QCOMPARE(all[2].source, QStringLiteral("rade_text"));
    QCOMPARE(all[2].callsign, QStringLiteral("VK3FOO"));
}

QTEST_GUILESS_MAIN(TestRxDecodeModel)
#include "tst_rx_decode_model.moc"
