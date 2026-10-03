// no-port-check: NereusSDR-original. R-R3-49 (parity Task 2): the TX and
// Phone/CW applets' settings on the `transmit` object. Loopback link, no RF
// and no hardware: nothing here keys a radio. "On the air" keys the Core's
// own MoxController with the receive-only MOX pre-check lifted, as
// tst_transmit_settings_gate and tst_tgxl_station_identity do. The Core's
// transmit chain is wired to a test TxChannel with no WDSP channel behind it
// (RadioModel::wireTransmitChainForTest), so a setting is checked on the
// channel's own state. No audio device is opened.
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 2): each setting
//                                    round-trips from a window to the
//                                    Core's TX chain and back, a value out
//                                    of range and a write on the air are
//                                    refused, the Tune Power slider follows
//                                    the Core's transmit band, and the
//                                    applets and the container MON button
//                                    work in a remote window.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-28  J.J. Boyd / KG4VCF  iPhone app plan Task 40: micMuted on the
//                                    link silences the Core's mic, version
//                                    10. AI-assisted via Anthropic Claude
//                                    Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 3): the Core offers at
//                                    least transmitSettingsVersion 2.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 4): the TX EQ, CFC,
//                                    phase rotator, CESSB, leveler and ALC
//                                    properties on the link, and version 4.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 5): the version
//                                    5 properties on `transmit` and
//                                    `stepAtt`, tuneDrivePowerSource
//                                    two-way, and version 5.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  Integration carry: the window signs in
//                                    to an upgraded Core with its token
//                                    (seedUpgradedCoreToken), as Part C's
//                                    paired-device sign-in requires.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  Remote parity on the air
//                                    (transmitSettingsVersion 13): the
//                                    applets' settings are taken on the
//                                    air; Tune Power is the holder's. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  PA on-air gate review: the per-band
//                                    power maps are the Core's own and
//                                    refuse a peer's write.
//                                    AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QApplication>
#include <QCoreApplication>
#include <QFile>
#include <QPushButton>
#include <QSignalSpy>
#include <QSlider>
#include <QTemporaryDir>

#include <cmath>
#include <functional>
#include <memory>

#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/ConnectionState.h"
#include "core/HardwareProfile.h"
#include "core/MoxController.h"
#include "core/StepAttenuatorFacade.h"
#include "core/TxChannel.h"
#include "core/session/IStationLink.h"
#include "core/session/MirrorPolicy.h"
#include "core/session/MirrorSchema.h"
#include "core/session/SessionMessages.h"
#include "core/session/StationCapabilities.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/settings/SettingsProxy.h"
#include "fakes/LoopbackTransport.h"
#include "fakes/UpgradedCoreToken.h"
#include "gui/applets/PhoneCwApplet.h"
#include "gui/applets/TxApplet.h"
#include "gui/containers/ContainerButtonDispatcher.h"
#include "models/Band.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

#include "OperatorWording.h"

using namespace NereusSDR;
using NereusSDR::Test::LoopbackTransport;

