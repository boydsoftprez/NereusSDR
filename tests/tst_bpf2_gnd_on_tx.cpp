// no-port-check: test-only. Thetis file names appear only in source-cite
// comments that document which upstream line each assertion verifies.
// No Thetis logic is ported here; this file is NereusSDR-original.
//
// =================================================================
// "Gnd BPF2 on Tx" grounds the BPF2 input while keyed (P2 Alex1 bit 8).
// =================================================================
//
// Anvelina filter/OC plan, Task 5. NereusSDR used to set Alex1 bit 8
// (_rx2_gnd) on every key. Thetis makes it a Setup box, unticked on a fresh
// install, read at the TX edge:
//   From Thetis console.cs:29091 [v2.10.3.15] HdwMOXChanged
//     if (bpf2_gnd) NetworkIO.SetBPF2Gnd(1);
//   From Thetis console.cs:29170 [v2.10.3.15] (RX edge)
//     NetworkIO.SetBPF2Gnd(0);
//   From Thetis setup.cs:6190-6208 [v2.10.3.15] (shown on 8000D, 7000D,
//     G2, G2 1K, ANVELINAPRO3, REDPITAYA; hidden on every other model)
//     HardwareSpecific.Model == HPSDRModel.REDPITAYA) //DH1KLM
//
// Covered: the wire (keyed set when ticked, clear when unticked, unkeyed
// clear either way, taken at the TX edge), the box (model visibility, per
// MAC persistence, default unticked), a remote window's write reaching the
// Core (scheduleRemoteHardwareApply), and a remote window on a Core below
// radioHardwareVersion 14 (disabled with its reason).
// =================================================================

#include <QtTest/QtTest>

#include <QApplication>
#include <QCheckBox>

#include "core/AlexSettingsKeys.h"
#include "core/AppSettings.h"
#include "core/P2RadioConnection.h"
#include "core/RadioDiscovery.h"
#include "core/SkuUiProfile.h"
#include "core/accessories/AlexAntennaFacade.h"
#include "core/session/IStationLink.h"
#include "core/session/StationServer.h"
#include "gui/setup/HardwarePage.h"
#include "gui/setup/hardware/AntennaAlexTab.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

namespace {

const QString kMac = QStringLiteral("AA:BB:CC:DD:B2:01");
const QString kOtherMac = QStringLiteral("AA:BB:CC:DD:B2:02");
constexpr quint64 k40mHz = 7100000ULL;
constexpr quint32 kRx2Gnd = 1u << 8;

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

// Alex1 is high-priority bytes 1428-1431 (P2CodecOrionMkII::buildAlex1).
bool bpf2Grounded(P2RadioConnection& conn)
{
    quint8 buf[1444] = {};
    conn.composeCmdHighPriorityForTest(buf);
    return (readBE32(buf, 1428) & kRx2Gnd) != 0;
}

RadioInfo anvelina(const QString& mac)
{
    RadioInfo info;
    info.macAddress = mac;
    info.boardType = HPSDRHW::OrionMKII;
    info.modelOverride = HPSDRModel::ANVELINAPRO3;
    return info;
}

// A Core-side Anvelina PRO III that owns `conn` and names its radio.
void prepareCore(RadioModel& model, RadioConnection* conn, const QString& mac = kMac)
{
    model.setBoardForTest(HPSDRHW::OrionMKII);
    model.setHpsdrModelForTest(HPSDRModel::ANVELINAPRO3);
    model.setLastRadioInfoForTest(anvelina(mac));
    model.setConnectionStateForTest(ConnectionState::Connected);
    model.injectConnectionForTest(conn);
}

QCheckBox* bpf2Box(QWidget& page)
{
    return page.findChild<QCheckBox*>(QStringLiteral("alexBpf2GndOnTx"));
}

QString savedValue(const QString& mac)
{
    return AppSettings::instance()
        .hardwareValue(mac, QString::fromLatin1(alexKeys::kBpf2GndOnTx))
        .toString();
}

} // namespace

