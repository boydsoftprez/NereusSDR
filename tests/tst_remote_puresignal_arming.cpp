// no-port-check: NereusSDR-original. R-R3-49 (parity Task 7): PureSignal
// arming from a remote window. Loopback link, no RF and no hardware:
// nothing here keys a radio or starts a two-tone. "On the air" keys the
// Core's own MoxController with the receive-only MOX pre-check lifted, as
// tst_transmit_settings_gate and tst_remote_peripherals do. The Core's
// PureSignal runs against a test TxChannel (no WDSP channel).
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 7): a Core at
//                                    transmitSettingsVersion 7 takes a
//                                    window's Single Cal, Automatic, Apply
//                                    current correction, Restore and
//                                    PureSignal settings off the air and
//                                    refuses them on the air; the two-tone
//                                    test stays with remote transmit; the
//                                    window's controls follow. AI-assisted
//                                    via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  Checkpoint carry: the window signs in
//                                    to an upgraded Core with its token
//                                    (seedUpgradedCoreToken), as Part C's
//                                    paired-device sign-in requires.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  Fix wave GUI-I7: every greyed arming
//                                    and two-tone control says why.
//                                    AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QApplication>
#include <QCheckBox>
#include <QCoreApplication>
#include <QDoubleSpinBox>
#include <QFile>
#include <QPushButton>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <memory>

#include "core/AppSettings.h"
#include "core/ConnectionState.h"
#include "core/HardwareProfile.h"
#include "core/MoxController.h"
#include "core/PureSignal.h"
#include "core/TwoToneController.h"
#include "core/TxChannel.h"
#include "core/session/MirrorSchema.h"
#include "core/session/PureSignalSessionFacade.h"
#include "core/session/SessionMessages.h"
#include "core/session/StationCapabilities.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/settings/SettingsProxy.h"
#include "fakes/LoopbackTransport.h"
#include "fakes/UpgradedCoreToken.h"
#include "gui/PsForm.h"
#include "gui/applets/PureSignalApplet.h"
#include "gui/applets/TxApplet.h"
#include "gui/containers/ContainerButtonDispatcher.h"
#include "models/PureSignalSettings.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;
using NereusSDR::Test::LoopbackTransport;

