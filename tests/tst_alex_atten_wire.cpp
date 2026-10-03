// no-port-check: test-only. Thetis file names appear only in source-cite
// comments that document which upstream line each assertion verifies.
// No Thetis logic is ported here; this file is NereusSDR-original.
//
// =================================================================
// The Alex receive attenuator and the step attenuator above 31 dB on the
// wire, per board family, on Protocol 1 and Protocol 2.
// =================================================================
//
// Level Cal (JJ's ruling, 2026-09-29): each preamp setting drives the step
// attenuator, the preamp bit and the Alex attenuator as Thetis does
// (console.cs:19230-19330 [v2.10.3.15]). The Alex attenuator goes out as
//   From Thetis ChannelMaster/netInterface.c:421-432 [v2.10.3.15]
//     void SetAlexAtten(int bits)
//     { if (mkiibpf) return;
//       if ((prbpfilter->_20_dB_Atten | prbpfilter->_10_dB_Atten) != bits)
//       { prbpfilter->_20_dB_Atten = (bits & 0x2) == 0x2;
//         prbpfilter->_10_dB_Atten = bits & 0x1; ...
//   P1 bank 0 C3 bits 0-1: networkproto1.c:453 [v2.10.3.15]
//     C3 = (prbpfilter->_10_dB_Atten & 1) | ((prbpfilter->_20_dB_Atten << 1) & 2) | ...
//     (mi0bot networkproto1.c:951 [v2.10.3.13-beta2], the HL2's, is the same)
//   P2 Alex0: network.h:284-285 [v2.10.3.15]
//     _20_dB_Atten : 1, // bit 13
//     _10_dB_Atten : 1, // bit 14 (RX MASTER IN SEL RL22)
// Above 31 dB on an Alex board the step attenuator carries the value + 2
// (console.cs:11044-11056 [v2.10.3.15]), so 47 dB is 30 dB of Alex and 17
// of step attenuator: P1 bank 11 C4 0x31, P2 byte 1443 = 49.
//
// Nothing here keys a radio: every connection is an offline test object.
// =================================================================

#include <QtTest/QtTest>

#include "core/BoardCapabilities.h"
#include "core/HardwareProfile.h"
#include "core/P1RadioConnection.h"
#include "core/P2RadioConnection.h"

using namespace NereusSDR;

namespace {

class ConnectedP1 final : public P1RadioConnection {
public:
    ConnectedP1() { setState(ConnectionState::Connected); }
};

class ConnectedP2 final : public P2RadioConnection {
public:
    ConnectedP2() { setState(ConnectionState::Connected); }
};

quint32 readBE32(const quint8* buf, int offset)
{
    return (quint32(buf[offset])     << 24)
         | (quint32(buf[offset + 1]) << 16)
         | (quint32(buf[offset + 2]) << 8)
         |  quint32(buf[offset + 3]);
}

void setUpP1(P1RadioConnection& conn, HPSDRHW board, HPSDRModel model)
{
    conn.setHardwareProfile(profileForRadio(board, model));
    conn.setBoardForTest(board);
}

void setUpP2(P2RadioConnection& conn, HPSDRHW board, HPSDRModel model)
{
    conn.setHardwareProfile(profileForRadio(board, model));
    conn.setBoardForTest(board);
}

int p1AlexBits(const P1RadioConnection& conn)
{
    return quint8(conn.captureBank0ForTest()[3]) & 0x03;
}

// Alex0 bits 13 (20 dB) and 14 (10 dB) as SetAlexAtten's value (2, 1).
int p2AlexBits(const P2RadioConnection& conn)
{
    quint8 buf[1444] = {};
    conn.composeCmdHighPriorityForTest(buf);
    const quint32 reg = readBE32(buf, 1432);
    return ((reg >> 13) & 1u ? 2 : 0) | ((reg >> 14) & 1u ? 1 : 0);
}

quint8 p2StepAttByte(const P2RadioConnection& conn)
{
    quint8 buf[1444] = {};
    conn.composeCmdHighPriorityForTest(buf);
    return buf[1443];
}

} // namespace

