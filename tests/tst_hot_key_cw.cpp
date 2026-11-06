// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-05 - J.J. Boyd (KG4VCF), hot keys plan Task 5.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original test.
// CW messages and the comma, period and slash keys on real local, hosting and
// remote windows. A fake connection's key line stands in for the radio.
#include <QtTest/QtTest>

#include <QLoggingCategory>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QSslSocket>
#include <QTemporaryDir>

#include <memory>

#include "UnusedLoopbackPort.h"
#include "core/AppSettings.h"
#include "core/CwKeyLine.h"
#include "core/CwSoftPaddle.h"
#include "core/CwTypes.h"
#include "core/CwxSender.h"
#include "core/MoxController.h"
#include "core/RadioConnection.h"
#include "core/safety/TransmitHolder.h"
#include "core/safety/TxRefusal.h"
#include "core/session/DeviceSessionRegistry.h"
#include "core/session/RemoteTransmitClient.h"
#include "core/session/SliceAccessMirror.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/SliceOwnership.h"
#include "fakes/RemoteWindowHarness.h"
#include "gui/DesktopStationController.h"
#include "gui/MainWindow.h"
#include "gui/hotkeys/HotKeyDispatcher.h"
#include "gui/hotkeys/MainWindowHotKeyTargets.h"
#include "gui/widgets/StatusToast.h"
#include "models/CwKeyerController.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;
using NereusSDR::Test::RemoteWindowHarness;

namespace {

const HotKeySource kKeyboard{HotKeySourceKind::Keyboard, QString()};
const QString kDot = QStringLiteral("cw.dot");
const QString kDash = QStringLiteral("cw.dash");
const QString kStraight = QStringLiteral("cw.straight");
const QString kMsg1 = QStringLiteral("cw.msg1");
const QString kMsg2 = QStringLiteral("cw.msg2");
const QString kStop = QStringLiteral("cw.stop");
const QByteArray kPhoneDevice = QByteArrayLiteral("token:phone");

class FakeConnection : public RadioConnection {
    Q_OBJECT
public:
    explicit FakeConnection(QObject* parent = nullptr)
        : RadioConnection(parent)
    {
        setState(ConnectionState::Connected);
    }

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
    void sendTxIq(const float*, int) override {}
    void setWatchdogEnabled(bool) override {}
    void setAntennaRouting(AntennaRouting) override {}
    void setMox(bool) override {}
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

RadioInfo saturn()
{
    RadioInfo info;
    info.protocol = ProtocolVersion::Protocol2;
    info.boardType = HPSDRHW::Saturn;
    info.firmwareVersion = 10;
    info.macAddress = QStringLiteral("00:1c:c0:a2:13:dd");
    return info;
}

void pump(int passes = 6)
{
    for (int i = 0; i < passes; ++i) {
        QCoreApplication::processEvents();
    }
}

bool toastShown(MainWindow& window, const QString& message)
{
    for (StatusToast* toast : window.findChildren<StatusToast*>()) {
        if (toast->message() == message) {
            return true;
        }
    }
    return false;
}

int toastCount(MainWindow& window)
{
    return int(window.findChildren<StatusToast*>().size());
}

// Connects a model to a fake radio with one 20 m CW slice, break-in Semi.
SliceModel* tuneForCw(RadioModel* model, RadioConnection* connection)
{
    model->setCapsForTest(/*hasAlex=*/false);
    model->setBoardForTest(HPSDRHW::Saturn);
    model->injectConnectionForTest(connection);
    model->moxController()->setTimerIntervals(0, 0, 0, 0, 0, 0);
    SliceModel* const slice = model->sliceById(model->addSlice(QStringLiteral("pan-0")));
    slice->setFrequency(14'050'000.0);
    slice->setDspMode(DSPMode::CWU);
    model->transmitModel().setCwBreakInMode(static_cast<int>(CwBreakInMode::Semi));
    model->setLastRadioInfoForTest(saturn());
    model->wireCwKeyerConnectionForTest();
    pump();
    return slice;
}

struct LocalWindow {
    FakeConnection connection;
    MainWindow window{RemoteStationOptions{}, nullptr, MainWindow::ConnectionStartup::Deferred};
    RadioModel* model{window.radioModel()};
    MoxController* mox{model->moxController()};
    HotKeyDispatcher* keys{window.hotKeyDispatcher()};
    MainWindowHotKeyTargets targets{&window};
    SliceModel* slice{tuneForCw(model, &connection)};
    std::shared_ptr<CwKeyLine> line{connection.cwKeyLine()};

