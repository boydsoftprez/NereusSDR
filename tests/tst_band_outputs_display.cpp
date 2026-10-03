// no-port-check: test-only. Thetis file names appear only in source-cite
// comments that document which upstream line each assertion verifies.
// No Thetis logic is ported here; this file is NereusSDR-original.
//
// =================================================================
// Every window shows the band-output byte the radio actually gets.
// =================================================================
//
// Plan Task 14 fix wave, I1 and I2 (R-R3-49; remote parity). Thetis's Setup
// LED strip shows the bits UpdateExtCtrl returned:
//   From Thetis console.cs:29104-29107 [v2.10.3.15]
//     if (penny_ext_ctrl_enabled) //MW0LGE_21k
//     {
//         int bits = Penny.getPenny().UpdateExtCtrl(lo_band, lo_bandb, _mox, _tuning, SetupForm.TestIMD, chkExternalPA.Checked); //MW0LGE_21j
//         if (!IsSetupFormNull) SetupForm.UpdateOCLedStrip(_mox, bits);
//
// Before the fix:
// - the HL2 I/O tab's strip moved only on IoBoardHl2::currentOcByteChanged,
//   which only the Core's own connection emits, so in a remote window it
//   never moved;
// - the OC Outputs tab computed OcMatrix::maskFor(pan 1's band, MOX), which
//   in a cross-band split is the other slice's pins, and ignores the HL2's
//   receive bypass.
//
// The scenario is a cross-band split on the N2ADR board: slice A on 40 m
// with pan 1, slice B on 20 m holding the transmitter. On the wire:
// - unkeyed: 0x48, the receive pins of the higher slice (B, 20 m). In Auto
//   the HL2's receive pins follow the highest-frequency slice on the input
//   rather than bypassing the board (JJ's ruling of 2026-09-30);
// - keyed on B: 0x08 (the 30/20 m transmit low-pass).
// A byte computed from pan 1 (40 m) would be 40 m's pins in both states
// (0x44 unkeyed, 0x04 keyed).
//
// Remote-window parity Task 14 (R-R3-46): HL2 Options' output strip is not
// one of these displays any more. It shows the I/O board's output register
// read back, as mi0bot's ucOutPinsLedStripHF does (tst_hl2_options_tab,
// tst_remote_hl2_io).
// =================================================================
//
// Modification history (NereusSDR):
//   2026-09-30 - Unkeyed, the cross-band split now expects the 20 m receive
//                pins instead of the N2ADR receive bypass (0x00): in Auto the
//                HL2 pins follow the highest-frequency slice (JJ's ruling).
//                The roles are swapped (A and pan 1 on 40 m, B on 20 m
//                transmitting), so the wire differs from pan 1 both unkeyed
//                and keyed. J.J. Boyd (KG4VCF), with AI-assisted
//                implementation via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QSignalSpy>
#include <QTemporaryDir>

#include <memory>

#include "core/AppSettings.h"
#include "core/OcMatrix.h"
#include "core/P1RadioConnection.h"
#include "core/ReceiverManager.h"
#include "core/settings/SettingsProxy.h"
#include "core/TxSliceArbiter.h"
#include "core/accessories/N2adrPreset.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "fakes/LoopbackTransport.h"
#include "fakes/UpgradedCoreToken.h"
#include "gui/setup/hardware/Hl2IoBoardTab.h"
#include "gui/setup/hardware/OcOutputsHfTab.h"
#include "models/Band.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

constexpr double k20mHz = 14200000.0;
constexpr double k40mHz =  7100000.0;
constexpr quint8 kN2adrTx20m = 0x08;   // N2adrPreset.cpp: 20 m transmit, pin 4
// N2adrPreset.cpp: 20 m receive, pin 4 plus pin 7. Unkeyed, the split's
// wire byte: the pins of the higher slice, B.
constexpr quint8 kN2adrRx20m = 0x48;

class ConnectedP1 final : public P1RadioConnection {
public:
    ConnectedP1() { setState(ConnectionState::Connected); }
    // The connection's own state machine, as the watchdog and the
    // reconnect timer drive it (P1RadioConnection.cpp onWatchdogTick /
    // onReconnectTimeout), on this same object.
    void setStateForTest(ConnectionState s) { setState(s); }
};

struct DetachConnection {
    RadioModel* model{nullptr};
    ~DetachConnection() { if (model) { model->injectConnectionForTest(nullptr); } }
};

