// no-port-check: NereusSDR-original. R-R3-49 (parity Task 1): the transmit
// settings gate. Loopback link, no RF and no hardware: nothing here keys a
// radio. "On the air" keys the Core's own MoxController (and its two-tone
// controller against a test TxChannel) with the receive-only MOX pre-check
// lifted, as tst_tgxl_station_identity and tst_remote_peripherals do.
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 1): a
//                                    receive-only Core takes a transmit
//                                    setting while its radio is off the
//                                    air, refuses it while it is on the
//                                    air, and still refuses the keying
//                                    set. A window's on-the-air state
//                                    follows the Core. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 2): the Core
//                                    offers transmitSettingsVersion 2.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 3): the Core offers
//                                    transmitSettingsVersion 3.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  Integration carry: the window signs in
//                                    to an upgraded Core with its token
//                                    (seedUpgradedCoreToken), as Part C's
//                                    paired-device sign-in requires.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  Parity Task 19 (R-IOS-25):
//                                    recordStreamVersion and the record
//                                    streams. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  An RX DSP > Options apply waits for
//                                    receive as the TX half does. AI-
//                                    assisted via Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  Remote parity on the air
//                                    (transmitSettingsVersion 13): the
//                                    transmit settings and DSP > Options
//                                    TX keys are taken on the air.
//                                    AI-assisted via Anthropic Claude Code.
// =================================================================

#include <tuple>
#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <memory>

#include "core/AppSettings.h"
#include "core/ConnectionState.h"
#include "core/HardwareProfile.h"
#include "core/MicProfileManager.h"
#include "core/MoxController.h"
#include "core/TwoToneController.h"
#include "core/TxChannel.h"
#include "core/session/IStationLink.h"
#include "core/session/MirrorSchema.h"
#include "core/session/PureSignalSessionFacade.h"
#include "core/session/SessionMessages.h"
#include "core/session/StationCapabilities.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/settings/SettingsProxy.h"
#include "fakes/LoopbackTransport.h"
#include "fakes/UpgradedCoreToken.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;
using NereusSDR::Test::LoopbackTransport;

