// no-port-check: NereusSDR-original test.
//
// tests/tst_remote_tx_antennas.cpp
//
// R-R3-49 / R-R3-46 (remote-window parity Task 12): the transmit antennas
// and relays from a remote window, as from a local one.
//
// Covered:
//   - Setup > Hardware Config > Antenna Control in a remote window (a real
//     MainWindow joined to an in-process Core): the TX antenna grid, Block
//     TX on Ant 2 and Ant 3, RX bypass on TX, Ext 1 and Ext 2 on TX and the
//     RX bypass relay override change the Core's AlexController and show
//     the Core's values; a change made on the Core shows in the window.
//   - The VFO flag's BYPS sets the Core's rxOutOnTx and its lit state
//     follows the Core's value.
//   - On the air (the Core's own MoxController keyed, the receive-only MOX
//     pre-check lifted) these stay live in both windows and still change
//     the radio, because Thetis has no on-air rule for them (its Setup
//     handlers apply at once with tx = _mox; see
//     StationServer::radioHardwareVersion).
//   - The routing the Core then sends its radio, and the P1 and P2 bytes it
//     becomes, match what a local window's same edit sends, on receive and
//     while transmitting.
//
// No hardware: the radios are fake connections that only record, and the
// P1 / P2 bytes are composed by the real connection classes without a
// socket. Nothing keys a real radio.
//
// Modification history (NereusSDR):
//   2026-09-25: created (R-R3-49, R-R3-46), by J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-25: checkpoint carry: the window signs in to an upgraded Core
//               with its token (seedUpgradedCoreToken), as Part C's
//               paired-device sign-in requires. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-26: transmit group fix wave 2 (R-IOS-02): while the Core's own
//               key is on the air, this window's transmit antenna changes
//               wait until it ends (ruling 7.4). J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.

#include <QtTest/QtTest>

#include <QAction>
#include <QCheckBox>
#include <QLoggingCategory>
#include <QPushButton>
#include <QRadioButton>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QStackedWidget>
#include <QTemporaryDir>
#include <QTreeWidget>

#include <array>
#include <cstring>
#include <functional>
#include <memory>

#include "core/AppSettings.h"
#include "core/MoxController.h"
#include "core/P1RadioConnection.h"
#include "core/P2RadioConnection.h"
#include "core/RadioConnection.h"
#include "core/StepAttenuatorController.h"
#include "core/accessories/AlexAntennaFacade.h"
#include "core/accessories/AlexController.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/settings/SettingsProxy.h"
#include "gui/MainWindow.h"
#include "gui/SetupDialog.h"
#include "gui/setup/HardwarePage.h"
#include "gui/setup/hardware/AntennaAlexAntennaControlTab.h"
#include "gui/widgets/VfoWidget.h"
#include "models/Band.h"
#include "models/RadioModel.h"

#include "fakes/LoopbackTransport.h"
#include "fakes/UpgradedCoreToken.h"
#include "fakes/RemoteWindowHarness.h"

using namespace NereusSDR;
using NereusSDR::Test::LoopbackTransport;
using NereusSDR::Test::RemoteWindowHarness;

namespace {

// The radio end: records every antenna routing the model sends it.
class RoutingRecorder final : public RadioConnection {
    Q_OBJECT
public:
    explicit RoutingRecorder(QObject* parent = nullptr)
        : RadioConnection(parent)
    {
        setState(ConnectionState::Connected);
    }

    QList<AntennaRouting> calls;