namespace {

const QString kOnAir = QStringLiteral("The radio is on the air. Try again when it stops.");

std::unique_ptr<RadioModel> makeStationRadioModel()
{
    auto model = std::make_unique<RadioModel>();
    model->setBoardForTest(HPSDRHW::HermesLite);
    // As a connect does: the transmit model takes the radio's model (the
    // HL2's tune power range, 0 to 99).
    model->setHpsdrModelForTest(HPSDRModel::HERMESLITE);
    RadioInfo info;
    info.macAddress = QStringLiteral("AA:BB:CC:DD:EE:01");
    info.name = QStringLiteral("Bench HL2");
    info.boardType = HPSDRHW::HermesLite;
    model->setLastRadioInfoForTest(info);
    model->setConnectionStateForTest(ConnectionState::Connected);
    model->addSlice(QStringLiteral("pan-0"));
    return model;
}

// A receive-only Core with its transmit chain wired to a test TxChannel,
// and one window, handshake complete.
struct Session {
    explicit Session(const QString& securityDir, QObject* parent)
        : settings(settingsDir.filePath(QStringLiteral("NereusSDR.settings")))
        , txChannel(/*channelId=*/1)
    {
        core = makeStationRadioModel();
        core->wireTransmitChainForTest(&txChannel);
        server = std::make_unique<StationServer>(
            core.get(), settings, NereusSDR::Test::seedUpgradedCoreToken(securityDir));
        client = std::make_unique<StationClient>(&window, &proxy);
        coreEnd = new LoopbackTransport(QStringLiteral("station-end"), parent);
        windowEnd = new LoopbackTransport(QStringLiteral("client-end"), parent);
        coreEnd->linkTo(windowEnd);
    }
    ~Session()
    {
        // The channel dies before the Core: unhook it first.
        core->injectTxChannelForTest(nullptr);
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
        const quint32 writeId = ++nextId;
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
    // A raw command, and the Core's answer.
    SessionMessage invoke(const QByteArray& verb, const QList<MirrorUpdate>& arguments)
    {
        const quint32 id = ++nextId;
        windowEnd->sendText(SessionMessages::encode(
            SessionMessages::commandInvoke(verb, id, arguments)));
        SessionMessage found;
        found.reason = QStringLiteral("no command.result arrived");
        (void)QTest::qWaitFor([&] {
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

    QTemporaryDir settingsDir;
    AppSettings settings;
    TxChannel txChannel;
    std::unique_ptr<RadioModel> core;
    std::unique_ptr<StationServer> server;
    RadioModel window{RadioModel::Role::Remote};
    SettingsProxy proxy;
    std::unique_ptr<StationClient> client;
    LoopbackTransport* coreEnd = nullptr;
    LoopbackTransport* windowEnd = nullptr;
    quint32 nextId = 91000;
};

MirrorUpdate intArg(const QByteArray& name, qlonglong value)
{
    return MirrorUpdate{0, name, MirrorWireKind::Int64, QVariant(value)};
}

// One setting: how the window changes it (as its applet control does), how
// to read it on a model, and how it reads on the Core's TX chain.
struct Setting {
    QByteArray name;
    std::function<void(TransmitModel&)> setFromWindow;
    std::function<QVariant(const TransmitModel&)> read;
    QVariant expected;
    std::function<bool(Session&)> reachedChain;
};

}  // namespace

class TstTransmitModelProperties : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void propertiesAreOnTheLinkUnderTheirSetters();
    void coreOffersTransmitSettingsVersion2();
    void task4PropertiesAreOnTheLinkUnderTheirGetters();
    void coreOffersTransmitSettingsVersion4();
    void task5PropertiesAreOnTheLinkUnderTheirGetters();
    void eachSettingRoundTripsToTheCoreTxChain();
    void outOfRangeWritesAreRefusedWithTheRange();
    void eachSettingIsTakenOnTheAir();
    void keyingSetStaysRefused();
    void connectingNeverWritesTheWindowDefaults();
    void tunePowerCommandSetsTheTxBandAndSource();
    void tunePowerCommandRefusals();
    void coreBandChangeMovesTheTunePowerSlider();
    void firstKnownTransmitBandRepaintsTheTuneSlider();
    void remoteTxAppletControlsReachTheCore();
    void remotePhoneCwControlsReachTheCore();
    void containerMonButtonTogglesTheCoresMon();
    void micMutedIsOnTheLinkAndSilencesTheCoreMic();

private:
    QTemporaryDir m_securityDir;
};

void TstTransmitModelProperties::initTestCase()
{
    QVERIFY(m_securityDir.isValid());
    const QString profile = QStringLiteral("transmit-model-properties-%1")
                                .arg(QCoreApplication::applicationPid());
    AppSettings::setProfileOverride(profile);
    AppSettings::instance().clear();
    AppSettings::instance().setValue(QStringLiteral("SettingsSchemaVersion"), QStringLiteral("7"));
}

void TstTransmitModelProperties::cleanupTestCase()
{
    const QString path = AppSettings::instance().filePath();
    QFile::remove(path);
    QFile::remove(path + QStringLiteral(".bak"));
}

void TstTransmitModelProperties::propertiesAreOnTheLinkUnderTheirSetters()
{
    TransmitModel tx;
    const MirrorSchema& schema = MirrorSchema::forObject(&tx);
    const struct {
        const char* name;
        MirrorWireKind kind;
        bool twoWay;
    } expected[] = {
        {"tunePower", MirrorWireKind::Int64, true},
        {"voxThresholdDb", MirrorWireKind::Int64, true},
        {"voxHangTimeMs", MirrorWireKind::Int64, true},
        {"monEnabled", MirrorWireKind::Bool, true},
        {"monitorVolume", MirrorWireKind::Float64, true},
        {"txLevelerOn", MirrorWireKind::Bool, true},
        {"txEqEnabled", MirrorWireKind::Bool, true},
        {"cfcEnabled", MirrorWireKind::Bool, true},
        {"cpdrOn", MirrorWireKind::Bool, true},
        {"cpdrLevelDb", MirrorWireKind::Int64, true},
        {"amCarrierLevel", MirrorWireKind::Int64, true},
        {"dexpEnabled", MirrorWireKind::Bool, true},
        {"micGainDb", MirrorWireKind::Int64, true},
        {"tunePowerForTxBand", MirrorWireKind::Int64, false},
        // R-R3-49 (parity Task 5): writable since transmitSettingsVersion 5
        // (Setup > Transmit > Power's Tune group).
        {"tuneDrivePowerSource", MirrorWireKind::Enum, true},
    };
    // After paSettingsBypass, in this order: the earlier ordinals stay.
    const MirrorProperty* bypass = schema.byName("paSettingsBypass");
    QVERIFY(bypass);
    quint16 ordinal = bypass->ordinal;
    for (const auto& e : expected) {
        const MirrorProperty* prop = schema.byName(e.name);
        QVERIFY2(prop, e.name);
        QCOMPARE(prop->kind, e.kind);
        QCOMPARE(prop->ordinal, ++ordinal);
        QCOMPARE(MirrorPolicy::inboundAllowed("TransmitModel", e.name), e.twoWay);
        QCOMPARE(prop->isWritable, e.twoWay);
    }
}

void TstTransmitModelProperties::coreOffersTransmitSettingsVersion2()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    // At least 2 (3 since parity Task 3, tst_tx_profile_select).
    QVERIFY(s.server->buildCapabilities().transmitSettingsVersion >= 2);
    QCOMPARE(s.client->capabilities().transmitSettingsVersion,
             s.server->buildCapabilities().transmitSettingsVersion);
    QVERIFY(s.client->transmitSettingsAvailable(2));
}

// R-R3-49 (parity Task 4): the TX EQ, CFC, phase rotator, CESSB, leveler
// and ALC settings, after txProfilesJson in this order, each under its
// getter's type (the band arrays as utf8 JSON). Round trips, refusals and
// the dialogs are in tst_remote_tx_eq_cfc.
void TstTransmitModelProperties::task4PropertiesAreOnTheLinkUnderTheirGetters()
{
    TransmitModel tx;
    const MirrorSchema& schema = MirrorSchema::forObject(&tx);
    const struct {
        const char* name;
        MirrorWireKind kind;
    } expected[] = {
        {"txEqUseLegacy", MirrorWireKind::Bool},
        {"txEqPreamp", MirrorWireKind::Int64},
        {"txEqBandsJson", MirrorWireKind::Utf8},
        {"txEqFreqsJson", MirrorWireKind::Utf8},
        {"txEqNc", MirrorWireKind::Int64},
        {"txEqMp", MirrorWireKind::Bool},
        {"txEqCtfmode", MirrorWireKind::Int64},
        {"txEqWintype", MirrorWireKind::Int64},
        {"txEqParaEqData", MirrorWireKind::Utf8},
        {"cfcCompressionJson", MirrorWireKind::Utf8},
        {"cfcEqFreqJson", MirrorWireKind::Utf8},
        {"cfcPostEqBandGainJson", MirrorWireKind::Utf8},
        {"cfcPostEqEnabled", MirrorWireKind::Bool},
        {"cfcPostEqGainDb", MirrorWireKind::Int64},
        {"cfcPrecompDb", MirrorWireKind::Int64},
        {"cfcParaEqData", MirrorWireKind::Utf8},
        {"phaseRotatorEnabled", MirrorWireKind::Bool},
        {"phaseRotatorFreqHz", MirrorWireKind::Int64},
        {"phaseRotatorStages", MirrorWireKind::Int64},
        {"phaseReverseEnabled", MirrorWireKind::Bool},
        {"cessbOn", MirrorWireKind::Bool},
        {"txLevelerMaxGain", MirrorWireKind::Int64},
        {"txLevelerDecay", MirrorWireKind::Int64},
        {"txAlcMaxGain", MirrorWireKind::Int64},
        {"txAlcDecay", MirrorWireKind::Int64},
    };
    const MirrorProperty* last = schema.byName("txProfilesJson");
    QVERIFY(last);
    quint16 ordinal = last->ordinal;
    for (const auto& e : expected) {
        const MirrorProperty* prop = schema.byName(e.name);
        QVERIFY2(prop, e.name);
        QCOMPARE(prop->kind, e.kind);
        QCOMPARE(prop->ordinal, ++ordinal);
        QVERIFY2(MirrorPolicy::inboundAllowed("TransmitModel", e.name), e.name);
        QVERIFY(prop->isWritable);
    }
}

void TstTransmitModelProperties::coreOffersTransmitSettingsVersion4()
{
    // 5 since parity Task 5 (Power, DEXP/VOX, Two-Tone IMD), 6 since parity
    // Task 6 (Setup > PA), 7 since parity Task 7 (PureSignal arming), 8
    // since parity Task 13 (Hardware Config's OC transmit pins and transmit
    // calibration); 4 is within it.
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    QCOMPARE(s.server->buildCapabilities().transmitSettingsVersion, 15);
    QCOMPARE(s.client->capabilities().transmitSettingsVersion, 15);
    QVERIFY(s.client->transmitSettingsAvailable(4));
    QVERIFY(s.client->transmitSettingsAvailable(7));
    QVERIFY(s.client->transmitSettingsAvailable(8));
    QVERIFY(s.client->transmitSettingsAvailable(9));
    // 10 since iPhone app plan Task 40's mic mute (micMuted).
    QVERIFY(s.client->transmitSettingsAvailable(10));
    // 11 since hardware parity batch B's Disable HF PA (DisableHfPa).
    QVERIFY(s.client->transmitSettingsAvailable(11));
    // 12 since addendum G-42 (the Core's Extended transmit setting).
    QVERIFY(s.client->transmitSettingsAvailable(12));
    // 13 since the transmit settings are taken on the air as a local
    // window takes them.
    QVERIFY(s.client->transmitSettingsAvailable(13));
    // 14 since Prevent TX'ing on a different band became the Core's
    // setting (PreventTxOnDifferentBandToRx).
    QVERIFY(s.client->transmitSettingsAvailable(14));
    // 15 since a remote window edits the CFC bands (cfcProfile).
    QVERIFY(s.client->transmitSettingsAvailable(15));
    QVERIFY(!s.client->transmitSettingsAvailable(16));
}

// R-R3-49 (parity Task 5): the version 5 properties, after txAlcDecay in
// this order, each under its setter's name and its getter's type.
void TstTransmitModelProperties::task5PropertiesAreOnTheLinkUnderTheirGetters()
{
    TransmitModel tx;
    const MirrorSchema& schema = MirrorSchema::forObject(&tx);
    const struct {
        const char* name;
        MirrorWireKind kind;
    } expected[] = {
        {"powerByBandJson", MirrorWireKind::Utf8},
        {"tunePowerByBandJson", MirrorWireKind::Utf8},
        {"dexpAttackTimeMs", MirrorWireKind::Float64},
        {"dexpDetectorTauMs", MirrorWireKind::Float64},
        {"dexpExpansionRatioDb", MirrorWireKind::Float64},
        {"dexpHighCutHz", MirrorWireKind::Float64},
        {"dexpHysteresisRatioDb", MirrorWireKind::Float64},
        {"dexpLookAheadEnabled", MirrorWireKind::Bool},
        {"dexpLookAheadMs", MirrorWireKind::Float64},
        {"dexpLowCutHz", MirrorWireKind::Float64},
        {"dexpReleaseTimeMs", MirrorWireKind::Float64},
        {"dexpSideChannelFilterEnabled", MirrorWireKind::Bool},
        {"antiVoxGainDb", MirrorWireKind::Int64},
        {"twoToneFreq1", MirrorWireKind::Int64},
        {"twoToneFreq2", MirrorWireKind::Int64},
        {"twoToneLevel", MirrorWireKind::Float64},
        {"twoTonePower", MirrorWireKind::Int64},
        {"twoTonePulsed", MirrorWireKind::Bool},
        {"twoToneInvert", MirrorWireKind::Bool},
        {"twoToneFreq2Delay", MirrorWireKind::Int64},
        {"twoToneDrivePowerSource", MirrorWireKind::Enum},
    };
    const MirrorProperty* last = schema.byName("txAlcDecay");
    QVERIFY(last);
    quint16 ordinal = last->ordinal;
    for (const auto& e : expected) {
        const MirrorProperty* prop = schema.byName(e.name);
        QVERIFY2(prop, e.name);
        QCOMPARE(prop->kind, e.kind);
        QCOMPARE(prop->ordinal, ++ordinal);
        // The per-band power maps are the Core's own (PA on-air gate
        // review): the Core refuses a peer's write of either.
        const bool coreOwned = qstrcmp(e.name, "powerByBandJson") == 0
                               || qstrcmp(e.name, "tunePowerByBandJson") == 0;
        QCOMPARE(MirrorPolicy::inboundAllowed("TransmitModel", e.name), !coreOwned);
        QVERIFY(prop->isWritable);
    }
    // swrProtectFactor is the Core's runtime foldback (Thetis
    // NetworkIO.SWRProtect), not a setting: not on the link.
    QVERIFY(!schema.byName("swrProtectFactor"));
    // ATT on TX, its value and Force ATT are on `stepAtt`, after adcLinked
    // (the earlier ordinals stay put), two-way.
    StepAttenuatorFacade facade(nullptr);
    const MirrorSchema& att = MirrorSchema::forObject(&facade);
    const MirrorProperty* hold = att.byName("adcLinked");
    QVERIFY(hold);
    quint16 attOrdinal = hold->ordinal;
    const struct {
        const char* name;
        MirrorWireKind kind;
    } attExpected[] = {
        {"attOnTxEnabled", MirrorWireKind::Bool},
        {"attOnTxValue", MirrorWireKind::Int64},
        {"forceAttWhenPsOff", MirrorWireKind::Bool},
    };
    for (const auto& e : attExpected) {
        const MirrorProperty* prop = att.byName(e.name);
        QVERIFY2(prop, e.name);
        QCOMPARE(prop->kind, e.kind);
        QCOMPARE(prop->ordinal, ++attOrdinal);
        QVERIFY2(MirrorPolicy::inboundAllowed("StepAttenuatorFacade", e.name), e.name);
    }
}

void TstTransmitModelProperties::eachSettingRoundTripsToTheCoreTxChain()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    TransmitModel& coreTx = s.core->transmitModel();
    TransmitModel& windowTx = s.window.transmitModel();
    AudioEngine* const coreAudio = s.core->localAudioDevices();
    QVERIFY(coreAudio);