namespace {

const QString kOnAir = QStringLiteral("The radio is on the air. Try again when it stops.");
const QString kReceiveOnly =
    QStringLiteral("Transmit configuration is unavailable on this receive-only Core.");

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

// A receive-only Core and one window, handshake complete.
struct Session {
    explicit Session(const QString& securityDir, QObject* parent)
        : settings(settingsDir.filePath(QStringLiteral("NereusSDR.settings")))
    {
        core = makeStationRadioModel();
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
    // A raw `transmit` write the way any app can send it, and the Core's
    // answer for it.
    SessionPropertyResult writeTransmit(const QByteArray& name, MirrorWireKind kind,
                                        const QVariant& value)
    {
        const quint32 writeId = ++m_nextWriteId;
        MirrorUpdate update;
        update.ordinal = 1;
        update.name = name;
        update.kind = kind;
        update.value = value;
        windowEnd->sendText(SessionMessages::encode(
            SessionMessages::propertyWrite(QByteArrayLiteral("transmit"), {update}, writeId)));
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

    // A command the way any app sends it, and the Core's answer.
    SessionMessage invoke(const QByteArray& verb, const QList<MirrorUpdate>& arguments)
    {
        const quint32 id = ++m_nextWriteId;
        windowEnd->sendText(SessionMessages::encode(
            SessionMessages::commandInvoke(verb, id, arguments)));
        SessionMessage found;
        found.reason = QStringLiteral("no command.result arrived");
        static_cast<void>(QTest::qWaitFor([&] {
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
        }, 3000));
        return found;
    }

    QTemporaryDir settingsDir;
    AppSettings settings;
    std::unique_ptr<RadioModel> core;
    std::unique_ptr<StationServer> server;
    RadioModel window{RadioModel::Role::Remote};
    SettingsProxy proxy;
    std::unique_ptr<StationClient> client;
    LoopbackTransport* coreEnd = nullptr;
    LoopbackTransport* windowEnd = nullptr;
    quint32 m_nextWriteId = 90000;
};

}  // namespace

class TstTransmitSettingsGate : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void coreOffersTransmitSettingsVersion();
    void olderCoreOffersNoTransmitSettings();
    void olderAppIsNotOfferedTransmitSettings();
    void keyingSetStaysRefusedOnAndOffTheAir();
    void settingAppliedOffTheAirAndReturnsToTheWindow();
    void settingTakenWhileOnTheAir();
    void settingVerbsAreTheStationKeysWhileItHoldsTransmit();
    void onAirRefusalIsTheTgxlRefusal();
    void dspOptionsTxKeyTakenOffTheAirAndApplied();
    void dspOptionsTxKeyTakenOnTheAirAndAppliedAtTheUnkey();
    void dspOptionsTxApplyWaitsForTheUnkey();
    void dspOptionsTxApplyWaitsForTwoToneToEnd();
    void dspOptionsRxApplyWaitsForReceive_data();
    void dspOptionsRxApplyWaitsForReceive();
    void paReloadWaitsWhileOnTheAir();
    void transmitHardwareKeysStayRefused();
    void windowOnAirFollowsTheCore();
    void windowOnAirClearsWhenTheSessionEnds();

private:
    QTemporaryDir m_securityDir;
};

void TstTransmitSettingsGate::initTestCase()
{
    QVERIFY(m_securityDir.isValid());
    const QString profile = QStringLiteral("transmit-settings-gate-%1")
                                .arg(QCoreApplication::applicationPid());
    AppSettings::setProfileOverride(profile);
    AppSettings::instance().clear();
    // As CoreInit's migrations leave it, so the window's settings proxy
    // starts as it does in the app.
    AppSettings::instance().setValue(QStringLiteral("SettingsSchemaVersion"), QStringLiteral("7"));
}

void TstTransmitSettingsGate::cleanupTestCase()
{
    const QString path = AppSettings::instance().filePath();
    QFile::remove(path);
    QFile::remove(path + QStringLiteral(".bak"));
}

void TstTransmitSettingsGate::coreOffersTransmitSettingsVersion()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    QVERIFY(s.core->receiveOnlyStationPolicy());
    // 2 since parity Task 2 (the TX and Phone/CW applets' settings), 3
    // since parity Task 3 (the microphone input and the TX profiles), 4
    // since parity Task 4 (TX EQ, CFC, phase rotator, CESSB, leveler, ALC),
    // 5 since parity Task 5 (Power, DEXP/VOX, Two-Tone IMD), 6 since
    // parity Task 6 (Setup > PA), 7 since parity Task 7 (PureSignal arming),
    // 8 since parity Task 13 (Hardware Config's OC transmit pins and
    // transmit calibration), 10 since iPhone app plan Task 40 (micMuted),
    // 11 since hardware parity batch B (Disable HF PA), 12 since addendum
    // G-42 (the Core's Extended transmit setting), 13 since the transmit
    // settings are taken on the air as a local window takes them, 14 since
    // Prevent TX'ing on a different band became the Core's setting, 15 since
    // a remote window edits the CFC bands (cfcProfile).
    QCOMPARE(s.server->buildCapabilities().transmitSettingsVersion, 15);
    QCOMPARE(s.client->capabilities().transmitSettingsVersion, 15);
    QVERIFY(s.client->transmitSettingsAvailable());
    QVERIFY(s.client->transmitSettingsAvailable(1));
    QVERIFY(s.client->transmitSettingsAvailable(2));
    QVERIFY(s.client->transmitSettingsAvailable(3));
    QVERIFY(s.client->transmitSettingsAvailable(4));
    QVERIFY(s.client->transmitSettingsAvailable(5));
    QVERIFY(s.client->transmitSettingsAvailable(6));
    QVERIFY(s.client->transmitSettingsAvailable(7));
    QVERIFY(s.client->transmitSettingsAvailable(8));
    QVERIFY(s.client->transmitSettingsAvailable(9));
    QVERIFY(s.client->transmitSettingsAvailable(10));
    QVERIFY(s.client->transmitSettingsAvailable(11));
    QVERIFY(s.client->transmitSettingsAvailable(12));
    QVERIFY(s.client->transmitSettingsAvailable(13));
    QVERIFY(s.client->transmitSettingsAvailable(14));
    QVERIFY(s.client->transmitSettingsAvailable(15));
    QVERIFY(!s.client->transmitSettingsAvailable(16));
    QVERIFY(StationServer::isTransmitSettingKeyAcceptedOffAir(
        QStringLiteral("DspOptionsBufferSizePhoneTx")));
    QVERIFY(StationServer::isTransmitSettingKeyAcceptedOffAir(
        QStringLiteral("DspOptionsFilterTypeDigTx")));
    QVERIFY(!StationServer::isTransmitSettingKeyAcceptedOffAir(
        QStringLiteral("DspOptionsBufferSizePhoneRx")));
    QVERIFY(!StationServer::isTransmitSettingKeyAcceptedOffAir(
        QStringLiteral("hardware/aa:bb:cc:dd:ee:01/tx/power")));
    QCOMPARE(IStationLink::transmitSettingsUnavailableReason(),
             QStringLiteral("This Core does not let this app change transmit settings. "
                            "Updating the Core may help."));
}

