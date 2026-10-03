// no-port-check: test-only. Upstream file names appear only in source-cite
// comments that document which upstream line each assertion verifies.
// No upstream logic is ported here; this file is NereusSDR-original.
//
// Wire-byte tests for the HL2 TX buffer latency and PTT hang (P1 bank 17).
//
//   mi0bot Console/setup.cs:21236-21248 [@c26a8a4]
//     // MI0BOT: Controls the hardware tx buffer in the HL2
//     private void udTxBufferLat_ValueChanged(object sender, EventArgs e)
//     {
//         NetworkIO.SetTxLatency((int)udTxBufferLat.Value);
//     }
//     // MI0BOT: Controls the hardware PTT hang in the HL2
//     private void udPTTHang_ValueChanged(object sender, EventArgs e)
//     {
//         NetworkIO.SetPttHang((int)udPTTHang.Value);
//     }
//   mi0bot ChannelMaster/networkproto1.c:1162-1168 [@c26a8a4] (WriteMainLoop_HL2)
//     case 17: // TX latency and PTT hang 0x17
//         C0 |= 0x2e; //C0 0010 111x
//         C1 = 0;
//         C2 = 0;
//         C3 = (prn->tx[0].ptt_hang & 0b00011111);
//         C4 = (prn->tx[0].tx_latency & 0b01111111);
//   Defaults 20 and 12: mi0bot ChannelMaster/netInterface.c:1709-1710 [@c26a8a4]

#include <QtTest/QtTest>
#include "core/P1RadioConnection.h"

using namespace NereusSDR;

class TestP1Hl2TxTimingWire : public QObject {
    Q_OBJECT

private:
    static QByteArray bank17(const P1RadioConnection& conn)
    {
        quint8 out[5] = {};
        conn.composeCcForBankForTest(17, out);
        return QByteArray(reinterpret_cast<const char*>(out), 5);
    }

private slots:
    void hl2DefaultsAreTwentyAndTwelve()
    {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesLite);
        const QByteArray b = bank17(conn);
        QCOMPARE(int(quint8(b[0])) & 0xFE, 0x2E);
        QCOMPARE(int(quint8(b[1])), 0);
        QCOMPARE(int(quint8(b[2])), 0);
        QCOMPARE(int(quint8(b[3])), 12);
        QCOMPARE(int(quint8(b[4])), 20);
    }

    void savedValuesReachBank17()
    {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesLite);
        conn.setHl2PttHang(5);
        conn.setHl2TxLatency(50);
        const QByteArray b = bank17(conn);
        QCOMPARE(int(quint8(b[0])) & 0xFE, 0x2E);
        QCOMPARE(int(quint8(b[3])), 5);
        QCOMPARE(int(quint8(b[4])), 50);
    }

    void theRangeEndsFitTheirFields()
    {
        // udPTTHang 0..30 (5 bits), udTxBufferLat 0..70 (7 bits).
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesLite);
        conn.setHl2PttHang(30);
        conn.setHl2TxLatency(70);
        QByteArray b = bank17(conn);
        QCOMPARE(int(quint8(b[3])), 30);
        QCOMPARE(int(quint8(b[4])), 70);
        conn.setHl2PttHang(0);
        conn.setHl2TxLatency(0);
        b = bank17(conn);
        QCOMPARE(int(quint8(b[3])), 0);
        QCOMPARE(int(quint8(b[4])), 0);
    }
};

QTEST_APPLESS_MAIN(TestP1Hl2TxTimingWire)
#include "tst_p1_hl2_tx_timing_wire.moc"