    ~LocalWindow() { model->injectConnectionForTest(nullptr); }

    void press(const QString& id) { keys->press(id, kKeyboard); }
    void release(const QString& id) { keys->release(id, kKeyboard); }
    int bits() const { return line->p1Bits(true); }
    // The radio says everything is up, which clears a stop's latch.
    void radioAllUp() { model->cwKeyerController()->onRadioKeyStatus(false, false, false); }
};

struct HostingWindow {
    QTemporaryDir directory;
    AppSettings settings{directory.filePath(QStringLiteral("station.settings"))};
    FakeConnection connection;
    MainWindow window{RemoteStationOptions{}, nullptr, MainWindow::ConnectionStartup::Deferred};
    RadioModel* model{window.radioModel()};
    HotKeyDispatcher* keys{window.hotKeyDispatcher()};
    SliceModel* slice{tuneForCw(model, &connection)};
    std::shared_ptr<CwKeyLine> line{connection.cwKeyLine()};
    std::unique_ptr<DesktopStationController> controller;
    QObject phoneSession;

    bool start()
    {
        StationHostOptions options;
        options.settings = &settings;
        options.securityDirectory = directory.path();
        options.hostingDevice = StationHostOptions::HostingDevice{
            QStringLiteral("Shack Mac mini"), QStringLiteral("Shack")};
        options.remoteBind = QStringLiteral("127.0.0.1");
        options.statusPage = false;
        options.remotePort = unusedLoopbackPort();
        controller = std::make_unique<DesktopStationController>(model, options);
        window.setDesktopStationController(controller.get());
        return controller->start(true);
    }

    bool phoneHolds()
    {
        DeviceSessionRegistry::Entry peer;
        peer.deviceId = kPhoneDevice;
        peer.kind = DeviceSessionRegistry::Kind::Token;
        peer.name = QStringLiteral("Phone");
        peer.shortName = QStringLiteral("Phone");
        peer.deviceKind = QStringLiteral("phone");
        if (controller->server()->deviceSessions()->admit(peer, &phoneSession).admission
            != DeviceSessionRegistry::Admission::Admitted) {
            return false;
        }
        TransmitHolder::KeyRequest key;
        key.deviceId = peer.deviceId;
        return controller->server()->transmitHolder()->askKey(key).verdict == KeyingVerdict::Admit;
    }