void TstTransmitSettingsGate::olderCoreOffersNoTransmitSettings()
{
    // A Core from before transmitSettingsVersion sends the minor-11 block
    // without it: the window reads 0 and keeps every transmit setting
    // greyed with the Core reason.
    StationCapabilities caps;
    caps.radioIdentityEntries = true;
    caps.remoteTgxlControlVersion = 3;
    caps.transmitSettingsVersion = 1;
    QList<MirrorUpdate> updates = caps.toUpdates();
    // Pin the original contiguous minor-11 block by name rather than by its
    // distance from the end: independent capabilities can follow it.
    QList<QByteArray> names;
    for (const MirrorUpdate& update : updates) { names.append(update.name); }
    const QList<QByteArray> originalBlock{
        "transmitSettingsVersion", "bandSelectVersion", "meterReadingsVersion",
        "dspInfoVersion", "recordStreamVersion", "stationRadiosVersion",
        "txDisplayVersion", "displayClockVersion", "controlChannelVersion",
        "txMonitorAudioVersion", "stationFreedvVersion", "mediaReplaceVersion",
        "controlSwitchVersion", "relayAllowed", "supportBundleVersion",
        "mediaTunnelVersion", "mediaRelayRoutingVersion", "remoteIqVersion",
        "txModMonitorVersion"};
    const qsizetype first = names.indexOf(QByteArrayLiteral("transmitSettingsVersion"));
    QVERIFY(first >= 0);
    QCOMPARE(names.mid(first, originalBlock.size()), originalBlock);
    // This fixture has no feature declarations; the optional Setup and mini
    // display entries stay absent. Accessory transmit is separately appended.
    QVERIFY(!names.contains(QByteArrayLiteral("setupDescriptionVersion")));
    QVERIFY(!names.contains(QByteArrayLiteral("miniDisplayVersion")));
    QCOMPARE(names.indexOf(QByteArrayLiteral("accessoryTxVersion")),
             first + originalBlock.size());
    QCOMPARE(updates.at(first + originalBlock.size()).kind, MirrorWireKind::Int64);
    QCOMPARE(updates.at(first + originalBlock.size()).value.toInt(), 0);
    while (updates.size() > first + 1) { updates.removeLast(); }
    QCOMPARE(StationCapabilities::fromUpdates(updates).transmitSettingsVersion, 1);
    updates.removeLast();
    // The iPhone app's display extras entry now comes just before it.
    QCOMPARE(updates.last().name, QByteArrayLiteral("displayExtrasVersion"));
    const StationCapabilities older = StationCapabilities::fromUpdates(updates);
    QCOMPARE(older.transmitSettingsVersion, 0);
    QCOMPARE(older.remoteTgxlControlVersion, 3);
}

void TstTransmitSettingsGate::olderAppIsNotOfferedTransmitSettings()
{
    // An app below minor 11 is never sent transmitSettingsVersion, so a
    // receive-only Core refuses its transmit writes as before.
    auto core = makeStationRadioModel();
    QTemporaryDir dir;
    AppSettings settings(dir.filePath(QStringLiteral("older.settings")));
    settings.setValue(QStringLiteral("DspOptionsBufferSizePhoneTx"), QStringLiteral("1024"));
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
    const auto received = [peer](SessionMessageKind kind) {
        QList<SessionMessage> found;
        for (const QByteArray& wire : peer->received()) {
            SessionMessage message;
            if (SessionMessages::decode(wire, &message) && message.kind == kind) {
                found.append(message);
            }
        }
        return found;
    };
    QTRY_VERIFY(!received(SessionMessageKind::SnapshotComplete).isEmpty());
    for (const SessionMessage& caps : received(SessionMessageKind::Capabilities)) {
        for (const MirrorUpdate& u : caps.updates) {
            QVERIFY(u.name != "transmitSettingsVersion");
        }
    }

    const int power = core->transmitModel().power();
    peer->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
        QByteArrayLiteral("transmit"),
        {MirrorUpdate{2, "power", MirrorWireKind::Int64, QVariant(qlonglong(power == 40 ? 41 : 40))}},
        77)));
    QTRY_VERIFY(!received(SessionMessageKind::PropertyResult).isEmpty());
    const SessionMessage result = received(SessionMessageKind::PropertyResult).first();
    QCOMPARE(result.propertyResults.size(), 1);
    QVERIFY(!result.propertyResults.first().accepted);
    QCOMPARE(result.propertyResults.first().reason, kReceiveOnly);
    QCOMPARE(core->transmitModel().power(), power);

    peer->sendText(SessionMessages::encode(SessionMessages::settingsWrite(
        QStringLiteral("DspOptionsBufferSizePhoneTx"), QStringLiteral("2048"),
        QStringLiteral("older-app"))));
    QTRY_VERIFY(!received(SessionMessageKind::SettingsReject).isEmpty());
    QCOMPARE(received(SessionMessageKind::SettingsReject).first().reason, kReceiveOnly);
    QCOMPARE(settings.value(QStringLiteral("DspOptionsBufferSizePhoneTx")).toString(),
             QStringLiteral("1024"));
}

