// no-port-check: NereusSDR-original. R-R3-49 (parity Task 5): Setup >
// Transmit > Power, Transmit > DEXP/VOX and Test > Two-Tone IMD from a
// remote window while the Core's radio is off the air. Loopback link, no RF
// and no hardware: nothing here keys a radio. "On the air" keys the Core's
// own MoxController with the receive-only MOX pre-check lifted, as
// tst_transmit_model_properties does. The Core's transmit chain is wired to
// a test TxChannel with no WDSP channel behind it
// (RadioModel::wireTransmitChainForTest), and its step attenuator is a
// StepAttenuatorController with no radio connection. No audio device is
// opened.
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 5): the Power,
//                                    DEXP/VOX and two-tone settings from a
//                                    window reach the Core's live objects,
//                                    are refused out of range and on the
//                                    air, the three pages work in a remote
//                                    window, and nothing on them keys the
//                                    radio. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  Integration carry: the window signs in
//                                    to an upgraded Core with its token
//                                    (seedUpgradedCoreToken), as Part C's
//                                    paired-device sign-in requires.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  Transmit group fix wave 2 (M8,
//                                    R-IOS-13): Enable VOX shows disabled
//                                    with the reason while this computer
//                                    has no microphone line to the Core.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  Remote parity on the air
//                                    (transmitSettingsVersion 13): the
//                                    Power page, ATT on TX and version 5
//                                    settings are taken on the air.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  PA on-air gate review: the per-band
//                                    power and tune power maps are the
//                                    Core's own; a peer's write of either
//                                    is refused and leaves the Core's map
//                                    as it was, and the Core's change still
//                                    reaches the window.
//                                    AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QCoreApplication>
#include <QDoubleSpinBox>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalSpy>
#include <QSlider>
#include <QSpinBox>
#include <QTemporaryDir>

#include <memory>

#include "core/safety/TxRefusal.h"
#include "core/AppSettings.h"
#include "core/ConnectionState.h"
#include "core/HardwareProfile.h"
#include "core/MoxController.h"
#include "core/StepAttenuatorController.h"
#include "core/StepAttenuatorFacade.h"
#include "core/TwoToneController.h"
#include "core/TxChannel.h"
#include "core/safety/SwrProtectionController.h"
#include "core/session/IStationLink.h"
#include "core/session/SessionMessages.h"
#include "core/session/StationCapabilities.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/settings/SettingsProxy.h"
#include "fakes/LoopbackTransport.h"
#include "fakes/UpgradedCoreToken.h"
#include "gui/SetupDialog.h"
#include "gui/setup/TestTwoTonePage.h"
#include "gui/setup/TransmitSetupPages.h"
#include "models/Band.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

#include "OperatorWording.h"

using namespace NereusSDR;
using NereusSDR::Test::LoopbackTransport;

namespace {

const QString kOnAir = QStringLiteral("The radio is on the air. Try again when it stops.");
const QString kTransmitReason = QStringLiteral("Remote transmit is not here yet");

std::unique_ptr<RadioModel> makeStationRadioModel()
{
    auto model = std::make_unique<RadioModel>();
    model->setBoardForTest(HPSDRHW::Saturn);
    model->setHpsdrModelForTest(HPSDRModel::ANAN_G2);
    RadioInfo info;
    info.macAddress = QStringLiteral("AA:BB:CC:DD:EE:05");
    info.name = QStringLiteral("Bench G2");
    info.boardType = HPSDRHW::Saturn;
    model->setLastRadioInfoForTest(info);
    model->setConnectionStateForTest(ConnectionState::Connected);
    model->addSlice(QStringLiteral("pan-0"));
    return model;
}

// A receive-only Core with its transmit chain wired to a test TxChannel and
// a step attenuator with no radio behind it, and one window, handshake
// complete. The window's settings go through its SettingsProxy.
struct Session {
    explicit Session(const QString& securityDir, QObject* parent)
        : settings(settingsDir.filePath(QStringLiteral("NereusSDR.settings")))
        , txChannel(/*channelId=*/1)
    {
        // As CoreInit's migrations leave it, so both ends agree.
        settings.setValue(QStringLiteral("SettingsSchemaVersion"), QStringLiteral("7"));
        core = makeStationRadioModel();
        core->wireTransmitChainForTest(&txChannel);
        stepAtt.setTickTimerEnabled(false);
        core->setStepAttController(&stepAtt);
        server = std::make_unique<StationServer>(
            core.get(), settings, NereusSDR::Test::seedUpgradedCoreToken(securityDir));
        client = std::make_unique<StationClient>(&window, &proxy);
        coreEnd = new LoopbackTransport(QStringLiteral("station-end"), parent);
        windowEnd = new LoopbackTransport(QStringLiteral("client-end"), parent);
        coreEnd->linkTo(windowEnd);
    }
    ~Session()
    {
        AppSettings::instance().setRemoteBackend(nullptr);
        client.reset();
        server.reset();
        core->setStepAttController(nullptr);
        core->injectTxChannelForTest(nullptr);
    }
    bool connect()
    {
        QSignalSpy completed(client.get(), &StationClient::handshakeComplete);
        client->startSession(windowEnd, server->token());
        server->acceptTransport(coreEnd);
        return completed.wait(5000) || completed.count() == 1;
    }
    // A raw write of one property of `objectKey`, and the Core's answer.
    SessionPropertyResult write(const QByteArray& objectKey, const QByteArray& name,
                                MirrorWireKind kind, const QVariant& value)
    {
        const quint32 writeId = ++nextId;
        MirrorUpdate update;
        update.ordinal = 1;
        update.name = name;
        update.kind = kind;
        update.value = value;
        windowEnd->sendText(SessionMessages::encode(
            SessionMessages::propertyWrite(objectKey, {update}, writeId)));
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
    SessionPropertyResult writeTransmit(const QByteArray& name, MirrorWireKind kind,
                                        const QVariant& value)
    {
        return write(QByteArrayLiteral("transmit"), name, kind, value);
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

    QTemporaryDir settingsDir;
    AppSettings settings;
    TxChannel txChannel;
    StepAttenuatorController stepAtt;
    std::unique_ptr<RadioModel> core;
    std::unique_ptr<StationServer> server;
    RadioModel window{RadioModel::Role::Remote};
    SettingsProxy proxy;
    std::unique_ptr<StationClient> client;
    LoopbackTransport* coreEnd = nullptr;
    LoopbackTransport* windowEnd = nullptr;
    quint32 nextId = 95000;
};

// A whole band map, every band at `watts` but those in `changed`, keys in
// band order (not the Core's own order).
QString bandMap(int watts, const QList<QPair<QString, int>>& changed = {})
{
    QString json = QStringLiteral("{");
    for (int i = 0; i < 14; ++i) {
        const QString key = bandKeyName(static_cast<Band>(i));
        int value = watts;
        for (const auto& c : changed) {
            if (c.first == key) { value = c.second; }
        }
        json += QStringLiteral("%1\"%2\":%3").arg(i == 0 ? QString() : QStringLiteral(","))
                    .arg(key).arg(value);
    }
    return json + QStringLiteral("}");
}

// MOX, TUNE, the two-tone test and VOX all stay off on the Core.
bool nothingKeyed(const Session& s, QString* what)
{
    const TransmitModel& tx = s.core->transmitModel();
    const auto fail = [what](const char* name) {
        if (what) { *what = QString::fromLatin1(name); }
        return false;
    };
    if (s.core->mox() || tx.isMox()) { return fail("MOX"); }
    if (s.core->tune() || s.core->isTune() || tx.isTune()) { return fail("TUNE"); }
    if (tx.isTwoToneActive()) { return fail("two-tone"); }
    if (s.core->twoToneController() && s.core->twoToneController()->isActive()) {
        return fail("two-tone controller");
    }
    if (tx.voxEnabled()) { return fail("VOX"); }
    if (s.core->moxController()->state() != MoxState::Rx) { return fail("MoxController"); }
    return true;
}

// The settings.reject reason the window received for `key`, if any.
QString settingsRejectReason(LoopbackTransport* windowEnd, const QString& key)
{
    QString reason;
    for (const QByteArray& wire : windowEnd->received()) {
        SessionMessage message;
        if (SessionMessages::decode(wire, &message)
            && message.kind == SessionMessageKind::SettingsReject
            && QString::fromUtf8(message.objectKey) == key) {
            reason = message.reason;
        }
    }
    return reason;
}

}  // namespace

class TstRemoteTransmitSetupPages : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void cleanup();

