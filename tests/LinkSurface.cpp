// no-port-check: NereusSDR-original.
// =================================================================
// tests/LinkSurface.cpp  (NereusSDR)
// =================================================================
//
// See LinkSurface.h. Every section is read from the code: a table the code
// declares, a probe of the code's own codec, or a live session. Where the
// code has no reachable table (the media control field sets, three limits
// held in file-scope constants), the value is written here with a cite
// beside it, and tst_link_surface_manifest scans the cited source so the
// two cannot drift apart.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-30  J.J. Boyd / KG4VCF  RADE reason: the capture declares
//                                    radeReason, so radeReasonVersion and
//                                    the slice's radeReason are captured.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  PA on-air gate re-review: the capture
//                                    declares paTransmitBand, so
//                                    paTransmitBandVersion and radio's
//                                    paTransmitBand are captured.
//   2026-09-30  J.J. Boyd / KG4VCF  Shared-input filters (ruling (d)): the
//                                    capture declares rxFilterLowPass, so
//                                    rxFilterLowPassVersion and radio's
//                                    rxFilter0LowPassReason and
//                                    rxFilter0LowPassSlice are captured.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  Radio codec lane: the capture
//                                    declares radioMic, so radioMicVersion
//                                    is captured. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  Level Cal 2: the capture declares
//                                    rx2Attenuator, so rx2AttenuatorVersion
//                                    is captured. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  The direct media ladder: the capture
//                                    declares mediaDirect, so
//                                    mediaDirectVersion and mediaStunUrls
//                                    are captured, and the GUI's replace
//                                    may carry mediaDirectVersion.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  RADE status: the capture declares
//                                    radeStatus, so radeStatusVersion and
//                                    the slice's radeSynced and
//                                    radeFreqOffsetHz are captured.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 1 (R-IOS-01): link
//                                    surface capture. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 2 (R-IOS-01): the
//                                    station TCI server's class and
//                                    object after the RF-Kit merge.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 4 (R-IOS-01): the
//                                    accessory records' class after the
//                                    accessories merge; the hello's
//                                    `majors` and `features`. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Part A fix wave (R-IOS-01):
//                                    each message key's JSON type.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  Lane B takes integration (R-IOS-01,
//                                    R-R3-47): the `accessorySettings`
//                                    class. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 13 (R-IOS-08): the
//                                    `devices` class; the live session's
//                                    client declares deviceAuth so the
//                                    object is sent. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 14 (R-IOS-08): the
//                                    five pair.* kinds' sample messages.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-24: Part C fix wave: the optional device shortName in
//               auth.request, stored with the device. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-24: Part C fix wave (security Minors R1-M1, M2, M4,
//               M5): the confirm-step recheck, the step 1 point check, the
//               per-address handshake cap and 0600 on load. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic Claude
//               Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 19 (R-IOS-06): the
//                                    `catalog` class. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-25: iPhone app Task 71 (R-IOS-02): ConnectedDevicesFacade,
//               sessionHolderVersion (the live client declares
//               sessionHolder), and the maxDeviceSessions, graceMs and
//               lanAnnouncementMaxBytes limits. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-25: iPhone app plan Task 34 (R-IOS-02): remoteTxVersion (the
//               live client declares remoteTx) and tx.setTxSlice. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-25: iPhone app Task 73 (R-IOS-02): SliceMarker and the
//               `marker:<id>` key (a slice held for an away device in the
//               live session). J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-25: iPhone app Task 74 (R-IOS-30): confirm.request and notice
//               samples, and the confirmExpiryMs limit. J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-IOS-27, R-IOS-06: the clarity-retune
//                                    media operation. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  Remote-window parity Task 14 (R-R3-32):
//                                    stationTelemetryVersion 5, the HL2 link.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  iPhone app plan Task 77 (R-IOS-02,
//                                    R-IOS-03, R-IOS-13): the takeTransmit
//                                    holder in the confirm.request sample. AI-
//                                    assisted via Anthropic Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  Remote-window parity Task 19 (R-IOS-25):
//                                    the record.batch sample.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-26: iPhone app plan Task 28 (R-IOS-16): the
//               controlChannelChunkBytes limit. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-27  J.J. Boyd / KG4VCF  Remote-window parity Task 32 (R-IOS-13,
//                                    R-R3-49): the start's
//                                    txMonitorAudioVersion, monitor-audio
//                                    and monitor-audio-context. AI-assisted
//                                    via Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  The capture declares
//                                    stationTciSettings (the Core's TCI
//                                    server settings). AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  iPhone app plan Task 23: the capture
//                                    declares audioQuality; the audio
//                                    control's opusBitrate and the main
//                                    context's opusBitrateRefusal.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-28  J.J. Boyd / KG4VCF  R-IOS-13 / R-R3-49: the capture
//                                    declares txEqCurve, so
//                                    txEqCurveVersion and transmit's
//                                    txEqCurve are captured. AI-assisted
//   2026-09-28  J.J. Boyd / KG4VCF  Phone wire batch: the capture declares
//                                    diversityPattern, so
//                                    diversityPatternVersion and the
//                                    slice's diversityPattern are captured;
//                                    and logCategoryList, so
//                                    logCategoryListVersion and radio's
//                                    logCategoryList are; and radioModels,
//                                    so radioModelsVersion is. AI-assisted
//                                    via Anthropic Claude Code.
//   2026-09-28  J.J. Boyd / KG4VCF  R-IOS-13 / R-R3-49: the capture
//                                    declares txEqCurve 2, so
//                                    txEqCurveVersion is captured at 2 (the
//                                    curve verbs). AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  The phone's direct addresses: the
//                                    capture declares coreAddresses, so
//                                    coreAddressesVersion and devices'
//                                    coreAddresses are captured.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  The capture declares alexLpf, so
//                                    radio's alexLpfBits is captured.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-28  J.J. Boyd / KG4VCF  R-R3-46 / R-R3-11: the capture
//                                    declares adcAttenuators, so
//                                    adcAttenuatorVersion and stepAtt's
//                                    rx2AttenuationDb and rx2SliceMask are
//                                    captured. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  R-R3-49 / R-IOS-18: and paProfiles, so
//                                    paProfileVersion and the `paProfiles`
//                                    object are captured. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  HL2 port part 2: and txInhibitReason,
//                                    so txInhibitReasonVersion and radio's
//                                    txInhibitReason are. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-28: slice control plan Task 4: sliceAccessVersion (the live
//               client declares sliceAccess), SliceAccess and the
//               `access:<id>` key. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-30: take-over parity: the live client declares sliceAccess 2,
//               so sliceAccessVersion reads 2. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-30: core-slice take-over: the live client declares
//               sliceAccess 3, so sliceAccessVersion reads 3. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include "LinkSurface.h"

#include "core/SpotSourceHost.h"

#include <QCoreApplication>
#include <QDeadlineTimer>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QMetaObject>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QSet>
#include <QTemporaryDir>

#include <algorithm>
#include <functional>
#include <limits>
#include <memory>
#include <optional>

#include "core/AppSettings.h"
#include "core/ConnectionState.h"
#include "core/HpsdrModel.h"
#include "core/IoBoardHl2Facade.h"
#include "core/RadioDiscovery.h"
#include "core/StepAttenuatorController.h"
#include "core/StepAttenuatorFacade.h"
#include "core/accessories/AlexAntennaFacade.h"
#include "core/dsp/DspAssetService.h"
#include "core/security/DeviceStore.h"
#include "core/security/ClientDeviceIdentity.h"
#include "core/security/DeviceAuthenticator.h"
#include "core/security/StationIdentity.h"
#include "core/BuildIdentity.h"
#include "core/session/DataChannelTransport.h"
#include "core/session/PathRacer.h"
#include "core/session/RendezvousDialer.h"
#include "core/session/SwitchableTransport.h"
#include "core/session/MirrorEnumDomain.h"
#include "core/session/MirrorPolicy.h"
#include "core/session/MirrorSchema.h"
#include "core/session/PaProfilesFacade.h"
#include "core/session/PureSignalSessionFacade.h"
#include "core/session/SessionCommandDispatcher.h"
#include "core/session/SessionMessages.h"
#include "core/session/StationCapabilities.h"
#include "core/session/StationClient.h"
#include "core/session/StationCatalog.h"
#include "core/session/StationDevicesFacade.h"
#include "core/session/ConnectedDevicesFacade.h"
#include "core/session/SliceAccessSet.h"
#include "core/session/SliceMarker.h"
#include "core/session/StationVaxFacade.h"
#include "core/session/TransmitStateFacade.h"
#include "core/setup/SetupDescriptionService.h"
#include "core/SliceOwnership.h"
#include "core/session/ConfirmStep.h"
#include "core/session/DeviceSessionRegistry.h"
#include "core/session/StationLanAnnouncement.h"
#include "core/session/StationServer.h"
#include "core/session/StationTelemetry.h"
#include "core/session/media/DaemonMediaController.h"
#include "core/session/media/DisplayBudget.h"
#include "core/session/media/OpusAudioCodec.h"
#include "core/session/media/PcmAudioCodec.h"
#include "core/session/media/RemoteAudioContext.h"
#include "core/session/media/RemoteSpectrumContext.h"
#include "core/session/media/SpectrumEndpoint.h"
#include "core/settings/SettingsScope.h"
#include "models/AccessoryDataModel.h"
#include "models/AccessorySettingsModel.h"
#include "models/AmplifierModel.h"
#include "models/NotchModel.h"
#include "models/PanadapterModel.h"
#include "models/PureSignalSettings.h"
#include "models/RadioModel.h"
#include "models/RfKitModel.h"
#include "models/SliceModel.h"
#include "models/StationTciModel.h"
#include "models/TransmitModel.h"
#include "models/TunerModel.h"

#include "fakes/LoopbackTransport.h"
#include "fakes/UpgradedCoreToken.h"

