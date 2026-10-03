// no-port-check: test-only. Upstream file names appear only in source-cite
// comments that document which upstream line each assertion verifies.
// No upstream logic is ported here; this file is NereusSDR-original.
//
// Wire-byte tests for bank 0 C3 bits 3 and 4 on connect (P1).
//
// On a Hermes Lite 2 these two bits are not ADC dither and random: mi0bot
// drives bit 3 from the HL2 "Band Volts" option and bit 4 from "Disable PS
// Sync", both off by default, instead of the dither and random options:
//   mi0bot Console/setup.cs:2843-2848 [@c26a8a4]
//     if (HPSDRModel.HERMESLITE == HardwareSpecific.Model)
//     {
//         chkHL2BandVolts_CheckedChanged(this, e);        // MI0BOT: HL2 option page now doesn't share ditter and random
//         chkHL2PsSync_CheckedChanged(this, e);
//     }
//   mi0bot Console/setup.cs:13376-13390 [@c26a8a4]
//     // MI0BOT: Control band volts for the HL2      -> NetworkIO.SetADCDither(v)
//     // MI0BOT: Control power supply sync for the HL2 -> NetworkIO.SetADCRandom(v)
//   mi0bot ChannelMaster/networkproto1.c:950-953 [@c26a8a4] (WriteMainLoop_HL2 case 0)
//     C3 = ... | ((prn->adc[0].dither << 3) & 0b00001000) |
//          ((prn->adc[0].random << 4) & 0b00010000) | ...
// The Thetis HL2 capture (docs/protocols/openhpsdr-protocol1-capture-reference.md,
// bank 0 table) shows C3 = 0x00, both bits clear.
//
// Every other P1 board keeps Thetis's dither and random defaults, both on:
//   Thetis Console/setup.cs:296-298 [v2.10.3.15]
//     //MW0LGE_21k8 initialise these
//     chkMercDither.Checked = true;
//     chkMercRandom.Checked = true;

#include <QtTest/QtTest>
#include "core/P1RadioConnection.h"
#include "core/RadioConnection.h"
#include "core/RadioDiscovery.h"
#include "fakes/P1FakeRadio.h"

using namespace NereusSDR;
using NereusSDR::Test::P1FakeRadio;

namespace {
constexpr int kBandVoltsBit = 0x08; // bank 0 C3 bit 3
constexpr int kPsSyncBit    = 0x10; // bank 0 C3 bit 4
} // namespace

class TestP1Hl2BandVoltsPsSync : public QObject {
    Q_OBJECT

private:
    static RadioInfo makeInfo(P1FakeRadio& fake, HPSDRHW board)
    {
        RadioInfo info;
        info.address         = fake.localAddress();
        info.port            = fake.localPort();
        info.boardType       = board;
        info.protocol        = ProtocolVersion::Protocol1;
        info.macAddress      = QStringLiteral("aa:bb:cc:11:22:33");
        info.firmwareVersion = 72;
        info.name            = QStringLiteral("FakeRadio");
        return info;
    }

    // Connects `conn` as `board` and returns bank 0 C3 once connected.
    static int connectAndReadC3(P1RadioConnection& conn, P1FakeRadio& fake, HPSDRHW board)
    {
        conn.connectToRadio(makeInfo(fake, board));
        if (!QTest::qWaitFor([&conn] { return conn.state() == ConnectionState::Connected; },
                             3000)) {
            return -1;
        }
        return int(quint8(conn.captureBank0ForTest()[3]));
    }

private slots:
    void hl2ConnectWithDefaultsClearsBothBits()
    {
        P1FakeRadio fake;
        fake.start();
        P1RadioConnection conn;
        conn.init();
        conn.setBoardForTest(HPSDRHW::HermesLite);

        const int c3 = connectAndReadC3(conn, fake, HPSDRHW::HermesLite);
        QVERIFY(c3 >= 0);
        QCOMPARE(c3 & kBandVoltsBit, 0);
        QCOMPARE(c3 & kPsSyncBit, 0);

        conn.disconnect();
        fake.stop();
    }

    void hl2BandVoltsOnSetsBit3Only()
    {
        P1FakeRadio fake;
        fake.start();
        P1RadioConnection conn;
        conn.init();
        conn.setBoardForTest(HPSDRHW::HermesLite);
        conn.setHl2BandVolts(true);

        const int c3 = connectAndReadC3(conn, fake, HPSDRHW::HermesLite);
        QVERIFY(c3 >= 0);
        QCOMPARE(c3 & kBandVoltsBit, kBandVoltsBit);
        QCOMPARE(c3 & kPsSyncBit, 0);

        conn.disconnect();
        fake.stop();
    }

    void hl2PsSyncOnSetsBit4Only()
    {
        P1FakeRadio fake;
        fake.start();
        P1RadioConnection conn;
        conn.init();
        conn.setBoardForTest(HPSDRHW::HermesLite);
        conn.setHl2PsSync(true);

        const int c3 = connectAndReadC3(conn, fake, HPSDRHW::HermesLite);
        QVERIFY(c3 >= 0);
        QCOMPARE(c3 & kBandVoltsBit, 0);
        QCOMPARE(c3 & kPsSyncBit, kPsSyncBit);

        conn.disconnect();
        fake.stop();
    }

    void hl2OptionChangedWhileConnectedReachesTheWire()
    {
        P1FakeRadio fake;
        fake.start();
        P1RadioConnection conn;
        conn.init();
        conn.setBoardForTest(HPSDRHW::HermesLite);

        QVERIFY(connectAndReadC3(conn, fake, HPSDRHW::HermesLite) >= 0);

        conn.setHl2BandVolts(true);
        conn.setHl2PsSync(true);
        QVERIFY(conn.forceBank0NextForTest());
        int c3 = int(quint8(conn.captureBank0ForTest()[3]));
        QCOMPARE(c3 & (kBandVoltsBit | kPsSyncBit), kBandVoltsBit | kPsSyncBit);

        conn.setHl2BandVolts(false);
        conn.setHl2PsSync(false);
        c3 = int(quint8(conn.captureBank0ForTest()[3]));
        QCOMPARE(c3 & (kBandVoltsBit | kPsSyncBit), 0);

        conn.disconnect();
        fake.stop();
    }

    void hermesConnectKeepsThetisDitherAndRandom()
    {
        P1FakeRadio fake;
        fake.start();
        P1RadioConnection conn;
        conn.init();
        conn.setBoardForTest(HPSDRHW::Hermes);
        // The HL2 options mean nothing on another board.
        conn.setHl2BandVolts(false);
        conn.setHl2PsSync(false);

        const int c3 = connectAndReadC3(conn, fake, HPSDRHW::Hermes);
        QVERIFY(c3 >= 0);
        QCOMPARE(c3 & kBandVoltsBit, kBandVoltsBit);
        QCOMPARE(c3 & kPsSyncBit, kPsSyncBit);

        conn.disconnect();
        fake.stop();
    }
};

QTEST_MAIN(TestP1Hl2BandVoltsPsSync)
#include "tst_p1_hl2_band_volts_ps_sync.moc"
