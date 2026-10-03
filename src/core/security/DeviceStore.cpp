// no-port-check: NereusSDR-original.
// =================================================================
// src/core/security/DeviceStore.cpp  (NereusSDR)
// =================================================================
// See DeviceStore.h for the design (pairing design section 7).
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-24: Part C fix wave: the optional device shortName in
//               auth.request, stored with the device. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-24: Part C fix wave (security Minors R1-M1, M2, M4,
//               M5): the confirm-step recheck, the step 1 point check, the
//               per-address handshake cap and 0600 on load. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic Claude
//               Code.
//   2026-09-29: slice control plan Task 8b: touch() keeps a signed-in
//               device's current name too. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include "core/security/DeviceStore.h"

#include "core/LogCategories.h"
#include "core/security/StationIdentity.h"
#include "core/security/TokenStore.h"

#include <QDir>
#include <QFile>
#include <QFileDevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace NereusSDR {

namespace {

// 64 records of a few hundred bytes each fit many times over.
constexpr qint64 kMaxFileBytes = 1024 * 1024;
constexpr int kFileVersion = 1;

QString isoTime(const QDateTime& time)
{
    return time.isValid() ? time.toUTC().toString(Qt::ISODateWithMs) : QString();
}

QDateTime fromIso(const QString& text)
{
    QDateTime time = QDateTime::fromString(text, Qt::ISODateWithMs);
    if (!time.isValid()) {
        time = QDateTime::fromString(text, Qt::ISODate);
    }
    return time.isValid() ? time.toUTC() : QDateTime();
}

bool wellFormed(const PairedDevice& device)
{
    return StationIdentity::isP256Spki(device.publicKeySpki)
           && device.id == StationIdentity::fingerprintOf(device.publicKeySpki)
           && DeviceStore::isKnownKind(device.kind) && DeviceStore::isValidName(device.name)
           && (device.shortName.isEmpty() || DeviceStore::isValidShortName(device.shortName));
}

QJsonObject toJson(const PairedDevice& device)
{
    return QJsonObject{
        {QStringLiteral("id"), StationIdentity::toBase64Url(device.id)},
        {QStringLiteral("publicKey"), StationIdentity::toBase64Url(device.publicKeySpki)},
        {QStringLiteral("name"), device.name},
        {QStringLiteral("kind"), device.kind},
        {QStringLiteral("pairedAt"), isoTime(device.pairedAt)},
        {QStringLiteral("lastSeen"), isoTime(device.lastSeen)},
        {QStringLiteral("lastAddress"), device.lastAddress},
        {QStringLiteral("enrolledThroughToken"), device.enrolledThroughToken},
        {QStringLiteral("shortName"), device.shortName},
    };
}

bool fromJson(const QJsonValue& value, PairedDevice* device)
{
    if (!value.isObject()) {
        return false;
    }
    const QJsonObject o = value.toObject();
    for (const char* key : {"id", "publicKey", "name", "kind", "pairedAt", "lastSeen",
                            "lastAddress"}) {
        if (!o.value(QLatin1String(key)).isString()) {
            return false;
        }
    }
    if (!o.value(QStringLiteral("enrolledThroughToken")).isBool()) {
        return false;
    }
    bool idOk = false;
    bool keyOk = false;
    device->id = StationIdentity::fromBase64Url(o.value(QStringLiteral("id")).toString(), &idOk);
    device->publicKeySpki =
        StationIdentity::fromBase64Url(o.value(QStringLiteral("publicKey")).toString(), &keyOk);
    device->name = o.value(QStringLiteral("name")).toString();
    device->kind = o.value(QStringLiteral("kind")).toString();
    device->pairedAt = fromIso(o.value(QStringLiteral("pairedAt")).toString());
    device->lastSeen = fromIso(o.value(QStringLiteral("lastSeen")).toString());
    device->lastAddress = o.value(QStringLiteral("lastAddress")).toString();
    device->enrolledThroughToken = o.value(QStringLiteral("enrolledThroughToken")).toBool();
    // Part C fix wave, additive: a list written before it has no shortName.
    device->shortName.clear();
    if (o.contains(QStringLiteral("shortName"))) {
        const QJsonValue shortName = o.value(QStringLiteral("shortName"));
        if (!shortName.isString()) {
            return false;
        }
        device->shortName = shortName.toString();  // checked by wellFormed()
    }
    return idOk && keyOk && wellFormed(*device);
}

} // namespace