    void init() override {}
    void connectToRadio(const NereusSDR::RadioInfo&) override {}
    void disconnect() override {}
    void setReceiverFrequency(int, quint64) override {}
    void setTxFrequency(quint64) override {}
    void setActiveReceiverCount(int) override {}
    void setSampleRate(int) override {}
    void setAttenuator(int) override {}
    void setPreamp(bool) override {}
    void setTxDrive(int) override {}
    void setMox(bool) override {}
    void setAntennaRouting(AntennaRouting r) override { calls.append(r); }
    void setWatchdogEnabled(bool) override {}
    void sendTxIq(const float*, int) override {}
    void setTrxRelay(bool) override {}
    void setMicBoost(bool) override {}
    void setLineIn(bool) override {}
    void setMicTipRing(bool) override {}
    void setMicBias(bool) override {}
    void setLineInGain(int) override {}
    void setUserDigOut(quint8) override {}
    void setPuresignalRun(bool) override {}
    void setMicPTTDisabled(bool) override {}
    void setMicXlr(bool) override {}
};

bool sameRouting(const AntennaRouting& a, const AntennaRouting& b)
{
    return a.rxOnlyAnt == b.rxOnlyAnt && a.trxAnt == b.trxAnt && a.txAnt == b.txAnt
        && a.rxOut == b.rxOut && a.tx == b.tx;
}

QString describe(const AntennaRouting& r)
{
    return QStringLiteral("rxOnly=%1 trx=%2 tx=%3 rxOut=%4 mox=%5")
        .arg(r.rxOnlyAnt).arg(r.trxAnt).arg(r.txAnt).arg(r.rxOut).arg(r.tx);
}

// The P1 bank-0 C&C bytes (antenna, RX-only input, RX bypass out) an
// ANAN-100D would get for this routing.
QByteArray p1Bytes(const AntennaRouting& r)
{
    P1RadioConnection p1;
    p1.setBoardForTest(HPSDRHW::Angelia);
    p1.setAntennaRouting(r);
    return p1.captureBank0ForTest();
}

// The P2 high-priority packet (Alex0 and Alex1 words) a G2 would get.
QByteArray p2Bytes(const AntennaRouting& r)
{
    P2RadioConnection p2;
    p2.setBoardForTest(HPSDRHW::Saturn);
    p2.setAntennaRouting(r);
    // P2RadioConnection's kBufLen (Thetis BUFLEN), private to the class.
    constexpr std::size_t kP2BufLen = 1444;
    std::array<quint8, kP2BufLen> buf{};
    p2.composeCmdHighPriorityForTest(buf.data());
    return QByteArray(reinterpret_cast<const char*>(buf.data()), static_cast<int>(buf.size()));
}

// File > Settings..., then Hardware Config's Antenna Control tab.
AntennaAlexAntennaControlTab* openAntennaControl(RemoteWindowHarness& h)
{
    QAction* settings = h.menuAction(QStringLiteral("&File"), QStringLiteral("&Settings..."));
    if (!settings) { return nullptr; }
    settings->trigger();
    auto* dialog = h.window()->findChild<SetupDialog*>();
    auto* tree = dialog ? dialog->findChild<QTreeWidget*>() : nullptr;
    auto* stack = dialog ? dialog->findChild<QStackedWidget*>() : nullptr;
    if (!tree || !stack) { return nullptr; }
    const auto found = tree->findItems(QStringLiteral("Hardware Config"),
                                       Qt::MatchExactly | Qt::MatchRecursive);
    if (found.isEmpty()) { return nullptr; }
    tree->setCurrentItem(found.first());
    auto* hardware = qobject_cast<HardwarePage*>(stack->currentWidget());
    return hardware ? hardware->findChild<AntennaAlexAntennaControlTab*>() : nullptr;
}

} // namespace

class TstRemoteTxAntennas : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_securityDir;