    // The VOX threshold reaches the TX channel through MoxController's
    // scaling; compare two readings' ratio so the mic-boost factor cancels.
    const int voxA = coreTx.voxThresholdDb() == -30 ? -31 : -30;
    const int voxB = voxA - 20;
    double voxAtA = 0.0;

    const int tunePower = coreTx.tunePower() == 17 ? 18 : 17;
    const int hang = coreTx.voxHangTimeMs() == 700 ? 701 : 700;
    const bool mon = !coreTx.monEnabled();
    const float monVol = qFuzzyCompare(coreTx.monitorVolume(), 0.25f) ? 0.75f : 0.25f;
    const bool lev = !coreTx.txLevelerOn();
    const bool eq = !coreTx.txEqEnabled();
    const bool cfc = !coreTx.cfcEnabled();
    const bool cpdr = !coreTx.cpdrOn();
    const int cpdrDb = coreTx.cpdrLevelDb() == 7 ? 8 : 7;
    const int amCar = coreTx.amCarrierLevel() == 42 ? 43 : 42;
    const bool dexp = !coreTx.dexpEnabled();
    const int mic = coreTx.micGainDb() == 4 ? 5 : 4;

    const QList<Setting> settings{
        {"tunePower", [&](TransmitModel& t) { t.setTunePower(tunePower); },
         [](const TransmitModel& t) { return QVariant(t.tunePower()); }, QVariant(tunePower),
         // The fixed tune power is read by the TUNE path when TUNE starts,
         // which this test does not key; the Core's own model is its state.
         [&](Session& ss) { return ss.core->transmitModel().tunePower() == tunePower; }},
        {"voxThresholdDb", [&](TransmitModel& t) { t.setVoxThresholdDb(voxA); },
         [](const TransmitModel& t) { return QVariant(t.voxThresholdDb()); }, QVariant(voxA),
         [&](Session& ss) {
             voxAtA = ss.txChannel.lastVoxAttackThresholdForTest();
             return std::isfinite(voxAtA) && voxAtA > 0.0;
         }},
        {"voxThresholdDb", [&](TransmitModel& t) { t.setVoxThresholdDb(voxB); },
         [](const TransmitModel& t) { return QVariant(t.voxThresholdDb()); }, QVariant(voxB),
         [&](Session& ss) {
             const double ratio = ss.txChannel.lastVoxAttackThresholdForTest() / voxAtA;
             return std::abs(ratio - std::pow(10.0, (voxB - voxA) / 20.0)) < 1e-9;
         }},
        {"voxHangTimeMs", [&](TransmitModel& t) { t.setVoxHangTimeMs(hang); },
         [](const TransmitModel& t) { return QVariant(t.voxHangTimeMs()); }, QVariant(hang),
         [&](Session& ss) {
             return std::abs(ss.txChannel.lastVoxHangTimeForTest() - hang / 1000.0) < 1e-9;
         }},
        {"monEnabled", [&](TransmitModel& t) { t.setMonEnabled(mon); },
         [](const TransmitModel& t) { return QVariant(t.monEnabled()); }, QVariant(mon),
         [&](Session&) { return coreAudio->txMonitorEnabled() == mon; }},
        {"monitorVolume", [&](TransmitModel& t) { t.setMonitorVolume(monVol); },
         [](const TransmitModel& t) { return QVariant(t.monitorVolume()); }, QVariant(monVol),
         [&](Session&) { return qFuzzyCompare(coreAudio->txMonitorVolume(), monVol); }},
        {"txLevelerOn", [&](TransmitModel& t) { t.setTxLevelerOn(lev); },
         [](const TransmitModel& t) { return QVariant(t.txLevelerOn()); }, QVariant(lev),
         [&](Session& ss) { return ss.txChannel.lastTxLevelerOnForTest() == lev; }},
        {"txEqEnabled", [&](TransmitModel& t) { t.setTxEqEnabled(eq); },
         [](const TransmitModel& t) { return QVariant(t.txEqEnabled()); }, QVariant(eq),
         [&](Session& ss) { return ss.txChannel.lastTxEqRunningForTest() == eq; }},
        {"cfcEnabled", [&](TransmitModel& t) { t.setCfcEnabled(cfc); },
         [](const TransmitModel& t) { return QVariant(t.cfcEnabled()); }, QVariant(cfc),
         [&](Session& ss) { return ss.txChannel.lastTxCfcRunningForTest() == cfc; }},
        {"cpdrOn", [&](TransmitModel& t) { t.setCpdrOn(cpdr); },
         [](const TransmitModel& t) { return QVariant(t.cpdrOn()); }, QVariant(cpdr),
         [&](Session& ss) { return ss.txChannel.lastTxCpdrOnForTest() == cpdr; }},
        {"cpdrLevelDb", [&](TransmitModel& t) { t.setCpdrLevelDb(cpdrDb); },
         [](const TransmitModel& t) { return QVariant(t.cpdrLevelDb()); }, QVariant(cpdrDb),
         [&](Session& ss) { return ss.txChannel.lastTxCpdrGainDbForTest() == double(cpdrDb); }},
        {"amCarrierLevel", [&](TransmitModel& t) { t.setAmCarrierLevel(amCar); },
         [](const TransmitModel& t) { return QVariant(t.amCarrierLevel()); }, QVariant(amCar),
         [&](Session& ss) { return ss.txChannel.lastTxAmCarrierLevelForTest() == amCar; }},
        {"dexpEnabled", [&](TransmitModel& t) { t.setDexpEnabled(dexp); },
         [](const TransmitModel& t) { return QVariant(t.dexpEnabled()); }, QVariant(dexp),
         [&](Session& ss) { return ss.txChannel.lastDexpRunForTest() == dexp; }},
        {"micGainDb", [&](TransmitModel& t) { t.setMicGainDb(mic); },
         [](const TransmitModel& t) { return QVariant(t.micGainDb()); }, QVariant(mic),
         [&](Session& ss) {
             return std::abs(ss.txChannel.lastMicPreampForTest() - std::pow(10.0, mic / 20.0))
                 < 1e-9;
         }},
    };