    void swrProtectionKeysApplyToTheCoresController();
    void powerPageKeysTakenOnTheAir();
    void powerPageKeysOutOfRangeAreRefused();
    void attOnTxSettingsReachTheCoresStepAttenuator();
    void attOnTxSettingsAreTakenOnTheAirAndRefusedOutOfRange();
    void version5SettingsReachTheCore();
    void version5WritesOutOfRangeAreRefused();
    void version5WritesAreTakenOnTheAir();
    void remotePowerPageShowsAndChangesTheCoresValues();
    void remoteDexpPageChangesTheCoreAndVoxWaitsForTransmit();
    void remoteTwoTonePageChangesTheCore();
    void setupOpensThePagesWithoutRemoteTransmit();
    void localSwrProtectionAppliesAtOnce();
    void newReasonsArePlain();
    void tooltipsSayWhatTheControlsDo();
    void attOnTxBoxShowsTheRadiosRange();

private:
    QTemporaryDir m_securityDir;
};

void TstRemoteTransmitSetupPages::initTestCase()
{
    QVERIFY(m_securityDir.isValid());
    const QString profile = QStringLiteral("remote-transmit-setup-pages-%1")
                                .arg(QCoreApplication::applicationPid());
    AppSettings::setProfileOverride(profile);
    AppSettings::instance().clear();
    AppSettings::instance().setValue(QStringLiteral("SettingsSchemaVersion"), QStringLiteral("7"));
}

void TstRemoteTransmitSetupPages::cleanupTestCase()
{
    AppSettings::instance().setRemoteBackend(nullptr);
    const QString path = AppSettings::instance().filePath();
    QFile::remove(path);
    QFile::remove(path + QStringLiteral(".bak"));
}

void TstRemoteTransmitSetupPages::cleanup()
{
    AppSettings::instance().setRemoteBackend(nullptr);
}

// B5.12: each SWR protection key a window writes reaches the Core's live
// SwrProtectionController at once (read from the controller, not the file).
void TstRemoteTransmitSetupPages::swrProtectionKeysApplyToTheCoresController()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    QVERIFY(s.proxy.ready());
    safety::SwrProtectionController& swr = s.core->swrProt();

    QVERIFY(StationServer::isTransmitSettingKeyAcceptedOffAir(QStringLiteral("SwrProtectionEnabled")));
    QVERIFY(StationServer::isTransmitSettingKeyAcceptedOffAir(QStringLiteral("TxInhibitMonitorEnabled")));
    QVERIFY(StationServer::isTransmitSettingKeyAcceptedOffAir(QStringLiteral("TxInhibitMonitorReversed")));
    // transmitSettingsVersion 11: Disable HF PA, taken on and off the air
    // as Thetis applies it (no MOX check).
    QVERIFY(StationServer::isTransmitSettingKeyAcceptedOffAir(QStringLiteral("DisableHfPa")));
    QVERIFY(StationServer::isTransmitSettingKeyTakenOnAir(QStringLiteral("DisableHfPa")));

    s.proxy.setValue(QStringLiteral("SwrProtectionEnabled"), QStringLiteral("True"));
    QTRY_VERIFY(swr.isEnabled());
    s.proxy.setValue(QStringLiteral("SwrProtectionEnabled"), QStringLiteral("False"));
    QTRY_VERIFY(!swr.isEnabled());
    s.proxy.setValue(QStringLiteral("SwrProtectionLimit"), QStringLiteral("3.4"));
    QTRY_COMPARE(swr.limit(), 3.4f);
    s.proxy.setValue(QStringLiteral("SwrTuneProtectionEnabled"), QStringLiteral("True"));
    QTRY_VERIFY(swr.disableOnTune());
    s.proxy.setValue(QStringLiteral("TunePowerSwrIgnore"), QStringLiteral("42"));
    QTRY_COMPARE(swr.tunePowerSwrIgnore(), 42.0f);
    s.proxy.setValue(QStringLiteral("WindBackPowerSwr"), QStringLiteral("True"));
    QTRY_VERIFY(swr.windBackEnabled());

    // A removed key returns the controller to the default the Core starts on.
    s.proxy.remove(QStringLiteral("TunePowerSwrIgnore"));
    QTRY_COMPARE(swr.tunePowerSwrIgnore(), 35.0f);

    // External TX Inhibit is taken off the air and stored on the Core; gaps
    // Task 13 applies it to the Core's gate.
    s.proxy.setValue(QStringLiteral("TxInhibitMonitorEnabled"), QStringLiteral("True"));
    QTRY_COMPARE(s.settings.value(QStringLiteral("TxInhibitMonitorEnabled")).toString(),
                 QStringLiteral("True"));
    QString keyed;
    QVERIFY2(nothingKeyed(s, &keyed), qPrintable(keyed));
}

void TstRemoteTransmitSetupPages::powerPageKeysTakenOnTheAir()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    safety::SwrProtectionController& swr = s.core->swrProt();
    s.proxy.setValue(QStringLiteral("SwrProtectionEnabled"), QStringLiteral("False"));
    QTRY_VERIFY(!swr.isEnabled());

    // Remote parity on the air (transmitSettingsVersion 13): the local page
    // changes them while transmitting, so the Core takes them keyed.
    s.keyCore();
    QTRY_VERIFY(s.window.isCoreOnAir());
    QSignalSpy rejected(&s.proxy, &SettingsProxy::valueRejected);
    s.proxy.setValue(QStringLiteral("SwrProtectionEnabled"), QStringLiteral("True"));
    QTRY_VERIFY(swr.isEnabled());
    QCOMPARE(rejected.count(), 0);
    // External TX Inhibit is the transmitter's own setting (the shared
    // settings rule): while another device, here the Core's own key, holds
    // transmit on the air, this device's change waits for it, as before.
    s.proxy.setValue(QStringLiteral("TxInhibitMonitorEnabled"), QStringLiteral("True"));
    QTest::qWait(150);
    QCOMPARE(s.settings.value(QStringLiteral("TxInhibitMonitorEnabled"),
                              QStringLiteral("False")).toString(),
             QStringLiteral("False"));
    s.unkeyCore();
    QTRY_VERIFY(!s.window.isCoreOnAir());
}