DeviceStore::DeviceStore(const QString& directory, const TokenStore* tokens, QObject* parent)
    : QObject(parent)
    , m_path(QDir(directory).filePath(QString::fromLatin1(kFileName)))
    , m_tokens(tokens)
{
    if (!QDir().mkpath(directory)) {
        m_valid = false;
        m_lastError = QStringLiteral("Could not create %1").arg(directory);
        return;
    }
    m_valid = load();
    if (!m_valid) {
        qCWarning(lcConnection) << "Paired devices unavailable, no device can sign in:"
                                << m_lastError;
    }
}

bool DeviceStore::load()
{
    QFile file(m_path);
    if (!file.exists()) {
        return true;
    }
    // Part C fix wave (R1-M5): a list restored at wider permissions is
    // made owner-only again, or a warning says it could not be.
    StationIdentity::keepOwnerOnly(m_path);
    if (!file.open(QIODevice::ReadOnly)) {
        m_lastError = QStringLiteral("%1 could not be read: %2").arg(m_path, file.errorString());
        return false;
    }
    if (file.size() > kMaxFileBytes) {
        m_lastError = QStringLiteral("%1 is larger than a paired-device list can be").arg(m_path);
        return false;
    }
    QJsonParseError error{};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        m_lastError = QStringLiteral("%1 is not a paired-device list").arg(m_path);
        return false;
    }
    const QJsonObject root = doc.object();
    if (root.value(QStringLiteral("version")).toInt(-1) != kFileVersion
        || !root.value(QStringLiteral("devices")).isArray()) {
        m_lastError = QStringLiteral("%1 is not a paired-device list this Core reads").arg(m_path);
        return false;
    }
    QList<PairedDevice> devices;
    for (const QJsonValue& value : root.value(QStringLiteral("devices")).toArray()) {
        PairedDevice device;
        if (!fromJson(value, &device)) {
            m_lastError = QStringLiteral("%1 holds a device record that is not well formed")
                              .arg(m_path);
            return false;
        }
        for (const PairedDevice& other : devices) {
            if (other.id == device.id) {
                m_lastError = QStringLiteral("%1 lists one device twice").arg(m_path);
                return false;
            }
        }
        devices.append(device);
    }
    if (devices.size() > kMaxDevices) {
        m_lastError = QStringLiteral("%1 lists more devices than a Core keeps").arg(m_path);
        return false;
    }
    m_devices = devices;
    return true;
}

bool DeviceStore::save(const QList<PairedDevice>& devices)
{
    QJsonArray array;
    for (const PairedDevice& device : devices) {
        array.append(toJson(device));
    }
    const QByteArray bytes = QJsonDocument(QJsonObject{
                                               {QStringLiteral("version"), kFileVersion},
                                               {QStringLiteral("devices"), array},
                                           })
                                 .toJson(QJsonDocument::Indented);
    // Mode 0600 on the temporary file before commit() (TokenStore's
    // ordering), so the list never exists under its name more widely
    // readable.
    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly)
        || !file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner)
        || file.write(bytes) != bytes.size() || !file.commit()) {
        file.cancelWriting();
        qCWarning(lcConnection) << "Could not write the paired-device list" << m_path << ":"
                                << file.errorString();
        return false;
    }
    return true;
}

QDateTime DeviceStore::now() const
{
    return m_clock ? m_clock().toUTC() : QDateTime::currentDateTimeUtc();
}

void DeviceStore::setClock(std::function<QDateTime()> clock)
{
    m_clock = std::move(clock);
}

bool DeviceStore::isKnownKind(const QString& kind)
{
    return kind == QLatin1String("phone") || kind == QLatin1String("tablet")
           || kind == QLatin1String("computer");
}

bool DeviceStore::isValidName(const QString& name)
{
    return isValidLabel(name, kMaxNameBytes);
}

bool DeviceStore::isValidShortName(const QString& shortName)
{
    return isValidLabel(shortName, kMaxShortNameBytes);
}