    for (const Setting& setting : settings) {
        // As the window's applet control does: its own model, which the link
        // sends on. The Core applies it, its chain follows, and the Core's
        // value comes back.
        setting.setFromWindow(windowTx);
        QTRY_VERIFY2(setting.read(coreTx) == setting.expected, setting.name.constData());
        QTRY_VERIFY2(setting.reachedChain(s), setting.name.constData());
        QTRY_VERIFY2(setting.read(windowTx) == setting.expected, setting.name.constData());
    }

    // A second window sees the change.
    RadioModel second{RadioModel::Role::Remote};
    SettingsProxy secondProxy;
    StationClient secondClient(&second, &secondProxy);
    auto* coreEnd = new LoopbackTransport(QStringLiteral("station-end-2"), this);
    auto* secondEnd = new LoopbackTransport(QStringLiteral("client-end-2"), this);
    coreEnd->linkTo(secondEnd);
    QSignalSpy completed(&secondClient, &StationClient::handshakeComplete);
    secondClient.startSession(secondEnd, s.server->token());
    s.server->acceptTransport(coreEnd);
    QVERIFY(completed.wait(5000) || completed.count() == 1);
    for (const Setting& setting : settings) {
        QTRY_VERIFY2(setting.read(second.transmitModel()) == setting.read(coreTx),
                     setting.name.constData());
    }
    // The window that connected later (the Core takes one window at a time)
    // changes it in turn.
    second.transmitModel().setTxLevelerOn(!lev);
    QTRY_COMPARE(coreTx.txLevelerOn(), !lev);
    QTRY_COMPARE(second.transmitModel().txLevelerOn(), !lev);
}

void TstTransmitModelProperties::outOfRangeWritesAreRefusedWithTheRange()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    TransmitModel& coreTx = s.core->transmitModel();
    // The Core's model is the HL2's (setBoardForTest): tune power 0 to 99.
    QCOMPARE(coreTx.tunePowerMax(), 99);
    const struct {
        const char* name;
        MirrorWireKind kind;
        QVariant value;
        const char* reason;
    } cases[] = {
        {"tunePower", MirrorWireKind::Int64, QVariant(qlonglong(100)),
         "Choose a tune power from 0 to 99."},
        {"tunePower", MirrorWireKind::Int64, QVariant(qlonglong(-1)),
         "Choose a tune power from 0 to 99."},
        {"voxThresholdDb", MirrorWireKind::Int64, QVariant(qlonglong(1)),
         "Choose a VOX level from -80 to 0 dB."},
        {"voxThresholdDb", MirrorWireKind::Int64, QVariant(qlonglong(-81)),
         "Choose a VOX level from -80 to 0 dB."},
        {"voxHangTimeMs", MirrorWireKind::Int64, QVariant(qlonglong(0)),
         "Choose a VOX delay from 1 to 2000 ms."},
        {"voxHangTimeMs", MirrorWireKind::Int64, QVariant(qlonglong(2001)),
         "Choose a VOX delay from 1 to 2000 ms."},
        {"monitorVolume", MirrorWireKind::Float64, QVariant(1.5),
         "Choose a monitor level from 0.0 to 1.0."},
        {"monitorVolume", MirrorWireKind::Float64, QVariant(-0.1),
         "Choose a monitor level from 0.0 to 1.0."},
        {"cpdrLevelDb", MirrorWireKind::Int64, QVariant(qlonglong(21)),
         "Choose a PROC level from 0 to 20 dB."},
        {"amCarrierLevel", MirrorWireKind::Int64, QVariant(qlonglong(101)),
         "Choose an AM carrier level from 0 to 100 percent."},
        {"micGainDb", MirrorWireKind::Int64, QVariant(qlonglong(71)),
         "Choose a mic level from -50 to 70 dB."},
        {"micGainDb", MirrorWireKind::Int64, QVariant(qlonglong(-51)),
         "Choose a mic level from -50 to 70 dB."},
    };
    for (const auto& c : cases) {
        const QVariant before = coreTx.property(c.name);
        const SessionPropertyResult result = s.writeTransmit(c.name, c.kind, c.value);
        QVERIFY2(!result.accepted, c.name);
        QCOMPARE(result.reason, QString::fromLatin1(c.reason));
        QVERIFY2(OperatorWording::isPlain(result.reason), qPrintable(result.reason));
        QCOMPARE(coreTx.property(c.name), before);
    }
    // The ends of each range are taken.
    for (const auto& c : {std::pair<const char*, qlonglong>{"tunePower", 99},
                          {"voxThresholdDb", -80}, {"voxHangTimeMs", 2000},
                          {"cpdrLevelDb", 20}, {"amCarrierLevel", 0}, {"micGainDb", -50}}) {
        const SessionPropertyResult result =
            s.writeTransmit(c.first, MirrorWireKind::Int64, QVariant(c.second));
        QVERIFY2(result.accepted, qPrintable(result.reason));
        QCOMPARE(coreTx.property(c.first).toLongLong(), c.second);
    }
    const SessionPropertyResult full =
        s.writeTransmit("monitorVolume", MirrorWireKind::Float64, QVariant(1.0));
    QVERIFY2(full.accepted, qPrintable(full.reason));