void TstTransmitSettingsGate::keyingSetStaysRefusedOnAndOffTheAir()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    TransmitModel& coreTx = s.core->transmitModel();
    MoxController* const mox = s.core->moxController();
    QVERIFY(mox);

    const auto expectKeyingRefused = [&] {
        for (const QByteArray& name : {QByteArrayLiteral("mox"), QByteArrayLiteral("tune"),
                                       QByteArrayLiteral("voxEnabled"),
                                       QByteArrayLiteral("twoToneActive")}) {
            const SessionPropertyResult result =
                s.writeTransmit(name, MirrorWireKind::Bool, QVariant(true));
            QVERIFY2(!result.accepted, name.constData());
            QCOMPARE(result.reason, kReceiveOnly);
        }
    };
    // Off the air.
    expectKeyingRefused();
    if (QTest::currentTestFailed()) { return; }
    QVERIFY(!coreTx.isMox());
    QVERIFY(!coreTx.isTune());
    QVERIFY(!coreTx.voxEnabled());
    QVERIFY(!mox->isMox());

    // On the air (the Core's own MoxController, pre-check lifted).
    mox->setMoxCheck({});
    mox->setMox(true);
    QTRY_VERIFY(s.core->isTransmitting());
    expectKeyingRefused();
    if (QTest::currentTestFailed()) { return; }
    QVERIFY(!coreTx.isTune());
    QVERIFY(!coreTx.voxEnabled());
    mox->setMox(false);
    QTRY_VERIFY(mox->state() == MoxState::Rx);
}

void TstTransmitSettingsGate::settingAppliedOffTheAirAndReturnsToTheWindow()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    TransmitModel& coreTx = s.core->transmitModel();
    TransmitModel& windowTx = s.window.transmitModel();
    QVERIFY(!s.core->isTransmitting());

    // Through the window's own model, as the TX applet's RF Power slider.
    const int power = coreTx.power() == 37 ? 38 : 37;
    windowTx.setPower(power);
    QTRY_COMPARE(coreTx.power(), power);
    QTRY_COMPARE(windowTx.power(), power);

    // The TX passband, as the TX applet's spin boxes and Shift-click.
    windowTx.setFilterLow(200);
    windowTx.setFilterHigh(2700);
    QTRY_COMPARE(coreTx.filterLow(), 200);
    QTRY_COMPARE(coreTx.filterHigh(), 2700);
    QTRY_COMPARE(windowTx.filterLow(), 200);
    QTRY_COMPARE(windowTx.filterHigh(), 2700);

    // A raw write from any app gets an accepted result.
    const int again = power + 1;
    const SessionPropertyResult result =
        s.writeTransmit(QByteArrayLiteral("power"), MirrorWireKind::Int64,
                        QVariant(qlonglong(again)));
    QVERIFY2(result.accepted, qPrintable(result.reason));
    QCOMPARE(coreTx.power(), again);
}

