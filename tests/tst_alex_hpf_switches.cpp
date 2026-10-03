// no-port-check: test-only. Thetis file names appear only in source-cite
// comments that document which upstream line each assertion verifies.
// No Thetis logic is ported here; this file is NereusSDR-original.
//
// =================================================================
// The Alex tab's high-pass switches reach the wire, as Thetis wires them.
// =================================================================
//
// Plan Task 14 fix wave (R-R3-49: every control does what its label says).
// The Setup > Hardware > Alex tab saved these and nothing read them.
//
// HPF Bypass on PureSignal feedback:
//   From Thetis setup.cs:29440-29458 [v2.10.3.15]
//     chkDisableHPFonPS_CheckedChanged -> console.DisableHPFonPS = ...
//   From Thetis console.cs:18764-18773 [v2.10.3.15]
//     DisableHPFonPS { set { disable_hpf_on_ps = value; ... setAlex1HPF(freq); } }
//   From Thetis console.cs:6957 [v2.10.3.15] (setBPF1ForOrionIISaturn)
//     if (_mox && (disable_hpf_on_tx || (disable_hpf_on_ps && PureSignalEnabled)))
// and setBPF1ForOrionIISaturn runs only for Orion MkII, Saturn and
// HermesC10 (setAlex1HPF, console.cs:6827-6836).
//
// HPF Bypass (the master switch, chkAlexHPFBypass "ByPass/55 MHz HPF"):
//   From Thetis setup.cs:15374-15379 [v2.10.3.15]
//     chkAlexHPFBypass_CheckedChanged -> console.AlexHPFBypass = ...
//   From Thetis console.cs:18793-18803 [v2.10.3.15]
//     AlexHPFBypass { set { alex_hpf_bypass = value; ... setAlex1HPF(freq); ... } }
//   From Thetis console.cs:6850-6855 and 6965-6970 [v2.10.3.15]
//     if (alex_hpf_bypass) { NetworkIO.SetAlexHPFBits(0x20); ... return; }
// keyed or not, on every Alex board.
//
// Disable 6m LNA on TX / on RX (chkDisable6mLNAonTX, default on, and
// chkDisable6mLNAonRX):
//   From Thetis setup.cs:15340-15350 [v2.10.3.15]
//     console.Disable6mLNAonTX = ...; console.Disable6mLNAonRX = ...;
//   From Thetis console.cs:18719-18751 [v2.10.3.15] (each setter re-applies)
//   (inline attribution at console.cs:18731, verbatim:
//     HardwareSpecific.Model == HPSDRModel.ANAN_G2_1K || HardwareSpecific.Model == HPSDRModel.REDPITAYA) //DH1KLM)
//   From Thetis console.cs:6931-6936 and 7046-7051 [v2.10.3.15]
//     if (alex6bphpf_bypass || disable_6m_lna_on_rx || (_mox && disable_6m_lna_on_tx))
//     { NetworkIO.SetAlexHPFBits(0x20); // Bypass HPF
// in place of the 6 m BPF/LNA selection (0x40), on the band's own branch.
//
// Each switch is driven from the Setup tab on the Core, and from a remote
// window's write arriving at the Core (scheduleRemoteHardwareApply).
// =================================================================

#include <QtTest/QtTest>

#include <QCheckBox>

#include "core/AppSettings.h"
#include "core/P1RadioConnection.h"
#include "core/ReceiverManager.h"
#include "core/accessories/AlexController.h"
#include "core/P2RadioConnection.h"
#include "core/RadioDiscovery.h"
#include "core/codec/AlexFilterMap.h"
#include "gui/setup/hardware/AntennaAlexAlex1Tab.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "OperatorWording.h"

using namespace NereusSDR;