    // A non-HL2 Core says watts, 0 to 100.
    TransmitModel anan;
    anan.setHpsdrModel(HPSDRModel::ANAN_G2);
    QCOMPARE(anan.settingRangeRefusal("tunePower", QVariant(101)),
             QStringLiteral("Choose a tune power from 0 to 100 W."));
    QVERIFY(anan.settingRangeRefusal("tunePower", QVariant(100)).isEmpty());
}

void TstTransmitModelProperties::eachSettingIsTakenOnTheAir()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    TransmitModel& coreTx = s.core->transmitModel();
    s.keyCore();
    QTRY_VERIFY(s.core->moxController()->state() == MoxState::Tx);
    const struct {
        const char* name;
        MirrorWireKind kind;
        QVariant value;
    } cases[] = {
        {"tunePower", MirrorWireKind::Int64, QVariant(qlonglong(coreTx.tunePower() == 5 ? 6 : 5))},
        {"voxThresholdDb", MirrorWireKind::Int64,
         QVariant(qlonglong(coreTx.voxThresholdDb() == -50 ? -51 : -50))},
        {"voxHangTimeMs", MirrorWireKind::Int64,
         QVariant(qlonglong(coreTx.voxHangTimeMs() == 300 ? 301 : 300))},
        {"monEnabled", MirrorWireKind::Bool, QVariant(!coreTx.monEnabled())},
        {"monitorVolume", MirrorWireKind::Float64, QVariant(0.125)},
        {"txLevelerOn", MirrorWireKind::Bool, QVariant(!coreTx.txLevelerOn())},
        {"txEqEnabled", MirrorWireKind::Bool, QVariant(!coreTx.txEqEnabled())},
        {"cfcEnabled", MirrorWireKind::Bool, QVariant(!coreTx.cfcEnabled())},
        {"cpdrOn", MirrorWireKind::Bool, QVariant(!coreTx.cpdrOn())},
        {"cpdrLevelDb", MirrorWireKind::Int64,
         QVariant(qlonglong(coreTx.cpdrLevelDb() == 3 ? 4 : 3))},
        {"amCarrierLevel", MirrorWireKind::Int64,
         QVariant(qlonglong(coreTx.amCarrierLevel() == 60 ? 61 : 60))},
        {"dexpEnabled", MirrorWireKind::Bool, QVariant(!coreTx.dexpEnabled())},
        {"micGainDb", MirrorWireKind::Int64, QVariant(qlonglong(coreTx.micGainDb() == 2 ? 3 : 2))},
        // iPhone app plan Task 40: a receive-only Core's mic mute follows
        // the other settings.
        {"micMuted", MirrorWireKind::Bool, QVariant(!coreTx.micMuted())},
    };
    // Remote parity on the air (transmitSettingsVersion 13): a local
    // window's applets change these while transmitting; so does the Core.
    for (const auto& c : cases) {
        const SessionPropertyResult result = s.writeTransmit(c.name, c.kind, c.value);
        QVERIFY2(result.accepted, qPrintable(QString::fromLatin1(c.name)
                                             + QStringLiteral(": ") + result.reason));
        QCOMPARE(coreTx.property(c.name).toString(), c.value.toString());
    }
    // The window's own change reaches the Core too. (The raw writes above
    // are not echoed to this window, ruling 5.7, so a setting they did not
    // touch is used.)
    TransmitModel& windowTx = s.window.transmitModel();
    const int decay = coreTx.txAlcDecay() == 20 ? 21 : 20;
    windowTx.setTxAlcDecay(decay);
    QTRY_COMPARE(coreTx.txAlcDecay(), decay);

    // The Tune Power command is the transmitter's own setting: while the
    // Core's own key holds transmit it is the holder's (ruling 7.7).
    const int band = coreTx.tunePowerForTxBand();
    const SessionMessage refused = s.invoke("setTunePowerForTxBand", {intArg("watts", 11)});
    QVERIFY(!refused.accepted);
    QCOMPARE(refused.reason, kOnAir);
    QCOMPARE(coreTx.tunePowerForTxBand(), band);

    s.unkeyCore();
    QTRY_VERIFY(s.core->moxController()->state() == MoxState::Rx);
}