private slots:
    void initTestCase()
    {
        QLoggingCategory::setFilterRules(QStringLiteral("nereus.*.debug=false"));
        QVERIFY(m_securityDir.isValid());
        QVERIFY(RemoteWindowHarness::useIsolatedProfile(QStringLiteral("remote-tx-antennas")));
    }

    void init() { QVERIFY(RemoteWindowHarness::clearIsolatedProfile()); }

    void cleanupTestCase() { QVERIFY(RemoteWindowHarness::removeIsolatedProfile()); }

    // B4.1, B2.2 and the on-air rule, through the operator's own controls.
    void remoteWindowChangesTheCoresTransmitAntennas()
    {
        StepAttenuatorController coreAtt;
        coreAtt.setTickTimerEnabled(false);
        RemoteWindowHarness h;
        h.station().setStepAttController(&coreAtt);
        const auto unbind = qScopeGuard([&h] { h.station().setStepAttController(nullptr); });
        RadioInfo radio;
        radio.macAddress = QStringLiteral("AA:BB:CC:DD:EE:12");
        radio.boardType = HPSDRHW::Saturn;
        h.station().setLastRadioInfoForTest(radio);
        QVERIFY(h.start());
        QAction* connect = h.menuAction(QStringLiteral("&Radio"), QStringLiteral("&Connect"));
        QVERIFY(connect && connect->isEnabled());
        connect->trigger();
        QTRY_VERIFY_WITH_TIMEOUT(h.client()->isHandshakeComplete(), 10000);
        QCOMPARE(h.client()->capabilities().radioHardwareVersion, 13);
        QVERIFY(h.client()->remoteTransmitAntennasAvailable());
        // The Core does not offer remote transmit; these do not need it.
        QVERIFY(!h.client()->capabilities().txPermitted);

        AlexController& core = h.station().alexControllerMutable();
        AntennaAlexAntennaControlTab* tab = openAntennaControl(h);
        QVERIFY(tab);
        QTRY_VERIFY(tab->txGridForTest()->isEnabled());
        QVERIFY(tab->blockTxAnt2ForTest()->isEnabled());
        QVERIFY(tab->rxOutOnTxForTest()->isEnabled());
        QVERIFY(tab->ext1OutOnTxForTest()->isEnabled());
        QVERIFY(tab->rxOutOverrideForTest()->isEnabled());

        // TX antenna grid.
        tab->txButtonForTest(Band::Band20m, 3)->click();
        QTRY_COMPARE(core.txAnt(Band::Band20m), 3);
        // The window shows the Core's value once its delta arrives.
        QTRY_VERIFY(tab->txButtonForTest(Band::Band20m, 3)->isChecked());
        // Parity mini-round: the grid sends only the band clicked
        // (setAlexTxAntenna), so a TX antenna the Core changed on another
        // band just before the click stays.
        core.setTxAnt(Band::Band40m, 2);
        tab->txButtonForTest(Band::Band80m, 3)->click();
        QTRY_COMPARE(core.txAnt(Band::Band80m), 3);
        QTest::qWait(100);
        QCOMPARE(core.txAnt(Band::Band40m), 2);
        QTRY_VERIFY(tab->txButtonForTest(Band::Band40m, 2)->isChecked());
        core.setTxAnt(Band::Band40m, 1);
        core.setTxAnt(Band::Band80m, 1);
        QTRY_VERIFY(tab->txButtonForTest(Band::Band80m, 1)->isChecked());
        // Block TX on Ant 3: the Core moves 20 m back to Ant 1, and the
        // window shows it with Ant 3 greyed.
        tab->blockTxAnt3ForTest()->click();
        QTRY_VERIFY(core.blockTxAnt3());
        QCOMPARE(core.txAnt(Band::Band20m), 1);
        QTRY_VERIFY(tab->txButtonForTest(Band::Band20m, 1)->isChecked());
        QVERIFY(!tab->txButtonForTest(Band::Band20m, 3)->isEnabled());
        tab->blockTxAnt3ForTest()->click();
        QTRY_VERIFY(!core.blockTxAnt3());
        tab->blockTxAnt2ForTest()->click();
        QTRY_VERIFY(core.blockTxAnt2());
        tab->blockTxAnt2ForTest()->click();
        QTRY_VERIFY(!core.blockTxAnt2());

        // The four TX relay switches; Ext 1 clears RX bypass on the Core, as
        // Thetis's chkEXT1OutOnTx does, and the window shows it.
        tab->rxOutOnTxForTest()->click();
        QTRY_VERIFY(core.rxOutOnTx());
        tab->ext1OutOnTxForTest()->click();
        QTRY_VERIFY(core.ext1OutOnTx());
        QVERIFY(!core.rxOutOnTx());
        QTRY_VERIFY(!tab->rxOutOnTxForTest()->isChecked());
        QVERIFY(tab->ext1OutOnTxForTest()->isChecked());
        tab->ext2OutOnTxForTest()->click();
        QTRY_VERIFY(core.ext2OutOnTx());
        QTRY_VERIFY(!tab->ext1OutOnTxForTest()->isChecked());
        tab->ext2OutOnTxForTest()->click();
        QTRY_VERIFY(!core.ext2OutOnTx());
        tab->rxOutOverrideForTest()->click();
        QTRY_VERIFY(core.rxOutOverride());
        tab->rxOutOverrideForTest()->click();
        QTRY_VERIFY(!core.rxOutOverride());

        // A change made on the Core shows in the window.
        core.setTxAnt(Band::Band40m, 2);
        QTRY_VERIFY(tab->txButtonForTest(Band::Band40m, 2)->isChecked());
        core.setRxOutOverride(true);
        QTRY_VERIFY(tab->rxOutOverrideForTest()->isChecked());
        core.setRxOutOverride(false);
        QTRY_VERIFY(!tab->rxOutOverrideForTest()->isChecked());

        // B2.2: the flag's BYPS sets the Core's value and lights from it.
        auto* bypass = h.window()->findChild<QPushButton*>(QStringLiteral("m_rxBypassBtn"));
        QVERIFY(bypass);
        QTRY_VERIFY(bypass->isEnabled());
        QVERIFY(!bypass->isChecked());
        bypass->click();
        QTRY_VERIFY(core.rxOutOnTx());
        core.setRxOutOnTx(false);
        QTRY_VERIFY(!bypass->isChecked());
        core.setRxOutOnTx(true);
        QTRY_VERIFY(bypass->isChecked());
        QTRY_VERIFY(tab->rxOutOnTxForTest()->isChecked());
        core.setRxOutOnTx(false);
        QTRY_VERIFY(!bypass->isChecked());

        // On the air (transmit group fix wave 2, the several-devices
        // design, ruling 7.4 and the controller's ruling): Thetis applies
        // these at once for the operator who is transmitting, so the holder
        // changes them on the air (tst_on_air_refusals); while the Core's
        // own key is on the air this window is another operator, and its
        // changes wait until the key ends.
        MoxController* const mox = h.station().moxController();
        QVERIFY(mox);
        mox->setMoxCheck({});
        mox->setMox(true);
        const auto unkey = qScopeGuard([mox] { mox->setMox(false); });
        QTRY_VERIFY(h.remoteModel()->isCoreOnAir());
        const int txAnt20 = core.txAnt(Band::Band20m);
        const int wanted20 = txAnt20 == 2 ? 3 : 2;
        tab->txButtonForTest(Band::Band20m, wanted20)->click();
        tab->ext2OutOnTxForTest()->click();
        bypass->click();
        QTest::qWait(300);
        QCOMPARE(core.txAnt(Band::Band20m), txAnt20);
        QVERIFY(!core.ext2OutOnTx());
        QVERIFY(!core.rxOutOnTx());
        mox->setMox(false);
        QTRY_VERIFY(!h.remoteModel()->isCoreOnAir());
        QTRY_VERIFY(!tab->ext2OutOnTxForTest()->isChecked());
        tab->txButtonForTest(Band::Band20m, wanted20)->click();
        QTRY_COMPARE(core.txAnt(Band::Band20m), wanted20);
        tab->ext2OutOnTxForTest()->click();
        QTRY_VERIFY(core.ext2OutOnTx());
    }

    // Parity the other way: a local window's Antenna Control stays live on
    // the air too, and writes this computer's controller.
    void localWindowTransmitAntennasStayLiveOnTheAir()
    {
        AppSettings::instance().setRemoteBackend(nullptr);
        RadioModel local;
        local.setBoardForTest(HPSDRHW::Saturn);
        local.configureStreamPool(5, 5, 192000);
        local.setConnectionStateForTest(ConnectionState::Connected);
        local.addSlice(QStringLiteral("pan-0"));
        AntennaAlexAntennaControlTab tab(&local);
        MoxController* const mox = local.moxController();
        QVERIFY(mox);
        mox->setMoxCheck({});
        mox->setMox(true);
        const auto unkey = qScopeGuard([mox] { mox->setMox(false); });
        QTRY_VERIFY(local.isCoreOnAir());
        QVERIFY(tab.txGridForTest()->isEnabled());
        QVERIFY(tab.blockTxAnt2ForTest()->isEnabled());
        QVERIFY(tab.rxOutOnTxForTest()->isEnabled());
        QVERIFY(tab.rxOutOverrideForTest()->isEnabled());
        tab.txButtonForTest(Band::Band20m, 2)->click();
        QCOMPARE(local.alexController().txAnt(Band::Band20m), 2);
        tab.ext1OutOnTxForTest()->click();
        QVERIFY(local.alexController().ext1OutOnTx());
        tab.blockTxAnt2ForTest()->click();
        QVERIFY(local.alexController().blockTxAnt2());
        QCOMPARE(local.alexController().txAnt(Band::Band20m), 1);
    }

    // What the Core sends its radio after a remote window's edit is what a
    // local window's same edit sends, routing and bytes, off and on the air.
    void coreSendsTheRadioWhatALocalWindowSends()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        AppSettings serverSettings(dir.filePath(QStringLiteral("core.settings")));

        // The Core, with a remote window joined over the loopback.
        RadioModel core;
        core.setCapsForTest(/*hasAlex=*/true);
        core.addSlice(QStringLiteral("pan-0"));
        StepAttenuatorController coreAtt;
        coreAtt.setTickTimerEnabled(false);
        core.setStepAttController(&coreAtt);
        auto* coreRadio = new RoutingRecorder();
        core.injectConnectionForTest(coreRadio);
        const auto coreDone = qScopeGuard([&core, coreRadio] {
            core.injectConnectionForTest(nullptr);
            core.setStepAttController(nullptr);
            delete coreRadio;
        });
        StationServer server(&core, serverSettings,
                             NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
        RadioModel window(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient client(&window, &proxy);
        auto* stationEnd = new LoopbackTransport(QStringLiteral("tx-ant-station"), this);
        auto* clientEnd = new LoopbackTransport(QStringLiteral("tx-ant-client"), this);
        stationEnd->linkTo(clientEnd);
        QSignalSpy completed(&client, &StationClient::handshakeComplete);
        client.startSession(clientEnd, server.token());
        server.acceptTransport(stationEnd);
        QTRY_COMPARE(completed.count(), 1);
        QVERIFY(client.remoteTransmitAntennasAvailable());
        AlexAntennaFacade* remote = window.alexAntennaFacade();

        // A local window's model, the same radio.
        RadioModel local;
        local.setCapsForTest(/*hasAlex=*/true);
        local.addSlice(QStringLiteral("pan-0"));
        auto* localRadio = new RoutingRecorder();
        local.injectConnectionForTest(localRadio);
        const auto localDone = qScopeGuard([&local, localRadio] {
            local.injectConnectionForTest(nullptr);
            delete localRadio;
        });
        AlexController& localAlex = local.alexControllerMutable();
        const Band band = core.lastBand();
        QCOMPARE(local.lastBand(), band);

        struct Step {
            const char* name;
            std::function<void()> remoteEdit;
            std::function<bool()> coreHasIt;
            std::function<void()> localEdit;
        };
        const QList<Step> steps{
            {"TX antenna 2",
             [&] { remote->setTxAnt(band, 2); },
             [&] { return core.alexController().txAnt(band) == 2; },
             [&] { localAlex.setTxAnt(band, 2); }},
            {"Use TX antenna for RX",
             [&] { remote->setUseTxAntennaForRx(true); },
             [&] { return core.alexController().useTxAntForRx(); },
             [&] { localAlex.setUseTxAntForRx(true); }},
            {"Ext 1 on TX",
             [&] { remote->setExt1OutOnTx(true); },
             [&] { return core.alexController().ext1OutOnTx(); },
             [&] { localAlex.setExt1OutOnTx(true); }},
            {"RX bypass relay override",
             [&] { remote->setRxOutOverride(true); },
             [&] { return core.alexController().rxOutOverride(); },
             [&] { localAlex.setRxOutOverride(true); }},
            {"Block TX on Ant 2",
             [&] { remote->setBlockTxAnt2(true); },
             [&] { return core.alexController().blockTxAnt2(); },
             [&] { localAlex.setBlockTxAnt2(true); }},
            {"key", [&] { core.onMoxHardwareFlipped(true); }, [] { return true; },
             [&] { local.onMoxHardwareFlipped(true); }},
            {"TX antenna 3 on the air",
             [&] { remote->setTxAnt(band, 3); },
             [&] { return core.alexController().txAnt(band) == 3; },
             [&] { localAlex.setTxAnt(band, 3); }},
            {"Ext 2 on TX on the air",
             [&] { remote->setExt2OutOnTx(true); },
             [&] { return core.alexController().ext2OutOnTx(); },
             [&] { localAlex.setExt2OutOnTx(true); }},
            {"unkey", [&] { core.onMoxHardwareFlipped(false); }, [] { return true; },
             [&] { local.onMoxHardwareFlipped(false); }},
            {"key again", [&] { core.onMoxHardwareFlipped(true); }, [] { return true; },
             [&] { local.onMoxHardwareFlipped(true); }},
            {"unkey again", [&] { core.onMoxHardwareFlipped(false); }, [] { return true; },
             [&] { local.onMoxHardwareFlipped(false); }},
        };
        bool sawTx = false;
        for (const Step& step : steps) {
            coreRadio->calls.clear();
            localRadio->calls.clear();
            step.remoteEdit();
            QTRY_VERIFY2(step.coreHasIt(), step.name);
            step.localEdit();
            QCOMPARE(coreRadio->calls.size(), localRadio->calls.size());
            if (localRadio->calls.isEmpty()) {
                continue;  // both hold it, for the next MOX edge
            }
            const AntennaRouting sent = coreRadio->calls.last();
            const AntennaRouting expected = localRadio->calls.last();
            QVERIFY2(sameRouting(sent, expected),
                     qPrintable(QStringLiteral("%1: Core %2, local %3")
                                    .arg(QLatin1String(step.name), describe(sent),
                                         describe(expected))));
            QCOMPARE(p1Bytes(sent), p1Bytes(expected));
            QCOMPARE(p2Bytes(sent), p2Bytes(expected));
            sawTx = sawTx || sent.tx;
        }
        QVERIFY(sawTx);
        // The last edge sent the new TX antenna and Ext 2 on the relays.
        QCOMPARE(core.alexController().txAnt(band), 3);
        QVERIFY(core.alexController().ext2OutOnTx());
        QVERIFY(!core.alexController().ext1OutOnTx());
        QVERIFY(core.alexController().blockTxAnt2());
    }
};

QTEST_MAIN(TstRemoteTxAntennas)
#include "tst_remote_tx_antennas.moc"