void TstRemoteTransmitSetupPages::powerPageKeysOutOfRangeAreRefused()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    safety::SwrProtectionController& swr = s.core->swrProt();
    s.proxy.setValue(QStringLiteral("TunePowerSwrIgnore"), QStringLiteral("70"));
    QTRY_COMPARE(settingsRejectReason(s.windowEnd, QStringLiteral("TunePowerSwrIgnore")),
                 QStringLiteral("Choose a tune power to ignore from 5 to 50 W."));
    QCOMPARE(swr.tunePowerSwrIgnore(), 35.0f);
    s.proxy.setValue(QStringLiteral("SwrProtectionEnabled"), QStringLiteral("yes"));
    QTRY_COMPARE(settingsRejectReason(s.windowEnd, QStringLiteral("SwrProtectionEnabled")),
                 QStringLiteral("The Core expected this box to be on or off."));
    s.proxy.setValue(QStringLiteral("SwrProtectionLimit"), QStringLiteral("9.0"));
    QTRY_COMPARE(settingsRejectReason(s.windowEnd, QStringLiteral("SwrProtectionLimit")),
                 QStringLiteral("Choose an SWR protection limit from 1.0 to 5.0."));
    // The ends are taken.
    s.proxy.setValue(QStringLiteral("TunePowerSwrIgnore"), QStringLiteral("5"));
    QTRY_COMPARE(swr.tunePowerSwrIgnore(), 5.0f);
    s.proxy.setValue(QStringLiteral("TunePowerSwrIgnore"), QStringLiteral("50"));
    QTRY_COMPARE(swr.tunePowerSwrIgnore(), 50.0f);
}

// ATT on TX, its value and Force ATT on `stepAtt` reach the Core's step
// attenuator, and the Core's own changes reach the window.
void TstRemoteTransmitSetupPages::attOnTxSettingsReachTheCoresStepAttenuator()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    QVERIFY(s.client->transmitSettingsAvailable(5));
    StepAttenuatorFacade* remote = s.window.stepAttFacade();
    QVERIFY(remote);
    QTRY_COMPARE(remote->attOnTxEnabled(), s.stepAtt.attOnTxEnabled());

    remote->setAttOnTxEnabled(!s.stepAtt.attOnTxEnabled());
    const bool want = remote->attOnTxEnabled();
    QTRY_COMPARE(s.stepAtt.attOnTxEnabled(), want);
    remote->setAttOnTxValue(12);
    QTRY_COMPARE(s.stepAtt.attOnTxValue(), 12);
    remote->setForceAttWhenPsOff(!s.stepAtt.forceAttWhenPsOff());
    const bool force = remote->forceAttWhenPsOff();
    QTRY_COMPARE(s.stepAtt.forceAttWhenPsOff(), force);

    // The Core's own change (PureSignal's AutoAtt writes the value) shows.
    s.stepAtt.setAttOnTxValue(20);
    QTRY_COMPARE(remote->attOnTxValue(), 20);
    s.stepAtt.setAttOnTxEnabled(!want);
    QTRY_COMPARE(remote->attOnTxEnabled(), !want);
    s.stepAtt.setForceAttWhenPsOff(!force);
    QTRY_COMPARE(remote->forceAttWhenPsOff(), !force);
    QString keyed;
    QVERIFY2(nothingKeyed(s, &keyed), qPrintable(keyed));
}

void TstRemoteTransmitSetupPages::attOnTxSettingsAreTakenOnTheAirAndRefusedOutOfRange()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    const QByteArray stepAtt = QByteArrayLiteral("stepAtt");
    SessionPropertyResult r = s.write(stepAtt, "attOnTxValue", MirrorWireKind::Int64, 32);
    QVERIFY(!r.accepted);
    QCOMPARE(r.reason, QStringLiteral("Choose an ATT on TX value from 0 to 31 dB."));
    r = s.write(stepAtt, "attOnTxValue", MirrorWireKind::Int64, -1);
    QVERIFY(!r.accepted);
    QVERIFY(s.write(stepAtt, "attOnTxValue", MirrorWireKind::Int64, 31).accepted);
    QCOMPARE(s.stepAtt.attOnTxValue(), 31);
    QVERIFY(s.write(stepAtt, "attOnTxValue", MirrorWireKind::Int64, 0).accepted);
    QCOMPARE(s.stepAtt.attOnTxValue(), 0);

    s.keyCore();
    QTRY_VERIFY(s.window.isCoreOnAir());
    const bool enabled = s.stepAtt.attOnTxEnabled();
    const bool force = s.stepAtt.forceAttWhenPsOff();
    // Taken on the air (version 13), as the local page takes them keyed;
    // the range still holds.
    r = s.write(stepAtt, "attOnTxEnabled", MirrorWireKind::Bool, !enabled);
    QVERIFY2(r.accepted, qPrintable(r.reason));
    r = s.write(stepAtt, "forceAttWhenPsOff", MirrorWireKind::Bool, !force);
    QVERIFY2(r.accepted, qPrintable(r.reason));
    r = s.write(stepAtt, "attOnTxValue", MirrorWireKind::Int64, 9);
    QVERIFY2(r.accepted, qPrintable(r.reason));
    QCOMPARE(s.stepAtt.attOnTxEnabled(), !enabled);
    QCOMPARE(s.stepAtt.forceAttWhenPsOff(), !force);
    QCOMPARE(s.stepAtt.attOnTxValue(), 9);
    r = s.write(stepAtt, "attOnTxValue", MirrorWireKind::Int64, 32);
    QCOMPARE(r.reason, QStringLiteral("Choose an ATT on TX value from 0 to 31 dB."));
    // The receive attenuator itself is not a transmit setting.
    QVERIFY(s.write(stepAtt, "attenuationDb", MirrorWireKind::Int64, 6).accepted);

    s.unkeyCore();
    QTRY_VERIFY(!s.window.isCoreOnAir());
}

