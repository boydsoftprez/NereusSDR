// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-05 - J.J. Boyd (KG4VCF), hot keys plan Task 4.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original test.
// What keys the radio from a hot key: hold to talk, MOX, TUNE and two-tone on
// real local, hosting and remote windows. Nothing here keys a real radio.
#include <QtTest/QtTest>

#include <QApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QPointer>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QSslSocket>
#include <QTemporaryDir>

#include <memory>

#include "UnusedLoopbackPort.h"
#include "core/AppSettings.h"
#include "core/MoxController.h"
#include "core/RadioConnection.h"
#include "core/SliceOwnership.h"
#include "core/TwoToneController.h"
#include "core/TxChannel.h"
#include "core/safety/TransmitHolder.h"
#include "core/safety/TxRefusal.h"
#include "core/session/DeviceSessionRegistry.h"
#include "core/session/RemoteTransmitClient.h"
#include "core/session/SliceAccessController.h"
#include "core/session/SliceAccessMirror.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "fakes/RemoteAudioSessionHarness.h"
#include "fakes/RemoteWindowHarness.h"
#include "gui/DesktopStationController.h"
#include "gui/MainWindow.h"
#include "gui/hotkeys/HotKeyDispatcher.h"
#include "gui/hotkeys/MainWindowHotKeyTargets.h"
#include "gui/multidevice/TakeTransmitDialog.h"
#include "gui/widgets/StatusToast.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;
using NereusSDR::Test::RemoteWindowHarness;

namespace {

const HotKeySource kKeyboard{HotKeySourceKind::Keyboard, QString()};
const QString kHold = QStringLiteral("tx.hold");
const QString kMox = QStringLiteral("tx.mox");
const QString kTune = QStringLiteral("tx.tune");
const QString kTwoTone = QStringLiteral("tx.twotone");
const QByteArray kPhoneDevice = QByteArrayLiteral("token:phone");

class ToneRecordingTxChannel : public TxChannel {
public:
    ToneRecordingTxChannel() : TxChannel(1) {}
    void setTxPostGenMode(int) override {}
    void setTxPostGenTTFreq1(double) override {}
    void setTxPostGenTTFreq2(double) override {}
    void setTxPostGenTTMag1(double) override {}
    void setTxPostGenTTMag2(double) override {}
    void setTxPostGenTTPulseToneFreq1(double) override {}
    void setTxPostGenTTPulseToneFreq2(double) override {}
    void setTxPostGenTTPulseMag1(double) override {}
    void setTxPostGenTTPulseMag2(double) override {}
    void setTxPostGenTTPulseFreq(int) override {}
    void setTxPostGenTTPulseDutyCycle(double) override {}
    void setTxPostGenTTPulseTransition(double) override {}
    void setTxPostGenTTPulseIQOut(bool) override {}
    void setTxPostGenRun(bool) override {}
};

// A connection that counts what would key the radio.
class CountingConnection : public RadioConnection {
    Q_OBJECT
public:
    explicit CountingConnection(QObject* parent = nullptr)
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
    void setMox(bool on) override { moxCalls.append(on); }
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

    QList<bool> moxCalls;
};

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

// One real local window on a Saturn, connected, no MOX delays.
struct LocalWindow {
    MainWindow window{RemoteStationOptions{}, nullptr, MainWindow::ConnectionStartup::Deferred};
    RadioModel* model{window.radioModel()};
    MoxController* mox{model->moxController()};
    HotKeyDispatcher* keys{window.hotKeyDispatcher()};
    MainWindowHotKeyTargets targets{&window};
    ToneRecordingTxChannel tone;
    SliceModel* slice{nullptr};

    LocalWindow()
    {
        model->setBoardForTest(HPSDRHW::Saturn);
        model->configureStreamPool(5, 5, 192000);
        model->setConnectionStateForTest(ConnectionState::Connected);
        mox->setTimerIntervals(0, 0, 0, 0, 0, 0);
        model->transmitModel().setMicSourceLocked(false);
        model->transmitModel().setMicSource(MicSource::Radio);
        slice = model->sliceById(model->addSlice(QStringLiteral("pan-0")));
        slice->setDspMode(DSPMode::USB);
        slice->setFrequency(14'200'000.0);
        TwoToneController* const twoTone = model->twoToneController();
        twoTone->setTxChannel(&tone);
        twoTone->setSliceModel(slice);
        twoTone->setSettleDelaysMs(0, 0);
        twoTone->setPowerOn(true);
    }