// Remote parity (both ways): a local window changes its transmit settings
// while transmitting (no TX applet, Phone/CW, TX EQ, CFC or Setup transmit
// control is greyed under MOX, as in Thetis), so a receive-only Core takes
// them on the air too, from a peer offered the transmit settings. They key
// nothing; the keying set stays refused (keyingSetStaysRefusedOnAndOffTheAir).
void TstTransmitSettingsGate::settingTakenWhileOnTheAir()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    TransmitModel& coreTx = s.core->transmitModel();
    TransmitModel& windowTx = s.window.transmitModel();
    int next = coreTx.power() == 21 ? 22 : 21;

    const auto expectTaken = [&](const char* how) {
        const SessionPropertyResult result =
            s.writeTransmit(QByteArrayLiteral("power"), MirrorWireKind::Int64,
                            QVariant(qlonglong(next)));
        QVERIFY2(result.accepted, qPrintable(QString::fromLatin1(how) + QStringLiteral(": ")
                                             + result.reason));
        QCOMPARE(coreTx.power(), next);
        next = next == 21 ? 22 : 21;
        // The window's own write reaches the Core too. (The raw write above
        // is not echoed to this window, ruling 5.7, so its own value is
        // moved to one it does not hold.)
        const int own = windowTx.power() == 40 ? 41 : 40;
        windowTx.setPower(own);
        QTRY_COMPARE(coreTx.power(), own);
    };

    // TUNE (the Core's transmit model), before the pre-check is lifted.
    coreTx.setTune(true);
    expectTaken("tune");
    if (QTest::currentTestFailed()) { return; }
    coreTx.setTune(false);

    MoxController* const mox = s.core->moxController();
    QVERIFY(mox);
    mox->setMoxCheck({});
    // MOX through the Core's MoxController.
    mox->setMox(true);
    QTRY_VERIFY(mox->state() == MoxState::Tx);
    expectTaken("mox");
    if (QTest::currentTestFailed()) { return; }
    // Other transmit settings of every group, keyed.
    for (const auto& [name, kind, value] :
         {std::tuple<QByteArray, MirrorWireKind, QVariant>{"monEnabled", MirrorWireKind::Bool,
                                                           !coreTx.monEnabled()},
          {"voxThresholdDb", MirrorWireKind::Int64, qlonglong(-40)},
          {"txLevelerOn", MirrorWireKind::Bool, !coreTx.txLevelerOn()},
          {"cfcEnabled", MirrorWireKind::Bool, !coreTx.cfcEnabled()},
          {"tunePower", MirrorWireKind::Int64, qlonglong(coreTx.tunePower() == 10 ? 11 : 10)},
          {"txAlcDecay", MirrorWireKind::Int64, qlonglong(coreTx.txAlcDecay() == 20 ? 21 : 20)},
          {"micBoost", MirrorWireKind::Bool, !coreTx.micBoost()}}) {
        const SessionPropertyResult result = s.writeTransmit(name, kind, value);
        QVERIFY2(result.accepted, qPrintable(QString::fromUtf8(name) + QStringLiteral(": ")
                                             + result.reason));
    }
    // The keying set is still refused keyed.
    QCOMPARE(s.writeTransmit(QByteArrayLiteral("mox"), MirrorWireKind::Bool, false).reason,
             kReceiveOnly);
    mox->setMox(false);
    QTRY_VERIFY(mox->state() == MoxState::Rx);

    // A hardware PTT press.
    mox->onMicPttFromRadio(true);
    QVERIFY(mox->isMox());
    expectTaken("hardware ptt");
    if (QTest::currentTestFailed()) { return; }
    mox->onMicPttFromRadio(false);
    QTRY_VERIFY(mox->state() == MoxState::Rx);

    // The two-tone test, keyed by its controller.
    {
        TxChannel tx(/*channelId=*/1);
        TwoToneController* const twoTone = s.core->twoToneController();
        QVERIFY(twoTone);
        twoTone->setTxChannel(&tx);
        twoTone->setSettleDelaysMs(0, 0);
        twoTone->setActive(true);
        QTRY_VERIFY(twoTone->isActive());
        expectTaken("two-tone");
        if (QTest::currentTestFailed()) { return; }
        twoTone->setActive(false);
        QTRY_VERIFY(!twoTone->isActive());
        QTRY_VERIFY(mox->state() == MoxState::Rx);
        twoTone->setTxChannel(nullptr);
    }
}

// The Tune Power command and TX profile save and delete are the
// transmitter's own settings: while the Core's own key holds transmit they
// are the holder's (ruling 7.7), refused with the station's words. The
// holder's own change is taken on the air (tst_on_air_refusals).
void TstTransmitSettingsGate::settingVerbsAreTheStationKeysWhileItHoldsTransmit()
{
    Session s(m_securityDir.path(), this);
    s.core->scopeTxProfiles(QStringLiteral("AA:BB:CC:DD:EE:01"));
    QVERIFY(s.connect());
    MoxController* const mox = s.core->moxController();
    mox->setMoxCheck({});
    mox->setMox(true);
    QTRY_VERIFY(mox->state() == MoxState::Tx);

    const SessionMessage tune = s.invoke(
        QByteArrayLiteral("setTunePowerForTxBand"),
        {MirrorUpdate{0, "watts", MirrorWireKind::Int64, QVariant(qlonglong(3))}});
    QCOMPARE(tune.reason, kOnAir);
    const SessionMessage save = s.invoke(
        QByteArrayLiteral("txProfile.save"),
        {MirrorUpdate{0, "name", MirrorWireKind::Utf8, QVariant(QStringLiteral("On air"))}});
    QCOMPARE(save.reason, kOnAir);
    QVERIFY(!s.core->micProfileManager()->profileNames().contains(QStringLiteral("On air")));
    const SessionMessage remove = s.invoke(
        QByteArrayLiteral("txProfile.delete"),
        {MirrorUpdate{0, "name", MirrorWireKind::Utf8, QVariant(QStringLiteral("Default"))}});
    QCOMPARE(remove.reason, kOnAir);
    mox->setMox(false);
    QTRY_VERIFY(mox->state() == MoxState::Rx);
}

void TstTransmitSettingsGate::onAirRefusalIsTheTgxlRefusal()
{
    RadioModel core;
    core.setReceiveOnlyStationPolicy(true);
    QString reason;
    QVERIFY(!core.stationOnAirRefusal(&reason));
    QVERIFY(reason.isEmpty());
    core.transmitModel().setTune(true);
    QVERIFY(core.stationOnAirRefusal(&reason));
    QCOMPARE(reason, kOnAir);
    QCOMPARE(RadioModel::onAirReason(), kOnAir);
    core.transmitModel().setTune(false);
    QVERIFY(!core.stationOnAirRefusal(nullptr));
}

