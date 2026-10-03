// no-port-check: test-only. Thetis file names appear only in source-cite
// comments that document which upstream line each assertion verifies.
// No Thetis logic is ported here; this file is NereusSDR-original.
//
// =================================================================
// "HPF Bypass on TX" bypasses the Alex high-pass while keyed.
// =================================================================
//
// Plan Task 14 (R-R3-49: every control does what its label says). The
// Setup > Hardware > Alex check box was saved but nothing read it, on
// either protocol. Thetis:
//   From Thetis console.cs:6839-6848 [v2.10.3.15]
//     private void setAlexHPF(double freq)
//     { if (alexpresent && !initializing)
//       { if (_mox && disable_hpf_on_tx)
//         { NetworkIO.SetAlexHPFBits(0x20); ... return; }
// and the band-pass boards (Orion MkII, Saturn, HermesC10) the same:
//   From Thetis console.cs:6957 [v2.10.3.15]
//     if (_mox && (disable_hpf_on_tx || (disable_hpf_on_ps && PureSignalEnabled)))
// The setter re-applies at once (console.cs:18754-18762 DisableHPFonTX).
//
// Both routes a change can take are covered: the Setup tab on the Core
// itself, and a remote window's write arriving at the Core
// (RadioModel::scheduleRemoteHardwareApply).
// =================================================================

#include <QtTest/QtTest>

#include <QCheckBox>

#include "core/AppSettings.h"
#include "core/P1RadioConnection.h"
#include "core/P2RadioConnection.h"
#include "core/RadioDiscovery.h"
#include "core/codec/AlexFilterMap.h"
#include "gui/setup/hardware/AntennaAlexAlex1Tab.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

