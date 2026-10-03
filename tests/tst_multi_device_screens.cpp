// =================================================================
// tests/tst_multi_device_screens.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. iPhone app plan Task 78 (R-IOS-02,
// R-IOS-07, R-IOS-30): the desktop remote window's screens for several
// devices on one Core (the several-devices design, section 12).
//
// A real StationClient (the remote window's), signed in with its own key to
// a real StationServer over a loopback link, beside a second device played
// by the test on the same Core. The window's screens (MultiDeviceController
// and its dialogs and notice cards, the pan's TX pill, the flags, the
// panadapter's other-device markers, the connected list) are driven by
// their own buttons; the second device's side is the wire. Nothing keys a
// real radio: the Core's model has no radio and a TX channel whose RF gate
// is only observed.
//
// NEREUS_TASK78_SHOTS, when set, names a directory the screenshots case
// writes its offscreen renders to (one PNG per state).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-26: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 78 (R-IOS-02, R-IOS-07,
//               R-IOS-30), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-28: the fifth-device choice and the named takeover on the stop
//               panel (Task 78 items 3 and 7, G-53). J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude Code.
//   2026-09-28: This Core manages the Core's devices (iPhone app plan Task
//               25). J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-29: slice control plan Task 14b: the hosting desktop's
//               "Your volume" on a listened flag sets only its own
//               listening level; the capture of a listened flag's audio
//               tab. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//               Code.
//   2026-09-29: slice control plan Task 10: the hosting desktop's Add at
//               full capacity asks with the remote window's chooser. J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude Code.
//   2026-09-29: slice control plan Task 11: a hosting window's empty pan
//               gets a station-device slice. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-30: fix wave GUI-I1 and GUI-M6: the take question does not
//               outlive its session, and a card's Take it back is shown off
//               once its session ends. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
// =================================================================

#include "MultiDeviceHarness.h"

#include <QApplication>
#include <QDir>
#include <QImage>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QSignalSpy>
#include <QTreeWidget>

#include <cmath>

#include "core/MoxController.h"
#include "core/StepAttenuatorController.h"
#include "core/StepAttenuatorFacade.h"
#include "core/TxChannel.h"
#include "core/WdspEngine.h"
#include "core/security/ClientDeviceIdentity.h"
#include "core/session/RemoteDevicesState.h"
#include "core/session/SliceAccessController.h"
#include "core/SliceOwnership.h"
#include "core/session/StationClient.h"
#include "core/session/TransmitStateFacade.h"
#include "core/settings/SettingsProxy.h"
#include "gui/HostingSliceActions.h"
#include "gui/MainWindow.h"
#include "gui/PanadapterApplet.h"
#include "gui/SpectrumWidget.h"
#include "gui/applets/RxApplet.h"
#include "gui/multidevice/ConfirmChangeDialog.h"
#include "gui/multidevice/ConnectedDevicesList.h"
#include "gui/multidevice/DeviceWords.h"
#include "gui/multidevice/MultiDeviceController.h"
#include "gui/multidevice/NoticeCard.h"
#include "gui/multidevice/ReplaceDeviceDialog.h"
#include "gui/RemoteConnectionController.h"
#include "gui/setup/ThisCorePage.h"
#include "core/session/StationDevicesFacade.h"
#include "gui/multidevice/TakeReceiverDialog.h"
#include "gui/multidevice/TakeTransmitDialog.h"
#include "gui/widgets/SpectrumStatusOverlay.h"
#include "gui/widgets/StatusBadge.h"
#include "gui/widgets/VfoWidget.h"

namespace {

// The window: this computer's own key, a remote model and its client.
struct Window {
    QTemporaryDir keyDir;
    std::shared_ptr<const ClientDeviceIdentity> key = std::make_shared<const ClientDeviceIdentity>(
        ClientDeviceIdentity::loadOrCreate(keyDir.path()));
    RadioModel remote{RadioModel::Role::Remote};
    SettingsProxy proxy;
    StationClient client{&remote, &proxy};
    QWidget host;

    Window()
    {
        client.setDeviceIdentity(key, QStringLiteral("Shack MacBook"), QStringLiteral("MacBook"));
        host.resize(900, 500);
    }

    PairedDevice record() const
    {
        PairedDevice device;
        device.id = key->fingerprint();
        device.publicKeySpki = key->publicKeySpki();
        device.name = QStringLiteral("Shack MacBook");
        device.kind = QStringLiteral("computer");
        return device;
    }

    QString id() const { return StationIdentity::toBase64Url(key->fingerprint()); }

    // Starts signing in to `core` with this computer's key and returns
    // without waiting (a full Core asks which device to replace first).
    void startTo(Core& core)
    {
        auto* station = new LoopbackTransport(QStringLiteral("station"));
        auto* peer = new LoopbackTransport(QStringLiteral("window"));
        station->setPeerAddress(QStringLiteral("192.0.2.30"));
        peer->setPeerCertificateSha256(core.certSha256());
        station->linkTo(peer);
        client.startSession(peer, QString(), QString(),
                            core.server->stationIdentity().fingerprint());
        core.server->acceptTransport(station);
    }

    // Signs in to `core` with this computer's key, as a paired window does.
    bool connectTo(Core& core)
    {
        startTo(core);
        return QTest::qWaitFor([this]() { return client.isHandshakeComplete(); }, 5000);
    }
};

// The Core's model with a TX channel wired as the connect path wires it
// (no WDSP channel; its RF gate is only observed), as the multi-session
// tests do.
struct CoreTx {
    RadioModel* model;
    TxChannel tx{WdspEngine::kTxChannelId};
    explicit CoreTx(RadioModel* m)
        : model(m)
    {
        model->injectTxChannelForTest(&tx);
        model->wireTxChannelKeyingForTest();
    }
    ~CoreTx() { model->injectTxChannelForTest(nullptr); }
};

bool heldBy(Core& core, const QByteArray& deviceId)
{
    return core.server->transmitHolder()->isHeldBy(deviceId);
}

template <typename T>
T* openDialogOf(MultiDeviceController& controller)
{
    T* dialog = nullptr;
    const bool shown = QTest::qWaitFor([&]() {
        dialog = qobject_cast<T*>(controller.openDialog());
        return dialog != nullptr;
    }, 5000);
    Q_UNUSED(shown);
    return dialog;
}

QString shotsDir()
{
    return qEnvironmentVariable("NEREUS_TASK78_SHOTS");
}

void saveShot(QWidget* widget, const QString& name, bool fitToHint = true)
{
    QVERIFY(widget != nullptr);
    if (fitToHint) {
        widget->adjustSize();
    }
    const QImage image = widget->grab().toImage();
    QVERIFY(!image.isNull());
    const QString dir = shotsDir();
    if (!dir.isEmpty()) {
        QDir().mkpath(dir);
        QVERIFY(image.save(QDir(dir).filePath(name + QStringLiteral(".png"))));
    }
}

void setTx(TransmitState& tx, const char* name, const QVariant& value)
{
    QVERIFY(tx.applyStationValue(QByteArray(name), value));
}

} // namespace