void TstTransmitSettingsGate::dspOptionsTxKeyTakenOffTheAirAndApplied()
{
    const QString txKey = QStringLiteral("DspOptionsBufferSizePhoneTx");
    Session s(m_securityDir.path(), this);
    s.settings.setValue(txKey, QStringLiteral("1024"));
    QList<DSPMode> txApplies;
    s.core->setDspOptionsTxApplyObserverForTest(
        [&txApplies](DSPMode mode) { txApplies.append(mode); });
    QVERIFY(s.connect());
    QVERIFY(s.proxy.ready());
    SliceModel* txSlice = s.core->txBoundSlice();
    QVERIFY(txSlice);
    txSlice->setDspMode(DSPMode::USB);

    QSignalSpy rejected(&s.proxy, &SettingsProxy::valueRejected);
    s.proxy.setValue(txKey, QStringLiteral("2048"));
    QTRY_COMPARE(s.settings.value(txKey).toString(), QStringLiteral("2048"));
    QTRY_COMPARE(txApplies.size(), 1);
    QCOMPARE(txApplies.first(), DSPMode::USB);
    QCOMPARE(rejected.count(), 0);

    // A key for a group the TX slice is not in is saved, not applied now.
    s.proxy.setValue(QStringLiteral("DspOptionsBufferSizeFmTx"), QStringLiteral("512"));
    QTRY_COMPARE(s.settings.value(QStringLiteral("DspOptionsBufferSizeFmTx")).toString(),
                 QStringLiteral("512"));
    QTest::qWait(120);
    QCOMPARE(txApplies.size(), 1);

    // A remove returns it to its default, applied the same way.
    s.proxy.remove(txKey);
    QTRY_VERIFY(!s.settings.contains(txKey));
    QTRY_COMPARE(txApplies.size(), 2);
    QCOMPARE(rejected.count(), 0);
}

// A local window's DSP > Options TX combos change while transmitting; the
// Core takes the key on the air too and applies it to the TX channel once
// the radio is back on receive (dspOptionsTxApplyWaitsForTheUnkey's rule).
void TstTransmitSettingsGate::dspOptionsTxKeyTakenOnTheAirAndAppliedAtTheUnkey()
{
    const QString txKey = QStringLiteral("DspOptionsFilterSizePhoneTx");
    Session s(m_securityDir.path(), this);
    s.settings.setValue(txKey, QStringLiteral("4096"));
    QList<DSPMode> txApplies;
    s.core->setDspOptionsTxApplyObserverForTest(
        [&txApplies](DSPMode mode) { txApplies.append(mode); });
    QVERIFY(s.connect());
    QVERIFY(s.proxy.ready());
    SliceModel* txSlice = s.core->txBoundSlice();
    QVERIFY(txSlice);
    txSlice->setDspMode(DSPMode::USB);

    MoxController* const mox = s.core->moxController();
    mox->setMoxCheck({});
    mox->setMox(true);
    QTRY_VERIFY(mox->state() == MoxState::Tx);

    QSignalSpy rejected(&s.proxy, &SettingsProxy::valueRejected);
    s.proxy.setValue(txKey, QStringLiteral("8192"));
    QTRY_COMPARE(s.settings.value(txKey).toString(), QStringLiteral("8192"));
    QTest::qWait(150);
    QCOMPARE(rejected.count(), 0);
    QVERIFY(txApplies.isEmpty());

    mox->setMox(false);
    QTRY_VERIFY(mox->state() == MoxState::Rx);
    QTRY_COMPARE(txApplies.size(), 1);
    QCOMPARE(txApplies.first(), DSPMode::USB);
}

// Group A fix wave, I2: a TX DSP > Options write accepted off the air
// applies up to 50 ms later. Keyed inside that window, nothing reaches the
// TX channel until the radio is back on receive; then the change applies.
void TstTransmitSettingsGate::dspOptionsTxApplyWaitsForTheUnkey()
{
    const QString txKey = QStringLiteral("DspOptionsBufferSizePhoneTx");
    Session s(m_securityDir.path(), this);
    QList<DSPMode> txApplies;
    s.core->setDspOptionsTxApplyObserverForTest(
        [&txApplies](DSPMode mode) { txApplies.append(mode); });
    QVERIFY(s.connect());
    SliceModel* txSlice = s.core->txBoundSlice();
    QVERIFY(txSlice);
    txSlice->setDspMode(DSPMode::USB);

    // Accepted off the air: the apply is queued for the coalescing window.
    QVERIFY(!s.core->stationOnAirRefusal(nullptr));
    s.settings.setValue(txKey, QStringLiteral("2048"));
    s.core->scheduleRemoteDspOptionsApply(txKey);

    // Keyed before the window ends.
    MoxController* const mox = s.core->moxController();
    mox->setMoxCheck({});
    mox->setMox(true);
    QVERIFY(s.core->stationOnAirRefusal(nullptr));
    QTest::qWait(150);
    QVERIFY(txApplies.isEmpty());

    // Back on receive: the held change applies once.
    mox->setMox(false);
    QTRY_VERIFY(mox->state() == MoxState::Rx);
    QTRY_COMPARE(txApplies.size(), 1);
    QCOMPARE(txApplies.first(), DSPMode::USB);
    QTest::qWait(120);
    QCOMPARE(txApplies.size(), 1);
}

