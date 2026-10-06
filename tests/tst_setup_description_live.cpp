// no-port-check: NereusSDR-original live-apply checks for described settings.
// 2026-09-29: the RX buffer size lock follows TUNE and the two-tone test.
// J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-09-30: the cap is 24 (Audio > TX Input's Line In Gain steps and
// Saturn Mic Tip-Ring); the HL2 Core's Hermes rows. J.J. Boyd (KG4VCF),
// AI-assisted via Anthropic Claude Code.
// 2026-10-04: restored TX profile watch coverage and Core-produced typed
// fixture. J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include <QtTest>

#include <algorithm>
#include <tuple>
#include <QMetaProperty>
#include <QSaveFile>

#include "core/AppSettings.h"
#include "core/MicProfileManager.h"
#include "core/CfcProfile.h"
#include "core/ParaEqCurve.h"
#include "core/PaTelemetryScaling.h"
#include "core/PaCalProfile.h"
#include "core/PaProfileManager.h"
#include "core/PaProfile.h"
#include "models/Band.h"
#include "core/FreeDVReporterClient.h"
#include "core/settings/SettingsProxyServer.h"
#include "core/session/SessionCommandDispatcher.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"
#include "core/setup/SetupDescriptionService.h"
#include "core/setup/SetupDescriptionV15.h"
#include "core/accessories/AlexAntennaFacade.h"
#include "core/accessories/AlexController.h"
#include "core/StepAttenuatorController.h"
#include "core/TxAnalyzer.h"
#include "core/TwoToneController.h"
#include "core/TxChannel.h"
#include "core/meters/SliceMeterPump.h"
#include "core/session/TransmitStateFacade.h"
#include "core/session/MirrorSchema.h"
#include "core/session/StateMirror.h"
#include "MultiDeviceHarness.h"

using namespace NereusSDR;


namespace {

struct TxProfileWatchField {
    QByteArray name;
    MirrorWireKind kind;
    QStringList savedKeys;
};

// Independent saved/restored-field oracle, source-audited against
// MicProfileManager::captureLiveValues/applyValuesToModel and the public
// TransmitModel properties. This must not be derived from the watch itself.
const QList<TxProfileWatchField> kTxProfileWatchFields{
    {"micGainDb", MirrorWireKind::Int64,
        {QStringLiteral("MicGain")}},
    {"micBoost", MirrorWireKind::Bool,
        {QStringLiteral("Mic_Input_Boost")}},
    {"micXlr", MirrorWireKind::Bool,
        {QStringLiteral("Mic_XLR")}},
    {"lineIn", MirrorWireKind::Bool,
        {QStringLiteral("Line_Input_On")}},
    {"lineInBoost", MirrorWireKind::Float64,
        {QStringLiteral("Line_Input_Level")}},
    {"micTipRing", MirrorWireKind::Bool,
        {QStringLiteral("Mic_TipRing")}},
    {"micBias", MirrorWireKind::Bool,
        {QStringLiteral("Mic_Bias")}},
    {"micPttDisabled", MirrorWireKind::Bool,
        {QStringLiteral("Mic_PTT_Disabled")}},
    {"voxThresholdDb", MirrorWireKind::Int64,
        {QStringLiteral("Dexp_Threshold")}},
    {"voxHangTimeMs", MirrorWireKind::Int64,
        {QStringLiteral("VOX_HangTime")}},
    {"antiVoxGainDb", MirrorWireKind::Int64,
        {QStringLiteral("AntiVox_Gain")}},
    {"monitorVolume", MirrorWireKind::Float64,
        {QStringLiteral("MonitorVolume")}},
    {"amCarrierLevel", MirrorWireKind::Int64,
        {QStringLiteral("AM_Carrier_Level")}},
    {"twoToneFreq1", MirrorWireKind::Int64,
        {QStringLiteral("TwoToneFreq1")}},
    {"twoToneFreq2", MirrorWireKind::Int64,
        {QStringLiteral("TwoToneFreq2")}},
    {"twoToneLevel", MirrorWireKind::Float64,
        {QStringLiteral("TwoToneLevel")}},
    {"twoTonePower", MirrorWireKind::Int64,
        {QStringLiteral("TwoTonePower")}},
    {"twoToneFreq2Delay", MirrorWireKind::Int64,
        {QStringLiteral("TwoToneFreq2Delay")}},
    {"twoToneInvert", MirrorWireKind::Bool,
        {QStringLiteral("TwoToneInvert")}},
    {"twoTonePulsed", MirrorWireKind::Bool,
        {QStringLiteral("TwoTonePulsed")}},
    {"twoToneDrivePowerSource", MirrorWireKind::Enum,
        {QStringLiteral("TwoToneDrivePowerOrigin")}},
    {"userDigOut", MirrorWireKind::Int64,
        {QStringLiteral("Mic_UserDigOut")}},
    {"txEqEnabled", MirrorWireKind::Bool,
        {QStringLiteral("TXEQEnabled")}},
    {"txEqPreamp", MirrorWireKind::Int64,
        {QStringLiteral("TXEQPreamp")}},
    {"txEqBandsJson", MirrorWireKind::Utf8,
        {QStringLiteral("TXEQ1"),
         QStringLiteral("TXEQ2"),
         QStringLiteral("TXEQ3"),
         QStringLiteral("TXEQ4"),
         QStringLiteral("TXEQ5"),
         QStringLiteral("TXEQ6"),
         QStringLiteral("TXEQ7"),
         QStringLiteral("TXEQ8"),
         QStringLiteral("TXEQ9"),
         QStringLiteral("TXEQ10")}},
    {"txEqFreqsJson", MirrorWireKind::Utf8,
        {QStringLiteral("TxEqFreq1"),
         QStringLiteral("TxEqFreq2"),
         QStringLiteral("TxEqFreq3"),
         QStringLiteral("TxEqFreq4"),
         QStringLiteral("TxEqFreq5"),
         QStringLiteral("TxEqFreq6"),
         QStringLiteral("TxEqFreq7"),
         QStringLiteral("TxEqFreq8"),
         QStringLiteral("TxEqFreq9"),
         QStringLiteral("TxEqFreq10")}},
    {"txEqParaEqData", MirrorWireKind::Utf8,
        {QStringLiteral("TXParaEQData")}},
    {"txEqUseLegacy", MirrorWireKind::Bool,
        {QStringLiteral("EQUseLegacy")}},
    {"txLevelerOn", MirrorWireKind::Bool,
        {QStringLiteral("Lev_On")}},
    {"txLevelerMaxGain", MirrorWireKind::Int64,
        {QStringLiteral("Lev_MaxGain")}},
    {"txLevelerDecay", MirrorWireKind::Int64,
        {QStringLiteral("Lev_Decay")}},
    {"txAlcMaxGain", MirrorWireKind::Int64,
        {QStringLiteral("ALC_MaximumGain")}},
    {"txAlcDecay", MirrorWireKind::Int64,
        {QStringLiteral("ALC_Decay")}},
    {"phaseRotatorEnabled", MirrorWireKind::Bool,
        {QStringLiteral("CFCPhaseRotatorEnabled")}},
    {"phaseReverseEnabled", MirrorWireKind::Bool,
        {QStringLiteral("CFCPhaseReverseEnabled")}},
    {"phaseRotatorFreqHz", MirrorWireKind::Int64,
        {QStringLiteral("CFCPhaseRotatorFreq")}},
    {"phaseRotatorStages", MirrorWireKind::Int64,
        {QStringLiteral("CFCPhaseRotatorStages")}},
    {"cfcEnabled", MirrorWireKind::Bool,
        {QStringLiteral("CFCEnabled")}},
    {"cfcPostEqEnabled", MirrorWireKind::Bool,
        {QStringLiteral("CFCPostEqEnabled")}},
    {"cfcPrecompDb", MirrorWireKind::Int64,
        {QStringLiteral("CFCPreComp")}},
    {"cfcPostEqGainDb", MirrorWireKind::Int64,
        {QStringLiteral("CFCPostEqGain")}},
    {"cfcEqFreqJson", MirrorWireKind::Utf8,
        {QStringLiteral("CFCEqFreq0"),
         QStringLiteral("CFCEqFreq1"),
         QStringLiteral("CFCEqFreq2"),
         QStringLiteral("CFCEqFreq3"),
         QStringLiteral("CFCEqFreq4"),
         QStringLiteral("CFCEqFreq5"),
         QStringLiteral("CFCEqFreq6"),
         QStringLiteral("CFCEqFreq7"),
         QStringLiteral("CFCEqFreq8"),
         QStringLiteral("CFCEqFreq9")}},
    {"cfcCompressionJson", MirrorWireKind::Utf8,
        {QStringLiteral("CFCPreComp0"),
         QStringLiteral("CFCPreComp1"),
         QStringLiteral("CFCPreComp2"),
         QStringLiteral("CFCPreComp3"),
         QStringLiteral("CFCPreComp4"),
         QStringLiteral("CFCPreComp5"),
         QStringLiteral("CFCPreComp6"),
         QStringLiteral("CFCPreComp7"),
         QStringLiteral("CFCPreComp8"),
         QStringLiteral("CFCPreComp9")}},
    {"cfcPostEqBandGainJson", MirrorWireKind::Utf8,
        {QStringLiteral("CFCPostEqGain0"),
         QStringLiteral("CFCPostEqGain1"),
         QStringLiteral("CFCPostEqGain2"),
         QStringLiteral("CFCPostEqGain3"),
         QStringLiteral("CFCPostEqGain4"),
         QStringLiteral("CFCPostEqGain5"),
         QStringLiteral("CFCPostEqGain6"),
         QStringLiteral("CFCPostEqGain7"),
         QStringLiteral("CFCPostEqGain8"),
         QStringLiteral("CFCPostEqGain9")}},
    {"cfcParaEqData", MirrorWireKind::Utf8,
        {QStringLiteral("CFCParaEQData")}},
    {"cpdrLevelDb", MirrorWireKind::Int64,
        {QStringLiteral("CompanderLevel")}},
    {"cessbOn", MirrorWireKind::Bool,
        {QStringLiteral("CESSB_On")}},
    {"filterLow", MirrorWireKind::Int64,
        {QStringLiteral("FilterLow")}},
    {"filterHigh", MirrorWireKind::Int64,
        {QStringLiteral("FilterHigh")}},
    {"dexpEnabled", MirrorWireKind::Bool,
        {QStringLiteral("DEXP_Enabled")}},
    {"dexpDetectorTauMs", MirrorWireKind::Float64,
        {QStringLiteral("DEXP_DetectorTauMs")}},
    {"dexpAttackTimeMs", MirrorWireKind::Float64,
        {QStringLiteral("DEXP_AttackTimeMs")}},
    {"dexpReleaseTimeMs", MirrorWireKind::Float64,
        {QStringLiteral("DEXP_ReleaseTimeMs")}},
    {"dexpExpansionRatioDb", MirrorWireKind::Float64,
        {QStringLiteral("DEXP_ExpansionRatioDb")}},
    {"dexpHysteresisRatioDb", MirrorWireKind::Float64,
        {QStringLiteral("DEXP_HysteresisRatioDb")}},
    {"dexpLookAheadEnabled", MirrorWireKind::Bool,
        {QStringLiteral("DEXP_LookAheadEnabled")}},
    {"dexpLookAheadMs", MirrorWireKind::Float64,
        {QStringLiteral("DEXP_LookAheadMs")}},
    {"dexpLowCutHz", MirrorWireKind::Float64,
        {QStringLiteral("DEXP_LowCutHz")}},
    {"dexpHighCutHz", MirrorWireKind::Float64,
        {QStringLiteral("DEXP_HighCutHz")}},
    {"dexpSideChannelFilterEnabled", MirrorWireKind::Bool,
        {QStringLiteral("DEXP_SideChannelFilterEnabled")}},
};

QJsonObject txProfileChoice(const QJsonObject& audio)
{
    for (const QJsonValue& page : audio.value("pages").toArray()) {
        for (const QJsonValue& section : page.toObject().value("sections").toArray()) {
            for (const QJsonValue& control : section.toObject().value("controls").toArray()) {
                const QJsonObject row = control.toObject();
                if (row.value("id") == QJsonValue("audio.txProfile.activeProfile")) {
                    return row;
                }
            }
        }
    }
    return {};
}


// Legal edits from the existing MicProfileManager/ParaEq/CFC fixtures and
// TransmitModel ranges. Values change test state only; no keying property.
QVariant txProfileEdit(const TxProfileWatchField& field,
                       const QHash<QByteArray, QVariant>& opening)
{
    if (field.kind == MirrorWireKind::Bool) {
        return !opening.value(field.name).toBool();
    }
    static const QHash<QByteArray, QVariant> edits{
        {"micGainDb", -3}, {"lineInBoost", -12.0}, {"voxThresholdDb", -30},
        {"voxHangTimeMs", 700}, {"antiVoxGainDb", 3}, {"monitorVolume", 0.625},
        {"amCarrierLevel", 75}, {"twoToneFreq1", 900}, {"twoToneFreq2", 1700},
        {"twoToneLevel", -9.0}, {"twoTonePower", 25}, {"twoToneFreq2Delay", 10},
        {"twoToneDrivePowerSource", QVariant::fromValue(DrivePowerSource::TuneSlider)},
        {"userDigOut", 5}, {"txEqPreamp", 5},
        {"txEqBandsJson", QStringLiteral("[-8,-12,-12,-1,1,4,9,12,-10,7]")},
        {"txEqFreqsJson", QStringLiteral("[50,63,125,250,500,1000,2000,4000,8000,14000]")},
        {"txLevelerMaxGain", 8}, {"txLevelerDecay", 250},
        {"txAlcMaxGain", 20}, {"txAlcDecay", 25},
        {"phaseRotatorFreqHz", 500}, {"phaseRotatorStages", 10},
        {"cfcPrecompDb", 3}, {"cfcPostEqGainDb", -2},
        {"cfcEqFreqJson", QStringLiteral("[0,150,300,600,1200,2400,3600,4500,5500,11000]")},
        {"cfcCompressionJson", QStringLiteral("[6,6,6,6,6,6,6,6,6,6]")},
        {"cfcPostEqBandGainJson", QStringLiteral("[1,1,1,1,1,1,1,1,1,1]")},
        {"cpdrLevelDb", 5}, {"filterLow", 200}, {"filterHigh", 2700},
        {"dexpDetectorTauMs", 35.0}, {"dexpAttackTimeMs", 8.0},
        {"dexpReleaseTimeMs", 250.0}, {"dexpExpansionRatioDb", 20.0},
        {"dexpHysteresisRatioDb", 5.0}, {"dexpLookAheadMs", 120.0},
        {"dexpLowCutHz", 300.0}, {"dexpHighCutHz", 2800.0},
    };
    if (field.name == "txEqParaEqData") {
        ParaEqCurve::TxEqPoints points;
        points.bandCount = 5;
        points.minHz = 50;
        points.maxHz = 3000;
        points.preampDb = -2.5;
        points.f = {50, 300, 1200, 2400, 3000};
        points.g = {-6, 3, -1.5, 4, 0};
        points.q = {1.5, 2, 4, 3, 1};
        return ParaEqCurve::txEqParaEqDataFromPoints(points);
    }
    if (field.name == "cfcParaEqData") {
        CfcProfile::Profile profile;
        profile.maxHz = profile.postMaxHz = 17000;
        // Change the valid saved blob while keeping its paired scalar
        // gains at the actual opening getters, isolating this watched ID.
        profile.precompDb = opening.value("cfcPrecompDb").toDouble();
        profile.postEqGainDb = opening.value("cfcPostEqGainDb").toDouble();
        profile.compParametric = profile.eqParametric = true;
        // Main's existing CfcProfile codec emits the snake_case graphs
        // its producer decodes; retain the eighteen-band precision input.
        for (int i = 0; i < 18; ++i) {
            profile.f.push_back(i == 1 ? 125.1254 : i * 1000.0);
            profile.g.push_back(3.54);
            profile.qg.push_back(1.2345);
            profile.e.push_back(-4.54);
            profile.qe.push_back(2.3456);
        }
        profile.postF = profile.f;
        return CfcProfile::encode(profile);
    }
    return edits.value(field.name);
}

QStringList txProfileSavedStrings(const TxProfileWatchField& field, const QVariant& edit)
{
    if (field.savedKeys.size() > 1) {
        QStringList strings;
        const QJsonArray values = QJsonDocument::fromJson(edit.toString().toUtf8()).array();
        for (const QJsonValue& value : values) {
            strings.append(QString::number(value.toInt()));
        }
        return strings;
    }
    if (field.kind == MirrorWireKind::Bool) {
        return {edit.toBool() ? QStringLiteral("True") : QStringLiteral("False")};
    }
    if (field.kind == MirrorWireKind::Enum) {
        return {QStringLiteral("TuneSlider")};
    }
    if (field.kind == MirrorWireKind::Int64) {
        return {QString::number(edit.toInt())};
    }
    if (field.kind == MirrorWireKind::Float64) {
        return {QString::number(edit.toDouble())};
    }
    return {edit.toString()};
}

} // namespace

class SetupDescriptionLiveTest : public QObject {
    Q_OBJECT
private slots:
    // Omitting a saved/restored observable value lets a described consumer
    // switch profiles without being told to ask about that value's edit.
    void pairedV15TxProfileWatchCoversEveryObservableRestoredField()
    {
        Core core;
        QVERIFY(core.model->connection() == nullptr);
        Device phone(QStringLiteral("TX watch iPhone"), QStringLiteral("phone"));
        core.pair(phone);
        QHash<QByteArray, int> features = kHolder;
        features.insert("setupDescription", 15);
        LoopbackTransport* peer = core.signIn(phone, features);
        QVERIFY(admitted(peer));
        const QJsonObject audio = QJsonDocument::fromJson(
            latest(peer->received(), "setup", "audio").toString().toUtf8()).object();
        const QJsonObject choice = txProfileChoice(audio);
        QVERIFY(!choice.isEmpty());
        const QJsonArray watch = choice.value("unsavedChanges").toObject().value("watch").toArray();
        QSet<QByteArray> watched;
        for (const QJsonValue& name : watch) {
            QVERIFY(name.isString());
            QVERIFY(!watched.contains(name.toString().toUtf8()));
            watched.insert(name.toString().toUtf8());
        }
        const MirrorSchema& schema = MirrorSchema::forMetaObject(&TransmitModel::staticMetaObject);
        QHash<QByteArray, MirrorUpdate> snapshot;
        for (const MirrorUpdate& update : core.server->stateMirror()->snapshot("transmit")) {
            snapshot.insert(update.name, update);
        }
        QStringList missing;
        for (const TxProfileWatchField& field : kTxProfileWatchFields) {
            const MirrorProperty* property = schema.byName(field.name);
            QVERIFY2(property != nullptr, field.name.constData());
            QCOMPARE(property->kind, field.kind);
            QVERIFY2(snapshot.contains(field.name), field.name.constData());
            QCOMPARE(snapshot.value(field.name).kind, field.kind);
            if (!watched.contains(field.name)) {
                missing.append(QString::fromUtf8(field.name));
            }
        }
        QVERIFY2(missing.isEmpty(), qPrintable("Missing saved/restored TX profile fields: " + missing.join(", ")));
        QCOMPARE(watched.size(), kTxProfileWatchFields.size());
        // These are unavailable, ignored-on-load, global, or derived editor
        // views; none belongs in this ordinary saved-value watch contract.
        for (const QByteArray& excluded : {QByteArray("voxGainScalar"), QByteArray("micSource"),
                 QByteArray("lineInGain"), QByteArray("txEqNc"), QByteArray("txEqMp"),
                 QByteArray("txEqCtfmode"), QByteArray("txEqWintype"), QByteArray("cpdrOn"),
                 QByteArray("txEqCurve"), QByteArray("cfcProfile")}) {
            QVERIFY2(!watched.contains(excluded), excluded.constData());
        }
    }