class TstMultiDeviceScreens : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        const QString profile =
            QStringLiteral("multi-device-screens-%1").arg(QCoreApplication::applicationPid());
        AppSettings::setProfileOverride(profile);
        AppSettings::instance().clear();
    }

    // The window signed in with its own key declares sessionHolder and
    // hears who is on the Core: each device once, under the name the Core
    // numbered, the other device's slice as a marker, and only its own
    // slices as slices.
    void aKeySignedInWindowSharesTheCoreAndHearsWhoIsOn()
    {
        Core core;
        core.model->configureStreamPool(3, 5, 192000);
        Window w;
        QVERIFY(core.server->deviceStore()->add(w.record()));
        QVERIFY(w.connectTo(core));
        QTRY_VERIFY(w.client.sessionHolderAvailable());
        QVERIFY(w.remote.stationMayCloseLastSlice());

        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"), QStringLiteral("iPad"));
        core.pair(b);
        LoopbackTransport* appB = core.signIn(b);
        QVERIFY(admitted(appB));
        RemoteDevicesState* devices = w.client.remoteDevices();
        QTRY_COMPARE(devices->connectedDevices().size(), 2);
        QStringList ids;
        for (const RemoteConnectedDevice& d : devices->connectedDevices()) {
            ids << d.deviceId;
        }
        QCOMPARE(ids.removeDuplicates(), 0);
        QVERIFY(ids.contains(w.id()));
        QVERIFY(ids.contains(b.id()));
        QCOMPARE(devices->deviceLimit(), 4);
        const RemoteConnectedDevice ipad = *devices->connectedDevice(b.id());
        QCOMPARE(ipad.name, QStringLiteral("iPad"));
        QCOMPARE(ipad.shortName, QStringLiteral("iPad"));

        // B's slice is a marker here, never a slice of this window's.
        const QList<int> bSlices = core.model->sliceOwnership()->ownedBy(b.key.fingerprint());
        QCOMPARE(bSlices.size(), 1);
        QTRY_VERIFY(devices->marker(bSlices.first()).has_value());
        QCOMPARE(devices->marker(bSlices.first())->ownerShortName, QStringLiteral("iPad"));
        QTRY_VERIFY(!w.remote.slices().isEmpty());
        for (SliceModel* slice : w.remote.slices()) {
            QVERIFY(!bSlices.contains(slice->sliceIndex()));
        }

        // The connected list: each device once, this window marked, then
        // nothing paired left over.
        ConnectedDevicesList list;
        list.setDevices(devices);
        QCOMPARE(list.tree()->topLevelItemCount(), 2);
        int self = 0;
        for (int i = 0; i < list.tree()->topLevelItemCount(); ++i) {
            if (list.tree()->topLevelItem(i)->text(0).endsWith(QStringLiteral("(this window)"))) {
                ++self;
            }
        }
        QCOMPARE(self, 1);
        QVERIFY(OperatorWording::isPlain(list.emptyLabel()->text()));
    }

    // An older window (no sessionHolder) sees today's wire: no list, no
    // markers, and it keeps its last slice.
    void anOlderWindowIsNotADeviceThatSharesTheCore()
    {
        Core core;
        Window w;
        w.client.setDeclaresSessionHolder(false);
        QVERIFY(core.server->deviceStore()->add(w.record()));
        QVERIFY(w.connectTo(core));
        QVERIFY(!w.client.sessionHolderAvailable());
        QVERIFY(!w.client.transmitTakeAvailable());
        QVERIFY(!w.remote.stationMayCloseLastSlice());
        QTest::qWait(50);
        QVERIFY(w.client.remoteDevices()->connectedDevices().isEmpty());
    }

    // The acceptance that matters most: after the radio's own PTT takes
    // transmit as "Radio", the window takes it back through the question,
    // and its MOX keys again.
    void theWindowTakesTransmitBackFromTheRadioAndItsMoxWorksAgain()
    {
        Core core;
        allowTransmit(core);
        CoreTx coreTx(core.model.get());
        Window w;
        QVERIFY(core.server->deviceStore()->add(w.record()));
        QVERIFY(w.connectTo(core));
        QTRY_VERIFY(w.client.transmitTakeAvailable());
        MoxController* mox = core.model->moxController();

        // The radio's PTT: the station takes transmit and keeps it after
        // the press ends.
        mox->onMicPttFromRadio(true);
        QTRY_VERIFY(heldBy(core, QByteArray(KeyerIdentity::kStationDeviceId)));
        QTRY_VERIFY(mox->isMox());
        mox->onMicPttFromRadio(false);
        QTRY_VERIFY(!mox->isMox());
        QVERIFY(heldBy(core, QByteArray(KeyerIdentity::kStationDeviceId)));
        TransmitState& tx = *w.client.transmitState();
        QTRY_COMPARE(tx.holderSource(), QStringLiteral("radioPtt"));
        QTRY_VERIFY(!tx.keyed());
        QTRY_VERIFY(w.client.transmitHeldElsewhere());
        QTRY_VERIFY(!w.client.capabilities().txPermitted);
        const DeviceWords::HolderBadge badge = DeviceWords::holderBadge(tx, w.id());
        QVERIFY(badge.shown);
        QCOMPARE(badge.label, QStringLiteral("Radio"));

        // Take transmit: asked first, plain (the radio is not on the air).
        MultiDeviceController controller(&w.client, &w.host);
        QSignalSpy taken(&controller, &MultiDeviceController::transmitTaken);
        controller.askTakeTransmit();
        auto* ask = openDialogOf<TakeTransmitDialog>(controller);
        QVERIFY(ask != nullptr);
        QCOMPARE(ask->questionLabel()->text(), QStringLiteral("Take transmit from Radio?"));
        QVERIFY(!ask->redButton());
        QCOMPARE(ask->takeButton()->text(), QStringLiteral("Take transmit"));
        QVERIFY(OperatorWording::isPlain(ask->detailLabel()->text()));
        QTest::mouseClick(ask->takeButton(), Qt::LeftButton);
        QTRY_VERIFY(heldBy(core, w.key->fingerprint()));
        QTRY_VERIFY(w.client.holdsTransmitHere());
        QTRY_VERIFY(w.client.capabilities().txPermitted);
        QTRY_COMPARE(taken.count(), 1);
        QVERIFY(!w.client.transmitHeldElsewhere());
        QVERIFY(!mox->isMox());  // a take never keys

        // MOX works again: the window's key reaches the radio as this
        // window's device.
        w.client.remoteTransmit()->setScreenKey(true);
        QTRY_VERIFY(mox->isMox());
        QCOMPARE(mox->currentKeyer().deviceId, w.key->fingerprint());
        w.client.remoteTransmit()->setScreenKey(false);
        QTRY_VERIFY(!mox->isMox());
    }

    // While the holder is on the air the question is the red "Unkey and
    // take over"; a question the Core asks (what the window showed no
    // longer holds) is the same shape and answers confirm.proceed.
    void aTakeFromTheAirIsRedAndTheCoresOwnQuestionIsTheSameShape()
    {
        Core core;
        allowTransmit(core);
        CoreTx coreTx(core.model.get());
        Window w;
        QVERIFY(core.server->deviceStore()->add(w.record()));
        QVERIFY(w.connectTo(core));
        QTRY_VERIFY(w.client.transmitTakeAvailable());
        MoxController* mox = core.model->moxController();
        mox->onMicPttFromRadio(true);
        QTRY_VERIFY(mox->isMox());
        TransmitState& tx = *w.client.transmitState();
        QTRY_VERIFY(tx.keyed() && w.client.transmitHeldElsewhere());
        QCOMPARE(DeviceWords::holderBadge(tx, w.id()).tone,
                 DeviceWords::HolderBadge::Tone::OnAir);

        MultiDeviceController controller(&w.client, &w.host);
        controller.askTakeTransmit();
        auto* ask = openDialogOf<TakeTransmitDialog>(controller);
        QVERIFY(ask != nullptr);
        QVERIFY(ask->redButton());
        QCOMPARE(ask->takeButton()->text(), QStringLiteral("Unkey and take over"));
        // The window cancels; nothing changes.
        QTest::mouseClick(ask->cancelButton(), Qt::LeftButton);
        QTest::qWait(50);
        QVERIFY(heldBy(core, QByteArray(KeyerIdentity::kStationDeviceId)));

        // A take sent without what was shown: the Core asks, red.
        QVERIFY(w.client.requestTakeTransmit(false, 0, false) != 0);
        auto* coreAsk = openDialogOf<TakeTransmitDialog>(controller);
        QVERIFY(coreAsk != nullptr);
        QCOMPARE(coreAsk->questionLabel()->text(), QStringLiteral("Take transmit from Radio?"));
        QVERIFY(coreAsk->redButton());
        QTest::mouseClick(coreAsk->takeButton(), Qt::LeftButton);
        QTRY_VERIFY(heldBy(core, w.key->fingerprint()));
        QTRY_VERIFY(!mox->isMox());
        mox->onMicPttFromRadio(false);
    }

    // What another device did arrives as a card with Take it back, which
    // sends notice.takeBack: the Core asks the take question again, and
    // Confirm takes transmit back.
    void aNoticeCardTakesTransmitBack()
    {
        Core core;
        allowTransmit(core);
        CoreTx coreTx(core.model.get());
        Window w;
        QVERIFY(core.server->deviceStore()->add(w.record()));
        QVERIFY(w.connectTo(core));
        QTRY_VERIFY(w.client.transmitTakeAvailable());
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"), QStringLiteral("iPad"));
        core.pair(b);
        LoopbackTransport* appB = core.signIn(b, kTransmitter);
        QVERIFY(admitted(appB));

        MultiDeviceController controller(&w.client, &w.host);
        controller.setNoticeHost(&w.host);
        // Unheld: the window's take is at once.
        QVERIFY(w.client.requestTakeTransmit(false, 0, false) != 0);
        QTRY_VERIFY(heldBy(core, w.key->fingerprint()));

        // B takes it (asked on B, confirmed there).
        core.invoke(appB, "tx.take", {});
        const QJsonObject asked = firstOfType(appB->received(), QStringLiteral("confirm.request"));
        QVERIFY(!asked.isEmpty());
        core.invoke(appB, "confirm.proceed",
                    {int64("id", asked.value(QStringLiteral("id")).toInteger()),
                     int64("choice", -1)});
        QTRY_VERIFY(heldBy(core, b.key.fingerprint()));

        // The window's card.
        QTRY_COMPARE(controller.noticeCards().size(), 1);
        NoticeCard* card = controller.noticeCards().first();
        QVERIFY(card->textLabel()->text().contains(QStringLiteral("took transmit.")));
        QVERIFY(OperatorWording::isPlain(card->textLabel()->text()));
        QVERIFY(card->takeBackButton() != nullptr);
        QTest::mouseClick(card->takeBackButton(), Qt::LeftButton);
        QTRY_VERIFY(controller.noticeCards().isEmpty());
        // notice.takeBack is tx.take with its question.
        auto* ask = openDialogOf<TakeTransmitDialog>(controller);
        QVERIFY(ask != nullptr);
        QCOMPARE(ask->questionLabel()->text(), QStringLiteral("Take transmit from iPad?"));
        QTest::mouseClick(ask->takeButton(), Qt::LeftButton);
        QTRY_VERIFY(heldBy(core, w.key->fingerprint()));
        // B is told in turn.
        QTRY_VERIFY(!ofType(appB->received(), QStringLiteral("notice")).isEmpty());
    }

    // Fix wave GUI-I1: the take question answers the session it was asked
    // in. A redial that replaces the link to another Core, whose radio
    // holds transmit at the same holder epoch, closes it, and its Take
    // sends nothing. Nothing keys.
    void theTakeQuestionDoesNotOutliveItsSession()
    {
        Core first;
        allowTransmit(first);
        CoreTx firstTx(first.model.get());
        Core second;
        allowTransmit(second);
        CoreTx secondTx(second.model.get());
        Window w;
        QVERIFY(first.server->deviceStore()->add(w.record()));
        QVERIFY(second.server->deviceStore()->add(w.record()));
        const QByteArray station(KeyerIdentity::kStationDeviceId);
        for (Core* core : {&first, &second}) {
            MoxController* mox = core->model->moxController();
            mox->onMicPttFromRadio(true);
            QTRY_VERIFY(heldBy(*core, station));
            QTRY_VERIFY(mox->isMox());
            mox->onMicPttFromRadio(false);
            QTRY_VERIFY(!mox->isMox());
        }
        QCOMPARE(first.server->transmitHolder()->epoch(),
                 second.server->transmitHolder()->epoch());

        QVERIFY(w.connectTo(first));
        QTRY_VERIFY(w.client.transmitHeldElsewhere());
        MultiDeviceController controller(&w.client, &w.host);
        controller.askTakeTransmit();
        QPointer<TakeTransmitDialog> ask = openDialogOf<TakeTransmitDialog>(controller);
        QVERIFY(ask != nullptr);

        const quint32 epoch = w.client.sessionEpoch();
        w.startTo(second);
        QTRY_VERIFY(w.client.sessionEpoch() == epoch + 1 && w.client.isHandshakeComplete());
        QTRY_VERIFY(w.client.transmitHeldElsewhere());
        if (ask && ask->isVisible()) {
            QTest::mouseClick(ask->takeButton(), Qt::LeftButton);
        }
        QCOMPARE(controller.lastTakeCommandId(), quint32(0));
        QTRY_VERIFY(controller.openDialog() == nullptr);
        QVERIFY(heldBy(second, station));
        QVERIFY(!second.model->moxController()->isMox());
    }

    // Fix wave GUI-M6: a notice card stays to be read after its session
    // ends, but its Take it back is shown off with the reason.
    void aNoticeCardsTakeBackEndsWithItsSession()
    {
        Core core;
        allowTransmit(core);
        CoreTx coreTx(core.model.get());
        Window w;
        QVERIFY(core.server->deviceStore()->add(w.record()));
        QVERIFY(w.connectTo(core));
        QTRY_VERIFY(w.client.transmitTakeAvailable());
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"), QStringLiteral("iPad"));
        core.pair(b);
        LoopbackTransport* appB = core.signIn(b, kTransmitter);
        QVERIFY(admitted(appB));
        MultiDeviceController controller(&w.client, &w.host);
        controller.setNoticeHost(&w.host);
        QVERIFY(w.client.requestTakeTransmit(false, 0, false) != 0);
        QTRY_VERIFY(heldBy(core, w.key->fingerprint()));
        core.invoke(appB, "tx.take", {});
        const QJsonObject asked = firstOfType(appB->received(), QStringLiteral("confirm.request"));
        QVERIFY(!asked.isEmpty());
        core.invoke(appB, "confirm.proceed",
                    {int64("id", asked.value(QStringLiteral("id")).toInteger()),
                     int64("choice", -1)});
        QTRY_VERIFY(heldBy(core, b.key.fingerprint()));
        QTRY_COMPARE(controller.noticeCards().size(), 1);
        QPointer<NoticeCard> card = controller.noticeCards().first();
        QVERIFY(card->takeBackButton() != nullptr);
        QVERIFY(card->takeBackButton()->isEnabled());

        w.client.disconnectFromStation(QStringLiteral("test complete"));
        QTRY_VERIFY(!card->takeBackButton()->isEnabled());
        QVERIFY(card);
        QCOMPARE(controller.noticeCards().size(), 1);
        QVERIFY(!card->takeBackButton()->toolTip().isEmpty());
        QVERIFY2(OperatorWording::isPlain(card->takeBackButton()->toolTip()),
                 qPrintable(card->takeBackButton()->toolTip()));
    }

    // A shared setting that asks does so with one dialog shape: Cancel
    // sends confirm.cancel and changes nothing; Confirm sends
    // confirm.proceed and the change lands.
    void theSharedSettingQuestionCancelsOrConfirms()
    {
        Core core;
        StepAttenuatorController stepAtt;
        stepAtt.setTickTimerEnabled(false);
        core.model->setStepAttController(&stepAtt);
        core.model->configureStreamPool(3, 5, 192000);
        core.model->sliceById(0)->setFrequency(7074000.0);
        Window w;
        QVERIFY(core.server->deviceStore()->add(w.record()));
        QVERIFY(w.connectTo(core));
        QTRY_VERIFY(w.client.sessionHolderAvailable());
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"), QStringLiteral("iPad"));
        core.pair(b);
        LoopbackTransport* appB = core.signIn(b);
        QVERIFY(admitted(appB));
        const int bSlice = core.model->sliceOwnership()->ownedBy(b.key.fingerprint()).first();
        core.model->sliceById(bSlice)->setFrequency(14074000.0);

        MultiDeviceController controller(&w.client, &w.host);
        // JJ's ruling 7.1a (2026-09-28): the attenuator applies at once and
        // tells, with no dialog; diversity asks.
        StepAttenuatorFacade* facade = w.remote.stepAttFacade();
        QVERIFY(facade != nullptr);
        QTRY_VERIFY(w.client.mirroredObject("stepAtt") != nullptr);
        facade->setAttenuationDb(20);
        QTRY_COMPARE(core.model->stepAttFacade()->attenuationDb(), 20);
        QTRY_VERIFY(!ofType(appB->received(), QStringLiteral("notice")).isEmpty());
        QVERIFY(!w.client.remoteDevices()->question().has_value());
        const qsizetype toldBefore = ofType(appB->received(), QStringLiteral("notice")).size();

        const int own = core.model->sliceOwnership()->ownedBy(w.key->fingerprint()).first();
        QTRY_VERIFY(w.remote.sliceById(own) != nullptr);
        SliceModel* mine = w.remote.sliceById(own);
        mine->setDiversityGainDb(6.0);
        auto* ask = openDialogOf<ConfirmChangeDialog>(controller);
        QVERIFY(ask != nullptr);
        QVERIFY2(ask->changeLabel()->text().contains(QStringLiteral("6 dB")),
                 qPrintable(ask->changeLabel()->text()));
        QVERIFY2(ask->affectedLabel()->text().contains(QStringLiteral("iPad")),
                 qPrintable(ask->affectedLabel()->text()));
        QVERIFY(OperatorWording::isPlain(ask->affectedLabel()->text()));
        QTest::mouseClick(ask->cancelButton(), Qt::LeftButton);
        QTRY_VERIFY(!w.client.remoteDevices()->question().has_value());
        QTest::qWait(50);
        QCOMPARE(core.model->sliceById(own)->diversityGainDb(), 0.0);

        mine->setDiversityGainDb(0.0);
        QTest::qWait(50);
        mine->setDiversityGainDb(6.0);
        auto* again = openDialogOf<ConfirmChangeDialog>(controller);
        QVERIFY(again != nullptr);
        QTest::mouseClick(again->confirmButton(), Qt::LeftButton);
        QTRY_COMPARE(core.model->sliceById(own)->diversityGainDb(), 6.0);
        QTRY_VERIFY(ofType(appB->received(), QStringLiteral("notice")).size() > toldBefore);
    }

    // The chooser lists the receivers with their devices and slices, one
    // pick, the untakeable ones disabled with why; Take sends the pick.
    void theChooserTakesAReceiver()
    {
        Core core;
        core.model->configureStreamPool(2, 5, 192000);
        core.model->sliceById(0)->setFrequency(7074000.0);
        Window w;
        QVERIFY(core.server->deviceStore()->add(w.record()));
        QVERIFY(w.connectTo(core));
        QTRY_VERIFY(w.client.sessionHolderAvailable());
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"), QStringLiteral("iPad"));
        core.pair(b);
        LoopbackTransport* appB = core.signIn(b);
        QVERIFY(admitted(appB));
        const int bSlice = core.model->sliceOwnership()->ownedBy(b.key.fingerprint()).first();
        core.model->sliceById(bSlice)->setFrequency(14074000.0);

        MultiDeviceController controller(&w.client, &w.host);
        w.client.invokeCommand("addSliceOnPan", {utf8("panId", QStringLiteral("pan-a2"))});
        auto* chooser = openDialogOf<TakeReceiverDialog>(controller);
        QVERIFY(chooser != nullptr);
        QVERIFY(chooser->choiceList()->count() >= 2);
        int disabled = 0;
        bool namesIpad = false;
        for (int i = 0; i < chooser->choiceList()->count(); ++i) {
            const QListWidgetItem* item = chooser->choiceList()->item(i);
            if (!(item->flags() & Qt::ItemIsEnabled)) {
                ++disabled;
                QVERIFY(item->text().contains(QStringLiteral("Cannot be taken:")));
            }
            namesIpad = namesIpad || item->text().contains(QStringLiteral("iPad"));
            QVERIFY(item->text().startsWith(QStringLiteral("Receiver ")));
        }
        QVERIFY(disabled >= 1);
        QVERIFY(namesIpad);
        QVERIFY(chooser->pickedChoice() >= 0);
        QVERIFY(chooser->takeButton()->isEnabled());
        QTest::mouseClick(chooser->takeButton(), Qt::LeftButton);
        QTRY_VERIFY(core.model->sliceOwnership()->ownedBy(b.key.fingerprint()).isEmpty());
        QTRY_VERIFY(!ofType(appB->received(), QStringLiteral("notice")).isEmpty());
        const QJsonObject told = ofType(appB->received(), QStringLiteral("notice")).last();
        QCOMPARE(told.value(QStringLiteral("kind")).toString(), QStringLiteral("receiverTaken"));
    }

    // Slice control plan Task 10: the hosting desktop's Add at full
    // capacity asks with the chooser a remote window shows, and Take sends
    // the pick as the station device's answer.
    void theHostingDesktopsAddAsksWithTheSameChooser()
    {
        Core core;
        core.model->configureStreamPool(2, 5, 192000);
        core.model->sliceById(0)->setFrequency(7074000.0);
        const QByteArray& station = SliceOwnership::stationDevice();
        core.server->deviceSessions()->registerHostingDevice(station, QStringLiteral("Mac"),
                                                              QStringLiteral("Mac"));
        core.server->setStationDeviceWords(QStringLiteral("Mac"), QStringLiteral("Mac"));
        SliceOwnership* ownership = core.model->sliceOwnership();
        ownership->adoptUnowned(station);
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"), QStringLiteral("iPad"));
        core.pair(b);
        LoopbackTransport* appB = core.signIn(
            b, {{"deviceAuth", 1}, {"sessionHolder", 1}, {"sliceAccess", 1}});
        QVERIFY(admitted(appB));
        const int bSlice = ownership->ownedBy(b.key.fingerprint()).first();
        core.model->sliceById(bSlice)->setFrequency(14074000.0);

        HostingSliceActions host(core.server.get(), core.model.get());
        QSignalSpy asked(&host, &HostingSliceActions::question);
        QSignalSpy finished(&host, &HostingSliceActions::finished);
        host.addOnPan(QStringLiteral("pan-host-2"));
        QTRY_COMPARE(asked.count(), 1);
        const SessionMessage question = asked.first().at(0).value<SessionMessage>();
        std::function<qint64()> choice;
        QWidget parent;
        QDialog* dialog = MultiDeviceController::questionDialog(question.prompt, &parent, &choice);
        auto* chooser = qobject_cast<TakeReceiverDialog*>(dialog);
        QVERIFY(chooser != nullptr);
        QVERIFY(chooser->choiceList()->count() >= 2);
        bool namesIpad = false;
        for (int i = 0; i < chooser->choiceList()->count(); ++i) {
            const QString text = chooser->choiceList()->item(i)->text();
            QVERIFY(OperatorWording::isPlain(text));
            namesIpad = namesIpad || text.contains(QStringLiteral("iPad"));
        }
        QVERIFY(namesIpad);
        QVERIFY(chooser->pickedChoice() >= 0);
        QObject::connect(dialog, &QDialog::accepted, &host, [&host, &question, &choice]() {
            host.proceed(question.prompt.id, choice());
        });
        QTest::mouseClick(chooser->takeButton(), Qt::LeftButton);
        QTRY_VERIFY(!finished.isEmpty()
                    && finished.last().at(0).toByteArray() == QByteArrayLiteral("confirm.proceed"));
        QVERIFY2(finished.last().at(2).toBool(), qPrintable(finished.last().at(3).toString()));
        bool onNewPan = false;
        for (int id : ownership->ownedBy(station)) {
            onNewPan = onNewPan
                       || core.model->sliceById(id)->panKey() == QStringLiteral("pan-host-2");
        }
        QVERIFY(onNewPan);
        delete dialog;
    }

    // Slice control plan Task 11: a hosting window filling its empty pans
    // (MainWindow::populatePanSlices, the path a layout change and connect
    // take) asks as the station device, so the new slice is the station's
    // and the other device's slice is left as it was.
    void aHostingWindowsEmptyPanGetsAStationSlice()
    {
        Core core;
        core.model->configureStreamPool(4, 5, 192000);
        core.model->sliceById(0)->setFrequency(7074000.0);
        const QByteArray& station = SliceOwnership::stationDevice();
        core.server->deviceSessions()->registerHostingDevice(station, QStringLiteral("Mac"),
                                                              QStringLiteral("Mac"));
        core.server->setStationDeviceWords(QStringLiteral("Mac"), QStringLiteral("Mac"));
        SliceOwnership* ownership = core.model->sliceOwnership();
        ownership->adoptUnowned(station);
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"), QStringLiteral("iPad"));
        core.pair(b);
        LoopbackTransport* appB = core.signIn(
            b, {{"deviceAuth", 1}, {"sessionHolder", 1}, {"sliceAccess", 1}});
        QVERIFY(admitted(appB));
        const QByteArray bKey = b.key.fingerprint();
        const int bSlice = ownership->ownedBy(bKey).first();
        core.model->sliceById(bSlice)->setFrequency(14074000.0);
        const QString bPan = core.model->sliceById(bSlice)->panKey();
        const int stationBefore = ownership->ownedBy(station).size();

        HostingSliceActions host(core.server.get(), core.model.get());
        MainWindow::populatePanSlices(core.model.get(), {QStringLiteral("pan-host-2")},
                                      false, false, &host);
        QTRY_COMPARE(ownership->ownedBy(station).size(), stationBefore + 1);
        bool onNewPan = false;
        for (int id : ownership->ownedBy(station)) {
            onNewPan = onNewPan
                       || core.model->sliceById(id)->panKey() == QStringLiteral("pan-host-2");
        }
        QVERIFY(onNewPan);
        QCOMPARE(ownership->ownedBy(bKey), QList<int>{bSlice});
        QCOMPARE(core.model->sliceById(bSlice)->panKey(), bPan);
        QCOMPARE(core.model->sliceById(bSlice)->frequency(), 14074000.0);
    }

    // Slice control plan Task 14b (ruling U5): the hosting desktop listens
    // to another device's slice. Its flag's "Your volume" and Mute, wired
    // as MainWindow::setFlagListenVolume wires them, set only the
    // station's own listening level: the slice's AF and mute, the
    // controller's audio and the other listener's level are unchanged. The
    // controller's AF at zero leaves the station's level where it was.
    void theHostingFlagsYourVolumeIsItsOwnLevel()
    {
        Core core;
        core.model->configureStreamPool(3, 5, 192000);
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"), QStringLiteral("iPad"));
        core.pair(b);
        LoopbackTransport* appB = core.signIn(
            b, {{"deviceAuth", 1}, {"sessionHolder", 1}, {"sliceAccess", 1}});
        QVERIFY(admitted(appB));
        const int shared = core.model->sliceOwnership()->ownedBy(b.key.fingerprint()).first();
        SliceModel* slice = core.model->sliceById(shared);
        slice->setAfGain(80);
        SliceAccessController* access = core.server->sliceAccessController();
        QVERIFY(access != nullptr);
        const QByteArray station = SliceOwnership::stationDevice();
        const SliceAccessController::Result joined =
            access->listen(station, core.model->sliceOwnership()->refOf(shared));
        QVERIFY2(joined.accepted, qPrintable(joined.reason));
        const SliceAccessController::ListenLevel bBefore =
            access->listenLevel(b.key.fingerprint(), shared);

        VfoWidget flag;
        flag.setSliceIndex(shared);
        flag.setAfGain(slice->afGain());
        QObject::connect(&flag, &VfoWidget::afGainChanged, slice, &SliceModel::setAfGain);
        QObject::connect(&flag, &VfoWidget::muteChanged, slice, &SliceModel::setMuted);
        QObject::connect(&flag, &VfoWidget::listenVolumeRequested, &flag,
                         [&core, access, station](int id, int level, bool muted) {
            access->setListenLevel(station, core.model->sliceOwnership()->refOf(id),
                                   level / 100.0, muted);
        });
        VfoWidget::SliceAccess listened;
        listened.state = VfoWidget::SliceAccess::State::Listening;
        listened.line = QStringLiteral("Listening · controlled by iPad");
        listened.heldReason = QStringLiteral("iPad controls this slice");
        const SliceAccessController::ListenLevel seeded = access->listenLevel(station, shared);
        flag.setListenVolume(static_cast<int>(std::lround(seeded.level * 100.0)), seeded.muted);
        flag.setSliceAccess(listened);
        QCOMPARE(flag.afNameForTest(), QStringLiteral("Your volume"));
        QCOMPARE(flag.afSliderForTest()->value(), 80);

        QSignalSpy af(slice, &SliceModel::afGainChanged);
        QSignalSpy mute(slice, &SliceModel::mutedChanged);
        flag.afSliderForTest()->setValue(25);
        flag.muteButtonForTest()->setChecked(true);

        SliceAccessController::ListenLevel mine = access->listenLevel(station, shared);
        QVERIFY(qAbs(mine.level - 0.25) < 1e-9);
        QVERIFY(mine.muted);
        QCOMPARE(af.count(), 0);
        QCOMPARE(mute.count(), 0);
        QCOMPARE(slice->afGain(), 80);
        QVERIFY(!slice->muted());
        const SliceAccessController::ListenLevel bAfter =
            access->listenLevel(b.key.fingerprint(), shared);
        QCOMPARE(bAfter.level, bBefore.level);
        QCOMPARE(bAfter.muted, bBefore.muted);

        // The controller turns its AF to zero: the station's level stays.
        flag.muteButtonForTest()->setChecked(false);
        slice->setAfGain(0);
        flag.setAfGain(0);
        mine = access->listenLevel(station, shared);
        QVERIFY(qAbs(mine.level - 0.25) < 1e-9);
        QVERIFY(!mine.muted);
        QCOMPARE(flag.afSliderForTest()->value(), 25);
    }

    // Slice control plan Task 15 (rulings U5, U6, U7): the RX applet on a
    // slice this window listens to. Tabs A (controlled here) and B
    // (listened); B's shared controls are shown disabled with the reason
    // naming the controlling device, and the applet has no volume or mute.
    void theRxAppletOnAListenedSliceShowsWhoControlsIt()
    {
        SliceModel a(0);
        SliceModel b(1);
        b.setFrequency(14225000.0);
        b.setDspMode(DSPMode::USB);
        RxApplet applet(&b, nullptr);
        VfoWidget::SliceAccess controlled;
        controlled.state = VfoWidget::SliceAccess::State::Controlled;
        controlled.line = QStringLiteral("You control this slice");
        VfoWidget::SliceAccess listened;
        listened.state = VfoWidget::SliceAccess::State::Listening;
        listened.line = QStringLiteral("Listening · controlled by iPad");
        listened.heldReason = QStringLiteral("iPad controls this slice");
        applet.setSliceTabAccess({{0, controlled}, {1, listened}});
        applet.setSliceAccess(listened);
        applet.updateSliceButtons({&a, &b}, 1);
        QVERIFY(applet.isListening());
        const QList<QWidget*> held = applet.heldControlsForTest();
        QVERIFY(!held.isEmpty());
        for (QWidget* control : held) {
            QVERIFY(!control->isEnabled());
            QCOMPARE(control->toolTip(), listened.heldReason);
        }
        QVERIFY(applet.sliceTabToolTipForTest(1).contains(QStringLiteral("iPad")));
        applet.resize(300, applet.sizeHint().height());
        saveShot(&applet, QStringLiteral("task15-rx-applet-listening"), false);
    }

    // A window whose last slice another device takes stays connected with
    // an empty band, a card with Take it back, and a pan that offers a take.
    void aWindowThatLosesItsLastSliceStaysConnectedWithAnEmptyBand()
    {
        Core core;
        core.model->configureStreamPool(2, 5, 192000);
        core.model->sliceById(0)->setFrequency(7074000.0);
        Window w;
        QVERIFY(core.server->deviceStore()->add(w.record()));
        QVERIFY(w.connectTo(core));
        QTRY_VERIFY(w.client.sessionHolderAvailable());
        QTRY_COMPARE(w.remote.slices().size(), 1);
        MultiDeviceController controller(&w.client, &w.host);
        controller.setNoticeHost(&w.host);

        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"), QStringLiteral("iPad"));
        core.pair(b);
        LoopbackTransport* appB = core.signIn(b);
        QVERIFY(admitted(appB));
        // B's slice on the other receiver; its next slice finds none free.
        const int bSlice = core.model->sliceOwnership()->ownedBy(b.key.fingerprint()).first();
        core.model->sliceById(bSlice)->setFrequency(14074000.0);
        const QJsonObject refused = core.invoke(appB, "addSliceOnPan",
                                                {utf8("panId", QStringLiteral("pan-b"))});
        QCOMPARE(refused.value(QStringLiteral("accepted")).toBool(true), false);
        const QJsonObject ask = firstOfType(appB->received(), QStringLiteral("confirm.request"));
        QCOMPARE(ask.value(QStringLiteral("kind")).toString(), QStringLiteral("takeReceiver"));
        qint64 choice = -1;
        for (const QJsonValue& v : ask.value(QStringLiteral("choices")).toArray()) {
            if (v.toObject().value(QStringLiteral("takeable")).toBool()) {
                choice = v.toObject().value(QStringLiteral("choice")).toInteger();
            }
        }
        QVERIFY2(choice >= 0, QJsonDocument(ask).toJson().constData());
        core.invoke(appB, "confirm.proceed",
                    {int64("id", ask.value(QStringLiteral("id")).toInteger()),
                     int64("choice", choice)});
        QTRY_VERIFY(w.remote.slices().isEmpty());
        QVERIFY(w.client.isHandshakeComplete());
        QVERIFY(w.client.isConnectionActive());
        QTRY_COMPARE(controller.noticeCards().size(), 1);
        QVERIFY(controller.noticeCards().first()->takeBackButton() != nullptr);

        // The pan's offer: the hint says why, and its button asks for a
        // slice there (which the Core answers with its chooser).
        PanadapterApplet pan(QStringLiteral("pan-0"));
        pan.resize(700, 300);
        pan.setNoSliceHintAllowed(true);
        pan.setTakeReceiverOffered(true);
        QCOMPARE(pan.visibleNoSliceHint(), PanadapterApplet::takeReceiverHintText());
        QVERIFY(OperatorWording::isPlain(PanadapterApplet::takeReceiverHintText()));
        QVERIFY(!pan.takeReceiverButton()->isHidden());
        QSignalSpy add(&pan, &PanadapterApplet::addSliceRequested);
        pan.takeReceiverButton()->click();
        QCOMPARE(add.count(), 1);
        QCOMPARE(add.first().first().toString(), QStringLiteral("pan-0"));
    }

    // The foreign marker: a dashed centre line in the slice's colour, a
    // hollow triangle, dashed grey edges with no fill, a label at the foot;
    // a click says whose it is and a drag moves nothing.
    void anotherDevicesMarkerIsDrawnDashedHollowAndUnfilled()
    {
        SpectrumWidget sw;
        sw.resize(800, 400);
        sw.setFrequencyRange(14'250'000.0, 96'000.0);
        sw.setConnectionState(ConnectionState::Connected);
        SpectrumWidget::ForeignSliceMarker m;
        m.sliceId = 1;
        m.centreHz = 14'250'000.0;
        m.filterLowHz = 200;
        m.filterHighHz = 12'000;
        m.color = VfoWidget::sliceColor(1);
        m.letter = QStringLiteral("B");
        m.ownerShortName = QStringLiteral("iPhone");
        m.ownerName = QStringLiteral("Jo's iPhone");
        m.tx = true;
        sw.setForeignSliceMarkers({m});
        QCOMPARE(SpectrumWidget::foreignMarkerLabel(m), QStringLiteral("B iPhone TX"));

        const QRect spec(0, 0, 800, 200);
        const QRect wf(0, 210, 800, 150);
        QImage img(800, 400, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::black);
        {
            QPainter p(&img);
            sw.drawForeignMarkersForTest(p, spec, wf);
        }
        const int x = 400;  // the centre at the middle of the span
        // Dashed: along the centre line some rows are coloured, some not.
        int lit = 0;
        int dark = 0;
        for (int y = 20; y < 150; ++y) {
            const QColor c = img.pixelColor(x, y);
            if (c.blue() > 120) { ++lit; } else { ++dark; }
        }
        QVERIFY2(lit > 10 && dark > 10, qPrintable(QStringLiteral("lit %1 dark %2").arg(lit).arg(dark)));
        // Hollow triangle: its outline is drawn, its middle is not filled.
        QVERIFY(img.pixelColor(x - 5, 1).blue() > 80);
        QCOMPARE(img.pixelColor(x - 2, 3), QColor(Qt::black));
        // No fill between the edges, away from the lines and the label.
        const int mid = 400 + (12'000 * 800 / 96'000) / 2;
        QCOMPARE(img.pixelColor(mid, 60), QColor(Qt::black));
        QCOMPARE(img.pixelColor(mid, 260), QColor(Qt::black));
        // The label sits at the foot of the spectrum.
        const QRect label = sw.foreignMarkerLabelRect(1);
        QVERIFY(label.isValid());
        QVERIFY(label.bottom() <= spec.bottom() && label.top() > spec.height() / 2);

        QSignalSpy clicked(&sw, &SpectrumWidget::foreignMarkerClicked);
        QSignalSpy tuned(&sw, &SpectrumWidget::frequencyClicked);
        QTest::mouseClick(&sw, Qt::LeftButton, Qt::NoModifier, label.center());
        QCOMPARE(clicked.count(), 1);
        // Desktop listening lane review (JJ, 2026-09-30): any slice can be
        // taken except while it transmits, so the click says who controls
        // it and offers Take control, in the listener words. It used to say
        // only the owner could tune or close it.
        QCOMPARE(clicked.first().at(1).toString(),
                 QStringLiteral("Slice B is controlled by Jo's iPhone. Take control to "
                                "change it."));
        QVERIFY(OperatorWording::isPlain(clicked.first().at(1).toString()));
        // A drag from the label moves nothing.
        QTest::mousePress(&sw, Qt::LeftButton, Qt::NoModifier, label.center());
        QTest::mouseMove(&sw, label.center() + QPoint(80, 0));
        QTest::mouseRelease(&sw, Qt::LeftButton, Qt::NoModifier, label.center() + QPoint(80, 0));
        QCOMPARE(tuned.count(), 0);
        QCOMPARE(sw.foreignSliceMarkers().first().centreHz, 14'250'000.0);
    }

    // The banner's words for who holds transmit.
    void theBadgeNamesTheHolder()
    {
        TransmitState tx;
        QVERIFY(!DeviceWords::holderBadge(tx, QStringLiteral("me")).shown);
        setTx(tx, "holderDeviceId", QStringLiteral("dev-b"));
        setTx(tx, "holderName", QStringLiteral("Jo's iPhone"));
        setTx(tx, "holderShortName", QStringLiteral("iPhone"));
        setTx(tx, "holderSource", QStringLiteral("device"));
        DeviceWords::HolderBadge b = DeviceWords::holderBadge(tx, QStringLiteral("me"));
        QVERIFY(b.shown);
        QCOMPARE(b.label, QStringLiteral("iPhone"));
        QCOMPARE(b.tone, DeviceWords::HolderBadge::Tone::Listening);
        setTx(tx, "keyed", true);
        QCOMPARE(DeviceWords::holderBadge(tx, QStringLiteral("me")).tone,
                 DeviceWords::HolderBadge::Tone::OnAir);
        setTx(tx, "keyed", false);
        setTx(tx, "holderAway", true);
        b = DeviceWords::holderBadge(tx, QStringLiteral("me"));
        QCOMPARE(b.tone, DeviceWords::HolderBadge::Tone::Away);
        QCOMPARE(b.label, QStringLiteral("iPhone, away"));
        setTx(tx, "holderAway", false);
        setTx(tx, "holderSource", QStringLiteral("radioPtt"));
        setTx(tx, "holderName", QStringLiteral("Radio"));
        QCOMPARE(DeviceWords::holderBadge(tx, QStringLiteral("me")).label, QStringLiteral("Radio"));
        setTx(tx, "holderTransferring", true);
        b = DeviceWords::holderBadge(tx, QStringLiteral("me"));
        QCOMPARE(b.label, QStringLiteral("changing hands"));
        QCOMPARE(b.tone, DeviceWords::HolderBadge::Tone::ChangingHands);
        setTx(tx, "holderTransferring", false);
        // This window's own hold is not named.
        QVERIFY(!DeviceWords::holderBadge(tx, QStringLiteral("dev-b")).shown);
        for (const QString& text : {b.toolTip, DeviceWords::holderBadge(tx, QStringLiteral("x")).toolTip}) {
            QVERIFY(OperatorWording::isPlain(text));
        }
    }

    // The pan's pill offers Take transmit, and a flag the radio's PTT
    // froze says so.
    void thePillOffersATakeAndTheFlagShowsTheRadiosFreeze()
    {
        SpectrumStatusOverlay overlay;
        overlay.resize(overlay.sizeHint());
        QVERIFY(!overlay.badgeRect(SpectrumStatusOverlay::Badge::Tx).isValid());
        overlay.setTakeTransmitOffered(true, QStringLiteral("iPhone"), true);
        overlay.resize(overlay.sizeHint());
        const QRect pill = overlay.badgeRect(SpectrumStatusOverlay::Badge::Tx);
        QVERIFY(pill.isValid());
        QVERIFY(overlay.toolTip().contains(QStringLiteral("iPhone is on the air")));
        QSignalSpy take(&overlay, &SpectrumStatusOverlay::takeTransmitClicked);
        QSignalSpy handoff(&overlay, &SpectrumStatusOverlay::txBadgeClicked);
        QTest::mouseClick(&overlay, Qt::LeftButton, Qt::NoModifier, pill.center());
        QCOMPARE(take.count(), 1);
        QCOMPARE(handoff.count(), 0);
        // This pan's own TX keeps its meaning.
        overlay.setTxBound(true);
        QTest::mouseClick(&overlay, Qt::LeftButton, Qt::NoModifier,
                          overlay.badgeRect(SpectrumStatusOverlay::Badge::Tx).center());
        QCOMPARE(handoff.count(), 1);
        QCOMPARE(take.count(), 1);

        VfoWidget flag;
        flag.setSliceIndex(0);
        flag.setTxSlice(true);
        QVERIFY(flag.txSliceShown());
        flag.setInUseByRadio(true);
        QVERIFY(!flag.txSliceShown());
        QCOMPARE(flag.findChild<QPushButton*>(QStringLiteral("VfoTxBadge"))->toolTip(),
                 VfoWidget::inUseByRadioText());
        flag.setTxSlice(true);
        QVERIFY(!flag.txSliceShown());
        flag.setInUseByRadio(false);
        flag.setTxSlice(true);
        QVERIFY(flag.txSliceShown());
    }

    // Every state rendered offscreen (NEREUS_TASK78_SHOTS keeps them).
    // Task 78 item 7 (G-53): a window that reaches a Core with four
    // devices on it is asked which one it replaces. Cancel leaves the Core
    // as it is; a choice lets the window in and the replaced device is told
    // who took its place.
    void aFullCoreAsksWhichDeviceToReplace()
    {
        Core core;
        Device devices[4];
        QList<LoopbackTransport*> apps;
        for (int i = 0; i < 4; ++i) {
            devices[i].name = QStringLiteral("Phone %1").arg(i + 1);
            core.pair(devices[i]);
            apps.append(core.signIn(devices[i]));
            QVERIFY(admitted(apps.last()));
        }
        Window w;
        QVERIFY(core.server->deviceStore()->add(w.record()));
        MultiDeviceController controller(&w.client, &w.host);
        RemoteConnectionController connection(&w.client, &w.remote, RemoteStationOptions{});

        // Cancel: the Core stays full and says so.
        w.startTo(core);
        auto* first = openDialogOf<ReplaceDeviceDialog>(controller);
        QVERIFY(first != nullptr);
        QCOMPARE(connection.statusText(), QStringLiteral("Core full, choose a device to replace"));
        QVERIFY(!w.client.isHandshakeComplete());
        first->cancelButton()->click();
        QTRY_VERIFY(!w.client.isConnectionActive());
        QVERIFY(!w.client.remoteDevices()->held());
        QCOMPARE(core.sessions().placesTaken(), 4);
        for (LoopbackTransport* app : std::as_const(apps)) {
            QVERIFY(app->isOpen());
        }

        // A choice: the list is the Core's four, each in plain words, and
        // the pick starts on the first that can be replaced.
        w.startTo(core);
        auto* dialog = openDialogOf<ReplaceDeviceDialog>(controller);
        QVERIFY(dialog != nullptr);
        QCOMPARE(dialog->deviceList()->count(), 4);
        QCOMPARE(dialog->deviceList()->currentRow(), 0);
        for (int i = 0; i < dialog->deviceList()->count(); ++i) {
            QVERIFY(OperatorWording::isPlain(dialog->deviceList()->item(i)->text()));
        }
        QVERIFY(OperatorWording::isPlain(dialog->intro()->text()));
        QCOMPARE(dialog->replaceButton()->text(), QStringLiteral("Replace"));
        dialog->deviceList()->setCurrentRow(1);
        const QString target = dialog->pickedDeviceId();
        QVERIFY(!target.isEmpty());
        dialog->replaceButton()->click();
        QTRY_VERIFY(w.client.isHandshakeComplete());
        QVERIFY(!w.client.remoteDevices()->held());
        QVERIFY(controller.openDialog() == nullptr);

        LoopbackTransport* replaced = nullptr;
        for (int i = 0; i < 4; ++i) {
            if (devices[i].id() == target) {
                replaced = apps.at(i);
            }
        }
        QVERIFY(replaced != nullptr);
        const QJsonObject end = endOf(replaced);
        QCOMPARE(end.value(QStringLiteral("code")).toString(), QStringLiteral("takenOver"));
        QCOMPARE(end.value(QStringLiteral("takenOverById")).toString(), w.id());
        QCOMPARE(core.sessions().placesTaken(), 4);
    }

    // Task 78 item 3 (G-53): the stop panel names the device that took this
    // window's place and when; Take it back reconnects into the list with
    // that device already picked.
    void theStopPanelNamesTheTakerAndTakeItBackStartsOnIt()
    {
        Core core;
        Window w;
        QVERIFY(core.server->deviceStore()->add(w.record()));
        QVERIFY(w.connectTo(core));
        Device others[3];
        for (int i = 0; i < 3; ++i) {
            others[i].name = QStringLiteral("Tablet %1").arg(i + 1);
            core.pair(others[i]);
            QVERIFY(admitted(core.signIn(others[i])));
        }
        Device fifth(QStringLiteral("Jo's iPhone"), QStringLiteral("phone"), QStringLiteral("iPhone"));
        core.pair(fifth);
        LoopbackTransport* app = core.signIn(fifth);
        QJsonObject held;
        verifyHeld(app, &held);
        RemoteConnectionController connection(&w.client, &w.remote, RemoteStationOptions{});
        app->sendText(SessionMessages::encode(SessionMessages::sessionTakeover(
            w.id(), static_cast<quint32>(held.value(QStringLiteral("revision")).toInteger()))));
        QTRY_VERIFY(admitted(app));
        QTRY_VERIFY(!w.client.isConnectionActive());

        const StationEndReport report = w.client.lastEndReport();
        QCOMPARE(report.kind, StationEndReport::Kind::TakenOver);
        QCOMPARE(report.takenOverByName, QStringLiteral("Jo's iPhone"));
        QCOMPARE(report.takenOverById, fifth.id());
        QVERIFY(report.endedAt.isValid());
        QVERIFY(qAbs(report.endedAt.secsTo(QDateTime::currentDateTime())) < 60);
        QCOMPARE(connection.stopNotice(), CoreStopNotice::TakenOver);
        const QString at = QLocale().toString(report.endedAt.time(), QLocale::ShortFormat);
        QVERIFY2(connection.stopText().startsWith(
                     QStringLiteral("Jo's iPhone took this window's place on the Core at %1.").arg(at)),
                 qPrintable(connection.stopText()));
        QVERIFY(OperatorWording::isPlain(connection.stopText()));
        QVERIFY(connection.offersTakeBack());
        CoreStopBanner banner(&connection, &w.host);
        banner.refresh();
        saveShot(&banner, QStringLiteral("stop-panel-named-takeover"));

        // Take it back: the Core is full, so it asks, starting on the taker.
        MultiDeviceController controller(&w.client, &w.host);
        w.startTo(core);
        auto* dialog = openDialogOf<ReplaceDeviceDialog>(controller);
        QVERIFY(dialog != nullptr);
        QCOMPARE(dialog->pickedDeviceId(), fifth.id());
        QVERIFY2(dialog->intro()->text().startsWith(
                     QStringLiteral("Jo's iPhone took this window's place just now.")),
                 qPrintable(dialog->intro()->text()));
        saveShot(dialog, QStringLiteral("replace-device-take-it-back"));
        dialog->replaceButton()->click();
        QTRY_VERIFY(w.client.isHandshakeComplete());
        QCOMPARE(endOf(app).value(QStringLiteral("code")).toString(), QStringLiteral("takenOver"));
    }

    // iPhone app plan Task 25: a remote window's This Core page lists the
    // Core's paired devices, revokes one, opens pairing and shows its code,
    // and records the key backup, through the Core's own verbs.
    void theThisCorePageManagesTheCoresDevices()
    {
        Core core;
        Window w;
        QVERIFY(core.server->deviceStore()->add(w.record()));
        Device phone(QStringLiteral("Jo's iPhone"), QStringLiteral("phone"), QStringLiteral("iPhone"));
        core.pair(phone);
        LoopbackTransport* app = core.signIn(phone);
        QVERIFY(admitted(app));
        ThisCorePage page(&w.remote);
        page.resize(760, 900);
        QVERIFY(!page.addDeviceButton()->isEnabled());
        QVERIFY(OperatorWording::isPlain(page.devicesUnavailableReason()));
        QVERIFY(w.connectTo(core));
        page.setStationSettingsAvailable(true, QString());
        QTRY_VERIFY(w.client.deviceAdminAvailable());
        QTRY_COMPARE(page.pairedRows()->findChildren<QPushButton*>(
                         QStringLiteral("thisCoreRevoke")).size(), 2);
        QVERIFY(page.devicesUnavailableReason().isEmpty());
        QPushButton* ownRevoke = nullptr;
        QPushButton* phoneRevoke = nullptr;
        for (QPushButton* b : page.pairedRows()->findChildren<QPushButton*>(
                 QStringLiteral("thisCoreRevoke"))) {
            if (b->property("deviceId").toString() == w.id()) { ownRevoke = b; }
            if (b->property("deviceId").toString() == phone.id()) { phoneRevoke = b; }
        }
        QVERIFY(ownRevoke != nullptr && phoneRevoke != nullptr);
        QVERIFY(!ownRevoke->isEnabled());
        QVERIFY(OperatorWording::isPlain(ownRevoke->toolTip()));
        QVERIFY(phoneRevoke->isEnabled());
        QCOMPARE(page.coreNameLabel()->text(), QStringLiteral("No Core name"));
        QVERIFY(!page.keyBackupButton()->isHidden());
        saveShot(&page, QStringLiteral("this-core-devices"), false);

        // Add a device: the Core opens pairing and the page shows its code.
        page.addDeviceButton()->click();
        QTRY_VERIFY(!page.pairingCodeLabel()->isHidden());
        QVERIFY(page.pairingCodeLabel()->text().startsWith(QStringLiteral("Pairing code: ")));
        QVERIFY(page.pairingCodeLabel()->text().size() > QStringLiteral("Pairing code: ").size());
        QVERIFY(!page.addDeviceButton()->isEnabled());
        saveShot(&page, QStringLiteral("this-core-pairing-code"), false);

        // Revoke: the Core drops the phone and it leaves the list. The rows
        // were rebuilt when the pairing window opened, so find it again.
        phoneRevoke = nullptr;
        for (QPushButton* b : page.pairedRows()->findChildren<QPushButton*>(
                 QStringLiteral("thisCoreRevoke"))) {
            if (b->property("deviceId").toString() == phone.id()) { phoneRevoke = b; }
        }
        QVERIFY(phoneRevoke != nullptr);
        phoneRevoke->click();
        QTRY_VERIFY(!app->isOpen());
        QVERIFY(!core.server->deviceStore()->find(phone.key.fingerprint()));
        QTRY_COMPARE(page.pairedRows()->findChildren<QPushButton*>(
                         QStringLiteral("thisCoreRevoke")).size(), 1);
        QVERIFY(page.devicesStatusLabel()->text().isEmpty());

        // The key backup, recorded on the Core.
        QVERIFY(page.keyBackupButton()->isEnabled());
        page.keyBackupButton()->click();
        QTRY_COMPARE(page.keyBackupLabel()->text(), QStringLiteral("The Core's key is backed up."));
        QVERIFY(core.server->devicesFacade()->keyBackupAcknowledged());
        QVERIFY(page.keyBackupButton()->isHidden());
    }

    // The fifth-device list's words: the desktop that runs the Core is shown
    // and cannot be chosen, the one on the air turns the button red, and a
    // place freed after time away is said.
    void theReplaceListShowsTheHostAndTurnsRedOnTheAir()
    {
        const auto entry = [](const QString& id, const QString& name, const QString& state,
                              bool replaceable) {
            RemoteHeldEntry e;
            e.device.deviceId = id;
            e.device.name = name;
            e.device.shortName = name.section(QLatin1Char(' '), -1);
            e.device.state = state;
            e.device.connectedForSeconds = 3900;
            e.device.lastActivitySeconds = 420;
            e.device.awayForSeconds = state == QStringLiteral("away") ? 95 : 0;
            e.device.transmittingForSeconds = state == QStringLiteral("transmitting") ? 130 : 0;
            RemoteDeviceSlice slice;
            slice.sliceId = 1;
            slice.letter = QStringLiteral("B");
            slice.band = static_cast<int>(Band::Band20m);
            slice.frequencyHz = 14074000.0;
            if (state == QStringLiteral("transmitting")) {
                e.device.holdsTransmit = true;
                e.device.transmittingOn = slice;
            } else if (state == QStringLiteral("listening")) {
                e.device.listeningOn = {slice};
            }
            e.replaceable = replaceable;
            return e;
        };
        RemoteHeldList held;
        held.revision = 3;
        held.placeFreedSecondsAgo = 200;
        held.entries = {entry(QStringLiteral("a"), QStringLiteral("Jo's iPad"), QStringLiteral("away"), true),
                        entry(QStringLiteral("h"), QStringLiteral("Shack Mac mini"), QStringLiteral("listening"), false),
                        entry(QStringLiteral("l"), QStringLiteral("Den MacBook"), QStringLiteral("listening"), true),
                        entry(QStringLiteral("t"), QStringLiteral("Jo's iPhone"), QStringLiteral("transmitting"), true)};
        ReplaceDeviceDialog dialog(held);
        QVERIFY(dialog.intro()->text().startsWith(
            QStringLiteral("This window's place was freed after 3 minutes away, 3 minutes ago.")));
        QCOMPARE(dialog.pickedDeviceId(), QStringLiteral("a"));
        QVERIFY(!(dialog.deviceList()->item(1)->flags() & Qt::ItemIsEnabled));
        QVERIFY(dialog.deviceList()->item(1)->text().contains(
            QStringLiteral("Runs the Core, so it cannot be replaced.")));
        QVERIFY(dialog.deviceList()->item(3)->text().contains(
            QStringLiteral("On the air for 2 minutes on slice B 20m 14.074 MHz")));
        for (int i = 0; i < dialog.deviceList()->count(); ++i) {
            QVERIFY(OperatorWording::isPlain(dialog.deviceList()->item(i)->text()));
        }
        saveShot(&dialog, QStringLiteral("replace-device-list"));
        dialog.deviceList()->setCurrentRow(3);
        QCOMPARE(dialog.replaceButton()->text(), QStringLiteral("Unkey and replace"));
        QVERIFY(!dialog.replaceButton()->styleSheet().isEmpty());
        saveShot(&dialog, QStringLiteral("replace-device-on-the-air"));
        // A newer list keeps the pick.
        held.revision = 4;
        dialog.setHeld(held);
        QCOMPARE(dialog.pickedDeviceId(), QStringLiteral("t"));
    }

    void screenshots()
    {
        // The dialogs.
        TakeTransmitDialog::Holder radio{QStringLiteral("Radio"), QStringLiteral("Radio"), false,
                                         false, 0};
        TakeTransmitDialog plain(radio);
        saveShot(&plain, QStringLiteral("take-transmit-from-radio"));
        TakeTransmitDialog::Holder keyed{QStringLiteral("Jo's iPhone"), QStringLiteral("iPhone"),
                                         true, false, 0};
        TakeTransmitDialog red(keyed);
        saveShot(&red, QStringLiteral("take-transmit-red"));

        SessionPrompt shared;
        shared.id = 7;
        shared.kind = QStringLiteral("sharedSetting");
        shared.change = QJsonObject{{QStringLiteral("label"), QStringLiteral("Attenuator, ADC 1")},
                                    {QStringLiteral("from"), QStringLiteral("0 dB")},
                                    {QStringLiteral("to"), QStringLiteral("20 dB")}};
        shared.affected = QJsonArray{QJsonObject{
            {QStringLiteral("deviceName"), QStringLiteral("Jo's iPhone")},
            {QStringLiteral("state"), QStringLiteral("listening")},
            {QStringLiteral("slices"),
             QJsonArray{QJsonObject{{QStringLiteral("sliceId"), 1},
                                    {QStringLiteral("letter"), QStringLiteral("B")},
                                    {QStringLiteral("frequencyHz"), 14074000.0},
                                    {QStringLiteral("mode"), 1},
                                    {QStringLiteral("streamIndex"), 1},
                                    {QStringLiteral("effect"), QStringLiteral("changes")}}}}}};
        ConfirmChangeDialog confirm(shared);
        saveShot(&confirm, QStringLiteral("confirm-shared-setting"));

        SessionPrompt take;
        take.id = 8;
        take.kind = QStringLiteral("takeReceiver");
        take.choices = QJsonArray{
            QJsonObject{{QStringLiteral("choice"), 0}, {QStringLiteral("streamIndex"), 0},
                        {QStringLiteral("takeable"), false},
                        {QStringLiteral("why"), QStringLiteral("Your panadapter already uses this receiver.")},
                        {QStringLiteral("slices"), QJsonArray{QJsonObject{
                             {QStringLiteral("sliceId"), 0}, {QStringLiteral("letter"), QStringLiteral("A")},
                             {QStringLiteral("frequencyHz"), 7074000.0}, {QStringLiteral("mode"), 1},
                             {QStringLiteral("deviceName"), QStringLiteral("Shack MacBook")}}}},
                        {QStringLiteral("devices"), QJsonArray{QJsonObject{
                             {QStringLiteral("name"), QStringLiteral("Shack MacBook")},
                             {QStringLiteral("state"), QStringLiteral("listening")}}}}},
            QJsonObject{{QStringLiteral("choice"), 1}, {QStringLiteral("streamIndex"), 1},
                        {QStringLiteral("takeable"), true},
                        {QStringLiteral("slices"), QJsonArray{QJsonObject{
                             {QStringLiteral("sliceId"), 1}, {QStringLiteral("letter"), QStringLiteral("B")},
                             {QStringLiteral("frequencyHz"), 14074000.0}, {QStringLiteral("mode"), 1},
                             {QStringLiteral("deviceName"), QStringLiteral("Jo's iPhone")},
                             {QStringLiteral("txSlice"), true}}}},
                        {QStringLiteral("devices"), QJsonArray{QJsonObject{
                             {QStringLiteral("name"), QStringLiteral("Jo's iPhone")},
                             {QStringLiteral("state"), QStringLiteral("listening")}}}}}};
        TakeReceiverDialog chooser(take);
        saveShot(&chooser, QStringLiteral("take-a-receiver"));
        for (int i = 0; i < chooser.choiceList()->count(); ++i) {
            QVERIFY(OperatorWording::isPlain(chooser.choiceList()->item(i)->text()));
        }

        // The notice card, with and without Take it back.
        RemotePrompt taken;
        taken.prompt.id = 3;
        taken.prompt.kind = QStringLiteral("transmitTaken");
        taken.prompt.takeBack = true;
        taken.reason = QStringLiteral("Jo's iPhone took transmit.");
        taken.receivedAt = QDateTime::currentDateTime();
        NoticeCard card(taken);
        card.resize(480, card.sizeHint().height());
        saveShot(&card, QStringLiteral("notice-take-it-back"));
        RemotePrompt grace;
        grace.prompt.id = 4;
        grace.prompt.kind = QStringLiteral("graceEnded");
        grace.reason = QStringLiteral("You were away for more than 3 minutes. Your slices are back.");
        grace.receivedAt = QDateTime::currentDateTime();
        NoticeCard graceCard(grace);
        QVERIFY(graceCard.takeBackButton() == nullptr);
        saveShot(&graceCard, QStringLiteral("notice-nothing-to-answer"));

        // The banner's holder chip in each state.
        struct { const char* name; const char* label; StatusBadge::Variant v; } chips[] = {
            {"badge-held-elsewhere", "iPhone", StatusBadge::Variant::Info},
            {"badge-on-the-air", "iPhone", StatusBadge::Variant::Tx},
            {"badge-away", "iPhone, away", StatusBadge::Variant::Warn},
            {"badge-changing-hands", "changing hands", StatusBadge::Variant::Info},
            {"badge-radio", "Radio", StatusBadge::Variant::Info},
        };
        for (const auto& c : chips) {
            StatusBadge chip;
            chip.setLabel(QString::fromLatin1(c.label));
            chip.setVariant(c.v);
            saveShot(&chip, QString::fromLatin1(c.name));
        }

        // The pill and the flag.
        SpectrumStatusOverlay overlay;
        overlay.setTakeTransmitOffered(true, QStringLiteral("iPhone"), true);
        overlay.resize(overlay.sizeHint());
        saveShot(&overlay, QStringLiteral("pan-take-tx-pill"));
        VfoWidget flag;
        flag.setSliceIndex(0);
        flag.setInUseByRadio(true);
        saveShot(&flag, QStringLiteral("flag-in-use-by-radio"));

        // Slice control plan Task 14a: a flag says who controls its slice.
        VfoWidget::SliceAccess controlled;
        controlled.state = VfoWidget::SliceAccess::State::Controlled;
        controlled.line = QStringLiteral("You control");
        VfoWidget::SliceAccess listened;
        listened.state = VfoWidget::SliceAccess::State::Listening;
        listened.line = QStringLiteral("Listening · controlled by Jo's iPhone");
        listened.heldReason = QStringLiteral("Jo's iPhone controls this slice");
        QVERIFY(OperatorWording::isPlain(listened.line));
        QVERIFY(OperatorWording::isPlain(listened.heldReason));
        VfoWidget flagControlled;
        flagControlled.setSliceIndex(0);
        flagControlled.setFrequency(14'074'000.0);
        flagControlled.setSliceAccess(controlled);
        saveShot(&flagControlled, QStringLiteral("flag-controlled"));
        VfoWidget flagListened;
        flagListened.setSliceIndex(1);
        flagListened.setFrequency(14'230'000.0);
        flagListened.setSliceAccess(listened);
        QVERIFY(flagListened.isListening());
        saveShot(&flagListened, QStringLiteral("flag-listened"));
        // Task 14b: the listened flag's audio tab, "Your volume" and Mute.
        VfoWidget flagYourVolume;
        flagYourVolume.setSliceIndex(1);
        flagYourVolume.setFrequency(14'230'000.0);
        flagYourVolume.setListenVolume(60, false);
        flagYourVolume.setSliceAccess(listened);
        QPushButton* audioTab = nullptr;
        for (QPushButton* button : flagYourVolume.findChildren<QPushButton*>()) {
            if (button->text() == QString::fromUtf8("\xF0\x9F\x94\x8A")) {
                audioTab = button;
                break;
            }
        }
        QVERIFY(audioTab != nullptr);
        audioTab->click();
        QCOMPARE(flagYourVolume.afNameForTest(), QStringLiteral("Your volume"));
        QVERIFY(flagYourVolume.afSliderForTest()->isEnabled());
        QVERIFY(flagYourVolume.muteButtonForTest()->isEnabled());
        QVERIFY(OperatorWording::isPlain(flagYourVolume.afNameForTest()));
        QVERIFY(OperatorWording::isPlain(flagYourVolume.afSliderForTest()->toolTip()));
        QVERIFY(OperatorWording::isPlain(flagYourVolume.muteButtonForTest()->toolTip()));
        saveShot(&flagYourVolume, QStringLiteral("flag-listened-your-volume"));
        VfoWidget flagOnAir;
        flagOnAir.setSliceIndex(1);
        flagOnAir.setFrequency(14'230'000.0);
        flagOnAir.setSliceAccess(listened);
        flagOnAir.setTxSlice(true);
        QVERIFY(flagOnAir.txSliceShown());
        saveShot(&flagOnAir, QStringLiteral("flag-listened-on-air"));

        // Two flags on one panadapter: this window controls A and listens
        // to B; they stack by today's rule.
        SpectrumWidget stackPan;
        stackPan.resize(800, 400);
        stackPan.setFrequencyRange(14'200'000.0, 96'000.0);
        VfoWidget* stackA = stackPan.addVfoWidget(0);
        VfoWidget* stackB = stackPan.addVfoWidget(1);
        QVERIFY(stackA && stackB);
        stackA->setFrequency(14'190'000.0);
        stackB->setFrequency(14'215'000.0);
        stackA->setSliceAccess(controlled);
        stackB->setSliceAccess(listened);
        stackPan.setVfoFrequency(14'190'000.0);
        stackPan.updateVfoPositions();
        QVERIFY(stackA->x() != stackB->x());
        saveShot(&stackPan, QStringLiteral("flags-stacked-on-one-pan"), false);

        // The markers on a panadapter.
        SpectrumWidget sw;
        sw.resize(800, 400);
        sw.setFrequencyRange(14'250'000.0, 96'000.0);
        SpectrumWidget::ForeignSliceMarker b;
        b.sliceId = 1;
        b.centreHz = 14'230'000.0;
        b.filterLowHz = 200;
        b.filterHighHz = 2800;
        b.color = VfoWidget::sliceColor(1);
        b.letter = QStringLiteral("B");
        b.ownerShortName = QStringLiteral("iPhone");
        b.tx = true;
        SpectrumWidget::ForeignSliceMarker c = b;
        c.sliceId = 2;
        c.centreHz = 14'270'000.0;
        c.color = VfoWidget::sliceColor(2);
        c.letter = QStringLiteral("C");
        c.ownerShortName = QStringLiteral("iPad");
        c.tx = false;
        c.away = true;
        sw.setForeignSliceMarkers({b, c});
        QImage img(800, 400, QImage::Format_ARGB32_Premultiplied);
        img.fill(QColor(0x0f, 0x0f, 0x1a));
        {
            QPainter p(&img);
            sw.drawForeignMarkersForTest(p, QRect(0, 0, 800, 200), QRect(0, 210, 800, 180));
        }
        if (!shotsDir().isEmpty()) {
            QVERIFY(img.save(QDir(shotsDir()).filePath(QStringLiteral("markers.png"))));
        }

        // The connected list, filled from a list as the Core sends it.
        RemoteDevicesState state;
        state.setSelfDeviceId(QStringLiteral("mac"));
        const QString list = QStringLiteral(
            "[{\"deviceId\":\"mac\",\"name\":\"Shack MacBook\",\"shortName\":\"MacBook\","
            "\"kind\":\"computer\",\"paired\":true,\"state\":\"listening\",\"holdsTransmit\":true,"
            "\"lastActivitySeconds\":20,\"connectedForSeconds\":3900,\"awayForSeconds\":0,"
            "\"transmittingForSeconds\":0,\"listeningOn\":[{\"sliceId\":0,\"letter\":\"A\","
            "\"band\":5,\"mode\":1}]},"
            "{\"deviceId\":\"ph\",\"name\":\"Jo's iPhone\",\"shortName\":\"iPhone\","
            "\"kind\":\"phone\",\"paired\":true,\"state\":\"away\",\"holdsTransmit\":false,"
            "\"lastActivitySeconds\":400,\"connectedForSeconds\":600,\"awayForSeconds\":95,"
            "\"transmittingForSeconds\":0,\"listeningOn\":[{\"sliceId\":1,\"letter\":\"B\","
            "\"band\":7,\"mode\":1}]}]");
        state.applyObject("connectedDevices",
                          {MirrorUpdate{0, "listJson", MirrorWireKind::Utf8, QVariant(list)},
                           MirrorUpdate{0, "deviceLimit", MirrorWireKind::Int64, QVariant(qint64(4))}});
        state.applyObject("devices",
                          {MirrorUpdate{0, "listJson", MirrorWireKind::Utf8,
                                        QVariant(QStringLiteral(
                                            "[{\"id\":\"mac\",\"name\":\"Shack MacBook\"},"
                                            "{\"id\":\"ph\",\"name\":\"Jo's iPhone\"},"
                                            "{\"id\":\"pad\",\"name\":\"iPad\",\"shortName\":\"iPad\"}]"))}});
        ConnectedDevicesList connected;
        connected.setDevices(&state);
        connected.resize(760, 220);
        QCOMPARE(connected.tree()->topLevelItemCount(), 4);  // 2 connected, header, 1 paired
        for (int i = 0; i < connected.tree()->topLevelItemCount(); ++i) {
            for (int col = 0; col < connected.tree()->columnCount(); ++col) {
                const QString cell = connected.tree()->topLevelItem(i)->text(col);
                QVERIFY2(cell.isEmpty() || OperatorWording::isPlain(cell), qPrintable(cell));
            }
        }
        saveShot(&connected, QStringLiteral("connected-list"), false);
    }
};

QTEST_MAIN(TstMultiDeviceScreens)
#include "tst_multi_device_screens.moc"