// Bank 0 C2 = (ocByte << 1) & 0xFE. Composing bank 0 is what sends it, and
// what makes the connection report it.
quint8 composeOcByte(const P1RadioConnection& conn)
{
    quint8 bank0[5] = {};
    conn.composeCcForBankForTest(0, bank0);
    return quint8(bank0[2] >> 1);
}

// A Core with an HL2 on Protocol 1, the N2ADR board and the production
// ReceiverManager -> connection and connection -> model wiring.
struct Hl2Core {
    Hl2Core()
    {
        AppSettings::instance().clear();
        // As CoreInit's migrations leave it, so the window's StationClient
        // starts from a migrated profile.
        AppSettings::instance().setValue(QStringLiteral("SettingsSchemaVersion"),
                                         QStringLiteral("6"));
        applyN2adrPreset(oc, true);
        model.setBoardForTest(HPSDRHW::HermesLite);
        RadioInfo info;
        info.macAddress = QStringLiteral("AA:BB:CC:DD:EE:16");
        info.name = QStringLiteral("Bench HL2");
        info.boardType = HPSDRHW::HermesLite;
        model.setLastRadioInfoForTest(info);
        conn.setBoardForTest(HPSDRHW::HermesLite);
        conn.setOcMatrix(&oc);
        model.injectConnectionForTest(&conn);
        detach.model = &model;
        model.wireReceiverManagerHardwarePushesForTest();
        model.wireBandOutputsReportForTest();
        model.configureStreamPool(2, 2, 192000);
        model.receiverManager()->createReceiver();
        model.receiverManager()->createReceiver();
        model.addPanadapter();
    }
    ~Hl2Core() { AppSettings::instance().clear(); }

    int add(double hz)
    {
        const int id = model.addSlice();
        model.sliceById(id)->setFrequency(hz);
        return id;
    }

    // The cross-band split, pan 1 on A's band (40 m), B on 20 m transmits.
    void crossBandSplit()
    {
        const int a = add(k40mHz);
        const int b = add(k20mHz);
        model.setActiveSlice(a);
        model.panadapters().first()->setCenterFrequency(k40mHz);
        QVERIFY(model.txSliceArbiter()->requestHandoff(b));
    }

    // Send a frame and let the queued report reach the model.
    quint8 send()
    {
        const quint8 wire = composeOcByte(conn);
        QCoreApplication::processEvents();
        return wire;
    }

    OcMatrix         oc;
    RadioModel       model;
    ConnectedP1      conn;
    DetachConnection detach;
};

} // namespace

class TestBandOutputsDisplay : public QObject {
    Q_OBJECT

private slots:
    // A settings profile of this process's own: other test executables run
    // in parallel and share the default one.
    void initTestCase()
    {
        AppSettings::setProfileOverride(
            QStringLiteral("band-outputs-display-%1").arg(QCoreApplication::applicationPid()));
        AppSettings::instance().clear();
    }
    void cleanupTestCase()
    {
        const QString path = AppSettings::instance().filePath();
        QFile::remove(path);
        QFile::remove(path + QStringLiteral(".bak"));
    }

    // ── Local window: the Core's own Setup ───────────────────────────────
    void local_crossBandSplit_showsTheWireByte()
    {
        Hl2Core core;
        core.crossBandSplit();

        OcOutputsHfTab ocTab(&core.model, &core.oc);
        Hl2IoBoardTab ioTab(&core.model);

        // What a byte computed from pan 1 would show: 40 m's pins.
        QCOMPARE(core.oc.maskFor(Band::Band20m, /*tx=*/false), kN2adrRx20m);
        QVERIFY(core.oc.maskFor(Band::Band40m, /*tx=*/false) != kN2adrRx20m);
        QCOMPARE(core.oc.maskFor(Band::Band20m, /*tx=*/true), kN2adrTx20m);
        QVERIFY(core.oc.maskFor(Band::Band40m, /*tx=*/true)  != kN2adrTx20m);

        // Unkeyed: the receive pins of the higher slice, B on 20 m.
        QCOMPARE(core.send(), kN2adrRx20m);
        QVERIFY(core.model.bandOutputsKnown());
        QCOMPARE(core.model.bandOutputsByte(), int(kN2adrRx20m));
        QCOMPARE(core.model.bandOutputsKeyed(), false);
        QCOMPARE(ocTab.currentOcByteForTest(), kN2adrRx20m);
        QCOMPARE(ioTab.ocShownByteForTest(), int(kN2adrRx20m));
        QCOMPARE(ioTab.ocKeyedTextForTest(), QStringLiteral("RX"));

        // Keyed on B: B's 20 m transmit pin.
        core.conn.setMox(true);
        QCOMPARE(core.send(), kN2adrTx20m);
        QCOMPARE(core.model.bandOutputsByte(), int(kN2adrTx20m));
        QCOMPARE(core.model.bandOutputsBand(), int(Band::Band20m));
        QCOMPARE(core.model.bandOutputsKeyed(), true);
        QCOMPARE(ocTab.currentOcByteForTest(), kN2adrTx20m);
        QVERIFY(ocTab.livePinLitForTest(3));
        QCOMPARE(ioTab.ocShownByteForTest(), int(kN2adrTx20m));
        QCOMPARE(ioTab.ocByteTextForTest(), QStringLiteral("0X08"));
        QCOMPARE(ioTab.ocBandTextForTest(), QStringLiteral("band=20m"));
        QCOMPARE(ioTab.ocKeyedTextForTest(), QStringLiteral("TX"));

        // Unkeyed again.
        core.conn.setMox(false);
        QCOMPARE(core.send(), kN2adrRx20m);
        QCOMPARE(ocTab.currentOcByteForTest(), kN2adrRx20m);
        QCOMPARE(ioTab.ocShownByteForTest(), int(kN2adrRx20m));
    }