namespace {

const QString kOnAir = QStringLiteral("The radio is on the air. Try again when it stops.");
const QString kNotYet = QStringLiteral("PureSignal cannot be run from a remote window.");

const QList<QByteArray> kArmingVerbs = {
    QByteArrayLiteral("ps3.single"), QByteArrayLiteral("ps3.automatic"),
    QByteArrayLiteral("ps3.applyCurrent"), QByteArrayLiteral("ps3.restoreCorrection")};

QList<MirrorUpdate> argumentsFor(const QByteArray& verb)
{
    if (verb == QByteArrayLiteral("ps3.restoreCorrection")) {
        return {{0, "assetId", MirrorWireKind::Utf8, QStringLiteral("correction")}};
    }
    if (verb == QByteArrayLiteral("ps3.twoTone")) {
        return {{0, "enabled", MirrorWireKind::Bool, true}};
    }
    return {};
}

std::unique_ptr<RadioModel> makeStationRadioModel()
{
    auto model = std::make_unique<RadioModel>();
    model->setBoardForTest(HPSDRHW::HermesLite);
    RadioInfo info;
    info.macAddress = QStringLiteral("AA:BB:CC:DD:EE:01");
    info.name = QStringLiteral("Bench HL2");
    info.boardType = HPSDRHW::HermesLite;
    model->setLastRadioInfoForTest(info);
    model->setConnectionStateForTest(ConnectionState::Connected);
    model->addSlice(QStringLiteral("pan-0"));
    return model;
}

// A receive-only Core with PureSignal on a test TxChannel, and one window.
struct Session {
    explicit Session(const QString& securityDir, QObject* parent)
        : settings(settingsDir.filePath(QStringLiteral("NereusSDR.settings")))
    {
        core = makeStationRadioModel();
        coordinator = core->installPureSignalForTest(&tx);
        coordinator->setTimersEnabled(false);
        server = std::make_unique<StationServer>(
            core.get(), settings, NereusSDR::Test::seedUpgradedCoreToken(securityDir));
        client = std::make_unique<StationClient>(&window, &proxy);
        coreEnd = new LoopbackTransport(QStringLiteral("station-end"), parent);
        windowEnd = new LoopbackTransport(QStringLiteral("client-end"), parent);
        coreEnd->linkTo(windowEnd);
    }
    bool connect()
    {
        QSignalSpy completed(client.get(), &StationClient::handshakeComplete);
        client->startSession(windowEnd, server->token());
        server->acceptTransport(coreEnd);
        return completed.wait(5000) || completed.count() == 1;
    }
    // The first command.result the Core sends for `id`.
    SessionMessage firstResult(quint32 id)
    {
        SessionMessage found;
        const bool arrived = QTest::qWaitFor([&] {
            for (const QByteArray& wire : windowEnd->received()) {
                SessionMessage message;
                if (SessionMessages::decode(wire, &message)
                    && message.kind == SessionMessageKind::CommandResult
                    && message.commandId == id) {
                    found = message;
                    return true;
                }
            }
            return false;
        }, 3000);
        if (!arrived) {
            found.reason = QStringLiteral("no command.result arrived");
        }
        return found;
    }
    // A raw command the way any app can send it.
    SessionMessage invoke(const QByteArray& verb)
    {
        const quint32 id = ++nextId;
        windowEnd->sendText(SessionMessages::encode(
            SessionMessages::commandInvoke(verb, id, argumentsFor(verb))));
        return firstResult(id);
    }
    // A raw pureSignalSettings write and the Core's answer for it.
    SessionPropertyResult writeSetting(const QByteArray& name, MirrorWireKind kind,
                                       const QVariant& value)
    {
        const quint32 writeId = ++nextId;
        windowEnd->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
            QByteArrayLiteral("pureSignalSettings"), {MirrorUpdate{4, name, kind, value}},
            writeId)));
        SessionPropertyResult found;
        const bool arrived = QTest::qWaitFor([&] {
            for (const QByteArray& wire : windowEnd->received()) {
                SessionMessage message;
                if (!SessionMessages::decode(wire, &message)
                    || message.kind != SessionMessageKind::PropertyResult
                    || message.writeId != writeId) {
                    continue;
                }
                for (const SessionPropertyResult& result : message.propertyResults) {
                    if (result.property == name) {
                        found = result;
                        return true;
                    }
                }
            }
            return false;
        }, 3000);
        if (!arrived) {
            found.reason = QStringLiteral("no property.result arrived");
        }
        return found;
    }
    void keyCore()
    {
        MoxController* const mox = core->moxController();
        mox->setMoxCheck({});
        mox->setMox(true);
    }
    void unkeyCore()
    {
        core->moxController()->setMox(false);
    }
    // Nothing keyed the radio or started a two-tone.
    bool coreStayedOffTheAir() const
    {
        return !core->moxController()->isMox() && !core->isTransmitting()
            && !core->transmitModel().isTune() && !core->transmitModel().isMox()
            && !(core->twoToneController() && core->twoToneController()->isActive());
    }

    TxChannel tx{/*channelId=*/1};
    QTemporaryDir settingsDir;
    AppSettings settings;
    std::unique_ptr<RadioModel> core;
    PureSignal* coordinator = nullptr;
    std::unique_ptr<StationServer> server;
    RadioModel window{RadioModel::Role::Remote};
    SettingsProxy proxy;
    std::unique_ptr<StationClient> client;
    LoopbackTransport* coreEnd = nullptr;
    LoopbackTransport* windowEnd = nullptr;
    quint32 nextId = 70000;
};

}  // namespace

class TstRemotePureSignalArming : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void coreOffersArmingAtVersionSeven();
    void receiveOnlyCorePermitsPureSignal();
    void armingTakenOffTheAirKeysNothing();
    void armingRefusedWhileOnTheAir();
    void twoToneStaysWithRemoteTransmit();
    void olderAppKeepsTodaysReason();
    void olderCoreKeepsTodaysGate();
    void settingsLiveOffTheAirAndRefusedOnIt();
    void psFormFollowsTheCore();
    void pureSignalAppletFollowsTheCore();
    void psaFollowsItsOwnGate();
    void everyGreyedArmingControlSaysWhy();

private:
    QTemporaryDir m_securityDir;
};