class TestAlexAttenWire : public QObject {
    Q_OBJECT
private slots:
    // Hermes-class P1 (ANAN-100): each setting lands in bank 0 C3 bits 0-1
    // and bank 0 goes out on the next frame.
    void p1HermesCarriesTheAlexAttenuator()
    {
        ConnectedP1 conn;
        setUpP1(conn, HPSDRHW::Hermes, HPSDRModel::ANAN100);
        QCOMPARE(p1AlexBits(conn), 0);
        conn.setAlexAtten(1);
        QCOMPARE(p1AlexBits(conn), 1);
        QVERIFY(conn.forceBank0NextForTest());
        conn.setAlexAtten(0);
        conn.setAlexAtten(2);
        QCOMPARE(p1AlexBits(conn), 2);
        conn.setAlexAtten(3);
        QCOMPARE(p1AlexBits(conn), 3);
        conn.setAlexAtten(0);
        QCOMPARE(p1AlexBits(conn), 0);
    }

    // Angelia and Orion (ANAN-100D / 200D) on P1: the same bits.
    void p1AngeliaAndOrionCarryTheAlexAttenuator()
    {
        ConnectedP1 angelia;
        setUpP1(angelia, HPSDRHW::Angelia, HPSDRModel::ANAN100D);
        angelia.setAlexAtten(2);
        QCOMPARE(p1AlexBits(angelia), 2);
        ConnectedP1 orion;
        setUpP1(orion, HPSDRHW::Orion, HPSDRModel::ANAN200D);
        orion.setAlexAtten(3);
        QCOMPARE(p1AlexBits(orion), 3);
    }

    // SetAlexAtten compares the OR of the two bits with the new value, so a
    // change to 1 from 2 or 3 is dropped, as in Thetis.
    void p1ChangeToOneFromTwoOrThreeIsDropped()
    {
        ConnectedP1 conn;
        setUpP1(conn, HPSDRHW::Hermes, HPSDRModel::ANAN100);
        conn.setAlexAtten(3);
        conn.setAlexAtten(1);
        QCOMPARE(p1AlexBits(conn), 3);
        conn.setAlexAtten(2);
        QCOMPARE(p1AlexBits(conn), 2);
        conn.setAlexAtten(1);
        QCOMPARE(p1AlexBits(conn), 2);
        conn.setAlexAtten(0);
        conn.setAlexAtten(1);
        QCOMPARE(p1AlexBits(conn), 1);
    }

    // Mk II band-pass boards ignore it (the bit is their RX master input).
    void p1MkiiBoardIgnoresTheAlexAttenuator()
    {
        ConnectedP1 conn;
        setUpP1(conn, HPSDRHW::OrionMKII, HPSDRModel::ORIONMKII);
        conn.setAlexAtten(3);
        QCOMPARE(p1AlexBits(conn), 0);
    }

    // The HL2's bank 0 has the same bits (mi0bot networkproto1.c:951).
    void p1Hl2BankZeroHasTheSameBits()
    {
        ConnectedP1 conn;
        setUpP1(conn, HPSDRHW::HermesLite, HPSDRModel::HERMESLITE);
        conn.setAlexAtten(2);
        QCOMPARE(p1AlexBits(conn), 2);
    }

    // Above 31 dB an Alex board's step attenuator takes the value + 2:
    // 49 goes out as bank 11 C4 (49 & 0x1F) | 0x20.
    void p1StepAttenuatorAbove31OnAnAlexBoard()
    {
        ConnectedP1 conn;
        setUpP1(conn, HPSDRHW::Hermes, HPSDRModel::ANAN100);
        conn.setAttenuator(49);
        QCOMPARE(conn.currentAttenForTest(), 49);
        QCOMPARE(quint8(conn.captureBank11ForTest()[4]), quint8(0x31));
        conn.setAttenuator(70);
        QCOMPARE(conn.currentAttenForTest(), 63);
    }