    // ── Protocol 1's automatic reconnect, on the same connection ─────────
    //
    // Re-review N1: leaving Connected clears the model's copy, so the
    // connection must forget what it last reported too. Otherwise the same
    // byte, composed again after the reconnect, is suppressed as unchanged
    // and every display stays blank until the band or the key changes.
    void p1Reconnect_sameConnection_showsTheByteAgain()
    {
        Hl2Core core;
        core.crossBandSplit();
        core.model.setConnectionStateForTest(ConnectionState::Connected);
        Hl2IoBoardTab ioTab(&core.model);

        QCOMPARE(core.send(), kN2adrRx20m);
        QVERIFY(core.model.bandOutputsKnown());
        QCOMPARE(ioTab.ocShownByteForTest(), int(kN2adrRx20m));

        // The watchdog declares the link lost; the model forgets the byte.
        for (ConnectionState s : {ConnectionState::LinkLost, ConnectionState::Connecting}) {
            core.conn.setStateForTest(s);
            core.model.setConnectionStateForTest(s);
        }
        QVERIFY(!core.model.bandOutputsKnown());
        QCOMPARE(ioTab.ocShownByteForTest(), -1);

        // The retry succeeds on the same object and composes the same byte.
        core.conn.setStateForTest(ConnectionState::Connected);
        core.model.setConnectionStateForTest(ConnectionState::Connected);
        QCOMPARE(core.send(), kN2adrRx20m);
        QVERIFY(core.model.bandOutputsKnown());
        QCOMPARE(core.model.bandOutputsByte(), int(kN2adrRx20m));
        QCOMPARE(core.model.bandOutputsKeyed(), false);
        QCOMPARE(ioTab.ocShownByteForTest(), int(kN2adrRx20m));
        QCOMPARE(ioTab.ocKeyedTextForTest(), QStringLiteral("RX"));
    }

    // ── A tab shows the byte it is given, never one of its own ───────────
    //
    // The model reports a byte no matrix, band or MOX state would produce
    // here (an empty matrix composes 0). A tab that recomputed would show 0.
    void tabs_showTheReportedByte_notARecomputedOne()
    {
        RadioModel model;
        model.addPanadapter();
        model.panadapters().first()->setCenterFrequency(k20mHz);
        model.ocMatrixMutable().setPin(Band::Band20m, 0, /*tx=*/false, true);
        OcOutputsHfTab ocTab(&model, &model.ocMatrixMutable());
        Hl2IoBoardTab ioTab(&model);

        // Nothing composed yet: nothing lit, whatever the matrix says.
        QVERIFY(!model.bandOutputsKnown());
        QCOMPARE(ocTab.currentOcByteForTest(), quint8(0));
        QCOMPARE(ioTab.ocShownByteForTest(), -1);
        QCOMPARE(ioTab.ocByteTextForTest(), QStringLiteral("--"));

        model.reportBandOutputsForTest(0x52, int(Band::Band17m), /*keyed=*/true);
        QCOMPARE(ocTab.currentOcByteForTest(), quint8(0x52));
        QCOMPARE(ioTab.ocShownByteForTest(), 0x52);
        QCOMPARE(ioTab.ocBandTextForTest(), QStringLiteral("band=17m"));

        // A matrix edit or a MOX change on its own changes nothing shown:
        // only the connection's next report does.
        model.ocMatrixMutable().setPin(Band::Band20m, 1, /*tx=*/false, true);
        model.transmitModel().setMox(false);
        QCOMPARE(ocTab.currentOcByteForTest(), quint8(0x52));
        QCOMPARE(ioTab.ocShownByteForTest(), 0x52);
    }

