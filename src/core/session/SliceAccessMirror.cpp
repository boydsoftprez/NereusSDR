// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/SliceAccessMirror.cpp  (NereusSDR)
// =================================================================
//
// See SliceAccessMirror.h. Slice control and shared listening plan Task 5.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-28: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), slice control and shared listening plan Task 5,
//               with AI-assisted implementation via Anthropic Claude Code.
//   2026-09-29: slice control plan Task 17: listenerReason names the
//               controller as StationServer::sliceHolderWords does. J.J.
//               Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-30: core-slice take-over: setCoreSliceTakeable(). J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-30: listenerReason names the hosting desktop for the station
//               device by its hostsCore entry. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include "core/session/SliceAccessMirror.h"

#include "core/session/DeviceSessionRegistry.h"
#include "core/session/RemoteDevicesState.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <QJsonArray>
#include <QJsonDocument>

namespace NereusSDR {

namespace {

constexpr const char* kAccessPrefix = "access:";
// The Core's own position (SliceOwnership::stationDevice()).
constexpr const char* kStationDevice = "station";

} // namespace

SliceAccessMirror::SliceAccessMirror(RadioModel* radio, RemoteDevicesState* devices,
                                     QObject* parent)
    : QObject(parent)
    , m_radio(radio)
    , m_devices(devices)
{
    if (radio != nullptr) {
        // A slice this window is sent after its access object (a listen
        // swaps the marker for the slice) is marked when it appears.
        connect(radio, &RadioModel::sliceAdded, this, [this](int sliceId) { markSlice(sliceId); });
    }
    if (devices != nullptr) {
        // The controller's name is in the listener words.
        connect(devices, &RemoteDevicesState::connectedDevicesChanged, this,
                &SliceAccessMirror::refreshSlices);
    }
}

bool SliceAccessMirror::holdsKey(const QByteArray& objectKey)
{
    return sliceIdOf(objectKey) >= 0;
}

int SliceAccessMirror::sliceIdOf(const QByteArray& objectKey)
{
    if (!objectKey.startsWith(kAccessPrefix)) {
        return -1;
    }
    bool ok = false;
    const int id = objectKey.mid(int(qstrlen(kAccessPrefix))).toInt(&ok);
    return ok && id >= 0 ? id : -1;
}

QStringList SliceAccessMirror::parseIds(const QString& json)
{
    QStringList ids;
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    for (const QJsonValue& value : doc.array()) {
        if (value.isString()) {
            ids.append(value.toString());
        }
    }
    return ids;
}

void SliceAccessMirror::applyObject(const QByteArray& objectKey,
                                    const QList<MirrorUpdate>& updates)
{
    const int sliceId = sliceIdOf(objectKey);
    if (sliceId < 0) {
        return;
    }
    auto it = m_entries.find(sliceId);
    if (it == m_entries.end()) {
        it = m_entries.insert(sliceId, Entry{sliceId, 0, QString(), 0, {}, {}, false, false});
    }
    Entry& e = it.value();
    for (const MirrorUpdate& u : updates) {
        if (u.name == "incarnation") {
            e.incarnation = static_cast<quint64>(u.value.toLongLong());
        } else if (u.name == "controllerDeviceId") {
            e.controllerDeviceId = u.value.toString();
        } else if (u.name == "controlRevision") {
            e.controlRevision = static_cast<quint64>(u.value.toLongLong());
        } else if (u.name == "listenerDeviceIds") {
            e.listeners = parseIds(u.value.toString());
        } else if (u.name == "activeRxDeviceIds") {
            e.activeRx = parseIds(u.value.toString());
        } else if (u.name == "txSelected") {
            e.txSelected = u.value.toBool();
        } else if (u.name == "onAir") {
            e.onAir = u.value.toBool();
        }
        // sliceId is the key's; anything a newer Core adds is not kept.
    }
    markSlice(sliceId);
    emit changed(sliceId);
}

void SliceAccessMirror::destroyObject(const QByteArray& objectKey)
{
    const int sliceId = sliceIdOf(objectKey);
    if (sliceId < 0 || m_entries.remove(sliceId) == 0) {
        return;
    }
    markSlice(sliceId);
    emit changed(sliceId);
}

void SliceAccessMirror::clear()
{
    const QList<int> ids = m_entries.keys();
    m_entries.clear();
    for (int sliceId : ids) {
        emit changed(sliceId);
    }
}

void SliceAccessMirror::setCoreSliceTakeable(bool takeable)
{
    if (m_coreSliceTakeable == takeable) {
        return;
    }
    m_coreSliceTakeable = takeable;
    // Every slice's Take control follows it.
    const QList<int> ids = m_entries.keys();
    for (int sliceId : ids) {
        emit changed(sliceId);
    }
}

void SliceAccessMirror::setSelfDeviceId(const QString& id)
{
    if (m_selfDeviceId == id) {
        return;
    }
    m_selfDeviceId = id;
    refreshSlices();
}

std::optional<SliceAccessMirror::Entry> SliceAccessMirror::entry(int sliceId) const
{
    const auto it = m_entries.constFind(sliceId);
    if (it == m_entries.cend()) {
        return std::nullopt;
    }
    return it.value();
}

bool SliceAccessMirror::controlledHere(int sliceId) const
{
    const auto it = m_entries.constFind(sliceId);
    return it != m_entries.cend() && !m_selfDeviceId.isEmpty()
        && it->controllerDeviceId == m_selfDeviceId;
}

bool SliceAccessMirror::listeningHere(int sliceId) const
{
    const auto it = m_entries.constFind(sliceId);
    return it != m_entries.cend() && !m_selfDeviceId.isEmpty()
        && it->listeners.contains(m_selfDeviceId);
}

QString SliceAccessMirror::listenerReason(int sliceId) const
{
    // The Core's own sentence (StationServer::listenerChangeReason), so a
    // change held here reads the same as the Core's refusal of it.
    const QString letter = QString(QChar(QLatin1Char('A').unicode() + sliceId));
    const auto it = m_entries.constFind(sliceId);
    const QString controller = it != m_entries.cend() ? it->controllerDeviceId : QString();
    if (controller.isEmpty()) {
        return QStringLiteral("Nobody controls slice %1. Take control to change it.").arg(letter);
    }
    // Slice control plan Task 17: who controls it, as the Core names it
    // (StationServer::sliceHolderWords): the device's name (the hosting
    // desktop's too), the plain word for its kind when it has no name,
    // "another device" when neither is known, and the Core only for the
    // station device on a Core no desktop hosts. The station device is the
    // hosting desktop's entry (hostsCore), never matched by id.
    std::optional<RemoteConnectedDevice> device;
    if (m_devices) {
        device = m_devices->sliceHolderDevice(controller);
    }
    QString owner;
    if (device && !device->name.isEmpty()) {
        owner = device->name;
    } else if (controller == QLatin1String(kStationDevice)) {
        owner = QStringLiteral("the Core");
    } else if (device && !device->kind.isEmpty()) {
        const QString kind = DeviceSessionRegistry::kindWord(device->kind).toLower();
        owner = QStringLiteral("a %1").arg(kind);
    } else {
        owner = QStringLiteral("another device");
    }
    return QStringLiteral("Slice %1 is controlled by %2. Take control to change it.")
        .arg(letter)
        .arg(owner);
}

void SliceAccessMirror::refreshSlices()
{
    if (!m_radio) {
        return;
    }
    for (SliceModel* slice : m_radio->slices()) {
        if (slice != nullptr) {
            markSlice(slice->sliceIndex());
        }
    }
}

void SliceAccessMirror::markSlice(int sliceId)
{
    if (!m_radio) {
        return;
    }
    SliceModel* slice = m_radio->sliceById(sliceId);
    if (slice == nullptr) {
        return;
    }
    // A slice this window holds and does not control is one it listens to.
    // Unknown (no entry, or this window's id not known yet): not held.
    const bool readOnly = !m_selfDeviceId.isEmpty() && m_entries.contains(sliceId)
        && !controlledHere(sliceId);
    slice->setReadOnlyListener(readOnly, readOnly ? listenerReason(sliceId) : QString());
}

} // namespace NereusSDR