namespace {

const QString kMac = QStringLiteral("AA:BB:CC:DD:EE:15");
constexpr quint64 k40mHz = 7100000ULL;
constexpr quint64 k6mHz  = 50125000ULL;
constexpr quint8  k6mLna = 0x40;
constexpr quint8  kBypass = 0x20;

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

quint8 hpfFromWord(quint32 reg)
{
    quint8 bits = 0;
    if (reg & (1u << 1))  { bits |= 0x01; }
    if (reg & (1u << 2))  { bits |= 0x02; }
    if (reg & (1u << 4))  { bits |= 0x04; }
    if (reg & (1u << 5))  { bits |= 0x08; }
    if (reg & (1u << 6))  { bits |= 0x10; }
    if (reg & (1u << 12)) { bits |= 0x20; }
    if (reg & (1u << 3))  { bits |= 0x40; }
    return bits;
}

// Alex0, the word SetAlexHPFBits writes.
quint8 p2Alex0Hpf(P2RadioConnection& conn)
{
    quint8 buf[1444] = {};
    conn.composeCmdHighPriorityForTest(buf);
    return hpfFromWord(readBE32(buf, 1432));
}

// Alex1, which none of these switches touches.
quint8 p2Alex1Hpf(P2RadioConnection& conn)
{
    quint8 buf[1444] = {};
    conn.composeCmdHighPriorityForTest(buf);
    return hpfFromWord(readBE32(buf, 1428));
}

quint8 p1Hpf(const P1RadioConnection& conn)
{
    return quint8(conn.captureBank10ForTest()[3]) & 0x7F;
}

QCheckBox* boxNamed(AntennaAlexAlex1Tab& tab, const QString& text)
{
    for (QCheckBox* box : tab.findChildren<QCheckBox*>()) {
        if (box->text() == text) {
            return box;
        }
    }
    return nullptr;
}

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

// A G2 Core on Protocol 2 with one slice at `hz` on chain 0: stream pool,
// a receiver per stream, and the slice's stream bound, so the per-chain
// filter state (AlexController, rxFilter0Effective, the WIDE badge) is
// computed from a live slice as it is with a radio.
// `board` defaults to the G2's; the PureSignal case also runs it on a
// high-pass board.
SliceModel* g2CoreWithSlice(RadioModel& model, P2RadioConnection& conn, double hz,
                            HPSDRHW board = HPSDRHW::Saturn)
{
    conn.setBoardForTest(board);
    conn.setReceiverFrequency(2, quint64(hz));
    prepareCore(model, board, &conn);
    model.configureStreamPool(5, 5, 192000);
    for (int st = 0; st < 5; ++st) {
        model.receiverManager()->createReceiver();
    }
    const int id = model.addSlice();
    SliceModel* slice = model.sliceById(id);
    if (slice) {
        slice->setFrequency(hz);
    }
    model.requestDdcAssignment();
    return slice;
}

bool setSwitch(const char* key, bool on)
{
    AppSettings::instance().setValue(
        QStringLiteral("hardware/%1/alex/master/%2").arg(kMac, QLatin1String(key)),
        on ? QStringLiteral("True") : QStringLiteral("False"));
    return on;
}

} // namespace

class TestAlexHpfSwitches : public QObject {
    Q_OBJECT

private slots:
    void init()    { AppSettings::instance().clearHardwareValues(kMac); }
    void cleanup() { AppSettings::instance().clearHardwareValues(kMac); }

    // ── HPF Bypass on PureSignal feedback: the boards Thetis names ───────
    void psBypass_followsTheSetupTab_onBandPassBoards_data()
    {
        QTest::addColumn<int>("board");
        QTest::newRow("Saturn (G2)")      << int(HPSDRHW::Saturn);
        QTest::newRow("HermesC10 (G2E)")  << int(HPSDRHW::HermesC10);
        QTest::newRow("Orion MkII")       << int(HPSDRHW::OrionMKII);
    }
    void psBypass_followsTheSetupTab_onBandPassBoards()
    {
        QFETCH(int, board);
        const HPSDRHW hw = HPSDRHW(board);
        ConnectedP2 conn;
        conn.setBoardForTest(hw);
        conn.setReceiverFrequency(2, k40mHz);
        const quint8 filtered = p2Alex0Hpf(conn);
        QVERIFY(filtered != kBypass);
        const quint8 alex1 = p2Alex1Hpf(conn);

        RadioModel model;
        prepareCore(model, hw, &conn);
        AntennaAlexAlex1Tab tab(&model);
        tab.restoreSettings(kMac);
        tab.setImdWarningResultForTest(AntennaAlexAlex1Tab::TestImdResult::ConfirmOk);
        QCheckBox* box = tab.hpfBypassOnPsCheckboxForTest();
        QVERIFY(box);
        QVERIFY(box->isChecked());  // Thetis's default

        conn.setPuresignalRun(true);
        conn.setMox(true);
        // On (the default): keyed with PureSignal, Alex0 is the bypass alone,
        // as SetAlexHPFBits(0x20) leaves it. Alex1 is not written.
        QCOMPARE(p2Alex0Hpf(conn), kBypass);
        QCOMPARE(p2Alex1Hpf(conn), alex1);

        // Off: the band's filter stays in while keyed with PureSignal.
        box->setChecked(false);
        QCOMPARE(p2Alex0Hpf(conn), filtered);

        // On again, applied at once.
        box->setChecked(true);
        QCOMPARE(p2Alex0Hpf(conn), kBypass);

        // Unkeyed, or keyed without PureSignal, the switch does nothing.
        conn.setMox(false);
        QCOMPARE(p2Alex0Hpf(conn), filtered);
        conn.setPuresignalRun(false);
        conn.setMox(true);
        QCOMPARE(p2Alex0Hpf(conn), filtered);
        conn.setMox(false);

        model.injectConnectionForTest(nullptr);
    }