// B5.12, B5.14, B5.16: each version 5 `transmit` property a window sets
// reaches the Core, and the DEXP settings reach its TX channel.
void TstRemoteTransmitSetupPages::version5SettingsReachTheCore()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    TransmitModel& coreTx = s.core->transmitModel();
    TransmitModel& windowTx = s.window.transmitModel();

    windowTx.setTuneDrivePowerSource(DrivePowerSource::Fixed);
    QTRY_COMPARE(coreTx.tuneDrivePowerSource(), DrivePowerSource::Fixed);

    windowTx.setDexpAttackTimeMs(12.0);
    windowTx.setDexpDetectorTauMs(33.0);
    windowTx.setDexpExpansionRatioDb(4.5);
    windowTx.setDexpHighCutHz(3100.0);
    windowTx.setDexpHysteresisRatioDb(2.5);
    windowTx.setDexpLookAheadEnabled(!coreTx.dexpLookAheadEnabled());
    const bool lookAhead = windowTx.dexpLookAheadEnabled();
    windowTx.setDexpLookAheadMs(120.0);
    windowTx.setDexpLowCutHz(400.0);
    windowTx.setDexpReleaseTimeMs(250.0);
    windowTx.setDexpSideChannelFilterEnabled(!coreTx.dexpSideChannelFilterEnabled());
    const bool scf = windowTx.dexpSideChannelFilterEnabled();
    windowTx.setAntiVoxGainDb(-12);
    QTRY_COMPARE(coreTx.antiVoxGainDb(), -12);
    QCOMPARE(coreTx.dexpAttackTimeMs(), 12.0);
    QCOMPARE(coreTx.dexpDetectorTauMs(), 33.0);
    QCOMPARE(coreTx.dexpExpansionRatioDb(), 4.5);
    QCOMPARE(coreTx.dexpHighCutHz(), 3100.0);
    QCOMPARE(coreTx.dexpHysteresisRatioDb(), 2.5);
    QCOMPARE(coreTx.dexpLookAheadEnabled(), lookAhead);
    QCOMPARE(coreTx.dexpLookAheadMs(), 120.0);
    QCOMPARE(coreTx.dexpLowCutHz(), 400.0);
    QCOMPARE(coreTx.dexpReleaseTimeMs(), 250.0);
    QCOMPARE(coreTx.dexpSideChannelFilterEnabled(), scf);
    // On the Core's TX channel.
    QCOMPARE(s.txChannel.lastDexpAttackTimeForTest(), 12.0);
    QCOMPARE(s.txChannel.lastDexpDetectorTauForTest(), 33.0);
    QCOMPARE(s.txChannel.lastDexpExpansionRatioDbForTest(), 4.5);
    QCOMPARE(s.txChannel.lastDexpHighCutHzForTest(), 3100.0);
    QCOMPARE(s.txChannel.lastDexpHysteresisRatioDbForTest(), 2.5);
    QCOMPARE(s.txChannel.lastDexpRunAudioDelayForTest(), lookAhead);
    QCOMPARE(s.txChannel.lastDexpAudioDelayMsForTest(), 120.0);
    QCOMPARE(s.txChannel.lastDexpLowCutHzForTest(), 400.0);
    QCOMPARE(s.txChannel.lastDexpReleaseTimeForTest(), 250.0);
    QCOMPARE(s.txChannel.lastDexpRunSideChannelFilterForTest(), scf);

    windowTx.setTwoToneFreq1(900);
    windowTx.setTwoToneFreq2(2100);
    windowTx.setTwoToneLevel(-3.5);
    windowTx.setTwoTonePower(35);
    windowTx.setTwoTonePulsed(!coreTx.twoTonePulsed());
    const bool pulsed = windowTx.twoTonePulsed();
    windowTx.setTwoToneInvert(!coreTx.twoToneInvert());
    const bool invert = windowTx.twoToneInvert();
    windowTx.setTwoToneFreq2Delay(40);
    windowTx.setTwoToneDrivePowerSource(DrivePowerSource::TuneSlider);
    QTRY_COMPARE(coreTx.twoToneDrivePowerSource(), DrivePowerSource::TuneSlider);
    QCOMPARE(coreTx.twoToneFreq1(), 900);
    QCOMPARE(coreTx.twoToneFreq2(), 2100);
    QCOMPARE(coreTx.twoToneLevel(), -3.5);
    QCOMPARE(coreTx.twoTonePower(), 35);
    QCOMPARE(coreTx.twoTonePulsed(), pulsed);
    QCOMPARE(coreTx.twoToneInvert(), invert);
    QCOMPARE(coreTx.twoToneFreq2Delay(), 40);

    // The per-band power maps are the Core's own (the review of the PA
    // on-air gate): the window keeps its change to itself and the Core
    // refuses a peer's write of either map, leaving its map as it was.
    const int core17 = coreTx.powerForBand(Band::Band17m);
    windowTx.setPowerForBand(Band::Band17m, core17 == 61 ? 62 : 61);
    const QString corePowerMap = coreTx.powerByBandJson();
    const QString coreTuneMap = coreTx.tunePowerByBandJson();
    SessionPropertyResult r = s.writeTransmit(
        "powerByBandJson", MirrorWireKind::Utf8,
        bandMap(50, {{QStringLiteral("40m"), 33}, {QStringLiteral("XVTR"), 7}}));
    QVERIFY(!r.accepted);
    QCOMPARE(r.reason, QStringLiteral("The Core sets this itself; it cannot be changed from here."));
    QCOMPARE(coreTx.powerByBandJson(), corePowerMap);
    r = s.writeTransmit("tunePowerByBandJson", MirrorWireKind::Utf8,
                        bandMap(10, {{QStringLiteral("160m"), 15}}));
    QVERIFY(!r.accepted);
    QCOMPARE(r.reason, QStringLiteral("The Core sets this itself; it cannot be changed from here."));
    QCOMPARE(coreTx.tunePowerByBandJson(), coreTuneMap);
    // A write with all 15 bands is refused the same way.
    QString with2m = bandMap(50, {{QStringLiteral("40m"), 33}, {QStringLiteral("XVTR"), 7}});
    with2m.insert(with2m.size() - 1, QStringLiteral(",\"2m\":44"));
    r = s.writeTransmit("powerByBandJson", MirrorWireKind::Utf8, with2m);
    QVERIFY(!r.accepted);
    QCOMPARE(coreTx.powerByBandJson(), corePowerMap);
    QCOMPARE(coreTx.powerForBand(Band::Band17m), core17);
    // The Core's whole map has all 15 bands (R-IOS-26).
    const QJsonObject map = QJsonDocument::fromJson(coreTx.powerByBandJson().toUtf8()).object();
    QCOMPARE(map.size(), 15);
    QCOMPARE(map.value(QStringLiteral("2m")).toInt(), coreTx.powerForBand(Band::Band2m));
    // A Core-side band change reaches the window.
    coreTx.setPowerForBand(Band::Band30m, 27);
    QTRY_COMPARE(windowTx.powerForBand(Band::Band30m), 27);
    coreTx.setTunePowerForBand(Band::Band6m, 9);
    QTRY_COMPARE(windowTx.tunePowerForBand(Band::Band6m), 9);

    QString keyed;
    QVERIFY2(nothingKeyed(s, &keyed), qPrintable(keyed));
}

void TstRemoteTransmitSetupPages::version5WritesOutOfRangeAreRefused()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    TransmitModel& coreTx = s.core->transmitModel();
    const struct {
        QByteArray name;
        MirrorWireKind kind;
        QVariant value;
        QString reason;
    } bad[] = {
        {"dexpAttackTimeMs", MirrorWireKind::Float64, 1.0,
         QStringLiteral("Choose a DEXP attack time from 2 to 100 ms.")},
        {"dexpDetectorTauMs", MirrorWireKind::Float64, 101.0,
         QStringLiteral("Choose a DEXP detector time from 1 to 100 ms.")},
        {"dexpReleaseTimeMs", MirrorWireKind::Float64, 1001.0,
         QStringLiteral("Choose a DEXP release time from 2 to 1000 ms.")},
        {"dexpExpansionRatioDb", MirrorWireKind::Float64, 31.0,
         QStringLiteral("Choose a DEXP expansion ratio from 0.0 to 30.0 dB.")},
        {"dexpHysteresisRatioDb", MirrorWireKind::Float64, -0.5,
         QStringLiteral("Choose a DEXP hysteresis ratio from 0.0 to 10.0 dB.")},
        {"dexpLookAheadMs", MirrorWireKind::Float64, 5.0,
         QStringLiteral("Choose a look-ahead time from 10 to 999 ms.")},
        {"dexpLowCutHz", MirrorWireKind::Float64, 50.0,
         QStringLiteral("Choose a VOX trigger filter cut from 100 to 10000 Hz.")},
        {"dexpHighCutHz", MirrorWireKind::Float64, 20000.0,
         QStringLiteral("Choose a VOX trigger filter cut from 100 to 10000 Hz.")},
        {"antiVoxGainDb", MirrorWireKind::Int64, 61,
         QStringLiteral("Choose an anti-VOX gain from -60 to 60 dB.")},
        {"twoToneFreq1", MirrorWireKind::Int64, 20001,
         QStringLiteral("Choose a tone frequency from -20000 to 20000 Hz.")},
        {"twoToneFreq2", MirrorWireKind::Int64, -20001,
         QStringLiteral("Choose a tone frequency from -20000 to 20000 Hz.")},
        {"twoToneLevel", MirrorWireKind::Float64, 0.5,
         QStringLiteral("Choose a two-tone level from -96 to 0 dB.")},
        {"twoTonePower", MirrorWireKind::Int64, 101,
         QStringLiteral("Choose a two-tone power from 0 to 100 percent.")},
        {"twoToneFreq2Delay", MirrorWireKind::Int64, 1001,
         QStringLiteral("Choose a second tone delay from 0 to 1000 ms.")},
    };
    for (const auto& b : bad) {
        const QVariant before = coreTx.property(b.name.constData());
        const SessionPropertyResult r = s.writeTransmit(b.name, b.kind, b.value);
        QVERIFY2(!r.accepted, b.name.constData());
        QCOMPARE(r.reason, b.reason);
        QCOMPARE(coreTx.property(b.name.constData()), before);
    }
    // The ends are taken.
    QVERIFY(s.writeTransmit("dexpAttackTimeMs", MirrorWireKind::Float64, 2.0).accepted);
    QVERIFY(s.writeTransmit("dexpLookAheadMs", MirrorWireKind::Float64, 999.0).accepted);
    QVERIFY(s.writeTransmit("twoToneLevel", MirrorWireKind::Float64, -96.0).accepted);
    QVERIFY(s.writeTransmit("antiVoxGainDb", MirrorWireKind::Int64, -60).accepted);
    // An enum outside the drive source's values is not the Core's.
    QVERIFY(!s.writeTransmit("twoToneDrivePowerSource", MirrorWireKind::Enum, 7).accepted);
}

