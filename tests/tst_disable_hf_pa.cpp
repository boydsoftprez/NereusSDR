// no-port-check: test-only. Thetis file names appear only in source-cite
// comments that document which upstream line each assertion verifies.
// No Thetis logic is ported here; this file is NereusSDR-original.
//
// =================================================================
// "Disable HF PA" switches the radio's PA off, on both protocols.
// =================================================================
//
// Setup > Transmit > Power's box was saved and read by nothing. Thetis:
//   From Thetis setup.cs:16750-16754 [v2.10.3.15]
//     console.HFTRRelay = chkHFTRRelay.Checked;
//   From Thetis console.cs:31619-31630 [v2.10.3.15]
//     if (hf_tr_relay) NetworkIO.DisablePA(1); else NetworkIO.DisablePA(0);
//   From Thetis ChannelMaster/netInterface.c:623-631 [v2.10.3.15] DisablePA
//     prn->tx[0].pa = bit; ... CmdGeneral();
// On the wire:
//   P1 bank 10 C3 bit 7 = tx[0].pa (networkproto1.c:586 [v2.10.3.15]); on
//   the HL2 also bank 10 C2 bit 3 = !pa (mi0bot netInterface.c:628-629
//   [@c26a8a4] EnableApolloTuner(!bit)).
//   P2 CmdGeneral byte 58 = !pa (network.c:904 [v2.10.3.15]); and the Alex
//   T/R relay is not engaged while keyed (netInterface.c:378 SetTRXrelay).
// The SWR protection lets a high SWR pass (console.cs:26109-26110
// [v2.10.3.15]: if (tx_xvtr_index >= 0 || hf_tr_relay) swr_pass = true).
// Thetis hides and unchecks the box on the Hermes and the Atlas kit
// (setup.cs:6321-6327 [v2.10.3.15]); NereusSDR shows it disabled with the
// reason and keeps the PA on there.
// =================================================================

#include <QtTest/QtTest>

#include <QCheckBox>

#include "core/AppSettings.h"
#include "core/P1RadioConnection.h"
#include "core/P2RadioConnection.h"
#include "core/RadioDiscovery.h"
#include "core/safety/SwrProtectionController.h"
#include "gui/setup/TransmitSetupPages.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

namespace {

const QString kMac = QStringLiteral("AA:BB:CC:DD:EE:25");
const QString kKey = QStringLiteral("DisableHfPa");
constexpr quint64 k40mHz = 7100000ULL;

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

bool p1Bit7(const P1RadioConnection& conn)
{
    return (quint8(conn.captureBank10ForTest()[3]) & 0x80) != 0;
}

bool p1Hl2PaEnable(const P1RadioConnection& conn)
{
    return (quint8(conn.captureBank10ForTest()[2]) & 0x08) != 0;
}

quint8 p2Byte58(P2RadioConnection& conn)
{
    quint8 buf[60] = {};
    conn.composeCmdGeneralForTest(buf);
    return buf[58];
}

quint32 p2Alex0(P2RadioConnection& conn)
{
    quint8 buf[1444] = {};
    conn.composeCmdHighPriorityForTest(buf);
    return readBE32(buf, 1432);
}

quint32 p2Alex1(P2RadioConnection& conn)
{
    quint8 buf[1444] = {};
    conn.composeCmdHighPriorityForTest(buf);
    return readBE32(buf, 1428);
}

constexpr quint32 kTrRelay = 1u << 27;
constexpr quint32 kTrxStatus = 1u << 18;

// A Core-side model that owns the connection and names its radio.
void prepareCore(RadioModel& model, HPSDRHW board, RadioConnection* conn)
{
    model.setBoardForTest(board);
    RadioInfo info;
    info.macAddress = kMac;
    info.boardType = board;
    model.setLastRadioInfoForTest(info);
    model.setConnectionStateForTest(ConnectionState::Connected);
    model.injectConnectionForTest(conn);
}

} // namespace