    ~LocalWindow()
    {
        model->twoToneController()->setTxChannel(nullptr);
    }

    void press(const QString& id) { keys->press(id, kKeyboard); }
    void release(const QString& id) { keys->release(id, kKeyboard); }
};

// One real hosting window with a hosted Core.
struct HostingWindow {
    QTemporaryDir directory;
    AppSettings settings{directory.filePath(QStringLiteral("station.settings"))};
    MainWindow window{RemoteStationOptions{}, nullptr, MainWindow::ConnectionStartup::Deferred};
    RadioModel* model{window.radioModel()};
    MoxController* mox{model->moxController()};
    HotKeyDispatcher* keys{window.hotKeyDispatcher()};
    MainWindowHotKeyTargets targets{&window};
    std::unique_ptr<DesktopStationController> controller;
    QObject phoneSession;

    bool start()
    {
        model->setBoardForTest(HPSDRHW::Saturn);
        model->configureStreamPool(5, 5, 192000);
        model->setConnectionStateForTest(ConnectionState::Connected);
        mox->setTimerIntervals(0, 0, 0, 0, 0, 0);
        model->transmitModel().setMicSourceLocked(false);
        model->transmitModel().setMicSource(MicSource::Radio);
        SliceModel* const slice = model->sliceById(model->addSlice(QStringLiteral("pan-0")));
        slice->setDspMode(DSPMode::USB);
        slice->setFrequency(14'200'000.0);
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

    // The phone holds transmit, unkeyed.
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
    }
};

// As tst_remote_window_slice_a_close: the window shares slices and controls
// each slice the Core has.
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

QList<QJsonObject> commandsOf(const Test::LoopbackTransport* core, const QString& verb)
{
    QList<QJsonObject> out;
    for (const QByteArray& wire : core->received()) {
        const QJsonObject o = QJsonDocument::fromJson(wire).object();
        if (o.value(QStringLiteral("type")).toString() == QLatin1String("command.invoke")
            && o.value(QStringLiteral("verb")).toString() == verb) {
            out.append(o);
        }
    }
    return out;
}

QJsonValue argument(const QJsonObject& command, const QString& name)
{
    for (const QJsonValue& a : command.value(QStringLiteral("args")).toArray()) {
        if (a.toObject().value(QStringLiteral("name")).toString() == name) {
            return a.toObject().value(QStringLiteral("value"));
        }
    }
    return {};
}

} // namespace