class TestBpf2GndOnTx : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        if (!qApp) {
            static int argc = 0;
            new QApplication(argc, nullptr);
        }
    }
    void init()
    {
        AppSettings::instance().clearHardwareValues(kMac);
        AppSettings::instance().clearHardwareValues(kOtherMac);
    }
    void cleanup()
    {
        AppSettings::instance().clearHardwareValues(kMac);
        AppSettings::instance().clearHardwareValues(kOtherMac);
    }

    // ── The wire ─────────────────────────────────────────────────────────
    void wire_bit8FollowsTheBoxWhileKeyed_data()
    {
        QTest::addColumn<int>("board");
        QTest::newRow("OrionMKII") << int(HPSDRHW::OrionMKII);
        QTest::newRow("Saturn") << int(HPSDRHW::Saturn);
    }
    void wire_bit8FollowsTheBoxWhileKeyed()
    {
        QFETCH(int, board);
        ConnectedP2 conn;
        conn.setBoardForTest(static_cast<HPSDRHW>(board));
        conn.setReceiverFrequency(0, k40mHz);

        // Default unticked: clear keyed and unkeyed (the behavior change
        // from the old hard-coded set).
        QVERIFY(!conn.bpf2GndOnTx());
        QVERIFY(!bpf2Grounded(conn));
        conn.setMox(true);
        QVERIFY(!bpf2Grounded(conn));
        conn.setMox(false);

        // Ticked: unkeyed stays clear, keyed sets bit 8.
        conn.setBpf2GndOnTx(true);
        QVERIFY(!bpf2Grounded(conn));
        conn.setMox(true);
        QVERIFY(bpf2Grounded(conn));
        conn.setMox(false);
        QVERIFY(!bpf2Grounded(conn));
    }

    // Thetis reads bpf2_gnd at the TX edge only: a change while keyed waits
    // for the next key.
    void wire_takenAtTheTxEdge()
    {
        ConnectedP2 conn;
        conn.setBoardForTest(HPSDRHW::OrionMKII);
        conn.setReceiverFrequency(0, k40mHz);

        conn.setMox(true);
        conn.setBpf2GndOnTx(true);
        QVERIFY(!bpf2Grounded(conn));
        conn.setMox(false);
        conn.setMox(true);
        QVERIFY(bpf2Grounded(conn));
        conn.setBpf2GndOnTx(false);
        QVERIFY(bpf2Grounded(conn));
        conn.setMox(false);
        QVERIFY(!bpf2Grounded(conn));
        conn.setMox(true);
        QVERIFY(!bpf2Grounded(conn));
        conn.setMox(false);
    }

    // ── Which models show the box ────────────────────────────────────────
    void shownWhereThetisShowsIt_data()
    {
        QTest::addColumn<int>("model");
        QTest::addColumn<bool>("shown");
        QTest::newRow("ANAN8000D") << int(HPSDRModel::ANAN8000D) << true;
        QTest::newRow("ANAN7000D") << int(HPSDRModel::ANAN7000D) << true;
        QTest::newRow("ANAN_G2") << int(HPSDRModel::ANAN_G2) << true;
        QTest::newRow("ANAN_G2_1K") << int(HPSDRModel::ANAN_G2_1K) << true;
        QTest::newRow("ANVELINAPRO3") << int(HPSDRModel::ANVELINAPRO3) << true;
        QTest::newRow("REDPITAYA") << int(HPSDRModel::REDPITAYA) << true;
        QTest::newRow("ANAN_G2E") << int(HPSDRModel::ANAN_G2E) << false;
        QTest::newRow("ORIONMKII") << int(HPSDRModel::ORIONMKII) << false;
        QTest::newRow("ANAN200D") << int(HPSDRModel::ANAN200D) << false;
        QTest::newRow("ANAN100D") << int(HPSDRModel::ANAN100D) << false;
        QTest::newRow("HERMES") << int(HPSDRModel::HERMES) << false;
        QTest::newRow("ANAN10E") << int(HPSDRModel::ANAN10E) << false;
    }
    void shownWhereThetisShowsIt()
    {
        QFETCH(int, model);
        QFETCH(bool, shown);
        const auto sku = static_cast<HPSDRModel>(model);
        QCOMPARE(skuUiProfileFor(sku).hasBpf2GndOnTx, shown);

        RadioModel radio;
        radio.setHpsdrModelForTest(sku);
        AntennaAlexTab tab(&radio);
        QCheckBox* box = bpf2Box(tab);
        QVERIFY(box != nullptr);
        QCOMPARE(!box->isHidden(), shown);
        QCOMPARE(box->text(), QStringLiteral("Gnd BPF2 on Tx"));
        QCOMPARE(box->toolTip(), QStringLiteral("Ground BPF2 input on Tx"));
        QVERIFY(!box->isChecked());
    }

    // ── The box: per MAC, default unticked, reaches the radio ────────────
    void box_persistsPerMacAndReachesTheRadio()
    {
        ConnectedP2 conn;
        conn.setBoardForTest(HPSDRHW::OrionMKII);
        conn.setReceiverFrequency(0, k40mHz);
        RadioModel model;
        prepareCore(model, &conn);

        {
            AntennaAlexTab tab(&model);
            tab.populate(anvelina(kMac), *model.hardwareProfile().caps);
            QCheckBox* box = bpf2Box(tab);
            QVERIFY(box != nullptr);
            QVERIFY(!box->isHidden());
            QVERIFY(box->isEnabled());
            QVERIFY(!box->isChecked());
            QVERIFY(savedValue(kMac).isEmpty());   // nothing written until changed

            box->click();
            QCOMPARE(savedValue(kMac), QStringLiteral("True"));
            QVERIFY(savedValue(kOtherMac).isEmpty());
            QVERIFY(conn.bpf2GndOnTx());
            conn.setMox(true);
            QVERIFY(bpf2Grounded(conn));
            conn.setMox(false);
            QVERIFY(!bpf2Grounded(conn));
        }

        // A new tab restores it for this radio, and not for another.
        {
            AntennaAlexTab tab(&model);
            tab.populate(anvelina(kMac), *model.hardwareProfile().caps);
            QVERIFY(bpf2Box(tab)->isChecked());
            tab.populate(anvelina(kOtherMac), *model.hardwareProfile().caps);
            QVERIFY(!bpf2Box(tab)->isChecked());
            tab.populate(anvelina(kMac), *model.hardwareProfile().caps);
            QVERIFY(bpf2Box(tab)->isChecked());

            bpf2Box(tab)->click();
            QCOMPARE(savedValue(kMac), QStringLiteral("False"));
            QVERIFY(!conn.bpf2GndOnTx());
            conn.setMox(true);
            QVERIFY(!bpf2Grounded(conn));
            conn.setMox(false);
        }

        model.injectConnectionForTest(nullptr);
    }

    // The connect path's apply sends the saved value.
    void savedValue_isApplied()
    {
        AppSettings::instance().setHardwareValue(
            kMac, QString::fromLatin1(alexKeys::kBpf2GndOnTx), QStringLiteral("True"));
        ConnectedP2 conn;
        conn.setBoardForTest(HPSDRHW::OrionMKII);
        RadioModel model;
        prepareCore(model, &conn);
        model.applyAlexHpfSwitchSettings();
        conn.setMox(true);
        QVERIFY(bpf2Grounded(conn));
        conn.setMox(false);
        model.injectConnectionForTest(nullptr);
    }

    // ── A remote window's write reaches the Core ─────────────────────────
    //
    // The window's write lands in the Core's settings through StationServer,
    // which then calls scheduleRemoteHardwareApply with the key.
    void remoteWindowWrite_reachesTheCore()
    {
        ConnectedP2 conn;
        conn.setBoardForTest(HPSDRHW::OrionMKII);
        RadioModel core;
        prepareCore(core, &conn);
        QStringList reloads;
        core.setHardwareApplyObserverForTest([&reloads](const QString& name) { reloads << name; });

        const QString key = QStringLiteral("hardware/%1/%2")
                                .arg(kMac, QString::fromLatin1(alexKeys::kBpf2GndOnTx));
        AppSettings::instance().setValue(key, QStringLiteral("True"));
        core.scheduleRemoteHardwareApply(key);
        QTRY_COMPARE(reloads, QStringList{QStringLiteral("alex")});
        conn.setMox(true);
        QVERIFY(bpf2Grounded(conn));
        conn.setMox(false);

        reloads.clear();
        AppSettings::instance().setValue(key, QStringLiteral("False"));
        core.scheduleRemoteHardwareApply(key);
        QTRY_COMPARE(reloads, QStringList{QStringLiteral("alex")});
        conn.setMox(true);
        QVERIFY(!bpf2Grounded(conn));
        conn.setMox(false);

        core.injectConnectionForTest(nullptr);
    }

    // A transmit hardware key: never on the receive-only off-air list
    // (a receive-only Core takes it from a peer offered version 14).
    void isATransmitHardwareKey()
    {
        const QString key = QStringLiteral("hardware/%1/%2")
                                .arg(kMac, QString::fromLatin1(alexKeys::kBpf2GndOnTx));
        QVERIFY(!StationServer::isTransmitSettingKeyAcceptedOffAir(key));
    }

    // ── A remote window on an older Core ─────────────────────────────────
    //
    // A Core below radioHardwareVersion 14 stores the key without effect, so
    // the box is shown disabled with its reason, never hidden.
    void olderCoreKeepsTheBoxClosedWithItsReason()
    {
        RadioModel remote(RadioModel::Role::Remote);
        remote.setHpsdrModelForTest(HPSDRModel::ANVELINAPRO3);
        remote.alexAntennaFacade()->setWindowAvailability(true, {});
        HardwarePage page(&remote);
        QCheckBox* box = bpf2Box(page);
        QVERIFY(box != nullptr);
        QVERIFY(!box->isHidden());
        QVERIFY(box->testAttribute(Qt::WA_Disabled));   // its own gate, not a parent's
        QCOMPARE(box->toolTip(), IStationLink::bpf2GndOnTxUnavailableReason());
    }

    // A local window's box is always live, with Thetis's tooltip.
    void localWindowBoxIsLive()
    {
        RadioModel local;
        local.setHpsdrModelForTest(HPSDRModel::ANVELINAPRO3);
        HardwarePage page(&local);
        QCheckBox* box = bpf2Box(page);
        QVERIFY(box != nullptr);
        QVERIFY(!box->testAttribute(Qt::WA_Disabled));
        QCOMPARE(box->toolTip(), QStringLiteral("Ground BPF2 input on Tx"));
    }
};

QTEST_APPLESS_MAIN(TestBpf2GndOnTx)
#include "tst_bpf2_gnd_on_tx.moc"
