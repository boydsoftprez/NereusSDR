// no-port-check: NereusSDR-original.
// =================================================================
// src/core/DeviceLayoutStore.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 73 (R-IOS-02): each device's closed slices, kept for
// its return. See DeviceLayoutStore.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 73 (R-IOS-02), with
//               AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include "core/DeviceLayoutStore.h"

#include "core/AppSettings.h"
#include "core/WdspEngine.h"
#include "core/session/MirrorEnumDomain.h"
#include "models/SliceModel.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaType>

#include <cmath>

namespace NereusSDR {

namespace {

constexpr int kSchemaVersion = 1;
constexpr qsizetype kMaximumRawJsonBytes = 256 * 1024;
const QString kGlobalPrefix = QStringLiteral("Slice/");
const QString kRadioPrefix = QStringLiteral("radio/");

QString sliceKeyPrefix(int sliceId)
{
    return QStringLiteral("Slice%1/").arg(sliceId);
}

QString radioSliceKeyPrefix(const QString& mac, int sliceId)
{
    return QStringLiteral("hardware/%1/slices/%2/").arg(mac).arg(sliceId);
}

bool validSlice(const SavedSlice& slice)
{
    return slice.id >= 0 && slice.id < WdspEngine::kMaxSliceChannels
        && std::isfinite(slice.frequencyHz)
        && slice.frequencyHz >= SliceModel::kMinReceiveFrequencyHz
        && slice.frequencyHz <= SliceModel::kMaxReceiveFrequencyHz
        && MirrorEnumDomain::contains(QMetaType::fromType<DSPMode>(),
                                      static_cast<int>(slice.dspMode));
}

QJsonObject toJson(const SavedSlice& slice)
{
    QJsonObject settings;
    for (auto it = slice.settings.cbegin(); it != slice.settings.cend(); ++it) {
        settings.insert(it.key(), it.value());
    }
    return QJsonObject{
        {QStringLiteral("id"), slice.id},
        {QStringLiteral("panKey"), slice.panKey.isEmpty() ? QStringLiteral("pan-0") : slice.panKey},
        {QStringLiteral("frequencyHz"), slice.frequencyHz},
        {QStringLiteral("dspMode"), static_cast<int>(slice.dspMode)},
        {QStringLiteral("settings"), settings},
    };
}

bool fromJson(const QJsonValue& value, SavedSlice* out)
{
    if (!value.isObject()) {
        return false;
    }
    const QJsonObject o = value.toObject();
    const QJsonValue id = o.value(QStringLiteral("id"));
    const QJsonValue hz = o.value(QStringLiteral("frequencyHz"));
    const QJsonValue mode = o.value(QStringLiteral("dspMode"));
    if (!id.isDouble() || !hz.isDouble() || !mode.isDouble()
        || !o.value(QStringLiteral("panKey")).isString()
        || !o.value(QStringLiteral("settings")).isObject()) {
        return false;
    }
    if (std::floor(id.toDouble()) != id.toDouble() || std::floor(mode.toDouble()) != mode.toDouble()) {
        return false;
    }
    SavedSlice slice;
    slice.id = id.toInt(-1);
    slice.panKey = o.value(QStringLiteral("panKey")).toString();
    slice.frequencyHz = hz.toDouble();
    slice.dspMode = static_cast<DSPMode>(mode.toInt());
    const QJsonObject settings = o.value(QStringLiteral("settings")).toObject();
    for (auto it = settings.constBegin(); it != settings.constEnd(); ++it) {
        if (!it.value().isString()
            || !(it.key().startsWith(kGlobalPrefix) || it.key().startsWith(kRadioPrefix))) {
            return false;
        }
        slice.settings.insert(it.key(), it.value().toString());
    }
    if (!validSlice(slice)) {
        return false;
    }
    *out = slice;
    return true;
}

} // namespace

QString DeviceLayoutStore::recordKey(const QByteArray& deviceId)
{
    return QStringLiteral("deviceLayouts/")
        + QString::fromLatin1(deviceId.toBase64(QByteArray::Base64UrlEncoding
                                                | QByteArray::OmitTrailingEquals));
}

QList<SavedSlice> DeviceLayoutStore::load(const AppSettings& settings, const QString& mac,
                                          const QByteArray& deviceId)
{
    const QString normalized = AppSettings::normalizedRadioMac(mac);
    if (normalized.isEmpty() || deviceId.isEmpty()) {
        return {};
    }
    const QVariant stored = settings.hardwareValue(normalized, recordKey(deviceId));
    if (!stored.isValid()) {
        return {};
    }
    const QByteArray bytes = stored.toString().toUtf8();
    if (bytes.size() > kMaximumRawJsonBytes) {
        return {};
    }
    const QJsonDocument document = QJsonDocument::fromJson(bytes);
    const QJsonObject root = document.object();
    if (!document.isObject() || root.value(QStringLiteral("version")).toInt() != kSchemaVersion
        || !root.value(QStringLiteral("slices")).isArray()) {
        return {};
    }
    QList<SavedSlice> slices;
    for (const QJsonValue& value : root.value(QStringLiteral("slices")).toArray()) {
        SavedSlice slice;
        if (fromJson(value, &slice)) {
            slices.removeIf([&slice](const SavedSlice& s) { return s.id == slice.id; });
            slices.append(slice);
        }
    }
    return slices;
}

bool DeviceLayoutStore::replace(AppSettings& settings, const QString& mac,
                                const QByteArray& deviceId, const QList<SavedSlice>& slices,
                                int maxSlices)
{
    const QString normalized = AppSettings::normalizedRadioMac(mac);
    if (normalized.isEmpty() || deviceId.isEmpty()) {
        return false;
    }
    QList<SavedSlice> kept;
    for (const SavedSlice& slice : slices) {
        if (!validSlice(slice)) {
            return false;
        }
        kept.removeIf([&slice](const SavedSlice& s) { return s.id == slice.id; });
        kept.append(slice);
    }
    while (!kept.isEmpty() && kept.size() > std::max(0, maxSlices)) {
        kept.removeFirst();
    }
    const QString key = QStringLiteral("hardware/%1/%2").arg(normalized, recordKey(deviceId));
    if (kept.isEmpty()) {
        settings.remove(key);
        return true;
    }
    QJsonArray array;
    for (const SavedSlice& slice : std::as_const(kept)) {
        array.append(toJson(slice));
    }
    const QJsonObject root{{QStringLiteral("version"), kSchemaVersion},
                           {QStringLiteral("slices"), array}};
    const QByteArray raw = QJsonDocument(root).toJson(QJsonDocument::Compact);
    if (raw.size() > kMaximumRawJsonBytes) {
        return false;
    }
    settings.setHardwareValue(normalized, recordKey(deviceId), QString::fromUtf8(raw));
    return true;
}

bool DeviceLayoutStore::append(AppSettings& settings, const QString& mac,
                               const QByteArray& deviceId, const SavedSlice& slice,
                               int maxSlices)
{
    if (!validSlice(slice)) {
        return false;
    }
    QList<SavedSlice> slices = load(settings, mac, deviceId);
    slices.append(slice);
    return replace(settings, mac, deviceId, slices, maxSlices);
}

void DeviceLayoutStore::forgetDevice(AppSettings& settings, const QByteArray& deviceId)
{
    if (deviceId.isEmpty()) {
        return;
    }
    const QString suffix = QLatin1Char('/') + recordKey(deviceId);
    const QStringList keys = settings.allKeys();
    for (const QString& key : keys) {
        if (key.startsWith(QLatin1String("hardware/")) && key.endsWith(suffix)) {
            settings.remove(key);
        }
    }
}

QMap<QString, QString> DeviceLayoutStore::captureSliceSettings(const AppSettings& settings,
                                                               const QString& mac, int sliceId)
{
    QMap<QString, QString> copy;
    const QString global = sliceKeyPrefix(sliceId);
    const QString normalized = AppSettings::normalizedRadioMac(mac);
    const QString radio = normalized.isEmpty() ? QString() : radioSliceKeyPrefix(normalized, sliceId);
    const QStringList keys = settings.allKeys();
    for (const QString& key : keys) {
        if (key.startsWith(global)) {
            copy.insert(kGlobalPrefix + key.mid(global.size()), settings.value(key).toString());
        } else if (!radio.isEmpty() && key.startsWith(radio)) {
            copy.insert(kRadioPrefix + key.mid(radio.size()), settings.value(key).toString());
        }
    }
    return copy;
}

void DeviceLayoutStore::clearSliceSettings(AppSettings& settings, const QString& mac, int sliceId)
{
    const QString global = sliceKeyPrefix(sliceId);
    const QString normalized = AppSettings::normalizedRadioMac(mac);
    const QString radio = normalized.isEmpty() ? QString() : radioSliceKeyPrefix(normalized, sliceId);
    const QStringList keys = settings.allKeys();
    for (const QString& key : keys) {
        if (key.startsWith(global) || (!radio.isEmpty() && key.startsWith(radio))) {
            settings.remove(key);
        }
    }
}

void DeviceLayoutStore::writeSliceSettings(AppSettings& settings, const QString& mac,
                                           int sliceId, const QMap<QString, QString>& copy)
{
    clearSliceSettings(settings, mac, sliceId);
    const QString normalized = AppSettings::normalizedRadioMac(mac);
    for (auto it = copy.cbegin(); it != copy.cend(); ++it) {
        if (it.key().startsWith(kGlobalPrefix)) {
            settings.setValue(sliceKeyPrefix(sliceId) + it.key().mid(kGlobalPrefix.size()),
                              it.value());
        } else if (it.key().startsWith(kRadioPrefix) && !normalized.isEmpty()) {
            settings.setValue(radioSliceKeyPrefix(normalized, sliceId)
                                  + it.key().mid(kRadioPrefix.size()),
                              it.value());
        }
    }
}

} // namespace NereusSDR
