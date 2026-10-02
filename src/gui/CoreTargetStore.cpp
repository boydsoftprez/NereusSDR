// =================================================================
// src/gui/CoreTargetStore.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Remote-daemon R3 Task 4g.
//
// iPhone app Task 18 (R-IOS-08): the saved Cores live under
// ConnectionTargets/V2, which adds each Core's identity fingerprint. A
// ConnectionTargets/V1 document is migrated once, every record's trust
// details carried over exactly and its identity left empty, and is never
// read again while V2 exists. V1 itself stays, so a build from before V2
// still finds its own list, but it follows V2's forgets and edits (Part C
// fix wave, R2-M3): a forgotten Core's record, token and all, leaves V1
// too, and an edited one is edited there.
//
// iPhone app plan Task 27 (R-IOS-16), 2026-09-26: `lastAddresses` on a V2
// record, the Core's last good addresses (rememberAddress()). J.J. Boyd
// (KG4VCF), AI-assisted via Anthropic Claude Code.
//
// iPhone app plan Task 28 fix wave (R-IOS-16), 2026-09-26:
// `controlChannelVersion` on a V2 record, what the Core declared at the last
// sign-in (rememberControlChannelVersion()); absent until then. J.J. Boyd
// (KG4VCF), AI-assisted via Anthropic Claude Code.
// iPhone app plan Task 29 (R-IOS-16), 2026-09-27: `rendezvousId`,
// `relayAllowed` and `reachFromAnywhere` on a V2 record, each optional, so
// a connect can race the internet service beside the Core's addresses.
// J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// Desktop code-only pairing: V3 is authoritative and accepts authenticated
// addressless targets. Older V2 and V1 keys retain only their representable
// existing records after edits and forgets so credentials cannot reappear
// when an older app opens the profile.
// =================================================================

// 2026-10-01: Authenticated Core address inventory and reconnect learning.
// J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex. NereusSDR-original.

#include "gui/CoreTargetStore.h"

#include "core/AppSettings.h"
#include "core/security/StationIdentity.h"
#include "core/session/RendezvousWire.h"
#include "core/session/CoreAddresses.h"
#include "core/session/IceConfiguration.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QDateTime>
#include <QLatin1String>
#include <QRegularExpression>
#include <QSet>
#include <QUuid>
#include <QUrl>

#include <algorithm>
#include <utility>