namespace NereusSDR::Test {

namespace {

QString wireKind(MirrorWireKind kind)
{
    return QString::fromUtf8(SessionMessages::wireKindName(kind));
}

QJsonArray sortedArray(QStringList values)
{
    values.sort();
    values.removeDuplicates();
    return QJsonArray::fromStringList(values);
}

// ── messageKinds ─────────────────────────────────────────────────────────

MirrorUpdate sampleUpdate(const char* name)
{
    return MirrorUpdate{0, QByteArray(name), MirrorWireKind::Int64, QVariant(qlonglong(1))};
}

StationTelemetrySnapshot sampleTelemetry(int version);

// A well-formed message of each kind with every optional field the encoder
// can emit present, so dropping one key at a time tells which keys decode()
// requires. nullopt for a kind nobody taught this function: the capture
// then records an error the manifest test refuses.
std::optional<SessionMessage> sampleMessage(SessionMessageKind kind)
{
    switch (kind) {
    case SessionMessageKind::Schema:
        return SessionMessages::schema("SliceModel",
                                       {SessionSchemaField{0, "frequency", MirrorWireKind::Int64}});
    case SessionMessageKind::ObjectCreate:
        return SessionMessages::objectCreate("slice:0", "SliceModel", {sampleUpdate("frequency")});
    case SessionMessageKind::ObjectDestroy:
        return SessionMessages::objectDestroy("slice:0", "SliceModel");
    case SessionMessageKind::Delta:
        return SessionMessages::delta("slice:0", {sampleUpdate("frequency")});
    case SessionMessageKind::SnapshotComplete:
        return SessionMessages::snapshotComplete();
    case SessionMessageKind::CommandInvoke:
        return SessionMessages::commandInvoke("removeSlice", 1, {sampleUpdate("sliceId")});
    case SessionMessageKind::CommandResult:
        return SessionMessages::commandResult("removeSlice", 1, true, QString(), {"slice:0"},
                                              {sampleUpdate("revision")});
    case SessionMessageKind::Hello: {
        // With the Task 4 declarations, so `majors` and `features` are
        // recorded (as optional: an older peer sends neither), and the
        // Core's Task 12 `identity` and `challenge` (optional: a client's
        // hello, and an older Core's, carry neither). Placeholders, not keys.
        SessionMessage m =
            SessionMessages::hello(kSessionProtocolMajor, kSessionProtocolMinor, 1,
                                   QStringLiteral("link-surface"), {kSessionProtocolMajor},
                                   {{"linkSurface", 1}});
        m.stationIdentity = SessionStationIdentity{QStringLiteral("key"),
                                                   QStringLiteral("binding")};
        m.challenge = QStringLiteral("challenge");
        return m;
    }
    case SessionMessageKind::AuthRequest:
        // Placeholders, never a real token, key or signature. With Task 12's
        // `device` (optional: a token sign-in carries none).
        return SessionMessages::authRequest(
            QStringLiteral("placeholder"),
            SessionDeviceBlock{QStringLiteral("id"), QStringLiteral("key"),
                               QStringLiteral("name"), QStringLiteral("phone"),
                               QStringLiteral("signature"), QStringLiteral("short")});
    case SessionMessageKind::AuthResult:
        // With Task 12's end `code` (optional).
        return SessionMessages::authResult(false, QStringLiteral("refused"), true,
                                           QStringLiteral("code"));
    case SessionMessageKind::Capabilities:
        return SessionMessages::capabilities({sampleUpdate("remoteMediaVersion")});
    case SessionMessageKind::SessionEnd: {
        auto end = SessionMessages::sessionEnd(QStringLiteral("ended"), true,
                                                QStringLiteral("code"));
        end.takenOverBy = QStringLiteral("tablet");
        end.takenOverById = QStringLiteral("id");
        end.secondsAgo = 0;
        return end;
    }
    case SessionMessageKind::SessionHeld:
        return SessionMessages::sessionHeld({}, 1,
            QJsonObject{{QStringLiteral("byName"), QStringLiteral("tablet")},
                        {QStringLiteral("byId"), QStringLiteral("id")},
                        {QStringLiteral("secondsAgo"), 0}},
            QJsonObject{{QStringLiteral("secondsAgo"), 0}});
    case SessionMessageKind::SessionTakeover:
        return SessionMessages::sessionTakeover(QStringLiteral("id"), 1);
    case SessionMessageKind::PropertyWrite:
        return SessionMessages::propertyWrite("slice:0", {sampleUpdate("frequency")}, 7);
    case SessionMessageKind::PropertyResult: {
        SessionPropertyResult result;
        result.property = "frequency";
        result.accepted = true;
        result.hasValue = true;
        result.value = sampleUpdate("frequency");
        return SessionMessages::propertyResult("slice:0", 7, {result});
    }
    case SessionMessageKind::SettingsSnapshot:
        return SessionMessages::settingsSnapshot(
            {MirrorUpdate{0, "StationCallsign", MirrorWireKind::Utf8, QStringLiteral("x")}});
    case SessionMessageKind::SettingsWrite:
        return SessionMessages::settingsWrite(QStringLiteral("StationCallsign"),
                                              QStringLiteral("x"), QStringLiteral("origin"));
    case SessionMessageKind::SettingsRemove:
        return SessionMessages::settingsRemove(QStringLiteral("StationCallsign"));
    case SessionMessageKind::SettingsValue:
        return SessionMessages::settingsValue(QStringLiteral("StationCallsign"),
                                              QStringLiteral("x"), QStringLiteral("origin"));
    case SessionMessageKind::SettingsReject:
        return SessionMessages::settingsReject(QStringLiteral("StationCallsign"), true,
                                               QStringLiteral("x"), QStringLiteral("refused"));
    case SessionMessageKind::MediaControl: {
        SessionMessage m;
        m.kind = SessionMessageKind::MediaControl;
        m.mediaPayload = {{QStringLiteral("op"), QStringLiteral("start")}};
        return m;
    }
    case SessionMessageKind::StationTelemetry: {
        SessionMessage m;
        m.kind = SessionMessageKind::StationTelemetry;
        m.telemetry = sampleTelemetry(4);
        return m;
    }
    // iPhone app Task 14: the pair.* kinds. Placeholders, never a real key,
    // code, share or box.
    case SessionMessageKind::PairStart:
        return SessionMessages::pairStart(
            QStringLiteral("code"),
            SessionPairDevice{QStringLiteral("key"), QStringLiteral("name"),
                              QStringLiteral("phone")});
    case SessionMessageKind::PairAccept:
        return SessionMessages::pairAccept(
            SessionStationIdentity{QStringLiteral("key"), QStringLiteral("binding")},
            QStringLiteral("KG4VCF/shack"));
    case SessionMessageKind::PairSpake:
        return SessionMessages::pairSpake(1, QStringLiteral("share"));
    case SessionMessageKind::PairConfirm:
        return SessionMessages::pairConfirm(QStringLiteral("box"));
    case SessionMessageKind::PairFail:
        return SessionMessages::pairFail(QStringLiteral("refused"), 5000);
    // iPhone app Task 74: every optional key present.
    case SessionMessageKind::ConfirmRequest: {
        SessionPrompt prompt;
        prompt.id = 1;
        prompt.kind = QStringLiteral("panMove");
        prompt.affected = QJsonArray{QJsonObject{}};
        prompt.expiresInMs = 60000;
        prompt.change = QJsonObject{{QStringLiteral("label"), QStringLiteral("Receiver 1")}};
        prompt.choices = QJsonArray{QJsonObject{}};
        prompt.forCommandId = 2;
        prompt.forWriteId = 3;
        prompt.forSettingsKey = QStringLiteral("StationCallsign");
        // iPhone app plan Task 77: takeTransmit's holder entry.
        prompt.holder = QJsonObject{{QStringLiteral("name"), QStringLiteral("iPhone")}};
        return SessionMessages::confirmRequest(prompt, QStringLiteral("asked"));
    }
    case SessionMessageKind::Notice: {
        SessionPrompt prompt;
        prompt.id = 1;
        prompt.kind = QStringLiteral("receiverTaken");
        prompt.secondsAgo = 3;
        prompt.takeBack = true;
        prompt.byDeviceId = QStringLiteral("id");
        prompt.byName = QStringLiteral("name");
        prompt.byShortName = QStringLiteral("short");
        prompt.byKind = QStringLiteral("phone");
        prompt.bySource = QStringLiteral("device");
        prompt.slices = QJsonArray{QJsonObject{}};
        prompt.change = QJsonObject{{QStringLiteral("label"), QStringLiteral("Receiver 1")}};
        return SessionMessages::notice(prompt, QStringLiteral("told"));
    }
    // Parity Task 19: a record stream's changes.
    case SessionMessageKind::RecordBatch: {
        RecordBatch batch;
        batch.stream = QStringLiteral("spots");
        batch.generation = 1;
        batch.upserts = {{QStringLiteral("1"), QJsonObject{{QStringLiteral("call"),
                                                            QStringLiteral("K1ABC")}}}};
        batch.removes = {QStringLiteral("2")};
        return SessionMessages::recordBatch(batch);
    }
    // iPhone app plan Task 29: moving a session to another connection.
    case SessionMessageKind::PathJoin:
        return SessionMessages::pathJoin(QStringLiteral("ticket"));
    case SessionMessageKind::PathSwitch:
        return SessionMessages::pathSwitch();
    }
    return std::nullopt;
}

// A JSON value's type by the name the link document uses.
QString jsonTypeName(const QJsonValue& value)
{
    switch (value.type()) {
    case QJsonValue::Bool: return QStringLiteral("boolean");
    case QJsonValue::Double: return QStringLiteral("number");
    case QJsonValue::String: return QStringLiteral("string");
    case QJsonValue::Array: return QStringLiteral("array");
    case QJsonValue::Object: return QStringLiteral("object");
    case QJsonValue::Null: return QStringLiteral("null");
    case QJsonValue::Undefined: break;
    }
    return QStringLiteral("undefined");
}

QJsonObject captureMessageKinds()
{
    QJsonObject kinds;
    for (const SessionMessageKind kind : SessionMessages::allKinds()) {
        const QString name = QString::fromUtf8(SessionMessages::kindName(kind));
        QJsonObject entry;
        const std::optional<SessionMessage> sample = sampleMessage(kind);
        SessionMessage decoded;
        const QByteArray wire = sample ? SessionMessages::encode(*sample) : QByteArray();
        if (!sample || wire.isEmpty() || !SessionMessages::decode(wire, &decoded)) {
            entry.insert(QStringLiteral("error"), QStringLiteral("no well-formed sample message"));
            kinds.insert(name.isEmpty() ? QStringLiteral("<unnamed>") : name, entry);
            continue;
        }
        const QJsonObject full = QJsonDocument::fromJson(wire).object();
        QStringList required;
        QStringList optional;
        for (auto it = full.constBegin(); it != full.constEnd(); ++it) {
            QJsonObject without = full;
            without.remove(it.key());
            SessionMessage probe;
            const bool decodes = SessionMessages::decode(
                QJsonDocument(without).toJson(QJsonDocument::Compact), &probe);
            (decodes ? optional : required).append(it.key());
        }
        entry.insert(QStringLiteral("required"), sortedArray(required));
        entry.insert(QStringLiteral("optional"), sortedArray(optional));
        // Each key's JSON type, as the encoder writes it.
        QJsonObject types;
        for (auto it = full.constBegin(); it != full.constEnd(); ++it) {
            types.insert(it.key(), jsonTypeName(it.value()));
        }
        entry.insert(QStringLiteral("types"), types);
        kinds.insert(name, entry);
    }
    return kinds;
}

// ── capabilities ─────────────────────────────────────────────────────────

// ── A live station session ───────────────────────────────────────────────

// Every message a real StationServer sends a peer at this build's minor,
// through snapshot.complete, read off the wire. The station model is
// tst_station_session's (makeStationRadioModel) set up as DaemonApp sets up a
// Core, so every object a Core can offer is there: the accessory identity
// (enableStationAccessoryIdentity, for `amplifier` and `rfkit`), the station
// TCI server (enableStationTci, for `stationTci`; it stays off, so nothing
// listens) and a step attenuator controller (for `stepAtt`; `alexAntennas`
// and `ioBoard` bind to a Local model's own controllers). One panadapter as
// well: StationServer watches every pan the model holds, though nereusd
// itself adds none. `configure` runs on the server before the peer connects.
// nullopt when the session does not complete; `error` then says why.
std::optional<QList<QByteArray>> liveSessionWire(
    const std::function<void(StationServer&)>& configure, QString* error,
    bool alexBoard = false)
{
    QTemporaryDir dir;
    if (!dir.isValid()) {
        *error = QStringLiteral("no scratch directory");
        return std::nullopt;
    }
    AppSettings stationSettings(dir.filePath(QStringLiteral("NereusSDR.settings")));
    const QString oldVersion = QCoreApplication::applicationVersion();
    const QString oldTag = BuildIdentity::buildTag();
    const auto restoreIdentity = qScopeGuard([&]() {
        QCoreApplication::setApplicationVersion(oldVersion);
        BuildIdentity::setBuildTag(oldTag);
    });
    QCoreApplication::setApplicationVersion(QStringLiteral("0.5.2"));
    BuildIdentity::setBuildTag(QStringLiteral("link-surface@fixture"));

    // Declared before the model so it outlives it.
    auto stepAtt = std::make_unique<StepAttenuatorController>();
    stepAtt->setTickTimerEnabled(false);
    auto model = std::make_unique<RadioModel>();
    const auto unbind = qScopeGuard([&model] { model->setStepAttController(nullptr); });
    model->enableStationAccessoryIdentity();
    model->enableStationTci(QStringLiteral("127.0.0.1"));
    model->setStepAttController(stepAtt.get());
    model->setBoardForTest(alexBoard ? HPSDRHW::Hermes : HPSDRHW::HermesLite);
    RadioInfo info;
    info.macAddress = QStringLiteral("AA:BB:CC:DD:EE:01");
    info.name = QStringLiteral("Link surface");
    info.boardType = alexBoard ? HPSDRHW::Hermes : HPSDRHW::HermesLite;
    model->setLastRadioInfoForTest(info);
    model->setConnectionStateForTest(ConnectionState::Connected);
    model->addSlice(QStringLiteral("pan-0"));
    // iPhone app Task 73: a second slice the Core runs for another device
    // that is away, so the session this peer sees holds a `marker:<id>` as
    // well as its own `slice:<id>`.
    const int held = model->addSlice(QStringLiteral("pan-0"));
    model->sliceOwnership()->hold(held, QByteArray(32, '\x5a'));
    model->addPanadapter();


    // Declared before the server so it outlives it; the server owns the
    // station end once it accepts it.
    auto clientEnd = std::make_unique<LoopbackTransport>(QStringLiteral("link-surface-client"));
    StationServer server(model.get(), stationSettings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
    const ClientDeviceIdentity device = ClientDeviceIdentity::loadOrCreate(dir.path());
    if (!device.isValid()) {
        *error = QStringLiteral("no paired client key");
        return std::nullopt;
    }
    PairedDevice record;
    record.id = device.fingerprint();
    record.publicKeySpki = device.publicKeySpki();
    record.name = QStringLiteral("link-surface");
    record.kind = QStringLiteral("computer");
    if (!server.deviceStore()->add(record)) {
        *error = QStringLiteral("could not enrol link-surface client");
        return std::nullopt;
    }
    if (configure) {
        configure(server);
    }
    auto* stationEnd = new LoopbackTransport(QStringLiteral("link-surface-station"), &server);
    stationEnd->linkTo(clientEnd.get());
    server.acceptTransport(stationEnd);
    const QDeadlineTimer helloDeadline(5000);
    while (clientEnd->received().isEmpty() && !helloDeadline.hasExpired()) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    if (clientEnd->received().isEmpty()) {
        *error = QStringLiteral("station hello did not arrive");
        return std::nullopt;
    }
    SessionMessage hello;
    if (!SessionMessages::decode(clientEnd->received().first(), &hello)
        || hello.kind != SessionMessageKind::Hello) {
        *error = QStringLiteral("station hello was invalid");
        return std::nullopt;
    }
    QString fingerprint = server.certificateFingerprint();
    fingerprint.remove(QLatin1Char(':'));
    const QByteArray certificate = QByteArray::fromHex(fingerprint.toLatin1());
    const QByteArray challenge = StationIdentity::fromBase64Url(hello.challenge);
    const QByteArray signature = device.sign(DeviceAuthenticator::transcript(
        challenge, certificate, server.stationIdentity().publicKeySpki(), device.publicKeySpki()));
    const SessionDeviceBlock block{
        StationIdentity::toBase64Url(device.fingerprint()),
        StationIdentity::toBase64Url(device.publicKeySpki()),
        QStringLiteral("link-surface"), QStringLiteral("computer"),
        StationIdentity::toBase64Url(signature), {}};
    // Declaring deviceAuth, as a device that signs in by key does, so the
    // `devices` object (iPhone app Task 13) is among what the Core sends;
    // and sessionHolder (iPhone app Task 71), so `connectedDevices` and
    // sessionHolderVersion are too; and remoteTx (iPhone app plan Task 34),
    // so remoteTxVersion is; and vax (iPhone app plan Task 25), so the
    // `vax` object and vaxVersion are; and band2m (R-IOS-26), so
    // band2mVersion is; and audioQuality (iPhone app plan Task 23), so
    // audioQualityVersion is; and stationTciSettings (JJ's ruling of
    // 2026-09-28), so stationTciSettingsVersion and the rest of
    // `stationTci`'s settings are.
    clientEnd->sendText(SessionMessages::encode(SessionMessages::hello(
        kSessionProtocolMajor, kSessionProtocolMinor, 0, QStringLiteral("link-surface"),
        {kSessionProtocolMajor}, {{"deviceAuth", 1}, {"sessionHolder", 1}, {"remoteTx", 1},
                                  {"settingsHygiene", 2}, {"coreBuildInfo", 1},
                                  {"settingsBackup", 1},
                                  {"setupDescription", 1}, {"miniDisplay", 1},
                                  {"radioAntennaRows", 1},
                                  // iPhone app plan Task 25: the `vax` object.
                                  {"vax", 1}, {"txEqCurve", 2}, {"band2m", 1},
                                  // Phone wire batch: each slice's
                                  // diversityPattern.
                                  {"diversityPattern", 1},
                                  // Phone wire batch: radio's
                                  // logCategoryList.
                                  {"logCategoryList", 1},
                                  // HL2 port part 2: radio's
                                  // txInhibitReason.
                                  {"txInhibitReason", 1},
                                  // Phone wire batch: stationRadios' model
                                  // labels and choices.
                                  {"radioModels", 1},
                                  // The phone's direct addresses: devices'
                                  // coreAddresses (the capture signs in
                                  // with its own key).
                                  {"coreAddresses", 1},
                                  {"audioQuality", 1}, {"stationTciSettings", 1},
                                  // R-R3-46 / R-R3-11: stepAtt's other ADC.
                                  {"adcAttenuators", 1},
                                  // R-R3-49 / R-IOS-18: the `paProfiles` object.
                                  {"paProfiles", 1},
                                  // RADE status: each slice's radeSynced
                                  // and radeFreqOffsetHz.
                                  {"radeStatus", 1},
                                  // The Alex-1 low-pass in use: radio's
                                  // alexLpfBits.
                                  {"alexLpf", 1},
                                  // PA on-air gate re-review: radio's
                                  // paTransmitBand.
                                  {"paTransmitBand", 1},
                                  // Level Cal: radio's levelCal* run
                                  // progress.
                                  {"levelCalibration", 1},
                                  // Slice control plan Task 4: SliceAccess
                                  // and the slice.* access verbs; take-over
                                  // parity: 2, Take it back on
                                  // controlTaken; core-slice take-over:
                                  // 3, the Core's own slice.
                                  {"sliceAccess", 3},
                                  // The direct media ladder:
                                  // mediaDirectVersion and mediaStunUrls.
                                  {"mediaDirect", 1},
                                  // Level Cal 2: rx2AttenuatorVersion,
                                  // the catalogue's RX2 input control.
                                  {"rx2Attenuator", 1},
                                  // Radio codec lane: radioMicVersion,
                                  // the catalogue's radio mic keys.
                                  {"radioMic", 2},
                                  // Shared-input filters, ruling (d):
                                  // rxFilterLowPassVersion and radio's
                                  // rxFilter0LowPass fields.
                                  {"rxFilterLowPass", 1},
                                  // RADE reason: radeReasonVersion and
                                  // each slice's radeReason.
                                  {"radeReason", 1}})));
    clientEnd->sendText(SessionMessages::encode(SessionMessages::authRequest({}, block)));

    // The loopback delivers on later event-loop turns, as a socket would.
    const QDeadlineTimer deadline(10000);
    while (!clientEnd->receivedKinds().contains(QByteArrayLiteral("snapshot.complete"))
           && !deadline.hasExpired()) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    if (!clientEnd->receivedKinds().contains(QByteArrayLiteral("snapshot.complete"))) {
        *error = QStringLiteral("the station snapshot did not complete");
        return std::nullopt;
    }
    return clientEnd->received();
}

QJsonArray captureCapabilities()
{
    // Every conditional block toUpdates() has switched on: a usable display
    // budget with its reason, and the R-R3-46 radio identity block. This
    // fixes the order and the wire kinds.
    StationCapabilities caps;
    caps.remoteDisplayBudgetVersion = 1;
    caps.displayBudget = DisplayBudgetLimits{1, 1, 1};
    caps.displayBudgetReason = DisplayBudgetReason::CoreBusy;
    caps.radioIdentityEntries = true;
    // iPhone app Task 71: sent to a peer that declared sessionHolder.
    caps.sessionHolderEntry = true;
    // iPhone app plan Task 34: sent to a peer that declared remoteTx.
    caps.remoteTxEntry = true;
    caps.settingsHygieneVersion = 2;
    caps.coreBuildInfo = CoreBuildInfo{QStringLiteral("0.5.2"), QStringLiteral("link-surface@fixture")};
    caps.settingsBackupVersion = 1;
    caps.remoteIqVersion = 1;
    caps.setupDescriptionVersion = 1;
    caps.miniDisplayVersion = 1;
    caps.radioAntennaRowsVersion = 1;
    // iPhone app plan Task 25: sent to a peer that declared vax.
    caps.vaxEntry = true;
    // R-IOS-13 / R-R3-49: sent to a peer that declared txEqCurve; 2 with
    // the curve verbs.
    caps.txEqCurveVersion = 2;
    // R-IOS-26 / R-R3-49: sent to a peer that declared band2m.
    caps.band2mVersion = 1;
    // Phone wire batch: sent to a peer that declared diversityPattern.
    caps.diversityPatternVersion = 1;
    // Phone wire batch: sent to a peer that declared logCategoryList.
    caps.logCategoryListVersion = 1;
    // Phone wire batch: sent to a peer that declared radioModels.
    caps.radioModelsEntry = true;
    // The phone's direct addresses: sent to a device signed in with its own
    // key that declared coreAddresses.
    caps.coreAddressesVersion = 1;
    // iPhone app plan Task 23: sent to a peer that declared audioQuality.
    caps.audioQualityVersion = 1;
    // JJ's ruling of 2026-09-28: sent to a peer that declared
    // stationTciSettings.
    caps.stationTciSettingsVersion = 1;
    // R-R3-46 / R-R3-11: sent to a peer that declared adcAttenuators.
    caps.adcAttenuatorVersion = 1;
    // R-R3-49 / R-IOS-18: sent to a peer that declared paProfiles.
    caps.paProfileVersion = 1;
    // RADE status: sent to a peer that declared radeStatus.
    caps.radeStatusVersion = 1;
    // HL2 port part 2: sent to a peer that declared txInhibitReason.
    caps.txInhibitReasonVersion = 1;
    // PA on-air gate re-review: sent to a peer that declared paTransmitBand.
    caps.paTransmitBandVersion = 1;
    // Slice control plan Task 4: sent to a peer that declared sliceAccess.
    caps.sliceAccessEntry = true;
    // The direct media ladder: sent to a peer with media that declared
    // mediaDirect.
    caps.mediaDirectVersion = 1;
    caps.mediaStunUrls = {QStringLiteral("stun:stun.example.test:3478")};
    // Level Cal 2: sent to a peer that declared rx2Attenuator.
    caps.rx2AttenuatorVersion = 1;
    // Radio codec lane: sent to a peer that declared radioMic.
    caps.radioMicVersion = 2;
    // Shared-input filters, ruling (d): sent to a peer that declared
    // rxFilterLowPass.
    caps.rxFilterLowPassVersion = 1;
    // RADE reason: sent to a peer that declared radeReason.
    caps.radeReasonVersion = 1;

    // The values come from a live station with every feature a Core can
    // switch on: media, telemetry, an enforced display budget with its
    // reason, the accessories, the station TCI server and the radio
    // hardware objects (liveSessionWire). They are what
    // StationServer::buildCapabilities() emits, so the per-feature versions
    // the link document's section 6.3 renders are pinned here and a version
    // bump shows as surface drift. The budget numbers are this capture's
    // own, not a Core's computed ones.
    QString error;
    const std::optional<QList<QByteArray>> wire = liveSessionWire(
        [](StationServer& server) {
            server.setMediaEnabled(true);
            server.setTelemetryEnabled(true);
            server.setDisplayBudgetEnforcementEnabled(true);
            server.setDisplayBudgetLimits(DisplayBudgetLimits{1, 1, 1},
                                          DisplayBudgetReason::CoreBusy);
            // The direct media ladder: the Core's STUN, as its rendezvous
            // hello names it (a reserved example name, never a real one).
            server.setMediaStun({QStringLiteral("stun:stun.example.test:3478")});
        },
        &error);
    QHash<QString, QJsonObject> live;
    if (wire) {
        for (const QByteArray& message : *wire) {
            const QJsonObject o = QJsonDocument::fromJson(message).object();
            if (o.value(QStringLiteral("type")).toString() != QStringLiteral("capabilities")) {
                continue;
            }
            for (const QJsonValue& entry : o.value(QStringLiteral("properties")).toArray()) {
                const QJsonObject e = entry.toObject();
                live.insert(e.value(QStringLiteral("name")).toString(), e);
            }
            break;
        }
    }
    // The default object-key fixture is an HL2, which has no Alex filter
    // board. Capture the optional radio-bound edit offer from an otherwise
    // identical connected Hermes Core instead of inventing a value.
    QString alexError;
    const auto alexWire = liveSessionWire({}, &alexError, true);
    if (alexWire) {
        for (const QByteArray& message : *alexWire) {
            const QJsonObject o = QJsonDocument::fromJson(message).object();
            if (o.value(QStringLiteral("type")) != QStringLiteral("capabilities")) {
                continue;
            }
            for (const QJsonValue& entry : o.value(QStringLiteral("properties")).toArray()) {
                const QJsonObject e = entry.toObject();
                if (e.value(QStringLiteral("name")) == QStringLiteral("radioAntennaRowsVersion")) {
                    live.insert(QStringLiteral("radioAntennaRowsVersion"), e);
                }
            }
            break;
        }
    }

    QJsonArray entries;
    for (const MirrorUpdate& u : caps.toUpdates()) {
        const QString name = QString::fromUtf8(u.name);
        QJsonObject entry{{QStringLiteral("name"), name},
                          {QStringLiteral("kind"), wireKind(u.kind)}};
        const auto found = live.constFind(name);
        if (!wire) {
            entry.insert(QStringLiteral("error"), error);
        } else if (found == live.cend()) {
            entry.insert(QStringLiteral("error"),
                         QStringLiteral("the live station did not send this capability"));
        } else if (found->value(QStringLiteral("kind")).toString() != wireKind(u.kind)) {
            entry.insert(QStringLiteral("error"),
                         QStringLiteral("the live station sent another wire kind"));
        } else {
            entry.insert(QStringLiteral("value"), found->value(QStringLiteral("value")));
        }
        entries.append(entry);
    }
    return entries;
}

// ── mirrorClasses ────────────────────────────────────────────────────────

QString directionName(MirrorDirection direction)
{
    switch (direction) {
    case MirrorDirection::Outbound: return QStringLiteral("outbound");
    case MirrorDirection::Bidirectional: return QStringLiteral("bidirectional");
    case MirrorDirection::ConstantSnapshot: return QStringLiteral("constantSnapshot");
    }
    return QString();
}

QJsonObject captureMirrorClasses()
{
    QJsonObject classes;
    for (const QMetaObject* mo : LinkSurface::mirroredMetaObjects()) {
        const MirrorSchema& schema = MirrorSchema::forMetaObject(mo);
        const QByteArray shortName = MirrorSchema::shortClassName(schema.className());
        QJsonArray properties;
        for (const MirrorProperty& prop : schema.properties()) {
            // What a schema message carries (StateMirror.cpp
            // schemaFieldsFor): a property with no wire kind is not sent.
            if (prop.kind == MirrorWireKind::Unsupported) {
                continue;
            }
            QJsonObject entry{
                {QStringLiteral("ordinal"), prop.ordinal},
                {QStringLiteral("name"), QString::fromUtf8(prop.name)},
                {QStringLiteral("kind"), wireKind(prop.kind)},
                {QStringLiteral("direction"),
                 directionName(MirrorPolicy::directionFor(shortName, prop.name))},
            };
            if (MirrorEnumDomain::hasDomain(prop.metaType)) {
                QJsonArray values;
                for (const qlonglong value : MirrorEnumDomain::valuesFor(prop.metaType)) {
                    values.append(static_cast<qint64>(value));
                }
                entry.insert(QStringLiteral("enumValues"), values);
            }
            properties.append(entry);
        }
        classes.insert(QString::fromUtf8(shortName),
                       QJsonObject{{QStringLiteral("properties"), properties}});
    }
    return classes;
}

// ── objectKeys ───────────────────────────────────────────────────────────

// The keys a station's connect-time snapshot creates (liveSessionWire), as
// patterns, so every minor-gated object is sent.
QJsonArray captureObjectKeys()
{
    QJsonArray keys;
    QString error;
    const std::optional<QList<QByteArray>> wire = liveSessionWire({}, &error);
    if (!wire) {
        keys.append(QJsonObject{{QStringLiteral("error"), error}});
        return keys;
    }

    static const QRegularExpression kPan(QStringLiteral("^pan:[0-9]+$"));
    static const QRegularExpression kSlice(QStringLiteral("^slice:[0-9]+$"));
    static const QRegularExpression kMarker(QStringLiteral("^marker:[0-9]+$"));
    static const QRegularExpression kAccess(QStringLiteral("^access:[0-9]+$"));
    QSet<QString> seen;
    for (const QByteArray& message : *wire) {
        SessionMessage decoded;
        if (!SessionMessages::decode(message, &decoded)
            || decoded.kind != SessionMessageKind::ObjectCreate) {
            continue;
        }
        QString pattern = QString::fromUtf8(decoded.objectKey);
        if (kPan.match(pattern).hasMatch()) {
            pattern = QStringLiteral("pan:<i>");
        } else if (kSlice.match(pattern).hasMatch()) {
            pattern = QStringLiteral("slice:<id>");
        } else if (kMarker.match(pattern).hasMatch()) {
            pattern = QStringLiteral("marker:<id>");
        } else if (kAccess.match(pattern).hasMatch()) {
            pattern = QStringLiteral("access:<id>");
        }
        if (seen.contains(pattern)) {
            continue;
        }
        seen.insert(pattern);
        keys.append(QJsonObject{{QStringLiteral("key"), pattern},
                                {QStringLiteral("class"), QString::fromUtf8(decoded.className)}});
    }
    return keys;
}

// ── commands ─────────────────────────────────────────────────────────────

QJsonArray captureCommands()
{
    QJsonArray commands;
    for (const CommandVerbSpec& spec : SessionCommandDispatcher::verbSpecs()) {
        QJsonArray arguments;
        for (const CommandArgumentSpec& argument : spec.arguments) {
            arguments.append(QJsonObject{{QStringLiteral("name"), QString::fromUtf8(argument.name)},
                                         {QStringLiteral("kind"), wireKind(argument.kind)},
                                         {QStringLiteral("optional"), argument.optional}});
        }
        commands.append(QJsonObject{
            {QStringLiteral("verb"), QString::fromUtf8(spec.verb)},
            {QStringLiteral("arguments"), arguments},
            {QStringLiteral("capability"), QString::fromUtf8(spec.capability)},
            {QStringLiteral("capabilityVersion"), spec.capabilityVersion},
            {QStringLiteral("minMinor"), static_cast<int>(spec.minMinor)},
        });
    }
    // StationServer handles this read-only, session-bound family before the
    // generic dispatcher. It still belongs to the published command surface.
    const auto exportCommand = [&commands](const QString& verb, const QJsonArray& arguments) {
        commands.append(QJsonObject{{QStringLiteral("verb"), verb},
                                    {QStringLiteral("arguments"), arguments},
                                    {QStringLiteral("capability"), QStringLiteral("settingsBackupVersion")},
                                    {QStringLiteral("capabilityVersion"), 1},
                                    {QStringLiteral("minMinor"), 11}});
    };
    const auto arg = [](const QString& name, const QString& kind) {
        return QJsonObject{{QStringLiteral("name"), name},
                           {QStringLiteral("kind"), kind},
                           {QStringLiteral("optional"), false}};
    };
    exportCommand(QStringLiteral("station.settingsExport.begin"), {});
    exportCommand(QStringLiteral("station.settingsExport.read"),
                  {arg(QStringLiteral("transferId"), QStringLiteral("utf8")),
                   arg(QStringLiteral("offset"), QStringLiteral("i64"))});
    exportCommand(QStringLiteral("station.settingsExport.cancel"),
                  {arg(QStringLiteral("transferId"), QStringLiteral("utf8"))});
    return commands;
}

// ── settingsScope ────────────────────────────────────────────────────────

QJsonObject captureSettingsScope()
{
    QJsonArray exceptions;
    QStringList stationPrefixes;
    QStringList operatorLocalPrefixes;
    QStringList stationKeys;
    QStringList operatorLocalKeys;
    for (const SettingsScopeRule& rule : settingsScopeRules()) {
        const bool station = rule.scope == SettingsScope::Station;
        switch (rule.kind) {
        case SettingsScopeRuleKind::Exception:
            exceptions.append(QJsonObject{
                {QStringLiteral("key"), rule.text},
                {QStringLiteral("scope"),
                 station ? QStringLiteral("station") : QStringLiteral("operatorLocal")}});
            break;
        case SettingsScopeRuleKind::Prefix:
            (station ? stationPrefixes : operatorLocalPrefixes).append(rule.text);
            break;
        case SettingsScopeRuleKind::WholeKey:
            (station ? stationKeys : operatorLocalKeys).append(rule.text);
            break;
        }
    }
    // Table order, not sorted: classifySettingsKey() walks each table in
    // this order and the first match wins.
    return QJsonObject{
        {QStringLiteral("exceptions"), exceptions},
        {QStringLiteral("stationPrefixes"), QJsonArray::fromStringList(stationPrefixes)},
        {QStringLiteral("operatorLocalPrefixes"), QJsonArray::fromStringList(operatorLocalPrefixes)},
        {QStringLiteral("stationKeys"), QJsonArray::fromStringList(stationKeys)},
        {QStringLiteral("operatorLocalKeys"), QJsonArray::fromStringList(operatorLocalKeys)},
    };
}

// ── telemetry ────────────────────────────────────────────────────────────

// Version 1: radio and audio. 2: adds host. 3: adds receivers. 4: adds the
// radio's PA readings and link quality in the radio section; 5 the Core's
// HL2 link (hl2*, remote-window parity Task 14); 6 adds radio diagnostics.
// (StationCapabilities.h stationTelemetryVersion; StationServer::
// sendTelemetry strips host below kCoreHostTelemetrySessionProtocolMinor and
// receivers below kReceiverLoadSessionProtocolMinor). Every optional field
// is set, so the paths are the whole field set of that version.
StationTelemetrySnapshot sampleTelemetry(int version)
{
    StationTelemetrySnapshot s;
    s.sequence = 1;
    s.sampledElapsedMs = 1;
    s.radio.connected = true;
    s.radio.rxMbps = 1.0;
    s.radio.txMbps = 1.0;
    s.radio.rttMs = 1;
    s.radio.rttAgeMs = 1;
    s.audio.active = true;
    s.audio.contextGeneration = 1;
    s.audio.sourceFramesPerSecond = 1.0;
    s.audio.sourceDropsPerSecond = 1.0;
    s.audio.encodedPacketsPerSecond = 1.0;
    s.audio.encodeFailuresPerSecond = 1.0;
    s.audio.sendAcceptedPerSecond = 1.0;
    s.audio.sendRejectedPerSecond = 1.0;
    if (version >= 2) {
        s.host.systemCpuPercent = 1.0;
        s.host.processCpuPercent = 1.0;
        s.host.memoryAvailableKiB = 1;
        s.host.memoryTotalKiB = 1;
        s.host.processResidentKiB = 1;
        s.host.hottestZoneCelsius = 1.0;
        s.host.hottestZoneName = QStringLiteral("cpu");
    }
    if (version >= 3) {
        StationReceiverTelemetry receiver;
        receiver.sliceId = 0;
        receiver.loadPercent = 1.0;
        receiver.inputDelayMs = 1;
        receiver.skippedInputMs = 1;
        s.receivers = QVector<StationReceiverTelemetry>{receiver};
    }
    if (version >= 4) {
        s.radio.paVolts = 1.0;
        s.radio.supplyVolts = 1.0;
        s.radio.paCurrentAmps = 1.0;
        s.radio.paTemperatureCelsius = 1.0;
        s.radio.packetLossPercent = 1.0;
        s.radio.jitterMs = 1.0;
        s.radio.packetGapMs = 1.0;
        s.radio.sampleRateHz = 1;
        s.radio.udpPacketsSeen = 1;
    }
    if (version >= 5) {
        s.radio.hl2RxBytesPerSecond = 1.0;
        s.radio.hl2TxBytesPerSecond = 1.0;
        s.radio.hl2Throttled = false;
        s.radio.hl2SequenceGaps = 1;
    }
    if (version >= 6) {
        s.radio.connectionAgeMs = 1;
        s.radio.radioUdpBasePort = 1024;
        s.radio.adcOverloads = QVector<StationAdcOverloadTelemetry>{
            {0, 1, 1, true, 1}};
    }
    return s;
}

void flattenPaths(const QString& prefix, const QJsonValue& value, QStringList* out)
{
    if (value.isObject()) {
        const QJsonObject object = value.toObject();
        for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
            flattenPaths(prefix.isEmpty() ? it.key() : prefix + QLatin1Char('.') + it.key(),
                         it.value(), out);
        }
        return;
    }
    if (value.isArray()) {
        const QJsonArray array = value.toArray();
        for (const QJsonValue& element : array) {
            flattenPaths(prefix + QStringLiteral("[]"), element, out);
        }
        return;
    }
    out->append(prefix);
}

QJsonObject captureTelemetry()
{
    // Versions 4 (remote-window parity Task 6), 5 (Task 14), and 6 need minor
    // 11, as 3 does.
    const quint16 minors[] = {kStationTelemetrySessionProtocolMinor,
                              kCoreHostTelemetrySessionProtocolMinor,
                              kReceiverLoadSessionProtocolMinor,
                              kReceiverLoadSessionProtocolMinor,
                              kReceiverLoadSessionProtocolMinor,
                              kReceiverLoadSessionProtocolMinor};
    QJsonArray versions;
    for (int version = 1; version <= 6; ++version) {
        const std::optional<QJsonObject> payload =
            StationTelemetryCodec::encode(sampleTelemetry(version));
        QStringList paths;
        if (payload) {
            flattenPaths(QString(), *payload, &paths);
        }
        versions.append(QJsonObject{
            {QStringLiteral("version"), version},
            {QStringLiteral("minMinor"), static_cast<int>(minors[version - 1])},
            {QStringLiteral("fields"), sortedArray(paths)},
        });
    }
    return QJsonObject{
        {QStringLiteral("kind"),
         QString::fromUtf8(SessionMessages::kindName(SessionMessageKind::StationTelemetry))},
        {QStringLiteral("versions"), versions},
    };
}

// ── mediaControl ─────────────────────────────────────────────────────────
//
// Each operation: `capability` is the StationCapabilities entry that
// enables it (remoteMediaVersion for the base operations), `fields` the
// keys it always carries, `conditional` each key it carries only in some
// states with the states that bring it, `nested` the key sets of object
// valued fields.

struct OpShape {
    QStringList tags;
    QJsonObject payload;
};

QJsonObject describeShapes(const QString& capability, const QList<OpShape>& shapes)
{
    QSet<QString> common;
    bool first = true;
    QHash<QString, QSet<QString>> tagsWhere;   // field -> tags common to every shape carrying it
    QHash<QString, bool> tagsSeeded;
    QHash<QString, QSet<QStringList>> nestedKeys;
    for (const OpShape& shape : shapes) {
        QSet<QString> keys;
        for (auto it = shape.payload.constBegin(); it != shape.payload.constEnd(); ++it) {
            keys.insert(it.key());
            const QSet<QString> tags(shape.tags.cbegin(), shape.tags.cend());
            if (!tagsSeeded.value(it.key())) {
                tagsWhere.insert(it.key(), tags);
                tagsSeeded.insert(it.key(), true);
            } else {
                tagsWhere[it.key()].intersect(tags);
            }
            if (it.value().isObject()) {
                QStringList nested = it.value().toObject().keys();
                nested.sort();
                nestedKeys[it.key()].insert(nested);
            }
        }
        if (first) {
            common = keys;
            first = false;
        } else {
            common.intersect(keys);
        }
    }
    QStringList fields(common.cbegin(), common.cend());
    QJsonObject conditional;
    for (auto it = tagsWhere.constBegin(); it != tagsWhere.constEnd(); ++it) {
        if (common.contains(it.key())) {
            continue;
        }
        QStringList tags(it.value().cbegin(), it.value().cend());
        conditional.insert(it.key(), sortedArray(tags));
    }
    QJsonObject nested;
    for (auto it = nestedKeys.constBegin(); it != nestedKeys.constEnd(); ++it) {
        QList<QStringList> sets(it.value().cbegin(), it.value().cend());
        std::sort(sets.begin(), sets.end());
        QJsonArray variants;
        for (const QStringList& set : sets) {
            variants.append(QJsonArray::fromStringList(set));
        }
        nested.insert(it.key(), variants);
    }
    QJsonObject op{{QStringLiteral("capability"), capability},
                   {QStringLiteral("fields"), sortedArray(fields)}};
    if (!conditional.isEmpty()) {
        op.insert(QStringLiteral("conditional"), conditional);
    }
    if (!nested.isEmpty()) {
        op.insert(QStringLiteral("nested"), nested);
    }
    return op;
}

// A declared operation: its always-present fields, and each conditional
// field with the capability or state that brings it.
QJsonObject declaredOp(const QString& capability, const QStringList& fields,
                       const QList<QPair<QString, QStringList>>& conditional = {},
                       const QList<QPair<QString, QStringList>>& nested = {})
{
    QJsonObject op{{QStringLiteral("capability"), capability},
                   {QStringLiteral("fields"), sortedArray(fields)}};
    if (!conditional.isEmpty()) {
        QJsonObject map;
        for (const auto& [field, tags] : conditional) {
            map.insert(field, sortedArray(tags));
        }
        op.insert(QStringLiteral("conditional"), map);
    }
    if (!nested.isEmpty()) {
        QJsonObject map;
        for (const auto& [field, keys] : nested) {
            map.insert(field, QJsonArray{sortedArray(keys)});
        }
        op.insert(QStringLiteral("nested"), map);
    }
    return op;
}

const QString kMedia = QStringLiteral("remoteMediaVersion");

QJsonObject guiToCoreOps()
{
    const QStringList peer{QStringLiteral("op"), QStringLiteral("connectionId")};
    const QStringList plane{QStringLiteral("detector"), QStringLiteral("averageMode"),
                            QStringLiteral("averageAlpha")};
    QJsonObject ops;
    // DaemonMediaController.cpp handleStart: exactKeys {"op","connectionId"}
    // once each declared version key is removed.
    ops.insert(QStringLiteral("start"), declaredOp(kMedia, peer, {
        {QStringLiteral("miniDisplayVersion"), {QStringLiteral("miniDisplayVersion")}},
        {QStringLiteral("audioProfileVersion"), {QStringLiteral("audioProfileVersion")}},
        {QStringLiteral("receiverAudioVersion"), {QStringLiteral("receiverAudioVersion")}},
        {QStringLiteral("remoteIqVersion"), {QStringLiteral("remoteIqVersion")}},
        {QStringLiteral("headphonesMixVersion"), {QStringLiteral("headphonesMixVersion")}},
        // iPhone app plan Task 36: the microphone line.
        {QStringLiteral("remoteTxVersion"), {QStringLiteral("remoteTxVersion")}},
        // Remote-window parity Task 32: the transmit monitor.
        {QStringLiteral("txMonitorAudioVersion"), {QStringLiteral("txMonitorAudioVersion")}},
        // iPhone app plan Task 29 step 2b: the media tunnel.
        {QStringLiteral("mediaTunnelVersion"), {QStringLiteral("mediaTunnelVersion")}},
        {QStringLiteral("mediaRelayRoutingVersion"),
         {QStringLiteral("mediaRelayRoutingVersion")}},
    }));
    // MediaPeer.cpp acceptControl: hasExactKeys for description / candidate.
    ops.insert(QStringLiteral("description"),
               declaredOp(kMedia, peer + QStringList{QStringLiteral("sdp"), QStringLiteral("type")}));
    ops.insert(QStringLiteral("candidate"),
               declaredOp(kMedia, peer + QStringList{QStringLiteral("candidate"), QStringLiteral("mid")}));
    // DaemonMediaController.cpp handleSubscribe (extendedView, decimation and the
    // display extras fields removed before exactKeys), parsePlane, and
    // DisplayExtras.cpp parseDisplayExtrasRequest (iPhone app Task 20).
    const QString extras = QStringLiteral("displayExtrasVersion");
    ops.insert(QStringLiteral("subscribe"), declaredOp(kMedia,
        peer + QStringList{QStringLiteral("endpointId"), QStringLiteral("revision"),
                           QStringLiteral("sliceId"), QStringLiteral("tier"),
                           QStringLiteral("fftSize"), QStringLiteral("windowType"),
                           QStringLiteral("centreHz"), QStringLiteral("spanHz"),
                           QStringLiteral("pixels"), QStringLiteral("fps"),
                           QStringLiteral("framesPerLine"), QStringLiteral("trace"),
                           QStringLiteral("waterfall"), QStringLiteral("minDbm"),
                           QStringLiteral("maxDbm"), QStringLiteral("wideSpanFactor")},
        {{QStringLiteral("displayRole"), {QStringLiteral("miniDisplayVersion")}},
         {QStringLiteral("extendedView"), {QStringLiteral("remoteWidebandDisplayVersion")}},
         // Parity Task 17: spectrumGrantVersion 2.
         {QStringLiteral("decimation"), {QStringLiteral("spectrumGrantVersion")}},
         {QStringLiteral("peakBlobs"), {extras}},
         {QStringLiteral("activePeakHold"), {extras}},
         {QStringLiteral("noiseFloor"), {extras}},
         {QStringLiteral("waterfallLevels"), {extras}},
         {QStringLiteral("normalize"), {extras}},
         {QStringLiteral("calibrationOffsetDb"), {extras}},
         {QStringLiteral("averageTimeMs"), {extras}},
         {QStringLiteral("waterfallAverageTimeMs"), {extras}}},
        {{QStringLiteral("trace"), plane}, {QStringLiteral("waterfall"), plane},
         {QStringLiteral("peakBlobs"),
          {QStringLiteral("count"), QStringLiteral("holdMs"),
           QStringLiteral("fallDbPerSec"), QStringLiteral("insideOnly")}},
         {QStringLiteral("activePeakHold"),
          {QStringLiteral("enabled"), QStringLiteral("holdMs"),
           QStringLiteral("fallDbPerSec"), QStringLiteral("onTx")}},
         {QStringLiteral("noiseFloor"),
          {QStringLiteral("enabled"), QStringLiteral("fastAttack"), QStringLiteral("shiftDb")}},
         {QStringLiteral("waterfallLevels"),
          {QStringLiteral("mode"), QStringLiteral("lowDbm"), QStringLiteral("highDbm"),
           QStringLiteral("offsetDb")}}}));
    // DaemonMediaController.cpp handleUnsubscribe: revision only with the
    // display budget.
    ops.insert(QStringLiteral("unsubscribe"),
               declaredOp(kMedia, peer + QStringList{QStringLiteral("endpointId")},
                          {{QStringLiteral("revision"), {QStringLiteral("remoteDisplayBudgetVersion")}}}));
    // DaemonMediaController.cpp handleKeyframe.
    ops.insert(QStringLiteral("keyframe"),
               declaredOp(kMedia, peer + QStringList{QStringLiteral("endpointId"),
                                                     QStringLiteral("contextGeneration")}));
    // DaemonMediaController.cpp handleClarityRetune (R-IOS-27, R-IOS-06):
    // displayExtrasVersion 2 (the Core now sends 4).
    ops.insert(QStringLiteral("clarity-retune"),
               declaredOp(extras, peer + QStringList{QStringLiteral("endpointId")}));
    // DaemonMediaController.cpp handleAudio: profile only from a GUI that
    // negotiated the audio detail and the Core's audio profiles; and
    // (iPhone app plan Task 23) opusBitrate, beside profile, only from a
    // device told audioQualityVersion.
    ops.insert(QStringLiteral("audio"),
               declaredOp(kMedia, peer + QStringList{QStringLiteral("revision"),
                                                     QStringLiteral("enabled")},
                          {{QStringLiteral("profile"), {QStringLiteral("remoteAudioStatusVersion"),
                                                        QStringLiteral("audioProfileVersion")}},
                           {QStringLiteral("opusBitrate"), {QStringLiteral("audioQualityVersion"),
                                                            QStringLiteral("audioProfileVersion")}}}));
    // DaemonMediaController.cpp handleReceiverAudio / handleHeadphonesAudio /
    // handleClockProbe.
    ops.insert(QStringLiteral("iq-stream"),
               declaredOp(QStringLiteral("remoteIqVersion"),
                          peer + QStringList{QStringLiteral("sliceId"), QStringLiteral("revision"),
                                             QStringLiteral("enabled")}));
    ops.insert(QStringLiteral("receiver-audio"),
               declaredOp(QStringLiteral("receiverAudioVersion"),
                          peer + QStringList{QStringLiteral("sliceId"), QStringLiteral("revision"),
                                             QStringLiteral("enabled"), QStringLiteral("profile")}));
    ops.insert(QStringLiteral("headphones-audio"),
               declaredOp(QStringLiteral("headphonesMixVersion"),
                          peer + QStringList{QStringLiteral("revision"), QStringLiteral("enabled"),
                                             QStringLiteral("profile")}));
    // DaemonMediaController.cpp handleMonitorAudio (parity Task 32).
    ops.insert(QStringLiteral("monitor-audio"),
               declaredOp(QStringLiteral("txMonitorAudioVersion"),
                          peer + QStringList{QStringLiteral("revision"), QStringLiteral("route")}));
    ops.insert(QStringLiteral("clock-probe"),
               declaredOp(QStringLiteral("audioClockVersion"),
                          peer + QStringList{QStringLiteral("id"), QStringLiteral("t0")}));
    // iPhone app plan Task 29: DaemonMediaController.cpp handleReplace.
    // The direct media ladder: mediaDirectVersion 1 asks for the
    // direct-only connection, only from a Core that sent mediaDirectVersion.
    ops.insert(QStringLiteral("replace"),
               declaredOp(QStringLiteral("mediaReplaceVersion"),
                          peer + QStringList{QStringLiteral("replaces")}, {
        {QStringLiteral("mediaDirectVersion"), {QStringLiteral("mediaDirectVersion")}},
    }));
    return ops;
}

QJsonObject spectrumContextShapes()
{
    SpectrumContextMessage message;
    message.connectionId = QStringLiteral("00000000-0000-0000-0000-000000000001");
    // The extended view: absent, present but unavailable, and available
    // (WidebandDisplayContext::toJson carries more keys when available).
    WidebandDisplayContext available;
    available.available = true;
    available.active = true;
    available.physicalAdcIndex = 0;
    available.filterChainIndex = 0;
    available.sourceGeneration = 1;
    available.adcRateHz = 122880000.0;
    const QList<std::optional<WidebandDisplayContext>> widebands{
        std::nullopt, WidebandDisplayContext{}, available};
    QList<OpShape> shapes;
    for (const std::optional<WidebandDisplayContext>& wideband : widebands) {
        for (const bool grant : {false, true}) {
            SpectrumContextMessage m = message;
            QStringList tags;
            if (wideband) {
                m.wideband = wideband;
                tags.append(QStringLiteral("remoteWidebandDisplayVersion"));
            }
            if (grant) {
                m.grant = SpectrumContextGrant{};
                tags.append(QStringLiteral("spectrumGrantVersion"));
            }
            shapes.append({tags, encodeRemoteSpectrumContext(m, grant)});
        }
    }
    return describeShapes(kMedia, shapes);
}

// The states an audio context can be encoded in: every combination of the
// profile, enabled and a profile refusal, each tagged with what brings it.
QList<RemoteAudioContextMessage> audioContextStates(QList<QStringList>* tags)
{
    QList<RemoteAudioContextMessage> states;
    for (const RemoteAudioProfile profile : {RemoteAudioProfile::Opus, RemoteAudioProfile::Lossless}) {
        for (const bool enabled : {true, false}) {
            for (const bool refused : {false, true}) {
                RemoteAudioContextMessage m;
                m.connectionId = QStringLiteral("00000000-0000-0000-0000-000000000001");
                m.revision = 1;
                m.generation = 1;
                m.enabled = enabled;
                m.ssrc = 1;
                m.profile = profile;
                if (enabled) {
                    m.encoder = OpusEncoderProfile{48000, 2, 1920, 1, 1};
                    m.losslessEncoder = l16EncoderProfile();
                } else {
                    m.offReason = RemoteAudioOffReason::ClientDisabled;
                }
                if (refused) {
                    m.profileRefusal = RemoteAudioProfileRefusal::NotAllowed;
                }
                QStringList t{enabled ? QStringLiteral("enabled=true")
                                      : QStringLiteral("enabled=false"),
                              profile == RemoteAudioProfile::Opus
                                  ? QStringLiteral("profile=opus")
                                  : QStringLiteral("profile=lossless")};
                if (refused) {
                    t.append(QStringLiteral("profileRefused"));
                }
                states.append(m);
                tags->append(t);
            }
        }
    }
    return states;
}

QJsonObject audioContextShapes()
{
    QList<QStringList> tags;
    const QList<RemoteAudioContextMessage> states = audioContextStates(&tags);
    QList<OpShape> shapes;
    for (qsizetype i = 0; i < states.size(); ++i) {
        // Minor 7: no detail. Minor 8: detail. With audioProfileVersion: the
        // profile shape (RemoteAudioContext.cpp encodeRemoteAudioContext).
        // Without the profile shape the Opus encoder is the only one.
        const bool opus = states[i].profile == RemoteAudioProfile::Opus;
        if (opus) {
            shapes.append({tags[i], encodeRemoteAudioContext(states[i], false, false)});
            shapes.append({tags[i] + QStringList{QStringLiteral("remoteAudioStatusVersion")},
                           encodeRemoteAudioContext(states[i], true, false)});
        }
        shapes.append({tags[i] + QStringList{QStringLiteral("remoteAudioStatusVersion"),
                                             QStringLiteral("audioProfileVersion")},
                       encodeRemoteAudioContext(states[i], true, true)});
        // iPhone app plan Task 23: a device's refused opusBitrate, in the
        // main context's profile shape only.
        if (opus) {
            RemoteAudioContextMessage refused = states[i];
            refused.opusBitrateRefusal = opusBitrateNotOfferedReason();
            shapes.append({tags[i] + QStringList{QStringLiteral("remoteAudioStatusVersion"),
                                                 QStringLiteral("audioProfileVersion"),
                                                 QStringLiteral("audioQualityVersion"),
                                                 QStringLiteral("opusBitrateRefused")},
                           encodeRemoteAudioContext(refused, true, true)});
        }
    }
    return describeShapes(kMedia, shapes);
}

QJsonObject receiverAudioContextShapes()
{
    QList<QStringList> tags;
    const QList<RemoteAudioContextMessage> states = audioContextStates(&tags);
    QList<OpShape> shapes;
    for (qsizetype i = 0; i < states.size(); ++i) {
        shapes.append({tags[i], encodeReceiverAudioContext({0, states[i]})});
    }
    return describeShapes(QStringLiteral("receiverAudioVersion"), shapes);
}

QJsonObject headphonesAudioContextShapes()
{
    QList<QStringList> tags;
    const QList<RemoteAudioContextMessage> states = audioContextStates(&tags);
    QList<OpShape> shapes;
    for (qsizetype i = 0; i < states.size(); ++i) {
        shapes.append({tags[i], encodeHeadphonesAudioContext(states[i])});
    }
    return describeShapes(QStringLiteral("headphonesMixVersion"), shapes);
}

QJsonObject coreToGuiOps()
{
    const QStringList peer{QStringLiteral("op"), QStringLiteral("connectionId")};
    QJsonObject ops;
    // MediaPeer.cpp: the local description and candidate controls.
    ops.insert(QStringLiteral("description"),
               declaredOp(kMedia, peer + QStringList{QStringLiteral("sdp"), QStringLiteral("type")}));
    ops.insert(QStringLiteral("candidate"),
               declaredOp(kMedia, peer + QStringList{QStringLiteral("candidate"), QStringLiteral("mid")}));
    ops.insert(QStringLiteral("context"), spectrumContextShapes());
    // DaemonMediaController.cpp sendRejected.
    ops.insert(QStringLiteral("rejected"),
               declaredOp(kMedia, peer + QStringList{QStringLiteral("endpointId"),
                                                     QStringLiteral("revision"),
                                                     QStringLiteral("reason")}));
    // DaemonMediaController.cpp sendAllocationResult.
    ops.insert(QStringLiteral("allocation-result"),
               declaredOp(QStringLiteral("remoteDisplayBudgetVersion"),
                          peer + QStringList{QStringLiteral("endpointId"),
                                             QStringLiteral("revision"),
                                             QStringLiteral("accepted"),
                                             QStringLiteral("reason"),
                                             QStringLiteral("budgetGeneration"),
                                             QStringLiteral("acceptedRevision"),
                                             QStringLiteral("applicationBytesPerSecond"),
                                             QStringLiteral("spectrumSampleUnitsPerSecond"),
                                             QStringLiteral("messagesPerSecond")}));
    // DaemonMediaController.cpp, the noise-floor message in the send tick.
    ops.insert(QStringLiteral("noise-floor"),
               declaredOp(kMedia, peer + QStringList{QStringLiteral("endpointId"),
                                                     QStringLiteral("revision"),
                                                     QStringLiteral("contextGeneration"),
                                                     QStringLiteral("floorDbm")}));
    ops.insert(QStringLiteral("iq-stream-context"),
               declaredOp(QStringLiteral("remoteIqVersion"),
                          peer + QStringList{QStringLiteral("sliceId"), QStringLiteral("revision"),
                                             QStringLiteral("enabled"), QStringLiteral("generation"),
                                             QStringLiteral("sampleRateHz"), QStringLiteral("reason")}));
    ops.insert(QStringLiteral("audio-context"), audioContextShapes());
    ops.insert(QStringLiteral("receiver-audio-context"), receiverAudioContextShapes());
    ops.insert(QStringLiteral("headphones-audio-context"), headphonesAudioContextShapes());
    // DaemonMediaController.cpp handleMonitorAudio's answer (parity Task 32).
    ops.insert(QStringLiteral("monitor-audio-context"),
               declaredOp(QStringLiteral("txMonitorAudioVersion"),
                          peer + QStringList{QStringLiteral("revision"), QStringLiteral("route")}));
    // DaemonMediaController.cpp handleClockProbe's reply.
    ops.insert(QStringLiteral("clock-echo"),
               declaredOp(QStringLiteral("audioClockVersion"),
                          peer + QStringList{QStringLiteral("id"), QStringLiteral("t0"),
                                             QStringLiteral("t1"), QStringLiteral("t2"),
                                             QStringLiteral("generation"),
                                             QStringLiteral("rtpTimestamp"),
                                             QStringLiteral("capturedNs")}));
    // iPhone app plan Task 29: DaemonMediaController.cpp finishReplacement.
    ops.insert(QStringLiteral("replace"),
               declaredOp(QStringLiteral("mediaReplaceVersion"),
                          peer + QStringList{QStringLiteral("replaces")}));
    return ops;
}

QJsonObject captureMediaControl()
{
    return QJsonObject{
        {QStringLiteral("kind"),
         QString::fromUtf8(SessionMessages::kindName(SessionMessageKind::MediaControl))},
        {QStringLiteral("guiToCore"), guiToCoreOps()},
        {QStringLiteral("coreToGui"), coreToGuiOps()},
    };
}

// ── limits ───────────────────────────────────────────────────────────────

QJsonObject limit(const QJsonValue& value, const QString& unit, const QString& source)
{
    return QJsonObject{{QStringLiteral("value"), value},
                       {QStringLiteral("unit"), unit},
                       {QStringLiteral("source"), source}};
}

QJsonObject captureLimits()
{
    QJsonObject limits;
    limits.insert(QStringLiteral("stationInboundMessageBytes"),
                  limit(static_cast<qint64>(StationServer::kMaxIncomingMessageBytes),
                        QStringLiteral("bytes"),
                        QStringLiteral("StationServer::kMaxIncomingMessageBytes")));
    limits.insert(QStringLiteral("clientInboundMessageBytes"),
                  limit(static_cast<qint64>(StationClient::kMaxIncomingMessageBytes),
                        QStringLiteral("bytes"),
                        QStringLiteral("StationClient::kMaxIncomingMessageBytes")));
    // iPhone app plan Task 28 (R-IOS-16): the control data channel's
    // longest message (the link document, "Control over a data channel").
    limits.insert(QStringLiteral("controlChannelChunkBytes"),
                  limit(static_cast<qint64>(ControlFraming::kMaxChunkBytes),
                        QStringLiteral("bytes"),
                        QStringLiteral("ControlFraming::kMaxChunkBytes")));
    limits.insert(QStringLiteral("mediaControlBytes"),
                  limit(static_cast<qint64>(kMaxMediaControlBytes), QStringLiteral("bytes"),
                        QStringLiteral("kMaxMediaControlBytes")));
    limits.insert(QStringLiteral("telemetryBytes"),
                  limit(static_cast<qint64>(kMaxStationTelemetryBytes), QStringLiteral("bytes"),
                        QStringLiteral("kMaxStationTelemetryBytes")));
    limits.insert(QStringLiteral("heartbeatIntervalMs"),
                  limit(StationServer::kDefaultHeartbeatIntervalMs, QStringLiteral("ms"),
                        QStringLiteral("StationServer::kDefaultHeartbeatIntervalMs")));
    limits.insert(QStringLiteral("missedPongs"),
                  limit(StationServer::kDefaultMaxMissedPongs, QStringLiteral("count"),
                        QStringLiteral("StationServer::kDefaultMaxMissedPongs")));
    limits.insert(QStringLiteral("connectDeadlineMs"),
                  limit(kStationHandshakeDeadlineMs, QStringLiteral("ms"),
                        QStringLiteral("kStationHandshakeDeadlineMs")));
    limits.insert(QStringLiteral("deltaFlushMs"),
                  limit(StationServer::kDefaultDeltaFlushMs, QStringLiteral("ms"),
                        QStringLiteral("StationServer::kDefaultDeltaFlushMs")));
    // StationClient.cpp:1171 [file scope, unreachable here]:
    //   constexpr int kReconnectBackoffSteps[] = { 1, 2, 5, 10, 30, 60 };
    // in units of StationClient::kDefaultReconnectBackoffUnitMs.
    // tst_link_surface_manifest reads that line back.
    QJsonArray backoff;
    for (const int step : {1, 2, 5, 10, 30, 60}) {
        backoff.append(step * StationClient::kDefaultReconnectBackoffUnitMs);
    }
    limits.insert(QStringLiteral("clientReconnectBackoffMs"),
                  limit(backoff, QStringLiteral("ms"),
                        QStringLiteral("StationClient.cpp kReconnectBackoffSteps x "
                                       "StationClient::kDefaultReconnectBackoffUnitMs")));
    limits.insert(QStringLiteral("maxPeers"),
                  limit(StationServer::kMaxConcurrentPeers, QStringLiteral("count"),
                        QStringLiteral("StationServer::kMaxConcurrentPeers")));
    // Part C fix wave (R1-M4): connecting peers one address may hold.
    limits.insert(QStringLiteral("maxHandshakesPerAddress"),
                  limit(StationServer::kMaxHandshakesPerAddress, QStringLiteral("count"),
                        QStringLiteral("StationServer::kMaxHandshakesPerAddress")));
    // iPhone app Task 71 (R-IOS-02): the devices that hold a place at once,
    // how long a dropped one keeps it, and the LAN announcement's largest
    // schema-2 datagram now that it carries the count.
    limits.insert(QStringLiteral("maxDeviceSessions"),
                  limit(StationServer::kMaxDeviceSessions, QStringLiteral("count"),
                        QStringLiteral("StationServer::kMaxDeviceSessions")));
    limits.insert(QStringLiteral("graceMs"),
                  limit(static_cast<qint64>(DeviceSessionRegistry::kGraceMs), QStringLiteral("ms"),
                        QStringLiteral("DeviceSessionRegistry::kGraceMs")));
    limits.insert(QStringLiteral("takeoverAnswerMs"),
                  limit(StationServer::kTakeoverAnswerMs, QStringLiteral("ms"),
                        QStringLiteral("StationServer::kTakeoverAnswerMs")));
    // iPhone app Task 74 (R-IOS-30): how long a question stays open
    // (ruling 7.5; sent as expiresInMs, enforced from Task 75).
    limits.insert(QStringLiteral("confirmExpiryMs"),
                  limit(static_cast<qint64>(ConfirmStep::kExpiryMs), QStringLiteral("ms"),
                        QStringLiteral("ConfirmStep::kExpiryMs")));
    limits.insert(QStringLiteral("lanAnnouncementMaxBytes"),
                  limit(kStationLanMaxSchema2DatagramBytes, QStringLiteral("bytes"),
                        QStringLiteral("kStationLanMaxSchema2DatagramBytes")));
    // Part C fix wave: auth.request's optional device `shortName`.
    limits.insert(QStringLiteral("shortNameMaxBytes"),
                  limit(DeviceStore::kMaxShortNameBytes, QStringLiteral("bytes"),
                        QStringLiteral("DeviceStore::kMaxShortNameBytes")));
    // DaemonMediaController.cpp:35 [file scope, unreachable here]:
    //   constexpr int kMaxEndpoints = 8;
    limits.insert(QStringLiteral("maxDisplayEndpoints"),
                  limit(8, QStringLiteral("count"),
                        QStringLiteral("DaemonMediaController.cpp kMaxEndpoints")));
    // DaemonMediaController.cpp:1158 handleSubscribe: the minimum is the
    // literal 1 in exactInt(pixels, 1, SpectrumEndpoint::kMaxPixels).
    limits.insert(QStringLiteral("endpointPixels"),
                  limit(QJsonObject{{QStringLiteral("min"), 1},
                                    {QStringLiteral("max"), SpectrumEndpoint::kMaxPixels}},
                        QStringLiteral("pixels"),
                        QStringLiteral("DaemonMediaController.cpp handleSubscribe literal 1; "
                                       "SpectrumEndpoint::kMaxPixels")));
    // DaemonMediaController.cpp:1159 handleSubscribe: exactInt(fps, 1, 60),
    // both literals; the maximum is also DisplayBudget.h's
    // kMaximumSpectrumDisplayFramesPerSecond, which names it.
    limits.insert(QStringLiteral("endpointFps"),
                  limit(QJsonObject{{QStringLiteral("min"), 1},
                                    {QStringLiteral("max"),
                                     static_cast<int>(kMaximumSpectrumDisplayFramesPerSecond)}},
                        QStringLiteral("frames per second"),
                        QStringLiteral("DaemonMediaController.cpp handleSubscribe literal 1; "
                                       "kMaximumSpectrumDisplayFramesPerSecond")));
    // iPhone app plan Task 29 (R-IOS-16; the link document, section 21):
    // the race, moving a session, and replacing its media.
    limits.insert(QStringLiteral("raceIpv4DelayMs"),
                  limit(PathRacer::kIpv4DelayMs, QStringLiteral("ms"),
                        QStringLiteral("PathRacer::kIpv4DelayMs")));
    QJsonArray upgrades;
    for (const int delay : PathRacer::kUpgradeRetryMs) {
        upgrades.append(delay);
    }
    limits.insert(QStringLiteral("pathUpgradeRetryMs"),
                  limit(upgrades, QStringLiteral("ms"),
                        QStringLiteral("PathRacer::kUpgradeRetryMs")));
    limits.insert(QStringLiteral("serviceAnswerDeadlineMs"),
                  limit(RendezvousDialer::kAnswerDeadlineMs, QStringLiteral("ms"),
                        QStringLiteral("RendezvousDialer::kAnswerDeadlineMs")));
    limits.insert(QStringLiteral("pathTicketLifetimeMs"),
                  limit(StationServer::kPathTicketLifetimeMs, QStringLiteral("ms"),
                        QStringLiteral("StationServer::kPathTicketLifetimeMs")));
    limits.insert(QStringLiteral("pathTicketMaxChars"),
                  limit(static_cast<qint64>(kMaxPathTicketChars), QStringLiteral("characters"),
                        QStringLiteral("kMaxPathTicketChars")));
    limits.insert(QStringLiteral("pathSwitchDeadlineMs"),
                  limit(SwitchableTransport::kSwitchDeadlineMs, QStringLiteral("ms"),
                        QStringLiteral("SwitchableTransport::kSwitchDeadlineMs")));
    limits.insert(QStringLiteral("pathOldCloseMs"),
                  limit(SwitchableTransport::kOldCloseMs, QStringLiteral("ms"),
                        QStringLiteral("SwitchableTransport::kOldCloseMs")));
    limits.insert(QStringLiteral("mediaReplaceOverlapMs"),
                  limit(DaemonMediaController::kReplaceOverlapMs, QStringLiteral("ms"),
                        QStringLiteral("DaemonMediaController::kReplaceOverlapMs")));
    limits.insert(QStringLiteral("mediaReplaceDrainMs"),
                  limit(DaemonMediaController::kReplaceDrainMs, QStringLiteral("ms"),
                        QStringLiteral("DaemonMediaController::kReplaceDrainMs")));
    return limits;
}

// ── differences ──────────────────────────────────────────────────────────

QString compact(const QJsonValue& value)
{
    if (value.isObject()) {
        return QString::fromUtf8(QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact));
    }
    if (value.isArray()) {
        return QString::fromUtf8(QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact));
    }
    // A scalar: wrap it so QJsonDocument prints it, then strip the wrapper.
    const QByteArray wrapped =
        QJsonDocument(QJsonArray{value}).toJson(QJsonDocument::Compact);
    return QString::fromUtf8(wrapped.mid(1, wrapped.size() - 2));
}