    // ── Other boards: setAlexHPF has no PureSignal arm ───────────────────
    void psBypass_isNotAppliedOnHighPassBoards()
    {
        ConnectedP2 conn;
        conn.setBoardForTest(HPSDRHW::Orion);  // ANAN-200D on Protocol 2
        conn.setReceiverFrequency(2, k40mHz);
        const quint8 filtered = p2Alex0Hpf(conn);
        QVERIFY(filtered != kBypass);
        QVERIFY(conn.hpfBypassOnPs());

        conn.setPuresignalRun(true);
        conn.setMox(true);
        QCOMPARE(p2Alex0Hpf(conn), filtered);
        conn.setMox(false);
    }

    // ── Protocol 1 with a band-pass board: Thetis's rule is the same ─────
    void psBypass_p1BandPassBoard()
    {
        ConnectedP1 conn;
        conn.setBoardForTest(HPSDRHW::OrionMKII);
        conn.setReceiverFrequency(0, k40mHz);
        const quint8 filtered = p1Hpf(conn);
        QVERIFY(filtered != kBypass);

        conn.setPuresignalRun(true);
        conn.setMox(true);
        QCOMPARE(p1Hpf(conn), kBypass);
        conn.setHpfBypassOnPs(false);
        QCOMPARE(p1Hpf(conn), filtered);
        conn.setMox(false);
        conn.setPuresignalRun(false);
    }

    // ── A remote window's change reaches the Core ────────────────────────
    void psBypass_remoteWindowWrite_reachesTheCore()
    {
        ConnectedP2 conn;
        conn.setBoardForTest(HPSDRHW::Saturn);
        conn.setReceiverFrequency(2, k40mHz);
        const quint8 filtered = p2Alex0Hpf(conn);

        RadioModel core;
        prepareCore(core, HPSDRHW::Saturn, &conn);
        QStringList reloads;
        core.setHardwareApplyObserverForTest([&reloads](const QString& name) { reloads << name; });

        conn.setPuresignalRun(true);
        conn.setMox(true);
        QCOMPARE(p2Alex0Hpf(conn), kBypass);

        const QString key =
            QStringLiteral("hardware/%1/alex/master/hpfBypassOnPs").arg(kMac);
        AppSettings::instance().setValue(key, QStringLiteral("False"));
        core.scheduleRemoteHardwareApply(key);
        QTRY_COMPARE(reloads, QStringList{QStringLiteral("alex")});
        QCOMPARE(p2Alex0Hpf(conn), filtered);

        reloads.clear();
        AppSettings::instance().setValue(key, QStringLiteral("True"));
        core.scheduleRemoteHardwareApply(key);
        QTRY_COMPARE(reloads, QStringList{QStringLiteral("alex")});
        QCOMPARE(p2Alex0Hpf(conn), kBypass);
        conn.setMox(false);

        core.injectConnectionForTest(nullptr);
    }
    // ── HPF Bypass (master): keyed or not, both protocols ────────────────
    void masterBypass_followsTheSetupTab_data()
    {
        QTest::addColumn<int>("protocol");
        QTest::addColumn<int>("board");
        QTest::newRow("P1 Angelia (high-pass ladder)") << 1 << int(HPSDRHW::Angelia);
        QTest::newRow("P1 Orion MkII (band-pass)")     << 1 << int(HPSDRHW::OrionMKII);
        QTest::newRow("P2 Saturn (G2)")                << 2 << int(HPSDRHW::Saturn);
        QTest::newRow("P2 Orion (ANAN-200D)")          << 2 << int(HPSDRHW::Orion);
    }
    void masterBypass_followsTheSetupTab()
    {
        QFETCH(int, protocol);
        QFETCH(int, board);
        const HPSDRHW hw = HPSDRHW(board);
        ConnectedP1 p1;
        ConnectedP2 p2;
        RadioConnection* conn = nullptr;
        if (protocol == 1) {
            p1.setBoardForTest(hw);
            p1.setReceiverFrequency(0, k40mHz);
            conn = &p1;
        } else {
            p2.setBoardForTest(hw);
            p2.setReceiverFrequency(2, k40mHz);
            conn = &p2;
        }
        auto hpf = [&]() { return protocol == 1 ? p1Hpf(p1) : p2Alex0Hpf(p2); };
        const quint8 filtered = hpf();
        QVERIFY(filtered != kBypass);
        const quint8 alex1 = protocol == 2 ? p2Alex1Hpf(p2) : 0;

        RadioModel model;
        prepareCore(model, hw, conn);
        AntennaAlexAlex1Tab tab(&model);
        tab.restoreSettings(kMac);
        QCheckBox* box = boxNamed(tab, QStringLiteral("HPF Bypass (master)"));
        QVERIFY(box);
        QVERIFY(!box->isChecked());  // Thetis's default

        box->setChecked(true);
        QCOMPARE(hpf(), kBypass);
        if (protocol == 2) {
            QCOMPARE(p2Alex1Hpf(p2), alex1);
        }
        conn->setMox(true);
        QCOMPARE(hpf(), kBypass);
        conn->setMox(false);

        box->setChecked(false);
        QCOMPARE(hpf(), filtered);

        model.injectConnectionForTest(nullptr);
    }