void TstRemoteTransmitSetupPages::version5WritesAreTakenOnTheAir()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    TransmitModel& coreTx = s.core->transmitModel();
    TransmitModel& windowTx = s.window.transmitModel();
    s.keyCore();
    QTRY_VERIFY(s.window.isCoreOnAir());
    const struct {
        QByteArray name;
        MirrorWireKind kind;
        QVariant value;
    } writes[] = {
        {"tuneDrivePowerSource", MirrorWireKind::Enum, 2},
        {"dexpAttackTimeMs", MirrorWireKind::Float64, 20.0},
        {"dexpDetectorTauMs", MirrorWireKind::Float64, 20.0},
        {"dexpExpansionRatioDb", MirrorWireKind::Float64, 2.0},
        {"dexpHighCutHz", MirrorWireKind::Float64, 2000.0},
        {"dexpHysteresisRatioDb", MirrorWireKind::Float64, 2.0},
        {"dexpLookAheadEnabled", MirrorWireKind::Bool, !coreTx.dexpLookAheadEnabled()},
        {"dexpLookAheadMs", MirrorWireKind::Float64, 200.0},
        {"dexpLowCutHz", MirrorWireKind::Float64, 200.0},
        {"dexpReleaseTimeMs", MirrorWireKind::Float64, 200.0},
        {"dexpSideChannelFilterEnabled", MirrorWireKind::Bool,
         !coreTx.dexpSideChannelFilterEnabled()},
        {"antiVoxGainDb", MirrorWireKind::Int64, 7},
        {"twoToneFreq1", MirrorWireKind::Int64, 800},
        {"twoToneFreq2", MirrorWireKind::Int64, 1800},
        {"twoToneLevel", MirrorWireKind::Float64, -6.0},
        {"twoTonePower", MirrorWireKind::Int64, 20},
        {"twoTonePulsed", MirrorWireKind::Bool, !coreTx.twoTonePulsed()},
        {"twoToneInvert", MirrorWireKind::Bool, !coreTx.twoToneInvert()},
        {"twoToneFreq2Delay", MirrorWireKind::Int64, 10},
        {"twoToneDrivePowerSource", MirrorWireKind::Enum, 2},
    };
    // Taken on the air (version 13), as the local pages take them keyed.
    for (const auto& w : writes) {
        const SessionPropertyResult r = s.writeTransmit(w.name, w.kind, w.value);
        QVERIFY2(r.accepted, qPrintable(QString::fromUtf8(w.name) + QStringLiteral(": ")
                                        + r.reason));
    }
    QCOMPARE(coreTx.twoTonePower(), 20);
    // The window's own change reaches the Core too.
    const int freq1 = coreTx.twoToneFreq1() == 1234 ? 1235 : 1234;
    windowTx.setTwoToneFreq1(freq1);
    QTRY_COMPARE(coreTx.twoToneFreq1(), freq1);

    s.unkeyCore();
    QTRY_VERIFY(!s.window.isCoreOnAir());
}