QString identityKey(const QJsonArray& array)
{
    for (const QString& candidate : {QStringLiteral("name"), QStringLiteral("verb"),
                                     QStringLiteral("key"), QStringLiteral("version")}) {
        bool all = !array.isEmpty();
        for (const QJsonValue& element : array) {
            if (!element.isObject() || !element.toObject().contains(candidate)) {
                all = false;
                break;
            }
        }
        if (all) {
            return candidate;
        }
    }
    return QString();
}

void diffValues(const QString& path, const QJsonValue& expected, const QJsonValue& actual,
                QStringList* out)
{
    if (expected.isObject() && actual.isObject()) {
        const QJsonObject e = expected.toObject();
        const QJsonObject a = actual.toObject();
        QStringList keys = e.keys() + a.keys();
        keys.sort();
        keys.removeDuplicates();
        for (const QString& key : keys) {
            const QString child = path.isEmpty() ? key : path + QLatin1Char('.') + key;
            if (!a.contains(key)) {
                out->append(child + QStringLiteral(": removed"));
            } else if (!e.contains(key)) {
                out->append(child + QStringLiteral(": added ") + compact(a.value(key)));
            } else {
                diffValues(child, e.value(key), a.value(key), out);
            }
        }
        return;
    }
    if (expected.isArray() && actual.isArray()) {
        const QJsonArray e = expected.toArray();
        const QJsonArray a = actual.toArray();
        const QString idE = identityKey(e);
        const QString idA = identityKey(a);
        const QString id = !idE.isEmpty() ? idE : idA;
        if (!id.isEmpty() && (e.isEmpty() || idE == id) && (a.isEmpty() || idA == id)) {
            QHash<QString, QJsonValue> byE;
            QHash<QString, QJsonValue> byA;
            QStringList order;
            for (const QJsonValue& v : e) {
                const QString k = compact(v.toObject().value(id));
                byE.insert(k, v);
                order.append(k);
            }
            for (const QJsonValue& v : a) {
                const QString k = compact(v.toObject().value(id));
                byA.insert(k, v);
                if (!byE.contains(k)) {
                    order.append(k);
                }
            }
            QStringList orderE;
            QStringList orderA;
            for (const QJsonValue& v : e) { orderE.append(compact(v.toObject().value(id))); }
            for (const QJsonValue& v : a) { orderA.append(compact(v.toObject().value(id))); }
            for (const QString& k : order) {
                QString label = k;
                if (label.startsWith(QLatin1Char('"')) && label.endsWith(QLatin1Char('"'))) {
                    label = label.mid(1, label.size() - 2);
                }
                const QString child = path + QLatin1Char('[') + label + QLatin1Char(']');
                if (!byA.contains(k)) {
                    out->append(child + QStringLiteral(": removed"));
                } else if (!byE.contains(k)) {
                    out->append(child + QStringLiteral(": added ") + compact(byA.value(k)));
                } else {
                    diffValues(child, byE.value(k), byA.value(k), out);
                }
            }
            QStringList commonE;
            QStringList commonA;
            for (const QString& k : orderE) { if (byA.contains(k)) { commonE.append(k); } }
            for (const QString& k : orderA) { if (byE.contains(k)) { commonA.append(k); } }
            if (commonE != commonA) {
                out->append(path + QStringLiteral(": order changed"));
            }
            return;
        }
        const qsizetype n = std::max(e.size(), a.size());
        for (qsizetype i = 0; i < n; ++i) {
            const QString child = path + QLatin1Char('[') + QString::number(i) + QLatin1Char(']');
            if (i >= a.size()) {
                out->append(child + QStringLiteral(": removed ") + compact(e.at(i)));
            } else if (i >= e.size()) {
                out->append(child + QStringLiteral(": added ") + compact(a.at(i)));
            } else {
                diffValues(child, e.at(i), a.at(i), out);
            }
        }
        return;
    }
    if (expected != actual) {
        out->append(path + QStringLiteral(": ") + compact(expected) + QStringLiteral(" -> ")
                    + compact(actual));
    }
}

} // namespace