namespace NereusSDR {
namespace {

constexpr auto kStorageKey = "ConnectionTargets/V3";
constexpr auto kV2StorageKey = "ConnectionTargets/V2";
constexpr auto kV1StorageKey = "ConnectionTargets/V1";
constexpr int kVersion = 3;
constexpr int kV2Version = 2;
constexpr int kV1Version = 1;
constexpr qsizetype kIdentityBytes = 32;
constexpr auto kLocalId = "local";
constexpr qsizetype kMaxDocumentBytes = 1024 * 1024;
constexpr qsizetype kMaxRecords = 128;

const QRegularExpression kIdPattern(
    QStringLiteral("^[A-Za-z0-9_-]{1,64}$"));

void setError(QString* error, const QString& text)
{
    if (error != nullptr) {
        *error = text;
    }
}

void clearError(QString* error)
{
    if (error != nullptr) {
        error->clear();
    }
}

bool hasString(const QJsonObject& object, const char* key, QString* value)
{
    const QJsonValue jsonValue = object.value(QLatin1String(key));
    if (!jsonValue.isString()) {
        return false;
    }
    *value = jsonValue.toString();
    return true;
}

bool isSavedId(const QString& id)
{
    return id != QLatin1String(kLocalId) && kIdPattern.match(id).hasMatch();
}

bool isBounded(const QString& text, qsizetype maximum)
{
    return text.size() <= maximum;
}

bool validateTarget(const SavedCoreTarget& target, QString* error, int version = kVersion)
{
    if (!isSavedId(target.id)) {
        setError(error, QStringLiteral("Saved Core target has an invalid identifier."));
        return false;
    }
    if (!isBounded(target.label, 512)
        || !isBounded(target.connection.url, 4096)
        || !isBounded(target.connection.token, 8192)
        || !isBounded(target.connection.fingerprint, 256)
        || !isBounded(target.lastRadioName, 512)
        || !isBounded(target.lastRadioMac, 64)) {
        setError(error, QStringLiteral("Saved Core target contains text that is too long."));
        return false;
    }
    if (target.connection.url.isEmpty()) {
        if (version < kVersion || !target.connection.isValidRemoteTarget()) {
            setError(error, QStringLiteral("Saved Core target needs a paired remote access route."));
            return false;
        }
    } else if (!RemoteStationOptions::isValidStationUrl(target.connection.url)) {
        setError(error, QStringLiteral("Saved Core target has an invalid Core address."));
        return false;
    }
    if (!target.connection.identityFingerprint.isEmpty()
        && target.connection.identityFingerprint.size() != kIdentityBytes) {
        setError(error, QStringLiteral("Saved Core target has an invalid Core identity."));
        return false;
    }
    if (!target.connection.rendezvousId.isEmpty()
        && !RendezvousWire::isRendezvousId(target.connection.rendezvousId)) {
        setError(error, QStringLiteral("Saved Core target has an invalid remote access route."));
        return false;
    }
    // iPhone app plan Task 27: the Core's last good addresses.
    if (target.connection.cachedAddresses.size() > RemoteStationOptions::kMaxCachedAddresses) {
        setError(error, QStringLiteral("Saved Core target has too many saved addresses."));
        return false;
    }
    for (const QString& address : target.connection.cachedAddresses) {
        if (!isBounded(address, 4096) || !RemoteStationOptions::isValidStationUrl(address)) {
            setError(error, QStringLiteral("Saved Core target has an invalid Core address."));
            return false;
        }
    }
    if (!target.connection.coreAddresses.isEmpty()) {
        if (target.connection.identityFingerprint.size() != kIdentityBytes
            || target.connection.coreAddresses.size() > CoreAddresses::kMaxAddresses) {
            setError(error, QStringLiteral("Saved Core target has invalid learned addresses."));
            return false;
        }
        QStringList endpoints;
        for (const QString& url : target.connection.coreAddresses) {
            const QUrl parsed(url);
            QString host = parsed.host();
            if (host.contains(QLatin1Char(':'))) { host = QLatin1Char('[') + host + QLatin1Char(']'); }
            endpoints.append(host + QLatin1Char(':') + QString::number(parsed.port()));
            if (parsed.scheme() != QLatin1String("wss") || !parsed.userInfo().isEmpty()
                || !parsed.query().isEmpty() || !parsed.fragment().isEmpty() || !parsed.path().isEmpty()) {
                setError(error, QStringLiteral("Saved Core target has invalid learned addresses."));
                return false;
            }
        }
        if (CoreTargetStore::parseCoreAddresses(CoreAddresses::toJson(endpoints))
            != target.connection.coreAddresses) {
            setError(error, QStringLiteral("Saved Core target has invalid learned addresses."));
            return false;
        }
    }
    return true;
}

bool validateDocument(const QList<SavedCoreTarget>& targets, const QString& selectedId,
                      QString* error, int version = kVersion)
{
    if (targets.size() > kMaxRecords) {
        setError(error, QStringLiteral("Saved Core targets document has too many records."));
        return false;
    }

    QSet<QString> ids;
    for (const SavedCoreTarget& target : targets) {
        if (!validateTarget(target, error, version)) {
            return false;
        }
        if (ids.contains(target.id)) {
            setError(error, QStringLiteral("Saved Core targets document has duplicate identifiers."));
            return false;
        }
        ids.insert(target.id);
    }

    if (selectedId != QLatin1String(kLocalId) && !ids.contains(selectedId)) {
        setError(error, QStringLiteral("Saved Core targets document has an invalid selection."));
        return false;
    }
    return true;
}

// A V1 record (kV1Version) carries no identity.
QJsonObject toJson(const SavedCoreTarget& target, int version)
{
    QJsonObject object{
        {QStringLiteral("id"), target.id},
        {QStringLiteral("label"), target.label},
        {QStringLiteral("url"), target.connection.url},
        {QStringLiteral("token"), target.connection.token},
        {QStringLiteral("fingerprint"), target.connection.fingerprint},
        {QStringLiteral("allowUnpinned"), target.connection.allowUnpinned},
        {QStringLiteral("lastRadioName"), target.lastRadioName},
        {QStringLiteral("lastRadioMac"), target.lastRadioMac},
    };
    if (version >= kVersion && !target.connection.coreAddresses.isEmpty()) {
        object.insert(QStringLiteral("coreAddresses"), QJsonArray::fromStringList(target.connection.coreAddresses));
    }
    if (version >= kV2Version) {
        object.insert(QStringLiteral("identity"),
                      StationIdentity::toBase64Url(target.connection.identityFingerprint));
        // iPhone app plan Task 27: absent when there are none, so a record
        // with none is written exactly as before.
        if (!target.connection.cachedAddresses.isEmpty()) {
            object.insert(QStringLiteral("lastAddresses"),
                          QJsonArray::fromStringList(target.connection.cachedAddresses));
        }
        // Task 28 fix wave: absent until a sign-in recorded it.
        if (target.connection.controlChannelVersion >= 0) {
            object.insert(QStringLiteral("controlChannelVersion"),
                          target.connection.controlChannelVersion);
        }
        if (target.connection.controlChannelVersion == 0
            && target.connection.negativeControlObservedMs >= 0) {
            object.insert(QStringLiteral("negativeControlObservedMs"),
                          static_cast<double>(target.connection.negativeControlObservedMs));
        }
        // Task 29: absent until a sign-in recorded them, and the
        // operator's choice only when it is off, so a record without them
        // is written exactly as before.
        if (!target.connection.rendezvousId.isEmpty()) {
            object.insert(QStringLiteral("rendezvousId"), target.connection.rendezvousId);
        }
        if (target.connection.relayAllowed >= 0) {
            object.insert(QStringLiteral("relayAllowed"), target.connection.relayAllowed == 1);
        }
        if (!target.connection.reachFromAnywhere) {
            object.insert(QStringLiteral("reachFromAnywhere"), false);
        }
        if (!target.autoConnect) {
            object.insert(QStringLiteral("autoConnect"), false);
        }
    }
    return object;
}

QString serialize(const QList<SavedCoreTarget>& targets, const QString& selectedId,
                  int version = kVersion)
{
    QJsonArray cores;
    for (const SavedCoreTarget& target : targets) {
        cores.append(toJson(target, version));
    }
    const QJsonObject document{
        {QStringLiteral("version"), version},
        {QStringLiteral("selectedId"), selectedId},
        {QStringLiteral("cores"), cores},
    };
    return QString::fromUtf8(QJsonDocument(document).toJson(QJsonDocument::Compact));
}

// V2 and V3 records carry the same fields; V3 also admits paired service-only
// records. V1 has no identity.
bool parseDocument(const QString& text, int expectedVersion, QList<SavedCoreTarget>* targets,
                   QString* selectedId, QString* error)
{
    if (text.size() > kMaxDocumentBytes) {
        setError(error, QStringLiteral("Saved Core targets document is too large."));
        return false;
    }
    const QByteArray utf8 = text.toUtf8();
    if (utf8.size() > kMaxDocumentBytes) {
        setError(error, QStringLiteral("Saved Core targets document is too large."));
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(utf8, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        setError(error, QStringLiteral("Saved Core targets document is malformed."));
        return false;
    }

    const QJsonObject root = document.object();
    const QJsonValue version = root.value(QStringLiteral("version"));
    const QJsonValue cores = root.value(QStringLiteral("cores"));
    if (!version.isDouble() || version.toDouble() != double(expectedVersion) || !cores.isArray()
        || !hasString(root, "selectedId", selectedId)) {
        setError(error, QStringLiteral("Saved Core targets document has an unsupported schema."));
        return false;
    }
    if (cores.toArray().size() > kMaxRecords) {
        setError(error, QStringLiteral("Saved Core targets document has too many records."));
        return false;
    }

    QList<SavedCoreTarget> parsed;
    parsed.reserve(cores.toArray().size());
    for (const QJsonValue& value : cores.toArray()) {
        if (!value.isObject()) {
            setError(error, QStringLiteral("Saved Core targets document has an invalid record."));
            return false;
        }
        const QJsonObject object = value.toObject();
        SavedCoreTarget target;
        if (!hasString(object, "id", &target.id)
            || !hasString(object, "label", &target.label)
            || !hasString(object, "url", &target.connection.url)
            || !hasString(object, "token", &target.connection.token)
            || !hasString(object, "fingerprint", &target.connection.fingerprint)
            || !hasString(object, "lastRadioName", &target.lastRadioName)
            || !hasString(object, "lastRadioMac", &target.lastRadioMac)
            || !object.value(QStringLiteral("allowUnpinned")).isBool()) {
            setError(error, QStringLiteral("Saved Core targets document has an invalid record."));
            return false;
        }
        target.connection.allowUnpinned =
            object.value(QStringLiteral("allowUnpinned")).toBool();
        if (expectedVersion >= kV2Version) {
            QString identity;
            bool decoded = true;
            if (!hasString(object, "identity", &identity)) {
                setError(error, QStringLiteral("Saved Core targets document has an invalid record."));
                return false;
            }
            target.connection.identityFingerprint =
                identity.isEmpty() ? QByteArray()
                                   : StationIdentity::fromBase64Url(identity, &decoded);
            if (!decoded || (!identity.isEmpty()
                             && target.connection.identityFingerprint.size() != kIdentityBytes)) {
                setError(error, QStringLiteral("Saved Core targets document has an invalid record."));
                return false;
            }
            // iPhone app plan Task 27: optional; a list of addresses.
            const QJsonValue addresses = object.value(QStringLiteral("lastAddresses"));
            if (!addresses.isUndefined()) {
                if (!addresses.isArray()) {
                    setError(error, QStringLiteral("Saved Core targets document has an invalid record."));
                    return false;
                }
                for (const QJsonValue& address : addresses.toArray()) {
                    if (!address.isString()) {
                        setError(error, QStringLiteral("Saved Core targets document has an invalid record."));
                        return false;
                    }
                    target.connection.cachedAddresses.append(address.toString());
                }
            }
            if (expectedVersion >= kVersion) {
                const QJsonValue learned = object.value(QStringLiteral("coreAddresses"));
                if (!learned.isUndefined()) {
                    if (!learned.isArray()) {
                        setError(error, QStringLiteral("Saved Core targets document has invalid learned addresses."));
                        return false;
                    }
                    for (const QJsonValue& value : learned.toArray()) {
                        if (!value.isString()) {
                            setError(error, QStringLiteral("Saved Core targets document has invalid learned addresses."));
                            return false;
                        }
                        target.connection.coreAddresses.append(value.toString());
                    }
                }
            }
            // Task 28 fix wave: optional; a whole number, at least 0.
            const QJsonValue channel = object.value(QStringLiteral("controlChannelVersion"));
            if (!channel.isUndefined()) {
                const double value = channel.toDouble(-1.0);
                if (!channel.isDouble() || value < 0.0 || value > 65535.0
                    || value != static_cast<double>(static_cast<int>(value))) {
                    setError(error, QStringLiteral("Saved Core targets document has an invalid record."));
                    return false;
                }
                target.connection.controlChannelVersion = static_cast<int>(value);
            }
            // A bad optional observation only ages out the negative result;
            // it never discards the saved Core or its trust binding.
            const QJsonValue observed = object.value(QStringLiteral("negativeControlObservedMs"));
            if (target.connection.controlChannelVersion == 0 && observed.isDouble()) {
                const double stamp = observed.toDouble();
                if (stamp >= 0.0 && stamp <= 9007199254740991.0
                    && stamp == static_cast<double>(static_cast<qint64>(stamp))) {
                    target.connection.negativeControlObservedMs = static_cast<qint64>(stamp);
                }
            }
            // Task 29: optional; a rendezvous id's form, and two booleans.
            const QJsonValue rendezvous = object.value(QStringLiteral("rendezvousId"));
            if (!rendezvous.isUndefined()) {
                if (!rendezvous.isString()
                    || !RendezvousWire::isRendezvousId(rendezvous.toString())) {
                    setError(error, QStringLiteral("Saved Core targets document has an invalid record."));
                    return false;
                }
                target.connection.rendezvousId = rendezvous.toString();
            }
            const QJsonValue relay = object.value(QStringLiteral("relayAllowed"));
            if (!relay.isUndefined()) {
                if (!relay.isBool()) {
                    setError(error, QStringLiteral("Saved Core targets document has an invalid record."));
                    return false;
                }
                target.connection.relayAllowed = relay.toBool() ? 1 : 0;
            }
            const QJsonValue anywhere = object.value(QStringLiteral("reachFromAnywhere"));
            if (!anywhere.isUndefined()) {
                if (!anywhere.isBool()) {
                    setError(error, QStringLiteral("Saved Core targets document has an invalid record."));
                    return false;
                }
                target.connection.reachFromAnywhere = anywhere.toBool();
            }
            const QJsonValue autoConnect = object.value(QStringLiteral("autoConnect"));
            if (!autoConnect.isUndefined()) {
                if (!autoConnect.isBool()) {
                    setError(error, QStringLiteral("Saved Core targets document has an invalid record."));
                    return false;
                }
                target.autoConnect = autoConnect.toBool();
            }
        }
        parsed.append(target);
    }

    if (!validateDocument(parsed, *selectedId, error, expectedVersion)) {
        return false;
    }
    *targets = std::move(parsed);
    return true;
}

} // namespace

CoreTargetStore::CoreTargetStore(AppSettings& settings, std::function<qint64()> clock)
    : m_settings(settings), m_clock(clock ? std::move(clock) : [] {
        return QDateTime::currentMSecsSinceEpoch();
    })
{
}

bool CoreTargetStore::load(QString* error)
{
    // Keep the last successfully loaded contents visible for presentation,
    // but never let a failed reload authorize a subsequent destructive write.
    m_loaded = false;
    const QString key = QLatin1String(kStorageKey);
    if (m_settings.contains(key)) {
        QList<SavedCoreTarget> parsedTargets;
        QString parsedSelectedId;
        if (!parseDocument(m_settings.value(key).toString(), kVersion, &parsedTargets,
                           &parsedSelectedId, error)) {
            return false;
        }
        bool repairedFuture = false;
        for (SavedCoreTarget& target : parsedTargets) {
            if (target.connection.controlChannelVersion == 0
                && target.connection.negativeControlObservedMs > m_clock()) {
                target.connection.negativeControlObservedMs = -1;
                repairedFuture = true;
            }
        }
        if (repairedFuture && !persist(parsedTargets, parsedSelectedId, error)) { return false; }
        m_targets = std::move(parsedTargets);
        m_selectedId = std::move(parsedSelectedId);
        m_loaded = true;
        clearError(error);
        return true;
    }

    // V3 is authoritative once present. Migrate the prior document exactly
    // once, keeping its bytes as a rollback snapshot for an older app.
    const QString v2Key = QLatin1String(kV2StorageKey);
    if (m_settings.contains(v2Key)) {
        QList<SavedCoreTarget> v2Targets;
        QString v2SelectedId;
        if (!parseDocument(m_settings.value(v2Key).toString(), kV2Version, &v2Targets,
                           &v2SelectedId, error)
            || !persist(v2Targets, v2SelectedId, error)) {
            return false;
        }
        m_targets = std::move(v2Targets);
        m_selectedId = std::move(v2SelectedId);
        m_loaded = true;
        clearError(error);
        return true;
    }

    // iPhone app Task 18: the V1 list moves to V3 via a V2 rollback copy,
    // each record as it
    // was (address, token, pin, bench flag, label, last radio) and with no
    // identity yet; the key is enrolled on the Core's next token sign-in.
    // A V1 document that cannot be read writes nothing, like a bad V2.
    const QString v1Key = QLatin1String(kV1StorageKey);
    if (m_settings.contains(v1Key)) {
        QList<SavedCoreTarget> v1Targets;
        QString v1SelectedId;
        if (!parseDocument(m_settings.value(v1Key).toString(), kV1Version, &v1Targets,
                           &v1SelectedId, error)) {
            return false;
        }
        if (!persist(v1Targets, v1SelectedId, error)) {
            return false;
        }
        m_targets = std::move(v1Targets);
        m_selectedId = std::move(v1SelectedId);
        m_loaded = true;
        clearError(error);
        return true;
    }

    const QString legacyUrl =
        m_settings.value(QStringLiteral("RemoteStationUrl"), QString()).toString();
    if (!legacyUrl.isEmpty() && !RemoteStationOptions::isValidStationUrl(legacyUrl)) {
        setError(error, QStringLiteral("Legacy Core address is invalid; correct it before migrating."));
        return false;
    }

    QList<SavedCoreTarget> migratedTargets;
    QString migratedSelectedId = QLatin1String(kLocalId);
    if (!legacyUrl.isEmpty()) {
        SavedCoreTarget migrated;
        migrated.id = QStringLiteral("legacy-core");
        migrated.label = QUrl(legacyUrl, QUrl::StrictMode).host();
        migrated.connection.url = legacyUrl;
        migrated.connection.token =
            m_settings.value(QStringLiteral("RemoteStationToken"), QString()).toString();
        migrated.connection.fingerprint =
            m_settings.value(QStringLiteral("RemoteStationFingerprint"), QString()).toString();
        migrated.connection.allowUnpinned =
            m_settings.value(QStringLiteral("RemoteStationAllowUnpinned"),
                             QStringLiteral("False")).toString()
            == QStringLiteral("True");
        migratedTargets.append(migrated);
        migratedSelectedId = migrated.id;
    }

    if (!persist(migratedTargets, migratedSelectedId, error)) {
        return false;
    }
    m_targets = std::move(migratedTargets);
    m_selectedId = std::move(migratedSelectedId);
    m_loaded = true;
    clearError(error);
    return true;
}

QList<SavedCoreTarget> CoreTargetStore::targets() const
{
    return m_targets;
}

std::optional<SavedCoreTarget> CoreTargetStore::target(const QString& id) const
{
    for (const SavedCoreTarget& candidate : m_targets) {
        if (candidate.id == id) {
            return candidate;
        }
    }
    return std::nullopt;
}

QString CoreTargetStore::selectedId() const
{
    return m_selectedId;
}

bool CoreTargetStore::upsert(const SavedCoreTarget& target, QString* error)
{
    if (!m_loaded) {
        setError(error, QStringLiteral("Saved Core targets must be loaded before changes can be made."));
        return false;
    }
    if (!validateTarget(target, error)) {
        return false;
    }

    QList<SavedCoreTarget> updated = m_targets;
    bool replaced = false;
    for (SavedCoreTarget& existing : updated) {
        if (existing.id == target.id) {
            existing = target;
            replaced = true;
            break;
        }
    }
    if (!replaced) {
        if (updated.size() >= kMaxRecords) {
            setError(error, QStringLiteral("Saved Core target limit has been reached."));
            return false;
        }
        updated.append(target);
    }

    if (!persist(updated, m_selectedId, error)) {
        return false;
    }
    m_targets = std::move(updated);
    clearError(error);
    return true;
}

bool CoreTargetStore::rememberAddress(const QString& id, const QString& url, QString* error)
{
    const std::optional<SavedCoreTarget> found = target(id);
    if (!found) {
        setError(error, QStringLiteral("Saved Core target was not found."));
        return false;
    }
    if (!RemoteStationOptions::isValidStationUrl(url)) {
        setError(error, QStringLiteral("Saved Core target has an invalid Core address."));
        return false;
    }
    SavedCoreTarget updated = *found;
    QStringList& addresses = updated.connection.cachedAddresses;
    if (!addresses.isEmpty() && addresses.first() == url) {
        clearError(error);
        return true;
    }
    addresses.removeAll(url);
    addresses.prepend(url);
    while (addresses.size() > RemoteStationOptions::kMaxCachedAddresses) {
        addresses.removeLast();
    }
    return upsert(updated, error);
}

// Existing link 7.1 Core address contract, matching accepted phone
// CoreAddressList at b2383c082; reuse Core's global IP rules rather than
// importing Swift. Listener ports are supplied here, never ICE candidates.
QStringList CoreTargetStore::parseCoreAddresses(const QString& text)
{
    if (text.size() > 65536) { return {}; }
    const QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8());
    if (!doc.isObject() || !doc.object().value(QStringLiteral("addresses")).isArray()) { return {}; }
    QStringList result;
    const QRegularExpression pattern(QStringLiteral("^(?:\\[([0-9A-Fa-f:]+)\\]|([0-9.]+)):([0-9]{1,5})$"));
    for (const QJsonValue& value : doc.object().value(QStringLiteral("addresses")).toArray()) {
        if (!value.isString()) { continue; }
        const auto match = pattern.match(value.toString());
        if (!match.hasMatch()) { continue; }
        const QString host = match.captured(1).isEmpty() ? match.captured(2) : match.captured(1);
        const QHostAddress ip(host);
        const int port = match.captured(3).toInt();
        if (port < 1 || port > 65535
            || !(CoreAddresses::isPublicIpv4(ip)
                 || (ip.protocol() == QAbstractSocket::IPv6Protocol && IceConfiguration::isUsableLocalAddress(ip)))) { continue; }
        // Bracket syntax must match address family, with no mapped IPv4.
        if ((match.captured(1).isEmpty()) != (ip.protocol() == QAbstractSocket::IPv4Protocol)) { continue; }
        QUrl url;
        url.setScheme(QStringLiteral("wss"));
        url.setHost(ip.toString().toLower());
        url.setPort(port);
        const QString canonical = url.toString();
        if (!result.contains(canonical)) { result.append(canonical); }
        if (result.size() == CoreAddresses::kMaxAddresses) { break; }
    }
    return result;
}

bool CoreTargetStore::rememberCoreAddresses(const QString& id, const QByteArray& identity,
                                           const QString& text, QString* error)
{
    const auto found = target(id);
    if (!found || identity.size() != kIdentityBytes || found->connection.identityFingerprint != identity) {
        setError(error, QStringLiteral("Saved Core identity no longer matches this connection."));
        return false;
    }
    const QStringList addresses = parseCoreAddresses(text);
    if (addresses.isEmpty() || addresses == found->connection.coreAddresses) {
        clearError(error);
        return true;
    }
    SavedCoreTarget updated = *found;
    updated.connection.coreAddresses = addresses;
    return upsert(updated, error);
}

bool CoreTargetStore::rememberControlChannelVersion(const QString& id, int version,
                                                   QString* error)
{
    const std::optional<SavedCoreTarget> found = target(id);
    if (!found) {
        setError(error, QStringLiteral("Saved Core target was not found."));
        return false;
    }
    const int recorded = std::max(0, version);
    const qint64 observed = recorded == 0 ? m_clock() : -1;
    if (found->connection.controlChannelVersion == recorded
        && found->connection.negativeControlObservedMs == observed) {
        clearError(error);
        return true;
    }
    SavedCoreTarget updated = *found;
    updated.connection.controlChannelVersion = recorded;
    updated.connection.negativeControlObservedMs = observed;
    return upsert(updated, error);
}

bool CoreTargetStore::invalidateNegativeControlObservations(QString* error)
{
    QList<SavedCoreTarget> updated = m_targets;
    bool changed = false;
    for (SavedCoreTarget& target : updated) {
        if (target.connection.controlChannelVersion == 0
            && target.connection.negativeControlObservedMs >= 0) {
            target.connection.negativeControlObservedMs = -1;
            changed = true;
        }
    }
    if (!changed) { clearError(error); return true; }
    if (!persist(updated, m_selectedId, error)) { return false; }
    m_targets = std::move(updated);
    clearError(error);
    return true;
}

bool CoreTargetStore::invalidateFutureNegativeObservations(QString* error)
{
    QList<SavedCoreTarget> updated = m_targets;
    bool changed = false;
    const qint64 now = m_clock();
    for (SavedCoreTarget& target : updated) {
        if (target.connection.controlChannelVersion == 0
            && target.connection.negativeControlObservedMs > now) {
            target.connection.negativeControlObservedMs = -1;
            changed = true;
        }
    }
    if (!changed) { clearError(error); return true; }
    if (!persist(updated, m_selectedId, error)) { return false; }
    m_targets = std::move(updated);
    clearError(error);
    return true;
}

bool CoreTargetStore::observeNetworkFingerprint(const QString& fingerprint, QString* error)
{
    if (!m_networkFingerprint) {
        m_networkFingerprint = fingerprint;
        clearError(error);
        return true;
    }
    if (*m_networkFingerprint == fingerprint) {
        clearError(error);
        return true;
    }
    if (!invalidateNegativeControlObservations(error)) { return false; }
    m_networkFingerprint = fingerprint;
    return true;
}

bool CoreTargetStore::rememberServiceRoute(const QString& id, const QString& rendezvousId,
                                           int relayAllowed, QString* error)
{
    const std::optional<SavedCoreTarget> found = target(id);
    if (!found) {
        setError(error, QStringLiteral("Saved Core target was not found."));
        return false;
    }
    SavedCoreTarget updated = *found;
    if (RendezvousWire::isRendezvousId(rendezvousId)) {
        updated.connection.rendezvousId = rendezvousId;
    }
    if (relayAllowed >= 0) {
        updated.connection.relayAllowed = relayAllowed > 0 ? 1 : 0;
    }
    if (updated.connection.rendezvousId == found->connection.rendezvousId
        && updated.connection.relayAllowed == found->connection.relayAllowed) {
        clearError(error);
        return true;
    }
    return upsert(updated, error);
}

bool CoreTargetStore::remove(const QString& id, QString* error)
{
    if (!m_loaded) {
        setError(error, QStringLiteral("Saved Core targets must be loaded before changes can be made."));
        return false;
    }
    QList<SavedCoreTarget> updated = m_targets;
    auto it = std::find_if(updated.begin(), updated.end(), [&id](const SavedCoreTarget& target) {
        return target.id == id;
    });
    if (it == updated.end()) {
        setError(error, QStringLiteral("Saved Core target does not exist."));
        return false;
    }
    updated.erase(it);
    const QString updatedSelectedId = id == m_selectedId
        ? QLatin1String(kLocalId) : m_selectedId;

    if (!persist(updated, updatedSelectedId, error)) {
        return false;
    }
    m_targets = std::move(updated);
    m_selectedId = updatedSelectedId;
    clearError(error);
    return true;
}

bool CoreTargetStore::select(const QString& id, QString* error)
{
    if (!m_loaded) {
        setError(error, QStringLiteral("Saved Core targets must be loaded before changes can be made."));
        return false;
    }
    if (id != QLatin1String(kLocalId) && !target(id).has_value()) {
        setError(error, QStringLiteral("Saved Core target does not exist."));
        return false;
    }
    if (!persist(m_targets, id, error)) {
        return false;
    }
    m_selectedId = id;
    clearError(error);
    return true;
}

QString CoreTargetStore::createId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

bool CoreTargetStore::persist(const QList<SavedCoreTarget>& targets,
                              const QString& selectedId, QString* error)
{
    if (!validateDocument(targets, selectedId, error)) {
        return false;
    }

    const QString serialized = serialize(targets, selectedId);
    if (serialized.size() > kMaxDocumentBytes
        || serialized.toUtf8().size() > kMaxDocumentBytes) {
        setError(error, QStringLiteral("Saved Core targets document is too large."));
        return false;
    }

    const QString key = QLatin1String(kStorageKey);
    const bool hadPreviousValue = m_settings.contains(key);
    const QVariant previousValue = m_settings.value(key);
    struct OlderWrite {
        QString key;
        QVariant previous;
        bool had = false;
        bool rewrite = false;
        bool remove = false;
        QString serialized;
    };
    QList<OlderWrite> older;
    for (const auto& [name, version] :
         {std::pair{kV2StorageKey, kV2Version}, std::pair{kV1StorageKey, kV1Version}}) {
        OlderWrite write;
        write.key = QString::fromLatin1(name);
        write.had = m_settings.contains(write.key);
        write.previous = m_settings.value(write.key);
        if (write.had && hadPreviousValue) {
            QList<SavedCoreTarget> before;
            QString oldSelection;
            if (!parseDocument(write.previous.toString(), version, &before, &oldSelection,
                               nullptr)) {
                write.remove = true;
            } else {
                QList<SavedCoreTarget> kept;
                for (const SavedCoreTarget& old : std::as_const(before)) {
                    const auto current = std::find_if(
                        targets.cbegin(), targets.cend(),
                        [&old](const SavedCoreTarget& candidate) {
                            return candidate.id == old.id && !candidate.connection.url.isEmpty();
                        });
                    if (current == targets.cend()) { continue; }
                    SavedCoreTarget carried = *current;
                    if (version == kV1Version) {
                        carried.connection.identityFingerprint.clear();
                    }
                    kept.append(carried);
                }
                const bool selectionKept = oldSelection == QLatin1String(kLocalId)
                    || std::any_of(kept.cbegin(), kept.cend(),
                                   [&oldSelection](const SavedCoreTarget& candidate) {
                                       return candidate.id == oldSelection;
                                   });
                const QString keptSelection = selectionKept ? oldSelection
                                                             : QString::fromLatin1(kLocalId);
                write.serialized = serialize(kept, keptSelection, version);
                write.rewrite = write.serialized != serialize(before, oldSelection, version);
            }
        } else if (!write.had && !hadPreviousValue && version == kV2Version) {
            // A fresh V3 store or a V1 import also leaves a V2-readable
            // rollback copy. The first V3 save cannot contain new service-
            // only records, because it is a migration of older data.
            if (!validateDocument(targets, selectedId, error, kV2Version)) { return false; }
            write.serialized = serialize(targets, selectedId, kV2Version);
            write.rewrite = true;
        }
        older.append(std::move(write));
    }
    m_settings.setValue(key, serialized);
    for (const OlderWrite& write : std::as_const(older)) {
        if (write.rewrite) { m_settings.setValue(write.key, write.serialized); }
        else if (write.remove) { m_settings.remove(write.key); }
    }

    QString saveError;
    if (m_settings.save(&saveError)) {
        return true;
    }

    if (hadPreviousValue) {
        m_settings.setValue(key, previousValue);
    } else {
        m_settings.remove(key);
    }
    for (const OlderWrite& write : std::as_const(older)) {
        if (!write.rewrite && !write.remove) { continue; }
        if (write.had) { m_settings.setValue(write.key, write.previous); }
        else { m_settings.remove(write.key); }
    }
    setError(error, QStringLiteral("Saved Core targets could not be saved."));
    return false;
}

} // namespace NereusSDR