void TstTransmitModelProperties::keyingSetStaysRefused()
{
    // The VOX button arms the radio to key: it stays refused, on and off
    // the air, beside the settings this task opens.
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    for (const QByteArray& name : {QByteArrayLiteral("mox"), QByteArrayLiteral("tune"),
                                   QByteArrayLiteral("voxEnabled"),
                                   QByteArrayLiteral("twoToneActive")}) {
        const SessionPropertyResult result =
            s.writeTransmit(name, MirrorWireKind::Bool, QVariant(true));
        QVERIFY2(!result.accepted, name.constData());
        QCOMPARE(result.reason,
                 QStringLiteral("Transmit configuration is unavailable on this receive-only Core."));
    }
    QVERIFY(!s.core->transmitModel().voxEnabled());
    QVERIFY(!s.core->transmitModel().isTune());
    QVERIFY(!s.core->moxController()->isMox());
    // tunePowerForTxBand changes only by command.
    const SessionPropertyResult raw =
        s.writeTransmit("tunePowerForTxBand", MirrorWireKind::Int64, QVariant(qlonglong(9)));
    QVERIFY(!raw.accepted);
    QCOMPARE(raw.reason, QStringLiteral("The Core sets this itself; it cannot be changed from here."));
}

void TstTransmitModelProperties::connectingNeverWritesTheWindowDefaults()
{
    Session s(m_securityDir.path(), this);
    TransmitModel& coreTx = s.core->transmitModel();
    // The Core's values differ from a fresh window's before it connects.
    coreTx.setTxLevelerOn(!s.window.transmitModel().txLevelerOn());
    coreTx.setCpdrLevelDb(s.window.transmitModel().cpdrLevelDb() == 9 ? 10 : 9);
    coreTx.setMicGainDb(s.window.transmitModel().micGainDb() == -12 ? -13 : -12);
    coreTx.setMonitorVolume(0.9f);
    const bool lev = coreTx.txLevelerOn();
    const int cpdr = coreTx.cpdrLevelDb();
    const int mic = coreTx.micGainDb();

    QSignalSpy coreLev(&coreTx, &TransmitModel::txLevelerOnChanged);
    QSignalSpy coreCpdr(&coreTx, &TransmitModel::cpdrLevelDbChanged);
    QSignalSpy coreMic(&coreTx, &TransmitModel::micGainDbChanged);
    QVERIFY(s.connect());
    // The snapshot hydrates the window; nothing flows back.
    QTRY_COMPARE(s.window.transmitModel().txLevelerOn(), lev);
    QTRY_COMPARE(s.window.transmitModel().cpdrLevelDb(), cpdr);
    QTRY_COMPARE(s.window.transmitModel().micGainDb(), mic);
    QTRY_VERIFY(qFuzzyCompare(s.window.transmitModel().monitorVolume(), 0.9f));
    QTest::qWait(200);
    QCOMPARE(coreLev.count(), 0);
    QCOMPARE(coreCpdr.count(), 0);
    QCOMPARE(coreMic.count(), 0);
    QCOMPARE(coreTx.txLevelerOn(), lev);
    QCOMPARE(coreTx.cpdrLevelDb(), cpdr);
    QCOMPARE(coreTx.micGainDb(), mic);
}

void TstTransmitModelProperties::tunePowerCommandSetsTheTxBandAndSource()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    TransmitModel& coreTx = s.core->transmitModel();
    TransmitModel& windowTx = s.window.transmitModel();
    SliceModel* const txSlice = s.core->txBoundSlice();
    QVERIFY(txSlice);
    txSlice->setFrequency(7'150'000.0);
    coreTx.setTuneDrivePowerSource(DrivePowerSource::DriveSlider);
    const int watts = coreTx.tunePowerForBand(Band::Band40m) == 23 ? 24 : 23;

    const IStationLink::CommandOutcome outcome = s.client->requestTunePowerForTxBand(watts);
    QVERIFY2(outcome.sent, qPrintable(outcome.reason));
    // As the local slider does: this band's tune power and the tune slider
    // as the tune drive source, on the Core.
    QTRY_COMPARE(coreTx.tunePowerForBand(Band::Band40m), watts);
    QCOMPARE(coreTx.tuneDrivePowerSource(), DrivePowerSource::TuneSlider);
    QCOMPARE(coreTx.tunePowerForTxBand(), watts);
    // The window shows the Core's.
    QTRY_COMPARE(windowTx.tunePowerForTxBand(), watts);
    QTRY_COMPARE(windowTx.tuneDrivePowerSource(), DrivePowerSource::TuneSlider);
    // Another band's is untouched.
    QVERIFY(coreTx.tunePowerForBand(Band::Band20m) != watts
            || coreTx.tunePowerForBand(Band::Band20m) == 50);
}

void TstTransmitModelProperties::tunePowerCommandRefusals()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    TransmitModel& coreTx = s.core->transmitModel();
    const int before = coreTx.tunePowerForTxBand();

    const SessionMessage range = s.invoke("setTunePowerForTxBand", {intArg("watts", 100)});
    QVERIFY(!range.accepted);
    QCOMPARE(range.reason, QStringLiteral("Choose a tune power from 0 to 99."));
    const SessionMessage wrong = s.invoke("setTunePowerForTxBand", {intArg("power", 10)});
    QVERIFY(!wrong.accepted);
    QCOMPARE(wrong.reason, QStringLiteral("The request to change the tune power was not understood."));
    QVERIFY(OperatorWording::isPlain(wrong.reason));
    QCOMPARE(coreTx.tunePowerForTxBand(), before);

    // A window whose request the Core refused shows the Core's value again.
    QSignalSpy shown(&s.window.transmitModel(), &TransmitModel::tunePowerForTxBandChanged);
    QVERIFY(s.client->requestTunePowerForTxBand(150).sent);
    QTRY_VERIFY(shown.count() >= 1);
    QCOMPARE(shown.last().at(0).toInt(), s.window.transmitModel().tunePowerForTxBand());
    QCOMPARE(s.window.transmitModel().tunePowerForTxBand(), before);

    // An app below minor 11, or a Core without version 2, is not asked.
    RadioModel lone{RadioModel::Role::Remote};
    SettingsProxy loneProxy;
    StationClient unconnected(&lone, &loneProxy);
    const IStationLink::CommandOutcome notAsked = unconnected.requestTunePowerForTxBand(10);
    QVERIFY(!notAsked.sent);
    QCOMPARE(notAsked.reason, IStationLink::transmitSettingsUnavailableReason());
}