bool DeviceStore::isValidLabel(const QString& name, int maxBytes)
{
    if (name.trimmed().isEmpty() || name.toUtf8().size() > maxBytes) {
        return false;
    }
    for (const QChar c : name) {
        const QChar::Category category = c.category();
        if (category == QChar::Other_Control || category == QChar::Other_Format
            || category == QChar::Separator_Line || category == QChar::Separator_Paragraph) {
            return false;
        }
    }
    // A lone surrogate does not survive a UTF-8 round trip.
    return QString::fromUtf8(name.toUtf8()) == name;
}

bool DeviceStore::add(const PairedDevice& device)
{
    if (!m_valid || !wellFormed(device) || m_devices.size() >= kMaxDevices
        || find(device.id).has_value()) {
        return false;
    }
    PairedDevice stored = device;
    const QDateTime time = now();
    if (!stored.pairedAt.isValid()) {
        stored.pairedAt = time;
    }
    if (!stored.lastSeen.isValid()) {
        stored.lastSeen = stored.pairedAt;
    }
    stored.pairedAt = stored.pairedAt.toUTC();
    stored.lastSeen = stored.lastSeen.toUTC();
    QList<PairedDevice> next = m_devices;
    next.append(stored);
    if (!save(next)) {
        return false;
    }
    m_devices = next;
    emit devicesChanged();
    return true;
}

bool DeviceStore::remove(const QByteArray& id)
{
    if (!m_valid) {
        return false;
    }
    QList<PairedDevice> next = m_devices;
    const auto it = std::find_if(next.begin(), next.end(),
                                 [&id](const PairedDevice& d) { return d.id == id; });
    if (it == next.end()) {
        return false;
    }
    next.erase(it);
    if (!save(next)) {
        return false;
    }
    m_devices = next;
    emit deviceRemoved(id);
    emit devicesChanged();
    return true;
}

bool DeviceStore::reset(QString* movedTo)
{
    if (movedTo) {
        movedTo->clear();
    }
    if (!m_valid && QFile::exists(m_path)) {
        // Kept, not deleted: the operator may want to see what was in it.
        const QString aside = m_path + QStringLiteral(".damaged-")
                              + now().toString(QStringLiteral("yyyyMMdd'T'HHmmsszzz'Z'"));
        if (!QFile::rename(m_path, aside)) {
            m_lastError = QStringLiteral("%1 could not be moved aside").arg(m_path);
            qCWarning(lcConnection) << "Could not move the damaged paired-device list aside:"
                                    << m_path;
            return false;
        }
        if (movedTo) {
            *movedTo = aside;
        }
        qCInfo(lcConnection) << "The damaged paired-device list was moved aside to" << aside;
    }
    if (!save({})) {
        m_lastError = QStringLiteral("%1 could not be written").arg(m_path);
        return false;
    }
    const QList<PairedDevice> removed = m_valid ? m_devices : QList<PairedDevice>{};
    m_devices.clear();
    m_valid = true;
    m_lastError.clear();
    for (const PairedDevice& device : removed) {
        emit deviceRemoved(device.id);
    }
    emit devicesChanged();
    return true;
}

std::optional<PairedDevice> DeviceStore::find(const QByteArray& id) const
{
    if (!m_valid) {
        return std::nullopt;
    }
    for (const PairedDevice& device : m_devices) {
        if (device.id == id) {
            return device;
        }
    }
    return std::nullopt;
}

void DeviceStore::touch(const QByteArray& id, const QString& address,
                        const QString& shortName, const QString& name)
{
    if (!m_valid) {
        return;
    }
    QList<PairedDevice> next = m_devices;
    for (PairedDevice& device : next) {
        if (device.id == id) {
            device.lastSeen = now();
            device.lastAddress = address;
            if (isValidShortName(shortName)) {
                device.shortName = shortName;
            }
            if (!name.isEmpty() && isValidName(name)) {
                device.name = name;
            }
            if (save(next)) {
                m_devices = next;
                emit devicesChanged();
            }
            return;
        }
    }
}

bool DeviceStore::isClaimed() const
{
    if (!m_valid) {
        return true;
    }
    // A token file that exists but cannot be read counts too: the Core may
    // well be claimed through it, and failing open would let a stranger
    // claim it.
    return !m_devices.isEmpty()
           || (m_tokens != nullptr && (m_tokens->isActive() || !m_tokens->isValid()));
}

} // namespace NereusSDR