class TestHotKeyTransmit final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QLoggingCategory::setFilterRules(QStringLiteral("nereus.*.debug=false"));
        QVERIFY(RemoteWindowHarness::useIsolatedProfile(QStringLiteral("hot-key-transmit")));
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

    // ---- Local: the buttons' own toggles ----

    void localMoxIsAManualKeyAndASecondPressUnkeys()
    {
        LocalWindow w;
        QVERIFY(!w.targets.transmitting());
        w.press(kMox);
        QVERIFY(w.mox->isMox());
        QVERIFY(w.mox->isManualKey());
        QVERIFY(w.targets.transmitting());

        // It stays on with no release call and through the failsafe.
        w.release(kMox);
        w.keys->releaseAll(kKeyboard);
        QVERIFY(w.mox->isMox());
        QVERIFY(w.mox->isManualKey());

        w.press(kMox);
        QVERIFY(!w.mox->isMox());
        QVERIFY(!w.mox->isManualKey());
        QVERIFY(!w.targets.transmitting());
    }

    void localTuneIsTheButtonsToggle()
    {
        LocalWindow w;
        w.press(kTune);
        QTRY_VERIFY(w.model->isTune());
        QVERIFY(w.mox->isMox());
        QVERIFY(w.mox->isManualKey());
        QVERIFY(w.targets.transmitting());
        QVERIFY(w.targets.tuneOrTwoToneOn());

        w.release(kTune);
        w.keys->releaseAll(kKeyboard);
        QVERIFY(w.model->isTune());
        QVERIFY(w.mox->isMox());

        w.press(kTune);
        QTRY_VERIFY(!w.model->isTune());
        QTRY_VERIFY(!w.mox->isMox());
        QVERIFY(!w.targets.tuneOrTwoToneOn());
    }

    void localTwoToneIsTheButtonsToggle()
    {
        LocalWindow w;
        TwoToneController* const twoTone = w.model->twoToneController();
        w.press(kTwoTone);
        QTRY_VERIFY(twoTone->isActive());
        QVERIFY(w.targets.transmitting());
        QVERIFY(w.targets.tuneOrTwoToneOn());

        w.press(kTwoTone);
        QTRY_VERIFY(!twoTone->isActive());
        QVERIFY(!w.targets.tuneOrTwoToneOn());
    }

    void aRefusedButtonSaysWhy()
    {
        LocalWindow w;
        w.model->setConnectionStateForTest(ConnectionState::Disconnected);
        const int before = w.window.findChildren<StatusToast*>().size();
        w.press(kMox);
        QVERIFY(!w.mox->isMox());
        QVERIFY(w.window.findChildren<StatusToast*>().size() > before);
    }

    // ---- Local: hold ----

    void localHoldKeysWithSpaceAndSurvivesAPollPass()
    {
        LocalWindow w;
        w.press(kHold);
        QVERIFY(w.mox->isMox());
        QCOMPARE(w.mox->pttMode(), PttMode::Space);
        QVERIFY(!w.mox->isManualKey());

        // Real PollPTT passes, with a source going up and down. Both of these
        // run MoxController::pollPtt; the key is Space's, not a polled source's.
        w.mox->onCwPtt(false);
        QVERIFY(w.mox->isMox());
        w.mox->onMicPttFromRadio(true);
        QVERIFY(w.mox->isMox());
        w.mox->onMicPttFromRadio(false);
        QVERIFY(w.mox->isMox());
        QCOMPARE(w.mox->pttMode(), PttMode::Space);

        w.release(kHold);
        QVERIFY(!w.mox->isMox());
    }

    void localHoldDropsOnFailsafe()
    {
        LocalWindow w;
        w.press(kHold);
        QVERIFY(w.mox->isMox());
        w.keys->releaseAll(kKeyboard);
        QVERIFY(!w.mox->isMox());
    }

    void aHoldLeavesALatchedMoxAlone()
    {
        LocalWindow w;
        w.press(kMox);
        QVERIFY(w.mox->isManualKey());
        w.press(kHold);
        w.release(kHold);
        QVERIFY(w.mox->isMox());
        QVERIFY(w.mox->isManualKey());
    }

    void aHoldLeavesTuneAlone()
    {
        LocalWindow w;
        w.press(kTune);
        QTRY_VERIFY(w.model->isTune());
        w.press(kHold);
        QVERIFY(w.model->isTune());
        QVERIFY(w.mox->isMox());
        QVERIFY(w.mox->isManualKey());
        w.release(kHold);
        QVERIFY(w.model->isTune());
        QVERIFY(w.mox->isMox());
    }

    void theMoxButtonEndsAHoldAndTheReleaseDoesNothing()
    {
        LocalWindow w;
        w.press(kHold);
        QVERIFY(w.mox->isMox());

        w.press(kMox);   // its off
        QVERIFY(!w.mox->isMox());

        // Latched while the key is still down: the release must not drop it.
        w.press(kMox);
        QVERIFY(w.mox->isMox());
        QVERIFY(w.mox->isManualKey());
        w.release(kHold);
        QVERIFY(w.mox->isMox());
        QVERIFY(w.mox->isManualKey());
    }

    void aTimeOutClearsTheClaimSoALaterMoxSurvivesTheRelease()
    {
        LocalWindow w;
        w.press(kHold);
        QVERIFY(w.mox->isMox());

        w.model->onTxTimeOut(QStringLiteral("MOX"), 180);
        QTRY_VERIFY(!w.mox->isMox());

        w.press(kMox);
        QTRY_VERIFY(w.mox->isMox());
        w.release(kHold);
        QVERIFY(w.mox->isMox());
        QVERIFY(w.mox->isManualKey());
    }

    void txInhibitRefusesThroughTheMoxToast()
    {
        LocalWindow w;
        QSignalSpy rejected(w.mox, &MoxController::moxRejected);
        const QString why = QStringLiteral("Test inhibit input is on.");
        w.mox->setTxInhibited(true, why);

        QVERIFY(!w.targets.keyHold(true));
        QVERIFY(!w.mox->isMox());
        QCOMPARE(rejected.size(), 1);
        QVERIFY(toastShown(w.window, rejected.first().first().toString()));

        // Through the dispatcher: nothing keys, and no claim outlives it.
        w.press(kHold);
        QVERIFY(!w.mox->isMox());
        w.release(kHold);
        w.mox->setTxInhibited(false);
        w.press(kHold);
        QVERIFY(w.mox->isMox());
        w.release(kHold);
        QVERIFY(!w.mox->isMox());
    }

    void aRefusedGateRefusesThroughTheMoxToast()
    {
        LocalWindow w;
        const TxRefusal refusal = TxRefusals::otherDeviceHolds(QStringLiteral("Test device"));
        w.mox->setKeyingGate([refusal](PttMode, const KeyerIdentity&) {
            KeyingAnswer answer;
            answer.verdict = KeyingVerdict::Refuse;
            answer.refusal = refusal;
            return answer;
        });
        QSignalSpy rejected(w.mox, &MoxController::moxRejected);

        QVERIFY(!w.targets.keyHold(true));
        QVERIFY(!w.mox->isMox());
        QCOMPARE(rejected.size(), 1);
        QVERIFY(toastShown(w.window, refusal.text));
        w.mox->setKeyingGate(nullptr);
    }

    void aHoldNeedsARadio()
    {
        LocalWindow w;
        w.model->setConnectionStateForTest(ConnectionState::Disconnected);
        QVERIFY(!w.targets.keyHold(true));
        QVERIFY(!w.mox->isMox());
    }

    void aHoldCompletesAPendingTuneOffFirst()
    {
        LocalWindow w;
        w.press(kTune);
        QTRY_VERIFY(w.model->isTune());
        w.mox->stallTxToRxWalkForTest();
        w.press(kTune);
        QVERIFY(w.model->tuneOffPendingForTest());
        QVERIFY(!w.targets.transmitting());

        w.press(kHold);
        QVERIFY(!w.model->tuneOffPendingForTest());
        QVERIFY(w.mox->isMox());
        QCOMPARE(w.mox->pttMode(), PttMode::Space);
        w.release(kHold);
        QVERIFY(!w.mox->isMox());
    }

    // ---- R-HK-10: connecting never keys ----

    void connectingNeverKeys()
    {
        AppSettings::instance().setValue(QStringLiteral("HotKeys/Enabled"), QStringLiteral("True"));
        MainWindow window(RemoteStationOptions{}, nullptr, MainWindow::ConnectionStartup::Deferred);
        RadioModel* const model = window.radioModel();
        CountingConnection connection;
        QSignalSpy keyed(model->moxController(), &MoxController::moxChanging);
        model->setBoardForTest(HPSDRHW::Saturn);
        model->configureStreamPool(5, 5, 192000);
        model->injectConnectionForTest(&connection);
        model->addSlice(QStringLiteral("pan-0"));
        pump();
        QVERIFY(connection.moxCalls.isEmpty());
        QVERIFY(keyed.isEmpty());
        QVERIFY(!model->moxController()->isMox());
        QVERIFY(!model->moxController()->isManualKey());
        model->injectConnectionForTest(nullptr);
    }

    // ---- Hosting ----

    void sleepUnkeysAHoldAndLeavesALatchedMox()
    {
        LocalWindow w;
        qint64 wall = 1000000;
        w.keys->setSleepClocksForTest([&wall] { return wall; }, [] { return qint64(5000); });

        w.press(kHold);
        QVERIFY(w.mox->isMox());
        wall += 3600000;
        w.keys->checkForSleep();
        QVERIFY(!w.mox->isMox());
        w.release(kHold);
        QVERIFY(!w.mox->isMox());

        // A MOX latched with a press stays on, as through every failsafe.
        w.press(kMox);
        QVERIFY(w.mox->isMox());
        w.press(kHold);
        wall += 3600000;
        w.keys->checkForSleep();
        QVERIFY(w.mox->isMox());
        QVERIFY(w.mox->isManualKey());
        w.press(kMox);
        QVERIFY(!w.mox->isMox());
    }

    void aHostedHoldKeysThroughTheController()
    {
        if (!QSslSocket::supportsSsl()) { QSKIP("Qt reports no working TLS backend."); }
        HostingWindow w;
        QVERIFY(w.directory.isValid());
        QVERIFY(w.start());

        w.keys->press(kHold, kKeyboard);
        QTRY_VERIFY(w.mox->isMox());
        QVERIFY(w.mox->isManualKey());
        QVERIFY(w.mox->currentKeyer().isStation());
        QVERIFY(w.targets.transmitting());

        w.keys->release(kHold, kKeyboard);
        QTRY_VERIFY(!w.mox->isMox());
        QVERIFY(!w.targets.transmitting());
    }

    void aHostedHoldWhileAnotherDeviceHoldsToastsAndOpensNoDialog()
    {
        if (!QSslSocket::supportsSsl()) { QSKIP("Qt reports no working TLS backend."); }
        HostingWindow w;
        QVERIFY(w.directory.isValid());
        QVERIFY(w.start());
        QVERIFY(w.phoneHolds());

        QVERIFY(!w.targets.keyHold(true));
        QVERIFY(toastShown(w.window, TxRefusals::otherDeviceHolds(QStringLiteral("Phone")).text));
        QVERIFY(w.window.findChild<TakeTransmitDialog*>() == nullptr);
        QVERIFY(!w.mox->isMox());
        QVERIFY(w.controller->server()->transmitHolder()->isHeldBy(kPhoneDevice));

        // Through the dispatcher the same; the release has nothing to do.
        w.keys->press(kHold, kKeyboard);
        QVERIFY(w.window.findChild<TakeTransmitDialog*>() == nullptr);
        QVERIFY(!w.mox->isMox());
        w.keys->release(kHold, kKeyboard);
        pump();
        QVERIFY(!w.mox->isMox());
        QVERIFY(w.controller->server()->transmitHolder()->isHeldBy(kPhoneDevice));
    }

    void anotherDevicesKeyIsNotThisWindowsTransmit()
    {
        if (!QSslSocket::supportsSsl()) { QSKIP("Qt reports no working TLS backend."); }
        HostingWindow w;
        QVERIFY(w.directory.isValid());
        QVERIFY(w.start());
        QVERIFY(w.phoneHolds());
        KeyerIdentity phone;
        phone.deviceId = kPhoneDevice;
        phone.source = PttMode::Manual;
        QSignalSpy wentOff(w.model, &RadioModel::transmitKeyWentOff);
        // This case is about the hold, not the Core's admission of the phone
        // (which wants a live session): the phone's key is simply on.
        w.mox->setKeyingGate([](PttMode, const KeyerIdentity&) { return KeyingAnswer{}; });
        w.mox->setMox(true, phone);
        QVERIFY(w.mox->isMox());

        // A hold is asked and refused rather than silently doing nothing.
        QVERIFY(!w.targets.transmitting());
        w.keys->press(kHold, kKeyboard);
        QVERIFY(toastShown(w.window, TxRefusals::otherDeviceHolds(QStringLiteral("Phone")).text));
        QVERIFY(w.window.findChild<TakeTransmitDialog*>() == nullptr);
        QVERIFY(w.mox->isMox());
        QCOMPARE(w.mox->currentKeyer().deviceId, kPhoneDevice);
        w.keys->release(kHold, kKeyboard);
        QVERIFY(w.mox->isMox());

        // That key ending is not this station's key going off.
        w.mox->setMox(false, phone);
        QVERIFY(!w.mox->isMox());
        QVERIFY(wentOff.isEmpty());
    }

    // ---- Remote: the wire ----

    void aRemoteHoldSendsOneKeyAndOneReleaseWithItsEpoch()
    {
        Test::RemoteAudioSessionHarness h;
        h.pairWindow = true;
        h.makeTransmitReady();
        h.openFakeMicrophoneLine();
        h.connectSession();
        QTRY_VERIFY(h.client.capabilities().txPermitted);
        MoxController* const coreMox = h.station.moxController();

        // Nothing pressed: a release sends nothing. Counted below, after the
        // key and release that follow it on the same ordered link.
        h.remote.setHoldToTalk(false);

        h.remote.setHoldToTalk(true);
        QVERIFY(h.client.remoteTransmit()->screenKeyDown());
        QTRY_VERIFY(coreMox->isMox());
        const quint32 epoch = h.station.keyedBy().epoch;
        QCOMPARE(h.station.keyedBy().trigger, QByteArrayLiteral("screen"));
        QTRY_COMPARE(h.client.remoteTransmit()->screenEpoch(), epoch);

        QSignalSpy wentOff(&h.remote, &RadioModel::transmitKeyWentOff);
        h.remote.setHoldToTalk(false);
        QCOMPARE(wentOff.size(), 1);
        QVERIFY(!h.client.remoteTransmit()->screenKeyDown());
        QTRY_VERIFY(!coreMox->isMox());

        // A second release with the key already up sends nothing either.
        h.remote.setHoldToTalk(false);

        const QList<QJsonObject> keys = commandsOf(h.stationLink, QStringLiteral("tx.key"));
        const QList<QJsonObject> unkeys = commandsOf(h.stationLink, QStringLiteral("tx.unkey"));
        QCOMPARE(keys.size(), RemoteTransmitClient::kCopies);
        QCOMPARE(argument(keys.first(), QStringLiteral("trigger")).toString(),
                 QStringLiteral("screen"));
        QCOMPARE(unkeys.size(), RemoteTransmitClient::kCopies);
        QCOMPARE(argument(unkeys.first(), QStringLiteral("epoch")).toInteger(), qint64(epoch));
        for (const QJsonObject& unkey : unkeys) {
            QCOMPARE(unkey.value(QStringLiteral("id")), unkeys.first().value(QStringLiteral("id")));
        }
        pump();
        QCOMPARE(commandsOf(h.stationLink, QStringLiteral("tx.unkey")).size(),
                 RemoteTransmitClient::kCopies);
    }

    void aRefusedRemoteHoldSaysItWentOff()
    {
        Test::RemoteAudioSessionHarness h;
        h.pairWindow = true;
        h.makeTransmitReady();
        // No microphone line: the Core refuses the voice key.
        h.connectSession();
        QTRY_VERIFY(h.client.capabilities().txPermitted);
        QSignalSpy refused(&h.remote, &RadioModel::remoteTransmitRefused);
        QSignalSpy wentOff(&h.remote, &RadioModel::transmitKeyWentOff);

        h.remote.setHoldToTalk(true);
        QTRY_VERIFY(!refused.isEmpty());
        QVERIFY(!h.client.remoteTransmit()->screenKeyDown());
        QCOMPARE(wentOff.size(), 1);
        QVERIFY(!h.station.moxController()->isMox());
    }

    // ---- Remote: the window ----

    void aRemoteWindowWithoutPermissionRefusesInTheCoresWords()
    {
        RemoteWindowHarness::Options options;
        options.sliceAccess = true;
        RemoteWindowHarness h(options);
        QVERIFY(h.start());
        h.server().setRemoteTransmitAllowed(true);
        h.server().setTokenSessionsMayTransmitForTest(true);
        QVERIFY(connectSharing(h));
        StationClient* const client = h.client();
        StationCapabilities refused = h.server().buildCapabilities();
        refused.txPermitted = false;
        const QString words = QStringLiteral("This Core is not taking keys from you today.");
        refused.txRefusalReason = words;
        h.pushCapabilities(refused);
        QTRY_VERIFY(!client->capabilities().txPermitted);
        QTRY_COMPARE(client->capabilities().txRefusalReason, words);

        HotKeyDispatcher* const keys = h.window()->hotKeyDispatcher();
        keys->press(kHold, kKeyboard);
        QVERIFY(toastShown(*h.window(), words));
        QVERIFY(!client->remoteTransmit()->screenKeyDown());
        QVERIFY(h.window()->findChild<TakeTransmitDialog*>() == nullptr);
        keys->release(kHold, kKeyboard);
        QVERIFY(!client->remoteTransmit()->screenKeyDown());
    }

    void aRemoteHoldFollowsTheLedger()
    {
        RemoteWindowHarness::Options options;
        options.sliceAccess = true;
        RemoteWindowHarness h(options);
        QVERIFY(h.start());
        h.server().setRemoteTransmitAllowed(true);
        h.server().setTokenSessionsMayTransmitForTest(true);
        QVERIFY(connectSharing(h));
        StationClient* const client = h.client();
        StationCapabilities granted = h.server().buildCapabilities();
        granted.txPermitted = true;
        h.pushCapabilities(granted);
        QTRY_VERIFY(client->capabilities().txPermitted);

        RemoteTransmitClient* const remote = client->remoteTransmit();
        HotKeyDispatcher* const keys = h.window()->hotKeyDispatcher();
        MainWindowHotKeyTargets targets(h.window());
        QVERIFY(!targets.transmitting());

        // No event loop runs from here to the release: each step is this
        // window's own, whatever the bench Core answers later.
        keys->press(kHold, kKeyboard);
        QVERIFY(remote->screenKeyDown());
        QVERIFY(targets.transmitting());

        keys->press(kMox, kKeyboard);   // its off
        QVERIFY(!remote->screenKeyDown());

        keys->press(kMox, kKeyboard);   // latched while the hold key is down
        QVERIFY(remote->screenKeyDown());
        keys->release(kHold, kKeyboard);
        QVERIFY(remote->screenKeyDown());

        keys->press(kMox, kKeyboard);
        QVERIFY(!remote->screenKeyDown());

        // A plain hold and release.
        keys->press(kHold, kKeyboard);
        QVERIFY(remote->screenKeyDown());
        keys->release(kHold, kKeyboard);
        QVERIFY(!remote->screenKeyDown());
    }

    // ---- First-look fixes: items 2, 9 and 11 ----

    // A TUNE still waiting for its answer must not take away the hold's unkey:
    // refused, it would leave the radio keyed with nothing holding it.
    void aTuneStillAskedDoesNotKeepTheKeyAfterTheRelease()
    {
        RemoteWindowHarness::Options options;
        options.sliceAccess = true;
        RemoteWindowHarness h(options);
        QVERIFY(h.start());
        h.server().setRemoteTransmitAllowed(true);
        h.server().setTokenSessionsMayTransmitForTest(true);
        QVERIFY(connectSharing(h));
        StationClient* const client = h.client();
        StationCapabilities granted = h.server().buildCapabilities();
        granted.txPermitted = true;
        h.pushCapabilities(granted);
        QTRY_VERIFY(client->capabilities().txPermitted);
        RemoteTransmitClient* const remote = client->remoteTransmit();
        HotKeyDispatcher* const keys = h.window()->hotKeyDispatcher();

        // No event loop from here on: the Core has answered nothing.
        keys->press(kHold, kKeyboard);
        QVERIFY(remote->screenKeyDown());
        keys->press(kTune, kKeyboard);
        QVERIFY(remote->tuneAsked());
        keys->release(kHold, kKeyboard);
        QVERIFY(!remote->screenKeyDown());
    }

    void aTuneConfirmedOnSurvivesTheRelease()
    {
        RemoteWindowHarness::Options options;
        options.sliceAccess = true;
        RemoteWindowHarness h(options);
        QVERIFY(h.start());
        h.server().setRemoteTransmitAllowed(true);
        h.server().setTokenSessionsMayTransmitForTest(true);
        QVERIFY(connectSharing(h));
        StationClient* const client = h.client();
        StationCapabilities granted = h.server().buildCapabilities();
        granted.txPermitted = true;
        h.pushCapabilities(granted);
        QTRY_VERIFY(client->capabilities().txPermitted);
        RemoteTransmitClient* const remote = client->remoteTransmit();
        HotKeyDispatcher* const keys = h.window()->hotKeyDispatcher();

        // No event loop here either: this bench Core would refuse the key.
        keys->press(kHold, kKeyboard);
        QVERIFY(remote->screenKeyDown());
        // The Core's TUNE reads on in the window's copy, as the button lights it.
        h.window()->radioModel()->transmitModel().setTune(true);
        keys->release(kHold, kKeyboard);
        QVERIFY(remote->screenKeyDown());
    }

    // A hosted take the Core doesn't grant leaves no claim: MOX latched after
    // it survives letting go.
    void aHostedTakeNotGrantedLeavesNoClaimOnALaterMox()
    {
        if (!QSslSocket::supportsSsl()) { QSKIP("Qt reports no working TLS backend."); }
        HostingWindow w;
        QVERIFY(w.directory.isValid());
        QVERIFY(w.start());
        TransmitHolder* const holder = w.controller->server()->transmitHolder();

        // MOX reads on past the wait, by the holder's hook only, so the take
        // waits and ends not assigned. The wait fires when this test says.
        const TransmitHolder::Hooks saved = holder->hooks();
        bool moxReadsOn = true;
        std::function<void()> endOfWait;
        TransmitHolder::Hooks forced = saved;
        forced.moxOn = [&moxReadsOn]() { return moxReadsOn; };
        forced.schedule = [&endOfWait](int, std::function<void()> fire) { endOfWait = std::move(fire); };
        holder->setHooks(forced);

        w.keys->press(kHold, kKeyboard);
        QVERIFY(!w.mox->isMox());
        QVERIFY(endOfWait);
        endOfWait();
        QVERIFY(!holder->isHeldBy(SliceOwnership::stationDevice()));

        moxReadsOn = false;
        holder->setHooks(saved);
        holder->onMoxReading(false);
        QTRY_VERIFY(holder->state() == TransmitHolder::State::Unheld);

        // Still holding the key, MOX with the button.
        w.keys->press(kMox, kKeyboard);
        QTRY_VERIFY(w.mox->isMox());
        w.keys->release(kHold, kKeyboard);
        pump();
        QVERIFY(w.mox->isMox());
        w.keys->press(kMox, kKeyboard);
        QTRY_VERIFY(!w.mox->isMox());
    }

    // A hosting window says why a hold does nothing while another device is on
    // the air. A remote one says the same, and sends the Core nothing.
    void aRemoteHoldWhileAnotherDeviceIsOnTheAirSaysWhy()
    {
        RemoteWindowHarness::Options options;
        options.sliceAccess = true;
        RemoteWindowHarness h(options);
        QVERIFY(h.start());
        h.server().setRemoteTransmitAllowed(true);
        h.server().setTokenSessionsMayTransmitForTest(true);
        QVERIFY(connectSharing(h));
        StationClient* const client = h.client();

        DeviceSessionRegistry::Entry peer;
        peer.deviceId = kPhoneDevice;
        peer.kind = DeviceSessionRegistry::Kind::Token;
        peer.name = QStringLiteral("Phone");
        peer.shortName = QStringLiteral("Phone");
        peer.deviceKind = QStringLiteral("phone");
        QObject phoneSession;
        QCOMPARE(h.server().deviceSessions()->admit(peer, &phoneSession).admission,
                 DeviceSessionRegistry::Admission::Admitted);
        TransmitHolder::KeyRequest key;
        key.deviceId = peer.deviceId;
        QCOMPARE(h.server().transmitHolder()->askKey(key).verdict, KeyingVerdict::Admit);
        MoxController* const coreMox = h.station().moxController();
        coreMox->setTimerIntervals(0, 0, 0, 0, 0, 0);
        coreMox->setMoxCheck(nullptr);
        coreMox->setKeyingGate([](PttMode, const KeyerIdentity&) { return KeyingAnswer{}; });
        KeyerIdentity phone;
        phone.deviceId = kPhoneDevice;
        phone.source = PttMode::Manual;
        coreMox->setMox(true, phone);
        QVERIFY(coreMox->isMox());
        RadioModel* const model = h.window()->radioModel();
        QTRY_VERIFY(model->isTransmitting());
        const QString words = TxRefusals::otherDeviceHolds(QStringLiteral("Phone")).text;
        QTRY_COMPARE(client->otherHolderReason(), words);

        // Even with a permission the window has yet to hear go away.
        StationCapabilities granted = h.server().buildCapabilities();
        granted.txPermitted = true;
        h.pushCapabilities(granted);
        QTRY_VERIFY(client->capabilities().txPermitted);

        RemoteTransmitClient* const remote = client->remoteTransmit();
        HotKeyDispatcher* const keys = h.window()->hotKeyDispatcher();
        const auto shown = [&h, &words]() {
            int count = 0;
            for (StatusToast* const toast : h.window()->findChildren<StatusToast*>()) {
                count += toast->message() == words ? 1 : 0;
            }
            return count;
        };
        QCOMPARE(shown(), 0);
        keys->press(kHold, kKeyboard);
        QCOMPARE(shown(), 1);
        QVERIFY(!remote->screenKeyDown());
        keys->release(kHold, kKeyboard);
        QVERIFY(!remote->screenKeyDown());
        coreMox->setMox(false, phone);
    }

    void aCoreKeyIsTransmitting()
    {
        RemoteWindowHarness::Options options;
        options.sliceAccess = true;
        RemoteWindowHarness h(options);
        QVERIFY(h.start());
        QVERIFY(connectSharing(h));
        MainWindowHotKeyTargets targets(h.window());
        QVERIFY(!targets.transmitting());
        MoxController* const coreMox = h.station().moxController();
        coreMox->setTimerIntervals(0, 0, 0, 0, 0, 0);
        coreMox->setMoxCheck(nullptr);
        coreMox->setMox(true);
        QVERIFY(coreMox->isMox());
        QTRY_VERIFY(h.window()->radioModel()->isTransmitting());
        QVERIFY(targets.transmitting());
        coreMox->setMox(false);
        QTRY_VERIFY(!h.window()->radioModel()->isTransmitting());
        QVERIFY(!targets.transmitting());
    }
};

QTEST_MAIN(TestHotKeyTransmit)
#include "tst_hot_key_transmit.moc"