QStringList LinkSurface::sectionNames()
{
    return {QStringLiteral("messageKinds"), QStringLiteral("capabilities"),
            QStringLiteral("mirrorClasses"), QStringLiteral("objectKeys"),
            QStringLiteral("commands"), QStringLiteral("settingsScope"),
            QStringLiteral("telemetry"), QStringLiteral("mediaControl"),
            QStringLiteral("limits")};
}

QList<const QMetaObject*> LinkSurface::mirroredMetaObjects()
{
    // The classes MirrorSchema.cpp's kMirroredClasses allowlist names, in
    // its order. tst_link_surface_manifest compares their short names with
    // MirrorSchema::mirroredClassNames(), so a class added there and not
    // here fails.
    return {&SliceModel::staticMetaObject,
            &PureSignalSettings::staticMetaObject,
            &DspAssetService::staticMetaObject,
            &PureSignalSessionFacade::staticMetaObject,
            &TransmitModel::staticMetaObject,
            &TunerModel::staticMetaObject,
            &RadioModel::staticMetaObject,
            &PanadapterModel::staticMetaObject,
            &NotchModel::staticMetaObject,
            &StepAttenuatorFacade::staticMetaObject,
            &AlexAntennaFacade::staticMetaObject,
            &IoBoardHl2Facade::staticMetaObject,
            &AmplifierModel::staticMetaObject,
            &RfKitModel::staticMetaObject,
            &StationTciModel::staticMetaObject,
            &AccessoryDataModel::staticMetaObject,
            &AccessorySettingsModel::staticMetaObject,
            &StationDevicesFacade::staticMetaObject,
            &StationCatalog::staticMetaObject,
            &SetupDescription::staticMetaObject,
            &SpotSourceHost::staticMetaObject,
            &ConnectedDevicesFacade::staticMetaObject,
            &SliceMarker::staticMetaObject,
            &SliceAccess::staticMetaObject,
            &TransmitState::staticMetaObject,
            &StationVax::staticMetaObject,
            &PaProfilesFacade::staticMetaObject};
}

QJsonObject LinkSurface::capture()
{
    return QJsonObject{
        {QStringLiteral("messageKinds"), captureMessageKinds()},
        {QStringLiteral("capabilities"), captureCapabilities()},
        {QStringLiteral("mirrorClasses"), captureMirrorClasses()},
        {QStringLiteral("objectKeys"), captureObjectKeys()},
        {QStringLiteral("commands"), captureCommands()},
        {QStringLiteral("settingsScope"), captureSettingsScope()},
        {QStringLiteral("telemetry"), captureTelemetry()},
        {QStringLiteral("mediaControl"), captureMediaControl()},
        {QStringLiteral("limits"), captureLimits()},
    };
}

QStringList LinkSurface::differences(const QJsonObject& expected, const QJsonObject& actual)
{
    QStringList out;
    diffValues(QString(), expected, actual, &out);
    return out;
}

} // namespace NereusSDR::Test
