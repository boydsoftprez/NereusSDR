// =================================================================
// src/core/ReceiveLayoutStore.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original Core receive-layout persistence adapter.
//
// See ReceiveLayoutStore.h and
// docs/architecture/2026-09-22-core-receive-layout-design.md.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-22: Original implementation for R-R3-34 by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Codex.
//   2026-09-25: iPhone app plan Task 73 (R-IOS-02, ruling 5.3): owners and
//               held-for marks. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
// =================================================================

#include "core/ReceiveLayoutStore.h"

#include "core/AppSettings.h"
#include "core/SliceOwnership.h"
#include "core/WdspEngine.h"
#include "core/session/MirrorEnumDomain.h"
#include "models/SliceModel.h"

#include <algorithm>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMetaType>
#include <QSet>

#include <cmath>
#include <initializer_list>
#include <limits>

namespace NereusSDR {

namespace {

constexpr int kSchemaVersion = 1;
constexpr qsizetype kMaximumRawJsonBytes = 16 * 1024;
const QString kSettingsKey = QStringLiteral("receiveLayout");

void setError(QString* error, const QString& message)
{
    if (error) {
        *error = message;
    }
}

bool isCanonicalPanKey(const QString& panKey)
{
    if (!panKey.startsWith(QStringLiteral("pan-"))) {
        return false;
    }
    const QString suffix = panKey.mid(4);
    bool ok = false;
    const int number = suffix.toInt(&ok);
    return ok && number >= 0 && panKey == QStringLiteral("pan-%1").arg(number);
}

bool jsonInteger(const QJsonValue& value, int minimum, int maximum, int* output)
{
    if (!value.isDouble()) {
        return false;
    }
    const double number = value.toDouble();
    if (!std::isfinite(number) || std::floor(number) != number
        || number < static_cast<double>(minimum)
        || number > static_cast<double>(maximum)) {
        return false;
    }
    *output = static_cast<int>(number);
    return true;
}

bool exactKeys(const QJsonObject& object, std::initializer_list<const char*> keys)
{
    if (object.size() != static_cast<qsizetype>(keys.size())) {
        return false;
    }
    for (const char* key : keys) {
        if (!object.contains(QLatin1String(key))) {
            return false;
        }
    }
    return true;
}

// Task 73: an owner id as stored, and back. The station device is written
// by name; any other id as base64url. A token window's id is not stored.
bool storableOwner(const QByteArray& id)
{
    return !id.isEmpty() && !id.startsWith("token:");
}

QString storedOwner(const QByteArray& id)
{
    if (id == SliceOwnership::stationDevice()) {
        return QString::fromLatin1(id);
    }
    return QString::fromLatin1(
        id.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

bool ownerFromStored(const QJsonValue& value, QByteArray* id)
{
    if (!value.isString() || value.toString().isEmpty()) {
        return false;
    }
    const QByteArray text = value.toString().toLatin1();
    if (text == SliceOwnership::stationDevice()) {
        *id = text;
        return true;
    }
    const auto decoded = QByteArray::fromBase64Encoding(
        text, QByteArray::Base64UrlEncoding | QByteArray::AbortOnBase64DecodingErrors);
    if (!decoded || decoded.decoded.isEmpty()) {
        return false;
    }
    *id = decoded.decoded;
    return true;
}

bool isRadeMode(DSPMode mode)
{
    return mode == DSPMode::RADE_U || mode == DSPMode::RADE_L;
}

bool validateAndResolveOwner(const QList<ReceiveSliceState>& slices,
                             std::optional<int> requestedOwnerId,
                             std::optional<int>* resolvedOwnerId,
                             QString* error)
{
    if (error) {
        error->clear();
    }
    if (slices.isEmpty() || slices.size() > WdspEngine::kMaxSliceChannels) {
        setError(error, QStringLiteral("Receive layout must contain between 1 and %1 slices.")
                              .arg(WdspEngine::kMaxSliceChannels));
        return false;
    }

    QSet<int> ids;
    QList<int> radeIds;
    for (const ReceiveSliceState& slice : slices) {
        if (slice.id < 0 || slice.id >= WdspEngine::kMaxSliceChannels) {
            setError(error, QStringLiteral("Receive layout slice ID %1 is out of range.")
                                  .arg(slice.id));
            return false;
        }
        if (ids.contains(slice.id)) {
            setError(error, QStringLiteral("Receive layout contains duplicate slice ID %1.")
                                  .arg(slice.id));
            return false;
        }
        ids.insert(slice.id);

        if (!isCanonicalPanKey(slice.panKey)) {
            setError(error, QStringLiteral("Receive layout pan key '%1' is not canonical.")
                                  .arg(slice.panKey));
            return false;
        }
        if (!std::isfinite(slice.frequencyHz)
            || slice.frequencyHz < SliceModel::kMinReceiveFrequencyHz
            || slice.frequencyHz > SliceModel::kMaxReceiveFrequencyHz) {
            setError(error, QStringLiteral("Receive layout frequency is out of range."));
            return false;
        }
        if (!MirrorEnumDomain::contains(QMetaType::fromType<DSPMode>(),
                                        static_cast<int>(slice.dspMode))) {
            setError(error, QStringLiteral("Receive layout DSP mode is invalid."));
            return false;
        }
        if (isRadeMode(slice.dspMode)) {
            radeIds.append(slice.id);
        }
    }

    if (requestedOwnerId.has_value()) {
        if (!radeIds.contains(*requestedOwnerId)) {
            setError(error, QStringLiteral("Receive layout RADE receive owner is not a RADE slice."));
            return false;
        }
        if (resolvedOwnerId) {
            *resolvedOwnerId = requestedOwnerId;
        }
        return true;
    }
    if (radeIds.isEmpty()) {
        if (resolvedOwnerId) {
            resolvedOwnerId->reset();
        }
        return true;
    }
    if (radeIds.size() == 1) {
        if (resolvedOwnerId) {
            *resolvedOwnerId = radeIds.first();
        }
        return true;
    }

    setError(error, QStringLiteral("Receive layout has multiple RADE slices but no receive owner."));
    return false;
}

ReceiveLayoutStore::LoadResult invalidData(const QString& error)
{
    return {ReceiveLayoutStore::LoadState::InvalidData, {}, error};
}

} // namespace

bool ReceiveLayoutStore::validate(const QList<ReceiveSliceState>& slices,
                                  QString* error,
                                  std::optional<int> radeRxOwnerId)
{
    return validateAndResolveOwner(slices, radeRxOwnerId, nullptr, error);
}

bool ReceiveLayoutStore::stage(AppSettings& settings, const QString& mac,
                               const QList<ReceiveSliceState>& slices,
                               QString* error,
                               std::optional<int> radeRxOwnerId,
                               std::optional<int> diversityOwnerId)
{
    if (error) {
        error->clear();
    }
    const QString normalizedMac = AppSettings::normalizedRadioMac(mac);
    if (normalizedMac.isEmpty()) {
        setError(error, QStringLiteral("Receive layout requires a valid radio MAC address."));
        return false;
    }

    QList<ReceiveSliceState> normalizedSlices = slices;
    for (ReceiveSliceState& slice : normalizedSlices) {
        if (slice.panKey.isEmpty()) {
            slice.panKey = QStringLiteral("pan-0");
        }
    }
    std::optional<int> resolvedOwnerId;
    if (!validateAndResolveOwner(normalizedSlices, radeRxOwnerId,
                                 &resolvedOwnerId, error)) {
        return false;
    }

    QJsonArray jsonSlices;
    for (const ReceiveSliceState& slice : normalizedSlices) {
        QJsonObject object;
        object.insert(QStringLiteral("id"), slice.id);
        object.insert(QStringLiteral("panKey"), slice.panKey);
        object.insert(QStringLiteral("frequencyHz"), slice.frequencyHz);
        object.insert(QStringLiteral("dspMode"), static_cast<int>(slice.dspMode));
        // Task 73: written only when set, so a layout nobody owns is
        // written exactly as before owners existed.
        const bool held = storableOwner(slice.heldFor)
            && slice.owner == SliceOwnership::stationDevice();
        if (held) {
            object.insert(QStringLiteral("owner"), storedOwner(slice.owner));
            object.insert(QStringLiteral("heldFor"), storedOwner(slice.heldFor));
        } else if (storableOwner(slice.owner)) {
            object.insert(QStringLiteral("owner"), storedOwner(slice.owner));
        }
        jsonSlices.append(object);
    }
    QJsonObject root;
    root.insert(QStringLiteral("version"), kSchemaVersion);
    root.insert(QStringLiteral("slices"), jsonSlices);
    root.insert(QStringLiteral("radeRxOwnerId"),
                resolvedOwnerId.has_value()
                    ? QJsonValue(*resolvedOwnerId)
                    : QJsonValue(QJsonValue::Null));
    if (diversityOwnerId) {
        const bool present = std::any_of(normalizedSlices.begin(), normalizedSlices.end(),
            [diversityOwnerId](const ReceiveSliceState& slice) { return slice.id == *diversityOwnerId; });
        if (!present) { diversityOwnerId.reset(); }
        root.insert(QStringLiteral("diversityOwnerId"), diversityOwnerId
            ? QJsonValue(*diversityOwnerId) : QJsonValue(QJsonValue::Null));
    }
    const QString raw = QString::fromUtf8(
        QJsonDocument(root).toJson(QJsonDocument::Compact));
    if (raw.toUtf8().size() > kMaximumRawJsonBytes) {
        // The current bounded descriptor cannot normally reach this limit,
        // but keep staging non-destructive if a future schema expands it.
        setError(error, QStringLiteral("Receive layout exceeds the storage budget."));
        return false;
    }

    settings.setHardwareValue(normalizedMac, kSettingsKey, raw);
    return true;
}

void ReceiveLayoutStore::forget(AppSettings& settings, const QString& mac)
{
    const QString normalizedMac = AppSettings::normalizedRadioMac(mac);
    if (normalizedMac.isEmpty()) {
        return;
    }
    settings.remove(QStringLiteral("hardware/%1/%2").arg(normalizedMac, kSettingsKey));
}

ReceiveLayoutStore::LoadResult ReceiveLayoutStore::load(const AppSettings& settings,
                                                         const QString& mac)
{
    const QString normalizedMac = AppSettings::normalizedRadioMac(mac);
    if (normalizedMac.isEmpty()) {
        return {LoadState::InvalidIdentity, {},
                QStringLiteral("Receive layout requires a valid radio MAC address.")};
    }

    const QVariant stored = settings.hardwareValue(normalizedMac, kSettingsKey);
    if (!stored.isValid()) {
        return {LoadState::Missing, {}, {}};
    }
    if (stored.metaType() != QMetaType::fromType<QString>()) {
        return invalidData(QStringLiteral("Stored receive layout is not JSON text."));
    }

    const QString raw = stored.toString();
    const QByteArray bytes = raw.toUtf8();
    if (bytes.size() > kMaximumRawJsonBytes) {
        return invalidData(QStringLiteral("Stored receive layout exceeds the storage budget."));
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return invalidData(QStringLiteral("Stored receive layout is not a JSON object."));
    }
    const QJsonObject root = document.object();
    const bool hasOwnerField = root.contains(QStringLiteral("radeRxOwnerId"));
    if (!(exactKeys(root, {"version", "slices"})
          || exactKeys(root, {"version", "slices", "radeRxOwnerId"})
          || exactKeys(root, {"version", "slices", "diversityOwnerId"})
          || exactKeys(root, {"version", "slices", "radeRxOwnerId", "diversityOwnerId"}))) {
        return invalidData(QStringLiteral("Stored receive layout has an unsupported schema."));
    }

    int version = 0;
    if (!jsonInteger(root.value(QStringLiteral("version")),
                     kSchemaVersion, kSchemaVersion, &version)
        || version != kSchemaVersion || !root.value(QStringLiteral("slices")).isArray()) {
        return invalidData(QStringLiteral("Stored receive layout has an unsupported schema."));
    }

    const QJsonArray jsonSlices = root.value(QStringLiteral("slices")).toArray();
    if (jsonSlices.isEmpty() || jsonSlices.size() > WdspEngine::kMaxSliceChannels) {
        return invalidData(QStringLiteral("Stored receive layout has an invalid slice count."));
    }

    std::optional<int> requestedOwnerId;
    if (hasOwnerField && !root.value(QStringLiteral("radeRxOwnerId")).isNull()) {
        int ownerId = 0;
        if (!jsonInteger(root.value(QStringLiteral("radeRxOwnerId")), 0,
                         WdspEngine::kMaxSliceChannels - 1, &ownerId)) {
            return invalidData(QStringLiteral("Stored receive layout has an invalid RADE receive owner."));
        }
        requestedOwnerId = ownerId;
    }

    QList<ReceiveSliceState> slices;
    slices.reserve(jsonSlices.size());
    for (const QJsonValue& value : jsonSlices) {
        if (!value.isObject()) {
            return invalidData(QStringLiteral("Stored receive layout has an invalid slice."));
        }
        const QJsonObject object = value.toObject();
        if (!exactKeys(object, {"id", "panKey", "frequencyHz", "dspMode"})
            && !exactKeys(object, {"id", "panKey", "frequencyHz", "dspMode", "owner"})
            && !exactKeys(object, {"id", "panKey", "frequencyHz", "dspMode", "owner", "heldFor"})) {
            return invalidData(QStringLiteral("Stored receive layout has an invalid slice schema."));
        }

        ReceiveSliceState slice;
        int mode = 0;
        if (!jsonInteger(object.value(QStringLiteral("id")), 0,
                         WdspEngine::kMaxSliceChannels - 1, &slice.id)
            || !object.value(QStringLiteral("panKey")).isString()
            || !object.value(QStringLiteral("frequencyHz")).isDouble()
            || !jsonInteger(object.value(QStringLiteral("dspMode")),
                            std::numeric_limits<int>::min(),
                            std::numeric_limits<int>::max(),
                            &mode)) {
            return invalidData(QStringLiteral("Stored receive layout has invalid slice values."));
        }
        slice.panKey = object.value(QStringLiteral("panKey")).toString();
        slice.frequencyHz = object.value(QStringLiteral("frequencyHz")).toDouble();
        if (!std::isfinite(slice.frequencyHz)) {
            return invalidData(QStringLiteral("Stored receive layout has invalid slice values."));
        }
        slice.dspMode = static_cast<DSPMode>(mode);
        // Task 73: the owner, and the device a held slice is held for, which
        // only the station device can hold a slice for.
        if (object.contains(QStringLiteral("owner"))
            && !ownerFromStored(object.value(QStringLiteral("owner")), &slice.owner)) {
            return invalidData(QStringLiteral("Stored receive layout has an invalid slice owner."));
        }
        if (object.contains(QStringLiteral("heldFor"))
            && (!ownerFromStored(object.value(QStringLiteral("heldFor")), &slice.heldFor)
                || slice.owner != SliceOwnership::stationDevice()
                || slice.heldFor == SliceOwnership::stationDevice())) {
            return invalidData(QStringLiteral("Stored receive layout has an invalid slice owner."));
        }
        slices.append(slice);
    }

    QString error;
    std::optional<int> resolvedOwnerId;
    if (!validateAndResolveOwner(slices, requestedOwnerId, &resolvedOwnerId, &error)) {
        return invalidData(error);
    }
    std::optional<int> diversityOwner;
    if (root.contains(QStringLiteral("diversityOwnerId"))
        && !root.value(QStringLiteral("diversityOwnerId")).isNull()) {
        int id = -1;
        if (!jsonInteger(root.value(QStringLiteral("diversityOwnerId")), 0,
                         WdspEngine::kMaxSliceChannels - 1, &id)) {
            return invalidData(QStringLiteral("Stored receive layout has an invalid Diversity owner."));
        }
        // Same restored slice only. An absent owner is off, never promoted.
        if (std::any_of(slices.begin(), slices.end(), [id](const ReceiveSliceState& slice) { return slice.id == id; })) {
            diversityOwner = id;
        }
    }
    return {LoadState::Loaded, slices, {}, resolvedOwnerId, diversityOwner};
}

} // namespace NereusSDR