    // The producer fixture and capture/restore checks use real Core getter
    // reads and real encoded messages. A wrong kind/name, omitted capture
    // key, swallowed setter notification or missing restore must fail here.
    void txProfileWatchTypedCoreFixtureRoundTripsEveryField()
    {
        Core core;
        QVERIFY(core.model->connection() == nullptr);
        QVERIFY(QStandardPaths::isTestModeEnabled());
        const QString testHome = qEnvironmentVariable("CFFIXED_USER_HOME");
        if (!testHome.isEmpty()) {
            QVERIFY(AppSettings::instance().filePath().startsWith(testHome + '/'));
        }
        TransmitModel& tx = core.model->transmitModel();
        MicProfileManager* manager = core.model->micProfileManager();
        QVERIFY(manager != nullptr);
        const QString mac = core.model->currentRadioInfo().macAddress;
        QVERIFY(!mac.isEmpty());
        manager->setMacAddress(mac);
        manager->load();
        const QHash<QString, QVariant> defaults = MicProfileManager::defaultProfileValues();
        const auto storedProfile = [&mac](const QString& name) {
            QHash<QString, QVariant> values;
            const QString prefix = QStringLiteral("hardware/%1/tx/profile/%2/").arg(mac, name);
            for (const QString& key : AppSettings::instance().allKeys()) {
                if (key.startsWith(prefix)) {
                    values.insert(key.mid(prefix.size()), AppSettings::instance().value(key));
                }
            }
            return values;
        };
        QVERIFY(manager->saveProfile("Baseline", &tx));
        for (auto it = defaults.cbegin(); it != defaults.cend(); ++it) {
            AppSettings::instance().setValue(
                QStringLiteral("hardware/%1/tx/profile/Baseline/%2").arg(mac, it.key()), it.value());
        }
        QVERIFY(manager->setActiveProfile("Baseline", &tx));
        QVERIFY(manager->saveProfile("Alpha", &tx));
        QVERIFY(manager->saveProfile("Beta", &tx));
        QVERIFY(manager->saveProfile("Edited", &tx));
        QVERIFY(manager->setActiveProfile("Alpha", &tx));
        core.server->setRemoteTransmitAllowed(true);
        Device phone(QStringLiteral("TX fixture iPhone"), QStringLiteral("phone"));
        core.pair(phone);
        QHash<QByteArray, int> features = kHolder;
        features.insert("setupDescription", 15);
        features.insert("remoteTx", 1);
        LoopbackTransport* peer = core.signIn(phone, features);
        QVERIFY(admitted(peer));
        QVERIFY(txPermitted(peer));
        const MirrorSchema& schema = MirrorSchema::forObject(&tx);
        const QByteArray wireClass = MirrorSchema::shortClassName(schema.className());
        const QList<MirrorUpdate> baseline = core.server->stateMirror()->snapshot("transmit");
        QHash<QByteArray, QVariant> opening;
        for (const TxProfileWatchField& field : kTxProfileWatchFields) {
            const MirrorProperty* property = schema.byName(field.name);
            QVERIFY2(property != nullptr, field.name.constData());
            QCOMPARE(property->kind, field.kind);
            opening.insert(field.name, schema.read(*property, &tx));
        }
        QJsonObject fixture;
        fixture.insert("audio", QJsonDocument::fromJson(
            latest(peer->received(), "setup", "audio").toString().toUtf8()).object());
        QJsonArray initial;
        bool sawTransmit = false;
        int publishedSnapshotSize = 0;
        for (const QByteArray& wire : peer->received()) {
            SessionMessage message;
            QVERIFY(SessionMessages::decode(wire, &message));
            if (message.kind == SessionMessageKind::Schema && message.className == wireClass) {
                initial.append(QJsonDocument::fromJson(wire).object());
            }
            if (message.kind == SessionMessageKind::ObjectCreate && message.objectKey == "transmit") {
                sawTransmit = true;
                publishedSnapshotSize = message.updates.size();
                for (const TxProfileWatchField& field : kTxProfileWatchFields) {
                    const auto found = std::find_if(message.updates.cbegin(), message.updates.cend(),
                        [&field](const MirrorUpdate& update) { return update.name == field.name; });
                    QVERIFY2(found != message.updates.cend(), field.name.constData());
                    QCOMPARE(found->kind, field.kind);
                    QCOMPARE(found->value, opening.value(field.name));
                }
                initial.append(QJsonDocument::fromJson(wire).object());
            }
            if (message.kind == SessionMessageKind::SnapshotComplete) {
                initial.append(QJsonDocument::fromJson(wire).object());
            }
        }
        QVERIFY(sawTransmit);
        // Independently re-encode the full StateMirror getter snapshot and
        // decode it; ordinals/types come from the Core, never this table.
        const QByteArray baselineWire = SessionMessages::encode(
            SessionMessages::objectCreate("transmit", wireClass, baseline));
        SessionMessage baselineDecoded;
        QVERIFY(SessionMessages::decode(baselineWire, &baselineDecoded));
        QCOMPARE(baselineDecoded.updates.size(), baseline.size());
        fixture.insert("initial", initial);
        // Observe baseline/reset traffic through the actual encoded Core
        // messages before collecting an edit. The server's real mirror
        // flush cadence is driven by QTRY's event loop; no guessed delay.
        QHash<QByteArray, QVariant> observed;
        int receivedThrough = 0;
        const auto receivedBaseline = [&]() {
            while (receivedThrough < peer->received().size()) {
                SessionMessage message;
                if (!SessionMessages::decode(peer->received().at(receivedThrough++), &message)) {
                    return false;
                }
                if (message.objectKey == "transmit"
                    && (message.kind == SessionMessageKind::ObjectCreate
                        || message.kind == SessionMessageKind::Delta)) {
                    for (const MirrorUpdate& update : message.updates) {
                        observed.insert(update.name, update.value);
                    }
                }
            }
            if (observed.value("activeTxProfile").toString() != "Alpha") {
                return false;
            }
            for (const TxProfileWatchField& field : kTxProfileWatchFields) {
                if (!observed.contains(field.name) || observed.value(field.name) != opening.value(field.name)) {
                    return false;
                }
            }
            return true;
        };
        QJsonArray cases;
        for (const TxProfileWatchField& field : kTxProfileWatchFields) {
            qInfo() << "Profile fixture field:" << field.name;
            QVERIFY(manager->setActiveProfile("Alpha", &tx));
            for (const TxProfileWatchField& restoredField : kTxProfileWatchFields) {
                QCOMPARE(schema.read(*schema.byName(restoredField.name), &tx), opening.value(restoredField.name));
            }
            QTRY_VERIFY_WITH_TIMEOUT(receivedBaseline(), 1000);
            const MirrorProperty* property = schema.byName(field.name);
            const QVariant edit = txProfileEdit(field, opening);
            QVERIFY2(edit.isValid(), field.name.constData());
            if (field.name == "cfcParaEqData") {
                CfcProfile::Profile decoded;
                QVERIFY(CfcProfile::decode(edit.toString(), decoded));
                QCOMPARE(decoded.f.size(), std::size_t{18});
                QCOMPARE(decoded.postF, decoded.f);
                QCOMPARE(decoded.precompDb, opening.value("cfcPrecompDb").toDouble());
                QCOMPARE(decoded.postEqGainDb, opening.value("cfcPostEqGainDb").toDouble());
            }
            const int receivedStart = peer->received().size();
            const QMetaProperty meta = tx.metaObject()->property(property->metaIndex);
            QVERIFY2(meta.write(&tx, edit), field.name.constData());
            const QVariant changed = schema.read(*property, &tx);
            QVERIFY2(changed != opening.value(field.name), field.name.constData());
            QCOMPARE(changed, MirrorSchema::encode(*property, edit));
            QByteArray changedWire;
            const auto receivedChangedValue = [&]() {
                for (int i = receivedStart; i < peer->received().size(); ++i) {
                    const QByteArray wire = peer->received().at(i);
                    SessionMessage message;
                    if (!SessionMessages::decode(wire, &message)
                        || message.kind != SessionMessageKind::Delta || message.objectKey != "transmit") {
                        continue;
                    }
                    for (const MirrorUpdate& update : message.updates) {
                        if (update.name == field.name && update.kind == field.kind && update.value == changed) {
                            changedWire = wire;
                            return true;
                        }
                    }
                }
                return false;
            };
            QTRY_VERIFY_WITH_TIMEOUT(receivedChangedValue(), 1000);
            SessionMessage editMessage;
            QVERIFY(SessionMessages::decode(changedWire, &editMessage));
            for (const MirrorUpdate& update : editMessage.updates) {
                QVERIFY2(update.name != "activeTxProfile", field.name.constData());
                if (opening.contains(update.name)) {
                    QCOMPARE(update.kind, schema.byName(update.name)->kind);
                    QCOMPARE(update.ordinal, schema.byName(update.name)->ordinal);
                }
                if (update.name != field.name && opening.contains(update.name)) {
                    QVERIFY2(update.value == opening.value(update.name),
                        qPrintable(QString::fromUtf8(field.name) + " changes off-target "
                            + QString::fromUtf8(update.name)));
                }
            }

            const QStringList savedStrings = txProfileSavedStrings(field, edit);
            QCOMPARE(savedStrings.size(), field.savedKeys.size());
            QVERIFY(manager->saveProfile("Edited", &tx));
            const QHash<QString, QVariant> captured = storedProfile("Edited");
            QCOMPARE(captured.size(), 108);
            QJsonObject saved;
            for (int i = 0; i < field.savedKeys.size(); ++i) {
                const QString key = field.savedKeys.at(i);
                QCOMPARE(captured.value(key).toString(), savedStrings.at(i));
                const QString storedKey = QStringLiteral("hardware/%1/tx/profile/Edited/%2").arg(mac, key);
                QCOMPARE(AppSettings::instance().value(storedKey).toString(), savedStrings.at(i));
                saved.insert(key, savedStrings.at(i));
            }
            QVERIFY(AppSettings::instance().save());
            // Reload the serialized XML before profile selection: this
            // exercises persisted strings, not just the in-memory store.
            AppSettings::instance().load();
            QCOMPARE(storedProfile("Edited"), captured);
            QVERIFY(manager->setActiveProfile("Alpha", &tx));
            QCOMPARE(schema.read(*property, &tx), opening.value(field.name));
            QVERIFY(manager->setActiveProfile("Edited", &tx));
            QCOMPARE(schema.read(*property, &tx), changed);
            QVERIFY(manager->saveProfile("Restored", &tx));
            QCOMPARE(storedProfile("Restored"), captured);
            const QList<MirrorUpdate> restored = core.server->stateMirror()->currentValues("transmit", {property->ordinal});
            QCOMPARE(restored.size(), 1);
            cases.append(QJsonObject{
                {"field", QString::fromUtf8(field.name)},
                {"delta", QJsonDocument::fromJson(changedWire).object()},
                {"saved", saved},
                {"restored", QJsonDocument::fromJson(SessionMessages::encode(
                    SessionMessages::delta("transmit", restored))).object()}});
            QVERIFY(!tx.isMox());
            QVERIFY(!tx.isTune());
            QVERIFY(core.model->connection() == nullptr);
        }
        fixture.insert("cases", cases);
        const QString output = qEnvironmentVariable("NEREUS_TX_PROFILE_WATCH_FIXTURE");
        if (!output.isEmpty()) {
            QSaveFile file(output);
            QVERIFY2(file.open(QIODevice::WriteOnly), qPrintable(file.errorString()));
            const QByteArray bytes = QJsonDocument(fixture).toJson(QJsonDocument::Indented);
            QCOMPARE(file.write(bytes), bytes.size());
            QVERIFY(file.commit());
        }
        const QString fixturePath = output.isEmpty()
            ? QFINDTESTDATA("data/link/v1/tx-profile-watch-60.json") : output;
        QFile file(fixturePath);
        QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(file.errorString()));
        const QJsonObject checked = QJsonDocument::fromJson(file.readAll()).object();
        QCOMPARE(checked.value("audio"), fixture.value("audio"));
        QCOMPARE(checked.value("cases").toArray().size(), kTxProfileWatchFields.size());
        bool checkedTransmit = false;
        bool checkedSchema = false;
        bool checkedSnapshotComplete = false;
        for (const QJsonValue& value : checked.value("initial").toArray()) {
            SessionMessage message;
            QVERIFY(SessionMessages::decode(QJsonDocument(value.toObject()).toJson(
                QJsonDocument::Compact), &message));
            if (message.kind == SessionMessageKind::Schema) {
                QCOMPARE(message.className, wireClass);
                QCOMPARE(value, initial.first());
                checkedSchema = true;
            } else if (message.kind == SessionMessageKind::ObjectCreate) {
                QCOMPARE(message.objectKey, QByteArray("transmit"));
                QCOMPARE(message.className, wireClass);
                QCOMPARE(message.updates.size(), publishedSnapshotSize);
                for (const TxProfileWatchField& field : kTxProfileWatchFields) {
                    const auto found = std::find_if(message.updates.cbegin(), message.updates.cend(),
                        [&field](const MirrorUpdate& update) { return update.name == field.name; });
                    QVERIFY2(found != message.updates.cend(), field.name.constData());
                    QCOMPARE(found->ordinal, schema.byName(field.name)->ordinal);
                    QCOMPARE(found->kind, field.kind);
                    QCOMPARE(found->value, opening.value(field.name));
                }
                checkedTransmit = true;
            } else if (message.kind == SessionMessageKind::SnapshotComplete) {
                checkedSnapshotComplete = true;
            } else {
                QFAIL("Unexpected initial fixture message");
            }
        }
        QVERIFY(checkedSchema && checkedTransmit && checkedSnapshotComplete);
        // Match every encoded persisted-value observation; unrelated Core
        // derived revisions in complete messages are not profile values.
        for (int i = 0; i < cases.size(); ++i) {
            const QJsonObject expected = cases.at(i).toObject();
            const QJsonObject stored = checked.value("cases").toArray().at(i).toObject();
            QCOMPARE(stored.value("field"), expected.value("field"));
            QCOMPARE(stored.value("saved"), expected.value("saved"));
            QCOMPARE(stored.value("restored"), expected.value("restored"));
            const TxProfileWatchField& field = kTxProfileWatchFields.at(i);
            SessionMessage delta;
            SessionMessage restored;
            QVERIFY(SessionMessages::decode(QJsonDocument(stored.value("delta").toObject()).toJson(
                QJsonDocument::Compact), &delta));
            QVERIFY(SessionMessages::decode(QJsonDocument(stored.value("restored").toObject()).toJson(
                QJsonDocument::Compact), &restored));
            QCOMPARE(delta.kind, SessionMessageKind::Delta);
            QCOMPARE(delta.objectKey, QByteArray("transmit"));
            QCOMPARE(restored.updates.size(), 1);
            const auto found = std::find_if(delta.updates.cbegin(), delta.updates.cend(),
                [&field](const MirrorUpdate& update) { return update.name == field.name; });
            QVERIFY2(found != delta.updates.cend(), field.name.constData());
            QCOMPARE(found->ordinal, schema.byName(field.name)->ordinal);
            QCOMPARE(found->kind, field.kind);
            QCOMPARE(found->value, restored.updates.first().value);
            QVERIFY2(found->value != opening.value(field.name), field.name.constData());
            for (const MirrorUpdate& update : delta.updates) {
                QVERIFY2(update.name != "activeTxProfile", field.name.constData());
                if (opening.contains(update.name)) {
                    QCOMPARE(update.kind, schema.byName(update.name)->kind);
                    QCOMPARE(update.ordinal, schema.byName(update.name)->ordinal);
                }
                if (update.name != field.name && opening.contains(update.name)) {
                    QVERIFY2(update.value == opening.value(update.name),
                        qPrintable(QString::fromUtf8(field.name) + " changes off-target "
                            + QString::fromUtf8(update.name)));
                }
            }
            // lineInGain is a nonwatched derivation of lineInBoost; derived
            // editor/revision views may also accompany saved-value changes.
            // Such accompanying values do not prove watch coverage.
        }
    }

    // Version 16: a phone that declares version 16 or later reads the HL2
    // Options rows on HL2 I/O, the stored-only ones disabled with their
    // reason; a version 15 phone keeps version 13's Hardware. Version 17
    // (the Alex-1 low-pass rows) keeps them, and version 18 (HL2 Options'
    // clock rows) opens the clock rows; version 23 (Calibration's Rx1 6m
    // LNA row) is Hardware's cap; 24 (Audio > TX Input) is the
    // description's own (21: CAT & Network's Forget row follows Duplicate;
    // 22: DSP's RX buffer size lock).
    void pairedV16PhoneReadsHl2Options()
    {
        const auto hl2OptionsOf = [](const QJsonObject& hardware) {
            QJsonObject options;
            for (const QJsonValue& page : hardware.value("pages").toArray()) {
                for (const QJsonValue& section : page.toObject().value("sections").toArray()) {
                    if (section.toObject().value("title") == QJsonValue("Hermes Lite Options")) {
                        options = section.toObject();
                    }
                }
            }
            return options;
        };
        // {declared, capability sent back, Hardware version the phone reads}
        const QList<std::tuple<int, int, int>> declarations{
            {16, 16, 16}, {17, 17, 17}, {18, 18, 18}, {19, 19, 18}, {20, 20, 18},
            {21, 21, 18}, {22, 22, 18}, {23, 23, 23}, {24, 24, 23}, {99, 24, 23},
            {15, 15, 13}};
        for (const auto& [declared, granted, received] : declarations) {
            // One Core per phone: five phones are more than a Core's places.
            Core core;
            core.server->setupDescription()->setRadioContext(core.model->boardCapabilities(),
                                                             core.model->hardwareProfile().model,
                                                             core.model->currentRadioInfo());
            Device phone(QStringLiteral("HL2 V%1 iPhone").arg(declared), QStringLiteral("phone"));
            core.pair(phone);
            QHash<QByteArray, int> features = kHolder;
            features.insert("setupDescription", declared);
            auto* peer = core.signIn(phone, features);
            QVERIFY(admitted(peer));
            QCOMPARE(capability(peer->received(), QStringLiteral("setupDescriptionVersion")),
                     std::optional<qint64>(granted));
            const QJsonObject hardware = QJsonDocument::fromJson(latest(peer->received(),
                QStringLiteral("setup"), QStringLiteral("hardware")).toString().toUtf8()).object();
            QCOMPARE(hardware.value("version"), QJsonValue(received));
            const QJsonArray rows = hl2OptionsOf(hardware).value("controls").toArray();
            QCOMPARE(rows.size(), received >= 16 ? 9 : 0);
            if (received >= 16) {
                const QJsonObject swap = rows.last().toObject();
                QCOMPARE(swap.value("id"), QJsonValue("hardware.hl2Io.swapAudioChannels"));
                // Open at every version from 16: the Core sends the HL2
                // its receive audio (radio codec lane).
                QVERIFY(!swap.contains("availability"));
                // The clock rows are open from version 18; a version 16 or
                // 17 phone keeps them closed.
                const QJsonObject cl2 = rows.at(2).toObject();
                QCOMPARE(cl2.value("id"), QJsonValue("hardware.hl2Io.cl2Enable"));
                QCOMPARE(cl2.contains("availability"), received < 18);
                QCOMPARE(rows.at(3).toObject().contains("enabledWhen"), received >= 18);
            }
        }
    }

    // Version 23: a version 23 phone reads Calibration's Rx1 6m LNA row and
    // its write reaches the Core's per-radio key, on a receive-only Core too
    // (a receive calibration, not a transmit setting); a version 22 phone's
    // Calibration page is unchanged.
    void pairedV23PhoneReadsAndWritesRx1SixMeterLna()
    {
        Core core;
        const QString mac = core.model->currentRadioInfo().macAddress;
        core.model->setReceiveOnlyStationPolicy(true);
        core.server->setupDescription()->setRadioContext(core.model->boardCapabilities(),
                                                         core.model->hardwareProfile().model,
                                                         core.model->currentRadioInfo());
        Device current(QStringLiteral("LNA V23 iPhone"), QStringLiteral("phone"));
        Device older(QStringLiteral("LNA V22 iPhone"), QStringLiteral("phone"));
        core.pair(current);
        core.pair(older);
        QHash<QByteArray, int> v23 = kHolder;
        v23.insert("setupDescription", 23);
        QHash<QByteArray, int> v22 = kHolder;
        v22.insert("setupDescription", 22);
        auto* app = core.signIn(current, v23);
        auto* olderApp = core.signIn(older, v22);
        QVERIFY(admitted(app) && admitted(olderApp));
        QCOMPARE(capability(app->received(), QStringLiteral("setupDescriptionVersion")),
                 std::optional<qint64>(23));

        const auto calibrationOf = [](LoopbackTransport* peer) {
            const QJsonObject hardware = QJsonDocument::fromJson(latest(peer->received(),
                QStringLiteral("setup"), QStringLiteral("hardware")).toString().toUtf8()).object();
            for (const QJsonValue& page : hardware.value("pages").toArray()) {
                if (page.toObject().value("id") == QJsonValue("hardware.calibration")) {
                    return page.toObject();
                }
            }
            return QJsonObject{};
        };
        const QJsonArray sections = calibrationOf(app).value("sections").toArray();
        QCOMPARE(sections.size(), 2);
        QCOMPARE(sections.at(0).toObject().value("title"), QJsonValue("Level Cal"));
        const QJsonArray rows = sections.at(0).toObject().value("controls").toArray();
        QCOMPARE(rows.size(), 1);
        const QJsonObject row = rows.first().toObject();
        QCOMPARE(row.value("id"), QJsonValue("hardware.calibration.rx1_6mLna"));
        QCOMPARE(row.value("binding"), QJsonValue(QJsonObject{{"radioSetting", "cal/rx1_6mLna"}}));
        QVERIFY(SetupDescriptionService::validateHardwareV23Control(row));

        const QJsonArray olderSections = calibrationOf(olderApp).value("sections").toArray();
        QCOMPARE(olderSections.size(), 1);
        QCOMPARE(olderSections.at(0).toObject().value("title"), QJsonValue("TX Display Cal"));

        const QString key = QStringLiteral("hardware/%1/cal/rx1_6mLna").arg(mac);
        app->sendText(SessionMessages::encode(
            SessionMessages::settingsWrite(key, QStringLiteral("7"), QStringLiteral("phone"))));
        QTRY_COMPARE(core.settings->value(key).toString(), QStringLiteral("7"));
    }

    // Version 24 (radio codec lane): on the HL2 Core a version 24 phone
    // reads TX Input's Hermes rows, titled for the HL2 with the audio add-on
    // note, Line In Gain in 1.5 dB steps; a version 23 phone reads version
    // 15's whole decibels. Both reach the Core's Line In Gain.
    void pairedV24PhoneReadsHl2LineInGainSteps()
    {
        Core core;
        core.server->setupDescription()->setRadioContext(core.model->boardCapabilities(),
                                                         core.model->hardwareProfile().model,
                                                         core.model->currentRadioInfo());
        QVERIFY(core.model->boardCapabilities().radioMicNeedsAddOn);
        Device current(QStringLiteral("Line V24 iPhone"), QStringLiteral("phone"));
        Device older(QStringLiteral("Line V23 iPhone"), QStringLiteral("phone"));
        core.pair(current);
        core.pair(older);
        QHash<QByteArray, int> v24 = kHolder;
        v24.insert("setupDescription", 24);
        QHash<QByteArray, int> v23 = kHolder;
        v23.insert("setupDescription", 23);
        auto* app = core.signIn(current, v24);
        auto* olderApp = core.signIn(older, v23);
        QVERIFY(admitted(app) && admitted(olderApp));
        QCOMPARE(capability(app->received(), QStringLiteral("setupDescriptionVersion")),
                 std::optional<qint64>(24));

        const auto hermesOf = [](LoopbackTransport* peer, QJsonValue* version) {
            const QJsonObject audio = QJsonDocument::fromJson(latest(peer->received(),
                QStringLiteral("setup"), QStringLiteral("audio")).toString().toUtf8()).object();
            *version = audio.value("version");
            for (const QJsonValue& page : audio.value("pages").toArray()) {
                for (const QJsonValue& section : page.toObject().value("sections").toArray()) {
                    if (section.toObject().value("title")
                        == QJsonValue("Radio Mic (Hermes Lite 2)")) {
                        return section.toObject().value("controls").toArray();
                    }
                }
            }
            return QJsonArray{};
        };
        QJsonValue version;
        const QJsonArray rows = hermesOf(app, &version);
        QCOMPARE(version, QJsonValue(24));
        QCOMPARE(rows.size(), 3);
        const QJsonObject gain = rows.at(2).toObject();
        QCOMPARE(gain.value("id"), QJsonValue("audio.txInput.hermesLineInGain"));
        QCOMPARE(gain.value("step"), QJsonValue(1.5));
        QCOMPARE(gain.value("min"), QJsonValue(-34.5));
        QCOMPARE(gain.value("tooltip"), QJsonValue(
            "Needs the Hermes Lite 2 audio add-on board. A stock Hermes Lite 2 sends no mic audio."));
        // Without the HL2's note it is the published row.
        QJsonObject published = gain;
        published.insert("tooltip", QString());
        QVERIFY(SetupDescriptionService::validateAudioV24Control(published));

        const QJsonArray olderRows = hermesOf(olderApp, &version);
        QCOMPARE(version, QJsonValue(15));
        QCOMPARE(olderRows.size(), 3);
        QCOMPARE(olderRows.at(2).toObject().value("step"), QJsonValue(1));
        QCOMPARE(olderRows.at(2).toObject().value("min"), QJsonValue(-34));
        QVERIFY(!olderRows.at(2).toObject().contains("decimals"));
    }

    // Version 13 (R-R3-49, R-IOS-18): a paired phone reads PA and Hardware
    // Config's new rows while a version 12 phone keeps its projection, and
    // the described radio settings reach the Core through the gates the
    // desktop's own writes go through: the Watt Meter's points off the air
    // only, TX Display Cal on the air too.
    void pairedV13PaAndHardwareWritesUseTheCoresSettingsGates()
    {
        Core core;
        const RadioInfo info = core.model->currentRadioInfo();
        core.model->setReceiveOnlyStationPolicy(true);
        core.server->setupDescription()->setRadioContext(core.model->boardCapabilities(),
                                                         core.model->hardwareProfile().model,
                                                         info);
        Device current(QStringLiteral("PA V13 iPhone"), QStringLiteral("phone"));
        Device older(QStringLiteral("PA V12 iPhone"), QStringLiteral("phone"));
        core.pair(current);
        core.pair(older);
        QHash<QByteArray, int> v13Features = kHolder;
        v13Features.insert("setupDescription", 13);
        auto* v13 = core.signIn(current, v13Features);
        QVERIFY(admitted(v13));
        QCOMPARE(capability(v13->received(), QStringLiteral("setupDescriptionVersion")),
                 std::optional<qint64>(13));
        const QJsonObject pa = QJsonDocument::fromJson(latest(v13->received(),
            QStringLiteral("setup"), QStringLiteral("pa")).toString().toUtf8()).object();
        QCOMPARE(pa.value("version"), QJsonValue(13));
        const QJsonObject hardware = QJsonDocument::fromJson(latest(v13->received(),
            QStringLiteral("setup"), QStringLiteral("hardware")).toString().toUtf8()).object();
        QCOMPARE(hardware.value("version"), QJsonValue(13));
        QJsonObject board;
        QJsonObject offset;
        for (const QJsonValue& page : hardware.value("pages").toArray()) {
            for (const QJsonValue& section : page.toObject().value("sections").toArray()) {
                for (const QJsonValue& raw : section.toObject().value("controls").toArray()) {
                    const QJsonObject control = raw.toObject();
                    if (control.value("id") == QJsonValue("hardware.radioInfo.board")) { board = control; }
                    if (control.value("id") == QJsonValue("hardware.calibration.txDisplayOffset")) {
                        offset = control;
                    }
                }
            }
        }
        QCOMPARE(board.value("value"), QJsonValue("Bench HL2"));
        QVERIFY(!offset.isEmpty());
        QJsonObject point;
        for (const QJsonValue& page : pa.value("pages").toArray()) {
            for (const QJsonValue& section : page.toObject().value("sections").toArray()) {
                for (const QJsonValue& raw : section.toObject().value("controls").toArray()) {
                    if (raw.toObject().value("id") == QJsonValue("pa.wattMeter.calPoint3")) {
                        point = raw.toObject();
                    }
                }
            }
        }
        QCOMPARE(point.value("label"), QJsonValue("3 W"));
        QCOMPARE(point.value("boardClass"), QJsonValue(int(PaCalBoardClass::Anan10)));
        // The Core takes a point on the air (below), so the gate says so.
        QCOMPARE(point.value("gate"), QJsonValue(QJsonObject{
            {"capability", "transmitSettingsVersion"}, {"min", 6}}));

        QHash<QByteArray, int> v12Features = kHolder;
        v12Features.insert("setupDescription", 12);
        auto* v12 = core.signIn(older, v12Features);
        QVERIFY(admitted(v12));
        const QJsonObject oldPa = QJsonDocument::fromJson(latest(v12->received(),
            QStringLiteral("setup"), QStringLiteral("pa")).toString().toUtf8()).object();
        QCOMPARE(oldPa.value("version"), QJsonValue(5));
        QVERIFY(!QJsonDocument(oldPa).toJson().contains("pa.wattMeter"));
        // The HL2 has no ALEX filters: no Hardware category before 13.
        QVERIFY(latest(v12->received(), QStringLiteral("setup"),
                       QStringLiteral("hardware")).toString().isEmpty());

        // The key a phone writes: hardware/<the Core's radio>/<radioSetting>.
        const auto keyOf = [&info](const QJsonObject& control) {
            return QStringLiteral("hardware/%1/%2").arg(info.macAddress,
                control.value("binding").toObject().value("radioSetting").toString());
        };
        const auto rejectReason = [v13](const QString& key) {
            QString reason;
            for (const QByteArray& wire : v13->received()) {
                SessionMessage message;
                if (SessionMessages::decode(wire, &message)
                    && message.kind == SessionMessageKind::SettingsReject
                    && QString::fromUtf8(message.objectKey) == key) {
                    reason = message.reason;
                }
            }
            return reason;
        };
        const QString pointKey = keyOf(point);
        const QString offsetKey = keyOf(offset);
        v13->sendText(SessionMessages::encode(SessionMessages::settingsWrite(
            pointKey, QStringLiteral("3.4"), QStringLiteral("phone"))));
        QTRY_COMPARE(core.settings->value(pointKey).toString(), QStringLiteral("3.4"));

        allowTransmit(core);
        core.model->setReceiveOnlyStationPolicy(true);
        MoxController* mox = core.model->moxController();
        mox->setMoxCheck({});
        mox->setMox(true); // logical test state, no radio transport
        QTRY_COMPARE(mox->state(), MoxState::Tx);
        // Remote parity on the air (transmitSettingsVersion 13): the local
        // Watt Meter page changes a point while transmitting, so the Core
        // takes it.
        v13->sendText(SessionMessages::encode(SessionMessages::settingsWrite(
            pointKey, QStringLiteral("5"), QStringLiteral("phone"))));
        QTRY_COMPARE(core.settings->value(pointKey).toString(), QStringLiteral("5"));
        QVERIFY(rejectReason(pointKey).isEmpty());
        v13->sendText(SessionMessages::encode(SessionMessages::settingsWrite(
            offsetKey, QStringLiteral("-2.5"), QStringLiteral("phone"))));
        QTRY_COMPARE(core.settings->value(offsetKey).toString(), QStringLiteral("-2.5"));
        QVERIFY(rejectReason(offsetKey).isEmpty());
        mox->setMox(false);
        QTRY_COMPARE(mox->state(), MoxState::Rx);
        QVERIFY(!core.model->tune());
    }

    // Version 13 (R-R3-46, R-R3-49): a paired phone reads Disable HF PA,
    // the Alex receive filter rows and the whole radio's sample rate from
    // the running Core, and an Alex row it edits reaches the Core's
    // settings; a version 12 phone sees none of them.
    void pairedV13ReadsHfPaAlexRowsAndSampleRate()
    {
        Core core;
        const RadioInfo info = core.model->currentRadioInfo();
        // An ANAN-G2 Core: both Alex filter pages. The Core reads its
        // radio's context again for each peer's snapshot.
        core.model->setBoardForTest(HPSDRHW::Saturn);
        core.model->setHpsdrModelForTest(HPSDRModel::ANAN_G2);
        QCOMPARE(core.model->boardCapabilities().board, HPSDRHW::Saturn);
        Device current(QStringLiteral("Filters V13 iPhone"), QStringLiteral("phone"));
        Device older(QStringLiteral("Filters V12 iPhone"), QStringLiteral("phone"));
        core.pair(current);
        core.pair(older);
        QHash<QByteArray, int> v13Features = kHolder;
        v13Features.insert("setupDescription", 13);
        auto* v13 = core.signIn(current, v13Features);
        QVERIFY(admitted(v13));
        const QString transmit = latest(v13->received(), QStringLiteral("setup"),
                                        QStringLiteral("transmit")).toString();
        QVERIFY(transmit.contains(QStringLiteral("transmit.power.DisableHfPa")));
        QCOMPARE(QJsonDocument::fromJson(transmit.toUtf8()).object().value("version"),
                 QJsonValue(13));
        const QString hardware = latest(v13->received(), QStringLiteral("setup"),
                                        QStringLiteral("hardware")).toString();
        QVERIFY(hardware.contains(QStringLiteral("hardware.alex1Filters.bpf1.6mBP.bypass")));
        QVERIFY(hardware.contains(QStringLiteral("hardware.alex2Filters.bypass55MhzBpf")));
        QVERIFY(hardware.contains(QStringLiteral("hardware.alex1Filters.hpfBypassOnTx")));
        QVERIFY(hardware.contains(QStringLiteral("hardware.radioInfo.sampleRate")));

        QHash<QByteArray, int> v12Features = kHolder;
        v12Features.insert("setupDescription", 12);
        auto* v12 = core.signIn(older, v12Features);
        QVERIFY(admitted(v12));
        const QString oldTransmit = latest(v12->received(), QStringLiteral("setup"),
                                           QStringLiteral("transmit")).toString();
        QVERIFY(!oldTransmit.isEmpty());
        QVERIFY(!oldTransmit.contains(QStringLiteral("DisableHfPa")));
        const QString oldHardware = latest(v12->received(), QStringLiteral("setup"),
                                           QStringLiteral("hardware")).toString();
        QVERIFY(!oldHardware.contains(QStringLiteral("alex1Filters")));
        QVERIFY(!oldHardware.contains(QStringLiteral("sampleRate")));

        // The Start box of the 1.5 MHz high-pass row, written as the phone
        // writes it: hardware/<the Core's radio>/<radioSetting>.
        const QString key = QStringLiteral("hardware/%1/alex/hpf/1_5MHz/start")
            .arg(info.macAddress);
        v13->sendText(SessionMessages::encode(SessionMessages::settingsWrite(
            key, QStringLiteral("1.9"), QStringLiteral("phone"))));
        QTRY_COMPARE(core.settings->value(key).toString(), QStringLiteral("1.9"));

        // A switch above the rows, written the same way: Disable 6m LNA
        // on RX.
        const QString lnaKey = QStringLiteral("hardware/%1/alex/master/disable6mLnaOnRx")
            .arg(info.macAddress);
        v13->sendText(SessionMessages::encode(SessionMessages::settingsWrite(
            lnaKey, QStringLiteral("True"), QStringLiteral("phone"))));
        QTRY_COMPARE(core.settings->value(lnaKey).toString(), QStringLiteral("True"));
    }

    // R-R3-49 (lead's ruling): the Core refuses a calibration write whole
    // when the value is outside the control's range, says the range in plain
    // words, and hands back its own value; an in-range write is taken.
    // R-R3-49 / R-IOS-18 (paProfileVersion 1): a paired phone that declares
    // paProfiles reads the Core's PA Gain profiles on the `paProfiles` object
    // and changes them with the paProfile verbs, as the local PA Gain page
    // does, through the gates the desktop's own profile writes meet: off the
    // air on a receive-only Core; not at all for a peer that did not declare
    // the feature.
    void pairedPaProfileVerbsChangeTheCoresBankOffTheAirOnly()
    {
        Core core;
        const QString mac = core.model->currentRadioInfo().macAddress;
        PaProfileManager* bank = core.model->paProfileManager();
        QVERIFY(bank != nullptr);
        bank->setMacAddress(mac);
        bank->load(core.model->hardwareProfile().model);
        core.model->setReceiveOnlyStationPolicy(true);
        const QString factory = bank->activeProfileName();
        QVERIFY(!factory.isEmpty());

        Device phone(QStringLiteral("PA profile iPhone"), QStringLiteral("phone"));
        Device older(QStringLiteral("No PA profile iPhone"), QStringLiteral("phone"));
        core.pair(phone);
        core.pair(older);
        QHash<QByteArray, int> features = kHolder;
        features.insert("paProfiles", 1);
        auto* app = core.signIn(phone, features);
        QVERIFY(admitted(app));
        QCOMPARE(capability(app->received(), QStringLiteral("paProfileVersion")),
                 std::optional<qint64>(1));
        const auto mirror = [app]() {
            return QJsonDocument::fromJson(latest(app->received(), QStringLiteral("paProfiles"),
                                                  QStringLiteral("json")).toString().toUtf8())
                .object();
        };
        QTRY_COMPARE(mirror().value("active"), QJsonValue(factory));
        QCOMPARE(mirror().value("bands").toArray().size(), 14);
        QCOMPARE(mirror().value("bands").toArray().first().toObject().value("band"),
                 QJsonValue("160m"));
        QVERIFY(mirror().value("names").toArray().contains(QJsonValue(factory)));

        const auto ok = [&core, app](const QByteArray& verb, const QList<MirrorUpdate>& args) {
            const QJsonObject result = core.invoke(app, verb, args);
            return result.value("accepted").toBool()
                ? QString() : result.value("reason").toString(QStringLiteral("(no answer)"));
        };
        const auto name = [](const QString& n) {
            return MirrorUpdate{0, "name", MirrorWireKind::Utf8, n};
        };
        const auto band = [](int b) { return MirrorUpdate{0, "band", MirrorWireKind::Int64, qlonglong(b)}; };
        const auto value = [](double v) { return MirrorUpdate{0, "value", MirrorWireKind::Float64, v}; };

        QCOMPARE(ok("paProfile.new", {name("Contest")}), QString());
        QCOMPARE(bank->activeProfileName(), QStringLiteral("Contest"));
        QTRY_COMPARE(mirror().value("active"), QJsonValue("Contest"));
        QCOMPARE(ok("paProfile.new", {name("Default mine")}),
                 QStringLiteral("Profile names starting with \"Default\" are reserved for factory "
                                "entries. Choose a different name."));
        QCOMPARE(ok("paProfile.new", {name("contest")}),
                 QStringLiteral("A profile named \"contest\" already exists. Choose a different name."));
        // 20 m is Band 5.
        QCOMPARE(ok("paProfile.setGain", {band(5), value(47.54)}), QString());
        QCOMPARE(bank->activeProfile()->getGainForBand(Band::Band20m), 47.5f);
        QTRY_COMPARE(mirror().value("bands").toArray().at(5).toObject().value("gain").toDouble(),
                     47.5);
        QCOMPARE(ok("paProfile.setGain", {band(5), value(30.0)}),
                 QStringLiteral("Choose a PA gain from 38.8 to 100 dB."));
        QCOMPARE(ok("paProfile.setAdjust",
                    {band(5), MirrorUpdate{0, "step", MirrorWireKind::Int64, qlonglong(8)},
                     value(-1.2)}), QString());
        QCOMPARE(bank->activeProfile()->getAdjust(Band::Band20m, 8), -1.2f);
        QCOMPARE(ok("paProfile.setAdjust",
                    {band(5), MirrorUpdate{0, "step", MirrorWireKind::Int64, qlonglong(8)},
                     value(10.5)}), QStringLiteral("Choose a drive-step adjust from -10 to 10 dB."));
        QCOMPARE(ok("paProfile.setMaxPower", {band(5), value(80.0)}), QString());
        QCOMPARE(ok("paProfile.setUseMax", {band(5), MirrorUpdate{0, "on", MirrorWireKind::Bool, true}}),
                 QString());
        QVERIFY(bank->activeProfile()->getMaxPowerUse(Band::Band20m));
        QCOMPARE(ok("paProfile.setMaxPower", {band(5), value(1500.5)}),
                 QStringLiteral("Choose a max power from 0 to 1500 W."));
        QCOMPARE(ok("paProfile.copy", {name("Contest 2")}), QString());
        QCOMPARE(bank->activeProfile()->getGainForBand(Band::Band20m), 47.5f);
        QCOMPARE(ok("paProfile.delete", {name("Contest 2")}), QString());
        // Thetis selects the radio's Default profile after a delete.
        QCOMPARE(bank->activeProfileName(), factory);
        QCOMPARE(ok("paProfile.select", {name("Contest")}), QString());
        QCOMPARE(ok("paProfile.reset", {}), QString());
        QCOMPARE(bank->activeProfile()->getMaxPowerUse(Band::Band20m), false);
        QCOMPARE(ok("paProfile.select", {name("Nothing")}),
                 QStringLiteral("There is no PA profile called Nothing."));

        // On the air, a device that does not hold transmit changes nothing
        // (onAirPaProfileVerbsFollowThetisForTheHolderOnly has the rest).
        const float before = bank->activeProfile()->getGainForBand(Band::Band20m);
        allowTransmit(core);
        core.model->setReceiveOnlyStationPolicy(true);
        MoxController* mox = core.model->moxController();
        mox->setMoxCheck({});
        mox->setMox(true); // logical test state, no radio transport
        QTRY_COMPARE(mox->state(), MoxState::Tx);
        QCOMPARE(ok("paProfile.setGain", {band(5), value(48.0)}),
                 QStringLiteral("Only the device that is transmitting can change this."));
        QCOMPARE(ok("paProfile.select", {name(factory)}),
                 QStringLiteral("Can't change while transmitting."));
        QCOMPARE(bank->activeProfile()->getGainForBand(Band::Band20m), before);
        mox->setMox(false);
        QTRY_COMPARE(mox->state(), MoxState::Rx);

        // A peer that did not declare the feature sees nothing and changes
        // nothing.
        auto* old = core.signIn(older, kHolder);
        QVERIFY(admitted(old));
        QCOMPARE(capability(old->received(), QStringLiteral("paProfileVersion")),
                 std::optional<qint64>());
        QVERIFY(latest(old->received(), QStringLiteral("paProfiles"), QStringLiteral("json"))
                    .isNull());
        const QJsonObject refused = core.invoke(old, "paProfile.select", {name(factory)});
        QVERIFY(!refused.value("accepted").toBool(true));
        QVERIFY(!refused.value("reason").toString().isEmpty());
    }

    // R-R3-49 / R-IOS-18 / R-IOS-27 (JJ's ruling, follow Thetis): on the
    // air the device that holds transmit changes the transmitting band's
    // gain, drive-step adjust, max power and use-max, and an adjust moves
    // the drive to the step it adjusts (Thetis nudAdjustGain_ValueChanged,
    // setup.cs:24199-24225 [v2.10.3.15]). Every profile action and every
    // other band is refused (OnMoxChangeHandler, setup.cs:23826-23834), and
    // a device that does not hold transmit changes nothing.
    void onAirPaProfileVerbsFollowThetisForTheHolderOnly()
    {
        Core core(true);
        const QString mac = core.model->currentRadioInfo().macAddress;
        PaProfileManager* bank = core.model->paProfileManager();
        QVERIFY(bank != nullptr);
        bank->setMacAddress(mac);
        bank->load(core.model->hardwareProfile().model);
        allowTransmit(core); // the slice is on 20 m (Band 5)
        const QString factory = bank->activeProfileName();
        QVERIFY(bank->saveProfile(QStringLiteral("On-air spare"), *bank->activeProfile()));
        Device holderDevice(QStringLiteral("PA holder iPhone"), QStringLiteral("phone"));
        Device otherDevice(QStringLiteral("PA other iPad"), QStringLiteral("tablet"));
        core.pair(holderDevice);
        core.pair(otherDevice);
        QHash<QByteArray, int> features = kTransmitter;
        features.insert("paProfiles", 1);
        auto* holder = core.signIn(holderDevice, features);
        auto* other = core.signIn(otherDevice, features);
        QVERIFY(admitted(holder) && admitted(other));
        const QJsonObject key = core.invoke(holder, "tx.key",
                                            {MirrorUpdate{0, "trigger", MirrorWireKind::Utf8,
                                                          QStringLiteral("screen")}});
        QVERIFY2(key.value(QStringLiteral("accepted")).toBool(),
                 qPrintable(key.value(QStringLiteral("reason")).toString()));
        QTRY_VERIFY(core.model->isCoreOnAir());
        QVERIFY(core.server->transmitHolder()->isHeldBy(holderDevice.key.fingerprint()));

        const auto ok = [&core](LoopbackTransport* app, const QByteArray& verb,
                                const QList<MirrorUpdate>& args) {
            const QJsonObject result = core.invoke(app, verb, args);
            return result.value("accepted").toBool()
                ? QString() : result.value("reason").toString(QStringLiteral("(no answer)"));
        };
        const auto name = [](const QString& n) {
            return MirrorUpdate{0, "name", MirrorWireKind::Utf8, n};
        };
        const auto band = [](int b) { return MirrorUpdate{0, "band", MirrorWireKind::Int64, qlonglong(b)}; };
        const auto step = [](int s) { return MirrorUpdate{0, "step", MirrorWireKind::Int64, qlonglong(s)}; };
        const auto value = [](double v) { return MirrorUpdate{0, "value", MirrorWireKind::Float64, v}; };
        const auto on = [](bool b) { return MirrorUpdate{0, "on", MirrorWireKind::Bool, b}; };
        const QString locked = QStringLiteral("Can't change while transmitting.");

        // Allowed: the transmitting band's four values, from the holder.
        QCOMPARE(ok(holder, "paProfile.setGain", {band(5), value(47.0)}), QString());
        QCOMPARE(bank->activeProfile()->getGainForBand(Band::Band20m), 47.0f);
        core.model->transmitModel().setPower(100);
        QCOMPARE(ok(holder, "paProfile.setAdjust", {band(5), step(3), value(-1.5)}), QString());
        QCOMPARE(bank->activeProfile()->getAdjust(Band::Band20m, 3), -1.5f);
        // DRIVE_SLIDER: the drive moves to the step being adjusted (40%).
        QCOMPARE(core.model->transmitModel().power(), 40);
        QCOMPARE(ok(holder, "paProfile.setMaxPower", {band(5), value(60.0)}), QString());
        QCOMPARE(ok(holder, "paProfile.setUseMax", {band(5), on(true)}), QString());
        QVERIFY(bank->activeProfile()->getMaxPowerUse(Band::Band20m));
        // TUNE_SLIDER: the tune power moves instead.
        core.model->transmitModel().setTuneDrivePowerSource(DrivePowerSource::TuneSlider);
        QCOMPARE(ok(holder, "paProfile.setAdjust", {band(5), step(5), value(0.5)}), QString());
        QCOMPARE(core.model->transmitModel().tunePowerForTxBand(), 60);
        QCOMPARE(core.model->transmitModel().power(), 40);
        // FIXED: neither moves.
        core.model->transmitModel().setTuneDrivePowerSource(DrivePowerSource::Fixed);
        QCOMPARE(ok(holder, "paProfile.setAdjust", {band(5), step(1), value(0.2)}), QString());
        QCOMPARE(core.model->transmitModel().power(), 40);
        QCOMPARE(core.model->transmitModel().tunePowerForTxBand(), 60);
        core.model->transmitModel().setTuneDrivePowerSource(DrivePowerSource::DriveSlider);

        // Refused: any other band, and every profile action.
        const PaProfile before = *bank->activeProfile();
        QCOMPARE(ok(holder, "paProfile.setGain", {band(3), value(47.0)}), locked);
        QCOMPARE(ok(holder, "paProfile.setAdjust", {band(3), step(3), value(1.0)}), locked);
        QCOMPARE(ok(holder, "paProfile.setMaxPower", {band(3), value(50.0)}), locked);
        QCOMPARE(ok(holder, "paProfile.setUseMax", {band(3), on(true)}), locked);
        QCOMPARE(ok(holder, "paProfile.select", {name(QStringLiteral("On-air spare"))}), locked);
        QCOMPARE(ok(holder, "paProfile.new", {name(QStringLiteral("On-air new"))}), locked);
        QCOMPARE(ok(holder, "paProfile.copy", {name(QStringLiteral("On-air new"))}), locked);
        QCOMPARE(ok(holder, "paProfile.delete", {name(QStringLiteral("On-air spare"))}), locked);
        QCOMPARE(ok(holder, "paProfile.reset", {}), locked);
        QCOMPARE(bank->activeProfileName(), factory);
        QVERIFY(bank->profileNames().contains(QStringLiteral("On-air spare")));
        QVERIFY(!bank->profileNames().contains(QStringLiteral("On-air new")));
        QCOMPARE(bank->activeProfile()->dataToString(), before.dataToString());

        // Refused: every edit from a device that does not hold transmit.
        // With remote transmit on, the Core's transmit gate answers first
        // and names the holder; on a receive-only Core the PA rule does
        // (pairedPaProfileVerbsChangeTheCoresBankOffTheAirOnly).
        const QString holderNamed = QStringLiteral("PA holder iPhone has the transmitter.");
        QCOMPARE(ok(other, "paProfile.setGain", {band(5), value(48.0)}), holderNamed);
        QCOMPARE(ok(other, "paProfile.setAdjust", {band(5), step(3), value(1.0)}), holderNamed);
        QCOMPARE(ok(other, "paProfile.setMaxPower", {band(5), value(70.0)}), holderNamed);
        QCOMPARE(ok(other, "paProfile.setUseMax", {band(5), on(false)}), holderNamed);
        QCOMPARE(ok(other, "paProfile.select", {name(QStringLiteral("On-air spare"))}), holderNamed);
        QCOMPARE(ok(other, "paProfile.setGain", {band(3), value(48.0)}), holderNamed);
        QCOMPARE(bank->activeProfile()->dataToString(), before.dataToString());
        QCOMPARE(core.model->transmitModel().power(), 40);
    }

    // R-R3-49 / R-IOS-27 (JJ's ruling): a window's raw PA profile keys
    // (hardware/<mac>/pa/profile/...) follow the same on-air rule as the
    // verbs. The holder's change to the active profile's transmitting band
    // is taken and applied at once (an adjust moves the drive); any other
    // band, the active profile, the list and every other profile are
    // refused with the Core's value handed back, never held until receive.
    void onAirRawPaProfileKeysFollowThetisForTheHolderOnly()
    {
        Core core(true);
        const QString mac = core.model->currentRadioInfo().macAddress;
        PaProfileManager* bank = core.model->paProfileManager();
        QVERIFY(bank != nullptr);
        bank->setMacAddress(mac);
        bank->load(core.model->hardwareProfile().model);
        allowTransmit(core); // the slice is on 20 m (Band 5)
        const QString active = bank->activeProfileName();
        QVERIFY(bank->saveProfile(QStringLiteral("Raw spare"), *bank->activeProfile()));
        Device holderDevice(QStringLiteral("Raw holder iPhone"), QStringLiteral("phone"));
        Device otherDevice(QStringLiteral("Raw other iPad"), QStringLiteral("tablet"));
        core.pair(holderDevice);
        core.pair(otherDevice);
        auto* holder = core.signIn(holderDevice, kTransmitter);
        auto* other = core.signIn(otherDevice, kTransmitter);
        QVERIFY(admitted(holder) && admitted(other));
        const QJsonObject key = core.invoke(holder, "tx.key",
                                            {MirrorUpdate{0, "trigger", MirrorWireKind::Utf8,
                                                          QStringLiteral("screen")}});
        QVERIFY2(key.value(QStringLiteral("accepted")).toBool(),
                 qPrintable(key.value(QStringLiteral("reason")).toString()));
        QTRY_VERIFY(core.model->isCoreOnAir());

        const auto paKey = [&mac](const QString& rest) {
            return QStringLiteral("hardware/%1/pa/profile/%2").arg(mac, rest);
        };
        const auto rejects = [](LoopbackTransport* app, const QString& k) {
            int count = 0;
            QString reason;
            for (const QByteArray& wire : app->received()) {
                SessionMessage message;
                if (SessionMessages::decode(wire, &message)
                    && message.kind == SessionMessageKind::SettingsReject
                    && QString::fromUtf8(message.objectKey) == k) {
                    ++count;
                    reason = message.reason;
                }
            }
            return qMakePair(count, reason);
        };
        const auto write = [](LoopbackTransport* app, const QString& k, const QString& v) {
            app->sendText(SessionMessages::encode(
                SessionMessages::settingsWrite(k, v, QStringLiteral("window"))));
        };
        const auto remove = [](LoopbackTransport* app, const QString& k) {
            app->sendText(SessionMessages::encode(SessionMessages::settingsRemove(k)));
        };
        const QString locked = QStringLiteral("Can't change while transmitting.");

        // Taken, and applied at once: the transmitting band's gain.
        PaProfile edited = *bank->activeProfile();
        edited.setGainForBand(Band::Band20m, 46.0f);
        write(holder, paKey(active), edited.dataToString());
        QTRY_COMPARE(bank->activeProfile()->getGainForBand(Band::Band20m), 46.0f);
        QCOMPARE(core.settings->value(paKey(active)).toString(), edited.dataToString());
        // An adjust moves the drive to the step being adjusted (50%). The
        // bank is the process's: values no other test writes.
        core.model->transmitModel().setPower(100);
        edited.setAdjust(Band::Band20m, 4, -2.5f);
        write(holder, paKey(active), edited.dataToString());
        QTRY_COMPARE(bank->activeProfile()->getAdjust(Band::Band20m, 4), -2.5f);
        QCOMPARE(core.model->transmitModel().power(), 50);
        edited.setMaxPower(Band::Band20m, 65.0f);
        edited.setMaxPowerUse(Band::Band20m, !edited.getMaxPowerUse(Band::Band20m));
        const bool useMax = edited.getMaxPowerUse(Band::Band20m);
        write(holder, paKey(active), edited.dataToString());
        QTRY_COMPARE(bank->activeProfile()->getMaxPower(Band::Band20m), 65.0f);
        QCOMPARE(bank->activeProfile()->getMaxPowerUse(Band::Band20m), useMax);
        QCOMPARE(rejects(holder, paKey(active)).first, 0);

        // Refused, with the Core's value handed back: another band, the
        // active profile, the list, another profile, a remove.
        const QString kept = core.settings->value(paKey(active)).toString();
        PaProfile otherBand = *bank->activeProfile();
        otherBand.setGainForBand(Band::Band40m, 51.0f);
        write(holder, paKey(active), otherBand.dataToString());
        QTRY_COMPARE(rejects(holder, paKey(active)).second, locked);
        QCOMPARE(core.settings->value(paKey(active)).toString(), kept);
        const QString activeKept = core.settings->value(paKey(QStringLiteral("active"))).toString();
        write(holder, paKey(QStringLiteral("active")), QStringLiteral("Raw spare"));
        QTRY_COMPARE(rejects(holder, paKey(QStringLiteral("active"))).second, locked);
        QCOMPARE(core.settings->value(paKey(QStringLiteral("active"))).toString(), activeKept);
        const QString names = core.settings->value(paKey(QStringLiteral("_names"))).toString();
        write(holder, paKey(QStringLiteral("_names")), names + QStringLiteral(",Raw new"));
        QTRY_COMPARE(rejects(holder, paKey(QStringLiteral("_names"))).second, locked);
        QCOMPARE(core.settings->value(paKey(QStringLiteral("_names"))).toString(), names);
        const QString spare = core.settings->value(paKey(QStringLiteral("Raw spare"))).toString();
        write(holder, paKey(QStringLiteral("Raw spare")), otherBand.dataToString());
        QTRY_COMPARE(rejects(holder, paKey(QStringLiteral("Raw spare"))).second, locked);
        QCOMPARE(core.settings->value(paKey(QStringLiteral("Raw spare"))).toString(), spare);
        remove(holder, paKey(QStringLiteral("Raw spare")));
        QTRY_COMPARE(rejects(holder, paKey(QStringLiteral("Raw spare"))).first, 2);
        QCOMPARE(rejects(holder, paKey(QStringLiteral("Raw spare"))).second, locked);
        QVERIFY(bank->profileNames().contains(QStringLiteral("Raw spare")));
        QCOMPARE(bank->activeProfileName(), active);

        // Refused: a value out of the page's range, with the phone verbs'
        // words. The key's text is not clamped on the way in, so without the
        // check a gain of 20 dB would reach the live drive (Job B item 1).
        const auto rangeRefused = [&](const PaProfile& p, const QString& reason) {
            const int before = rejects(holder, paKey(active)).first;
            QSignalSpy volume(&core.model->transmitModel(), &TransmitModel::audioVolumeChanged);
            const int power = core.model->transmitModel().power();
            write(holder, paKey(active), p.dataToString());
            QTRY_COMPARE(rejects(holder, paKey(active)).first, before + 1);
            QCOMPARE(rejects(holder, paKey(active)).second, reason);
            QCOMPARE(core.settings->value(paKey(active)).toString(), kept);
            QCOMPARE(bank->activeProfile()->dataToString(), kept);
            QCOMPARE(core.model->transmitModel().power(), power);
            QCOMPARE(volume.count(), 0);
        };
        PaProfile lowGain = *bank->activeProfile();
        lowGain.setGainForBand(Band::Band20m, 20.0f);
        rangeRefused(lowGain, QStringLiteral("Choose a PA gain from 38.8 to 100 dB."));
        QCOMPARE(bank->activeProfile()->getGainForBand(Band::Band20m), 46.0f);
        PaProfile wideAdjust = *bank->activeProfile();
        wideAdjust.setAdjust(Band::Band20m, 3, 11.5f);
        rangeRefused(wideAdjust, QStringLiteral("Choose a drive-step adjust from -10 to 10 dB."));
        PaProfile bigMax = *bank->activeProfile();
        bigMax.setMaxPower(Band::Band20m, 1600.0f);
        rangeRefused(bigMax, QStringLiteral("Choose a max power from 0 to 1500 W."));

        // Refused: the transmitting band from a device that does not hold
        // transmit (the Core's transmit gate names the holder first).
        PaProfile notHolder = *bank->activeProfile();
        notHolder.setGainForBand(Band::Band20m, 44.0f);
        write(other, paKey(active), notHolder.dataToString());
        QTRY_COMPARE(rejects(other, paKey(active)).second,
                     QStringLiteral("Raw holder iPhone has the transmitter."));
        QCOMPARE(bank->activeProfile()->getGainForBand(Band::Band20m), 46.0f);
        QCOMPARE(core.settings->value(paKey(active)).toString(), kept);
    }

    // Version 20 (JJ's ruling, holder only): each paired device reads PA
    // Gain's on-the-air lock for itself. The holder's transmitting band
    // stays open; another device sees it locked with the plain reason; a
    // version 19 device keeps the closed rows.
    void onAirPaGainLockIsPublishedPerDevice()
    {
        Core core(true);
        allowTransmit(core); // the slice is on 20 m (Band 5)
        Device holderDevice(QStringLiteral("Lock holder iPhone"), QStringLiteral("phone"));
        Device otherDevice(QStringLiteral("Lock other iPad"), QStringLiteral("tablet"));
        Device olderDevice(QStringLiteral("Lock V19 iPhone"), QStringLiteral("phone"));
        core.pair(holderDevice);
        core.pair(otherDevice);
        core.pair(olderDevice);
        QHash<QByteArray, int> v20 = kTransmitter;
        v20.insert("setupDescription", 20);
        v20.insert("paProfiles", 1);
        QHash<QByteArray, int> v19 = v20;
        v19.insert("setupDescription", 19);
        auto* holder = core.signIn(holderDevice, v20);
        auto* other = core.signIn(otherDevice, v20);
        auto* older = core.signIn(olderDevice, v19);
        QVERIFY(admitted(holder) && admitted(other) && admitted(older));
        QCOMPARE(capability(holder->received(), QStringLiteral("setupDescriptionVersion")),
                 std::optional<qint64>(20));
        QCOMPARE(capability(older->received(), QStringLiteral("setupDescriptionVersion")),
                 std::optional<qint64>(19));

        const auto table = [](LoopbackTransport* app) {
            const QJsonObject pa = QJsonDocument::fromJson(latest(app->received(),
                QStringLiteral("setup"), QStringLiteral("pa")).toString().toUtf8()).object();
            for (const QJsonValue& page : pa.value("pages").toArray()) {
                for (const QJsonValue& section : page.toObject().value("sections").toArray()) {
                    for (const QJsonValue& raw : section.toObject().value("controls").toArray()) {
                        if (raw.toObject().value("id") == QJsonValue("pa.gain.table")) {
                            return raw.toObject();
                        }
                    }
                }
            }
            return QJsonObject{};
        };
        const auto rowLock = [&table](LoopbackTransport* app, int band) {
            for (const QJsonValue& row : table(app).value("rows").toArray()) {
                if (row.toObject().value("band") == QJsonValue(band)) {
                    return row.toObject().value("availability");
                }
            }
            return QJsonValue(QStringLiteral("no row"));
        };
        const QJsonObject locked{{"enabled", false},
                                 {"reason", RadioModel::paOnAirLockedReason()}};
        const QJsonObject holderOnly{{"enabled", false},
                                     {"reason", RadioModel::paHolderOnlyReason()}};
        QVERIFY(!table(holder).isEmpty());
        QCOMPARE(rowLock(holder, 5), QJsonValue(QJsonValue::Undefined));
        const QJsonObject olderTable = table(older);
        QVERIFY(SetupDescriptionService::validatePaV14Control(olderTable));

        const QJsonObject key = core.invoke(holder, "tx.key",
                                            {MirrorUpdate{0, "trigger", MirrorWireKind::Utf8,
                                                          QStringLiteral("screen")}});
        QVERIFY2(key.value(QStringLiteral("accepted")).toBool(),
                 qPrintable(key.value(QStringLiteral("reason")).toString()));
        QTRY_VERIFY(core.model->isCoreOnAir());

        QTRY_COMPARE(rowLock(holder, 3), QJsonValue(locked));
        QCOMPARE(rowLock(holder, 5), QJsonValue(QJsonValue::Undefined));
        QTRY_COMPARE(rowLock(other, 3), QJsonValue(locked));
        QCOMPARE(rowLock(other, 5), QJsonValue(holderOnly));
        QVERIFY(!QJsonDocument(table(other)).toJson().contains("holderMayEdit"));
        // The version 19 device's rows never change.
        QCOMPARE(table(older), olderTable);

        const QJsonObject unkey = core.invoke(holder, "tx.unkey", {int64("epoch", 1)});
        QVERIFY2(unkey.value(QStringLiteral("accepted")).toBool(),
                 qPrintable(unkey.value(QStringLiteral("reason")).toString()));
        QTRY_VERIFY(!core.model->isCoreOnAir());
        QTRY_COMPARE(rowLock(other, 5), QJsonValue(QJsonValue::Undefined));
        QCOMPARE(rowLock(other, 3), QJsonValue(QJsonValue::Undefined));
        QCOMPARE(rowLock(holder, 3), QJsonValue(QJsonValue::Undefined));
        QCOMPARE(table(older), olderTable);
    }

    // Version 22: DSP > Options' RX buffer sizes lock while the Core is on
    // the air, for every version 22 peer, receive-only ones (no remoteTx,
    // so no txState) included, as Thetis greys grpDSPBufferSize while MOX
    // is on (setup.cs:5159 [v2.10.3.15]). A version 20 peer's DSP never
    // changes, and the Core refuses the rows' keys on the air.
    void onAirRxBufferSizeLockReachesReceiveOnlyPeers()
    {
        Core core(true);
        allowTransmit(core);
        Device keyer(QStringLiteral("Buffer keyer iPhone"), QStringLiteral("phone"));
        Device listener(QStringLiteral("Buffer listener iPad"), QStringLiteral("tablet"));
        Device older(QStringLiteral("Buffer V20 iPhone"), QStringLiteral("phone"));
        core.pair(keyer);
        core.pair(listener);
        core.pair(older);
        QHash<QByteArray, int> transmitter = kTransmitter;
        transmitter.insert("setupDescription", 22);
        QHash<QByteArray, int> receiveOnly = kHolder;
        receiveOnly.insert("setupDescription", 22);
        QHash<QByteArray, int> v20 = kHolder;
        v20.insert("setupDescription", 20);
        auto* keyerApp = core.signIn(keyer, transmitter);
        auto* listenerApp = core.signIn(listener, receiveOnly);
        auto* olderApp = core.signIn(older, v20);
        QVERIFY(admitted(keyerApp) && admitted(listenerApp) && admitted(olderApp));
        QCOMPARE(capability(listenerApp->received(), QStringLiteral("setupDescriptionVersion")),
                 std::optional<qint64>(22));
        QCOMPARE(capability(olderApp->received(), QStringLiteral("setupDescriptionVersion")),
                 std::optional<qint64>(20));
        QVERIFY(!receiveOnly.contains("remoteTx"));

        const auto dspOf = [](LoopbackTransport* app) {
            return latest(app->received(), QStringLiteral("setup"), QStringLiteral("dsp"))
                .toString();
        };
        const auto rowLock = [&dspOf](LoopbackTransport* app, const QString& id) {
            const QJsonObject dsp = QJsonDocument::fromJson(dspOf(app).toUtf8()).object();
            for (const QJsonValue& page : dsp.value("pages").toArray()) {
                for (const QJsonValue& section : page.toObject().value("sections").toArray()) {
                    for (const QJsonValue& raw : section.toObject().value("controls").toArray()) {
                        if (raw.toObject().value("id") == QJsonValue(id)) {
                            return raw.toObject().value("availability");
                        }
                    }
                }
            }
            return QJsonValue(QStringLiteral("no row"));
        };
        const auto rejectReason = [](LoopbackTransport* app, const QString& key) {
            QString reason;
            for (const QByteArray& wire : app->received()) {
                SessionMessage message;
                if (SessionMessages::decode(wire, &message)
                    && message.kind == SessionMessageKind::SettingsReject
                    && QString::fromUtf8(message.objectKey) == key) {
                    reason = message.reason;
                }
            }
            return reason;
        };
        const QStringList rxIds{QStringLiteral("dsp.options.DspOptionsBufferSizePhoneRx"),
                                QStringLiteral("dsp.options.DspOptionsBufferSizeFmRx"),
                                QStringLiteral("dsp.options.DspOptionsBufferSizeCwRx"),
                                QStringLiteral("dsp.options.DspOptionsBufferSizeDigRx")};
        const QJsonObject locked{{"enabled", false},
                                 {"reason", RadioModel::dspBufferOnAirLockedReason()}};

        // Off the air: every row is enabled, and the receive-only peer's
        // write is taken.
        for (const QString& id : rxIds) {
            QCOMPARE(rowLock(listenerApp, id), QJsonValue(QJsonValue::Undefined));
        }
        const QString olderDsp = dspOf(olderApp);
        QVERIFY(!olderDsp.isEmpty());
        const QString key = QStringLiteral("DspOptionsBufferSizePhoneRx");
        listenerApp->sendText(SessionMessages::encode(SessionMessages::settingsWrite(
            key, QStringLiteral("256"), QStringLiteral("phone"))));
        QTRY_COMPARE(core.settings->value(key).toString(), QStringLiteral("256"));
        QVERIFY(rejectReason(listenerApp, key).isEmpty());

        const QJsonObject keyed = core.invoke(keyerApp, "tx.key",
                                              {MirrorUpdate{0, "trigger", MirrorWireKind::Utf8,
                                                            QStringLiteral("screen")}});
        QVERIFY2(keyed.value(QStringLiteral("accepted")).toBool(),
                 qPrintable(keyed.value(QStringLiteral("reason")).toString()));
        QTRY_VERIFY(core.model->isCoreOnAir());

        // On the air: locked with its reason for both version 22 peers.
        for (const QString& id : rxIds) {
            QTRY_COMPARE(rowLock(listenerApp, id), QJsonValue(locked));
            QTRY_COMPARE(rowLock(keyerApp, id), QJsonValue(locked));
        }
        QCOMPARE(dspOf(olderApp), olderDsp);
        listenerApp->sendText(SessionMessages::encode(SessionMessages::settingsWrite(
            key, QStringLiteral("512"), QStringLiteral("phone"))));
        QTRY_COMPARE(rejectReason(listenerApp, key), RadioModel::dspBufferOnAirLockedReason());
        QCOMPARE(core.settings->value(key).toString(), QStringLiteral("256"));

        const QJsonObject unkeyed = core.invoke(keyerApp, "tx.unkey", {int64("epoch", 1)});
        QVERIFY2(unkeyed.value(QStringLiteral("accepted")).toBool(),
                 qPrintable(unkeyed.value(QStringLiteral("reason")).toString()));
        QTRY_VERIFY(!core.model->isCoreOnAir());
        for (const QString& id : rxIds) {
            QTRY_COMPARE(rowLock(listenerApp, id), QJsonValue(QJsonValue::Undefined));
        }
        QCOMPARE(dspOf(olderApp), olderDsp);
        // Back on receive the write is taken again.
        listenerApp->sendText(SessionMessages::encode(SessionMessages::settingsWrite(
            key, QStringLiteral("512"), QStringLiteral("phone"))));
        QTRY_COMPARE(core.settings->value(key).toString(), QStringLiteral("512"));
    }

    // The same lock follows TUNE and the two-tone test, the Core's other
    // ways onto the air (isCoreOnAir), both edges: locked with its reason
    // and the write refused while keyed, open and taken again after.
    void onAirRxBufferSizeLockFollowsTuneAndTwoTone_data()
    {
        QTest::addColumn<bool>("twoToneKey");
        QTest::newRow("tune") << false;
        QTest::newRow("two-tone") << true;
    }

    void onAirRxBufferSizeLockFollowsTuneAndTwoTone()
    {
        QFETCH(bool, twoToneKey);
        Core core(true);
        Device listener(QStringLiteral("Buffer edge iPad"), QStringLiteral("tablet"));
        core.pair(listener);
        QHash<QByteArray, int> receiveOnly = kHolder;
        receiveOnly.insert("setupDescription", 22);
        auto* app = core.signIn(listener, receiveOnly);
        QVERIFY(admitted(app));

        const QString id = QStringLiteral("dsp.options.DspOptionsBufferSizeCwRx");
        const QString key = QStringLiteral("DspOptionsBufferSizeCwRx");
        const auto rowLock = [app, &id]() {
            const QJsonObject dsp = QJsonDocument::fromJson(
                latest(app->received(), QStringLiteral("setup"), QStringLiteral("dsp"))
                    .toString().toUtf8()).object();
            for (const QJsonValue& page : dsp.value("pages").toArray()) {
                for (const QJsonValue& section : page.toObject().value("sections").toArray()) {
                    for (const QJsonValue& raw : section.toObject().value("controls").toArray()) {
                        if (raw.toObject().value("id") == QJsonValue(id)) {
                            return raw.toObject().value("availability");
                        }
                    }
                }
            }
            return QJsonValue(QStringLiteral("no row"));
        };
        const auto rejectReason = [app, &key]() {
            QString reason;
            for (const QByteArray& wire : app->received()) {
                SessionMessage message;
                if (SessionMessages::decode(wire, &message)
                    && message.kind == SessionMessageKind::SettingsReject
                    && QString::fromUtf8(message.objectKey) == key) {
                    reason = message.reason;
                }
            }
            return reason;
        };
        const QJsonObject locked{{"enabled", false},
                                 {"reason", RadioModel::dspBufferOnAirLockedReason()}};
        QCOMPARE(rowLock(), QJsonValue(QJsonValue::Undefined));

        // State only: no radio, no RF.
        TxChannel tx(/*channelId=*/1);
        TwoToneController* const twoTone = core.model->twoToneController();
        QVERIFY(twoTone);
        twoTone->setTxChannel(&tx);
        twoTone->setSettleDelaysMs(0, 0);
        core.model->moxController()->setMoxCheck({});
        const auto setKeyed = [&](bool on) {
            if (twoToneKey) {
                twoTone->setActive(on);
                QTRY_COMPARE(twoTone->isActive(), on);
            } else {
                core.model->transmitModel().setTune(on);
            }
        };

        setKeyed(true);
        QTRY_VERIFY(core.model->isCoreOnAir());
        QTRY_COMPARE(rowLock(), QJsonValue(locked));
        app->sendText(SessionMessages::encode(SessionMessages::settingsWrite(
            key, QStringLiteral("512"), QStringLiteral("cw"))));
        QTRY_COMPARE(rejectReason(), RadioModel::dspBufferOnAirLockedReason());
        QVERIFY(core.settings->value(key).toString() != QStringLiteral("512"));

        setKeyed(false);
        QTRY_VERIFY(!core.model->isCoreOnAir());
        QTRY_VERIFY(!core.model->stationOnAirRefusal(nullptr));
        QTRY_COMPARE(rowLock(), QJsonValue(QJsonValue::Undefined));
        app->sendText(SessionMessages::encode(SessionMessages::settingsWrite(
            key, QStringLiteral("512"), QStringLiteral("cw"))));
        QTRY_COMPARE(core.settings->value(key).toString(), QStringLiteral("512"));
        twoTone->setTxChannel(nullptr);
    }

    void calibrationWritesOutsideTheirRangeAreRefusedWhole()
    {
        Core core;
        const QString mac = core.model->currentRadioInfo().macAddress;
        core.model->setReceiveOnlyStationPolicy(true);
        Device phone(QStringLiteral("Calibration range iPhone"), QStringLiteral("phone"));
        core.pair(phone);
        QHash<QByteArray, int> features = kHolder;
        features.insert("setupDescription", 13);
        auto* app = core.signIn(phone, features);
        QVERIFY(admitted(app));
        const auto key = [&mac](const QString& rest) {
            return QStringLiteral("hardware/%1/%2").arg(mac, rest);
        };
        const auto rejectReason = [app](const QString& k) {
            QString reason;
            for (const QByteArray& wire : app->received()) {
                SessionMessage message;
                if (SessionMessages::decode(wire, &message)
                    && message.kind == SessionMessageKind::SettingsReject
                    && QString::fromUtf8(message.objectKey) == k) {
                    reason = message.reason;
                }
            }
            return reason;
        };
        const auto write = [app](const QString& k, const QString& value) {
            app->sendText(SessionMessages::encode(
                SessionMessages::settingsWrite(k, value, QStringLiteral("phone"))));
        };
        struct Case { QString rest; QString bad; QString good; QString reason; };
        // The Core's radio is an HL2: the ANAN-10 class table (point 3 up
        // to 10 W, point 10 up to 12 W).
        const QList<Case> cases{
            {"paCalibration/calPoint3", "10.5", "9.5", "Choose a calibration point from 0 to 10 W."},
            {"paCalibration/calPoint10", "12.5", "11.9", "Choose a calibration point from 0 to 12 W."},
            {"paCalibration/calPoint1", "-1", "0.5", "Choose a calibration point from 0 to 10 W."},
            {"paCalibration/calPoint2", "plenty", "2", "Choose a calibration point from 0 to 10 W."},
            {"paCalibration/boardClass", "2", "1", "The Core expected this radio's power calibration table."},
            {"cal/txDisplayOffset", "100.5", "-99.5", "Choose a TX display offset from -100 to 100 dB."},
            {"paCalibration/cal/txDisplayOffset", "-101", "5", "Choose a TX display offset from -100 to 100 dB."},
            // Thetis's boxes hold 0 to 65 (lead's ruling: Thetis's range).
            {"cal/freqFactor", "65.5", "2.5", "Choose a correction factor from 0 to 65."},
            {"cal/freqFactor10M", "-0.1", "0.9999999", "Choose a correction factor from 0 to 65."},
            {"cal/using10M", "yes", "True", "The Core expected this box to be on or off."},
            // The Calibration tab's own copies hold a stored bool, which
            // reaches the Core as "true" or "false".
            {"paCalibration/cal/using10M", "on", "true", "The Core expected this box to be on or off."},
            {"paCalibration/cal/logVoltsAmps", "1", "false", "The Core expected this box to be on or off."},
            {"cal/rx1_6mLna", "26", "13", "Choose a 6 m LNA offset from 0 to 25 dB."},
            {"cal/rx2_6mLna", "-1", "0", "Choose a 6 m LNA offset from 0 to 25 dB."},
            {"cal/paSens", "0", "120", "Choose an amp sensitivity from 0.001 to 5000."},
            {"cal/paOffset", "5001", "360", "Choose an amp voltage offset from 0 to 5000."},
        };
        for (const Case& c : cases) {
            write(key(c.rest), c.bad);
            QTRY_COMPARE_WITH_TIMEOUT(rejectReason(key(c.rest)), c.reason, 5000);
            QVERIFY2(core.settings->value(key(c.rest)).toString() != c.bad, qPrintable(c.rest));
            write(key(c.rest), c.good);
            QTRY_COMPARE_WITH_TIMEOUT(core.settings->value(key(c.rest)).toString(), c.good, 5000);
        }
    }

    void pairedV15PublishesTheRestAndKeepsV14Projection()
    {
        Core core;
        Device current(QStringLiteral("Setup V15 iPhone"), QStringLiteral("phone"));
        Device older(QStringLiteral("Setup V14 iPhone"), QStringLiteral("phone"));
        core.pair(current);
        core.pair(older);
        QHash<QByteArray, int> v15Features = kHolder;
        v15Features.insert("setupDescription", 15);
        auto* v15 = core.signIn(current, v15Features);
        QVERIFY(admitted(v15));
        QCOMPARE(capability(v15->received(), QStringLiteral("setupDescriptionVersion")),
                 std::optional<qint64>(15));
        const auto category = [](LoopbackTransport* app, const QString& name) {
            return QJsonDocument::fromJson(latest(app->received(), QStringLiteral("setup"), name)
                                               .toString().toUtf8()).object();
        };
        const auto pageIds = [](const QJsonObject& root) {
            QStringList ids;
            for (const QJsonValue& page : root.value("pages").toArray()) {
                ids << page.toObject().value("id").toString();
            }
            return ids;
        };
        // Every version 15 row the Core sends passes the Core's own check.
        for (const QString& name : {QStringLiteral("dsp"), QStringLiteral("transmit"),
                                    QStringLiteral("audio"), QStringLiteral("diagnostics"),
                                    QStringLiteral("catNetwork")}) {
            const QJsonObject root = category(v15, name);
            QCOMPARE(root.value("version"), QJsonValue(15));
            for (const QJsonValue& page : root.value("pages").toArray()) {
                for (const QJsonValue& section : page.toObject().value("sections").toArray()) {
                    QString why;
                    QVERIFY2(SetupDescriptionV15::validateSection(
                                 name, section.toObject().value("controls").toArray(), &why),
                             qPrintable(name + ": " + why));
                }
            }
        }
        QCOMPARE(pageIds(category(v15, "dsp")).size(), 10);
        QCOMPARE(pageIds(category(v15, "transmit")),
                 (QStringList{"transmit.power", "transmit.speechProcessor", "transmit.dexpVox"}));
        // TX Input and TX Profile (the radio mic rows follow the board).
        QCOMPARE(pageIds(category(v15, "audio")), (QStringList{"audio.txInput", "audio.txProfile"}));
        QCOMPARE(pageIds(category(v15, "diagnostics")),
                 (QStringList{"diagnostics.radioStatus", "diagnostics.connectionQuality",
                              "diagnostics.settingsValidation"}));

        // Rows reach the Core through its existing bindings: the APF centre
        // is the selected slice's tune offset, a Filter Presets row is the
        // Core's three filters/ settings.
        SliceModel* slice = core.model->sliceById(0);
        QVERIFY(slice != nullptr);
        v15->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
            "slice:0", {MirrorUpdate{0, "apfTuneHz", MirrorWireKind::Int64, qint64(50)}}, 1401)));
        QTRY_COMPARE(slice->apfTuneHz(), 50);
        for (const auto& [key, value] : {std::pair{QStringLiteral("filters/USB/0/name"), QStringLiteral("Wide")},
                                         std::pair{QStringLiteral("filters/USB/0/low"), QStringLiteral("100")},
                                         std::pair{QStringLiteral("filters/USB/0/high"), QStringLiteral("3100")}}) {
            v15->sendText(SessionMessages::encode(SessionMessages::settingsWrite(key, value, QStringLiteral("presets"))));
            QTRY_COMPARE(core.settings->value(key).toString(), value);
        }

        QHash<QByteArray, int> v14Features = kHolder;
        v14Features.insert("setupDescription", 14);
        auto* v14 = core.signIn(older, v14Features);
        QVERIFY(admitted(v14));
        QCOMPARE(capability(v14->received(), QStringLiteral("setupDescriptionVersion")),
                 std::optional<qint64>(14));
        const QJsonObject oldDsp = category(v14, "dsp");
        QCOMPARE(oldDsp.value("version"), QJsonValue(3));
        QCOMPARE(pageIds(oldDsp).size(), 9);
        QCOMPARE(pageIds(category(v14, "transmit")), (QStringList{"transmit.power", "transmit.dexpVox"}));
        QCOMPARE(pageIds(category(v14, "audio")), (QStringList{"audio.txProfile"}));
        QCOMPARE(pageIds(category(v14, "diagnostics")), (QStringList{"diagnostics.settingsValidation"}));
        for (const QString& name : {QStringLiteral("dsp"), QStringLiteral("transmit"),
                                    QStringLiteral("audio"), QStringLiteral("diagnostics"),
                                    QStringLiteral("catNetwork")}) {
            const QString text = latest(v14->received(), QStringLiteral("setup"), name).toString();
            QVERIFY2(!text.contains(QStringLiteral("\"requiresDescriptionVersion\":15")), qPrintable(name));
            QVERIFY2(!text.contains(QStringLiteral("coverageV15")), qPrintable(name));
            QVERIFY2(!text.contains(QStringLiteral("\"requiresDescriptionVersion\":19")), qPrintable(name));
            QVERIFY2(!text.contains(QStringLiteral("coverageV19")), qPrintable(name));
        }
    }

    // Version 19: a paired phone reads DSP > CFC's band editor; a version 18
    // phone keeps DSP version 15 without it.
    void pairedV19PhoneReadsCfcBands()
    {
        Core core;
        const auto cfcBandsOf = [](const QJsonObject& dsp) {
            for (const QJsonValue& page : dsp.value("pages").toArray()) {
                for (const QJsonValue& section : page.toObject().value("sections").toArray()) {
                    for (const QJsonValue& raw : section.toObject().value("controls").toArray()) {
                        if (raw.toObject().value("id") == QJsonValue("dsp.cfc.bands")) {
                            return raw.toObject();
                        }
                    }
                }
            }
            return QJsonObject();
        };
        // {declared, capability sent back, DSP version the phone reads}
        const QList<std::tuple<int, int, int>> declarations{{19, 19, 19}, {18, 18, 15}};
        for (const auto& [declared, granted, received] : declarations) {
            Device phone(QStringLiteral("CFC V%1 iPhone").arg(declared), QStringLiteral("phone"));
            core.pair(phone);
            QHash<QByteArray, int> features = kHolder;
            features.insert("setupDescription", declared);
            auto* peer = core.signIn(phone, features);
            QVERIFY(admitted(peer));
            QCOMPARE(capability(peer->received(), QStringLiteral("setupDescriptionVersion")),
                     std::optional<qint64>(granted));
            const QString text = latest(peer->received(), QStringLiteral("setup"),
                                        QStringLiteral("dsp")).toString();
            QVERIFY(!text.contains(QStringLiteral("coverageV19")));
            const QJsonObject dsp = QJsonDocument::fromJson(text.toUtf8()).object();
            QCOMPARE(dsp.value("version"), QJsonValue(received));
            const QJsonObject bands = cfcBandsOf(dsp);
            QCOMPARE(bands.isEmpty(), received != 19);
            if (received == 19) {
                QVERIFY(SetupDescriptionService::validateDspV19Control(bands));
                // Published with no off-air rule: the Core takes it on the air.
                QCOMPARE(bands.value("gate"), QJsonValue(QJsonObject{
                    {"capability", "transmitSettingsVersion"}, {"min", 15}}));
                QCOMPARE(dsp.value("coverage"),
                         QJsonValue("partial: the NR3 and NNR model files are not described"));
            }
        }
    }

    void pairedV12PublishesTheRestOfDisplayAndKeepsV11Projection()
    {
        Core core;
        Device current(QStringLiteral("Display V12 iPhone"), QStringLiteral("phone"));
        Device older(QStringLiteral("Display V11 iPhone"), QStringLiteral("phone"));
        core.pair(current);
        core.pair(older);
        QHash<QByteArray, int> v12Features = kHolder;
        v12Features.insert("setupDescription", 12);
        auto* v12 = core.signIn(current, v12Features);
        QVERIFY(admitted(v12));
        QCOMPARE(capability(v12->received(), QStringLiteral("setupDescriptionVersion")),
                 std::optional<qint64>(12));
        const QJsonObject display = QJsonDocument::fromJson(latest(v12->received(),
            QStringLiteral("setup"), QStringLiteral("display")).toString().toUtf8()).object();
        QCOMPARE(display.value("version"), QJsonValue(12));
        QStringList pageIds;
        int described = 0;
        int v12Rows = 0;
        for (const QJsonValue& page : display.value("pages").toArray()) {
            pageIds << page.toObject().value("id").toString();
            for (const QJsonValue& section : page.toObject().value("sections").toArray()) {
                for (const QJsonValue& raw : section.toObject().value("controls").toArray()) {
                    const QJsonObject control = raw.toObject();
                    ++described;
                    if (control.value("requiresDescriptionVersion") == QJsonValue(12)) {
                        QVERIFY2(SetupDescriptionService::validateDisplayPhoneBinding(control),
                                 qPrintable(control.value("id").toString()));
                        ++v12Rows;
                    }
                }
            }
        }
        QCOMPARE(pageIds, (QStringList{"display.spectrumDefaults", "display.spectrumPeaks",
                                       "display.waterfallDefaults", "display.gridScales",
                                       "display.multimeter", "display.txDisplay",
                                       "display.threeD"}));
        QCOMPARE(described, 100);
        QCOMPARE(v12Rows, 52);
        const QJsonObject appearance = QJsonDocument::fromJson(latest(v12->received(),
            QStringLiteral("setup"), QStringLiteral("appearance")).toString().toUtf8()).object();
        QCOMPARE(appearance.value("version"), QJsonValue(12));
        const QJsonArray colourSections = appearance.value("pages").toArray().first().toObject()
            .value("sections").toArray();
        QCOMPARE(colourSections.size(), 2);
        QVERIFY(SetupDescriptionService::validateAppearanceResetColours(
            colourSections.at(1).toObject().value("controls").toArray().first().toObject()));

        QHash<QByteArray, int> v11Features = kHolder;
        v11Features.insert("setupDescription", 11);
        auto* v11 = core.signIn(older, v11Features);
        QVERIFY(admitted(v11));
        QCOMPARE(capability(v11->received(), QStringLiteral("setupDescriptionVersion")),
                 std::optional<qint64>(11));
        const QJsonObject old = QJsonDocument::fromJson(latest(v11->received(),
            QStringLiteral("setup"), QStringLiteral("display")).toString().toUtf8()).object();
        QCOMPARE(old.value("version"), QJsonValue(11));
        QCOMPARE(old.value("pages").toArray().size(), 5);
        QVERIFY(!QJsonDocument(old).toJson(QJsonDocument::Compact).contains("requiresDescriptionVersion\":12"));
        const QJsonObject oldAppearance = QJsonDocument::fromJson(latest(v11->received(),
            QStringLiteral("setup"), QStringLiteral("appearance")).toString().toUtf8()).object();
        QCOMPARE(oldAppearance.value("version"), QJsonValue(7));
        QCOMPARE(oldAppearance.value("pages").toArray().first().toObject()
                     .value("sections").toArray().size(), 1);
    }

    void pairedV11PublishesSpectrumPeaksAndKeepsV10Projection()
    {
        Core core;
        Device current(QStringLiteral("Spectrum peaks V11 iPhone"), QStringLiteral("phone"));
        Device older(QStringLiteral("Spectrum peaks V10 iPhone"), QStringLiteral("phone"));
        core.pair(current);
        core.pair(older);
        QHash<QByteArray, int> v11Features = kHolder;
        v11Features.insert("setupDescription", 11);
        auto* v11 = core.signIn(current, v11Features);
        QVERIFY(admitted(v11));
        QCOMPARE(capability(v11->received(), QStringLiteral("setupDescriptionVersion")),
                 std::optional<qint64>(11));
        const QJsonObject display = QJsonDocument::fromJson(latest(v11->received(),
            QStringLiteral("setup"), QStringLiteral("display")).toString().toUtf8()).object();
        QCOMPARE(display.value("version"), QJsonValue(11));
        const QJsonObject peaks = display.value("pages").toArray().at(1).toObject();
        QCOMPARE(peaks.value("id"), QJsonValue("display.spectrumPeaks"));
        int described = 0;
        for (const QJsonValue& section : peaks.value("sections").toArray()) {
            for (const QJsonValue& raw : section.toObject().value("controls").toArray()) {
                QVERIFY(SetupDescriptionService::validateDisplayPhoneBinding(raw.toObject()));
                ++described;
            }
        }
        QCOMPARE(described, 15);
        QHash<QByteArray, int> v10Features = kHolder;
        v10Features.insert("setupDescription", 10);
        auto* v10 = core.signIn(older, v10Features);
        QVERIFY(admitted(v10));
        QCOMPARE(capability(v10->received(), QStringLiteral("setupDescriptionVersion")),
                 std::optional<qint64>(10));
        const QJsonObject old = QJsonDocument::fromJson(latest(v10->received(),
            QStringLiteral("setup"), QStringLiteral("display")).toString().toUtf8()).object();
        QCOMPARE(old.value("version"), QJsonValue(10));
        QCOMPARE(old.value("pages").toArray().size(), 4);
        QCOMPARE(old.value("pages").toArray().at(1).toObject().value("id"),
                 QJsonValue("display.waterfallDefaults"));
    }

    void pairedV10PublishesWaterfallOverlaysAndKeepsV9Projection()
    {
        Core core;
        Device current(QStringLiteral("Waterfall overlays V10 iPhone"), QStringLiteral("phone"));
        Device older(QStringLiteral("Waterfall overlays V9 iPhone"), QStringLiteral("phone"));
        core.pair(current);
        core.pair(older);
        QHash<QByteArray, int> v10Features = kHolder;
        v10Features.insert("setupDescription", 10);
        auto* v10 = core.signIn(current, v10Features);
        QVERIFY(admitted(v10));
        QCOMPARE(capability(v10->received(), QStringLiteral("setupDescriptionVersion")),
                 std::optional<qint64>(10));
        const QJsonObject display = QJsonDocument::fromJson(latest(v10->received(),
            QStringLiteral("setup"), QStringLiteral("display")).toString().toUtf8()).object();
        QCOMPARE(display.value("version"), QJsonValue(10));
        const QJsonArray overlays = display.value("pages").toArray().at(1).toObject()
            .value("sections").toArray().at(1).toObject().value("controls").toArray();
        QCOMPARE(overlays.size(), 4);
        for (const QJsonValue& raw : overlays) {
            QVERIFY(SetupDescriptionService::validateDisplayPhoneBinding(raw.toObject()));
        }
        QHash<QByteArray, int> v9Features = kHolder;
        v9Features.insert("setupDescription", 9);
        auto* v9 = core.signIn(older, v9Features);
        QVERIFY(admitted(v9));
        QCOMPARE(capability(v9->received(), QStringLiteral("setupDescriptionVersion")),
                 std::optional<qint64>(9));
        const QJsonObject old = QJsonDocument::fromJson(latest(v9->received(),
            QStringLiteral("setup"), QStringLiteral("display")).toString().toUtf8()).object();
        QCOMPARE(old.value("version"), QJsonValue(9));
        QCOMPARE(old.value("pages").toArray().at(1).toObject()
                     .value("sections").toArray().size(), 1);
    }

    void pairedV9PublishesLocalRendererDispatchWithoutChangingV8()
    {
        Core core;
        Device current(QStringLiteral("Renderer V9 iPhone"), QStringLiteral("phone"));
        Device older(QStringLiteral("Renderer V8 iPhone"), QStringLiteral("phone"));
        core.pair(current);
        core.pair(older);
        QHash<QByteArray, int> v9Features = kHolder;
        v9Features.insert("setupDescription", 9);
        auto* v9 = core.signIn(current, v9Features);
        QVERIFY(admitted(v9));
        QCOMPARE(capability(v9->received(), QStringLiteral("setupDescriptionVersion")),
                 std::optional<qint64>(9));
        const QJsonObject display = QJsonDocument::fromJson(latest(v9->received(),
            QStringLiteral("setup"), QStringLiteral("display")).toString().toUtf8()).object();
        QCOMPARE(display.value("version"), QJsonValue(9));
        const QJsonArray pages = display.value("pages").toArray();
        const QJsonArray rendering = pages.at(0).toObject().value("sections").toArray()
            .at(1).toObject().value("controls").toArray();
        const QJsonArray waterfall = pages.at(1).toObject().value("sections").toArray()
            .at(0).toObject().value("controls").toArray();
        QCOMPARE(rendering.size(), 10);
        QCOMPARE(waterfall.size(), 6);
        for (int i = 5; i < rendering.size(); ++i) {
            QVERIFY(SetupDescriptionService::validateDisplayPhoneBinding(rendering.at(i).toObject()));
        }
        for (int i = 3; i < waterfall.size(); ++i) {
            QVERIFY(SetupDescriptionService::validateDisplayPhoneBinding(waterfall.at(i).toObject()));
        }
        QHash<QByteArray, int> v8Features = kHolder;
        v8Features.insert("setupDescription", 8);
        auto* v8 = core.signIn(older, v8Features);
        QVERIFY(admitted(v8));
        QCOMPARE(capability(v8->received(), QStringLiteral("setupDescriptionVersion")),
                 std::optional<qint64>(8));
        const QJsonObject old = QJsonDocument::fromJson(latest(v8->received(),
            QStringLiteral("setup"), QStringLiteral("display")).toString().toUtf8()).object();
        QCOMPARE(old.value("version"), QJsonValue(8));
        QCOMPARE(old.value("pages").toArray().at(0).toObject().value("sections").toArray()
                     .at(1).toObject().value("controls").toArray().size(), 5);
        QCOMPARE(old.value("pages").toArray().at(1).toObject().value("sections").toArray()
                     .at(0).toObject().value("controls").toArray().size(), 3);
    }

    void pairedV8PublishesOnlyClosedPhoneRxDispatch()
    {
        Core core;
        Device current(QStringLiteral("RX display V8 iPhone"), QStringLiteral("phone"));
        Device older(QStringLiteral("RX display V7 iPhone"), QStringLiteral("phone"));
        core.pair(current);
        core.pair(older);
        QHash<QByteArray, int> v8Features = kHolder;
        v8Features.insert("setupDescription", 8);
        auto* v8 = core.signIn(current, v8Features);
        QVERIFY(admitted(v8));
        QCOMPARE(capability(v8->received(), QStringLiteral("setupDescriptionVersion")),
                 std::optional<qint64>(8));
        const QJsonObject display = QJsonDocument::fromJson(latest(v8->received(),
            QStringLiteral("setup"), QStringLiteral("display")).toString().toUtf8()).object();
        QCOMPARE(display.value("version"), QJsonValue(8));
        const QJsonArray pages = display.value("pages").toArray();
        QCOMPARE(pages.size(), 4);
        const QJsonArray rendering = pages.at(0).toObject().value("sections").toArray()
            .at(1).toObject().value("controls").toArray();
        QCOMPARE(rendering.size(), 5);
        const QJsonArray waterfall = pages.at(1).toObject().value("sections").toArray()
            .at(0).toObject().value("controls").toArray();
        QCOMPARE(waterfall.size(), 3);
        for (int i = 1; i < rendering.size(); ++i) {
            QVERIFY(SetupDescriptionService::validateDisplayPhoneBinding(rendering.at(i).toObject()));
        }
        for (const QJsonValue& raw : waterfall) {
            QVERIFY(SetupDescriptionService::validateDisplayPhoneBinding(raw.toObject()));
        }
        QHash<QByteArray, int> v7Features = kHolder;
        v7Features.insert("setupDescription", 7);
        auto* v7 = core.signIn(older, v7Features);
        QVERIFY(admitted(v7));
        QCOMPARE(capability(v7->received(), QStringLiteral("setupDescriptionVersion")),
                 std::optional<qint64>(7));
        const QJsonObject oldDisplay = QJsonDocument::fromJson(latest(v7->received(),
            QStringLiteral("setup"), QStringLiteral("display")).toString().toUtf8()).object();
        QCOMPARE(oldDisplay.value("version"), QJsonValue(4));
        QCOMPARE(oldDisplay.value("pages").toArray().size(), 3);
        QCOMPARE(oldDisplay.value("pages").toArray().first().toObject()
                     .value("sections").toArray().at(1).toObject()
                     .value("controls").toArray().size(), 1);
    }

    void pairedV7PublishesOnlyPhoneOwnedMeterStyles()
    {
        Core core;
        Device current(QStringLiteral("Meter styles iPhone"), QStringLiteral("phone"));
        Device older(QStringLiteral("Older meter styles iPhone"), QStringLiteral("phone"));
        core.pair(current);
        core.pair(older);
        QHash<QByteArray, int> v7Features = kHolder;
        v7Features.insert("setupDescription", 7);
        auto* v7 = core.signIn(current, v7Features);
        QVERIFY(admitted(v7));
        QCOMPARE(capability(v7->received(), QStringLiteral("setupDescriptionVersion")),
                 std::optional<qint64>(7));
        const QJsonObject appearance = QJsonDocument::fromJson(latest(v7->received(),
            QStringLiteral("setup"), QStringLiteral("appearance")).toString().toUtf8()).object();
        QCOMPARE(appearance.value("version"), QJsonValue(7));
        const QJsonArray pages = appearance.value("pages").toArray();
        QCOMPARE(pages.size(), 2);
        const QJsonArray styles = pages.at(1).toObject().value("sections").toArray()
            .first().toObject().value("controls").toArray();
        QCOMPARE(styles.size(), 3);
        for (const QJsonValue& raw : styles) {
            const QJsonObject control = raw.toObject();
            QVERIFY(SetupDescriptionService::validateAppearanceMeterStyleBinding(control));
            QCOMPARE(control.value("binding").toObject().size(), 1);
            QVERIFY(control.value("binding").toObject().contains("phone"));
            QVERIFY(!control.contains("gate"));
            QVERIFY(!control.contains("valueEncoding"));
        }
        QHash<QByteArray, int> v6Features = kHolder;
        v6Features.insert("setupDescription", 6);
        auto* v6 = core.signIn(older, v6Features);
        QVERIFY(admitted(v6));
        QCOMPARE(capability(v6->received(), QStringLiteral("setupDescriptionVersion")),
                 std::optional<qint64>(6));
        const QJsonObject olderAppearance = QJsonDocument::fromJson(latest(v6->received(),
            QStringLiteral("setup"), QStringLiteral("appearance")).toString().toUtf8()).object();
        QCOMPARE(olderAppearance.value("version"), QJsonValue(4));
        QCOMPARE(olderAppearance.value("pages").toArray().size(), 1);
        QCOMPARE(olderAppearance.value("pages").toArray().first().toObject()
                     .value("sections").toArray().first().toObject()
                     .value("controls").toArray().size(), 10);
    }

    void pairedV6RowsStayDescribedWhileRadioConnectsWithoutReconnect()
    {
        Core core;
        StepAttenuatorController step;
        step.setTickTimerEnabled(false);
        core.model->setStepAttController(&step);
        core.model->setBoardForTest(HPSDRHW::Hermes);
        RadioInfo info = core.model->currentRadioInfo();
        info.boardType = HPSDRHW::Hermes;
        core.model->setLastRadioInfoForTest(info);
        core.model->setConnectionStateForTest(ConnectionState::Disconnected);
        // As StationServer does: the context carries the Core's radio, which
        // version 13's Radio Info describes (the same radio when it connects).
        core.server->setupDescription()->setRadioContext(core.model->boardCapabilities(),
                                                         core.model->hardwareProfile().model,
                                                         core.model->currentRadioInfo());
        const quint32 unchangedDescriptionRevision = core.server->setupDescription()->revision();
        const QString unchangedDescription = core.server->setupDescription()->hardware();
        QVERIFY(!unchangedDescription.isEmpty());
        QVERIFY(core.model->currentRadioMac().isEmpty());

        Device phone(QStringLiteral("Late-radio antenna iPhone"), QStringLiteral("phone"));
        core.pair(phone);
        QHash<QByteArray, int> features = kHolder;
        features.insert("setupDescription", 6);
        features.insert("radioAntennaRows", 1);
        // A phone that knows 2 m (R-IOS-26): the 15 rows the Core validates.
        features.insert("band2m", 1);
        auto* app = core.signIn(phone, features);
        QVERIFY(admitted(app));
        const auto hasRows = [app] {
            const QString hardware = latest(app->received(), QStringLiteral("setup"),
                                            QStringLiteral("hardware")).toString();
            return hardware.contains(QStringLiteral("hardware.antenna.txRows"))
                && hardware.contains(QStringLiteral("hardware.antenna.rxRows"));
        };
        const auto latestRowsCapability = [app] {
            const QList<QJsonObject> messages = ofType(app->received(),
                                                        QStringLiteral("capabilities"));
            for (auto it = messages.crbegin(); it != messages.crend(); ++it) {
                for (const QJsonValue& raw : it->value(QStringLiteral("properties")).toArray()) {
                    const QJsonObject property = raw.toObject();
                    if (property.value(QStringLiteral("name"))
                        == QJsonValue(QStringLiteral("radioAntennaRowsVersion"))) {
                        return property.value(QStringLiteral("value")).toInteger();
                    }
                }
                return qint64(0);
            }
            return qint64(0);
        };
        QCOMPARE(latestRowsCapability(), qint64(0));
        QVERIFY(hasRows());

        const QJsonObject hardware = QJsonDocument::fromJson(latest(app->received(),
            QStringLiteral("setup"), QStringLiteral("hardware")).toString().toUtf8()).object();
        const QJsonArray controls = hardware.value("pages").toArray().first().toObject()
            .value("sections").toArray().first().toObject().value("controls").toArray();
        const QJsonObject rx = controls.last().toObject();
        QVERIFY(SetupDescriptionService::validateAntennaRowsTable(rx,
            core.model->hardwareProfile().model));
        const QJsonObject row = rx.value("rows").toArray().at(3).toObject();
        const QJsonObject column = rx.value("columns").toArray().at(1).toObject();
        QCOMPARE(row.value("label"), QJsonValue("40m"));
        QCOMPARE(column.value("antenna"), QJsonValue(2));

        core.model->setConnectionStateForTest(ConnectionState::Connected);
        core.model->currentRadioChanged(core.model->currentRadioInfo());
        QTRY_COMPARE(latestRowsCapability(), qint64(1));
        QCOMPARE(core.server->setupDescription()->revision(), unchangedDescriptionRevision);
        QCOMPARE(core.server->setupDescription()->hardware(), unchangedDescription);
        QVERIFY(hasRows());
        const QString mac = core.model->currentRadioMac();
        QVERIFY(!mac.isEmpty());
        const QJsonObject result = core.invoke(app, "setAlexRxAntennaForRadio",
            {MirrorUpdate{0, "mac", MirrorWireKind::Utf8, mac},
             MirrorUpdate{0, "band", MirrorWireKind::Int64,
                          qlonglong(row.value("band").toInt())},
             MirrorUpdate{0, "antenna", MirrorWireKind::Int64,
                          qlonglong(column.value("antenna").toInt())},
             MirrorUpdate{0, "rxOnly", MirrorWireKind::Bool, false}});
        QVERIFY2(result.value("accepted").toBool(),
                 qPrintable(result.value("reason").toString()));
        QCOMPARE(core.model->alexController().rxAnt(Band::Band40m), 2);
        QCOMPARE(core.model->alexController().rxAnt(Band::Band20m), 1);
    }

    void pairedV6DisconnectRetainsInertTableUntilSameRadioReturns()
    {
        Core core;
        StepAttenuatorController step;
        step.setTickTimerEnabled(false);
        core.model->setStepAttController(&step);
        core.model->setBoardForTest(HPSDRHW::Hermes);
        core.server->setupDescription()->setRadioContext(core.model->boardCapabilities(),
                                                         core.model->hardwareProfile().model);
        Device phone(QStringLiteral("Reconnect antenna iPhone"), QStringLiteral("phone"));
        core.pair(phone);
        QHash<QByteArray, int> features = kHolder;
        features.insert("setupDescription", 6);
        features.insert("radioAntennaRows", 1);
        auto* app = core.signIn(phone, features);
        QVERIFY(admitted(app));
        QCOMPARE(capability(app->received(), QStringLiteral("radioAntennaRowsVersion")),
                 std::optional<qint64>(1));
        const QString tableDescription = latest(app->received(), QStringLiteral("setup"),
                                                 QStringLiteral("hardware")).toString();
        QVERIFY(tableDescription.contains(QStringLiteral("hardware.antenna.txRows")));
        // This phone did not declare band2m (R-IOS-26): it is sent the 14
        // rows it was built for, without 2 m (station link section 6.1).
        {
            const QJsonArray controls = QJsonDocument::fromJson(tableDescription.toUtf8())
                .object().value("pages").toArray().first().toObject()
                .value("sections").toArray().first().toObject().value("controls").toArray();
            const QJsonArray rows = controls.last().toObject().value("rows").toArray();
            QCOMPARE(rows.size(), 14);
            QCOMPARE(rows.last().toObject().value("band"), QJsonValue(13));
        }
        const int capabilityCount = ofType(app->received(), QStringLiteral("capabilities")).size();
        core.model->setConnectionStateForTest(ConnectionState::Disconnected);
        core.model->currentRadioChanged(core.model->currentRadioInfo());
        QTRY_VERIFY(ofType(app->received(), QStringLiteral("capabilities")).size()
                    > capabilityCount);
        const QJsonObject withdrawn = ofType(app->received(), QStringLiteral("capabilities")).last();
        for (const QJsonValue& raw : withdrawn.value(QStringLiteral("properties")).toArray()) {
            QVERIFY(raw.toObject().value(QStringLiteral("name"))
                    != QJsonValue(QStringLiteral("radioAntennaRowsVersion")));
        }
        QCOMPARE(latest(app->received(), QStringLiteral("setup"),
                        QStringLiteral("hardware")).toString(), tableDescription);
        core.model->setConnectionStateForTest(ConnectionState::Connected);
        core.model->currentRadioChanged(core.model->currentRadioInfo());
        const auto rowCapRestored = [app] {
            const QList<QJsonObject> messages = ofType(app->received(),
                                                        QStringLiteral("capabilities"));
            if (messages.isEmpty()) { return false; }
            for (const QJsonValue& raw : messages.last().value(QStringLiteral("properties")).toArray()) {
                if (raw.toObject().value(QStringLiteral("name"))
                        == QJsonValue(QStringLiteral("radioAntennaRowsVersion"))
                    && raw.toObject().value(QStringLiteral("value")) == QJsonValue(1)) {
                    return true;
                }
            }
            return false;
        };
        QTRY_VERIFY(rowCapRestored());
        QCOMPARE(latest(app->received(), QStringLiteral("setup"),
                        QStringLiteral("hardware")).toString(), tableDescription);
    }

    void pairedV6AntennaTableRowsUseCurrentRadioCommands()
    {
        Core core;
        StepAttenuatorController step;
        step.setTickTimerEnabled(false);
        core.model->setStepAttController(&step);
        core.model->setBoardForTest(HPSDRHW::Hermes);
        core.server->setupDescription()->setRadioContext(core.model->boardCapabilities(),
                                                         core.model->hardwareProfile().model);
        Device a(QStringLiteral("Antenna table iPhone A"), QStringLiteral("phone"));
        Device b(QStringLiteral("Antenna table iPhone B"), QStringLiteral("phone"));
        core.pair(a);
        core.pair(b);
        QHash<QByteArray, int> features = kHolder;
        features.insert("setupDescription", 6);
        features.insert("radioAntennaRows", 1);
        // A phone that knows 2 m (R-IOS-26): the 15 rows the Core validates.
        features.insert("band2m", 1);
        auto* first = core.signIn(a, features);
        QVERIFY(admitted(first));
        QCOMPARE(capability(first->received(), QStringLiteral("setupDescriptionVersion")),
                 std::optional<qint64>(6));
        QCOMPARE(capability(first->received(), QStringLiteral("radioAntennaRowsVersion")),
                 std::optional<qint64>(1));
        const QJsonObject hardware = QJsonDocument::fromJson(latest(first->received(),
            QStringLiteral("setup"), QStringLiteral("hardware")).toString().toUtf8()).object();
        QCOMPARE(hardware.value("version"), QJsonValue(6));
        const QJsonArray controls = hardware.value("pages").toArray().first().toObject()
            .value("sections").toArray().first().toObject().value("controls").toArray();
        const QJsonObject tx = controls.at(controls.size() - 2).toObject();
        const QJsonObject rx = controls.last().toObject();
        QVERIFY(SetupDescriptionService::validateAntennaRowsTable(tx,
            core.model->hardwareProfile().model));
        QVERIFY(SetupDescriptionService::validateAntennaRowsTable(rx,
            core.model->hardwareProfile().model));
        const QString mac = core.model->currentRadioMac();
        QVERIFY(!mac.isEmpty());
        const QJsonObject row = rx.value("rows").toArray().at(3).toObject();
        const int band = row.value("band").toInt();
        const QJsonObject rxColumn = rx.value("columns").toArray().at(1).toObject();
        const QJsonObject onlyColumn = rx.value("columns").toArray().at(4).toObject();
        QCOMPARE(rxColumn.value("field"), QJsonValue("rx"));
        QCOMPARE(onlyColumn.value("field"), QJsonValue("rxOnly"));
        const auto rxArgs = [&mac, band](const QJsonObject& column) {
            return QList<MirrorUpdate>{
                MirrorUpdate{0, "mac", MirrorWireKind::Utf8, mac},
                MirrorUpdate{0, "band", MirrorWireKind::Int64, qlonglong(band)},
                MirrorUpdate{0, "antenna", MirrorWireKind::Int64,
                             qlonglong(column.value("antenna").toInt())},
                MirrorUpdate{0, "rxOnly", MirrorWireKind::Bool,
                             column.value("field") == QJsonValue("rxOnly")}};
        };
        const QJsonObject rxResult = core.invoke(first, "setAlexRxAntennaForRadio",
                                                 rxArgs(rxColumn));
        QVERIFY2(rxResult.value("accepted").toBool(),
                 qPrintable(rxResult.value("reason").toString()));
        QCOMPARE(core.model->alexController().rxAnt(Band::Band40m), 2);
        QCOMPARE(core.model->alexController().rxAnt(Band::Band20m), 1);
        QVERIFY(core.invoke(first, "setAlexRxAntennaForRadio", rxArgs(onlyColumn))
                    .value("accepted").toBool());
        QCOMPARE(core.model->alexController().rxOnlyAnt(Band::Band40m), 2);
        const QJsonObject txColumn = tx.value("columns").toArray().at(2).toObject();
        const auto txArgs = [&mac, band](const QJsonObject& column) {
            return QList<MirrorUpdate>{
                MirrorUpdate{0, "mac", MirrorWireKind::Utf8, mac},
                MirrorUpdate{0, "band", MirrorWireKind::Int64, qlonglong(band)},
                MirrorUpdate{0, "antenna", MirrorWireKind::Int64,
                             qlonglong(column.value("antenna").toInt())}};
        };
        QVERIFY(core.invoke(first, "setAlexTxAntennaForRadio", txArgs(txColumn))
                    .value("accepted").toBool());
        QCOMPARE(core.model->alexController().txAnt(Band::Band40m), 3);
        QCOMPARE(core.model->alexController().txAnt(Band::Band20m), 1);
        auto* second = core.signIn(b, features);
        QVERIFY(admitted(second));
        QCOMPARE(capability(second->received(), QStringLiteral("radioAntennaRowsVersion")),
                 std::optional<qint64>(1));
        QTRY_COMPARE(latest(second->received(), QStringLiteral("alexAntennas"),
                            QStringLiteral("rxAntennas")).toString(),
                     core.model->alexAntennaFacade()->rxAntennas());
        QTRY_COMPARE(latest(second->received(), QStringLiteral("alexAntennas"),
                            QStringLiteral("rxOnlyAntennas")).toString(),
                     core.model->alexAntennaFacade()->rxOnlyAntennas());
        QTRY_COMPARE(latest(second->received(), QStringLiteral("alexAntennas"),
                            QStringLiteral("txAntennas")).toString(),
                     core.model->alexAntennaFacade()->txAntennas());
        // A gesture derived from the published row and column while both
        // paired devices are present follows the real shared-confirmation
        // route, then reaches the already-connected second device's mirror.
        const QJsonObject liveRow = rx.value("rows").toArray().at(5).toObject();
        const QJsonObject liveColumn = rx.value("columns").toArray().at(2).toObject();
        QCOMPARE(liveRow.value("label"), QJsonValue("20m"));
        QCOMPARE(liveColumn.value("field"), QJsonValue("rx"));
        const int asksBefore = ofType(first->received(), QStringLiteral("confirm.request")).size();
        const QJsonObject pending = core.invoke(first, "setAlexRxAntennaForRadio",
            {MirrorUpdate{0, "mac", MirrorWireKind::Utf8, mac},
             MirrorUpdate{0, "band", MirrorWireKind::Int64,
                          qlonglong(liveRow.value("band").toInt())},
             MirrorUpdate{0, "antenna", MirrorWireKind::Int64,
                          qlonglong(liveColumn.value("antenna").toInt())},
             MirrorUpdate{0, "rxOnly", MirrorWireKind::Bool, false}});
        QVERIFY(!pending.value("accepted").toBool(true));
        QTRY_VERIFY(ofType(first->received(), QStringLiteral("confirm.request")).size()
                    > asksBefore);
        const QJsonObject question =
            ofType(first->received(), QStringLiteral("confirm.request")).last();
        const QJsonObject confirmed = core.invoke(first, "confirm.proceed",
            {MirrorUpdate{0, "id", MirrorWireKind::Int64,
                          qlonglong(question.value("id").toInteger())},
             MirrorUpdate{0, "choice", MirrorWireKind::Int64, qlonglong(-1)}});
        QVERIFY2(confirmed.value("accepted").toBool(),
                 qPrintable(confirmed.value("reason").toString()));
        QCOMPARE(core.model->alexController().rxAnt(Band::Band20m), 3);
        QCOMPARE(core.model->alexController().rxAnt(Band::Band40m), 2);
        QTRY_COMPARE(latest(second->received(), QStringLiteral("alexAntennas"),
                            QStringLiteral("rxAntennas")).toString(),
                     core.model->alexAntennaFacade()->rxAntennas());
        core.model->alexControllerMutable().setBlockTxAnt3(true);
        QVERIFY(!core.invoke(first, "setAlexTxAntennaForRadio", txArgs(txColumn))
                     .value("accepted").toBool(true));
        QCOMPARE(core.model->alexController().txAnt(Band::Band40m), 1);
        QList<MirrorUpdate> wrong = rxArgs(rxColumn);
        wrong[0].value = QStringLiteral("AA:BB:CC:DD:EE:99");
        QVERIFY(!core.invoke(first, "setAlexRxAntennaForRadio", wrong)
                     .value("accepted").toBool(true));
        Device withoutRows(QStringLiteral("No row feature"), QStringLiteral("phone"));
        core.pair(withoutRows);
        QHash<QByteArray, int> noRows = kHolder;
        noRows.insert("setupDescription", 6);
        auto* older = core.signIn(withoutRows, noRows);
        QVERIFY(admitted(older));
        const QString noRowsHardware = latest(older->received(), QStringLiteral("setup"),
                                               QStringLiteral("hardware")).toString();
        QVERIFY(!noRowsHardware.isEmpty());
        QVERIFY(!noRowsHardware.contains(QStringLiteral("hardware.antenna.txRows")));
        QVERIFY(!noRowsHardware.contains(QStringLiteral("hardware.antenna.rxRows")));

        const int capabilitiesBefore = ofType(first->received(), QStringLiteral("capabilities")).size();
        core.model->setConnectionStateForTest(ConnectionState::Disconnected);
        QTRY_VERIFY(ofType(first->received(), QStringLiteral("capabilities")).size()
                    > capabilitiesBefore);
        const QJsonObject withdrawn = ofType(first->received(), QStringLiteral("capabilities")).last();
        for (const QJsonValue& value : withdrawn.value(QStringLiteral("properties")).toArray()) {
            QVERIFY(value.toObject().value(QStringLiteral("name"))
                    != QJsonValue(QStringLiteral("radioAntennaRowsVersion")));
        }
        QVERIFY(!core.invoke(first, "setAlexRxAntennaForRadio", rxArgs(rxColumn))
                     .value("accepted").toBool(true));
        QCOMPARE(core.model->alexController().rxAnt(Band::Band40m), 2);
    }
    void pairedV5PaTelemetryDescriptionUsesExistingOptionalWire()
    {
        Core core;
        core.model->setBoardForTest(HPSDRHW::Saturn);
        RadioInfo info = core.model->currentRadioInfo();
        info.boardType = HPSDRHW::Saturn;
        core.model->setLastRadioInfoForTest(info);
        core.server->setupDescription()->setRadioContext(core.model->boardCapabilities(),
                                                         core.model->hardwareProfile().model);
        core.server->setTelemetryEnabled(true);
        Device phone(QStringLiteral("PA telemetry iPhone"), QStringLiteral("phone"));
        core.pair(phone);
        QHash<QByteArray, int> features = kHolder;
        features.insert("setupDescription", 5);
        LoopbackTransport* app = core.signIn(phone, features);
        QVERIFY(admitted(app));
        QCOMPARE(capability(app->received(), QStringLiteral("setupDescriptionVersion")),
                 std::optional<qint64>(5));
        QCOMPARE(capability(app->received(), QStringLiteral("stationTelemetryVersion")),
                 std::optional<qint64>(6));
        const QJsonObject pa = QJsonDocument::fromJson(latest(app->received(),
            QStringLiteral("setup"), QStringLiteral("pa")).toString().toUtf8()).object();
        QCOMPARE(pa.value("version"), QJsonValue(5));
        const QJsonArray telemetry = pa.value("pages").toArray().last().toObject()
            .value("sections").toArray().at(1).toObject().value("controls").toArray();
        QCOMPARE(telemetry.size(), 4);
        QVERIFY(SetupDescriptionService::validatePaTelemetryReadoutBinding(telemetry.at(0).toObject()));
        QVERIFY(SetupDescriptionService::validatePaTelemetryReadoutBinding(telemetry.at(1).toObject()));

        StationTelemetrySnapshot sample;
        sample.sequence = 1;
        sample.radio.connected = true;
        sample.radio.paCurrentAmps = 0.0;
        sample.radio.supplyVolts = 13.8;
        QVERIFY(core.server->sendTelemetry(sample, core.server->sessionEpoch()));
        QJsonObject radio;
        QTRY_VERIFY([&] {
            for (const QJsonObject& message : ofType(app->received(), QStringLiteral("station.metrics.v1"))) {
                if (message.value("payload").toObject().value("sequence") == QJsonValue(1)) {
                    radio = message.value("payload").toObject().value("radio").toObject();
                    return true;
                }
            }
            return false;
        }());
        QCOMPARE(radio.value("paCurrentAmps"), QJsonValue(0.0));
        QCOMPARE(radio.value("supplyVolts"), QJsonValue(13.8));
        QVERIFY(!radio.contains("paVolts"));
        sample.sequence = 2;
        sample.radio.paCurrentAmps.reset();
        sample.radio.supplyVolts.reset();
        QVERIFY(core.server->sendTelemetry(sample, core.server->sessionEpoch()));
        QTRY_VERIFY([&] {
            for (const QJsonObject& message : ofType(app->received(), QStringLiteral("station.metrics.v1"))) {
                if (message.value("payload").toObject().value("sequence") != QJsonValue(2)) {
                    continue;
                }
                const QJsonObject next = message.value("payload").toObject().value("radio").toObject();
                return !next.contains("paCurrentAmps") && !next.contains("supplyVolts");
            }
            return false;
        }());
    }

    void pairedV4DisplaySettingsReachCoreAndNormalizeTracksCurrentDetector()
    {
        TxAnalyzer analyzer(TxAnalyzer::kTxDispId);
        Core core;
        core.model->setTxAnalyzer(&analyzer);
        core.server->setMediaEnabled(true);
        Device phone(QStringLiteral("Display settings iPhone"), QStringLiteral("phone"));
        core.pair(phone);
        QHash<QByteArray, int> features = kHolder;
        features.insert("setupDescription", 4);
        LoopbackTransport* app = core.signIn(phone, features);
        QVERIFY(admitted(app));
        QCOMPARE(capability(app->received(), QStringLiteral("setupDescriptionVersion")),
                 std::optional<qint64>(4));
        QCOMPARE(capability(app->received(), QStringLiteral("txDisplayVersion")),
                 std::optional<qint64>(3));
        const QJsonObject display = QJsonDocument::fromJson(latest(app->received(),
            QStringLiteral("setup"), QStringLiteral("display")).toString().toUtf8()).object();
        QCOMPARE(display.value("version"), QJsonValue(4));
        const QJsonObject normalize = display.value("pages").toArray().at(2).toObject()
            .value("sections").toArray().at(1).toObject().value("controls").toArray().at(3).toObject();
        QCOMPARE(normalize.value("enabledWhen").toObject().value("setting"),
                 QJsonValue("DisplayTxPanDetector"));
        QVERIFY(!normalize.value("gate").toObject().contains("offAir"));
        QVERIFY(!normalize.value("gate").toObject().contains("transmit"));
        const QJsonArray permitted = normalize.value("enabledWhen").toObject().value("oneOf").toArray();
        const auto enabled = [&permitted](const QString& current) {
            return !current.isEmpty() && permitted.contains(QJsonValue(current));
        };
        QVERIFY(!enabled({}));
        QVERIFY(!enabled(QStringLiteral("invalid")));
        QVERIFY(!enabled(QStringLiteral("1")));
        QVERIFY(enabled(QStringLiteral("2")));
        QVERIFY(enabled(QStringLiteral("3")));
        QVERIFY(enabled(QStringLiteral("4")));

        const auto write = [&core, app](const QString& key, const QString& value,
                                        const QString& origin) {
            app->sendText(SessionMessages::encode(SessionMessages::settingsWrite(key, value, origin)));
            const bool observed = QTest::qWaitFor([&core, app, &key, &value, &origin] {
                if (core.settings->value(key).toString() != value) { return false; }
                for (const QJsonObject& item : ofType(app->received(), QStringLiteral("settings.value"))) {
                    const QJsonArray properties = item.value("properties").toArray();
                    if (item.value("key") == QJsonValue(key)
                        && !properties.isEmpty()
                        && properties.first().toObject().value("value") == QJsonValue(value)
                        && item.value("origin") == QJsonValue(origin)) { return true; }
                }
                return false;
            }, 5000);
            if (!observed) {
                qWarning() << "Display write not observed" << key << value
                           << "stored" << core.settings->value(key)
                           << "wire" << app->received().mid(qMax(0, app->received().size() - 5));
            }
            return observed;
        };
        QVERIFY(write(QStringLiteral("DisplayFftSize"), QStringLiteral("8192"), QStringLiteral("rx")));
        QVERIFY(write(QStringLiteral("DisplayTxWindowType"), QStringLiteral("6"), QStringLiteral("tx")));
        SliceMeterPump* pump = core.model->sliceMeterPump();
        QVERIFY(pump != nullptr);
        QVERIFY(write(QStringLiteral("MultimeterDelayMs"), QStringLiteral("180"), QStringLiteral("meter")));
        QTRY_COMPARE(pump->intervalMs(), 180);
        for (const QString& value : {QStringLiteral("0"), QStringLiteral("2"),
                                     QStringLiteral("1")}) {
            QVERIFY(write(QStringLiteral("DisplayTxPanDetector"), value,
                          QStringLiteral("detector-") + value));
            QCOMPARE(enabled(core.settings->value(QStringLiteral("DisplayTxPanDetector")).toString()),
                     value == QLatin1String("2"));
        }
    }
    void pairedV3SettingsValidationPanelUsesExistingHygieneCapability()
    {
        Core core;
        Device phone(QStringLiteral("Settings Validation iPhone"), QStringLiteral("phone"));
        core.pair(phone);
        QHash<QByteArray, int> features = kHolder;
        features.insert("setupDescription", 3);
        features.insert("settingsHygiene", 1);
        LoopbackTransport* app = core.signIn(phone, features);
        QVERIFY(admitted(app));
        QCOMPARE(capability(app->received(), QStringLiteral("setupDescriptionVersion")),
                 std::optional<qint64>(3));
        QCOMPARE(capability(app->received(), QStringLiteral("settingsHygieneVersion")),
                 std::optional<qint64>(1));
        const QJsonObject diagnostics = QJsonDocument::fromJson(latest(app->received(),
            QStringLiteral("setup"), QStringLiteral("diagnostics")).toString().toUtf8()).object();
        QCOMPARE(diagnostics.value("version"), QJsonValue(3));
        const QJsonObject panel = diagnostics.value("pages").toArray().first().toObject()
            .value("sections").toArray().first().toObject()
            .value("controls").toArray().first().toObject();
        QVERIFY(SetupDescriptionService::validateSettingsHygienePanel(panel));
        const QJsonObject validated = core.invoke(app, "station.validateSettings",
            {MirrorUpdate{0, "mac", MirrorWireKind::Utf8, core.model->currentRadioMac()}});
        QVERIFY2(validated.value("accepted").toBool(),
                 qPrintable(validated.value("reason").toString()));
        QCOMPARE(validated.value("values").toArray().size(), 2);
        const QJsonObject noReset = core.invoke(app, "station.resetSettings",
            {MirrorUpdate{0, "mac", MirrorWireKind::Utf8, core.model->currentRadioMac()}});
        QVERIFY(!noReset.value("accepted").toBool(true));

        Device withoutHygiene(QStringLiteral("Older Settings iPhone"), QStringLiteral("phone"));
        core.pair(withoutHygiene);
        QHash<QByteArray, int> descriptionOnly = kHolder;
        descriptionOnly.insert("setupDescription", 3);
        LoopbackTransport* older = core.signIn(withoutHygiene, descriptionOnly);
        QVERIFY(admitted(older));
        QVERIFY(!capability(older->received(), QStringLiteral("settingsHygieneVersion"))
                     .value_or(0));
        const QJsonObject gated = core.invoke(older, "station.validateSettings",
            {MirrorUpdate{0, "mac", MirrorWireKind::Utf8, core.model->currentRadioMac()}});
        QVERIFY(!gated.value("accepted").toBool(true));
    }

    void pairedPaBypassSettingUsesExistingCoreAuthority()
    {
        Core core;
        core.model->setHpsdrModelForTest(HPSDRModel::ANAN_G2E);
        RadioInfo info = core.model->currentRadioInfo();
        info.boardType = HPSDRHW::HermesC10;
        core.model->setLastRadioInfoForTest(info);
        core.model->setReceiveOnlyStationPolicy(true);
        core.server->setupDescription()->setRadioContext(core.model->boardCapabilities(),
                                                         core.model->hardwareProfile().model);
        Device settingsPhone(QStringLiteral("PA settings phone"), QStringLiteral("phone"));
        core.pair(settingsPhone);
        QHash<QByteArray, int> features = kHolder;
        features.insert("setupDescription", 1);
        LoopbackTransport* app = core.signIn(settingsPhone, features);
        QVERIFY(admitted(app));
        // The Core's own advertised version, not a literal: it rises with
        // each transmit setting the Core learns to take.
        QCOMPARE(capability(app->received(), QStringLiteral("transmitSettingsVersion")),
                 std::optional<qint64>(
                     core.server->buildCapabilities().transmitSettingsVersion));
        const QJsonObject pa = QJsonDocument::fromJson(latest(app->received(),
            QStringLiteral("setup"), QStringLiteral("pa")).toString().toUtf8()).object();
        QCOMPARE(pa.value("pages").toArray().size(), 2);
        const QJsonObject control = pa.value("pages").toArray().first().toObject()
            .value("sections").toArray().first().toObject()
            .value("controls").toArray().first().toObject();
        QVERIFY(SetupDescriptionService::validatePaBypassBinding(control));
        QVERIFY(!txPermitted(app));
        QCOMPARE(core.model->transmitModel().paSettingsBypass(), false);
        Device txPhone(QStringLiteral("Denied TX phone"), QStringLiteral("phone"));
        core.pair(txPhone);
        QHash<QByteArray, int> txFeatures = kTransmitter;
        txFeatures.insert("setupDescription", 1);
        LoopbackTransport* denied = core.signIn(txPhone, txFeatures);
        QVERIFY(admitted(denied));
        QVERIFY(!txPermitted(denied));

        qint64 writeId = 970;
        const auto write = [&writeId](LoopbackTransport* peer, bool bypass) {
            const qint64 id = ++writeId;
            peer->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
                "transmit", {MirrorUpdate{0, "paSettingsBypass", MirrorWireKind::Bool, bypass}},
                static_cast<quint32>(id))));
            if (!QTest::qWaitFor([peer, id] { return !propertyResult(peer, id).isEmpty(); }, 5000)) {
                return QJsonObject{};
            }
            const QJsonArray results = propertyResult(peer, id).value("results").toArray();
            return results.isEmpty() ? QJsonObject{} : results.first().toObject();
        };
        const QJsonObject accepted = write(app, true);
        QVERIFY2(accepted.value("accepted").toBool(),
                 qPrintable(accepted.value("reason").toString()));
        QCOMPARE(core.model->transmitModel().paSettingsBypass(), true);
        // The writer gets settled readback in property.result; its own
        // requested delta is intentionally withheld by the Core. The other
        // paired session receives the changed mirror value.
        QTRY_COMPARE(latest(denied->received(), QStringLiteral("transmit"),
                            QStringLiteral("paSettingsBypass")), QJsonValue(true));
        core.model->transmitModel().setPaSettingsBypass(false);
        QTRY_COMPARE(latest(denied->received(), QStringLiteral("transmit"),
                            QStringLiteral("paSettingsBypass")), QJsonValue(false));
        QVERIFY(write(app, true).value("accepted").toBool());
        QTRY_COMPARE(latest(denied->received(), QStringLiteral("transmit"),
                            QStringLiteral("paSettingsBypass")), QJsonValue(true));

        // A client asking for remote transmit remains subject to the Core's
        // ordinary transmit decision when the receive-only settings exception
        // is absent. The description changes no permission policy.
        core.model->setReceiveOnlyStationPolicy(false);
        const QJsonObject refused = write(denied, false);
        QVERIFY(!refused.value("accepted").toBool(true));
        QVERIFY(!refused.value("reason").toString().isEmpty());
        QCOMPARE(core.model->transmitModel().paSettingsBypass(), true);

        allowTransmit(core);
        // allowTransmit configures a logical no-socket key but also turns
        // off receive-only station policy. Restore the daemon-style settings
        // exception before checking it on the air.
        core.model->setReceiveOnlyStationPolicy(true);
        MoxController* mox = core.model->moxController();
        mox->setMoxCheck({});
        mox->setMox(true); // logical test state, no radio transport
        QTRY_COMPARE(mox->state(), MoxState::Tx);
        // Remote parity on the air (transmitSettingsVersion 13): the local
        // PA page changes it while transmitting, so the Core takes it.
        const QJsonObject onAir = write(app, false);
        QVERIFY(onAir.value("accepted").toBool(false));
        QCOMPARE(core.model->transmitModel().paSettingsBypass(), false);
        mox->setMox(false);
        QTRY_COMPARE(mox->state(), MoxState::Rx);
        QVERIFY(!core.model->tune());
        QVERIFY(!core.model->transmitModel().isTwoToneActive());
    }

    void pairedPaReadoutsMirrorMetersAndRawCountsWithoutWriteAuthority()
    {
        Core core;
        allowTransmit(core);
        core.model->setBoardForTest(HPSDRHW::Saturn);
        RadioInfo info = core.model->currentRadioInfo();
        info.boardType = HPSDRHW::Saturn;
        core.model->setLastRadioInfoForTest(info);
        core.server->setupDescription()->setRadioContext(core.model->boardCapabilities(),
                                                         core.model->hardwareProfile().model);
        Device phone(QStringLiteral("PA iPhone"), QStringLiteral("phone"));
        core.pair(phone);
        QHash<QByteArray, int> features = kTransmitter;
        features.insert("setupDescription", 1);
        LoopbackTransport* app = core.signIn(phone, features);
        QVERIFY(admitted(app));
        // A9 (iPhone app plan Task 39): 3 adds the stage readings.
        QCOMPARE(capability(app->received(), QStringLiteral("txReadingsVersion")),
                 std::optional<qint64>(3));
        const QJsonObject pa = QJsonDocument::fromJson(latest(app->received(),
            QStringLiteral("setup"), QStringLiteral("pa")).toString().toUtf8()).object();
        QVERIFY(!pa.isEmpty());
        QCOMPARE(pa.value("pages").toArray().first().toObject().value("sections").toArray().size(), 3);

        core.model->handlePaTelemetryForTest(2400, 320, 0, 0, 0, 0);
        QVERIFY(core.model->radioStatus().forwardPowerWatts() > 0.0);
        QVERIFY(core.model->radioStatus().reflectedPowerWatts() > 0.0);
        QTRY_COMPARE(latest(app->received(), QStringLiteral("txState"),
                            QStringLiteral("forwardAdcRaw")).toInteger(), qint64(2400));
        QTRY_COMPARE(latest(app->received(), QStringLiteral("txState"),
                            QStringLiteral("reflectedAdcRaw")).toInteger(), qint64(320));
        QTRY_COMPARE(latest(app->received(), QStringLiteral("txState"),
                            QStringLiteral("forwardRawPowerWatts")).toDouble(),
                     scaleFwdPowerWatts(HPSDRModel::ANAN_G2, 2400));
        QTRY_COMPARE(latest(app->received(), QStringLiteral("txState"),
                            QStringLiteral("forwardAdcVolts")).toDouble(),
                     scaleFwdRevVoltage(HPSDRModel::ANAN_G2, 2400));
        QTRY_COMPARE(latest(app->received(), QStringLiteral("txState"),
                            QStringLiteral("reflectedAdcVolts")).toDouble(),
                     scaleFwdRevVoltage(HPSDRModel::ANAN_G2, 320));
        QTRY_COMPARE(latest(app->received(), QStringLiteral("txState"),
                            QStringLiteral("forwardPowerWatts")).toDouble(),
                     core.model->radioStatus().forwardPowerWatts());
        QTRY_COMPARE(latest(app->received(), QStringLiteral("txState"),
                            QStringLiteral("reflectedPowerWatts")).toDouble(),
                     core.model->radioStatus().reflectedPowerWatts());
        QTRY_COMPARE(latest(app->received(), QStringLiteral("txState"),
                            QStringLiteral("swr")).toDouble(),
                     core.model->radioStatus().swrRatio());

        const qint64 writeId = 876;
        app->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
            "txState", {MirrorUpdate{0, "forwardAdcRaw", MirrorWireKind::Int64, qint64(1)}},
            static_cast<quint32>(writeId))));
        QTRY_VERIFY(!propertyResult(app, writeId).isEmpty());
        const QJsonArray results = propertyResult(app, writeId).value("results").toArray();
        QVERIFY(!results.isEmpty());
        QVERIFY(!results.first().toObject().value("accepted").toBool(true));
        QCOMPARE(core.server->transmitState()->forwardAdcRaw(), qint64(2400));
        QVERIFY(core.invoke(app, "tx.take", {}).value(QStringLiteral("accepted")).toBool());
        QVERIFY(txPermitted(app));
        const qint64 ownerWriteId = 877;
        app->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
            "txState", {MirrorUpdate{34, "forwardRawPowerWatts", MirrorWireKind::Float64, 1.0}},
            static_cast<quint32>(ownerWriteId))));
        QTRY_VERIFY(!propertyResult(app, ownerWriteId).isEmpty());
        QVERIFY(!propertyResult(app, ownerWriteId).value("results").toArray()
                     .first().toObject().value("accepted").toBool(true));
        QCOMPARE(core.server->transmitState()->forwardRawPowerWatts(),
                 scaleFwdPowerWatts(HPSDRModel::ANAN_G2, 2400));
        Device observer(QStringLiteral("PA observer"), QStringLiteral("phone"));
        core.pair(observer);
        LoopbackTransport* other = core.signIn(observer, kHolder);
        QVERIFY(admitted(other));
        QVERIFY(!txPermitted(other));
        const qint64 observerWriteId = 878;
        other->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
            "txState", {MirrorUpdate{35, "forwardAdcVolts", MirrorWireKind::Float64, 1.0}},
            static_cast<quint32>(observerWriteId))));
        QTRY_VERIFY(!propertyResult(other, observerWriteId).isEmpty());
        QVERIFY(!propertyResult(other, observerWriteId).value("results").toArray()
                     .first().toObject().value("accepted").toBool(true));
        QCOMPARE(core.server->transmitState()->forwardAdcVolts(),
                 scaleFwdRevVoltage(HPSDRModel::ANAN_G2, 2400));
    }

    void pairedPaDriveReadoutUsesSelectedPowerMirror()
    {
        Core core;
        core.model->setBoardForTest(HPSDRHW::Saturn);
        RadioInfo info = core.model->currentRadioInfo();
        info.boardType = HPSDRHW::Saturn;
        core.model->setLastRadioInfoForTest(info);
        core.server->setupDescription()->setRadioContext(core.model->boardCapabilities(),
                                                         core.model->hardwareProfile().model);
        Device phone(QStringLiteral("PA Drive phone"), QStringLiteral("phone"));
        core.pair(phone);
        QHash<QByteArray, int> features = kHolder;
        features.insert("setupDescription", 1);
        LoopbackTransport* app = core.signIn(phone, features);
        QVERIFY(admitted(app));
        // The Core's own advertised version, not a literal: it rises with
        // each transmit setting the Core learns to take.
        QCOMPARE(capability(app->received(), QStringLiteral("transmitSettingsVersion")),
                 std::optional<qint64>(
                     core.server->buildCapabilities().transmitSettingsVersion));
        const QJsonObject pa = QJsonDocument::fromJson(latest(app->received(),
            QStringLiteral("setup"), QStringLiteral("pa")).toString().toUtf8()).object();
        const QJsonArray power = pa.value("pages").toArray().last().toObject()
            .value("sections").toArray().first().toObject().value("controls").toArray();
        QCOMPARE(power.size(), 5);
        const QJsonObject drive = power.last().toObject();
        QVERIFY(SetupDescriptionService::validatePaDriveReadoutBinding(drive));
        QCOMPARE(drive.value("kind"), QJsonValue("readout"));
        core.model->transmitModel().setPower(37);
        QTRY_COMPARE(latest(app->received(), QStringLiteral("transmit"),
                            QStringLiteral("power")).toInteger(), qint64(37));
        QCOMPARE(core.model->transmitModel().power(), 37);
    }

    void pairedHardwareDescriptionWritesReachBoundAlexAndRetireOnSwap()
    {
        Core core;
        core.model->setBoardForTest(HPSDRHW::Hermes);
        RadioInfo info = core.model->currentRadioInfo();
        info.boardType = HPSDRHW::Hermes;
        core.model->setLastRadioInfoForTest(info);
        StepAttenuatorController attenuator;
        attenuator.setTickTimerEnabled(false);
        core.model->setStepAttController(&attenuator);
        core.server->setupDescription()->setRadioContext(core.model->boardCapabilities(),
                                                         core.model->hardwareProfile().model);
        Device phone(QStringLiteral("Setup iPhone"), QStringLiteral("phone"));
        core.pair(phone);
        QHash<QByteArray, int> features = kHolder;
        features.insert("setupDescription", 1);
        LoopbackTransport* app = core.signIn(phone, features);
        QVERIFY(admitted(app));
        QCOMPARE(capability(app->received(), QStringLiteral("setupDescriptionVersion")),
                 std::optional<qint64>(1));
        const QString description = latest(app->received(), QStringLiteral("setup"),
                                           QStringLiteral("hardware")).toString();
        const QJsonObject hardware = QJsonDocument::fromJson(description.toUtf8()).object();
        QVERIFY(!hardware.isEmpty());
        const QJsonArray controls = hardware.value("pages").toArray().first().toObject()
            .value("sections").toArray().first().toObject().value("controls").toArray();
        QCOMPARE(controls.size(), 6);
        const auto find = [&controls](const QString& id) {
            for (const QJsonValue& raw : controls) {
                const QJsonObject control = raw.toObject();
                if (control.value("id") == QJsonValue(id)) { return control; }
            }
            return QJsonObject{};
        };
        const QJsonObject rx = find(QStringLiteral("hardware.antennaAlex.useTxAntennaForRx"));
        const QJsonObject tx = find(QStringLiteral("hardware.antennaAlex.blockTxAnt2"));
        const QJsonObject relay = find(QStringLiteral("hardware.antennaAlex.ext1OutOnTx"));
        QVERIFY(SetupDescriptionService::validateHardwarePropertyBinding(rx));
        QVERIFY(SetupDescriptionService::validateHardwarePropertyBinding(tx));
        QVERIFY(SetupDescriptionService::validateHardwarePropertyBinding(
            relay, core.model->hardwareProfile().model));
        QCOMPARE(relay.value("gate").toObject().value("offAir"), QJsonValue(true));
        QVERIFY(!tx.value("gate").toObject().contains("transmit"));
        QCOMPARE(tx.value("gate").toObject().value("offAir"), QJsonValue(true));

        qint64 writeId = 700;
        const auto write = [&](const QJsonObject& control, bool value) {
            const QByteArray name = control.value("binding").toObject()
                .value("property").toObject().value("name").toString().toUtf8();
            const qint64 id = ++writeId;
            app->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
                "alexAntennas", {MirrorUpdate{0, name, MirrorWireKind::Bool, value}},
                static_cast<quint32>(id))));
            const bool answered = QTest::qWaitFor(
                [app, id] { return !propertyResult(app, id).isEmpty(); }, 5000);
            if (!answered) { return QJsonObject{}; }
            const QJsonArray results = propertyResult(app, id).value("results").toArray();
            return results.isEmpty() ? QJsonObject{} : results.first().toObject();
        };
        MoxController* mox = core.model->moxController();
        QVERIFY(mox != nullptr);
        QSignalSpy keying(mox, &MoxController::stateChanged);
        const QJsonObject rxResult = write(rx, true);
        QVERIFY2(rxResult.value("accepted").toBool(),
                 qPrintable(rxResult.value("reason").toString()));
        const QJsonObject txResult = write(tx, true);
        QVERIFY2(txResult.value("accepted").toBool(),
                 qPrintable(txResult.value("reason").toString()));
        const QJsonObject relayResult = write(relay, true);
        QVERIFY2(relayResult.value("accepted").toBool(),
                 qPrintable(relayResult.value("reason").toString()));
        QVERIFY(core.model->alexController().useTxAntForRx());
        QVERIFY(core.model->alexController().blockTxAnt2());
        QVERIFY(core.model->alexController().ext1OutOnTx());
        QCOMPARE(mox->state(), MoxState::Rx);
        QCOMPARE(keying.count(), 0);

        // These property writes are supported even for a paired receive-only
        // session while off air; the Core's Alex policy has no TX permission
        // requirement. An unavailable hardware controller is still refused.
        core.model->alexAntennaFacade()->bindController(nullptr);
        const QJsonObject unavailable = write(tx, false);
        QVERIFY(!unavailable.value("accepted").toBool(true));
        QVERIFY(!unavailable.value("reason").toString().isEmpty());
        QVERIFY(core.model->alexController().blockTxAnt2());
        core.model->alexAntennaFacade()->bindController(&core.model->alexControllerMutable());

        allowTransmit(core);
        mox->setMox(true); // station's own holder; no radio connection exists
        QTRY_COMPARE(mox->state(), MoxState::Tx);
        const QJsonObject onAir = write(tx, false);
        QVERIFY(!onAir.value("accepted").toBool(true));
        QCOMPARE(onAir.value("reason").toString(),
                 QStringLiteral("The radio is on the air. Try again when it stops."));
        QVERIFY(core.model->alexController().blockTxAnt2());
        const QJsonObject relayOnAir = write(relay, false);
        QVERIFY(!relayOnAir.value("accepted").toBool(true));
        QCOMPARE(relayOnAir.value("reason").toString(),
                 QStringLiteral("The radio is on the air. Try again when it stops."));
        QVERIFY(core.model->alexController().ext1OutOnTx());
        mox->setMox(false);
        QTRY_COMPARE(mox->state(), MoxState::Rx);

        // A connected-board replacement removes the whole partial category.
        // Its new revision invalidates a renderer's old gesture; even a
        // forced stale wire write is refused after the controller retires.
        const quint32 capturedRevision = core.server->setupDescription()->revision();
        core.model->setBoardForTest(HPSDRHW::HermesLite);
        info.boardType = HPSDRHW::HermesLite;
        core.model->setLastRadioInfoForTest(info);
        core.model->alexAntennaFacade()->bindController(nullptr);
        core.server->setupDescription()->setRadioContext(core.model->boardCapabilities(),
                                                         core.model->hardwareProfile().model);
        QVERIFY(core.server->setupDescription()->revision() > capturedRevision);
        QTRY_VERIFY(latest(app->received(), QStringLiteral("setup"),
                           QStringLiteral("hardware")).toString().isEmpty());
        // Version 13's Radio Info and Calibration stay; Antenna / ALEX goes.
        QVERIFY(!core.server->setupDescription()->hardware()
                     .contains(QStringLiteral("hardware.antennaAlex")));
        const QJsonObject stale = write(rx, false);
        QVERIFY(!stale.value("accepted").toBool(true));
        QVERIFY(!stale.value("reason").toString().isEmpty());
        QVERIFY(core.model->alexController().useTxAntForRx());
    }

    void twoTonePresetHonoursHolderAndOnAirGuards()
    {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::HermesLiteRxOnly);
        SessionCommandDispatcher dispatcher(&model);
        QList<SessionMessage> results;
        connect(&dispatcher, &SessionCommandDispatcher::commandResultReady, this,
                [&results](const SessionMessage& result) { results.append(result); });
        bool anotherHolds = false;
        SessionCommandDispatcher::TransmitAccess access;
        access.transmitter = [&anotherHolds](const QByteArray&) -> TxRefusal {
            return anotherHolds ? TxRefusals::otherDeviceHolds(QStringLiteral("Another phone"))
                                : TxRefusal{};
        };
        dispatcher.setTransmitAccess(access);
        dispatcher.setRequester("phone");
        auto invoke = [&dispatcher, &results](const QString& name) {
            results.clear();
            dispatcher.dispatch(SessionMessages::commandInvoke("tx.twoTonePreset", 1,
                {MirrorUpdate{0, "name", MirrorWireKind::Utf8, name}}));
            return results.isEmpty() ? SessionMessage{} : results.last();
        };
        QVERIFY(invoke(QStringLiteral("stealth")).accepted);
        QCOMPARE(model.transmitModel().twoToneFreq1(), 70);
        QCOMPARE(model.transmitModel().twoToneFreq2(), 190);
        anotherHolds = true;
        QVERIFY(!invoke(QStringLiteral("defaults")).accepted);
        QCOMPARE(model.transmitModel().twoToneFreq1(), 70);
        anotherHolds = false;
        model.transmitModel().setMox(true);
        QVERIFY(!invoke(QStringLiteral("defaults")).accepted);
        model.transmitModel().setMox(false);
        model.transmitModel().setTune(true);
        QVERIFY(!invoke(QStringLiteral("defaults")).accepted);
        model.transmitModel().setTune(false);
        QVERIFY(!invoke(QStringLiteral("unknown")).accepted);
        QCOMPARE(model.transmitModel().twoToneFreq1(), 70);
        QVERIFY(invoke(QStringLiteral("defaults")).accepted);
        QCOMPARE(model.transmitModel().twoToneFreq1(), 700);
        QCOMPARE(model.transmitModel().twoToneFreq2(), 1900);
    }

    void twoTonePresetPublishesACompletePairBeforePropertySignals()
    {
        TransmitModel model;
        int otherFrequencyAtFirstSignal = 0;
        int combinedSignals = 0;
        connect(&model, &TransmitModel::twoToneFrequenciesChanged, this,
                [&combinedSignals](int, int) { ++combinedSignals; });
        connect(&model, &TransmitModel::twoToneFreq1Changed, this,
                [&model, &otherFrequencyAtFirstSignal](int) {
                    otherFrequencyAtFirstSignal = model.twoToneFreq2();
                });
        model.setTwoToneFrequencies(70, 190);
        QCOMPARE(model.twoToneFreq1(), 70);
        QCOMPARE(model.twoToneFreq2(), 190);
        QCOMPARE(otherFrequencyAtFirstSignal, 190);
        QCOMPARE(combinedSignals, 1);
    }

    void remoteIdentityWritesReachRunningReporter()
    {
        AppSettings& settings = AppSettings::instance();
        settings.clear();
        RadioModel model;
        SettingsProxyServer proxy(settings);
        auto* reporter = model.freeDvReporter();
        QVERIFY(reporter != nullptr);

        const auto call = proxy.applyInboundWrite(
            QStringLiteral("User/Callsign"), QStringLiteral("KG4VCF"), QStringLiteral("phone"));
        QVERIFY2(call.accepted, qPrintable(call.reason));
        model.applyRemoteFreedvSetting(QStringLiteral("User/Callsign"), settings);
        QCOMPARE(reporter->callsignForTest(), QStringLiteral("KG4VCF"));
        QCOMPARE(settings.value(QStringLiteral("PskReporter/Callsign")).toString(),
                 QStringLiteral("KG4VCF"));

        const auto grid = proxy.applyInboundWrite(
            QStringLiteral("User/GridSquare"), QStringLiteral("EM73"), QStringLiteral("phone"));
        QVERIFY2(grid.accepted, qPrintable(grid.reason));
        model.applyRemoteFreedvSetting(QStringLiteral("User/GridSquare"), settings);
        QCOMPARE(reporter->gridSquareForTest(), QStringLiteral("EM73"));
        QCOMPARE(settings.value(QStringLiteral("PskReporter/GridSquare")).toString(),
                 QStringLiteral("EM73"));
        settings.clear();
    }
};

QTEST_MAIN(SetupDescriptionLiveTest)
#include "tst_setup_description_live.moc"