// B5.12: Setup > Transmit > Power in a remote window shows the Core's values
// and changes them; nothing on it keys the radio.
void TstRemoteTransmitSetupPages::remotePowerPageShowsAndChangesTheCoresValues()
{
    Session s(m_securityDir.path(), this);
    s.settings.setValue(QStringLiteral("SwrProtectionEnabled"), QStringLiteral("True"));
    s.settings.setValue(QStringLiteral("TunePowerSwrIgnore"), QStringLiteral("22"));
    s.stepAtt.setAttOnTxValue(14);
    AppSettings::instance().setRemoteBackend(&s.proxy);
    QVERIFY(s.connect());
    QVERIFY(s.proxy.ready());
    TransmitModel& coreTx = s.core->transmitModel();
    safety::SwrProtectionController& swr = s.core->swrProt();

    PowerPage page(&s.window);
    auto* maxPower = page.findChild<QSlider*>(QStringLiteral("maxPowerSlider"));
    auto* attOnTx = page.findChild<QCheckBox*>(QStringLiteral("chkATTOnTX"));
    auto* attValue = page.findChild<QSpinBox*>(QStringLiteral("udATTOnTX"));
    auto* forceAtt = page.findChild<QCheckBox*>(QStringLiteral("chkForceATTwhenPSAoff"));
    auto* fixed = page.findChild<QRadioButton*>(QStringLiteral("radUseFixedDriveTune"));
    auto* fixedPower = page.findChild<QDoubleSpinBox*>(QStringLiteral("udTXTunePower"));
    auto* swrOn = page.findChild<QCheckBox*>(QStringLiteral("chkSWRProtection"));
    auto* swrLimit = page.findChild<QDoubleSpinBox*>(QStringLiteral("udSwrProtectionLimit"));
    auto* swrTune = page.findChild<QCheckBox*>(QStringLiteral("chkSWRTuneProtection"));
    auto* swrIgnore = page.findChild<QSpinBox*>(QStringLiteral("udTunePowerSwrIgnore"));
    auto* windBack = page.findChild<QCheckBox*>(QStringLiteral("chkWindBackPowerSWR"));
    auto* inhibit = page.findChild<QCheckBox*>(QStringLiteral("chkTXInhibit"));
    auto* inhibitReverse = page.findChild<QCheckBox*>(QStringLiteral("chkTXInhibitReverse"));
    auto* hfPa = page.findChild<QCheckBox*>(QStringLiteral("chkHFTRRelay"));
    QVERIFY(maxPower && attOnTx && attValue && forceAtt && fixed && fixedPower && swrOn
            && swrLimit && swrTune && swrIgnore && windBack && inhibit && inhibitReverse && hfPa);

    // Closed until the dialog pushes the version 5 gate, with the reason.
    QVERIFY(!swrOn->isEnabled());
    QCOMPARE(swrOn->toolTip(), IStationLink::transmitSettingsUnavailableReason());
    QVERIFY(!attOnTx->isEnabled() && !maxPower->isEnabled() && !fixed->isEnabled());
    // Disable HF PA follows version 11, closed until the dialog pushes it.
    QVERIFY(!hfPa->isEnabled());
    QCOMPARE(hfPa->toolTip(), IStationLink::transmitSettingsUnavailableReason());
    page.setTransmitSettingsPermittedAt(5, true, QString());
    QVERIFY(swrOn->isEnabled() && attOnTx->isEnabled() && maxPower->isEnabled());
    QVERIFY(!hfPa->isEnabled());
    page.setTransmitSettingsPermittedAt(11, true, QString());
    QVERIFY(hfPa->isEnabled());
    QCOMPARE(hfPa->toolTip(), QStringLiteral("Disables HF PA."));

    // The Core's values.
    QVERIFY(swrOn->isChecked());
    QCOMPARE(swrIgnore->value(), 22);
    QCOMPARE(attValue->value(), 14);
    QCOMPARE(attOnTx->isChecked(), s.stepAtt.attOnTxEnabled());

    // Changes reach the Core's live objects.
    maxPower->setValue(37);
    QTRY_COMPARE(coreTx.power(), 37);
    attOnTx->setChecked(!attOnTx->isChecked());
    QTRY_COMPARE(s.stepAtt.attOnTxEnabled(), attOnTx->isChecked());
    attValue->setValue(8);
    QTRY_COMPARE(s.stepAtt.attOnTxValue(), 8);
    forceAtt->setChecked(!forceAtt->isChecked());
    QTRY_COMPARE(s.stepAtt.forceAttWhenPsOff(), forceAtt->isChecked());
    fixed->setChecked(true);
    QTRY_COMPARE(coreTx.tuneDrivePowerSource(), DrivePowerSource::Fixed);
    QTRY_VERIFY(fixedPower->isEnabled());
    fixedPower->setValue(17.0);
    QTRY_COMPARE(coreTx.tunePower(), 17);
    swrOn->setChecked(false);
    QTRY_VERIFY(!swr.isEnabled());
    swrLimit->setValue(2.7);
    QTRY_COMPARE(swr.limit(), 2.7f);
    swrTune->setChecked(true);
    QTRY_VERIFY(swr.disableOnTune());
    swrIgnore->setValue(30);
    QTRY_COMPARE(swr.tunePowerSwrIgnore(), 30.0f);
    windBack->setChecked(true);
    QTRY_VERIFY(swr.windBackEnabled());
    inhibit->setChecked(true);
    QTRY_COMPARE(s.settings.value(QStringLiteral("TxInhibitMonitorEnabled")).toString(),
                 QStringLiteral("True"));
    inhibitReverse->setChecked(true);
    QTRY_COMPARE(s.settings.value(QStringLiteral("TxInhibitMonitorReversed")).toString(),
                 QStringLiteral("True"));
    // Disable HF PA reaches the Core's SWR protection (and its radio).
    QVERIFY(!swr.hfPaDisabled());
    hfPa->setChecked(true);
    QTRY_VERIFY(swr.hfPaDisabled());
    QCOMPARE(s.settings.value(QStringLiteral("DisableHfPa")).toString(), QStringLiteral("True"));

    // The Core's own changes show on the page.
    s.stepAtt.setAttOnTxValue(25);
    QTRY_COMPARE(attValue->value(), 25);
    coreTx.setPower(44);
    QTRY_COMPARE(maxPower->value(), 44);
    s.settings.setValue(QStringLiteral("WindBackPowerSwr"), QStringLiteral("False"));
    QTRY_VERIFY(!windBack->isChecked());
    s.settings.setValue(QStringLiteral("DisableHfPa"), QStringLiteral("False"));
    QTRY_VERIFY(!hfPa->isChecked());

    // On the air the change is taken, as on a local page (version 13).
    s.keyCore();
    QTRY_VERIFY(s.window.isCoreOnAir());
    swrTune->setChecked(false);
    QTRY_VERIFY(!swr.disableOnTune());
    QVERIFY(!swrTune->isChecked());
    // Disable HF PA is taken on the air, as Thetis applies it.
    hfPa->setChecked(true);
    QTRY_VERIFY(swr.hfPaDisabled());
    hfPa->setChecked(false);
    QTRY_VERIFY(!swr.hfPaDisabled());
    s.unkeyCore();
    QTRY_VERIFY(!s.window.isCoreOnAir());
    QTRY_COMPARE(s.core->moxController()->state(), MoxState::Rx);

    // The gate closes every Core setting with its reason.
    page.setTransmitSettingsPermittedAt(5, false, kOnAir);
    QVERIFY(!inhibit->isEnabled() && !fixedPower->isEnabled() && !attValue->isEnabled());
    QCOMPARE(inhibit->toolTip(), kOnAir);
    QCOMPARE(fixedPower->toolTip(), kOnAir);
    QString keyed;
    QVERIFY2(nothingKeyed(s, &keyed), qPrintable(keyed));
}