void TstTransmitModelProperties::coreBandChangeMovesTheTunePowerSlider()
{
    Session s(m_securityDir.path(), this);
    TransmitModel& coreTx = s.core->transmitModel();
    coreTx.setTunePowerForBand(Band::Band40m, 22);
    coreTx.setTunePowerForBand(Band::Band20m, 33);
    SliceModel* const txSlice = s.core->txBoundSlice();
    QVERIFY(txSlice);
    txSlice->setFrequency(14'200'000.0);
    QVERIFY(s.connect());

    TxApplet applet(&s.window);
    applet.setTransmitChainSettingsPermitted(true);
    QSlider* const slider = applet.tunePowerSlider();
    QVERIFY(slider);
    QTRY_COMPARE(s.window.transmitModel().tunePowerForTxBand(), 33);
    QCOMPARE(slider->value(), 33);

    // The Core's transmit slice moves to 40 m: the slider follows.
    txSlice->setFrequency(7'100'000.0);
    QTRY_COMPARE(slider->value(), 22);
    // The window's own band does not move it.
    applet.setCurrentBand(Band::Band80m);
    QCOMPARE(slider->value(), 22);
    // The Core's own change of that band's value moves it too.
    coreTx.setTunePowerForBand(Band::Band40m, 27);
    QTRY_COMPARE(slider->value(), 27);

    // The slider asks the Core for its transmit band (the release of a drag,
    // or a wheel or key step).
    slider->setValue(31);
    QTRY_COMPARE(coreTx.tunePowerForBand(Band::Band40m), 31);
    QCOMPARE(coreTx.tuneDrivePowerSource(), DrivePowerSource::TuneSlider);
    QCOMPARE(coreTx.tunePowerForBand(Band::Band80m), 50);
    QTRY_COMPARE(s.window.transmitModel().tunePowerForTxBand(), 31);
    QCOMPARE(slider->value(), 31);

    // While the Core's own key holds transmit the command is the holder's
    // (ruling 7.7); the slider goes back to the Core's value.
    s.keyCore();
    QTRY_VERIFY(s.core->moxController()->state() == MoxState::Tx);
    slider->setValue(12);
    QTRY_COMPARE(slider->value(), 31);
    QCOMPARE(coreTx.tunePowerForBand(Band::Band40m), 31);
    s.unkeyCore();
    QTRY_VERIFY(s.core->moxController()->state() == MoxState::Rx);
}

// PA on-air gate re-review, Minor: the first transmit band a local window
// learns repaints the Tune Power slider with that band's tune power, even
// when that value equals the one cached before the band was known.
void TstTransmitModelProperties::firstKnownTransmitBandRepaintsTheTuneSlider()
{
    RadioModel model;
    TransmitModel& tx = model.transmitModel();
    tx.setTunePowerForBand(Band::Band40m, 30);
    QCOMPARE(tx.tunePowerForBand(Band::Band20m), 50);
    QVERIFY(!tx.tuneTxBandKnown());

    TxApplet applet(&model);
    applet.setCurrentBand(Band::Band40m);
    QSlider* const slider = applet.tunePowerSlider();
    QVERIFY(slider);
    QCOMPARE(slider->value(), 30);

    QSignalSpy shown(&tx, &TransmitModel::tunePowerForTxBandChanged);
    tx.setTuneTxBand(Band::Band20m);
    QCOMPARE(shown.count(), 1);
    QCOMPARE(shown.at(0).at(0).toInt(), 50);
    QCOMPARE(slider->value(), 50);

    // Setting the same band again is not a change.
    tx.setTuneTxBand(Band::Band20m);
    QCOMPARE(shown.count(), 1);
}

void TstTransmitModelProperties::remoteTxAppletControlsReachTheCore()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    TransmitModel& coreTx = s.core->transmitModel();
    TxApplet applet(&s.window);
    const auto byName = [&applet](const char* objectName) {
        return applet.findChild<QPushButton*>(QString::fromLatin1(objectName));
    };
    const auto byAccessible = [&applet](const QString& name) -> QWidget* {
        for (QWidget* w : applet.findChildren<QWidget*>()) {
            if (w->accessibleName() == name) { return w; }
        }
        return nullptr;
    };
    QPushButton* const lev = byName("TxLevButton");
    QPushButton* const eq = byName("TxEqButton");
    QPushButton* const cfc = byName("TxCfcButton");
    QPushButton* const mon = qobject_cast<QPushButton*>(byAccessible(QStringLiteral("Monitor enable")));
    QPushButton* const vox = byName("TxVoxButton");
    QVERIFY(lev && eq && cfc && mon && vox);

    // Closed until MainWindow opens the gate, with the Core reason.
    QVERIFY(!lev->isEnabled());
    QCOMPARE(lev->toolTip(), IStationLink::transmitSettingsUnavailableReason());
    applet.setTransmitPermitted(false, QStringLiteral("Remote transmit is unavailable"));
    applet.setTransmitChainSettingsPermitted(true);
    for (QWidget* w : std::initializer_list<QWidget*>{lev, eq, cfc, mon,
                                                       applet.tunePowerSlider()}) {
        QVERIFY(w->isEnabled());
    }
    // VOX (the arming button) keeps the keying gate.
    QVERIFY(!vox->isEnabled());
    QCOMPARE(vox->toolTip(), QStringLiteral("Remote transmit is unavailable"));

    const bool levBefore = coreTx.txLevelerOn();
    lev->click();
    QTRY_COMPARE(coreTx.txLevelerOn(), !levBefore);
    QTRY_COMPARE(lev->isChecked(), !levBefore);
    const bool eqBefore = coreTx.txEqEnabled();
    eq->click();
    QTRY_COMPARE(coreTx.txEqEnabled(), !eqBefore);
    const bool cfcBefore = coreTx.cfcEnabled();
    cfc->click();
    QTRY_COMPARE(coreTx.cfcEnabled(), !cfcBefore);
    const bool monBefore = coreTx.monEnabled();
    mon->click();
    QTRY_COMPARE(coreTx.monEnabled(), !monBefore);

    // The Core's change shows on the control (from the mirror).
    coreTx.setCfcEnabled(cfcBefore);
    QTRY_COMPARE(cfc->isChecked(), cfcBefore);

    // On the air: grey, with the on-air reason.
    applet.setTransmitChainSettingsPermitted(false, kOnAir);
    for (QWidget* w : std::initializer_list<QWidget*>{lev, eq, cfc, mon,
                                                       applet.tunePowerSlider()}) {
        QVERIFY(!w->isEnabled());
        QCOMPARE(w->toolTip(), kOnAir);
    }
}