// Group A follow-up (group B fix wave): the held change is also released
// when two-tone ends (TwoToneController clears isActive after MOX drops),
// not only on the next plain unkey.
void TstTransmitSettingsGate::dspOptionsTxApplyWaitsForTwoToneToEnd()
{
    const QString txKey = QStringLiteral("DspOptionsBufferSizePhoneTx");
    Session s(m_securityDir.path(), this);
    QList<DSPMode> txApplies;
    s.core->setDspOptionsTxApplyObserverForTest(
        [&txApplies](DSPMode mode) { txApplies.append(mode); });
    QVERIFY(s.connect());
    SliceModel* txSlice = s.core->txBoundSlice();
    QVERIFY(txSlice);
    txSlice->setDspMode(DSPMode::USB);

    TxChannel tx(/*channelId=*/1);
    TwoToneController* const twoTone = s.core->twoToneController();
    twoTone->setTxChannel(&tx);
    s.core->moxController()->setMoxCheck({});
    // Accepted off the air, then the two-tone test starts inside the
    // coalescing window.
    s.settings.setValue(txKey, QStringLiteral("2048"));
    s.core->scheduleRemoteDspOptionsApply(txKey);
    twoTone->setActive(true);
    QTRY_VERIFY(twoTone->isActive());
    QVERIFY(s.core->stationOnAirRefusal(nullptr));
    QTest::qWait(150);
    QVERIFY(txApplies.isEmpty());

    twoTone->setActive(false);
    QTRY_VERIFY(!twoTone->isActive());
    QTRY_VERIFY(!s.core->stationOnAirRefusal(nullptr));
    QTRY_COMPARE(txApplies.size(), 1);
    QCOMPARE(txApplies.first(), DSPMode::USB);
    QTest::qWait(120);
    QCOMPARE(txApplies.size(), 1);
    twoTone->setTxChannel(nullptr);
}

// Setup description version 22: an RX DSP > Options write accepted off the
// air applies up to 50 ms later. Keyed inside that window (MOX or the
// two-tone test), nothing reaches the RX channels until the radio is back
// on receive; then the change applies once, as the TX half waits.
void TstTransmitSettingsGate::dspOptionsRxApplyWaitsForReceive_data()
{
    QTest::addColumn<bool>("twoToneKey");
    QTest::newRow("mox") << false;
    QTest::newRow("two-tone") << true;
}

void TstTransmitSettingsGate::dspOptionsRxApplyWaitsForReceive()
{
    QFETCH(bool, twoToneKey);
    const QString rxKey = QStringLiteral("DspOptionsBufferSizePhoneRx");
    Session s(m_securityDir.path(), this);
    QList<DSPMode> rxApplies;
    s.core->setDspOptionsApplyObserverForTest(
        [&rxApplies](int, DSPMode mode) { rxApplies.append(mode); });
    QVERIFY(s.connect());
    SliceModel* slice = s.core->slices().first();
    QVERIFY(slice);
    slice->setDspMode(DSPMode::USB);

    TxChannel tx(/*channelId=*/1);
    TwoToneController* const twoTone = s.core->twoToneController();
    twoTone->setTxChannel(&tx);
    MoxController* const mox = s.core->moxController();
    mox->setMoxCheck({});

    // Accepted off the air: the apply is queued for the coalescing window.
    QVERIFY(!s.core->stationOnAirRefusal(nullptr));
    s.settings.setValue(rxKey, QStringLiteral("2048"));
    s.core->scheduleRemoteDspOptionsApply(rxKey);

    // Keyed before the window ends.
    if (twoToneKey) {
        twoTone->setActive(true);
        QTRY_VERIFY(twoTone->isActive());
    } else {
        mox->setMox(true);
    }
    QVERIFY(s.core->stationOnAirRefusal(nullptr));
    QTest::qWait(150);
    QVERIFY(rxApplies.isEmpty());

    // Back on receive: the held change applies once.
    if (twoToneKey) {
        twoTone->setActive(false);
        QTRY_VERIFY(!twoTone->isActive());
    } else {
        mox->setMox(false);
    }
    QTRY_VERIFY(!s.core->stationOnAirRefusal(nullptr));
    QTRY_COMPARE(rxApplies.size(), 1);
    QCOMPARE(rxApplies.first(), DSPMode::USB);
    QTest::qWait(120);
    QCOMPARE(rxApplies.size(), 1);
    twoTone->setTxChannel(nullptr);
}