    // Boards whose step attenuator stops at 31 still stop there.
    void p1StepAttenuatorStopsAt31WithoutTheAlexRange()
    {
        ConnectedP1 mkii;
        setUpP1(mkii, HPSDRHW::OrionMKII, HPSDRModel::ORIONMKII);
        mkii.setAttenuator(49);
        QCOMPARE(mkii.currentAttenForTest(), 31);
        ConnectedP1 hl2;
        setUpP1(hl2, HPSDRHW::HermesLite, HPSDRModel::HERMESLITE);
        hl2.setAttenuator(49);
        QCOMPARE(hl2.currentAttenForTest(), 31);
    }

    // P2 Angelia (ANAN-100D): Alex0 bits 13 and 14.
    void p2AngeliaCarriesTheAlexAttenuator()
    {
        ConnectedP2 conn;
        setUpP2(conn, HPSDRHW::Angelia, HPSDRModel::ANAN100D);
        QCOMPARE(p2AlexBits(conn), 0);
        conn.setAlexAtten(1);
        QCOMPARE(p2AlexBits(conn), 1);
        conn.setAlexAtten(0);
        conn.setAlexAtten(2);
        QCOMPARE(p2AlexBits(conn), 2);
        conn.setAlexAtten(3);
        QCOMPARE(p2AlexBits(conn), 3);
        conn.setAlexAtten(1);
        QCOMPARE(p2AlexBits(conn), 3);
    }

    // P2 Orion (ANAN-200D): the same.
    void p2OrionCarriesTheAlexAttenuator()
    {
        ConnectedP2 conn;
        setUpP2(conn, HPSDRHW::Orion, HPSDRModel::ANAN200D);
        conn.setAlexAtten(2);
        QCOMPARE(p2AlexBits(conn), 2);
    }

    // P2 Mk II band-pass boards (G2, 7000D) ignore it.
    void p2MkiiBoardIgnoresTheAlexAttenuator()
    {
        ConnectedP2 g2;
        setUpP2(g2, HPSDRHW::Saturn, HPSDRModel::ANAN_G2);
        g2.setAlexAtten(3);
        QCOMPARE(p2AlexBits(g2), 0);
        ConnectedP2 mkii;
        setUpP2(mkii, HPSDRHW::OrionMKII, HPSDRModel::ANAN7000D);
        mkii.setAlexAtten(2);
        QCOMPARE(p2AlexBits(mkii), 0);
    }

    // P2 step attenuator above 31 on an Alex board: byte 1443 = 49.
    void p2StepAttenuatorAbove31OnAnAlexBoard()
    {
        ConnectedP2 angelia;
        setUpP2(angelia, HPSDRHW::Angelia, HPSDRModel::ANAN100D);
        angelia.setAttenuator(49);
        QCOMPARE(p2StepAttByte(angelia), quint8(49));
        ConnectedP2 g2;
        setUpP2(g2, HPSDRHW::Saturn, HPSDRModel::ANAN_G2);
        g2.setAttenuator(49);
        QCOMPARE(p2StepAttByte(g2), quint8(31));
    }

    // The wire range the connections accept: the Alex range + 2 where the
    // board has it, the board's own step attenuator otherwise.
    void stepAttWireMaxFollowsTheAlexRange()
    {
        QCOMPARE(BoardCapsTable::stepAttWireMaxDb(BoardCapsTable::forBoard(HPSDRHW::Hermes)), 63);
        QCOMPARE(BoardCapsTable::stepAttWireMaxDb(BoardCapsTable::forBoard(HPSDRHW::Angelia)), 63);
        QCOMPARE(BoardCapsTable::stepAttWireMaxDb(BoardCapsTable::forBoard(HPSDRHW::Saturn)), 31);
        QCOMPARE(BoardCapsTable::stepAttWireMaxDb(BoardCapsTable::forBoard(HPSDRHW::OrionMKII)), 31);
        QCOMPARE(BoardCapsTable::stepAttWireMaxDb(BoardCapsTable::forBoard(HPSDRHW::HermesLite)), 31);
    }
};

QTEST_MAIN(TestAlexAttenWire)
#include "tst_alex_atten_wire.moc"
