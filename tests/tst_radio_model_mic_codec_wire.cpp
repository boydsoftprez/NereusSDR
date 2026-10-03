// no-port-check: NereusSDR-original unit-test file. The Thetis cite
// comments below document which upstream lines each assertion verifies;
// no upstream logic is ported in this file.
// =================================================================
// tests/tst_radio_model_mic_codec_wire.cpp  (NereusSDR)
// =================================================================
//
// Radio codec lane (2026-09-30): the radio's mic codec controls (mic
// boost, line in, tip/ring, bias, XLR) reach the wire on connect and on
// every change, through RadioModel::connectMicCodecSignals, on a real
// P1RadioConnection (bank 10 C2, bank 11 C1) and a real P2RadioConnection
// (CmdTx byte 50). The line-in dB value reaches the index Thetis sends
// (bank 11 C2, byte 51) through TransmitModel::lineInGainIndexForBoost.
//
// Upstream behavior verified (comments only):
//   Thetis console.cs:40920-40933 [v2.10.3.15] SetMicGain sends
//     SetMicBoost, SetLineIn and SetLineBoost(Array.IndexOf(lineinboost,
//     line_in_boost)); the MicBoost / LineIn / LineInBoost setters and
//     power on (console.cs:27461) call it.
//   Thetis console.cs:40900-40912 [v2.10.3.15] MakeLineInList:
//     lineinboost[k] = -34.5 + 1.5 k, k = 0..31.
//   Thetis networkproto1.c:593-601 [v2.10.3.15] bank 10 C2 bit 0 boost,
//     bit 1 line in; bank 11 C1 bit 4 mic_trs, bit 5 bias; C2 line gain.
//   Thetis network.c:1234-1236 [v2.10.3.15] byte 50 mic_control, byte 51
//     line_in_gain.
//
// Nothing in this file keys a radio: no connection is opened, and only
// the composed C&C bytes are inspected.
// =================================================================

#include <QtTest/QtTest>
#include <QObject>
#include <QCoreApplication>
#include <QScopeGuard>

#include "core/P1RadioConnection.h"
#include "core/P2RadioConnection.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

namespace {

void pump()
{
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
}

// Moves every codec control off its default so a prime is observable.
void setNonDefaults(TransmitModel& tx)
{
    tx.setMicBoost(false);   // default true
    tx.setLineIn(true);      // default false
    tx.setMicTipRing(false); // default true (tip is mic)
    tx.setMicBias(true);     // default false
    tx.setMicXlr(false);     // default true
}

} // namespace

class TestRadioModelMicCodecWire : public QObject {
    Q_OBJECT
private slots:

    // ── Line In Gain: dB to index, against Thetis's lineinboost list ─────
    void lineInGainIndex_matchesThetisList_data()
    {
        QTest::addColumn<double>("dB");
        QTest::addColumn<int>("index");
        // Every entry of MakeLineInList: -34.5 + 1.5 k.
        for (int k = 0; k <= 31; ++k) {
            const double dB = -34.5 + 1.5 * k;
            QTest::newRow(qPrintable(QStringLiteral("k%1").arg(k))) << dB << k;
        }
    }
    void lineInGainIndex_matchesThetisList()
    {
        QFETCH(double, dB);
        QFETCH(int, index);
        QCOMPARE(TransmitModel::lineInGainIndexForBoost(dB), index);
    }

    void lineInGainIndex_namedPoints()
    {
        QCOMPARE(TransmitModel::lineInGainIndexForBoost(-34.5), 0);
        QCOMPARE(TransmitModel::lineInGainIndexForBoost(0.0), 23);
        QCOMPARE(TransmitModel::lineInGainIndexForBoost(12.0), 31);
        // Off-grid values take the nearest entry; out of range clamps.
        QCOMPARE(TransmitModel::lineInGainIndexForBoost(0.4), 23);
        QCOMPARE(TransmitModel::lineInGainIndexForBoost(1.0), 24);
        QCOMPARE(TransmitModel::lineInGainIndexForBoost(-40.0), 0);
        QCOMPARE(TransmitModel::lineInGainIndexForBoost(20.0), 31);
    }

    void setLineInBoost_updatesLineInGain()
    {
        TransmitModel tx;
        QCOMPARE(tx.lineInGain(), 23);
        tx.setLineInBoost(-9.0);
        QCOMPARE(tx.lineInGain(), 17);
        tx.setLineInBoost(12.0);
        QCOMPARE(tx.lineInGain(), 31);
    }