    // ── HPF Bypass (master) from a remote window ─────────────────────────
    void masterBypass_remoteWindowWrite_reachesTheCore()
    {
        ConnectedP1 conn;
        conn.setBoardForTest(HPSDRHW::Angelia);
        conn.setReceiverFrequency(0, k40mHz);
        const quint8 filtered = p1Hpf(conn);

        RadioModel core;
        prepareCore(core, HPSDRHW::Angelia, &conn);
        QStringList reloads;
        core.setHardwareApplyObserverForTest([&reloads](const QString& name) { reloads << name; });

        const QString key =
            QStringLiteral("hardware/%1/alex/master/hpfBypass").arg(kMac);
        AppSettings::instance().setValue(key, QStringLiteral("True"));
        core.scheduleRemoteHardwareApply(key);
        QTRY_COMPARE(reloads, QStringList{QStringLiteral("alex")});
        QCOMPARE(p1Hpf(conn), kBypass);

        reloads.clear();
        AppSettings::instance().setValue(key, QStringLiteral("False"));
        core.scheduleRemoteHardwareApply(key);
        QTRY_COMPARE(reloads, QStringList{QStringLiteral("alex")});
        QCOMPARE(p1Hpf(conn), filtered);

        core.injectConnectionForTest(nullptr);
    }

    // ── The HL2 has no Alex board: the master switch leaves it alone ─────
    void masterBypass_hl2IsUntouched()
    {
        ConnectedP1 conn;
        conn.setBoardForTest(HPSDRHW::HermesLite);
        conn.setReceiverFrequency(0, k40mHz);
        const quint8 before = p1Hpf(conn);
        codec::alex::Alex1HpfSwitches sw;
        sw.hpfBypass = true;
        QCOMPARE(codec::alex::applyAlex1HpfSwitches(before, HPSDRHW::HermesLite,
                                                    false, false, sw),
                 kBypass);  // the rule itself would bypass...
        conn.setAlexHpfBypass(true);
        QCOMPARE(p1Hpf(conn), before);  // ...but the HL2 has no Alex board
    }
    // ── Disable 6m LNA on TX / on RX, both protocols ─────────────────────
    void sixMetreLna_followsTheSetupTab_data()
    {
        QTest::addColumn<int>("protocol");
        QTest::addColumn<int>("board");
        QTest::newRow("P1 Angelia (high-pass ladder)") << 1 << int(HPSDRHW::Angelia);
        QTest::newRow("P1 Orion MkII (band-pass)")     << 1 << int(HPSDRHW::OrionMKII);
        QTest::newRow("P2 Saturn (G2)")                << 2 << int(HPSDRHW::Saturn);
        QTest::newRow("P2 Orion (ANAN-200D)")          << 2 << int(HPSDRHW::Orion);
    }
    void sixMetreLna_followsTheSetupTab()
    {
        QFETCH(int, protocol);
        QFETCH(int, board);
        const HPSDRHW hw = HPSDRHW(board);
        ConnectedP1 p1;
        ConnectedP2 p2;
        RadioConnection* conn = nullptr;
        auto tune = [&](quint64 hz) {
            if (protocol == 1) { p1.setReceiverFrequency(0, hz); }
            else               { p2.setReceiverFrequency(2, hz); }
        };
        if (protocol == 1) { p1.setBoardForTest(hw); conn = &p1; }
        else               { p2.setBoardForTest(hw); conn = &p2; }
        tune(k6mHz);
        auto hpf = [&]() { return protocol == 1 ? p1Hpf(p1) : p2Alex0Hpf(p2); };

        RadioModel model;
        prepareCore(model, hw, conn);
        AntennaAlexAlex1Tab tab(&model);
        tab.restoreSettings(kMac);
        QCheckBox* onTx = boxNamed(tab, QStringLiteral("Disable 6m LNA on TX"));
        QCheckBox* onRx = boxNamed(tab, QStringLiteral("Disable 6m LNA on RX"));
        QVERIFY(onTx && onRx);
        QVERIFY(onTx->isChecked());   // Thetis's defaults
        QVERIFY(!onRx->isChecked());
        // The tab applies what it restored; the connection's own defaults
        // are Thetis's too.
        model.applyAlexHpfSwitchSettings();

        // Receiving on 6 m the LNA is in.
        QCOMPARE(hpf(), k6mLna);
        // Keyed, the TX switch (on) bypasses it.
        conn->setMox(true);
        QCOMPARE(hpf(), kBypass);
        // Off: keyed keeps the LNA.
        onTx->setChecked(false);
        QCOMPARE(hpf(), k6mLna);
        conn->setMox(false);

        // The RX switch bypasses it receiving, keyed or not.
        onRx->setChecked(true);
        QCOMPARE(hpf(), kBypass);
        conn->setMox(true);
        QCOMPARE(hpf(), kBypass);
        conn->setMox(false);

        // Off 6 m neither switch changes the band's filter.
        onTx->setChecked(true);
        tune(k40mHz);
        const quint8 filtered = codec::alex::computeRxPreselector(k40mHz / 1e6, hw);
        QCOMPARE(hpf(), filtered);
        conn->setMox(true);
        QCOMPARE(hpf(), filtered);
        conn->setMox(false);

        onRx->setChecked(false);
        model.injectConnectionForTest(nullptr);
    }

