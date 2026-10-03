// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/RendezvousWire.cpp  (NereusSDR)
// =================================================================
//
// See RendezvousWire.h. The wire is
// docs/architecture/2026-09-23-rendezvous-v1.md.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-26: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-27: iPhone app plan Task 29 fix wave (R-IOS-16): relay.grant
//               (section 12.1), from the service to a station and a client.
//               J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
// =================================================================

#include "core/session/RendezvousWire.h"

#include "core/security/StationIdentity.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>

#include <cmath>

namespace NereusSDR::RendezvousWire {

namespace {

// Section 4.2 to 4.4's prefixes, each ending in one line feed.
constexpr char kIdPrefix[] = "NereusSDR rendezvous id v1\n";
constexpr char kRegisterPrefix[] = "NereusSDR rendezvous register v1\n";
constexpr char kIntroducePrefix[] = "NereusSDR introduce v1\n";

struct KindName {
    Kind kind;
    const char* name;
};

constexpr KindName kKindNames[] = {
    {Kind::Hello, "hello"},
    {Kind::Register, "register"},
    {Kind::Challenge, "challenge"},
    {Kind::Prove, "prove"},
    {Kind::Registered, "registered"},
    {Kind::Introduce, "introduce"},
    {Kind::Introduction, "introduction"},
    {Kind::Answer, "answer"},
    {Kind::Credentials, "credentials"},
    {Kind::Candidate, "candidate"},
    {Kind::IntroductionEnd, "introduction.end"},
    {Kind::NameplateClaim, "nameplate.claim"},
    {Kind::Nameplate, "nameplate"},
    {Kind::NameplateRelease, "nameplate.release"},
    {Kind::NameplateReleased, "nameplate.released"},
    {Kind::MailboxOpen, "mailbox.open"},
    {Kind::MailboxOpened, "mailbox.opened"},
    {Kind::Mailbox, "mailbox"},
    {Kind::MailboxClose, "mailbox.close"},
    {Kind::MailboxClosed, "mailbox.closed"},
    {Kind::Error, "error"},
    {Kind::RelayGrant, "relay.grant"},
};

Kind kindFromName(const QString& name)
{
    for (const KindName& entry : kKindNames) {
        if (name == QLatin1String(entry.name)) {
            return entry.kind;
        }
    }
    return Kind::Unknown;
}

// Section 5.3: the kinds each direction carries.
bool directionCarries(Direction direction, Kind kind)
{
    switch (direction) {
    case Direction::StationToService:
        return kind == Kind::Register || kind == Kind::Prove || kind == Kind::Answer
               || kind == Kind::Candidate || kind == Kind::NameplateClaim
               || kind == Kind::NameplateRelease || kind == Kind::Mailbox
               || kind == Kind::MailboxClose;
    case Direction::ClientToService:
        return kind == Kind::Introduce || kind == Kind::Candidate || kind == Kind::MailboxOpen
               || kind == Kind::Mailbox || kind == Kind::MailboxClose;
    case Direction::ServiceToStation:
        return kind == Kind::Hello || kind == Kind::Challenge || kind == Kind::Registered
               || kind == Kind::Introduction || kind == Kind::Credentials
               || kind == Kind::Candidate || kind == Kind::IntroductionEnd
               || kind == Kind::Nameplate || kind == Kind::NameplateReleased
               || kind == Kind::MailboxOpened || kind == Kind::Mailbox
               || kind == Kind::MailboxClosed || kind == Kind::Error
               || kind == Kind::RelayGrant;
    case Direction::ServiceToClient:
        return kind == Kind::Hello || kind == Kind::Answer || kind == Kind::Candidate
               || kind == Kind::IntroductionEnd || kind == Kind::MailboxOpened
               || kind == Kind::Mailbox || kind == Kind::MailboxClosed || kind == Kind::Error
               || kind == Kind::RelayGrant;
    }
    return false;
}

qsizetype utf8Bytes(const QString& text)
{
    return text.toUtf8().size();
}

// Section 5.2's `relayUrl`: printable ASCII, starting `wss://`.
bool isRelayUrl(const QString& text)
{
    if (!text.startsWith(QLatin1String("wss://"))) {
        return false;
    }
    for (const QChar c : text) {
        if (c.unicode() < 0x21 || c.unicode() > 0x7E) {
            return false;
        }
    }
    return true;
}

// Section 5.2's `relayToken`: `A-Z a-z 0-9 - _` only.
bool isRelayToken(const QString& text)
{
    for (const QChar c : text) {
        const ushort u = c.unicode();
        if (!((u >= 'A' && u <= 'Z') || (u >= 'a' && u <= 'z') || (u >= '0' && u <= '9')
              || u == '-' || u == '_')) {
            return false;
        }
    }
    return true;
}

// ── Field kinds (section 5.2), reading ──────────────────────────────────

class Reader {
public:
    Reader(const QJsonObject& object, QString* why) : m_object(object), m_why(why) {}