// B5.14: Setup > Transmit > DEXP/VOX in a remote window changes the Core's
// values; Enable VOX keeps the remote transmit reason.
void TstRemoteTransmitSetupPages::remoteDexpPageChangesTheCoreAndVoxWaitsForTransmit()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    TransmitModel& coreTx = s.core->transmitModel();

    DexpVoxPage page(&s.window);
    auto* vox = page.findChild<QCheckBox*>(QStringLiteral("chkVOXEnable"));
    auto* dexp = page.findChild<QCheckBox*>(QStringLiteral("chkDEXPEnable"));
    auto* threshold = page.findChild<QSpinBox*>(QStringLiteral("udDEXPThreshold"));
    auto* hyst = page.findChild<QDoubleSpinBox*>(QStringLiteral("udDEXPHysteresisRatio"));
    auto* exp = page.findChild<QDoubleSpinBox*>(QStringLiteral("udDEXPExpansionRatio"));
    auto* attack = page.findChild<QSpinBox*>(QStringLiteral("udDEXPAttack"));
    auto* hold = page.findChild<QSpinBox*>(QStringLiteral("udDEXPHold"));
    auto* release = page.findChild<QSpinBox*>(QStringLiteral("udDEXPRelease"));
    auto* tau = page.findChild<QSpinBox*>(QStringLiteral("udDEXPDetTau"));
    auto* lookAheadOn = page.findChild<QCheckBox*>(QStringLiteral("chkDEXPLookAheadEnable"));
    auto* lookAhead = page.findChild<QSpinBox*>(QStringLiteral("udDEXPLookAhead"));
    auto* scf = page.findChild<QCheckBox*>(QStringLiteral("chkSCFEnable"));
    auto* lowCut = page.findChild<QSpinBox*>(QStringLiteral("udSCFLowCut"));
    auto* highCut = page.findChild<QSpinBox*>(QStringLiteral("udSCFHighCut"));
    auto* antiVox = page.findChild<QCheckBox*>(QStringLiteral("chkAntiVoxEnable"));
    auto* antiVoxGain = page.findChild<QSpinBox*>(QStringLiteral("udAntiVoxGain"));
    auto* antiVoxTau = page.findChild<QSpinBox*>(QStringLiteral("udAntiVoxTau"));
    QVERIFY(vox && dexp && threshold && hyst && exp && attack && hold && release && tau
            && lookAheadOn && lookAhead && scf && lowCut && highCut && antiVox && antiVoxGain
            && antiVoxTau);

    QVERIFY(!vox->isEnabled() && !dexp->isEnabled() && !attack->isEnabled());
    page.setTransmitPermitted(false, kTransmitReason);
    page.setTransmitSettingsPermittedAt(5, true, QString());
    QVERIFY(dexp->isEnabled() && attack->isEnabled() && antiVoxGain->isEnabled());
    QVERIFY(!vox->isEnabled());
    QCOMPARE(vox->toolTip(), kTransmitReason);
    // Fix wave 2 (M8): with transmit permitted, Enable VOX still waits for
    // this computer's microphone line, disabled with the plain reason.
    const QString noLine = TxRefusals::micNotConnected().text;
    page.setVoxPermitted(false, noLine);
    QCOMPARE(vox->toolTip(), kTransmitReason);
    page.setTransmitPermitted(true, QString());
    QVERIFY(!vox->isEnabled());
    QCOMPARE(vox->toolTip(), noLine);
    page.setVoxPermitted(true, QString());
    QVERIFY(vox->isEnabled());
    QVERIFY(vox->toolTip() != noLine);
    page.setTransmitPermitted(false, kTransmitReason);
    QVERIFY(!vox->isEnabled());
    QCOMPARE(vox->toolTip(), kTransmitReason);

    dexp->setChecked(!coreTx.dexpEnabled());
    const bool dexpWant = dexp->isChecked();
    threshold->setValue(-33);
    hyst->setValue(3.5);
    exp->setValue(12.5);
    attack->setValue(9);
    hold->setValue(700);
    release->setValue(300);
    tau->setValue(25);
    lookAheadOn->setChecked(!coreTx.dexpLookAheadEnabled());
    const bool lookAheadWant = lookAheadOn->isChecked();
    lookAhead->setValue(150);
    scf->setChecked(!coreTx.dexpSideChannelFilterEnabled());
    const bool scfWant = scf->isChecked();
    lowCut->setValue(350);
    highCut->setValue(2800);
    antiVox->setChecked(!coreTx.antiVoxRun());
    const bool antiVoxWant = antiVox->isChecked();
    antiVoxGain->setValue(-9);
    antiVoxTau->setValue(40);
    QTRY_COMPARE(coreTx.antiVoxTauMs(), 40);
    QCOMPARE(coreTx.dexpEnabled(), dexpWant);
    QCOMPARE(coreTx.voxThresholdDb(), -33);
    QCOMPARE(coreTx.dexpHysteresisRatioDb(), 3.5);
    QCOMPARE(coreTx.dexpExpansionRatioDb(), 12.5);
    QCOMPARE(coreTx.dexpAttackTimeMs(), 9.0);
    QCOMPARE(coreTx.voxHangTimeMs(), 700);
    QCOMPARE(coreTx.dexpReleaseTimeMs(), 300.0);
    QCOMPARE(coreTx.dexpDetectorTauMs(), 25.0);
    QCOMPARE(coreTx.dexpLookAheadEnabled(), lookAheadWant);
    QCOMPARE(coreTx.dexpLookAheadMs(), 150.0);
    QCOMPARE(coreTx.dexpSideChannelFilterEnabled(), scfWant);
    QCOMPARE(coreTx.dexpLowCutHz(), 350.0);
    QCOMPARE(coreTx.dexpHighCutHz(), 2800.0);
    QCOMPARE(coreTx.antiVoxRun(), antiVoxWant);
    QCOMPARE(coreTx.antiVoxGainDb(), -9);
    QCOMPARE(s.txChannel.lastDexpRunForTest(), dexpWant);
    QCOMPARE(s.txChannel.lastDexpAttackTimeForTest(), 9.0);

    // A Core change shows on the page.
    coreTx.setDexpReleaseTimeMs(420.0);
    QTRY_COMPARE(release->value(), 420);

    // On the air every setting greys with the on-air reason.
    page.setTransmitSettingsPermittedAt(5, false, kOnAir);
    QVERIFY(!attack->isEnabled());
    QCOMPARE(attack->toolTip(), kOnAir);
    QVERIFY(!coreTx.voxEnabled());
    QString keyed;
    QVERIFY2(nothingKeyed(s, &keyed), qPrintable(keyed));
}

// B5.16: Setup > Test > Two-Tone IMD's settings change the Core's values;
// the page has no start of its own.
void TstRemoteTransmitSetupPages::remoteTwoTonePageChangesTheCore()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    TransmitModel& coreTx = s.core->transmitModel();

    TestTwoTonePage page(&s.window);
    QVERIFY(!page.freq1Spin()->isEnabled());
    QCOMPARE(page.freq1Spin()->toolTip(), IStationLink::transmitSettingsUnavailableReason());
    page.setTransmitSettingsPermittedAt(5, true, QString());
    QVERIFY(page.freq1Spin()->isEnabled() && page.fixedDriveRadio()->isEnabled());

    page.freq1Spin()->setValue(650);
    page.freq2Spin()->setValue(1750);
    page.levelSpin()->setValue(-4.25);
    page.powerSpin()->setValue(28);
    page.pulsedCheck()->setChecked(!coreTx.twoTonePulsed());
    const bool pulsed = page.pulsedCheck()->isChecked();
    page.invertCheck()->setChecked(!coreTx.twoToneInvert());
    const bool invert = page.invertCheck()->isChecked();
    page.freq2DelaySpin()->setValue(60);
    page.fixedDriveRadio()->setChecked(true);
    QTRY_COMPARE(coreTx.twoToneDrivePowerSource(), DrivePowerSource::Fixed);
    QCOMPARE(coreTx.twoToneFreq1(), 650);
    QCOMPARE(coreTx.twoToneFreq2(), 1750);
    QCOMPARE(coreTx.twoToneLevel(), -4.25);
    QCOMPARE(coreTx.twoTonePower(), 28);
    QCOMPARE(coreTx.twoTonePulsed(), pulsed);
    QCOMPARE(coreTx.twoToneInvert(), invert);
    QCOMPARE(coreTx.twoToneFreq2Delay(), 60);
    page.stealthButton()->click();
    QTRY_COMPARE(coreTx.twoToneFreq2(), 190);
    QCOMPARE(coreTx.twoToneFreq1(), 70);

    // A Core change shows on the page.
    coreTx.setTwoTonePower(55);
    QTRY_COMPARE(page.powerSpin()->value(), 55);

    page.setTransmitSettingsPermittedAt(5, false, kOnAir);
    QVERIFY(!page.defaultsButton()->isEnabled());
    QCOMPARE(page.defaultsButton()->toolTip(), kOnAir);
    QString keyed;
    QVERIFY2(nothingKeyed(s, &keyed), qPrintable(keyed));
}

// B5.12 / B5.14 / B5.16: the three pages open with remote transmit denied.
void TstRemoteTransmitSetupPages::setupOpensThePagesWithoutRemoteTransmit()
{
    RadioModel remote(RadioModel::Role::Remote);
    SetupDialog dialog(&remote);
    dialog.setStationSettingsAvailable(true, QString());
    dialog.setTransmitPermitted(false, kTransmitReason);
    dialog.setTransmitSettingsPermitted(true, QString(), 5);
    for (const QString& label : {QStringLiteral("Power"), QStringLiteral("DEXP/VOX"),
                                 QStringLiteral("Two-Tone IMD")}) {
        dialog.selectPage(label);
        QWidget* const page = dialog.realizedPageForTest(label);
        QVERIFY2(page, qPrintable(label));
        QVERIFY2(page->isEnabled(), qPrintable(label));
    }
    QWidget* const dexp = dialog.realizedPageForTest(QStringLiteral("DEXP/VOX"));
    auto* vox = dexp->findChild<QCheckBox*>(QStringLiteral("chkVOXEnable"));
    auto* attack = dexp->findChild<QSpinBox*>(QStringLiteral("udDEXPAttack"));
    QVERIFY(vox && attack);
    QVERIFY(!vox->isEnabled());
    QCOMPARE(vox->toolTip(), kTransmitReason);
    QVERIFY(attack->isEnabled());
    QWidget* const power = dialog.realizedPageForTest(QStringLiteral("Power"));
    auto* swrOn = power->findChild<QCheckBox*>(QStringLiteral("chkSWRProtection"));
    QVERIFY(swrOn && swrOn->isEnabled());
    // The version 5 gate closes the settings with its reason.
    dialog.setTransmitSettingsPermitted(false, kOnAir, 5);
    QVERIFY(!swrOn->isEnabled());
    QCOMPARE(swrOn->toolTip(), kOnAir);
    QVERIFY(!attack->isEnabled());
}