void TstRemotePureSignalArming::initTestCase()
{
    QVERIFY(m_securityDir.isValid());
    AppSettings::setProfileOverride(QStringLiteral("remote-puresignal-arming-%1")
                                        .arg(QCoreApplication::applicationPid()));
    AppSettings::instance().clear();
    AppSettings::instance().setValue(QStringLiteral("SettingsSchemaVersion"), QStringLiteral("7"));
}

void TstRemotePureSignalArming::cleanupTestCase()
{
    const QString path = AppSettings::instance().filePath();
    QFile::remove(path);
    QFile::remove(path + QStringLiteral(".bak"));
}

void TstRemotePureSignalArming::coreOffersArmingAtVersionSeven()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    // 8 since parity Task 13; 7 is within it.
    // 14 since the Core owns Prevent transmitting on a different band, 15
    // since a remote window edits the CFC bands.
    QCOMPARE(s.server->buildCapabilities().transmitSettingsVersion, 15);
    QVERIFY(s.client->transmitSettingsAvailable(7));
    QVERIFY(s.client->pureSignalArmingOffered());
    PureSignalSessionFacade* facade = s.window.pureSignalFacade();
    QTRY_VERIFY(facade->available());
    QTRY_VERIFY(facade->canArm());
    // The two-tone test keys the radio: it stays with remote transmit.
    QVERIFY(!facade->canActuate());
    QVERIFY(facade->armingRefusal().isEmpty());
    QVERIFY(facade->settingsRefusal().isEmpty());
}

void TstRemotePureSignalArming::receiveOnlyCorePermitsPureSignal()
{
    RadioModel core;
    core.setReceiveOnlyStationPolicy(true);
    QVERIFY(core.pureSignalOperationPermitted());
    TxChannel tx(/*channelId=*/1);
    PureSignal* coordinator = core.installPureSignalForTest(&tx);
    QVERIFY(coordinator->canActuate());
    // A remote window never runs PureSignal itself.
    RadioModel window(RadioModel::Role::Remote);
    QVERIFY(!window.pureSignalOperationPermitted());
}

void TstRemotePureSignalArming::armingTakenOffTheAirKeysNothing()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    PureSignalSessionFacade* facade = s.window.pureSignalFacade();
    QTRY_VERIFY(facade->canArm());
    QSignalSpy results(facade, &PureSignalSessionFacade::actionResult);

    // Automatic, from the window's facade as PsForm's Start Auto asks for
    // it: the Core's automatic calibration is armed and the window shows
    // it (PS-A lights from the mirrored preference).
    const quint32 automatic = facade->requestAction(Ps3Action::StartAutomatic);
    QVERIFY2(automatic != 0, qPrintable(facade->lastActionError()));
    QTRY_VERIFY(!results.isEmpty());
    QCOMPARE(results.first().at(0).toUInt(), automatic);
    QVERIFY(qvariant_cast<Ps3ActionPhase>(results.first().at(1)) != Ps3ActionPhase::Failed);
    QTRY_VERIFY(s.core->pureSignalSettings()->autoCalEnabled());
    QTRY_VERIFY(s.window.pureSignalSettings()->autoCalEnabled());
    int info[16] = {};
    s.coordinator->processNewInfo(info);
    s.coordinator->processNewInfo(info);
    QVERIFY(s.coordinator->isPsEnabled());
    QVERIFY(s.coreStayedOffTheAir());

    // Single Cal, Apply current correction and Restore reach the Core,
    // which takes them (they answer later on their own terms), not with
    // the remote window refusal.
    for (const QByteArray& verb : {QByteArrayLiteral("ps3.single"),
                                   QByteArrayLiteral("ps3.applyCurrent"),
                                   QByteArrayLiteral("ps3.restoreCorrection")}) {
        const SessionMessage reply = s.invoke(verb);
        QVERIFY2(reply.accepted, qPrintable(verb + ": " + reply.reason));
    }
    QVERIFY(s.coreStayedOffTheAir());
}