    bool fail(const char* key, const char* what)
    {
        if (m_why != nullptr) {
            *m_why = QStringLiteral("%1: %2").arg(QLatin1String(key), QLatin1String(what));
        }
        return false;
    }

    bool string(const char* key, qsizetype minBytes, qsizetype maxBytes, QString* out)
    {
        const QJsonValue value = m_object.value(QLatin1String(key));
        if (!value.isString()) {
            return fail(key, "missing or not a string");
        }
        const QString text = value.toString();
        const qsizetype bytes = utf8Bytes(text);
        if (bytes < minBytes || bytes > maxBytes) {
            return fail(key, "wrong length");
        }
        *out = text;
        return true;
    }

    // Strict base64url of exactly `bytes` bytes (section 4.1).
    bool binary(const char* key, int bytes, QByteArray* out)
    {
        const QJsonValue value = m_object.value(QLatin1String(key));
        if (!value.isString()) {
            return fail(key, "missing or not a string");
        }
        bool ok = false;
        const QByteArray decoded = StationIdentity::fromBase64Url(value.toString(), &ok);
        if (!ok || decoded.size() != bytes) {
            return fail(key, "not strict base64url of the right length");
        }
        *out = decoded;
        return true;
    }

    // A whole number: no fraction, not a boolean, within range.
    bool whole(const char* key, qint64 minimum, qint64 maximum, qint64* out)
    {
        const QJsonValue value = m_object.value(QLatin1String(key));
        if (!value.isDouble()) {
            return fail(key, "missing or not a number");
        }
        const double number = value.toDouble();
        if (!std::isfinite(number) || std::floor(number) != number
            || number < static_cast<double>(minimum) || number > static_cast<double>(maximum)) {
            return fail(key, "not a whole number in range");
        }
        *out = static_cast<qint64>(number);
        return true;
    }

    bool rid(const char* key, QString* out)
    {
        QString text;
        if (!string(key, kIdLength, kIdLength, &text) || !isRendezvousId(text)) {
            return fail(key, "not a rendezvous id");
        }
        *out = text;
        return true;
    }

    bool boolean(const char* key, bool* out)
    {
        const QJsonValue value = m_object.value(QLatin1String(key));
        if (!value.isBool()) {
            return fail(key, "missing or not true or false");
        }
        *out = value.toBool();
        return true;
    }

    bool code(const char* key, QString* out)
    {
        QString text;
        if (!string(key, 1, kMaxCodeBytes, &text)) {
            return false;
        }
        for (const QChar c : text) {
            const ushort u = c.unicode();
            if (!((u >= 'A' && u <= 'Z') || (u >= 'a' && u <= 'z'))) {
                return fail(key, "not letters");
            }
        }
        *out = text;
        return true;
    }

    bool candidate(const char* key, QString* out)
    {
        QString text;
        if (!string(key, 0, kMaxCandidateBytes, &text) || !isCandidate(text)) {
            return fail(key, "not a candidate");
        }
        *out = text;
        return true;
    }

    bool urls(const char* key, QStringList* out)
    {
        return urlsIn(m_object.value(QLatin1String(key)), key, out);
    }