// Group A follow-up (group B fix wave): Setup > PA's reload on the 50 ms
// timer re-checks the on-air rule: a PA key accepted off the air and keyed
// inside the window reloads once the radio is back on receive.
void TstTransmitSettingsGate::paReloadWaitsWhileOnTheAir()
{
    Session s(m_securityDir.path(), this);
    QStringList reloads;
    s.core->setHardwareApplyObserverForTest(
        [&reloads](const QString& name) { reloads.append(name); });
    QVERIFY(s.connect());
    const QString mac = s.core->currentRadioMac();
    QVERIFY(!mac.isEmpty());

    s.core->scheduleRemoteHardwareApply(
        QStringLiteral("hardware/%1/pa/profiles/active").arg(mac));
    s.core->scheduleRemoteHardwareApply(
        QStringLiteral("hardware/%1/paCalibration/fwd").arg(mac));
    MoxController* const mox = s.core->moxController();
    mox->setMoxCheck({});
    mox->setMox(true);
    QVERIFY(s.core->stationOnAirRefusal(nullptr));
    QTest::qWait(150);
    QVERIFY2(reloads.isEmpty(), qPrintable(reloads.join(u',')));

    mox->setMox(false);
    QTRY_VERIFY(mox->state() == MoxState::Rx);
    QTRY_VERIFY(reloads.contains(QStringLiteral("pa")));
    QVERIFY(reloads.contains(QStringLiteral("cal")));
    QTest::qWait(120);
    QCOMPARE(reloads.count(QStringLiteral("pa")), 1);
    QCOMPARE(reloads.count(QStringLiteral("cal")), 1);
}

void TstTransmitSettingsGate::transmitHardwareKeysStayRefused()
{
    Session s(m_securityDir.path(), this);
    const QString key = QStringLiteral("hardware/AA:BB:CC:DD:EE:01/tx/micGainDb");
    s.settings.setValue(key, QStringLiteral("3"));
    QVERIFY(s.connect());
    QVERIFY(s.proxy.ready());
    QSignalSpy rejected(&s.proxy, &SettingsProxy::valueRejected);
    s.proxy.setValue(key, QStringLiteral("9"));
    QTRY_COMPARE(rejected.count(), 1);
    QCOMPARE(s.settings.value(key).toString(), QStringLiteral("3"));
}

void TstTransmitSettingsGate::windowOnAirFollowsTheCore()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    QVERIFY(!s.window.isCoreOnAir());
    QSignalSpy onAir(&s.window, &RadioModel::coreOnAirChanged);

    // The Core's MOX, reported as `transmitting`.
    MoxController* const mox = s.core->moxController();
    mox->setMoxCheck({});
    mox->setMox(true);
    QTRY_VERIFY(s.window.isCoreOnAir());
    QCOMPARE(onAir.last().at(0).toBool(), true);
    mox->setMox(false);
    QTRY_VERIFY(!s.window.isCoreOnAir());
    QCOMPARE(onAir.last().at(0).toBool(), false);

    // The Core's TUNE, reported on the mirrored transmit model.
    s.core->transmitModel().setTune(true);
    QTRY_VERIFY(s.window.isCoreOnAir());
    s.core->transmitModel().setTune(false);
    QTRY_VERIFY(!s.window.isCoreOnAir());

    // PureSignal's two-tone, reported by the facade.
    {
        TxChannel tx(/*channelId=*/1);
        TwoToneController* const twoTone = s.core->twoToneController();
        twoTone->setTxChannel(&tx);
        twoTone->setSettleDelaysMs(0, 0);
        twoTone->setActive(true);
        QTRY_VERIFY(twoTone->isActive());
        QTRY_VERIFY(s.window.pureSignalFacade()->twoToneOn());
        QVERIFY(s.window.isCoreOnAir());
        twoTone->setActive(false);
        QTRY_VERIFY(!twoTone->isActive());
        QTRY_VERIFY(!s.window.isCoreOnAir());
        twoTone->setTxChannel(nullptr);
    }
    QTRY_VERIFY(mox->state() == MoxState::Rx);
}

void TstTransmitSettingsGate::windowOnAirClearsWhenTheSessionEnds()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    MoxController* const mox = s.core->moxController();
    mox->setMoxCheck({});
    mox->setMox(true);
    QTRY_VERIFY(s.window.isTransmitting());
    QTRY_VERIFY(s.window.isCoreOnAir());

    // The Core goes away while the window last heard "on the air".
    s.coreEnd->closeLink(QStringLiteral("test"));
    QTRY_VERIFY(!s.window.isTransmitting());
    QTRY_VERIFY(!s.window.isCoreOnAir());
    mox->setMox(false);
    QTRY_VERIFY(mox->state() == MoxState::Rx);
}

QTEST_MAIN(TstTransmitSettingsGate)
#include "tst_transmit_settings_gate.moc"