void TstRemotePureSignalArming::armingRefusedWhileOnTheAir()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    PureSignalSessionFacade* facade = s.window.pureSignalFacade();
    QTRY_VERIFY(facade->canArm());
    QSignalSpy started(s.coordinator, &PureSignal::calibrationStarted);

    s.keyCore();
    QTRY_VERIFY(s.window.isCoreOnAir());
    QVERIFY(!facade->canArm());
    QCOMPARE(facade->armingRefusal(), kOnAir);
    QCOMPARE(facade->settingsRefusal(), kOnAir);
    // The window does not send it.
    QCOMPARE(facade->requestAction(Ps3Action::Single), 0u);
    QCOMPARE(facade->lastActionError(), kOnAir);
    // An app that sends it anyway is refused by the Core.
    for (const QByteArray& verb : kArmingVerbs) {
        const SessionMessage reply = s.invoke(verb);
        QVERIFY2(!reply.accepted, verb.constData());
        QCOMPARE(reply.reason, kOnAir);
    }
    QCOMPARE(started.size(), 0);
    QVERIFY(!s.core->pureSignalSettings()->autoCalEnabled());

    // Back to receive: the window may arm again.
    s.unkeyCore();
    QTRY_VERIFY(!s.window.isCoreOnAir());
    QTRY_VERIFY(facade->canArm());
    QVERIFY(facade->armingRefusal().isEmpty());
}

void TstRemotePureSignalArming::twoToneStaysWithRemoteTransmit()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    PureSignalSessionFacade* facade = s.window.pureSignalFacade();
    QTRY_VERIFY(facade->canArm());
    QCOMPARE(facade->requestAction(Ps3Action::SetTwoTone, {{QStringLiteral("enabled"), true}}), 0u);
    QCOMPARE(facade->lastActionError(), kNotYet);
    const SessionMessage reply = s.invoke("ps3.twoTone");
    QVERIFY(!reply.accepted);
    // iPhone app plan Task 77 (ruling 8.3): the two-tone test is a key; on
    // this receive-only Core the transmit gate refuses it.
    QCOMPARE(reply.reason, QStringLiteral("This Core is set to receive only."));
    QVERIFY(s.coreStayedOffTheAir());
}

void TstRemotePureSignalArming::olderAppKeepsTodaysReason()
{
    // An app below minor 11 is never offered transmitSettingsVersion: the
    // Core refuses its arming as before, and keeps its settings writes
    // without replaying them.
    auto core = makeStationRadioModel();
    TxChannel tx(/*channelId=*/1);
    PureSignal* coordinator = core->installPureSignalForTest(&tx);
    coordinator->setTimersEnabled(false);
    QTemporaryDir dir;
    AppSettings settings(dir.filePath(QStringLiteral("older.settings")));
    StationServer server(core.get(), settings,
                         NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    auto* coreEnd = new LoopbackTransport(QStringLiteral("core"), this);
    auto* peer = new LoopbackTransport(QStringLiteral("older-app"), this);
    coreEnd->linkTo(peer);
    server.acceptTransport(coreEnd);
    peer->sendText(SessionMessages::encode(SessionMessages::hello(
        kSessionProtocolMajor, quint16(kRadioIdentitySessionProtocolMinor - 1), 0,
        QStringLiteral("older-app"))));
    peer->sendText(SessionMessages::encode(SessionMessages::authRequest(server.token())));
    const auto results = [peer](SessionMessageKind kind) {
        QList<SessionMessage> found;
        for (const QByteArray& wire : peer->received()) {
            SessionMessage message;
            if (SessionMessages::decode(wire, &message) && message.kind == kind) {
                found.append(message);
            }
        }
        return found;
    };
    QTRY_VERIFY(!results(SessionMessageKind::SnapshotComplete).isEmpty());
    QSignalSpy started(coordinator, &PureSignal::calibrationStarted);
    quint32 id = 500;
    for (const QByteArray& verb : kArmingVerbs) {
        ++id;
        const int before = results(SessionMessageKind::CommandResult).size();
        peer->sendText(SessionMessages::encode(
            SessionMessages::commandInvoke(verb, id, argumentsFor(verb))));
        QTRY_VERIFY(results(SessionMessageKind::CommandResult).size() > before);
        const SessionMessage reply = results(SessionMessageKind::CommandResult).last();
        QCOMPARE(reply.commandId, id);
        QVERIFY(!reply.accepted);
        QCOMPARE(reply.reason, kNotYet);
    }
    QCOMPARE(started.size(), 0);

    // Its settings write is kept, not replayed: automatic calibration is
    // not started on the Core.
    QVERIFY(coordinator->applyAcceptedSettingsToEngine());
    peer->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
        QByteArrayLiteral("pureSignalSettings"),
        {MirrorUpdate{0, "autoCalEnabled", MirrorWireKind::Bool, true}}, 77)));
    QTRY_VERIFY(core->pureSignalSettings()->autoCalEnabled());
    int info[16] = {};
    coordinator->processNewInfo(info);
    coordinator->processNewInfo(info);
    QVERIFY(!coordinator->isPsEnabled());
}