    bool urlsIn(const QJsonValue& value, const char* key, QStringList* out)
    {
        if (!value.isArray()) {
            return fail(key, "missing or not a list");
        }
        const QJsonArray array = value.toArray();
        if (array.size() > kMaxUrls) {
            return fail(key, "too many addresses");
        }
        QStringList list;
        for (const QJsonValue& entry : array) {
            if (!entry.isString()) {
                return fail(key, "an address is not a string");
            }
            const qsizetype bytes = utf8Bytes(entry.toString());
            if (bytes < 1 || bytes > kMaxUrlBytes) {
                return fail(key, "an address has the wrong length");
            }
            list.append(entry.toString());
        }
        *out = list;
        return true;
    }

    // Section 5.2's `relayUrl`: printable ASCII (0x21 to 0x7E), starting
    // `wss://`.
    bool relayUrl(const char* key, QString* out)
    {
        QString text;
        if (!string(key, 1, kMaxRelayUrlBytes, &text) || !isRelayUrl(text)) {
            return fail(key, "not a wss:// address");
        }
        *out = text;
        return true;
    }

    // Section 5.2's `relayToken`: the base64url alphabet only, no padding.
    bool relayToken(const char* key, QString* out)
    {
        QString text;
        if (!string(key, 1, kMaxRelayTokenBytes, &text) || !isRelayToken(text)) {
            return fail(key, "not a relay token");
        }
        *out = text;
        return true;
    }