// A local window's SWR Protection change reaches its controller at once, as
// Thetis's chkSWRProtection_CheckedChanged does.
void TstRemoteTransmitSetupPages::localSwrProtectionAppliesAtOnce()
{
    RadioModel local;
    PowerPage page(&local);
    auto* swrOn = page.findChild<QCheckBox*>(QStringLiteral("chkSWRProtection"));
    auto* swrIgnore = page.findChild<QSpinBox*>(QStringLiteral("udTunePowerSwrIgnore"));
    QVERIFY(swrOn && swrIgnore);
    QVERIFY(swrOn->isEnabled());
    swrOn->setChecked(!local.swrProt().isEnabled());
    QCOMPARE(local.swrProt().isEnabled(), swrOn->isChecked());
    QCOMPARE(AppSettings::instance().value(QStringLiteral("SwrProtectionEnabled")).toString(),
             swrOn->isChecked() ? QStringLiteral("True") : QStringLiteral("False"));
    swrIgnore->setValue(swrIgnore->value() == 12 ? 13 : 12);
    QCOMPARE(local.swrProt().tunePowerSwrIgnore(), static_cast<float>(swrIgnore->value()));
}

void TstRemoteTransmitSetupPages::newReasonsArePlain()
{
    TransmitModel tx;
    const struct { QByteArray name; QVariant value; } bad[] = {
        {"dexpAttackTimeMs", 0.0}, {"dexpDetectorTauMs", 0.0}, {"dexpReleaseTimeMs", 0.0},
        {"dexpExpansionRatioDb", 99.0}, {"dexpHysteresisRatioDb", 99.0},
        {"dexpLookAheadMs", 0.0}, {"dexpLowCutHz", 0.0}, {"dexpHighCutHz", 0.0},
        {"antiVoxGainDb", 99}, {"twoToneFreq1", 99999}, {"twoToneFreq2", 99999},
        {"twoToneLevel", 9.0}, {"twoTonePower", 999}, {"twoToneFreq2Delay", 9999},
        {"powerByBandJson", QStringLiteral("x")}, {"tunePowerByBandJson", QStringLiteral("x")},
    };
    for (const auto& b : bad) {
        const QString reason = tx.settingRangeRefusal(b.name, b.value);
        QVERIFY2(!reason.isEmpty(), b.name.constData());
        QVERIFY2(OperatorWording::isPlain(reason), qPrintable(reason));
        QVERIFY(!reason.contains(QChar(0x2014)));
    }
    StepAttenuatorFacade facade(nullptr);
    const QString att = facade.transmitSettingRefusal("attOnTxValue", 99);
    QVERIFY(!att.isEmpty());
    QVERIFY2(OperatorWording::isPlain(att), qPrintable(att));
    for (const QString& reason : {QStringLiteral("Choose a tune power to ignore from 5 to 50 W."),
                                  QStringLiteral("The Core expected this box to be on or off.")}) {
        QVERIFY2(OperatorWording::isPlain(reason), qPrintable(reason));
    }
}

// Group A fix wave, M8: two tooltips now enabled in remote windows said the
// wrong thing: the TX inhibit box named Thetis, and Manage... sent the
// operator to Save buttons the TX EQ editor no longer has.
void TstRemoteTransmitSetupPages::tooltipsSayWhatTheControlsDo()
{
    RadioModel model;
    PowerPage power(&model);
    auto* inhibit = power.findChild<QCheckBox*>(QStringLiteral("chkTXInhibit"));
    QVERIFY(inhibit);
    const QString inhibitTip = inhibit->toolTip();
    QVERIFY2(OperatorWording::isPlain(inhibitTip), qPrintable(inhibitTip));
    QVERIFY(!inhibitTip.contains(QStringLiteral("Thetis")));
    QVERIFY(inhibitTip.contains(QStringLiteral("NereusSDR")));

    SpeechProcessorPage speech(&model);
    auto* manage = speech.findChild<QPushButton*>(QStringLiteral("btnManageProfile"));
    QVERIFY(manage);
    const QString manageTip = manage->toolTip();
    QVERIFY2(OperatorWording::isPlain(manageTip), qPrintable(manageTip));
    QVERIFY(!manageTip.contains(QStringLiteral("Save As")));
    QVERIFY(manageTip.contains(QStringLiteral("Setup > Audio > TX Profile")));
    for (const QString& tip : {inhibitTip, manageTip}) {
        QVERIFY(!tip.contains(QChar(0x2014)));
    }
}

// Group A fix wave: the ATT on TX box takes the radio's own range, the
// Core's radio's in a remote window: -28 to 31 dB on the HL2 (mi0bot-Thetis
// udATTOnTX.Minimum = -28), 0 to 31 dB elsewhere. A range change never
// writes a value.
void TstRemoteTransmitSetupPages::attOnTxBoxShowsTheRadiosRange()
{
    // A local window.
    {
        RadioModel model;
        StepAttenuatorController att;
        att.setTickTimerEnabled(false);
        model.setStepAttController(&att);
        PowerPage page(&model);
        auto* box = page.findChild<QSpinBox*>(QStringLiteral("udATTOnTX"));
        QVERIFY(box);
        QCOMPARE(box->minimum(), 0);
        QCOMPARE(box->maximum(), 31);
        att.setMinAttenuation(-28);  // an HL2 connects
        QCOMPARE(box->minimum(), -28);
        QCOMPARE(box->maximum(), 31);
        QVERIFY(box->toolTip().contains(QStringLiteral("-28..31")));
        box->setValue(-25);
        QCOMPARE(att.attOnTxValue(), -25);
        model.setStepAttController(nullptr);
    }

    // A remote window on an HL2's Core.
    Session s(m_securityDir.path(), this);
    s.stepAtt.setMinAttenuation(-28);
    AppSettings::instance().setRemoteBackend(&s.proxy);
    QVERIFY(s.connect());
    StepAttenuatorFacade* remote = s.window.stepAttFacade();
    QVERIFY(remote);
    QTRY_COMPARE(remote->minDb(), -28);
    PowerPage page(&s.window);
    auto* box = page.findChild<QSpinBox*>(QStringLiteral("udATTOnTX"));
    QVERIFY(box);
    QCOMPARE(box->minimum(), -28);
    QCOMPARE(box->maximum(), 31);
    box->setValue(-20);
    QTRY_COMPARE(s.stepAtt.attOnTxValue(), -20);

    // The Core's radio changes to one with a 0 dB bottom: the box follows
    // and writes nothing back.
    s.stepAtt.setMinAttenuation(0);
    QTRY_COMPARE(box->minimum(), 0);
    QTest::qWait(100);
    QCOMPARE(s.stepAtt.attOnTxValue(), -20);
    AppSettings::instance().setRemoteBackend(nullptr);
}

QTEST_MAIN(TstRemoteTransmitSetupPages)
#include "tst_remote_transmit_setup_pages.moc"
