// =================================================================
// src/core/session/SessionMessages.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Remote-daemon R2 Task 10.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-05  J.J. Boyd / KG4VCF  Remote daemon R2 Task 10: session
//                                    message shapes and JSON codec. AI-
//                                    assisted transformation via Anthropic
//                                    Claude Code.
//   2026-08-05  J.J. Boyd / KG4VCF  Remote daemon R2 Task 11: CommandInvoke
//                                    / CommandResult codec. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
//   2026-08-08  J.J. Boyd / KG4VCF  Remote daemon R2 Task 18: handshake,
//                                    property-write and settings codec.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
//   2026-08-09  J.J. Boyd / KG4VCF  Whole-branch review, Important 2:
//                                    range-check the Int64/Enum value
//                                    before narrowing it to qlonglong.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
//   2026-08-09  J.J. Boyd / KG4VCF  Whole-branch review, Important 4:
//                                    settingsValueAbsent(), the distinct
//                                    absence encoding for a removed key.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 1 (R-IOS-01):
//                                    allKinds(). AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 4 (R-IOS-01): the
//                                    hello's `majors` and `features`,
//                                    optional on decode. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Part A fix wave (R-IOS-01):
//                                    the hello builder never sends an
//                                    empty `majors`. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 12 (R-IOS-08,
//                                    R-IOS-01): hello `identity` and
//                                    `challenge`, auth.request `device`,
//                                    the end `code`. AI-assisted
//                                    implementation via Anthropic Claude
//                                    Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 14 (R-IOS-08): the
//                                    five pair.* kinds. AI-assisted
//                                    implementation via Anthropic Claude
//                                    Code.
//   2026-09-24: Part C fix wave: the optional device shortName in
//               auth.request, stored with the device. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-25: iPhone app Task 74 (R-IOS-30): the confirm.request and
//               notice codec. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-26: iPhone app plan Task 77 (R-IOS-02, R-IOS-03, R-IOS-13):
//               confirm.request holder (takeTransmit). J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude Code.
//   2026-09-26: parity Task 19 (R-IOS-25): the record.batch codec.
//               J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-27: iPhone app plan Task 29 (R-IOS-16): path.join and
//               path.switch. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
// =================================================================

#include "core/session/SessionMessages.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSet>

#include <cmath>
#include <limits>