    // `turn`: null, or an object of its four keys (any other key ignored).
    bool turn(const char* key, std::optional<Turn>* out)
    {
        const QJsonValue value = m_object.value(QLatin1String(key));
        if (value.isNull()) {
            *out = std::nullopt;
            return true;
        }
        if (!value.isObject()) {
            return fail(key, "not null or an object");
        }
        Reader inner(value.toObject(), m_why);
        Turn turn;
        qint64 expires = 0;
        if (!inner.string("username", 1, kMaxTurnUsernameBytes, &turn.username)
            || !inner.string("password", 1, kMaxTurnPasswordBytes, &turn.password)
            || !inner.whole("expires", 0, kMaxTurnExpires, &expires)
            || !inner.urls("urls", &turn.urls)) {
            return false;
        }
        turn.expires = expires;
        *out = turn;
        return true;
    }

private:
    QJsonObject m_object;
    QString* m_why;
};

bool readFields(Direction direction, Kind kind, const QJsonObject& object, Message* out,
                QString* why)
{
    Reader r(object, why);
    const bool toStation = direction == Direction::ServiceToStation;
    const bool fromStation = direction == Direction::StationToService;
    qint64 number = 0;
    switch (kind) {
    case Kind::Hello:
        if (!r.whole("version", 1, 65535, &number)) {
            return false;
        }
        out->version = static_cast<int>(number);
        return r.binary("nonce", kNonceBytes, &out->nonce) && r.urls("stun", &out->stun);
    case Kind::Register:
        return r.rid("id", &out->id) && r.binary("publicKey", kPublicKeyBytes, &out->publicKey);
    case Kind::Challenge:
        return r.binary("nonce", kNonceBytes, &out->nonce);
    case Kind::Prove:
        return r.binary("signature", kSignatureBytes, &out->signature);
    case Kind::Registered:
        return r.rid("id", &out->id);
    case Kind::Introduce:
        return r.rid("id", &out->id) && r.binary("device", kDeviceIdBytes, &out->device)
               && r.binary("deviceSignature", kSignatureBytes, &out->deviceSignature)
               && r.string("offer", 1, kMaxSdpBytes, &out->sdp);
    case Kind::Introduction:
        return r.binary("from", kIntroBytes, &out->intro)
               && r.binary("device", kDeviceIdBytes, &out->device)
               && r.binary("deviceSignature", kSignatureBytes, &out->deviceSignature)
               && r.string("offer", 1, kMaxSdpBytes, &out->sdp)
               && r.binary("nonce", kNonceBytes, &out->nonce);
    case Kind::Answer:
        if (fromStation) {
            return r.binary("to", kIntroBytes, &out->intro)
                   && r.string("answer", 1, kMaxSdpBytes, &out->sdp)
                   && r.boolean("turn", &out->turnRequested);
        }
        return r.string("answer", 1, kMaxSdpBytes, &out->sdp) && r.turn("turn", &out->turn);
    case Kind::Credentials:
        return r.binary("from", kIntroBytes, &out->intro) && r.turn("turn", &out->turn);
    case Kind::Candidate:
        if (fromStation) {
            return r.binary("to", kIntroBytes, &out->intro)
                   && r.candidate("candidate", &out->candidate);
        }
        if (toStation) {
            return r.binary("from", kIntroBytes, &out->intro)
                   && r.candidate("candidate", &out->candidate);
        }
        return r.candidate("candidate", &out->candidate);
    case Kind::IntroductionEnd:
        if (toStation && !r.binary("from", kIntroBytes, &out->intro)) {
            return false;
        }
        return r.code("code", &out->code);
    case Kind::Nameplate:
    case Kind::MailboxOpen:
    case Kind::MailboxOpened:
        if (!r.whole("nameplate", 1, kMaxNameplate, &number)) {
            return false;
        }
        out->nameplate = static_cast<int>(number);
        return true;
    case Kind::Mailbox:
        return r.string("body", 1, kMaxBodyBytes, &out->body);
    case Kind::MailboxClosed:
        return r.code("code", &out->code);
    case Kind::Error:
        if (!r.code("code", &out->code) || !r.string("reason", 1, kMaxReasonBytes, &out->reason)
            || !r.whole("retryAfterMs", 0, kMaxRetryAfterMs, &number)) {
            return false;
        }
        out->retryAfterMs = number;
        return true;
    case Kind::RelayGrant:
        if (toStation && !r.binary("from", kIntroBytes, &out->intro)) {
            return false;
        }
        if (!r.relayUrl("url", &out->relayUrl) || !r.relayToken("token", &out->relayToken)
            || !r.whole("expires", 0, kMaxTurnExpires, &number)) {
            return false;
        }
        out->relayExpires = number;
        return true;
    case Kind::NameplateClaim:
    case Kind::NameplateRelease:
    case Kind::NameplateReleased:
    case Kind::MailboxClose:
        return true;
    case Kind::Unknown:
        break;
    }
    return false;
}

// ── Writing ──────────────────────────────────────────────────────────────

bool fits(const QString& text, qsizetype minBytes, qsizetype maxBytes)
{
    // A lone surrogate is not a Unicode scalar value (section 5.1): never
    // sent.
    if (!QStringView(text).isValidUtf16()) {
        return false;
    }
    const qsizetype bytes = utf8Bytes(text);
    return bytes >= minBytes && bytes <= maxBytes;
}

QJsonValue b64(const QByteArray& bytes)
{
    return StationIdentity::toBase64Url(bytes);
}

} // namespace

QString kindName(Kind kind)
{
    for (const KindName& entry : kKindNames) {
        if (entry.kind == kind) {
            return QString::fromLatin1(entry.name);
        }
    }
    return {};
}

bool decode(Direction direction, const QByteArray& wire, Message* out, QString* why)
{
    Message message;
    if (out != nullptr) {
        *out = message;
    }
    if (wire.size() > kMaxIncomingMessageBytes) {
        if (why != nullptr) {
            *why = QStringLiteral("message too long");
        }
        return false;
    }
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(wire, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        if (why != nullptr) {
            *why = QStringLiteral("not a JSON object");
        }
        return false;
    }
    const QJsonObject object = document.object();
    const QJsonValue type = object.value(QStringLiteral("type"));
    if (!type.isString()) {
        if (why != nullptr) {
            *why = QStringLiteral("type: missing or not a string");
        }
        return false;
    }
    const Kind kind = kindFromName(type.toString());
    if (kind == Kind::Unknown || !directionCarries(direction, kind)) {
        if (why != nullptr) {
            *why = QStringLiteral("a kind this end does not take");
        }
        return false;
    }
    message.kind = kind;
    if (!readFields(direction, kind, object, &message, why)) {
        return false;
    }
    // Section 12.9: validate recognized optional extensions strictly; unknown
    // keys retain version-1 behavior. Zero/empty in Message means absent.
    Reader optional(object, why);
    if ((kind == Kind::Hello || kind == Kind::Register || kind == Kind::Introduce)
        && object.contains(QStringLiteral("watchRelayVersion"))) {
        qint64 version = 0;
        if (!optional.whole("watchRelayVersion", 1, 65535, &version)) {
            return false;
        }
        message.watchRelayVersion = static_cast<int>(version);
    }
    if (kind == Kind::RelayGrant && object.contains(QStringLiteral("watchToken"))
        && !optional.relayToken("watchToken", &message.watchToken)) {
        return false;
    }
    if (out != nullptr) {
        *out = message;
    }
    return true;
}

QByteArray encode(Direction direction, const Message& message)
{
    if (!directionCarries(direction, message.kind)) {
        return {};
    }
    const bool fromStation = direction == Direction::StationToService;
    const bool toStation = direction == Direction::ServiceToStation;
    const auto urlsFit = [](const QStringList& urls) {
        if (urls.size() > kMaxUrls) {
            return false;
        }
        for (const QString& url : urls) {
            if (!fits(url, 1, kMaxUrlBytes)) {
                return false;
            }
        }
        return true;
    };
    const auto codeFits = [](const QString& code) {
        if (code.isEmpty() || code.size() > kMaxCodeBytes) {
            return false;
        }
        for (const QChar c : code) {
            const ushort u = c.unicode();
            if (!((u >= 'A' && u <= 'Z') || (u >= 'a' && u <= 'z'))) {
                return false;
            }
        }
        return true;
    };
    // `turn`: null, or its four keys.
    const auto turnValue = [&urlsFit](const std::optional<Turn>& turn, bool* ok) -> QJsonValue {
        *ok = true;
        if (!turn) {
            return QJsonValue(QJsonValue::Null);
        }
        if (!fits(turn->username, 1, kMaxTurnUsernameBytes)
            || !fits(turn->password, 1, kMaxTurnPasswordBytes) || turn->expires < 0
            || turn->expires > kMaxTurnExpires || !urlsFit(turn->urls)) {
            *ok = false;
            return {};
        }
        return QJsonObject{
            {QStringLiteral("username"), turn->username},
            {QStringLiteral("password"), turn->password},
            {QStringLiteral("expires"), static_cast<double>(turn->expires)},
            {QStringLiteral("urls"), QJsonArray::fromStringList(turn->urls)},
        };
    };

    QJsonObject object;
    object.insert(QStringLiteral("type"), kindName(message.kind));
    switch (message.kind) {
    case Kind::Hello:
        if (message.version < 1 || message.version > 65535
            || message.nonce.size() != kNonceBytes || !urlsFit(message.stun)) {
            return {};
        }
        object.insert(QStringLiteral("version"), message.version);
        object.insert(QStringLiteral("nonce"), b64(message.nonce));
        object.insert(QStringLiteral("stun"), QJsonArray::fromStringList(message.stun));
        break;
    case Kind::Register:
        if (!isRendezvousId(message.id) || message.publicKey.size() != kPublicKeyBytes) {
            return {};
        }
        object.insert(QStringLiteral("id"), message.id);
        object.insert(QStringLiteral("publicKey"), b64(message.publicKey));
        break;
    case Kind::Challenge:
        if (message.nonce.size() != kNonceBytes) {
            return {};
        }
        object.insert(QStringLiteral("nonce"), b64(message.nonce));
        break;
    case Kind::Prove:
        if (message.signature.size() != kSignatureBytes) {
            return {};
        }
        object.insert(QStringLiteral("signature"), b64(message.signature));
        break;
    case Kind::Registered:
        if (!isRendezvousId(message.id)) {
            return {};
        }
        object.insert(QStringLiteral("id"), message.id);
        break;
    case Kind::Introduce:
        if (!isRendezvousId(message.id) || message.device.size() != kDeviceIdBytes
            || message.deviceSignature.size() != kSignatureBytes
            || !fits(message.sdp, 1, kMaxSdpBytes)) {
            return {};
        }
        object.insert(QStringLiteral("id"), message.id);
        object.insert(QStringLiteral("device"), b64(message.device));
        object.insert(QStringLiteral("deviceSignature"), b64(message.deviceSignature));
        object.insert(QStringLiteral("offer"), message.sdp);
        break;
    case Kind::Introduction:
        if (message.intro.size() != kIntroBytes || message.device.size() != kDeviceIdBytes
            || message.deviceSignature.size() != kSignatureBytes
            || !fits(message.sdp, 1, kMaxSdpBytes) || message.nonce.size() != kNonceBytes) {
            return {};
        }
        object.insert(QStringLiteral("from"), b64(message.intro));
        object.insert(QStringLiteral("device"), b64(message.device));
        object.insert(QStringLiteral("deviceSignature"), b64(message.deviceSignature));
        object.insert(QStringLiteral("offer"), message.sdp);
        object.insert(QStringLiteral("nonce"), b64(message.nonce));
        break;
    case Kind::Answer:
        if (!fits(message.sdp, 1, kMaxSdpBytes)) {
            return {};
        }
        if (fromStation) {
            if (message.intro.size() != kIntroBytes) {
                return {};
            }
            object.insert(QStringLiteral("to"), b64(message.intro));
            object.insert(QStringLiteral("answer"), message.sdp);
            object.insert(QStringLiteral("turn"), message.turnRequested);
        } else {
            bool ok = false;
            const QJsonValue turn = turnValue(message.turn, &ok);
            if (!ok) {
                return {};
            }
            object.insert(QStringLiteral("answer"), message.sdp);
            object.insert(QStringLiteral("turn"), turn);
        }
        break;
    case Kind::RelayGrant:
        if (!fits(message.relayUrl, 1, kMaxRelayUrlBytes) || !isRelayUrl(message.relayUrl)
            || !fits(message.relayToken, 1, kMaxRelayTokenBytes)
            || !isRelayToken(message.relayToken) || message.relayExpires < 0
            || message.relayExpires > kMaxTurnExpires) {
            return {};
        }
        if (direction == Direction::ServiceToStation) {
            if (message.intro.size() != kIntroBytes) {
                return {};
            }
            object.insert(QStringLiteral("from"), b64(message.intro));
        }
        object.insert(QStringLiteral("url"), message.relayUrl);
        object.insert(QStringLiteral("token"), message.relayToken);
        object.insert(QStringLiteral("expires"), static_cast<double>(message.relayExpires));
        break;
    case Kind::Credentials: {
        bool ok = false;
        const QJsonValue turn = turnValue(message.turn, &ok);
        if (!ok || message.intro.size() != kIntroBytes) {
            return {};
        }
        object.insert(QStringLiteral("from"), b64(message.intro));
        object.insert(QStringLiteral("turn"), turn);
        break;
    }
    case Kind::Candidate:
        if (!fits(message.candidate, 0, kMaxCandidateBytes) || !isCandidate(message.candidate)) {
            return {};
        }
        if (fromStation || toStation) {
            if (message.intro.size() != kIntroBytes) {
                return {};
            }
            object.insert(fromStation ? QStringLiteral("to") : QStringLiteral("from"),
                          b64(message.intro));
        }
        object.insert(QStringLiteral("candidate"), message.candidate);
        break;
    case Kind::IntroductionEnd:
        if (!codeFits(message.code)) {
            return {};
        }
        if (toStation) {
            if (message.intro.size() != kIntroBytes) {
                return {};
            }
            object.insert(QStringLiteral("from"), b64(message.intro));
        }
        object.insert(QStringLiteral("code"), message.code);
        break;
    case Kind::Nameplate:
    case Kind::MailboxOpen:
    case Kind::MailboxOpened:
        if (message.nameplate < 1 || message.nameplate > kMaxNameplate) {
            return {};
        }
        object.insert(QStringLiteral("nameplate"), message.nameplate);
        break;
    case Kind::Mailbox:
        if (!fits(message.body, 1, kMaxBodyBytes)) {
            return {};
        }
        object.insert(QStringLiteral("body"), message.body);
        break;
    case Kind::MailboxClosed:
        if (!codeFits(message.code)) {
            return {};
        }
        object.insert(QStringLiteral("code"), message.code);
        break;
    case Kind::Error:
        if (!codeFits(message.code) || !fits(message.reason, 1, kMaxReasonBytes)
            || message.retryAfterMs < 0 || message.retryAfterMs > kMaxRetryAfterMs) {
            return {};
        }
        object.insert(QStringLiteral("code"), message.code);
        object.insert(QStringLiteral("reason"), message.reason);
        object.insert(QStringLiteral("retryAfterMs"), static_cast<double>(message.retryAfterMs));
        break;
    case Kind::NameplateClaim:
    case Kind::NameplateRelease:
    case Kind::NameplateReleased:
    case Kind::MailboxClose:
        break;
    case Kind::Unknown:
        return {};
    }
    if ((message.kind == Kind::Hello || message.kind == Kind::Register
         || message.kind == Kind::Introduce) && message.watchRelayVersion != 0) {
        if (message.watchRelayVersion < 1 || message.watchRelayVersion > 65535) {
            return {};
        }
        object.insert(QStringLiteral("watchRelayVersion"), message.watchRelayVersion);
    }
    if (message.kind == Kind::RelayGrant && !message.watchToken.isEmpty()) {
        if (!fits(message.watchToken, 1, kMaxRelayTokenBytes)
            || !isRelayToken(message.watchToken)) {
            return {};
        }
        object.insert(QStringLiteral("watchToken"), message.watchToken);
    }
    QByteArray wire = QJsonDocument(object).toJson(QJsonDocument::Compact);
    // Section 2, the sender's rule: the whole message as sent. The
    // service's own messages are held to what a peer accepts.
    const qsizetype cap = (direction == Direction::StationToService
                           || direction == Direction::ClientToService)
        ? kMaxOutgoingMessageBytes
        : kMaxIncomingMessageBytes;
    if (wire.size() > cap) {
        return {};
    }
    return wire;
}

QString rendezvousId(const QByteArray& spki)
{
    if (spki.size() != kPublicKeyBytes) {
        return {};
    }
    const QByteArray digest = QCryptographicHash::hash(
        QByteArray(kIdPrefix) + spki, QCryptographicHash::Sha256);
    // RFC 4648 base32, standard alphabet, lowercased, no padding: five bits
    // a character, most significant first.
    static constexpr char kAlphabet[] = "abcdefghijklmnopqrstuvwxyz234567";
    QString id;
    quint32 buffer = 0;
    int bits = 0;
    for (const char byte : digest) {
        buffer = (buffer << 8) | static_cast<quint8>(byte);
        bits += 8;
        while (bits >= 5 && id.size() < kIdLength) {
            id.append(QLatin1Char(kAlphabet[(buffer >> (bits - 5)) & 0x1f]));
            bits -= 5;
        }
        if (id.size() >= kIdLength) {
            break;
        }
    }
    return id;
}

bool isRendezvousId(const QString& text)
{
    if (text.size() != kIdLength) {
        return false;
    }
    for (const QChar c : text) {
        const ushort u = c.unicode();
        if (!((u >= 'a' && u <= 'z') || (u >= '2' && u <= '7'))) {
            return false;
        }
    }
    return true;
}

QByteArray registerTranscript(const QByteArray& nonce)
{
    return QByteArray(kRegisterPrefix) + nonce;
}

QByteArray introduceTranscript(const QString& stationId, const QByteArray& nonce)
{
    return QByteArray(kIntroducePrefix) + stationId.toLatin1() + nonce;
}

bool isCandidate(const QString& candidate)
{
    if (candidate.isEmpty()) {
        return true;
    }
    return candidate.startsWith(QLatin1String("candidate:"))
           && utf8Bytes(candidate) <= kMaxCandidateBytes;
}

QString wireCandidate(const QString& fromIceLibrary)
{
    if (fromIceLibrary.startsWith(QLatin1String("a="))) {
        return fromIceLibrary.mid(2);
    }
    return fromIceLibrary;
}

} // namespace NereusSDR::RendezvousWire