    // ── Remote window: the Core's byte through the station link ──────────
    void remote_crossBandSplit_showsTheCoresWireByte()
    {
        QTemporaryDir settingsDir;
        QVERIFY(settingsDir.isValid());
        QTemporaryDir securityDir;
        QVERIFY(securityDir.isValid());
        Hl2Core core;
        core.crossBandSplit();
        core.model.setConnectionStateForTest(ConnectionState::Connected);
        AppSettings settings(settingsDir.filePath(QStringLiteral("NereusSDR.settings")));
        settings.setValue(QStringLiteral("SettingsSchemaVersion"), QStringLiteral("6"));
        StationServer server(&core.model, settings,
                             NereusSDR::Test::seedUpgradedCoreToken(securityDir.path()));

        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        QSignalSpy completed(&client, &StationClient::handshakeComplete);

        // The window's own copy of the matrix and its own pan say 40 m.
        remote.addPanadapter();
        remote.panadapters().first()->setCenterFrequency(k40mHz);
        applyN2adrPreset(remote.ocMatrixMutable(), true);
        OcOutputsHfTab ocTab(&remote, &remote.ocMatrixMutable());
        Hl2IoBoardTab ioTab(&remote);
        QCOMPARE(ioTab.ocShownByteForTest(), -1);

        core.send();   // unkeyed, before the window connects

        auto* stationEnd = new Test::LoopbackTransport(QStringLiteral("oc-station"), this);
        auto* clientEnd  = new Test::LoopbackTransport(QStringLiteral("oc-client"), this);
        stationEnd->linkTo(clientEnd);
        client.startSession(clientEnd, server.token());
        server.acceptTransport(stationEnd);
        QTRY_COMPARE(completed.count(), 1);

        // Unkeyed: the Core's receive pins, B's 20 m.
        QTRY_VERIFY(remote.bandOutputsKnown());
        QCOMPARE(remote.bandOutputsByte(), int(kN2adrRx20m));
        QCOMPARE(remote.bandOutputsKeyed(), false);
        QCOMPARE(ocTab.currentOcByteForTest(), kN2adrRx20m);
        QCOMPARE(ioTab.ocShownByteForTest(), int(kN2adrRx20m));
        QCOMPARE(ioTab.ocKeyedTextForTest(), QStringLiteral("RX"));

        // Keyed on B at the Core: the window shows B's 20 m transmit pin.
        core.conn.setMox(true);
        core.send();
        QTRY_COMPARE(remote.bandOutputsByte(), int(kN2adrTx20m));
        QTRY_COMPARE(remote.bandOutputsKeyed(), true);
        QCOMPARE(remote.bandOutputsBand(), int(Band::Band20m));
        QCOMPARE(ocTab.currentOcByteForTest(), kN2adrTx20m);
        QCOMPARE(ioTab.ocShownByteForTest(), int(kN2adrTx20m));
        QCOMPARE(ioTab.ocBandTextForTest(), QStringLiteral("band=20m"));
        QCOMPARE(ioTab.ocKeyedTextForTest(), QStringLiteral("TX"));

        // Unkeyed again.
        core.conn.setMox(false);
        core.send();
        QTRY_COMPARE(remote.bandOutputsKeyed(), false);
        QCOMPARE(remote.bandOutputsByte(), int(kN2adrRx20m));
        QCOMPARE(ocTab.currentOcByteForTest(), kN2adrRx20m);

        // The window cannot write them.
        QVERIFY(!core.model.applyStationBandOutputsValue("bandOutputsByte", 0x7F));

        // Session over: nothing is known, nothing is lit.
        client.disconnectFromStation(QStringLiteral("operator disconnect"));
        QVERIFY(!remote.bandOutputsKnown());
        QCOMPARE(ioTab.ocShownByteForTest(), -1);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
};

QTEST_MAIN(TestBandOutputsDisplay)
#include "tst_band_outputs_display.moc"