namespace NereusSDR {

// ── Builders ─────────────────────────────────────────────────────────────

SessionMessage SessionMessages::schema(const QByteArray& className,
                                       const QList<SessionSchemaField>& fields)
{
    SessionMessage m;
    m.kind = SessionMessageKind::Schema;
    m.className = className;
    m.fields = fields;
    return m;
}

SessionMessage SessionMessages::objectCreate(const QByteArray& objectKey,
                                             const QByteArray& className,
                                             const QList<MirrorUpdate>& fullBag)
{
    SessionMessage m;
    m.kind = SessionMessageKind::ObjectCreate;
    m.objectKey = objectKey;
    m.className = className;
    m.updates = fullBag;
    return m;
}

SessionMessage SessionMessages::objectDestroy(const QByteArray& objectKey,
                                              const QByteArray& className)
{
    SessionMessage m;
    m.kind = SessionMessageKind::ObjectDestroy;
    m.objectKey = objectKey;
    m.className = className;
    return m;
}

SessionMessage SessionMessages::delta(const QByteArray& objectKey,
                                      const QList<MirrorUpdate>& changed)
{
    SessionMessage m;
    m.kind = SessionMessageKind::Delta;
    m.objectKey = objectKey;
    m.updates = changed;
    return m;
}

SessionMessage SessionMessages::snapshotComplete()
{
    SessionMessage m;
    m.kind = SessionMessageKind::SnapshotComplete;
    return m;
}

SessionMessage SessionMessages::commandInvoke(const QByteArray& verb, quint32 commandId,
                                              const QList<MirrorUpdate>& arguments)
{
    SessionMessage m;
    m.kind = SessionMessageKind::CommandInvoke;
    m.commandVerb = verb;
    m.commandId = commandId;
    m.arguments = arguments;
    return m;
}

SessionMessage SessionMessages::commandResult(const QByteArray& verb, quint32 commandId,
                                              bool accepted, const QString& reason,
                                              const QList<QByteArray>& affectedKeys,
                                              const QList<MirrorUpdate>& values)
{
    SessionMessage m;
    m.kind = SessionMessageKind::CommandResult;
    m.commandVerb = verb;
    m.commandId = commandId;
    m.accepted = accepted;
    m.reason = reason;
    m.affectedKeys = affectedKeys;
    m.updates = values;
    return m;
}

// ── Task 18 builders ─────────────────────────────────────────────────────

namespace {
/// One settings key/value pair in the generic {name, kind, value} shape
/// the settings kinds reuse. `ordinal` is meaningless for settings (there
/// is no schema and no ordinal dictionary for a flat key space) and is
/// always 0, exactly as it is for CommandInvoke arguments.
MirrorUpdate settingsEntry(const QString& key, const QString& value)
{
    return MirrorUpdate{ 0, key.toUtf8(), MirrorWireKind::Utf8, QVariant(value) };
}
} // namespace

SessionMessage SessionMessages::hello(quint16 major, quint16 minor,
                                      qint32 settingsSchemaVersion,
                                      const QString& peerName)
{
    SessionMessage m;
    m.kind = SessionMessageKind::Hello;
    m.protocolMajor = major;
    m.protocolMinor = minor;
    m.settingsSchemaVersion = settingsSchemaVersion;
    m.peerName = peerName;
    m.supportedMajors = {major};
    return m;
}

SessionMessage SessionMessages::hello(quint16 major, quint16 minor,
                                      qint32 settingsSchemaVersion,
                                      const QString& peerName,
                                      const QList<quint16>& supportedMajors,
                                      const QHash<QByteArray, int>& features)
{
    SessionMessage m = hello(major, minor, settingsSchemaVersion, peerName);
    // `majors` is never empty on the wire (decode refuses []): an empty
    // list sends what an absent key means, [major].
    if (!supportedMajors.isEmpty()) {
        m.supportedMajors = supportedMajors;
    }
    m.features = features;
    m.majorsOnWire = true;
    m.featuresOnWire = true;
    return m;
}

SessionMessage SessionMessages::authRequest(const QString& token)
{
    SessionMessage m;
    m.kind = SessionMessageKind::AuthRequest;
    m.token = token;
    return m;
}

SessionMessage SessionMessages::authRequest(const QString& token,
                                            const SessionDeviceBlock& device)
{
    SessionMessage m = authRequest(token);
    m.device = device;
    return m;
}

SessionMessage SessionMessages::authResult(bool accepted, const QString& reason, bool retryable,
                                           const QString& endCode)
{
    SessionMessage m = authResult(accepted, reason, retryable);
    m.endCode = endCode;
    return m;
}

SessionMessage SessionMessages::sessionEnd(const QString& reason, bool retryable,
                                           const QString& endCode)
{
    SessionMessage m = sessionEnd(reason, retryable);
    m.endCode = endCode;
    return m;
}

SessionMessage SessionMessages::sessionHeld(const QJsonArray& devices, quint32 revision,
                                            std::optional<QJsonObject> placeTaken,
                                            std::optional<QJsonObject> placeFreed)
{
    SessionMessage m;
    m.kind = SessionMessageKind::SessionHeld;
    m.heldDevices = devices;
    m.heldRevision = revision;
    m.placeTaken = std::move(placeTaken);
    m.placeFreed = std::move(placeFreed);
    return m;
}

SessionMessage SessionMessages::sessionTakeover(const QString& deviceId, quint32 revision)
{
    SessionMessage m;
    m.kind = SessionMessageKind::SessionTakeover;
    m.takeoverDeviceId = deviceId;
    m.heldRevision = revision;
    return m;
}

SessionMessage SessionMessages::authResult(bool accepted, const QString& reason,
                                           bool retryable)
{
    SessionMessage m;
    m.kind = SessionMessageKind::AuthResult;
    m.accepted = accepted;
    m.reason = reason;
    m.retryable = retryable;
    return m;
}

SessionMessage SessionMessages::capabilities(const QList<MirrorUpdate>& descriptor)
{
    SessionMessage m;
    m.kind = SessionMessageKind::Capabilities;
    m.updates = descriptor;
    return m;
}

SessionMessage SessionMessages::sessionEnd(const QString& reason, bool retryable)
{
    SessionMessage m;
    m.kind = SessionMessageKind::SessionEnd;
    m.reason = reason;
    m.retryable = retryable;
    return m;
}

SessionMessage SessionMessages::pairStart(const QString& mode, const SessionPairDevice& device)
{
    SessionMessage m;
    m.kind = SessionMessageKind::PairStart;
    m.pairMode = mode;
    m.pairDevice = device;
    return m;
}

SessionMessage SessionMessages::pairAccept(const SessionStationIdentity& identity,
                                           const QString& label)
{
    SessionMessage m;
    m.kind = SessionMessageKind::PairAccept;
    m.stationIdentity = identity;
    m.pairLabel = label;
    return m;
}

SessionMessage SessionMessages::pairSpake(int step, const QString& data)
{
    SessionMessage m;
    m.kind = SessionMessageKind::PairSpake;
    m.pairStep = step;
    m.pairData = data;
    return m;
}

SessionMessage SessionMessages::pairConfirm(const QString& box)
{
    SessionMessage m;
    m.kind = SessionMessageKind::PairConfirm;
    m.pairBox = box;
    return m;
}

SessionMessage SessionMessages::pairFail(const QString& reason, qint64 retryAfterMs)
{
    SessionMessage m;
    m.kind = SessionMessageKind::PairFail;
    m.reason = reason;
    m.retryAfterMs = retryAfterMs;
    return m;
}

SessionMessage SessionMessages::confirmRequest(const SessionPrompt& prompt, const QString& reason)
{
    SessionMessage m;
    m.kind = SessionMessageKind::ConfirmRequest;
    m.prompt = prompt;
    m.reason = reason;
    return m;
}

SessionMessage SessionMessages::notice(const SessionPrompt& prompt, const QString& reason)
{
    SessionMessage m;
    m.kind = SessionMessageKind::Notice;
    m.prompt = prompt;
    m.reason = reason;
    return m;
}

SessionMessage SessionMessages::recordBatch(const RecordBatch& batch)
{
    SessionMessage m;
    m.kind = SessionMessageKind::RecordBatch;
    m.recordBatch = batch;
    return m;
}

SessionMessage SessionMessages::pathJoin(const QString& ticket)
{
    SessionMessage m;
    m.kind = SessionMessageKind::PathJoin;
    m.pathTicket = ticket;
    return m;
}

SessionMessage SessionMessages::pathSwitch()
{
    SessionMessage m;
    m.kind = SessionMessageKind::PathSwitch;
    return m;
}

SessionMessage SessionMessages::propertyWrite(const QByteArray& objectKey,
                                              const QList<MirrorUpdate>& updates,
                                              quint32 writeId)
{
    SessionMessage m;
    m.kind = SessionMessageKind::PropertyWrite;
    m.objectKey = objectKey;
    m.updates = updates;
    m.writeId = writeId;
    return m;
}

SessionMessage SessionMessages::propertyResult(const QByteArray& objectKey,
                                               quint32 writeId,
                                               const QList<SessionPropertyResult>& results)
{
    SessionMessage m;
    m.kind = SessionMessageKind::PropertyResult;
    m.objectKey = objectKey;
    m.writeId = writeId;
    m.propertyResults = results;
    return m;
}

SessionMessage SessionMessages::settingsSnapshot(const QList<MirrorUpdate>& entries)
{
    SessionMessage m;
    m.kind = SessionMessageKind::SettingsSnapshot;
    m.updates = entries;
    return m;
}

SessionMessage SessionMessages::settingsWrite(const QString& key, const QString& value,
                                              const QString& originTag)
{
    SessionMessage m;
    m.kind = SessionMessageKind::SettingsWrite;
    m.objectKey = key.toUtf8();
    m.updates = { settingsEntry(key, value) };
    m.originTag = originTag;
    return m;
}

SessionMessage SessionMessages::settingsRemove(const QString& key)
{
    SessionMessage m;
    m.kind = SessionMessageKind::SettingsRemove;
    m.objectKey = key.toUtf8();
    return m;
}

SessionMessage SessionMessages::settingsValue(const QString& key, const QString& value,
                                              const QString& originTag)
{
    SessionMessage m;
    m.kind = SessionMessageKind::SettingsValue;
    m.objectKey = key.toUtf8();
    m.updates = { settingsEntry(key, value) };
    m.originTag = originTag;
    return m;
}

SessionMessage SessionMessages::settingsValueAbsent(const QString& key,
                                                    const QString& originTag)
{
    // No entry at all, deliberately: see the header. An entry carrying an
    // empty string is the exact thing this exists to stop being sent.
    SessionMessage m;
    m.kind = SessionMessageKind::SettingsValue;
    m.objectKey = key.toUtf8();
    m.originTag = originTag;
    return m;
}

SessionMessage SessionMessages::settingsReject(const QString& key, bool hasRestoredValue,
                                               const QString& restoredValue,
                                               const QString& reason)
{
    SessionMessage m;
    m.kind = SessionMessageKind::SettingsReject;
    m.reason = reason;
    m.objectKey = key.toUtf8();
    if (hasRestoredValue) {
        m.updates = { settingsEntry(key, restoredValue) };
    }
    return m;
}

// ── Kind name tables ─────────────────────────────────────────────────────

namespace {

struct KindName {
    SessionMessageKind kind;
    const char* name;
};
// The dotted style ("object.create") matches ObjectRegistry.h's own wire
// vocabulary comment and the R2 design addendum section 7's terminology,
// rather than inventing a second naming convention for the same concept.
constexpr KindName kKindNames[] = {
    { SessionMessageKind::Schema, "schema" },
    { SessionMessageKind::ObjectCreate, "object.create" },
    { SessionMessageKind::ObjectDestroy, "object.destroy" },
    { SessionMessageKind::Delta, "delta" },
    { SessionMessageKind::SnapshotComplete, "snapshot.complete" },
    { SessionMessageKind::CommandInvoke, "command.invoke" },
    { SessionMessageKind::CommandResult, "command.result" },
    { SessionMessageKind::Hello, "hello" },
    { SessionMessageKind::AuthRequest, "auth.request" },
    { SessionMessageKind::AuthResult, "auth.result" },
    { SessionMessageKind::Capabilities, "capabilities" },
    { SessionMessageKind::SessionEnd, "session.end" },
    { SessionMessageKind::SessionHeld, "session.held" },
    { SessionMessageKind::SessionTakeover, "session.takeover" },
    { SessionMessageKind::PropertyWrite, "property.write" },
    { SessionMessageKind::PropertyResult, "property.result" },
    { SessionMessageKind::SettingsSnapshot, "settings.snapshot" },
    { SessionMessageKind::SettingsWrite, "settings.write" },
    { SessionMessageKind::SettingsRemove, "settings.remove" },
    { SessionMessageKind::SettingsValue, "settings.value" },
    { SessionMessageKind::SettingsReject, "settings.reject" },
    { SessionMessageKind::MediaControl, "media.control" },
    { SessionMessageKind::StationTelemetry, "station.metrics.v1" },
    // iPhone app Task 14 (R-IOS-08): pairing a device.
    { SessionMessageKind::PairStart, "pair.start" },
    { SessionMessageKind::PairAccept, "pair.accept" },
    { SessionMessageKind::PairSpake, "pair.spake" },
    { SessionMessageKind::PairConfirm, "pair.confirm" },
    { SessionMessageKind::PairFail, "pair.fail" },
    // iPhone app Task 74 (R-IOS-30): asking first, telling afterwards.
    { SessionMessageKind::ConfirmRequest, "confirm.request" },
    { SessionMessageKind::Notice, "notice" },
    // Parity Task 19 (R-IOS-25): a record stream's changes.
    { SessionMessageKind::RecordBatch, "record.batch" },
    // iPhone app plan Task 29 (R-IOS-16): moving a session.
    { SessionMessageKind::PathJoin, "path.join" },
    { SessionMessageKind::PathSwitch, "path.switch" },
};

struct WireKindName {
    MirrorWireKind kind;
    const char* name;
};
constexpr WireKindName kWireKindNames[] = {
    { MirrorWireKind::Bool, "bool" },
    { MirrorWireKind::Int64, "i64" },
    { MirrorWireKind::Float64, "f64" },
    { MirrorWireKind::Utf8, "utf8" },
    { MirrorWireKind::Enum, "enum" },
    { MirrorWireKind::Unsupported, "unsupported" },
};

} // namespace

QByteArray SessionMessages::kindName(SessionMessageKind kind)
{
    for (const KindName& k : kKindNames) {
        if (k.kind == kind) {
            return QByteArray(k.name);
        }
    }
    return QByteArray();
}

bool SessionMessages::kindFromName(const QByteArray& name, SessionMessageKind* out)
{
    for (const KindName& k : kKindNames) {
        if (name == k.name) {
            if (out != nullptr) {
                *out = k.kind;
            }
            return true;
        }
    }
    return false;
}

QList<SessionMessageKind> SessionMessages::allKinds()
{
    // Declaration order of SessionMessageKind (SessionMessages.h), not the
    // order of kKindNames above, which lists property.result beside
    // property.write.
    return {
        SessionMessageKind::Schema,
        SessionMessageKind::ObjectCreate,
        SessionMessageKind::ObjectDestroy,
        SessionMessageKind::Delta,
        SessionMessageKind::SnapshotComplete,
        SessionMessageKind::CommandInvoke,
        SessionMessageKind::CommandResult,
        SessionMessageKind::Hello,
        SessionMessageKind::AuthRequest,
        SessionMessageKind::AuthResult,
        SessionMessageKind::Capabilities,
        SessionMessageKind::SessionEnd,
        SessionMessageKind::SessionHeld,
        SessionMessageKind::SessionTakeover,
        SessionMessageKind::PropertyWrite,
        SessionMessageKind::SettingsSnapshot,
        SessionMessageKind::SettingsWrite,
        SessionMessageKind::SettingsRemove,
        SessionMessageKind::SettingsValue,
        SessionMessageKind::SettingsReject,
        SessionMessageKind::MediaControl,
        SessionMessageKind::StationTelemetry,
        SessionMessageKind::PropertyResult,
        SessionMessageKind::PairStart,
        SessionMessageKind::PairAccept,
        SessionMessageKind::PairSpake,
        SessionMessageKind::PairConfirm,
        SessionMessageKind::PairFail,
        SessionMessageKind::ConfirmRequest,
        SessionMessageKind::Notice,
        SessionMessageKind::RecordBatch,
        SessionMessageKind::PathJoin,
        SessionMessageKind::PathSwitch,
    };
}

QByteArray SessionMessages::wireKindName(MirrorWireKind kind)
{
    for (const WireKindName& k : kWireKindNames) {
        if (k.kind == kind) {
            return QByteArray(k.name);
        }
    }
    return QByteArray();
}

bool SessionMessages::wireKindFromName(const QByteArray& name, MirrorWireKind* out)
{
    for (const WireKindName& k : kWireKindNames) {
        if (name == k.name) {
            if (out != nullptr) {
                *out = k.kind;
            }
            return true;
        }
    }
    return false;
}

// ── Value codec: MirrorUpdate.value <-> QJsonValue ──────────────────────

namespace {

// MirrorSchema::encode() (MirrorSchema.cpp) settles every wire value onto
// exactly one of four QVariant runtime types before StateMirror ever hands
// it here: bool, qlonglong (Int64 AND Enum alike -- see its own `case
// MirrorWireKind::Int64: case MirrorWireKind::Enum:` fallthrough), double,
// QString. This mirrors that same choice on the JSON side rather than
// inventing a second one.
// JSON has no NaN and no Infinity. QJsonDocument::toJson() silently
// serialises a non-finite double as `null`, and a `null` fails
// fromJsonValue()'s isDouble() gate, so the WHOLE message is rejected --
// not just the one property.
//
// This is not hypothetical and it is not rare. SliceModel::snrDb defaults
// to std::numeric_limits<double>::quiet_NaN() (SliceModel.h, the RADE
// SNR row, which is genuinely unknown until RADE syncs), so EVERY slice
// object.create carried a NaN and every one of them was discarded on
// arrival -- the client saw no slices at all. Found by Task 18's own wss
// slot, which is the first thing in this plan to put a real slice snapshot
// through the codec end to end.
//
// So non-finite doubles travel as one of three explicit string tokens.
// Lossless (all three round-trip exactly, and +Inf stays distinct from
// -Inf), self-describing in a capture, and impossible to confuse with a
// real value because a Float64 property's ordinary encoding is a JSON
// number and never a string.
constexpr const char* kFloatNan = "nan";
constexpr const char* kFloatPosInf = "inf";
constexpr const char* kFloatNegInf = "-inf";

QJsonValue toJsonValue(MirrorWireKind kind, const QVariant& value)
{
    switch (kind) {
    case MirrorWireKind::Bool:
        return QJsonValue(value.toBool());
    case MirrorWireKind::Int64:
    case MirrorWireKind::Enum:
        return QJsonValue(static_cast<double>(value.toLongLong()));
    case MirrorWireKind::Float64: {
        const double d = value.toDouble();
        if (std::isnan(d)) {
            return QJsonValue(QString::fromLatin1(kFloatNan));
        }
        if (std::isinf(d)) {
            return QJsonValue(QString::fromLatin1(d > 0.0 ? kFloatPosInf : kFloatNegInf));
        }
        return QJsonValue(d);
    }
    case MirrorWireKind::Utf8:
        return QJsonValue(value.toString());
    case MirrorWireKind::Unsupported:
        break;
    }
    return QJsonValue();
}

// iPhone app Task 4: a JSON number that is a whole number within [lo, hi].
bool isWholeNumberIn(const QJsonValue& value, double lo, double hi)
{
    if (!value.isDouble()) {
        return false;
    }
    const double raw = value.toDouble();
    return std::isfinite(raw) && raw >= lo && raw <= hi && std::floor(raw) == raw;
}

bool fromJsonValue(MirrorWireKind kind, const QJsonValue& json, QVariant* out)
{
    if (out == nullptr) {
        return false;
    }
    switch (kind) {
    case MirrorWireKind::Bool:
        if (!json.isBool()) {
            return false;
        }
        *out = QVariant(json.toBool());
        return true;
    case MirrorWireKind::Int64:
    case MirrorWireKind::Enum: {
        if (!json.isDouble()) {
            return false;
        }
        // Whole-branch review, Important 2: range-checked BEFORE the
        // narrowing static_cast<qlonglong>, the same discipline this file
        // already applies to the ordinal (updateFromJson / fieldFromJson,
        // below), to the CommandInvoke/CommandResult id, and to Hello's
        // major/minor/settingsSchema. This was the one place the header
        // comment's promise about untrusted input was not kept.
        //
        // {"value":1e300} parses cleanly and passes the isDouble() gate,
        // and casting it to qlonglong is a floating-to-integer conversion
        // of an unrepresentable value: undefined behaviour, and one that
        // diverges by platform (a saturating result on arm64, the
        // indefinite value on x86-64), so a developer's Mac and the Pi 4
        // target would not even agree on the wrong answer. Any UBSan
        // build trips on it.
        //
        // qlonglong's MINIMUM converts to double exactly (it is -2^63);
        // its MAXIMUM does not (2^63 - 1 rounds UP to 2^63), so the upper
        // bound has to be the exclusive 2^63 rather than an inexact
        // max(). Written as the negated minimum so both bounds come from
        // the type rather than from a transcribed digit string. The
        // comparison is spelt as a rejection of everything OUTSIDE the
        // range, which also refuses a NaN (every comparison against one
        // is false).
        constexpr double kMinAsDouble =
            static_cast<double>(std::numeric_limits<qlonglong>::min());
        constexpr double kOnePastMaxAsDouble = -kMinAsDouble;
        const double raw = json.toDouble();
        if (!(raw >= kMinAsDouble && raw < kOnePastMaxAsDouble)
            || std::floor(raw) != raw) {
            return false;
        }
        *out = QVariant(static_cast<qlonglong>(raw));
        return true;
    }
    case MirrorWireKind::Float64:
        if (json.isDouble()) {
            *out = QVariant(json.toDouble());
            return true;
        }
        // The three non-finite tokens toJsonValue() emits. Anything else
        // that is not a number is still rejected outright: this is the far
        // side of a socket, and "some other string" is not a double.
        if (json.isString()) {
            const QString token = json.toString();
            if (token == QLatin1String(kFloatNan)) {
                *out = QVariant(std::numeric_limits<double>::quiet_NaN());
                return true;
            }
            if (token == QLatin1String(kFloatPosInf)) {
                *out = QVariant(std::numeric_limits<double>::infinity());
                return true;
            }
            if (token == QLatin1String(kFloatNegInf)) {
                *out = QVariant(-std::numeric_limits<double>::infinity());
                return true;
            }
        }
        return false;
    case MirrorWireKind::Utf8:
        if (!json.isString()) {
            return false;
        }
        *out = QVariant(json.toString());
        return true;
    case MirrorWireKind::Unsupported:
        break;
    }
    return false;
}

QJsonObject updateToJson(const MirrorUpdate& u)
{
    QJsonObject o;
    o.insert(QStringLiteral("ordinal"), static_cast<int>(u.ordinal));
    o.insert(QStringLiteral("name"), QString::fromUtf8(u.name));
    o.insert(QStringLiteral("kind"), QString::fromUtf8(SessionMessages::wireKindName(u.kind)));
    o.insert(QStringLiteral("value"), toJsonValue(u.kind, u.value));
    return o;
}

// False (leaving *out untouched) for a malformed entry: missing/negative/
// overflowing ordinal, an unrecognised kind name, or a value that does not
// match the kind it claims. Untrusted input from the far side of a socket
// gets rejected rather than coerced into something plausible-looking.
bool updateFromJson(const QJsonValue& v, MirrorUpdate* out)
{
    if (!v.isObject() || out == nullptr) {
        return false;
    }
    const QJsonObject o = v.toObject();
    if (!o.contains(QStringLiteral("ordinal")) || !o.value(QStringLiteral("ordinal")).isDouble()) {
        return false;
    }
    const double ordinalRaw = o.value(QStringLiteral("ordinal")).toDouble();
    if (ordinalRaw < 0.0 || ordinalRaw > 65535.0) {
        return false;
    }

    MirrorWireKind kind = MirrorWireKind::Unsupported;
    if (!SessionMessages::wireKindFromName(
            o.value(QStringLiteral("kind")).toString().toUtf8(), &kind)) {
        return false;
    }

    QVariant value;
    if (!fromJsonValue(kind, o.value(QStringLiteral("value")), &value)) {
        return false;
    }

    out->ordinal = static_cast<quint16>(ordinalRaw);
    out->name = o.value(QStringLiteral("name")).toString().toUtf8();
    out->kind = kind;
    out->value = value;
    return true;
}

QJsonObject fieldToJson(const SessionSchemaField& f)
{
    QJsonObject o;
    o.insert(QStringLiteral("ordinal"), static_cast<int>(f.ordinal));
    o.insert(QStringLiteral("name"), QString::fromUtf8(f.name));
    o.insert(QStringLiteral("kind"), QString::fromUtf8(SessionMessages::wireKindName(f.kind)));
    return o;
}

bool fieldFromJson(const QJsonValue& v, SessionSchemaField* out)
{
    if (!v.isObject() || out == nullptr) {
        return false;
    }
    const QJsonObject o = v.toObject();
    if (!o.contains(QStringLiteral("ordinal")) || !o.value(QStringLiteral("ordinal")).isDouble()) {
        return false;
    }
    const double ordinalRaw = o.value(QStringLiteral("ordinal")).toDouble();
    if (ordinalRaw < 0.0 || ordinalRaw > 65535.0) {
        return false;
    }

    MirrorWireKind kind = MirrorWireKind::Unsupported;
    if (!SessionMessages::wireKindFromName(
            o.value(QStringLiteral("kind")).toString().toUtf8(), &kind)) {
        return false;
    }

    out->ordinal = static_cast<quint16>(ordinalRaw);
    out->name = o.value(QStringLiteral("name")).toString().toUtf8();
    out->kind = kind;
    return true;
}

} // namespace

// ── Message codec ────────────────────────────────────────────────────────

QByteArray SessionMessages::encode(const SessionMessage& message)
{
    QJsonObject o;
    o.insert(QStringLiteral("type"), QString::fromUtf8(kindName(message.kind)));

    switch (message.kind) {
    case SessionMessageKind::StationTelemetry: {
        const auto payload = StationTelemetryCodec::encode(message.telemetry);
        if (!payload) { return {}; }
        o.insert(QStringLiteral("payload"), *payload);
        break;
    }
    case SessionMessageKind::MediaControl:
        o.insert(QStringLiteral("payload"), message.mediaPayload);
        break;
    case SessionMessageKind::Schema: {
        o.insert(QStringLiteral("class"), QString::fromUtf8(message.className));
        QJsonArray fields;
        for (const SessionSchemaField& f : message.fields) {
            fields.append(fieldToJson(f));
        }
        o.insert(QStringLiteral("fields"), fields);
        break;
    }
    case SessionMessageKind::ObjectCreate: {
        o.insert(QStringLiteral("key"), QString::fromUtf8(message.objectKey));
        o.insert(QStringLiteral("class"), QString::fromUtf8(message.className));
        QJsonArray props;
        for (const MirrorUpdate& u : message.updates) {
            props.append(updateToJson(u));
        }
        o.insert(QStringLiteral("properties"), props);
        break;
    }
    case SessionMessageKind::ObjectDestroy:
        o.insert(QStringLiteral("key"), QString::fromUtf8(message.objectKey));
        o.insert(QStringLiteral("class"), QString::fromUtf8(message.className));
        break;
    case SessionMessageKind::Delta: {
        o.insert(QStringLiteral("key"), QString::fromUtf8(message.objectKey));
        QJsonArray props;
        for (const MirrorUpdate& u : message.updates) {
            props.append(updateToJson(u));
        }
        o.insert(QStringLiteral("properties"), props);
        break;
    }
    case SessionMessageKind::SnapshotComplete:
        break;
    case SessionMessageKind::CommandInvoke: {
        o.insert(QStringLiteral("verb"), QString::fromUtf8(message.commandVerb));
        o.insert(QStringLiteral("id"), static_cast<double>(message.commandId));
        QJsonArray args;
        for (const MirrorUpdate& u : message.arguments) {
            args.append(updateToJson(u));
        }
        o.insert(QStringLiteral("args"), args);
        break;
    }
    case SessionMessageKind::CommandResult: {
        o.insert(QStringLiteral("verb"), QString::fromUtf8(message.commandVerb));
        o.insert(QStringLiteral("id"), static_cast<double>(message.commandId));
        o.insert(QStringLiteral("accepted"), message.accepted);
        o.insert(QStringLiteral("reason"), message.reason);
        QJsonArray affected;
        for (const QByteArray& key : message.affectedKeys) {
            affected.append(QString::fromUtf8(key));
        }
        o.insert(QStringLiteral("affected"), affected);
        if (!message.updates.isEmpty()) {
            QJsonArray values;
            for (const auto& value : message.updates) {
                values.append(updateToJson(value));
            }
            o.insert(QStringLiteral("values"), values);
        }
        break;
    }
    case SessionMessageKind::Hello:
        o.insert(QStringLiteral("major"), static_cast<int>(message.protocolMajor));
        o.insert(QStringLiteral("minor"), static_cast<int>(message.protocolMinor));
        o.insert(QStringLiteral("settingsSchema"),
                 static_cast<double>(message.settingsSchemaVersion));
        o.insert(QStringLiteral("peer"), message.peerName);
        // iPhone app Task 4: only when the sender declares them, so an
        // older peer's hello encodes again exactly as it arrived.
        if (message.majorsOnWire) {
            QJsonArray majors;
            for (const quint16 major : message.supportedMajors) {
                majors.append(static_cast<int>(major));
            }
            o.insert(QStringLiteral("majors"), majors);
        }
        if (message.featuresOnWire) {
            QJsonObject features;
            for (auto it = message.features.cbegin(); it != message.features.cend(); ++it) {
                features.insert(QString::fromUtf8(it.key()), it.value());
            }
            o.insert(QStringLiteral("features"), features);
        }
        // iPhone app Task 12: the Core's identity and this connection's
        // challenge, only when the sender set them.
        if (message.stationIdentity) {
            o.insert(QStringLiteral("identity"),
                     QJsonObject{
                         {QStringLiteral("publicKey"), message.stationIdentity->publicKey},
                         {QStringLiteral("certBinding"), message.stationIdentity->certBinding},
                     });
        }
        if (!message.challenge.isEmpty()) {
            o.insert(QStringLiteral("challenge"), message.challenge);
        }
        break;
    case SessionMessageKind::AuthRequest:
        o.insert(QStringLiteral("token"), message.token);
        // iPhone app Task 12: the device sign-in block.
        if (message.device) {
            QJsonObject device{
                {QStringLiteral("id"), message.device->id},
                {QStringLiteral("publicKey"), message.device->publicKey},
                {QStringLiteral("name"), message.device->name},
                {QStringLiteral("kind"), message.device->kind},
                {QStringLiteral("signature"), message.device->signature},
            };
            // Part C fix wave: the optional short name, only when there is one.
            if (!message.device->shortName.isEmpty()) {
                device.insert(QStringLiteral("shortName"), message.device->shortName);
            }
            o.insert(QStringLiteral("device"), device);
        }
        break;
    case SessionMessageKind::AuthResult:
        o.insert(QStringLiteral("accepted"), message.accepted);
        o.insert(QStringLiteral("reason"), message.reason);
        o.insert(QStringLiteral("retryable"), message.retryable);
        // iPhone app Task 12: the end code, only when there is one.
        if (!message.endCode.isEmpty()) {
            o.insert(QStringLiteral("code"), message.endCode);
        }
        break;
    case SessionMessageKind::Capabilities:
    case SessionMessageKind::SettingsSnapshot: {
        QJsonArray entries;
        for (const MirrorUpdate& u : message.updates) {
            entries.append(updateToJson(u));
        }
        o.insert(QStringLiteral("properties"), entries);
        break;
    }
    case SessionMessageKind::SessionEnd:
        o.insert(QStringLiteral("reason"), message.reason);
        o.insert(QStringLiteral("retryable"), message.retryable);
        if (!message.endCode.isEmpty()) {
            o.insert(QStringLiteral("code"), message.endCode);
        }
        if (!message.takenOverBy.isEmpty()) {
            o.insert(QStringLiteral("takenOverBy"), message.takenOverBy);
            o.insert(QStringLiteral("takenOverById"), message.takenOverById);
            o.insert(QStringLiteral("secondsAgo"), static_cast<double>(message.secondsAgo.value_or(0)));
        }
        break;
    case SessionMessageKind::SessionHeld:
        o.insert(QStringLiteral("devices"), message.heldDevices);
        o.insert(QStringLiteral("revision"), static_cast<double>(message.heldRevision));
        if (message.placeTaken) {
            o.insert(QStringLiteral("placeTaken"), *message.placeTaken);
        }
        if (message.placeFreed) {
            o.insert(QStringLiteral("placeFreed"), *message.placeFreed);
        }
        break;
    case SessionMessageKind::SessionTakeover:
        o.insert(QStringLiteral("deviceId"), message.takeoverDeviceId);
        o.insert(QStringLiteral("revision"), static_cast<double>(message.heldRevision));
        break;
    case SessionMessageKind::PropertyWrite: {
        o.insert(QStringLiteral("key"), QString::fromUtf8(message.objectKey));
        if (message.writeId != 0) {
            o.insert(QStringLiteral("writeId"), static_cast<double>(message.writeId));
        }
        QJsonArray props;
        for (const MirrorUpdate& u : message.updates) {
            props.append(updateToJson(u));
        }
        o.insert(QStringLiteral("properties"), props);
        break;
    }
    case SessionMessageKind::PropertyResult: {
        o.insert(QStringLiteral("key"), QString::fromUtf8(message.objectKey));
        o.insert(QStringLiteral("writeId"), static_cast<double>(message.writeId));
        QJsonArray results;
        for (const auto& result : message.propertyResults) {
            QJsonObject entry;
            entry.insert(QStringLiteral("property"), QString::fromUtf8(result.property));
            entry.insert(QStringLiteral("accepted"), result.accepted);
            entry.insert(QStringLiteral("reason"), result.reason);
            entry.insert(QStringLiteral("hasValue"), result.hasValue);
            if (result.hasValue) {
                entry.insert(QStringLiteral("value"), updateToJson(result.value));
            }
            results.append(entry);
        }
        o.insert(QStringLiteral("results"), results);
        break;
    }
    // iPhone app Task 14: the pair.* kinds (the link document's Pairing
    // section).
    case SessionMessageKind::PairStart:
        o.insert(QStringLiteral("mode"), message.pairMode);
        o.insert(QStringLiteral("device"),
                 QJsonObject{
                     {QStringLiteral("publicKey"),
                      message.pairDevice ? message.pairDevice->publicKey : QString()},
                     {QStringLiteral("name"),
                      message.pairDevice ? message.pairDevice->name : QString()},
                     {QStringLiteral("kind"),
                      message.pairDevice ? message.pairDevice->kind : QString()},
                 });
        break;
    case SessionMessageKind::PairAccept:
        o.insert(QStringLiteral("identity"),
                 QJsonObject{
                     {QStringLiteral("publicKey"),
                      message.stationIdentity ? message.stationIdentity->publicKey : QString()},
                     {QStringLiteral("certBinding"),
                      message.stationIdentity ? message.stationIdentity->certBinding
                                              : QString()},
                 });
        o.insert(QStringLiteral("label"), message.pairLabel);
        break;
    case SessionMessageKind::PairSpake:
        o.insert(QStringLiteral("step"), message.pairStep);
        o.insert(QStringLiteral("data"), message.pairData);
        break;
    case SessionMessageKind::PairConfirm:
        o.insert(QStringLiteral("box"), message.pairBox);
        break;
    case SessionMessageKind::PairFail:
        o.insert(QStringLiteral("reason"), message.reason);
        o.insert(QStringLiteral("retryAfterMs"), static_cast<double>(message.retryAfterMs));
        break;
    // iPhone app Task 74 (R-IOS-30): the several-devices design, 7.3.
    case SessionMessageKind::ConfirmRequest: {
        const SessionPrompt& p = message.prompt;
        o.insert(QStringLiteral("id"), static_cast<double>(p.id));
        o.insert(QStringLiteral("kind"), p.kind);
        o.insert(QStringLiteral("reason"), message.reason);
        o.insert(QStringLiteral("affected"), p.affected);
        o.insert(QStringLiteral("expiresInMs"), static_cast<double>(p.expiresInMs));
        if (p.change) {
            o.insert(QStringLiteral("change"), *p.change);
        }
        if (p.choices) {
            o.insert(QStringLiteral("choices"), *p.choices);
        }
        if (p.forCommandId) {
            o.insert(QStringLiteral("forCommandId"), static_cast<double>(*p.forCommandId));
        }
        if (p.forWriteId) {
            o.insert(QStringLiteral("forWriteId"), static_cast<double>(*p.forWriteId));
        }
        if (!p.forSettingsKey.isEmpty()) {
            o.insert(QStringLiteral("forSettingsKey"), p.forSettingsKey);
        }
        if (p.holder) {
            o.insert(QStringLiteral("holder"), *p.holder);
        }
        break;
    }
    // iPhone app Task 74 (R-IOS-30): the several-devices design, 7.4.
    case SessionMessageKind::Notice: {
        const SessionPrompt& p = message.prompt;
        o.insert(QStringLiteral("id"), static_cast<double>(p.id));
        o.insert(QStringLiteral("kind"), p.kind);
        o.insert(QStringLiteral("reason"), message.reason);
        o.insert(QStringLiteral("secondsAgo"), static_cast<double>(p.secondsAgo));
        o.insert(QStringLiteral("takeBack"), p.takeBack);
        if (!p.byDeviceId.isEmpty()) {
            o.insert(QStringLiteral("byDeviceId"), p.byDeviceId);
        }
        if (!p.byName.isEmpty()) {
            o.insert(QStringLiteral("byName"), p.byName);
        }
        if (!p.byShortName.isEmpty()) {
            o.insert(QStringLiteral("byShortName"), p.byShortName);
        }
        if (!p.byKind.isEmpty()) {
            o.insert(QStringLiteral("byKind"), p.byKind);
        }
        if (!p.bySource.isEmpty()) {
            o.insert(QStringLiteral("bySource"), p.bySource);
        }
        if (p.slices) {
            o.insert(QStringLiteral("slices"), *p.slices);
        }
        if (p.change) {
            o.insert(QStringLiteral("change"), *p.change);
        }
        break;
    }
    // Parity Task 19 (R-IOS-25): the link's "Record streams" section.
    case SessionMessageKind::RecordBatch: {
        const RecordBatch& b = message.recordBatch;
        o.insert(QStringLiteral("stream"), b.stream);
        o.insert(QStringLiteral("generation"), static_cast<double>(b.generation));
        o.insert(QStringLiteral("reset"), b.reset);
        QJsonArray upserts;
        for (const RecordUpsert& u : b.upserts) {
            upserts.append(QJsonObject{{QStringLiteral("id"), u.id},
                                       {QStringLiteral("fields"), u.fields}});
        }
        o.insert(QStringLiteral("upserts"), upserts);
        o.insert(QStringLiteral("removes"), QJsonArray::fromStringList(b.removes));
        break;
    }
    // iPhone app plan Task 29 (R-IOS-16): the link's section 21.2.
    case SessionMessageKind::PathJoin:
        o.insert(QStringLiteral("ticket"), message.pathTicket);
        break;
    case SessionMessageKind::PathSwitch:
        break;
    case SessionMessageKind::SettingsWrite:
    case SessionMessageKind::SettingsValue:
    case SessionMessageKind::SettingsRemove:
    case SessionMessageKind::SettingsReject: {
        o.insert(QStringLiteral("key"), QString::fromUtf8(message.objectKey));
        QJsonArray entries;
        for (const MirrorUpdate& u : message.updates) {
            entries.append(updateToJson(u));
        }
        o.insert(QStringLiteral("properties"), entries);
        if (message.kind == SessionMessageKind::SettingsReject && !message.reason.isEmpty()) {
            o.insert(QStringLiteral("reason"), message.reason);
        }
        if (message.kind == SessionMessageKind::SettingsWrite
            || message.kind == SessionMessageKind::SettingsValue) {
            o.insert(QStringLiteral("origin"), message.originTag);
        }
        break;
    }
    }

    const QByteArray wire = QJsonDocument(o).toJson(QJsonDocument::Compact);
    if (message.kind == SessionMessageKind::StationTelemetry
        && wire.size() > kMaxStationTelemetryBytes) {
        return {};
    }
    if (message.kind == SessionMessageKind::MediaControl
        && wire.size() > kMaxMediaControlBytes) {
        return {};
    }
    return wire;
}

bool SessionMessages::decode(const QByteArray& wire, SessionMessage* out)
{
    if (out == nullptr) {
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(wire, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }
    const QJsonObject o = doc.object();

    SessionMessageKind kind = SessionMessageKind::Delta;
    if (!kindFromName(o.value(QStringLiteral("type")).toString().toUtf8(), &kind)) {
        return false;
    }
    if (kind == SessionMessageKind::MediaControl
        && (wire.size() > kMaxMediaControlBytes
            || !o.value(QStringLiteral("payload")).isObject())) {
        return false;
    }
    if (kind == SessionMessageKind::StationTelemetry
        && (wire.size() > kMaxStationTelemetryBytes
            || !o.value(QStringLiteral("payload")).isObject())) {
        return false;
    }

    // Structural fields a well-formed message of this kind MUST carry,
    // checked for PRESENCE and TYPE before any of them is read. A missing
    // field decodes to QJsonValue::Undefined, and Undefined's toString()/
    // toArray() silently return an empty QString/QJsonArray -- exactly the
    // shape a genuinely empty-but-present field would have, which is why
    // reading through those conversions without checking first would
    // silently coerce "absent" into "empty" rather than reject it. This is
    // the far side of a socket a remote peer controls.
    const bool needsKey = kind == SessionMessageKind::ObjectCreate
        || kind == SessionMessageKind::ObjectDestroy || kind == SessionMessageKind::Delta
        || kind == SessionMessageKind::PropertyWrite
        || kind == SessionMessageKind::PropertyResult
        || kind == SessionMessageKind::SettingsWrite
        || kind == SessionMessageKind::SettingsRemove
        || kind == SessionMessageKind::SettingsValue
        || kind == SessionMessageKind::SettingsReject;
    if (needsKey && !o.value(QStringLiteral("key")).isString()) {
        return false;
    }
    if (kind == SessionMessageKind::PropertyResult
        || (kind == SessionMessageKind::PropertyWrite && o.contains(QStringLiteral("writeId")))) {
        const QJsonValue id = o.value(QStringLiteral("writeId"));
        const double raw = id.toDouble(-1);
        if (!id.isDouble() || !(raw >= 1.0 && raw <= 4294967295.0)
            || std::floor(raw) != raw) {
            return false;
        }
    }
    if (kind == SessionMessageKind::PropertyResult
        && !o.value(QStringLiteral("results")).isArray()) {
        return false;
    }
    const bool needsClass = kind == SessionMessageKind::Schema
        || kind == SessionMessageKind::ObjectCreate || kind == SessionMessageKind::ObjectDestroy;
    if (needsClass && !o.value(QStringLiteral("class")).isString()) {
        return false;
    }
    if (kind == SessionMessageKind::Schema && !o.value(QStringLiteral("fields")).isArray()) {
        return false;
    }
    const bool needsProperties =
        kind == SessionMessageKind::ObjectCreate || kind == SessionMessageKind::Delta
        || kind == SessionMessageKind::PropertyWrite
        || kind == SessionMessageKind::Capabilities
        || kind == SessionMessageKind::SettingsSnapshot
        || kind == SessionMessageKind::SettingsWrite
        || kind == SessionMessageKind::SettingsRemove
        || kind == SessionMessageKind::SettingsValue
        || kind == SessionMessageKind::SettingsReject;
    if (needsProperties && !o.value(QStringLiteral("properties")).isArray()) {
        return false;
    }
    // Task 18: the section 7.0 handshake kinds. Same presence-and-type
    // discipline as every field above, for the same reason: this is the
    // far side of a socket, and these three kinds run BEFORE the peer has
    // authenticated, so they are the most exposed surface in the protocol.
    if (kind == SessionMessageKind::Hello) {
        if (!o.value(QStringLiteral("major")).isDouble()
            || !o.value(QStringLiteral("minor")).isDouble()
            || !o.value(QStringLiteral("settingsSchema")).isDouble()
            || !o.value(QStringLiteral("peer")).isString()) {
            return false;
        }
        // Range-checked before the narrowing casts below, the same
        // discipline the CommandInvoke id check above applies.
        const double majorRaw = o.value(QStringLiteral("major")).toDouble();
        const double minorRaw = o.value(QStringLiteral("minor")).toDouble();
        const double schemaRaw = o.value(QStringLiteral("settingsSchema")).toDouble();
        if (majorRaw < 0.0 || majorRaw > 65535.0 || minorRaw < 0.0 || minorRaw > 65535.0) {
            return false;
        }
        if (schemaRaw < -2147483648.0 || schemaRaw > 2147483647.0) {
            return false;
        }
        // iPhone app Task 4: `majors` and `features` are optional (an older
        // peer sends neither), but when present they are checked like every
        // other field here: a non-empty array of whole numbers 0..65535,
        // and an object of non-empty names to whole numbers 0..2^31-1.
        if (o.contains(QStringLiteral("majors"))) {
            const QJsonValue majors = o.value(QStringLiteral("majors"));
            if (!majors.isArray() || majors.toArray().isEmpty()) {
                return false;
            }
            for (const QJsonValue& major : majors.toArray()) {
                if (!isWholeNumberIn(major, 0.0, 65535.0)) {
                    return false;
                }
            }
        }
        if (o.contains(QStringLiteral("features"))) {
            const QJsonValue features = o.value(QStringLiteral("features"));
            if (!features.isObject()) {
                return false;
            }
            const QJsonObject declared = features.toObject();
            for (auto it = declared.constBegin(); it != declared.constEnd(); ++it) {
                if (it.key().isEmpty() || !isWholeNumberIn(it.value(), 0.0, 2147483647.0)) {
                    return false;
                }
            }
        }
    }
    // iPhone app Task 12: the Core hello's `identity` (an object of two
    // strings) and `challenge` (a string), both optional; when present,
    // checked like every field here. Their contents are the sign-in's to
    // judge, not the decoder's.
    if (kind == SessionMessageKind::Hello) {
        if (o.contains(QStringLiteral("identity"))) {
            const QJsonValue identity = o.value(QStringLiteral("identity"));
            if (!identity.isObject()
                || !identity.toObject().value(QStringLiteral("publicKey")).isString()
                || !identity.toObject().value(QStringLiteral("certBinding")).isString()) {
                return false;
            }
        }
        if (o.contains(QStringLiteral("challenge"))
            && (!o.value(QStringLiteral("challenge")).isString()
                || o.value(QStringLiteral("challenge")).toString().isEmpty())) {
            return false;
        }
    }
    if (kind == SessionMessageKind::AuthRequest
        && !o.value(QStringLiteral("token")).isString()) {
        return false;
    }
    if (kind == SessionMessageKind::AuthRequest && o.contains(QStringLiteral("device"))) {
        const QJsonValue device = o.value(QStringLiteral("device"));
        if (!device.isObject()) {
            return false;
        }
        for (const char* field : {"id", "publicKey", "name", "kind", "signature"}) {
            if (!device.toObject().value(QLatin1String(field)).isString()) {
                return false;
            }
        }
        // Part C fix wave: `shortName` is optional, and a string when present.
        if (device.toObject().contains(QStringLiteral("shortName"))
            && !device.toObject().value(QStringLiteral("shortName")).isString()) {
            return false;
        }
    }
    if (kind == SessionMessageKind::AuthResult) {
        if (!o.value(QStringLiteral("accepted")).isBool()
            || !o.value(QStringLiteral("reason")).isString()) {
            return false;
        }
    }
    if (kind == SessionMessageKind::SessionEnd
        && !o.value(QStringLiteral("reason")).isString()) {
        return false;
    }
    if (kind == SessionMessageKind::SessionTakeover
        && (!o.value(QStringLiteral("deviceId")).isString()
            || !isWholeNumberIn(o.value(QStringLiteral("revision")), 0.0, 4294967295.0))) {
        return false;
    }
    if (kind == SessionMessageKind::SessionHeld) {
        if (!o.value(QStringLiteral("devices")).isArray()
            || !isWholeNumberIn(o.value(QStringLiteral("revision")), 0.0, 4294967295.0)
            || o.value(QStringLiteral("devices")).toArray().size() > 4) {
            return false;
        }
        for (const QJsonValue& value : o.value(QStringLiteral("devices")).toArray()) {
            if (!value.isObject()) {
                return false;
            }
            const QJsonObject device = value.toObject();
            for (const char* key : {"deviceId", "name", "shortName", "kind", "state", "from"}) {
                if (!device.value(QLatin1String(key)).isString()) {
                    return false;
                }
            }
            if (!device.value(QStringLiteral("replaceable")).isBool()
                || !device.value(QStringLiteral("holdsTransmit")).isBool()
                || !device.value(QStringLiteral("listeningOn")).isArray()) {
                return false;
            }
            if (device.value(QStringLiteral("deviceId")).toString().isEmpty()
                || (device.value(QStringLiteral("state")).toString() != QLatin1String("away")
                    && device.value(QStringLiteral("state")).toString() != QLatin1String("listening")
                    && device.value(QStringLiteral("state")).toString() != QLatin1String("transmitting"))) {
                return false;
            }
            auto validSlice = [](const QJsonValue& value) {
                if (!value.isObject()) return false;
                const QJsonObject slice = value.toObject();
                return slice.value(QStringLiteral("letter")).isString()
                    && isWholeNumberIn(slice.value(QStringLiteral("mode")), 0.0, 65535.0)
                    && isWholeNumberIn(slice.value(QStringLiteral("band")), 0.0, 65535.0)
                    && slice.value(QStringLiteral("frequencyHz")).isDouble()
                    && std::isfinite(slice.value(QStringLiteral("frequencyHz")).toDouble())
                    && slice.value(QStringLiteral("frequencyHz")).toDouble() >= 0.0;
            };
            for (const QJsonValue& slice : device.value(QStringLiteral("listeningOn")).toArray()) {
                if (!validSlice(slice)) return false;
            }
            for (const char* key : {"lastActivitySeconds", "connectedForSeconds",
                                    "awayForSeconds", "transmittingForSeconds"}) {
                if (!isWholeNumberIn(device.value(QLatin1String(key)), 0.0, 9007199254740991.0)) {
                    return false;
                }
            }
            if (device.contains(QStringLiteral("transmittingOn"))
                && !validSlice(device.value(QStringLiteral("transmittingOn")))) {
                return false;
            }
        }
        if (o.contains(QStringLiteral("placeTaken"))) {
            const QJsonValue taken = o.value(QStringLiteral("placeTaken"));
            if (!taken.isObject() || !taken.toObject().value(QStringLiteral("byName")).isString()
                || !taken.toObject().value(QStringLiteral("byId")).isString()
                || !isWholeNumberIn(taken.toObject().value(QStringLiteral("secondsAgo")),
                                    0.0, 9007199254740991.0)) {
                return false;
            }
        }
        if (o.contains(QStringLiteral("placeFreed"))) {
            const QJsonValue freed = o.value(QStringLiteral("placeFreed"));
            if (!freed.isObject()
                || !isWholeNumberIn(freed.toObject().value(QStringLiteral("secondsAgo")),
                                    0.0, 9007199254740991.0)) {
                return false;
            }
        }
    }
    if (kind == SessionMessageKind::SessionEnd) {
        if ((o.contains(QStringLiteral("takenOverBy"))
             && !o.value(QStringLiteral("takenOverBy")).isString())
            || (o.contains(QStringLiteral("takenOverById"))
                && !o.value(QStringLiteral("takenOverById")).isString())
            || (o.contains(QStringLiteral("secondsAgo"))
                && !isWholeNumberIn(o.value(QStringLiteral("secondsAgo")), 0.0,
                                    9007199254740991.0))) {
            return false;
        }
    }
    // iPhone app Task 12: `code`, when present, is a non-empty string.
    if ((kind == SessionMessageKind::AuthResult || kind == SessionMessageKind::SessionEnd)
        && o.contains(QStringLiteral("code"))
        && (!o.value(QStringLiteral("code")).isString()
            || o.value(QStringLiteral("code")).toString().isEmpty())) {
        return false;
    }
    if ((kind == SessionMessageKind::SettingsWrite
         || kind == SessionMessageKind::SettingsValue)
        && !o.value(QStringLiteral("origin")).isString()) {
        return false;
    }
    // iPhone app Task 14: the pair.* kinds. These run before the peer has
    // signed in, so every field is checked for presence and type like the
    // handshake's; what the strings hold is the pairing's to judge.
    if (kind == SessionMessageKind::PairStart) {
        const QString mode = o.value(QStringLiteral("mode")).toString();
        const QJsonValue device = o.value(QStringLiteral("device"));
        if (!o.value(QStringLiteral("mode")).isString()
            || (mode != QLatin1String("lan") && mode != QLatin1String("code"))
            || !device.isObject()) {
            return false;
        }
        for (const char* field : {"publicKey", "name", "kind"}) {
            if (!device.toObject().value(QLatin1String(field)).isString()) {
                return false;
            }
        }
    }
    if (kind == SessionMessageKind::PairAccept) {
        const QJsonValue identity = o.value(QStringLiteral("identity"));
        if (!identity.isObject()
            || !identity.toObject().value(QStringLiteral("publicKey")).isString()
            || !identity.toObject().value(QStringLiteral("certBinding")).isString()
            || !o.value(QStringLiteral("label")).isString()) {
            return false;
        }
    }
    if (kind == SessionMessageKind::PairSpake
        && (!isWholeNumberIn(o.value(QStringLiteral("step")), 0.0, 3.0)
            || !o.value(QStringLiteral("data")).isString()
            || o.value(QStringLiteral("data")).toString().isEmpty())) {
        return false;
    }
    if (kind == SessionMessageKind::PairConfirm
        && (!o.value(QStringLiteral("box")).isString()
            || o.value(QStringLiteral("box")).toString().isEmpty())) {
        return false;
    }
    if (kind == SessionMessageKind::PairFail
        && (!o.value(QStringLiteral("reason")).isString()
            || !isWholeNumberIn(o.value(QStringLiteral("retryAfterMs")), 0.0, 2147483647.0))) {
        return false;
    }
    // iPhone app Task 74: confirm.request and notice carry a whole-number
    // id, a kind and a reason; the rest is checked as each reads it.
    if (kind == SessionMessageKind::ConfirmRequest || kind == SessionMessageKind::Notice) {
        if (!isWholeNumberIn(o.value(QStringLiteral("id")), 0.0, 9007199254740991.0)
            || !o.value(QStringLiteral("kind")).isString()
            || !o.value(QStringLiteral("reason")).isString()) {
            return false;
        }
    }
    if (kind == SessionMessageKind::ConfirmRequest
        && (!o.value(QStringLiteral("affected")).isArray()
            || !isWholeNumberIn(o.value(QStringLiteral("expiresInMs")), 0.0, 2147483647.0))) {
        return false;
    }
    if (kind == SessionMessageKind::Notice
        && (!isWholeNumberIn(o.value(QStringLiteral("secondsAgo")), 0.0, 9007199254740991.0)
            || !o.value(QStringLiteral("takeBack")).isBool())) {
        return false;
    }
    // Parity Task 19: a record batch names its stream and generation, and
    // every upsert carries an id and its fields; every remove is an id.
    if (kind == SessionMessageKind::RecordBatch) {
        if (!o.value(QStringLiteral("stream")).isString()
            || o.value(QStringLiteral("stream")).toString().isEmpty()
            || !isWholeNumberIn(o.value(QStringLiteral("generation")), 1.0, 9007199254740991.0)
            || !o.value(QStringLiteral("reset")).isBool()
            || !o.value(QStringLiteral("upserts")).isArray()
            || !o.value(QStringLiteral("removes")).isArray()) {
            return false;
        }
        for (const QJsonValue& u : o.value(QStringLiteral("upserts")).toArray()) {
            const QJsonObject upsert = u.toObject();
            if (!u.isObject() || !upsert.value(QStringLiteral("id")).isString()
                || upsert.value(QStringLiteral("id")).toString().isEmpty()
                || !upsert.value(QStringLiteral("fields")).isObject()) {
                return false;
            }
        }
        for (const QJsonValue& r : o.value(QStringLiteral("removes")).toArray()) {
            if (!r.isString() || r.toString().isEmpty()) {
                return false;
            }
        }
    }
    // iPhone app plan Task 29: path.join carries its ticket, a non-empty
    // string of at most kMaxPathTicketChars.
    if (kind == SessionMessageKind::PathJoin) {
        const QJsonValue ticket = o.value(QStringLiteral("ticket"));
        if (!ticket.isString() || ticket.toString().isEmpty()
            || ticket.toString().size() > kMaxPathTicketChars) {
            return false;
        }
    }
    // Task 11: CommandInvoke and CommandResult share "verb" and "id";
    // everything else is kind-specific. Same presence-and-type discipline
    // as every field above -- a missing "accepted" would otherwise decode
    // through QJsonValue::toBool()'s false default, indistinguishable from
    // a genuine, deliberate rejection.
    const bool needsVerb =
        kind == SessionMessageKind::CommandInvoke || kind == SessionMessageKind::CommandResult;
    if (needsVerb && !o.value(QStringLiteral("verb")).isString()) {
        return false;
    }
    if (needsVerb && !o.value(QStringLiteral("id")).isDouble()) {
        return false;
    }
    // Fix round 1 review finding (Important 2): range-check BEFORE the
    // narrowing static_cast<quint32> in the switch below, the same
    // discipline updateFromJson() and fieldFromJson() already apply to
    // MirrorUpdate/SessionSchemaField's quint16 ordinal (this file, above:
    // `ordinalRaw < 0.0 || ordinalRaw > 65535.0`, checked before the cast).
    // Without this, {"id":-1} or {"id":1e30} would both pass the isDouble()
    // gate above and then feed a floating-to-unsigned conversion of an
    // unrepresentable value into static_cast<quint32>, which is undefined
    // behaviour -- exactly what this file's own header comment promises
    // untrusted input never triggers. One check covers both CommandInvoke
    // and CommandResult, mirroring needsVerb's own single-check shape.
    if (needsVerb) {
        const double idRaw = o.value(QStringLiteral("id")).toDouble();
        if (idRaw < 0.0 || idRaw > 4294967295.0) { // quint32 max
            return false;
        }
    }
    if (kind == SessionMessageKind::CommandInvoke && !o.value(QStringLiteral("args")).isArray()) {
        return false;
    }
    if (kind == SessionMessageKind::CommandResult) {
        if (!o.value(QStringLiteral("accepted")).isBool()) {
            return false;
        }
        if (!o.value(QStringLiteral("reason")).isString()) {
            return false;
        }
        if (!o.value(QStringLiteral("affected")).isArray()) {
            return false;
        }
    }

    SessionMessage message;
    message.kind = kind;

    switch (kind) {
    case SessionMessageKind::StationTelemetry:
        if (!StationTelemetryCodec::decode(o.value(QStringLiteral("payload")).toObject(),
                                           &message.telemetry)) {
            return false;
        }
        break;
    case SessionMessageKind::MediaControl:
        message.mediaPayload = o.value(QStringLiteral("payload")).toObject();
        break;
    case SessionMessageKind::Schema: {
        message.className = o.value(QStringLiteral("class")).toString().toUtf8();
        const QJsonArray fields = o.value(QStringLiteral("fields")).toArray();
        message.fields.reserve(fields.size());
        for (const QJsonValue& v : fields) {
            SessionSchemaField f;
            if (!fieldFromJson(v, &f)) {
                return false;
            }
            message.fields.append(f);
        }
        break;
    }
    case SessionMessageKind::ObjectCreate:
    case SessionMessageKind::Delta: {
        message.objectKey = o.value(QStringLiteral("key")).toString().toUtf8();
        if (kind == SessionMessageKind::ObjectCreate) {
            message.className = o.value(QStringLiteral("class")).toString().toUtf8();
        }
        const QJsonArray props = o.value(QStringLiteral("properties")).toArray();
        message.updates.reserve(props.size());
        for (const QJsonValue& v : props) {
            MirrorUpdate u;
            if (!updateFromJson(v, &u)) {
                return false;
            }
            message.updates.append(u);
        }
        break;
    }
    case SessionMessageKind::ObjectDestroy:
        message.objectKey = o.value(QStringLiteral("key")).toString().toUtf8();
        message.className = o.value(QStringLiteral("class")).toString().toUtf8();
        break;
    case SessionMessageKind::SnapshotComplete:
        break;
    case SessionMessageKind::CommandInvoke: {
        message.commandVerb = o.value(QStringLiteral("verb")).toString().toUtf8();
        const bool strictCommandId = message.commandVerb.startsWith("nnr.")
            || message.commandVerb.startsWith("ps3.") || message.commandVerb.startsWith("dspAssets.")
            || message.commandVerb.startsWith("station.settingsExport.");
        const double commandId = o.value(QStringLiteral("id")).toDouble(0.0);
        if (strictCommandId && (!o.value(QStringLiteral("id")).isDouble()
            || !std::isfinite(commandId) || commandId < 1.0 || commandId > 4294967295.0
            || std::floor(commandId) != commandId)) {
            return false;
        }
        message.commandId = static_cast<quint32>(commandId);
        const QJsonArray args = o.value(QStringLiteral("args")).toArray();
        message.arguments.reserve(args.size());
        for (const QJsonValue& v : args) {
            MirrorUpdate u;
            if (!updateFromJson(v, &u)) {
                return false;
            }
            message.arguments.append(u);
        }
        break;
    }
    case SessionMessageKind::CommandResult: {
        message.commandVerb = o.value(QStringLiteral("verb")).toString().toUtf8();
        const bool strictCommandId = message.commandVerb.startsWith("nnr.")
            || message.commandVerb.startsWith("ps3.") || message.commandVerb.startsWith("dspAssets.")
            || message.commandVerb.startsWith("station.settingsExport.");
        const double commandId = o.value(QStringLiteral("id")).toDouble(0.0);
        if (strictCommandId && (!o.value(QStringLiteral("id")).isDouble()
            || !std::isfinite(commandId) || commandId < 1.0 || commandId > 4294967295.0
            || std::floor(commandId) != commandId)) {
            return false;
        }
        message.commandId = static_cast<quint32>(commandId);
        message.accepted = o.value(QStringLiteral("accepted")).toBool();
        message.reason = o.value(QStringLiteral("reason")).toString();
        const QJsonArray affected = o.value(QStringLiteral("affected")).toArray();
        message.affectedKeys.reserve(affected.size());
        for (const QJsonValue& v : affected) {
            // Same discipline as every other array element decoded in this
            // file: reject rather than coerce a non-string entry.
            if (!v.isString()) {
                return false;
            }
            message.affectedKeys.append(v.toString().toUtf8());
        }
        if (o.contains(QStringLiteral("values"))) {
            if (!o.value(QStringLiteral("values")).isArray()) {
                return false;
            }
            const QJsonArray values = o.value(QStringLiteral("values")).toArray();
            if (values.size() > 128) {
                return false;
            }
            QSet<QByteArray> names;
            for (const QJsonValue& value : values) {
                MirrorUpdate update;
                if (!updateFromJson(value, &update) || update.name.isEmpty()
                    || names.contains(update.name)) {
                    return false;
                }
                names.insert(update.name);
                message.updates.append(update);
            }
        }
        break;
    }
    case SessionMessageKind::Hello:
        message.protocolMajor =
            static_cast<quint16>(o.value(QStringLiteral("major")).toDouble());
        message.protocolMinor =
            static_cast<quint16>(o.value(QStringLiteral("minor")).toDouble());
        message.settingsSchemaVersion =
            static_cast<qint32>(o.value(QStringLiteral("settingsSchema")).toDouble());
        message.peerName = o.value(QStringLiteral("peer")).toString();
        // Validated above. Absent `majors` means [major]; absent
        // `features` means none.
        message.majorsOnWire = o.contains(QStringLiteral("majors"));
        message.featuresOnWire = o.contains(QStringLiteral("features"));
        message.supportedMajors.clear();
        if (message.majorsOnWire) {
            for (const QJsonValue& major : o.value(QStringLiteral("majors")).toArray()) {
                message.supportedMajors.append(static_cast<quint16>(major.toDouble()));
            }
        } else {
            message.supportedMajors.append(message.protocolMajor);
        }
        message.features.clear();
        if (message.featuresOnWire) {
            const QJsonObject declared = o.value(QStringLiteral("features")).toObject();
            for (auto it = declared.constBegin(); it != declared.constEnd(); ++it) {
                message.features.insert(it.key().toUtf8(),
                                        static_cast<int>(it.value().toDouble()));
            }
        }
        message.stationIdentity.reset();
        if (o.contains(QStringLiteral("identity"))) {
            const QJsonObject identity = o.value(QStringLiteral("identity")).toObject();
            message.stationIdentity = SessionStationIdentity{
                identity.value(QStringLiteral("publicKey")).toString(),
                identity.value(QStringLiteral("certBinding")).toString(),
            };
        }
        message.challenge = o.value(QStringLiteral("challenge")).toString();
        break;
    case SessionMessageKind::AuthRequest:
        message.token = o.value(QStringLiteral("token")).toString();
        message.device.reset();
        if (o.contains(QStringLiteral("device"))) {
            const QJsonObject device = o.value(QStringLiteral("device")).toObject();
            message.device = SessionDeviceBlock{
                device.value(QStringLiteral("id")).toString(),
                device.value(QStringLiteral("publicKey")).toString(),
                device.value(QStringLiteral("name")).toString(),
                device.value(QStringLiteral("kind")).toString(),
                device.value(QStringLiteral("signature")).toString(),
                device.value(QStringLiteral("shortName")).toString(),
            };
        }
        break;
    case SessionMessageKind::AuthResult:
        message.accepted = o.value(QStringLiteral("accepted")).toBool();
        message.reason = o.value(QStringLiteral("reason")).toString();
        // Read LENIENTLY, unlike "accepted" and "reason" above, which the
        // validator requires. See SessionMessage::retryable: a peer built
        // before this field existed sends no such key, and absent has to
        // mean "assume permanent" -- the safe direction, and exactly what
        // that peer's own client half did.
        message.retryable = o.value(QStringLiteral("retryable")).toBool();
        message.endCode = o.value(QStringLiteral("code")).toString();
        break;
    case SessionMessageKind::SessionEnd:
        message.reason = o.value(QStringLiteral("reason")).toString();
        message.retryable = o.value(QStringLiteral("retryable")).toBool();
        message.endCode = o.value(QStringLiteral("code")).toString();
        if (o.contains(QStringLiteral("takenOverBy"))) {
            message.takenOverBy = o.value(QStringLiteral("takenOverBy")).toString();
        }
        if (o.contains(QStringLiteral("takenOverById"))) {
            message.takenOverById = o.value(QStringLiteral("takenOverById")).toString();
        }
        if (o.contains(QStringLiteral("secondsAgo"))) {
            message.secondsAgo = static_cast<qint64>(o.value(QStringLiteral("secondsAgo")).toDouble());
        }
        break;
    case SessionMessageKind::SessionHeld:
        message.heldDevices = o.value(QStringLiteral("devices")).toArray();
        message.heldRevision = static_cast<quint32>(o.value(QStringLiteral("revision")).toDouble());
        if (o.contains(QStringLiteral("placeTaken"))) {
            message.placeTaken = o.value(QStringLiteral("placeTaken")).toObject();
        }
        if (o.contains(QStringLiteral("placeFreed"))) {
            message.placeFreed = o.value(QStringLiteral("placeFreed")).toObject();
        }
        break;
    case SessionMessageKind::SessionTakeover:
        message.takeoverDeviceId = o.value(QStringLiteral("deviceId")).toString();
        message.heldRevision = static_cast<quint32>(o.value(QStringLiteral("revision")).toDouble());
        break;
    case SessionMessageKind::PropertyResult: {
        message.objectKey = o.value(QStringLiteral("key")).toString().toUtf8();
        message.writeId = static_cast<quint32>(o.value(QStringLiteral("writeId")).toDouble());
        const QJsonArray entries = o.value(QStringLiteral("results")).toArray();
        if (entries.size() > 512) {
            return false;
        }
        QSet<QByteArray> seen;
        for (const QJsonValue& v : entries) {
            if (!v.isObject()) {
                return false;
            }
            const QJsonObject entry = v.toObject();
            if (!entry.value(QStringLiteral("property")).isString()
                || !entry.value(QStringLiteral("accepted")).isBool()
                || !entry.value(QStringLiteral("reason")).isString()
                || !entry.value(QStringLiteral("hasValue")).isBool()) {
                return false;
            }
            SessionPropertyResult result;
            result.property = entry.value(QStringLiteral("property")).toString().toUtf8();
            if (result.property.isEmpty() || seen.contains(result.property)) {
                return false;
            }
            seen.insert(result.property);
            result.accepted = entry.value(QStringLiteral("accepted")).toBool();
            result.reason = entry.value(QStringLiteral("reason")).toString();
            result.hasValue = entry.value(QStringLiteral("hasValue")).toBool();
            if (result.hasValue
                && (!updateFromJson(entry.value(QStringLiteral("value")), &result.value)
                    || result.value.name != result.property)) {
                return false;
            }
            if (result.accepted && (!result.hasValue || !result.reason.isEmpty())) {
                return false;
            }
            message.propertyResults.append(result);
        }
        break;
    }
    case SessionMessageKind::PairStart: {
        message.pairMode = o.value(QStringLiteral("mode")).toString();
        const QJsonObject device = o.value(QStringLiteral("device")).toObject();
        message.pairDevice = SessionPairDevice{
            device.value(QStringLiteral("publicKey")).toString(),
            device.value(QStringLiteral("name")).toString(),
            device.value(QStringLiteral("kind")).toString(),
        };
        break;
    }
    case SessionMessageKind::PairAccept: {
        const QJsonObject identity = o.value(QStringLiteral("identity")).toObject();
        message.stationIdentity = SessionStationIdentity{
            identity.value(QStringLiteral("publicKey")).toString(),
            identity.value(QStringLiteral("certBinding")).toString(),
        };
        message.pairLabel = o.value(QStringLiteral("label")).toString();
        break;
    }
    case SessionMessageKind::PairSpake:
        message.pairStep = static_cast<int>(o.value(QStringLiteral("step")).toDouble());
        message.pairData = o.value(QStringLiteral("data")).toString();
        break;
    case SessionMessageKind::PairConfirm:
        message.pairBox = o.value(QStringLiteral("box")).toString();
        break;
    case SessionMessageKind::PairFail:
        message.reason = o.value(QStringLiteral("reason")).toString();
        message.retryAfterMs =
            static_cast<qint64>(o.value(QStringLiteral("retryAfterMs")).toDouble());
        break;
    case SessionMessageKind::ConfirmRequest:
    case SessionMessageKind::Notice: {
        SessionPrompt& p = message.prompt;
        message.reason = o.value(QStringLiteral("reason")).toString();
        p.id = static_cast<qint64>(o.value(QStringLiteral("id")).toDouble());
        p.kind = o.value(QStringLiteral("kind")).toString();
        if (o.value(QStringLiteral("change")).isObject()) {
            p.change = o.value(QStringLiteral("change")).toObject();
        }
        if (kind == SessionMessageKind::ConfirmRequest) {
            p.affected = o.value(QStringLiteral("affected")).toArray();
            p.expiresInMs = static_cast<qint64>(o.value(QStringLiteral("expiresInMs")).toDouble());
            if (o.value(QStringLiteral("choices")).isArray()) {
                p.choices = o.value(QStringLiteral("choices")).toArray();
            }
            if (o.value(QStringLiteral("forCommandId")).isDouble()) {
                p.forCommandId =
                    static_cast<qint64>(o.value(QStringLiteral("forCommandId")).toDouble());
            }
            if (o.value(QStringLiteral("forWriteId")).isDouble()) {
                p.forWriteId = static_cast<qint64>(o.value(QStringLiteral("forWriteId")).toDouble());
            }
            p.forSettingsKey = o.value(QStringLiteral("forSettingsKey")).toString();
            if (o.value(QStringLiteral("holder")).isObject()) {
                p.holder = o.value(QStringLiteral("holder")).toObject();
            }
        } else {
            p.secondsAgo = static_cast<qint64>(o.value(QStringLiteral("secondsAgo")).toDouble());
            p.takeBack = o.value(QStringLiteral("takeBack")).toBool();
            p.byDeviceId = o.value(QStringLiteral("byDeviceId")).toString();
            p.byName = o.value(QStringLiteral("byName")).toString();
            p.byShortName = o.value(QStringLiteral("byShortName")).toString();
            p.byKind = o.value(QStringLiteral("byKind")).toString();
            p.bySource = o.value(QStringLiteral("bySource")).toString();
            if (o.value(QStringLiteral("slices")).isArray()) {
                p.slices = o.value(QStringLiteral("slices")).toArray();
            }
        }
        break;
    }
    case SessionMessageKind::PathJoin:
        message.pathTicket = o.value(QStringLiteral("ticket")).toString();
        break;
    case SessionMessageKind::PathSwitch:
        break;
    case SessionMessageKind::RecordBatch: {
        RecordBatch& b = message.recordBatch;
        b.stream = o.value(QStringLiteral("stream")).toString();
        b.generation = static_cast<quint64>(o.value(QStringLiteral("generation")).toDouble());
        b.reset = o.value(QStringLiteral("reset")).toBool();
        for (const QJsonValue& u : o.value(QStringLiteral("upserts")).toArray()) {
            const QJsonObject upsert = u.toObject();
            b.upserts.append({upsert.value(QStringLiteral("id")).toString(),
                              upsert.value(QStringLiteral("fields")).toObject()});
        }
        for (const QJsonValue& r : o.value(QStringLiteral("removes")).toArray()) {
            b.removes.append(r.toString());
        }
        break;
    }
    case SessionMessageKind::Capabilities:
    case SessionMessageKind::SettingsSnapshot:
    case SessionMessageKind::PropertyWrite:
    case SessionMessageKind::SettingsWrite:
    case SessionMessageKind::SettingsRemove:
    case SessionMessageKind::SettingsValue:
    case SessionMessageKind::SettingsReject: {
        if (kind != SessionMessageKind::Capabilities
            && kind != SessionMessageKind::SettingsSnapshot) {
            message.objectKey = o.value(QStringLiteral("key")).toString().toUtf8();
        }
        if (kind == SessionMessageKind::PropertyWrite) {
            message.writeId = static_cast<quint32>(o.value(QStringLiteral("writeId")).toDouble());
        }
        if (kind == SessionMessageKind::SettingsReject) {
            message.reason = o.value(QStringLiteral("reason")).toString();
        }
        if (kind == SessionMessageKind::SettingsWrite
            || kind == SessionMessageKind::SettingsValue) {
            message.originTag = o.value(QStringLiteral("origin")).toString();
        }
        const QJsonArray entries = o.value(QStringLiteral("properties")).toArray();
        message.updates.reserve(entries.size());
        for (const QJsonValue& v : entries) {
            MirrorUpdate u;
            if (!updateFromJson(v, &u)) {
                return false;
            }
            message.updates.append(u);
        }
        break;
    }
    }

    *out = message;
    return true;
}

} // namespace NereusSDR