void TstRemotePureSignalArming::olderCoreKeepsTodaysGate()
{
    // A Core below transmitSettingsVersion 7: the window keeps today's
    // gate and reason.
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    PureSignalSessionFacade* facade = s.window.pureSignalFacade();
    QTRY_VERIFY(facade->canArm());
    StationCapabilities capabilities = s.server->buildCapabilities();
    capabilities.transmitSettingsVersion = 6;
    s.coreEnd->sendText(SessionMessages::encode(
        SessionMessages::capabilities(capabilities.toUpdates())));
    QTRY_VERIFY(!s.client->pureSignalArmingOffered());
    QTRY_VERIFY(!facade->canArm());
    QVERIFY(facade->available());
    // Fix wave GUI-I7: a reason whenever canArm() is false.
    QCOMPARE(facade->armingRefusal(), kNotYet);
    QVERIFY(facade->settingsRefusal().isEmpty());
    QCOMPARE(facade->requestAction(Ps3Action::StartAutomatic), 0u);
    QCOMPARE(facade->lastActionError(), kNotYet);
}

void TstRemotePureSignalArming::settingsLiveOffTheAirAndRefusedOnIt()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    QTRY_VERIFY(s.window.pureSignalFacade()->canArm());
    QVERIFY(s.coordinator->applyAcceptedSettingsToEngine());

    // Off the air: the window's change reaches the Core's PureSignal at
    // once, as a local window's does (PsForm's Auto-cal box arms it).
    s.window.pureSignalSettings()->setAutoCalEnabled(true);
    QTRY_VERIFY(s.core->pureSignalSettings()->autoCalEnabled());
    int info[16] = {};
    s.coordinator->processNewInfo(info);
    s.coordinator->processNewInfo(info);
    QVERIFY(s.coordinator->isPsEnabled());
    QVERIFY(s.coreStayedOffTheAir());

    const SessionPropertyResult taken =
        s.writeSetting("moxDelaySeconds", MirrorWireKind::Float64, 0.4);
    QVERIFY2(taken.accepted, qPrintable(taken.reason));
    QCOMPARE(s.core->pureSignalSettings()->moxDelaySeconds(), 0.4);

    // On the air: refused, the Core's value kept.
    s.keyCore();
    QTRY_VERIFY(s.core->isTransmitting());
    const SessionPropertyResult refused =
        s.writeSetting("moxDelaySeconds", MirrorWireKind::Float64, 0.6);
    QVERIFY(!refused.accepted);
    QCOMPARE(refused.reason, kOnAir);
    QCOMPARE(s.core->pureSignalSettings()->moxDelaySeconds(), 0.4);
    s.unkeyCore();
}

void TstRemotePureSignalArming::psFormFollowsTheCore()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    QTRY_VERIFY(s.window.pureSignalFacade()->canArm());
    PsForm form(&s.window, nullptr);
    auto* single = form.findChild<QPushButton*>(QStringLiteral("btnPSCalibrate"));
    auto* automatic = form.findChild<QPushButton*>(QStringLiteral("btnPSAutomatic"));
    auto* apply = form.findChild<QPushButton*>(QStringLiteral("btnPSApplyCurrent"));
    auto* twoTone = form.findChild<QPushButton*>(QStringLiteral("btnPSTwoToneGen"));
    auto* autoCal = form.findChild<QCheckBox*>(QStringLiteral("chkPSAutoCalEnabled"));
    auto* moxDelay = form.findChild<QDoubleSpinBox*>(QStringLiteral("udPSMoxDelay"));
    QVERIFY(single && automatic && apply && twoTone && autoCal && moxDelay);
    const QString singleTip = single->toolTip();
    const QString autoCalTip = autoCal->toolTip();

    QTRY_VERIFY(single->isEnabled());
    QVERIFY(automatic->isEnabled());
    QVERIFY(apply->isEnabled());
    QVERIFY(!twoTone->isEnabled());
    QVERIFY(autoCal->isEnabled());

    // Start Auto arms the Core.
    automatic->click();
    QTRY_VERIFY(s.core->pureSignalSettings()->autoCalEnabled());
    QVERIFY(s.coreStayedOffTheAir());

    s.keyCore();
    QTRY_VERIFY(!single->isEnabled());
    for (QWidget* control : {static_cast<QWidget*>(single), static_cast<QWidget*>(automatic),
                             static_cast<QWidget*>(apply), static_cast<QWidget*>(autoCal),
                             static_cast<QWidget*>(moxDelay)}) {
        QVERIFY(!control->isEnabled());
        QCOMPARE(control->toolTip(), kOnAir);
    }
    QVERIFY(!twoTone->isEnabled());

    s.unkeyCore();
    QTRY_VERIFY(single->isEnabled());
    QCOMPARE(single->toolTip(), singleTip);
    QVERIFY(autoCal->isEnabled());
    QCOMPARE(autoCal->toolTip(), autoCalTip);
    QVERIFY(!twoTone->isEnabled());
}