    // ── Disable 6m LNA on RX from a remote window ────────────────────────
    void sixMetreLna_remoteWindowWrite_reachesTheCore()
    {
        ConnectedP2 conn;
        conn.setBoardForTest(HPSDRHW::Saturn);
        conn.setReceiverFrequency(2, k6mHz);
        QCOMPARE(p2Alex0Hpf(conn), k6mLna);

        RadioModel core;
        prepareCore(core, HPSDRHW::Saturn, &conn);
        QStringList reloads;
        core.setHardwareApplyObserverForTest([&reloads](const QString& name) { reloads << name; });

        const QString key =
            QStringLiteral("hardware/%1/alex/master/disable6mLnaOnRx").arg(kMac);
        AppSettings::instance().setValue(key, QStringLiteral("True"));
        core.scheduleRemoteHardwareApply(key);
        QTRY_COMPARE(reloads, QStringList{QStringLiteral("alex")});
        QCOMPARE(p2Alex0Hpf(conn), kBypass);

        const QString txKey =
            QStringLiteral("hardware/%1/alex/master/disable6mLnaOnTx").arg(kMac);
        reloads.clear();
        AppSettings::instance().setValue(key, QStringLiteral("False"));
        AppSettings::instance().setValue(txKey, QStringLiteral("False"));
        core.scheduleRemoteHardwareApply(key);
        core.scheduleRemoteHardwareApply(txKey);
        QTRY_COMPARE(reloads, QStringList{QStringLiteral("alex")});
        QCOMPARE(p2Alex0Hpf(conn), k6mLna);
        conn.setMox(true);
        QCOMPARE(p2Alex0Hpf(conn), k6mLna);
        conn.setMox(false);

        core.injectConnectionForTest(nullptr);
    }

    // ── The WIDE badge and rxFilter*Effective report the switches ────────
    //
    // Re-review N4 (Phase 3F design section 16.4.1): "WIDE means: the RX
    // preselector chain feeding this panadapter is bypassed on the wire
    // right now." HPF Bypass (master) puts 0x20 in Alex0 whatever the band,
    // so chain 0 is bypassed on the wire; the badge and the published
    // effective state must say so. The wire itself must not change.
    void masterBypass_isReportedAsWide_onChain0()
    {
        ConnectedP2 conn;
        RadioModel model;
        SliceModel* a = g2CoreWithSlice(model, conn, double(k40mHz));
        QVERIFY(a);
        QVERIFY(a->streamIndex() >= 0);
        QCOMPARE(model.chainForStream(a->streamIndex()), 0);
        model.applyAlexHpfSwitchSettings();
        const quint8 filtered = p2Alex0Hpf(conn);
        QVERIFY(filtered != kBypass);
        const quint8 alex1 = p2Alex1Hpf(conn);
        QCOMPARE(model.rxFilter0Effective(), int(AlexController::BpfEffective::Filtered));
        QVERIFY(!model.panBypassState({a->sliceIndex()}).bypassed);

        setSwitch("hpfBypass", true);
        model.applyAlexHpfSwitchSettings();
        // The wire: Alex0 bypassed, Alex1 untouched (as before this change).
        QCOMPARE(p2Alex0Hpf(conn), kBypass);
        QCOMPARE(p2Alex1Hpf(conn), alex1);
        // What every window is told.
        QCOMPARE(model.rxFilter0Effective(), int(AlexController::BpfEffective::Bypass));
        QVERIFY(!model.rxFilter0Reason().isEmpty());
        const RadioModel::PanBypassState wide = model.panBypassState({a->sliceIndex()});
        QVERIFY2(wide.bypassed, "HPF Bypass is on the wire but the pan shows no WIDE");
        QVERIFY2(wide.reason.contains(QStringLiteral("HPF Bypass")),
                 qPrintable(wide.reason));
        QVERIFY(!wide.reason.contains(QStringLiteral(".cs:")));
        // Alex1 is not written by the switch: chain 1 is not reported wide.
        QCOMPARE(model.rxFilter1Effective(), int(AlexController::BpfEffective::Filtered));

        setSwitch("hpfBypass", false);
        model.applyAlexHpfSwitchSettings();
        QCOMPARE(p2Alex0Hpf(conn), filtered);
        QCOMPARE(model.rxFilter0Effective(), int(AlexController::BpfEffective::Filtered));
        QVERIFY(!model.panBypassState({a->sliceIndex()}).bypassed);

        model.injectConnectionForTest(nullptr);
    }