    ~HostingWindow()
    {
        if (controller) {
            controller->stop();
        }
        model->injectConnectionForTest(nullptr);
    }
};

// As tst_hot_key_transmit: the window shares slices and controls each one.
bool connectSharing(RemoteWindowHarness& h)
{
    h.startStartupConnection();
    StationClient* const client = h.client();
    if (client == nullptr
        || !QTest::qWaitFor([client] { return client->stationLinkReady(); }, 10000)
        || !client->remoteSliceAccessAvailable()) {
        return false;
    }
    return QTest::qWaitFor([&h, client] {
        const QList<int> live = h.station().sliceOwnership()->liveSlices();
        for (const int id : live) {
            const auto entry = client->sliceAccess()->entry(id);
            if (!entry || entry->controllerDeviceId != QStringLiteral("token:1")) {
                return false;
            }
        }
        return !live.isEmpty();
    }, 10000);
}

// A remote window allowed to transmit, its Core's slice in `mode` and the mode
// on the window's copy too. Nobody holds transmit yet, so no slice is marked for
// it here: the window goes by its active slice, as the TX applet does.
bool connectRemote(RemoteWindowHarness& h, DSPMode mode)
{
    if (!h.start()) {
        return false;
    }
    h.server().setRemoteTransmitAllowed(true);
    h.server().setTokenSessionsMayTransmitForTest(true);
    if (!connectSharing(h)) {
        return false;
    }
    StationCapabilities granted = h.server().buildCapabilities();
    granted.txPermitted = true;
    h.pushCapabilities(granted);
    StationClient* const client = h.client();
    if (!QTest::qWaitFor([client] { return client->capabilities().txPermitted; }, 10000)) {
        return false;
    }
    RadioModel* const remote = h.window()->radioModel();
    const auto slices = h.station().slices();
    if (slices.isEmpty()) {
        return false;
    }
    slices.first()->setDspMode(mode);
    return QTest::qWaitFor([remote, mode] {
        const SliceModel* const active = remote->activeSlice();
        return active != nullptr && active->dspMode() == mode;
    }, 10000);
}

} // namespace

class TestHotKeyCw final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QLoggingCategory::setFilterRules(QStringLiteral("nereus.*.debug=false"));
        QVERIFY(RemoteWindowHarness::useIsolatedProfile(QStringLiteral("hot-key-cw")));
        qRegisterMetaType<NereusSDR::TxRefusal>();
    }

    void init()
    {
        QVERIFY(RemoteWindowHarness::clearIsolatedProfile());
    }

    void cleanupTestCase()
    {
        QVERIFY(RemoteWindowHarness::removeIsolatedProfile());
    }

    // ---- Local keys in CW ----

    void commaAndPeriodDriveTheSoftBits()
    {
        LocalWindow w;
        QVERIFY2(w.model->cwKeyerController()->keyerAllowed(),
                 qPrintable(w.model->cwKeyerController()->refusal().text));
        // Connecting never keys.
        QCOMPARE(w.bits(), 0);

        w.press(kDot);
        QVERIFY(w.line->dot());
        QVERIFY(!w.line->dash());
        w.press(kDash);
        QVERIFY(w.line->dash());
        w.release(kDot);
        QVERIFY(!w.line->dot());
        QVERIFY(w.line->dash());
        w.release(kDash);
        QCOMPARE(w.bits(), 0);
    }

    void aSerialPaddleSurvivesTheKey()
    {
        LocalWindow w;
        w.line->setDot(true);
        w.press(kDot);
        w.release(kDot);
        QVERIFY(w.line->dot());
        w.line->setDot(false);
        QCOMPARE(w.bits(), 0);
    }

    void slashIsTheHandKey()
    {
        LocalWindow w;
        w.press(kStraight);
        QVERIFY(w.line->cwx());
        w.release(kStraight);
        QVERIFY(!w.line->cwx());
        // Nothing pressed: a release leaves the serial reader's hand key alone.
        QVERIFY(w.model->cwxSender()->setHandKey(true));
        w.targets.setCwKeyDown(kStraight, false);
        QVERIFY(w.line->cwx());
        w.model->cwxSender()->setHandKey(false);
    }

    void losingTheKeyboardLetsGo()
    {
        LocalWindow w;
        w.press(kDot);
        w.press(kDash);
        w.press(kStraight);
        QVERIFY(w.line->dot() && w.line->dash() && w.line->cwx());
        w.keys->releaseAll(kKeyboard);
        QCOMPARE(w.bits(), 0);
    }

    // ---- A voice mode ----

    void inAVoiceModeTheKeysDoNothingAndSayNothing()
    {
        LocalWindow w;
        w.slice->setDspMode(DSPMode::USB);
        pump();
        QVERIFY(!w.model->cwKeyerController()->keyerAllowed());
        QSignalSpy rejected(w.mox, &MoxController::moxRejected);
        const int toasts = toastCount(w.window);

        for (const QString& id : {kDot, kDash, kStraight}) {
            QVERIFY(w.targets.unavailableReason(id).isEmpty());
            w.press(id);
            QCOMPARE(w.bits(), 0);
            w.release(id);
            QCOMPARE(w.bits(), 0);
        }
        QCOMPARE(rejected.size(), 0);
        QCOMPARE(toastCount(w.window), toasts);

        // A key held across the change to CW still needs a fresh press.
        w.press(kDot);
        w.press(kStraight);
        w.slice->setDspMode(DSPMode::CWU);
        pump();
        QVERIFY(w.model->cwKeyerController()->keyerAllowed());
        w.press(kDot);
        w.press(kStraight);
        QCOMPARE(w.bits(), 0);
        w.release(kDot);
        w.release(kStraight);
        w.press(kDot);
        QVERIFY(w.line->dot());
        w.release(kDot);
    }

    void leavingCwReleasesWhatIsDown()
    {
        LocalWindow w;
        w.press(kDot);
        w.press(kStraight);
        QVERIFY(w.line->dot() && w.line->cwx());
        w.slice->setDspMode(DSPMode::USB);
        pump();
        QCOMPARE(w.bits(), 0);
        w.release(kDot);
        w.release(kStraight);
        QCOMPARE(w.bits(), 0);
    }

    // ---- The keyer gate ----

    void aShutGateRefusesOncePerPressThenAFreshPressKeys_data()
    {
        QTest::addColumn<QString>("id");
        QTest::newRow("dot") << kDot;
        QTest::newRow("dash") << kDash;
        QTest::newRow("straight") << kStraight;
    }
    void aShutGateRefusesOncePerPressThenAFreshPressKeys()
    {
        QFETCH(QString, id);
        LocalWindow w;
        // Off the band plan: CW mode, gate shut.
        w.slice->setFrequency(14'400'000.0);
        pump();
        CwKeyerController* const keyer = w.model->cwKeyerController();
        QVERIFY(!keyer->keyerAllowed());
        const QString words = keyer->refusal().text;
        QVERIFY(!words.isEmpty());
        QSignalSpy rejected(w.mox, &MoxController::moxRejected);

        w.press(id);
        QCOMPARE(w.bits(), 0);
        QCOMPARE(rejected.size(), 1);
        QCOMPARE(rejected.first().first().toString(), words);
        QVERIFY(toastShown(w.window, words));
        w.press(id);    // key repeat
        QCOMPARE(rejected.size(), 1);

        // The gate opens under the held key: nothing until it is pressed again.
        w.slice->setFrequency(14'050'000.0);
        pump();
        QVERIFY(keyer->keyerAllowed());
        w.press(id);
        QCOMPARE(w.bits(), 0);
        w.release(id);
        w.press(id);
        QVERIFY(w.bits() != 0);
        w.release(id);
        QCOMPARE(w.bits(), 0);
    }

    void theGateGoingFalseReleasesTheKeysAndSaysSo()
    {
        LocalWindow w;
        QSignalSpy rejected(w.mox, &MoxController::moxRejected);
        w.press(kDot);
        w.press(kDash);
        QVERIFY(w.line->dot() && w.line->dash());
        w.slice->setFrequency(14'400'000.0);
        pump();
        QCOMPARE(w.bits(), 0);
        QCOMPARE(rejected.size(), 1);
        w.slice->setFrequency(14'050'000.0);
        pump();
        QCOMPARE(w.bits(), 0);
    }

    // ---- The stops ----

    void aStopReleasesTheKeys()
    {
        LocalWindow w;
        w.press(kDot);
        w.press(kStraight);
        QVERIFY(w.line->dot() && w.line->cwx());
        QVERIFY(w.model->cwSoftPaddle()->isKeying());
        w.model->stopAllTx(QStringLiteral("Stopped."));
        QCOMPARE(w.bits(), 0);
        QVERIFY(!w.model->cwSoftPaddle()->isKeying());

        // Still held, the latch cleared: nothing until pressed again.
        w.radioAllUp();
        pump();
        w.model->cwSoftPaddle()->setDot(true);
        QCOMPARE(w.bits(), 0);
        w.release(kDot);
        w.release(kStraight);
        w.press(kDot);
        QVERIFY(w.line->dot());
        w.release(kDot);
    }

    void aStopWithNothingOnTheAirStillReleases()
    {
        LocalWindow w;
        // Off with no MOX: stopAllTx takes its quiet path.
        w.model->transmitModel().setCwBreakInMode(static_cast<int>(CwBreakInMode::Off));
        pump();
        w.press(kDash);
        QVERIFY(w.line->dash());
        w.model->stopAllTx(QStringLiteral("Stopped."));
        QVERIFY(!w.line->dash());
        w.release(kDash);
    }

    void aTransmitCutsCatchUpReleases()
    {
        LocalWindow w;
        w.press(kDash);
        QVERIFY(w.line->dash());
        w.model->catchUpTransmitCut(QStringLiteral("The link went quiet."));
        QVERIFY(!w.line->dash());
        w.release(kDash);
    }

    void theConnectionGoingAwayLetsGoOfItsLine()
    {
        LocalWindow w;
        w.press(kDot);
        QVERIFY(w.line->dot());
        w.model->injectConnectionForTest(nullptr);
        QVERIFY(!w.line->dot());
        w.release(kDot);
        w.press(kDot);
        QVERIFY(!w.line->dot());
        w.release(kDot);
    }

    // ---- Messages ----

    void messagesGoToTheSenderAndStopStopsThem()
    {
        LocalWindow w;
        w.model->transmitModel().setCwxMemory1(QStringLiteral("CQ CQ CQ"));
        pump();
        CwxSender* const sender = w.model->cwxSender();
        w.press(kMsg1);
        QTRY_VERIFY(sender->isSending());
        w.press(kStop);
        QTRY_VERIFY(!sender->isSending());
        QCOMPARE(w.line->cwx(), false);
    }

    void aRefusedMessageReachesTheToast()
    {
        LocalWindow w;
        w.press(kMsg2);
        QVERIFY(toastShown(w.window, QString::fromLatin1(kCwxMemoryEmptyText).arg(2)));
        QVERIFY(!w.model->cwxSender()->isSending());

        w.slice->setDspMode(DSPMode::USB);
        pump();
        w.model->transmitModel().setCwxMemory1(QStringLiteral("CQ"));
        pump();
        w.press(kMsg1);
        QVERIFY(toastShown(w.window, QString::fromLatin1(kCwxNotCwText)));
        QVERIFY(!w.model->cwxSender()->isSending());
    }

    void onlyAKeysOwnRefusalIsToasted()
    {
        LocalWindow w;
        const int toasts = toastCount(w.window);
        QVERIFY(!w.model->cwxSender()->sendMemory(3));   // the applet's, say
        QCOMPARE(toastCount(w.window), toasts);
    }

    // ---- Hosting, another device on transmit ----

    void hostingAllThreeRefuseWithTheHoldersReason_data()
    {
        QTest::addColumn<QString>("id");
        QTest::newRow("dot") << kDot;
        QTest::newRow("dash") << kDash;
        QTest::newRow("straight") << kStraight;
    }
    void hostingAllThreeRefuseWithTheHoldersReason()
    {
        if (!QSslSocket::supportsSsl()) { QSKIP("Qt reports no working TLS backend."); }
        QFETCH(QString, id);
        HostingWindow w;
        QVERIFY(w.directory.isValid());
        QVERIFY(w.start());
        QVERIFY(w.phoneHolds());

        w.keys->press(id, kKeyboard);
        QCOMPARE(int(w.line->p1Bits(true)), 0);
        QVERIFY(toastShown(w.window, TxRefusals::otherDeviceHolds(QStringLiteral("Phone")).text));
        w.keys->release(id, kKeyboard);
        QCOMPARE(int(w.line->p1Bits(true)), 0);
    }

    void hostingInAVoiceModeStaysQuiet()
    {
        if (!QSslSocket::supportsSsl()) { QSKIP("Qt reports no working TLS backend."); }
        HostingWindow w;
        QVERIFY(w.start());
        QVERIFY(w.phoneHolds());
        w.slice->setDspMode(DSPMode::USB);
        pump();
        const int toasts = toastCount(w.window);
        for (const QString& id : {kDot, kDash, kStraight}) {
            w.keys->press(id, kKeyboard);
            w.keys->release(id, kKeyboard);
        }
        QCOMPARE(toastCount(w.window), toasts);
    }

    void anotherDeviceTakingTransmitReleasesTheKeys()
    {
        if (!QSslSocket::supportsSsl()) { QSKIP("Qt reports no working TLS backend."); }
        HostingWindow w;
        QVERIFY(w.start());
        w.keys->press(kDot, kKeyboard);
        QVERIFY2(w.line->dot(), qPrintable(w.model->cwKeyerController()->refusal().text));
        w.keys->press(kStraight, kKeyboard);
        QVERIFY(w.line->cwx());
        QVERIFY(w.phoneHolds());
        QTRY_VERIFY(!w.line->dot());
        QTRY_VERIFY(!w.line->cwx());
        w.keys->release(kDot, kKeyboard);
        w.keys->release(kStraight, kKeyboard);
        QCOMPARE(int(w.line->p1Bits(true)), 0);
    }

    // ---- Remote ----

    void remoteDotAndDashNeedANewerCoreOnlyInCw()
    {
        RemoteWindowHarness::Options options;
        options.sliceAccess = true;
        RemoteWindowHarness h(options);
        QVERIFY(connectRemote(h, DSPMode::CWU));
        MainWindowHotKeyTargets targets(h.window());
        const QString reason = QStringLiteral("Dot and dash need a newer Core.");
        QCOMPARE(targets.unavailableReason(kDot), reason);
        QCOMPARE(targets.unavailableReason(kDash), reason);
        QVERIFY(targets.unavailableReason(kStraight).isEmpty());

        h.window()->hotKeyDispatcher()->press(kDot, kKeyboard);
        QVERIFY(toastShown(*h.window(), reason));
        h.window()->hotKeyDispatcher()->release(kDot, kKeyboard);

        h.station().slices().first()->setDspMode(DSPMode::USB);
        QTRY_COMPARE(h.window()->radioModel()->activeSlice()->dspMode(), DSPMode::USB);
        QVERIFY(targets.unavailableReason(kDot).isEmpty());
        QVERIFY(targets.unavailableReason(kDash).isEmpty());
    }

    void remoteStraightKeyAndMessagesReachTheCore()
    {
        RemoteWindowHarness::Options options;
        options.sliceAccess = true;
        RemoteWindowHarness h(options);
        QVERIFY(connectRemote(h, DSPMode::CWU));
        // The Core on a fake radio, its keyer gate open.
        FakeConnection radio;
        RadioModel& core = h.station();
        core.setCapsForTest(/*hasAlex=*/false);
        core.setBoardForTest(HPSDRHW::Saturn);
        core.injectConnectionForTest(&radio);
        const auto letGoOfRadio = qScopeGuard([&core] { core.injectConnectionForTest(nullptr); });
        core.slices().first()->setFrequency(14'050'000.0);
        core.transmitModel().setCwBreakInMode(static_cast<int>(CwBreakInMode::Semi));
        core.transmitModel().setCwxMemory1(QStringLiteral("CQ CQ CQ"));
        core.setLastRadioInfoForTest(saturn());
        core.wireCwKeyerConnectionForTest();
        pump();
        QVERIFY2(core.cwKeyerController()->keyerAllowed(),
                 qPrintable(core.cwKeyerController()->refusal().text));

        StationClient* const client = h.client();
        QTRY_VERIFY(client->remoteCwAvailable());
        RemoteTransmitClient* const remote = client->remoteTransmit();
        QTRY_VERIFY(remote->cwAvailable());
        HotKeyDispatcher* const keys = h.window()->hotKeyDispatcher();

        // Nobody holds transmit: the applet's reason, and nothing goes out.
        keys->press(kStraight, kKeyboard);
        QVERIFY(toastShown(*h.window(), client->otherHolderReason().isEmpty()
                                            ? TxRefusals::notHolder().text
                                            : client->otherHolderReason()));
        keys->release(kStraight, kKeyboard);

        QVERIFY(client->requestTakeTransmit(false, 0, false) != 0);
        QTRY_VERIFY(client->holdsTransmitHere());
        QSignalSpy refused(remote, &RemoteTransmitClient::cwRefused);
        CwxSender* const coreCwx = core.cwxSender();

        // A memory with nothing in it says so, which proves the number arrived.
        keys->press(QStringLiteral("cw.msg5"), kKeyboard);
        QTRY_VERIFY(!refused.isEmpty());
        QCOMPARE(refused.first().first().toString(), QString::fromLatin1(kCwxMemoryEmptyText).arg(5));
        refused.clear();

        keys->press(kMsg1, kKeyboard);
        QTRY_VERIFY(coreCwx->isSending());
        keys->press(kStop, kKeyboard);
        QTRY_VERIFY(!coreCwx->isSending());

        keys->press(kStraight, kKeyboard);
        QTRY_VERIFY(coreCwx->isKeyHeld());
        QVERIFY(refused.isEmpty());
        keys->release(kStraight, kKeyboard);
        QTRY_VERIFY(!coreCwx->isKeyHeld());
        QVERIFY(refused.isEmpty());
    }

    void remoteOnAVoiceModeTheKeysSendNothing()
    {
        RemoteWindowHarness::Options options;
        options.sliceAccess = true;
        RemoteWindowHarness h(options);
        QVERIFY(connectRemote(h, DSPMode::USB));
        const int toasts = toastCount(*h.window());
        HotKeyDispatcher* const keys = h.window()->hotKeyDispatcher();
        CwxSender* const coreCwx = h.station().cwxSender();
        for (const QString& id : {kDot, kDash, kStraight}) {
            keys->press(id, kKeyboard);
            keys->release(id, kKeyboard);
        }
        QVERIFY(!coreCwx->isKeyHeld());
        QCOMPARE(toastCount(*h.window()), toasts);
    }

    void remoteAnOlderCoreSaysSo()
    {
        RemoteWindowHarness::Options options;
        options.sliceAccess = true;
        RemoteWindowHarness h(options);
        QVERIFY(connectRemote(h, DSPMode::CWU));
        StationCapabilities older = h.server().buildCapabilities();
        older.txPermitted = true;
        older.remoteCwVersion = 0;
        h.pushCapabilities(older);
        QTRY_VERIFY(!h.client()->remoteCwAvailable());
        const QString reason = QStringLiteral("Sending CW from a remote window needs a newer Core.");
        HotKeyDispatcher* const keys = h.window()->hotKeyDispatcher();

        keys->press(kStraight, kKeyboard);
        QVERIFY(toastShown(*h.window(), reason));
        keys->release(kStraight, kKeyboard);
    }

    void remoteMessageWithAnOlderCoreSaysSo()
    {
        RemoteWindowHarness::Options options;
        options.sliceAccess = true;
        RemoteWindowHarness h(options);
        QVERIFY(connectRemote(h, DSPMode::CWU));
        StationCapabilities older = h.server().buildCapabilities();
        older.txPermitted = true;
        older.remoteCwVersion = 0;
        h.pushCapabilities(older);
        QTRY_VERIFY(!h.client()->remoteCwAvailable());
        HotKeyDispatcher* const keys = h.window()->hotKeyDispatcher();

        keys->press(kMsg1, kKeyboard);
        QVERIFY(toastShown(*h.window(), QStringLiteral("Sending CW from a remote window needs a newer Core.")));
        // Stop has no version 1 to go to here either.
        keys->press(kStop, kKeyboard);
        QVERIFY(toastShown(*h.window(), QStringLiteral("Sending CW from a remote window needs a newer Core.")));
    }
};

QTEST_MAIN(TestHotKeyCw)
#include "tst_hot_key_cw.moc"