void TstRemotePureSignalArming::pureSignalAppletFollowsTheCore()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    QTRY_VERIFY(s.window.pureSignalFacade()->canArm());
    PureSignalApplet applet(&s.window);
    auto* calibrate = applet.findChild<QPushButton*>(QStringLiteral("PsAppletCalibrateBtn"));
    auto* autoCal = applet.findChild<QPushButton*>(QStringLiteral("PsAppletAutoCalBtn"));
    auto* twoTone = applet.findChild<QPushButton*>(QStringLiteral("PsAppletTwoToneBtn"));
    QVERIFY(calibrate && autoCal && twoTone);
    QTRY_VERIFY(calibrate->isEnabled());
    QVERIFY(autoCal->isEnabled());
    QVERIFY(!twoTone->isEnabled());

    QSignalSpy started(s.coordinator, &PureSignal::calibrationStarted);
    calibrate->click();
    QTRY_COMPARE(started.size(), 1);
    QVERIFY(s.coreStayedOffTheAir());

    s.keyCore();
    QTRY_VERIFY(!calibrate->isEnabled());
    QVERIFY(!autoCal->isEnabled());
    QCOMPARE(calibrate->toolTip(), kOnAir);
    QCOMPARE(autoCal->toolTip(), kOnAir);
    s.unkeyCore();
    QTRY_VERIFY(calibrate->isEnabled());
    QVERIFY(calibrate->toolTip() != kOnAir);
}

void TstRemotePureSignalArming::psaFollowsItsOwnGate()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    QTRY_VERIFY(s.window.pureSignalFacade()->canArm());

    // TX applet PS-A: the keying gate no longer holds it.
    TxApplet applet(&s.window);
    auto* psa = applet.findChild<QPushButton*>(QStringLiteral("TxAppletPsaBtn"));
    QVERIFY(psa);
    const QString remoteReason =
        QStringLiteral("Remote transmit controls are not available from this Core.");
    applet.setTransmitPermitted(false, remoteReason);
    applet.setPureSignalArmingPermitted(true);
    QVERIFY(psa->isEnabled());
    psa->click();
    QTRY_VERIFY(s.core->pureSignalSettings()->autoCalEnabled());
    QVERIFY(s.coreStayedOffTheAir());
    applet.setPureSignalArmingPermitted(false, kOnAir);
    QVERIFY(!psa->isEnabled());
    QCOMPARE(psa->toolTip(), kOnAir);
    applet.setPureSignalArmingPermitted(true);
    QVERIFY(psa->isEnabled());
    QVERIFY(psa->toolTip() != kOnAir);

    // The container's PS-A.
    bool permitted = true;
    QString reason;
    ContainerButtonDispatcher::Hooks hooks;
    hooks.transmitPermitted = [] { return false; };
    hooks.remoteTransmitReason = remoteReason;
    hooks.pureSignalArmingPermitted = [&permitted] { return permitted; };
    hooks.pureSignalArmingReason = [&reason] { return reason; };
    ContainerButtonDispatcher dispatcher(&s.window, std::move(hooks));
    using Id = ContainerButtonDispatcher::Id;
    QVERIFY(dispatcher.stateOf(Id::PsA, 0).available);
    permitted = false;
    reason = kOnAir;
    ContainerButtonDispatcher::State state = dispatcher.stateOf(Id::PsA, 0);
    QVERIFY(!state.available);
    QCOMPARE(state.reason, kOnAir);
    // A Core that does not offer arming keeps today's reason.
    reason.clear();
    state = dispatcher.stateOf(Id::PsA, 0);
    QVERIFY(!state.available);
    QCOMPARE(state.reason, remoteReason);
    // Its other transmit buttons keep the remote transmit gate.
    QVERIFY(!dispatcher.stateOf(Id::TwoTon, 0).available);
}