namespace {

const QString kMac = QStringLiteral("AA:BB:CC:DD:EE:14");
constexpr quint64 k40mHz = 7100000ULL;
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

// P1: bank 10 C3, bit 7 is the T/R relay.
quint8 p1Hpf(const P1RadioConnection& conn)
{
    return quint8(conn.captureBank10ForTest()[3]) & 0x7F;
}

// P2: the Alex0 HPF bits, inverse of P2CodecOrionMkII::buildAlex0's
// scatter (Thetis ChannelMaster/netInterface.c:605-621 [v2.10.3.15]).
quint8 p2Hpf(P2RadioConnection& conn)
{
    quint8 buf[1444] = {};
    conn.composeCmdHighPriorityForTest(buf);
    const quint32 reg = readBE32(buf, 1432);
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

QCheckBox* bypassOnTxBox(AntennaAlexAlex1Tab& tab)
{
    for (QCheckBox* box : tab.findChildren<QCheckBox*>()) {
        if (box->text() == QStringLiteral("HPF Bypass on TX")) {
            return box;
        }
    }
    return nullptr;
}

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

class TestHpfBypassOnTx : public QObject {
    Q_OBJECT

private slots:
    void init()    { AppSettings::instance().clearHardwareValues(kMac); }
    void cleanup() { AppSettings::instance().clearHardwareValues(kMac); }

    // ── Protocol 1, from the Setup tab ───────────────────────────────────
    void p1_setupTabBypassesTheHighPassWhileKeyed()
    {
        ConnectedP1 conn;
        conn.setBoardForTest(HPSDRHW::Angelia);
        conn.setReceiverFrequency(0, k40mHz);
        const quint8 filtered = p1Hpf(conn);
        QCOMPARE(filtered,
                 codec::alex::computeRxPreselector(k40mHz / 1e6, HPSDRHW::Angelia));
        QVERIFY(filtered != kBypass);

        RadioModel model;
        prepareCore(model, HPSDRHW::Angelia, &conn);
        AntennaAlexAlex1Tab tab(&model);
        tab.restoreSettings(kMac);
        QCheckBox* box = bypassOnTxBox(tab);
        QVERIFY(box);

        // Off: keyed keeps the receive high-pass, as before.
        conn.setMox(true);
        QCOMPARE(p1Hpf(conn), filtered);

        // On, while keyed: applied at once.
        box->setChecked(true);
        QCOMPARE(p1Hpf(conn), kBypass);

        // Unkeyed is untouched by the setting.
        conn.setMox(false);
        QCOMPARE(p1Hpf(conn), filtered);
        conn.setMox(true);
        QCOMPARE(p1Hpf(conn), kBypass);

        // Off again.
        box->setChecked(false);
        QCOMPARE(p1Hpf(conn), filtered);
        conn.setMox(false);

        model.injectConnectionForTest(nullptr);
    }

    // ── Protocol 2 (G2), from the Setup tab ──────────────────────────────
    void p2_setupTabBypassesTheHighPassWhileKeyed()
    {
        ConnectedP2 conn;
        conn.setBoardForTest(HPSDRHW::Saturn);
        conn.setReceiverFrequency(2, k40mHz);
        const quint8 filtered = p2Hpf(conn);
        QVERIFY(filtered != kBypass);

        RadioModel model;
        prepareCore(model, HPSDRHW::Saturn, &conn);
        AntennaAlexAlex1Tab tab(&model);
        tab.restoreSettings(kMac);
        QCheckBox* box = bypassOnTxBox(tab);
        QVERIFY(box);

        conn.setMox(true);
        QCOMPARE(p2Hpf(conn), filtered);
        box->setChecked(true);
        QCOMPARE(p2Hpf(conn), kBypass);
        conn.setMox(false);
        QCOMPARE(p2Hpf(conn), filtered);
        box->setChecked(false);
        conn.setMox(true);
        QCOMPARE(p2Hpf(conn), filtered);
        conn.setMox(false);

        model.injectConnectionForTest(nullptr);
    }

    // ── A remote window's change reaches the Core ────────────────────────
    //
    // The window's write lands in the Core's settings through StationServer,
    // which then calls scheduleRemoteHardwareApply with the key.
    void remoteWindowWrite_reachesTheCore()
    {
        ConnectedP2 conn;
        conn.setBoardForTest(HPSDRHW::Saturn);
        conn.setReceiverFrequency(2, k40mHz);
        const quint8 filtered = p2Hpf(conn);

        RadioModel core;
        prepareCore(core, HPSDRHW::Saturn, &conn);
        QStringList reloads;
        core.setHardwareApplyObserverForTest([&reloads](const QString& name) { reloads << name; });

        const QString key =
            QStringLiteral("hardware/%1/alex/master/hpfBypassOnTx").arg(kMac);
        AppSettings::instance().setValue(key, QStringLiteral("True"));
        core.scheduleRemoteHardwareApply(key);
        QTRY_COMPARE(reloads, QStringList{QStringLiteral("alex")});

        conn.setMox(true);
        QCOMPARE(p2Hpf(conn), kBypass);
        conn.setMox(false);

        reloads.clear();
        AppSettings::instance().setValue(key, QStringLiteral("False"));
        core.scheduleRemoteHardwareApply(key);
        QTRY_COMPARE(reloads, QStringList{QStringLiteral("alex")});
        conn.setMox(true);
        QCOMPARE(p2Hpf(conn), filtered);
        conn.setMox(false);

        core.injectConnectionForTest(nullptr);
    }

    // ── The saved value is applied on connect ────────────────────────────
    //
    // The connect path calls applyHpfBypassOnTxSetting after the radio's
    // settings are known. A remote window's model has no radio and does
    // nothing (its save goes to the Core).
    void savedValue_isApplied_andARemoteModelDoesNothing()
    {
        AppSettings::instance().setHardwareValue(
            kMac, QStringLiteral("alex/master/hpfBypassOnTx"), QStringLiteral("True"));

        ConnectedP1 conn;
        conn.setBoardForTest(HPSDRHW::Angelia);
        conn.setReceiverFrequency(0, k40mHz);
        RadioModel model;
        prepareCore(model, HPSDRHW::Angelia, &conn);
        model.applyHpfBypassOnTxSetting();
        conn.setMox(true);
        QCOMPARE(p1Hpf(conn), kBypass);
        conn.setMox(false);
        model.injectConnectionForTest(nullptr);

        ConnectedP1 other;
        other.setBoardForTest(HPSDRHW::Angelia);
        other.setReceiverFrequency(0, k40mHz);
        RadioModel remote(RadioModel::Role::Remote);
        prepareCore(remote, HPSDRHW::Angelia, &other);
        remote.applyHpfBypassOnTxSetting();
        other.setMox(true);
        QCOMPARE(p1Hpf(other), codec::alex::computeRxPreselector(k40mHz / 1e6, HPSDRHW::Angelia));
        other.setMox(false);
        remote.injectConnectionForTest(nullptr);
    }

    // ── The HL2 has no Alex board: its bank 10 C3 is left alone ──────────
    void hl2_isUntouched()
    {
        ConnectedP1 conn;
        conn.setBoardForTest(HPSDRHW::HermesLite);
        conn.setReceiverFrequency(0, k40mHz);
        conn.setHpfBypassOnTx(true);
        conn.setMox(true);
        const quint8 keyed = p1Hpf(conn);
        conn.setHpfBypassOnTx(false);
        QCOMPARE(p1Hpf(conn), keyed);
        conn.setMox(false);
    }
};

QTEST_MAIN(TestHpfBypassOnTx)
#include "tst_hpf_bypass_on_tx.moc"