    // Disable 6m LNA on RX: on 6 m the BPF/LNA selection (0x40) becomes the
    // bypass while receiving, so chain 0 is wide on the wire there, and only
    // there.
    void sixMetreLnaOnRx_isReportedAsWide_on6mOnly()
    {
        ConnectedP2 conn;
        RadioModel model;
        SliceModel* a = g2CoreWithSlice(model, conn, double(k6mHz));
        QVERIFY(a);
        QVERIFY(a->streamIndex() >= 0);
        model.applyAlexHpfSwitchSettings();
        QCOMPARE(p2Alex0Hpf(conn), k6mLna);
        const quint8 alex1 = p2Alex1Hpf(conn);
        QCOMPARE(model.rxFilter0Effective(), int(AlexController::BpfEffective::Filtered));

        setSwitch("disable6mLnaOnRx", true);
        model.applyAlexHpfSwitchSettings();
        QCOMPARE(p2Alex0Hpf(conn), kBypass);
        QCOMPARE(p2Alex1Hpf(conn), alex1);
        QCOMPARE(model.rxFilter0Effective(), int(AlexController::BpfEffective::Bypass));
        const RadioModel::PanBypassState wide = model.panBypassState({a->sliceIndex()});
        QVERIFY2(wide.bypassed, "the 6 m LNA is bypassed on the wire but the pan shows no WIDE");
        QVERIFY2(wide.reason.contains(QStringLiteral("Disable 6m LNA on RX")),
                 qPrintable(wide.reason));

        // Off 6 m the switch changes nothing on the wire, and nothing is shown.
        a->setFrequency(double(k40mHz));
        conn.setReceiverFrequency(2, k40mHz);
        model.requestDdcAssignment();
        QCOMPARE(p2Alex0Hpf(conn),
                 codec::alex::computeRxPreselector(double(k40mHz) / 1e6, HPSDRHW::Saturn));
        QCOMPARE(model.rxFilter0Effective(), int(AlexController::BpfEffective::Filtered));
        QVERIFY(!model.panBypassState({a->sliceIndex()}).bypassed);

        // Back on 6 m it is wide again; switched off it is filtered.
        a->setFrequency(double(k6mHz));
        conn.setReceiverFrequency(2, k6mHz);
        model.requestDdcAssignment();
        QCOMPARE(model.rxFilter0Effective(), int(AlexController::BpfEffective::Bypass));
        setSwitch("disable6mLnaOnRx", false);
        model.applyAlexHpfSwitchSettings();
        QCOMPARE(p2Alex0Hpf(conn), k6mLna);
        QCOMPARE(model.rxFilter0Effective(), int(AlexController::BpfEffective::Filtered));

        model.injectConnectionForTest(nullptr);
    }

    // ── The keyed bypasses (follow-up 2 to Task 14) ─────────────────────
    //
    // HPF Bypass on PureSignal feedback, HPF Bypass on TX and Disable 6m
    // LNA on TX put 0x20 in Alex0 only while keyed. WIDE shows each while it
    // is on the wire (design section 16.4.1), with its own tooltip (section
    // 16.4.4), and rxFilter0Effective says the same. Unkeyed nothing changes.
    // The model is keyed through the codec-context seam, the same MOX and
    // PureSignal inputs the DDC assignment reads; the connection is keyed
    // separately so the wire can be checked beside the report.