void TstRemotePureSignalArming::everyGreyedArmingControlSaysWhy()
{
    // Fix wave GUI-I7: a control that follows canArm or canActuate greys
    // with a reason, never a bare grey or another control's reason.
    const QString kNeedsRadio =
        QStringLiteral("PureSignal needs a connected radio that supports it.");

    // A station before its radio connects: no PureSignal to arm.
    {
        auto station = std::make_unique<RadioModel>();
        station->setBoardForTest(HPSDRHW::HermesLite);
        PureSignalSessionFacade* facade = station->pureSignalFacade();
        QVERIFY(facade);
        QVERIFY(!facade->available());
        QVERIFY(!facade->canArm());
        QCOMPARE(facade->armingRefusal(), kNeedsRadio);
        QCOMPARE(facade->twoToneRefusal(), kNeedsRadio);

        PureSignalApplet psApplet(station.get());
        auto* calibrate = psApplet.findChild<QPushButton*>(QStringLiteral("PsAppletCalibrateBtn"));
        auto* twoTone = psApplet.findChild<QPushButton*>(QStringLiteral("PsAppletTwoToneBtn"));
        QVERIFY(calibrate && twoTone);
        QVERIFY(!calibrate->isEnabled());
        QCOMPARE(calibrate->toolTip(), kNeedsRadio);
        QVERIFY(!twoTone->isEnabled());
        QCOMPARE(twoTone->toolTip(), kNeedsRadio);

        TxApplet txApplet(station.get());
        auto* psa = txApplet.findChild<QPushButton*>(QStringLiteral("TxAppletPsaBtn"));
        QVERIFY(psa);
        QVERIFY(!psa->isEnabled());
        QCOMPARE(psa->toolTip(), kNeedsRadio);
    }

    // A window whose Core is on the air: on the air, not "no radio".
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    PureSignalSessionFacade* facade = s.window.pureSignalFacade();
    QTRY_VERIFY(facade->canArm());
    TxApplet txApplet(&s.window);
    auto* psa = txApplet.findChild<QPushButton*>(QStringLiteral("TxAppletPsaBtn"));
    QVERIFY(psa);
    txApplet.setPureSignalArmingPermitted(true);
    QVERIFY(psa->isEnabled());
    const QString psaOwnTooltip = psa->toolTip();
    PureSignalApplet psApplet(&s.window);
    auto* twoTone = psApplet.findChild<QPushButton*>(QStringLiteral("PsAppletTwoToneBtn"));
    QVERIFY(twoTone);
    // The two-tone test keys the radio: this receive-only Core's window
    // may not transmit.
    QVERIFY(!twoTone->isEnabled());
    QCOMPARE(twoTone->toolTip(),
             QStringLiteral("The 2-tone test needs permission to transmit from the Core."));

    ContainerButtonDispatcher::Hooks hooks;
    hooks.transmitPermitted = [] { return false; };
    hooks.remoteTransmitReason =
        QStringLiteral("Remote transmit controls are not available from this Core.");
    hooks.pureSignalArmingPermitted = [] { return true; };
    hooks.pureSignalArmingReason = [] { return QString(); };
    ContainerButtonDispatcher dispatcher(&s.window, std::move(hooks));
    using Id = ContainerButtonDispatcher::Id;
    QVERIFY(dispatcher.stateOf(Id::PsA, 0).available);

    s.keyCore();
    QTRY_VERIFY(!facade->canArm());
    QTRY_VERIFY(!psa->isEnabled());
    QCOMPARE(psa->toolTip(), kOnAir);
    const ContainerButtonDispatcher::State state = dispatcher.stateOf(Id::PsA, 0);
    QVERIFY(!state.available);
    QCOMPARE(state.reason, kOnAir);

    s.unkeyCore();
    QTRY_VERIFY(psa->isEnabled());
    QCOMPARE(psa->toolTip(), psaOwnTooltip);
    QVERIFY(dispatcher.stateOf(Id::PsA, 0).available);
}

QTEST_MAIN(TstRemotePureSignalArming)
#include "tst_remote_puresignal_arming.moc"