class TestDisableHfPa : public QObject {
    Q_OBJECT

private slots:
    void init()    { AppSettings::instance().remove(kKey); }
    void cleanup() { AppSettings::instance().remove(kKey); }

    // ── Protocol 1 (Angelia): bank 10 C3 bit 7 ───────────────────────────
    void p1_bank10Bit7FollowsDisablePa()
    {
        ConnectedP1 conn;
        conn.setBoardForTest(HPSDRHW::Angelia);
        conn.setReceiverFrequency(0, k40mHz);
        // Keyed with the relay engaged: bit 7 clear while the PA is on.
        conn.setMox(true);
        conn.setTrxRelay(true);
        QVERIFY(!p1Bit7(conn));

        conn.setPaDisabled(true);
        QVERIFY(conn.forceBank10NextForTest());
        QVERIFY(p1Bit7(conn));
        // The rest of C3 (the high-pass word) is untouched.
        conn.setPaDisabled(false);
        const quint8 hpf = quint8(conn.captureBank10ForTest()[3]) & 0x7F;
        conn.setPaDisabled(true);
        QCOMPARE(quint8(quint8(conn.captureBank10ForTest()[3]) & 0x7F), hpf);

        conn.setPaDisabled(false);
        QVERIFY(!p1Bit7(conn));
        conn.setTrxRelay(false);
        conn.setMox(false);
    }

    // ── Protocol 1 (HL2): bank 10 C2 bit 3 is the PA enable ──────────────
    void p1Hl2_paEnableBitClearsWhenDisabled()
    {
        ConnectedP1 conn;
        conn.setBoardForTest(HPSDRHW::HermesLite);
        conn.setReceiverFrequency(0, k40mHz);
        conn.setMox(true);
        conn.setTrxRelay(true);
        QVERIFY(p1Hl2PaEnable(conn));
        QVERIFY(!p1Bit7(conn));

        conn.setPaDisabled(true);
        QVERIFY(!p1Hl2PaEnable(conn));
        QVERIFY(p1Bit7(conn));

        conn.setPaDisabled(false);
        QVERIFY(p1Hl2PaEnable(conn));
        QVERIFY(!p1Bit7(conn));
        conn.setTrxRelay(false);
        conn.setMox(false);
    }

    // ── Protocol 2 (G2): CmdGeneral byte 58 and the T/R relay ────────────
    void p2_byte58AndRelayFollowDisablePa()
    {
        ConnectedP2 conn;
        conn.setBoardForTest(HPSDRHW::Saturn);
        conn.setReceiverFrequency(2, k40mHz);
        QCOMPARE(p2Byte58(conn), quint8(1));

        conn.setMox(true);
        conn.setTrxRelay(true);
        QVERIFY(p2Alex0(conn) & kTrRelay);
        QVERIFY(p2Alex0(conn) & kTrxStatus);
        QVERIFY(p2Alex1(conn) & kTrxStatus);

        conn.setPaDisabled(true);
        QCOMPARE(p2Byte58(conn), quint8(0));
        QVERIFY(!(p2Alex0(conn) & kTrRelay));
        QVERIFY(!(p2Alex0(conn) & kTrxStatus));
        QVERIFY(!(p2Alex1(conn) & kTrxStatus));

        conn.setPaDisabled(false);
        QCOMPARE(p2Byte58(conn), quint8(1));
        QVERIFY(p2Alex0(conn) & kTrRelay);
        QVERIFY(p2Alex1(conn) & kTrxStatus);
        conn.setTrxRelay(false);
        conn.setMox(false);
    }

    // ── The SWR protection lets a high SWR pass with the HF PA off ───────
    void swrProtection_passesWithHfPaDisabled()
    {
        safety::SwrProtectionController swr;
        swr.setEnabled(true);
        swr.setLimit(2.0f);
        // 100 W forward, 25 W reflected: SWR 3, four readings trip.
        for (int i = 0; i < 4; ++i) {
            swr.ingest(100.0f, 25.0f, false);
        }
        QVERIFY(swr.highSwr());
        swr.onMoxOff();
        QVERIFY(!swr.highSwr());

        swr.setHfPaDisabled(true);
        for (int i = 0; i < 8; ++i) {
            swr.ingest(100.0f, 25.0f, false);
        }
        QVERIFY(!swr.highSwr());
        QCOMPARE(swr.protectFactor(), 1.0f);
    }