    // HPF Bypass on PureSignal feedback, on by default, on the G2 (a
    // band-pass board): WIDE while PureSignal transmits, and only then.
    void psFeedbackBypass_isReportedAsWide_whilePureSignalTransmits()
    {
        ConnectedP2 conn;
        RadioModel model;
        SliceModel* a = g2CoreWithSlice(model, conn, double(k40mHz));
        QVERIFY(a);
        QVERIFY(a->streamIndex() >= 0);
        model.applyAlexHpfSwitchSettings();
        const quint8 filtered = p2Alex0Hpf(conn);
        QVERIFY(filtered != kBypass);
        QCOMPARE(model.rxFilter0Effective(), int(AlexController::BpfEffective::Filtered));

        // Keyed without PureSignal: the filter stays in, nothing is shown.
        conn.setMox(true);
        model.setDdcContextForTest(true, false, false);
        model.requestDdcAssignment();
        QCOMPARE(p2Alex0Hpf(conn), filtered);
        QCOMPARE(model.rxFilter0Effective(), int(AlexController::BpfEffective::Filtered));
        QVERIFY(!model.panBypassState({a->sliceIndex()}).bypassed);

        // Keyed with PureSignal: the bypass is on the wire, and WIDE says so.
        conn.setPuresignalRun(true);
        model.setDdcContextForTest(true, true, false);
        model.requestDdcAssignment();
        QCOMPARE(p2Alex0Hpf(conn), kBypass);
        QCOMPARE(model.rxFilter0Effective(), int(AlexController::BpfEffective::Bypass));
        const RadioModel::PanBypassState wide = model.panBypassState({a->sliceIndex()});
        QVERIFY2(wide.bypassed, "PureSignal's bypass is on the wire but the pan shows no WIDE");
        // Design section 16.4.4, "PureSignal TX" row, verbatim.
        QCOMPARE(wide.reason,
                 QStringLiteral("Preselector bypassed while PureSignal is transmitting, so "
                                "the feedback path sees an unfiltered coupler signal. "
                                "Filtering returns when transmit ends."));
        QVERIFY(OperatorWording::isPlain(wide.reason));
        QVERIFY(!wide.reason.contains(QStringLiteral(".cs:")));
        QCOMPARE(model.rxFilter1Effective(), int(AlexController::BpfEffective::Filtered));

        // The switch off: keyed with PureSignal, the filter stays in.
        setSwitch("hpfBypassOnPs", false);
        model.applyAlexHpfSwitchSettings();
        QCOMPARE(p2Alex0Hpf(conn), filtered);
        QCOMPARE(model.rxFilter0Effective(), int(AlexController::BpfEffective::Filtered));
        setSwitch("hpfBypassOnPs", true);
        model.applyAlexHpfSwitchSettings();
        QCOMPARE(model.rxFilter0Effective(), int(AlexController::BpfEffective::Bypass));

        // Transmit ends: filtered again.
        conn.setMox(false);
        model.setDdcContextForTest(false, true, false);
        model.requestDdcAssignment();
        QCOMPARE(p2Alex0Hpf(conn), filtered);
        QCOMPARE(model.rxFilter0Effective(), int(AlexController::BpfEffective::Filtered));
        QVERIFY(!model.panBypassState({a->sliceIndex()}).bypassed);

        conn.setPuresignalRun(false);
        model.injectConnectionForTest(nullptr);
    }

    // setAlexHPF, which the high-pass boards run, has no PureSignal arm: on
    // the ANAN-200D keyed with PureSignal the filter stays in, and nothing
    // is shown.
    void psFeedbackBypass_isNotReported_onHighPassBoards()
    {
        ConnectedP2 conn;
        RadioModel model;
        SliceModel* a = g2CoreWithSlice(model, conn, double(k40mHz), HPSDRHW::Orion);
        QVERIFY(a);
        QVERIFY(a->streamIndex() >= 0);
        model.applyAlexHpfSwitchSettings();
        const quint8 filtered = p2Alex0Hpf(conn);
        QVERIFY(filtered != kBypass);

        conn.setPuresignalRun(true);
        conn.setMox(true);
        model.setDdcContextForTest(true, true, false);
        model.requestDdcAssignment();
        QCOMPARE(p2Alex0Hpf(conn), filtered);
        QCOMPARE(model.rxFilter0Effective(), int(AlexController::BpfEffective::Filtered));
        QVERIFY(!model.panBypassState({a->sliceIndex()}).bypassed);

        conn.setMox(false);
        conn.setPuresignalRun(false);
        model.injectConnectionForTest(nullptr);
    }

    // HPF Bypass on TX: WIDE while keyed, on any band.
    void hpfBypassOnTx_isReportedAsWide_whileKeyed()
    {
        ConnectedP2 conn;
        RadioModel model;
        SliceModel* a = g2CoreWithSlice(model, conn, double(k40mHz));
        QVERIFY(a);
        QVERIFY(a->streamIndex() >= 0);
        setSwitch("hpfBypassOnTx", true);
        model.applyAlexHpfSwitchSettings();
        const quint8 filtered = p2Alex0Hpf(conn);
        QVERIFY(filtered != kBypass);
        // Unkeyed the switch does nothing, on the wire or in the report.
        QCOMPARE(model.rxFilter0Effective(), int(AlexController::BpfEffective::Filtered));
        QVERIFY(!model.panBypassState({a->sliceIndex()}).bypassed);

        conn.setMox(true);
        model.setDdcContextForTest(true, false, false);
        model.requestDdcAssignment();
        QCOMPARE(p2Alex0Hpf(conn), kBypass);
        QCOMPARE(model.rxFilter0Effective(), int(AlexController::BpfEffective::Bypass));
        const RadioModel::PanBypassState wide = model.panBypassState({a->sliceIndex()});
        QVERIFY2(wide.bypassed, "HPF Bypass on TX is on the wire but the pan shows no WIDE");
        QCOMPARE(wide.reason,
                 QStringLiteral("Preselector bypassed while transmitting by the HPF Bypass on "
                                "TX setting on the Antenna / ALEX page of the hardware setup. "
                                "Filtering returns when transmit ends."));
        QVERIFY(OperatorWording::isPlain(wide.reason));
        QVERIFY(!wide.reason.contains(QStringLiteral(".cs:")));
        QCOMPARE(model.rxFilter1Effective(), int(AlexController::BpfEffective::Filtered));

        // The switch off while keyed: filtered.
        setSwitch("hpfBypassOnTx", false);
        model.applyAlexHpfSwitchSettings();
        QCOMPARE(p2Alex0Hpf(conn), filtered);
        QCOMPARE(model.rxFilter0Effective(), int(AlexController::BpfEffective::Filtered));
        setSwitch("hpfBypassOnTx", true);
        model.applyAlexHpfSwitchSettings();
        QCOMPARE(model.rxFilter0Effective(), int(AlexController::BpfEffective::Bypass));

        // Transmit ends: filtered again.
        conn.setMox(false);
        model.setDdcContextForTest(false, false, false);
        model.requestDdcAssignment();
        QCOMPARE(p2Alex0Hpf(conn), filtered);
        QCOMPARE(model.rxFilter0Effective(), int(AlexController::BpfEffective::Filtered));
        QVERIFY(!model.panBypassState({a->sliceIndex()}).bypassed);

        model.injectConnectionForTest(nullptr);
    }