    // ── P1: prime on connect ─────────────────────────────────────────────
    void p1_connect_primesCodecBits()
    {
        RadioModel model;
        auto conn = std::make_unique<P1RadioConnection>();
        model.injectConnectionForTest(conn.get());
        auto detach = qScopeGuard([&] { model.injectConnectionForTest(nullptr); });

        setNonDefaults(model.transmitModel());
        model.wireMicCodecForTest();
        pump();

        const QByteArray bank10 = conn->captureBank10ForTest();
        const QByteArray bank11 = conn->captureBank11ForTest();
        QCOMPARE(int(quint8(bank10[2]) & 0x01), 0);     // boost off
        QCOMPARE(int(quint8(bank10[2]) & 0x02), 0x02);  // line in on
        QCOMPARE(int(quint8(bank11[1]) & 0x10), 0x10);  // tip/ring swapped
        QCOMPARE(int(quint8(bank11[1]) & 0x20), 0x20);  // bias on
    }

    // ── P1: every change reaches the wire ────────────────────────────────
    void p1_change_reachesCodecBits()
    {
        RadioModel model;
        auto conn = std::make_unique<P1RadioConnection>();
        model.injectConnectionForTest(conn.get());
        auto detach = qScopeGuard([&] { model.injectConnectionForTest(nullptr); });

        model.wireMicCodecForTest();
        pump();
        QCOMPARE(int(quint8(conn->captureBank10ForTest()[2]) & 0x01), 0x01);  // default boost

        TransmitModel& tx = model.transmitModel();
        setNonDefaults(tx);
        pump();
        QByteArray bank10 = conn->captureBank10ForTest();
        QByteArray bank11 = conn->captureBank11ForTest();
        QCOMPARE(int(quint8(bank10[2]) & 0x03), 0x02);
        QCOMPARE(int(quint8(bank11[1]) & 0x30), 0x30);

        tx.setMicBoost(true);
        tx.setLineIn(false);
        tx.setMicTipRing(true);
        tx.setMicBias(false);
        pump();
        bank10 = conn->captureBank10ForTest();
        bank11 = conn->captureBank11ForTest();
        QCOMPARE(int(quint8(bank10[2]) & 0x03), 0x01);
        QCOMPARE(int(quint8(bank11[1]) & 0x30), 0);
    }

    // ── P2: prime on connect ─────────────────────────────────────────────
    void p2_connect_primesByte50()
    {
        RadioModel model;
        auto conn = std::make_unique<P2RadioConnection>();
        model.injectConnectionForTest(conn.get());
        auto detach = qScopeGuard([&] { model.injectConnectionForTest(nullptr); });

        setNonDefaults(model.transmitModel());
        model.wireMicCodecForTest();
        pump();

        quint8 buf[60] = {};
        conn->composeCmdTxForTest(buf);
        // bit 0 line in, bit 1 boost, bit 3 tip/ring, bit 4 bias, bit 5 XLR.
        QCOMPARE(int(buf[50] & 0x3B), 0x01 | 0x08 | 0x10);
    }

    // ── P2: every change reaches the wire ────────────────────────────────
    void p2_change_reachesByte50()
    {
        RadioModel model;
        auto conn = std::make_unique<P2RadioConnection>();
        model.injectConnectionForTest(conn.get());
        auto detach = qScopeGuard([&] { model.injectConnectionForTest(nullptr); });

        model.wireMicCodecForTest();
        pump();
        quint8 buf[60] = {};
        conn->composeCmdTxForTest(buf);
        QCOMPARE(int(buf[50] & 0x3B), 0x02 | 0x20);  // defaults: boost, XLR

        TransmitModel& tx = model.transmitModel();
        setNonDefaults(tx);
        pump();
        conn->composeCmdTxForTest(buf);
        QCOMPARE(int(buf[50] & 0x3B), 0x01 | 0x08 | 0x10);

        tx.setMicXlr(true);
        tx.setMicBoost(true);
        pump();
        conn->composeCmdTxForTest(buf);
        QCOMPARE(int(buf[50] & 0x3B), 0x01 | 0x02 | 0x08 | 0x10 | 0x20);
    }

    // ── Line In Gain index reaches the wire on both protocols ────────────
    void lineInGainIndex_reachesP1AndP2()
    {
        TransmitModel tx;
        tx.setLineInBoost(-9.0);

        P1RadioConnection p1;
        p1.setBoardForTest(HPSDRHW::HermesII);
        p1.setLineInGain(tx.lineInGain());
        QCOMPARE(int(quint8(p1.captureBank11ForTest()[2]) & 0x1F), 17);

        P2RadioConnection p2;
        p2.setLineInGain(tx.lineInGain());
        quint8 buf[60] = {};
        p2.composeCmdTxForTest(buf);
        QCOMPARE(int(buf[51]), 17);
    }
};

QTEST_MAIN(TestRadioModelMicCodecWire)
#include "tst_radio_model_mic_codec_wire.moc"
