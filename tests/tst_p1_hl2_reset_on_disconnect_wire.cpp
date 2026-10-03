// no-port-check: test-only. Upstream file names appear only in source-cite
// comments that document which upstream line each assertion verifies.
// No upstream logic is ported here; this file is NereusSDR-original.
//
// Wire-byte tests for the HL2 "Reset on Ethernet disconnect" option (P1
// bank 18).
//
//   mi0bot Console/setup.cs:21257-21262 [@c26a8a4]
//     // MI0BOT: Controls if the HL2 will reset after an Ethernet disconnect
//     private void chkDisconnectReset_CheckedChanged(object sender, EventArgs e)
//     {
//         int v = chkDisconnectReset.Checked ? 1 : 0;
//         NetworkIO.SetResetOnDisconnect(v);
//     }
//   mi0bot ChannelMaster/netInterface.c:816-823 [@c26a8a4]
//     PORT // MI0BOT: Causes a HL2 to perform a reset on disconnect
//     void SetResetOnDisconnect(int bit) { ... prn->reset_on_disconnect = bit & 0x1; }
//   mi0bot ChannelMaster/networkproto1.c:1170-1176 [@c26a8a4] (WriteMainLoop_HL2)
//     case 18: // Reset on disconnect 0x3a
//         C0 |= 0x74; //C0 0111 010x
//         C1 = 0; C2 = 0; C3 = 0;
//         C4 = prn->reset_on_disconnect;
//   Default off: netInterface.c:1724 [@c26a8a4]
//     prn->reset_on_disconnect = 0;	// MI0BOT: Intialised to not reset on software disconnect

#include <QtTest/QtTest>
#include "core/P1RadioConnection.h"

using namespace NereusSDR;

class TestP1Hl2ResetOnDisconnectWire : public QObject {
    Q_OBJECT

private:
    static QByteArray bank18(const P1RadioConnection& conn)
    {
        quint8 out[5] = {};
        conn.composeCcForBankForTest(18, out);
        return QByteArray(reinterpret_cast<const char*>(out), 5);
    }

private slots:
    void hl2DefaultIsOff()
    {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesLite);
        const QByteArray b = bank18(conn);
        QCOMPARE(int(quint8(b[0])) & 0xFE, 0x74);
        QCOMPARE(int(quint8(b[1])), 0);
        QCOMPARE(int(quint8(b[2])), 0);
        QCOMPARE(int(quint8(b[3])), 0);
        QCOMPARE(int(quint8(b[4])), 0);
    }

    void onSetsC4AndOffClearsIt()
    {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesLite);
        conn.setHl2ResetOnDisconnect(true);
        QByteArray b = bank18(conn);
        QCOMPARE(int(quint8(b[0])) & 0xFE, 0x74);
        QCOMPARE(int(quint8(b[3])), 0);
        QCOMPARE(int(quint8(b[4])), 1);
        conn.setHl2ResetOnDisconnect(false);
        b = bank18(conn);
        QCOMPARE(int(quint8(b[4])), 0);
    }
};

QTEST_APPLESS_MAIN(TestP1Hl2ResetOnDisconnectWire)
#include "tst_p1_hl2_reset_on_disconnect_wire.moc"