    // Disable 6m LNA on TX, on by default: WIDE while keyed on 6 m, and only
    // on 6 m.
    void sixMetreLnaOnTx_isReportedAsWide_whileKeyedOn6m()
    {
        ConnectedP2 conn;
        RadioModel model;
        SliceModel* a = g2CoreWithSlice(model, conn, double(k6mHz));
        QVERIFY(a);
        QVERIFY(a->streamIndex() >= 0);
        model.applyAlexHpfSwitchSettings();
        QCOMPARE(p2Alex0Hpf(conn), k6mLna);
        QCOMPARE(model.rxFilter0Effective(), int(AlexController::BpfEffective::Filtered));

        conn.setMox(true);
        model.setDdcContextForTest(true, false, false);
        model.requestDdcAssignment();
        QCOMPARE(p2Alex0Hpf(conn), kBypass);
        QCOMPARE(model.rxFilter0Effective(), int(AlexController::BpfEffective::Bypass));
        const RadioModel::PanBypassState wide = model.panBypassState({a->sliceIndex()});
        QVERIFY2(wide.bypassed, "the 6 m LNA is bypassed on the wire but the pan shows no WIDE");
        QCOMPARE(wide.reason,
                 QStringLiteral("Preselector bypassed on 6 m while transmitting by the Disable "
                                "6m LNA on TX setting on the Antenna / ALEX page of the "
                                "hardware setup. Filtering returns when transmit ends."));
        QVERIFY(OperatorWording::isPlain(wide.reason));
        QVERIFY(!wide.reason.contains(QStringLiteral(".cs:")));

        // Keyed on 40 m the switch changes nothing, and nothing is shown.
        a->setFrequency(double(k40mHz));
        conn.setReceiverFrequency(2, k40mHz);
        model.requestDdcAssignment();
        QCOMPARE(p2Alex0Hpf(conn),
                 codec::alex::computeRxPreselector(double(k40mHz) / 1e6, HPSDRHW::Saturn));
        QCOMPARE(model.rxFilter0Effective(), int(AlexController::BpfEffective::Filtered));
        QVERIFY(!model.panBypassState({a->sliceIndex()}).bypassed);

        // Back on 6 m, keyed: wide. The switch off: filtered.
        a->setFrequency(double(k6mHz));
        conn.setReceiverFrequency(2, k6mHz);
        model.requestDdcAssignment();
        QCOMPARE(model.rxFilter0Effective(), int(AlexController::BpfEffective::Bypass));
        setSwitch("disable6mLnaOnTx", false);
        model.applyAlexHpfSwitchSettings();
        QCOMPARE(p2Alex0Hpf(conn), k6mLna);
        QCOMPARE(model.rxFilter0Effective(), int(AlexController::BpfEffective::Filtered));
        setSwitch("disable6mLnaOnTx", true);
        model.applyAlexHpfSwitchSettings();
        QCOMPARE(model.rxFilter0Effective(), int(AlexController::BpfEffective::Bypass));

        // Transmit ends: filtered, the LNA back in.
        conn.setMox(false);
        model.setDdcContextForTest(false, false, false);
        model.requestDdcAssignment();
        QCOMPARE(p2Alex0Hpf(conn), k6mLna);
        QCOMPARE(model.rxFilter0Effective(), int(AlexController::BpfEffective::Filtered));
        QVERIFY(!model.panBypassState({a->sliceIndex()}).bypassed);

        model.injectConnectionForTest(nullptr);
    }
};

QTEST_MAIN(TestAlexHpfSwitches)
#include "tst_alex_hpf_switches.moc"