void TstTransmitModelProperties::remotePhoneCwControlsReachTheCore()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    TransmitModel& coreTx = s.core->transmitModel();
    PhoneCwApplet applet(&s.window);
    QSlider* mic = nullptr;
    for (QSlider* candidate : applet.findChildren<QSlider*>()) {
        if (candidate->accessibleName() == QStringLiteral("Microphone gain")) { mic = candidate; }
    }
    auto* const proc = applet.findChild<QPushButton*>(QStringLiteral("PhoneCwProcButton"));
    auto* const procSlider = applet.findChild<QSlider*>(QStringLiteral("PhoneCwProcSlider"));
    auto* const dexp = applet.findChild<QPushButton*>(QStringLiteral("PhoneCwDexpButton"));
    QVERIFY(mic && proc && procSlider && dexp);
    QVERIFY(!proc->isEnabled());

    applet.setTransmitPermitted(false, QStringLiteral("Remote transmit is unavailable"));
    applet.setTransmitSettingsPermitted(true);
    QVERIFY(mic->isEnabled());
    QVERIFY(proc->isEnabled());
    QVERIFY(procSlider->isEnabled());
    QVERIFY(dexp->isEnabled());

    const int micTarget = mic->value() == -5 ? -4 : -5;
    mic->setValue(micTarget);
    QTRY_COMPARE(coreTx.micGainDb(), micTarget);
    const bool procBefore = coreTx.cpdrOn();
    proc->click();
    QTRY_COMPARE(coreTx.cpdrOn(), !procBefore);
    const int procTarget = procSlider->value() == 6 ? 7 : 6;
    procSlider->setValue(procTarget);
    QTRY_COMPARE(coreTx.cpdrLevelDb(), procTarget);
    const bool dexpBefore = coreTx.dexpEnabled();
    dexp->click();
    QTRY_COMPARE(coreTx.dexpEnabled(), !dexpBefore);
    QTRY_COMPARE(s.txChannel.lastDexpRunForTest(), !dexpBefore);

    // The Core's own change shows on the slider.
    coreTx.setCpdrLevelDb(12);
    QTRY_COMPARE(procSlider->value(), 12);

    applet.setTransmitSettingsPermitted(false, kOnAir);
    for (QWidget* w : std::initializer_list<QWidget*>{mic, proc, procSlider, dexp}) {
        QVERIFY(!w->isEnabled());
        QCOMPARE(w->toolTip(), kOnAir);
    }
}

void TstTransmitModelProperties::containerMonButtonTogglesTheCoresMon()
{
    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    TransmitModel& coreTx = s.core->transmitModel();
    bool settingsOpen = true;
    QString settingsReason;
    ContainerButtonDispatcher::Hooks hooks;
    hooks.transmitPermitted = [] { return false; };
    hooks.remoteTransmitReason = QStringLiteral("Remote transmit is unavailable");
    hooks.transmitSettingsPermitted = [&settingsOpen] { return settingsOpen; };
    hooks.transmitSettingsReason = [&settingsReason] { return settingsReason; };
    ContainerButtonDispatcher dispatcher(&s.window, hooks);
    using Id = ContainerButtonDispatcher::Id;

    const bool before = coreTx.monEnabled();
    QTRY_COMPARE(dispatcher.stateOf(Id::Mon, 0).on, before);
    QVERIFY(dispatcher.stateOf(Id::Mon, 0).available);
    QVERIFY(dispatcher.click(Id::Mon, 0).isEmpty());
    QTRY_COMPARE(coreTx.monEnabled(), !before);
    QTRY_COMPARE(dispatcher.stateOf(Id::Mon, 0).on, !before);
    // The Core's change shows on the button.
    coreTx.setMonEnabled(before);
    QTRY_COMPARE(dispatcher.stateOf(Id::Mon, 0).on, before);
    // TUN and MOX keep the keying gate.
    QVERIFY(!dispatcher.stateOf(Id::Tun, 0).available);
    QVERIFY(!dispatcher.stateOf(Id::Mox, 0).available);

    settingsOpen = false;
    settingsReason = kOnAir;
    const ContainerButtonDispatcher::State closed = dispatcher.stateOf(Id::Mon, 0);
    QVERIFY(!closed.available);
    QCOMPARE(closed.reason, kOnAir);
    QCOMPARE(dispatcher.click(Id::Mon, 0), kOnAir);
    QTest::qWait(100);
    QCOMPARE(coreTx.monEnabled(), before);
}

// iPhone app plan Task 40: the Core's mic mute as `transmit.micMuted`
// (true = muted), appended after voxEnabled so every earlier ordinal stays.
// A write mutes the Core's TX chain as Thetis's chkMicMute does (the
// preamp at 0.0), unmuting restores the mic level, and the change reaches
// every window. Never persisted: the mic starts in use.
void TstTransmitModelProperties::micMutedIsOnTheLinkAndSilencesTheCoreMic()
{
    TransmitModel tx;
    const MirrorSchema& schema = MirrorSchema::forObject(&tx);
    const MirrorProperty* vox = schema.byName("voxEnabled");
    const MirrorProperty* muted = schema.byName("micMuted");
    QVERIFY(vox);
    QVERIFY(muted);
    QCOMPARE(muted->kind, MirrorWireKind::Bool);
    QCOMPARE(muted->ordinal, quint16(vox->ordinal + 1));
    QVERIFY(muted->isWritable);
    QVERIFY(MirrorPolicy::inboundAllowed("TransmitModel", "micMuted"));

    Session s(m_securityDir.path(), this);
    QVERIFY(s.connect());
    TransmitModel& coreTx = s.core->transmitModel();
    TransmitModel& windowTx = s.window.transmitModel();
    QVERIFY(!coreTx.micMuted());
    const double inUse = std::pow(10.0, coreTx.micGainDb() / 20.0);
    QTRY_VERIFY(std::abs(s.txChannel.lastMicPreampForTest() - inUse) < 1e-9);

    // A raw write, as the phone sends it.
    const SessionPropertyResult mute =
        s.writeTransmit("micMuted", MirrorWireKind::Bool, QVariant(true));
    QVERIFY2(mute.accepted, qPrintable(mute.reason));
    QVERIFY(coreTx.micMuted());
    QTRY_COMPARE(s.txChannel.lastMicPreampForTest(), 0.0);

    // The window's own change, as its model sends it (the raw write above
    // is this session's own, which the Core withholds from its view).
    windowTx.setMicMuted(true);
    windowTx.setMicMuted(false);
    QTRY_VERIFY(!coreTx.micMuted());
    QTRY_VERIFY(std::abs(s.txChannel.lastMicPreampForTest() - inUse) < 1e-9);

    // The Core's own change reaches the window.
    coreTx.setMicMute(false);
    QTRY_VERIFY(windowTx.micMuted());
    coreTx.setMicMute(true);
    QTRY_VERIFY(!windowTx.micMuted());
}

QTEST_MAIN(TstTransmitModelProperties)

#include "tst_transmit_model_properties.moc"