    // ── The Power page applies it to the model's radio at once ───────────
    void powerPage_appliesToTheRadioAndSwr()
    {
        ConnectedP2 conn;
        conn.setBoardForTest(HPSDRHW::Saturn);
        RadioModel model;
        prepareCore(model, HPSDRHW::Saturn, &conn);

        PowerPage page(&model);
        auto* box = page.findChild<QCheckBox*>(QStringLiteral("chkHFTRRelay"));
        QVERIFY(box);
        QVERIFY(box->isVisibleTo(&page));
        QVERIFY(box->isEnabled());
        QCOMPARE(box->toolTip(), QStringLiteral("Disables HF PA."));

        box->setChecked(true);
        QCOMPARE(AppSettings::instance().value(kKey).toString(), QStringLiteral("True"));
        QVERIFY(conn.paDisabled());
        QCOMPARE(p2Byte58(conn), quint8(0));
        QVERIFY(model.swrProt().hfPaDisabled());

        box->setChecked(false);
        QVERIFY(!conn.paDisabled());
        QCOMPARE(p2Byte58(conn), quint8(1));
        QVERIFY(!model.swrProt().hfPaDisabled());

        model.injectConnectionForTest(nullptr);
    }

    // ── The saved value applies when the radio connects ──────────────────
    void savedValue_appliesThroughTheApply()
    {
        AppSettings::instance().setValue(kKey, QStringLiteral("True"));
        ConnectedP1 conn;
        conn.setBoardForTest(HPSDRHW::Angelia);
        RadioModel model;
        prepareCore(model, HPSDRHW::Angelia, &conn);
        model.applyDisableHfPaSetting();
        QVERIFY(conn.paDisabled());
        QVERIFY(p1Bit7(conn));
        // A removal applies the default, off.
        model.applyDisableHfPaSetting(QVariant());
        QVERIFY(!conn.paDisabled());
        model.injectConnectionForTest(nullptr);
    }

    // ── Hermes and the Atlas kit: off, the box disabled with the reason ──
    void hermesAndAtlas_keepThePaOnAndSayWhy_data()
    {
        QTest::addColumn<int>("board");
        QTest::newRow("hermes") << int(HPSDRHW::Hermes);
        QTest::newRow("atlas") << int(HPSDRHW::Atlas);
    }
    void hermesAndAtlas_keepThePaOnAndSayWhy()
    {
        QFETCH(int, board);
        AppSettings::instance().setValue(kKey, QStringLiteral("True"));
        ConnectedP1 conn;
        conn.setBoardForTest(HPSDRHW(board));
        RadioModel model;
        prepareCore(model, HPSDRHW(board), &conn);
        QVERIFY(!RadioModel::hfPaSwitchAvailable(model.hardwareProfile().model));

        model.applyDisableHfPaSetting();
        QVERIFY(!conn.paDisabled());
        QVERIFY(!model.swrProt().hfPaDisabled());

        PowerPage page(&model);
        auto* box = page.findChild<QCheckBox*>(QStringLiteral("chkHFTRRelay"));
        QVERIFY(box);
        QVERIFY(box->isVisibleTo(&page));
        QVERIFY(!box->isEnabled());
        QCOMPARE(box->toolTip(), RadioModel::hfPaSwitchUnavailableReason());
        // The saved value is kept for a radio that offers the box.
        QCOMPARE(AppSettings::instance().value(kKey).toString(), QStringLiteral("True"));
        model.injectConnectionForTest(nullptr);
    }
};

QTEST_MAIN(TestDisableHfPa)
#include "tst_disable_hf_pa.moc"
