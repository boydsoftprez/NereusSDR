// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/ConnectedDevicesFacade.cpp  (NereusSDR)
// =================================================================
// See ConnectedDevicesFacade.h.
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 71 (R-IOS-02), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-25: iPhone app plan Task 73 (R-IOS-02): listeningOn and
//               describe(). J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-25: iPhone app plan Task 34 (R-IOS-02): holdsTransmit for the
//               holder and state "transmitting" while it is on the air
//               (setTransmitProvider). J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-26: iPhone app plan Task 77 (R-IOS-02, R-IOS-03, R-IOS-13):
//               transmittingOn. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
// =================================================================

#include "core/session/ConnectedDevicesFacade.h"

#include "core/security/DeviceStore.h"
#include "core/security/StationIdentity.h"
#include "core/session/DeviceSessionRegistry.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

#include <algorithm>

namespace NereusSDR {

namespace {

using Registry = DeviceSessionRegistry;

QString wireDeviceId(const Registry::Entry& entry)
{
    // A token window's id is already text; a paired device's (and a
    // hosting desktop's) is its key's fingerprint, as `devices` sends it.
    return entry.kind == Registry::Kind::Token ? QString::fromLatin1(entry.deviceId)
                                               : StationIdentity::toBase64Url(entry.deviceId);
}

qint64 wholeSeconds(qint64 ms)
{
    return ms > 0 ? ms / 1000 : 0;
}

} // namespace

ConnectedDevicesFacade::ConnectedDevicesFacade(const DeviceSessionRegistry& registry,
                                               const DeviceStore& devices, QObject* parent)
    : QObject(parent)
    , m_registry(registry)
    , m_devices(devices)
{
    m_stable = stableForm();
    connect(&m_registry, &DeviceSessionRegistry::changed, this, &ConnectedDevicesFacade::refresh);
    connect(&m_devices, &DeviceStore::devicesChanged, this, &ConnectedDevicesFacade::refresh);
}

int ConnectedDevicesFacade::deviceLimit() const
{
    return DeviceSessionRegistry::kMaxDeviceSessions;
}

QString ConnectedDevicesFacade::listJson() const
{
    return render(true);
}

QString ConnectedDevicesFacade::stableForm() const
{
    return render(false);
}

void ConnectedDevicesFacade::resumeRefresh()
{
    if (m_hold > 0) {
        --m_hold;
    }
    if (m_hold == 0 && m_refreshWanted) {
        m_refreshWanted = false;
        refresh();
    }
}

void ConnectedDevicesFacade::refresh()
{
    if (m_hold > 0) {
        m_refreshWanted = true;
        return;
    }
    const QString next = stableForm();
    if (next == m_stable) {
        return;
    }
    m_stable = next;
    ++m_revision;  // wraps past 2^32; readers compare by serial number
    emit connectedDevicesChanged();
}

void ConnectedDevicesFacade::setTransmitProvider(TransmitProvider provider)
{
    m_transmit = std::move(provider);
    refresh();
}

void ConnectedDevicesFacade::setListeningProvider(ListeningProvider provider)
{
    m_listening = std::move(provider);
    refresh();
}

QHash<QByteArray, Registry::NumberedName> ConnectedDevicesFacade::numbered() const
{
    const QList<Registry::Entry> entries = m_registry.entries();
    const QList<PairedDevice> paired = m_devices.list();

    // Ruling 4.3's numbering order: paired devices in pairing order, then a
    // hosting desktop the store does not hold (or any other entry it no
    // longer holds), then token windows in the order they connected.
    QList<Registry::NameInput> order;
    QSet<QByteArray> inStore;
    for (const PairedDevice& device : paired) {
        order.append({device.id, device.name,
                      Registry::usableShortName(device.shortName, device.kind)});
        inStore.insert(device.id);
    }
    QList<Registry::Entry> tokens;
    for (const Registry::Entry& entry : entries) {
        if (entry.kind == Registry::Kind::Token) {
            tokens.append(entry);
        } else if (!inStore.contains(entry.deviceId)) {
            order.append({entry.deviceId, entry.name,
                          Registry::usableShortName(entry.shortName, entry.deviceKind)});
        }
    }
    std::sort(tokens.begin(), tokens.end(),
              [](const Registry::Entry& a, const Registry::Entry& b) { return a.order < b.order; });
    for (const Registry::Entry& entry : tokens) {
        order.append({entry.deviceId, entry.name,
                      Registry::usableShortName(entry.shortName, entry.deviceKind)});
    }
    return Registry::numberNames(order);
}

std::optional<ConnectedDevicesFacade::DeviceWords>
ConnectedDevicesFacade::describe(const QByteArray& deviceId) const
{
    if (deviceId.isEmpty()) {
        return std::nullopt;
    }
    const QHash<QByteArray, Registry::NumberedName> names = numbered();
    const auto name = names.constFind(deviceId);
    if (name == names.cend()) {
        return std::nullopt;
    }
    DeviceWords words;
    words.name = name->name;
    words.shortName = name->shortName;
    const std::optional<Registry::Entry> entry = m_registry.entry(deviceId);
    const bool token = entry && entry->kind == Registry::Kind::Token;
    words.wireId = token ? QString::fromLatin1(deviceId) : StationIdentity::toBase64Url(deviceId);
    words.kind = entry ? entry->deviceKind : QString();
    if (!entry || entry->kind == Registry::Kind::Paired) {
        for (const PairedDevice& device : m_devices.list()) {
            if (device.id == deviceId) {
                words.kind = device.kind;
                break;
            }
        }
    }
    return words;
}

QString ConnectedDevicesFacade::render(bool withDurations) const
{
    const QList<Registry::Entry> entries = m_registry.entries();
    const QList<PairedDevice> paired = m_devices.list();
    QSet<QByteArray> inStore;
    for (const PairedDevice& device : paired) {
        inStore.insert(device.id);
    }
    const QHash<QByteArray, Registry::NumberedName> names = numbered();

    const qint64 now = m_registry.now();
    const TransmitState transmit = m_transmit ? m_transmit() : TransmitState{};
    QJsonArray list;
    for (const Registry::Entry& entry : entries) {
        const Registry::NumberedName name = names.value(entry.deviceId);
        const bool away = entry.state == Registry::State::Away;
        const bool isPaired = inStore.contains(entry.deviceId);
        const bool hostsCore = entry.kind == Registry::Kind::Hosting;
        const bool holds = !transmit.holderDeviceId.isEmpty()
            && transmit.holderDeviceId == entry.deviceId;
        const bool keyed = transmit.keyed;
        QString kind = entry.deviceKind;
        if (isPaired && !hostsCore) {
            for (const PairedDevice& device : paired) {
                if (device.id == entry.deviceId) {
                    kind = device.kind;
                    break;
                }
            }
        }
        QJsonObject o{
            {QStringLiteral("deviceId"), wireDeviceId(entry)},
            {QStringLiteral("name"), name.name},
            {QStringLiteral("shortName"), name.shortName},
            {QStringLiteral("kind"), kind},
            {QStringLiteral("paired"), isPaired},
            {QStringLiteral("hostsCore"), hostsCore},
            // Task 13's revoke refuses a token window's id, and a hosting
            // desktop runs the Core.
            {QStringLiteral("revocable"), isPaired && !hostsCore},
            // Task 34: "transmitting" while the device holding transmit is on
            // the air (an away holder is never keyed).
            {QStringLiteral("state"), away              ? QStringLiteral("away")
                                      : holds && keyed ? QStringLiteral("transmitting")
                                                       : QStringLiteral("listening")},
            // Task 34: the holder, keyed or not. Task 73 the slices; Task 77
            // transmittingOn.
            {QStringLiteral("holdsTransmit"), holds},
            {QStringLiteral("listeningOn"),
             m_listening ? m_listening(entry.deviceId) : QJsonArray{}},
        };
        // Task 77: the slice it transmits on, while it is on the air.
        if (holds && keyed && !away && !transmit.transmittingOn.isEmpty()) {
            o.insert(QStringLiteral("transmittingOn"), transmit.transmittingOn);
        }
        if (withDurations) {
            o.insert(QStringLiteral("lastActivitySeconds"),
                     wholeSeconds(now - entry.reportedActivityMs));
            o.insert(QStringLiteral("connectedForSeconds"),
                     wholeSeconds(now - entry.connectedSinceMs));
            o.insert(QStringLiteral("awayForSeconds"),
                     away ? wholeSeconds(now - entry.awaySinceMs) : 0);
            o.insert(QStringLiteral("transmittingForSeconds"),
                     holds && keyed ? wholeSeconds(now - transmit.keyedSinceMs) : 0);
        } else {
            // What the durations are measured from: a change here is a
            // change to the list.
            o.insert(QStringLiteral("reportedActivityMs"), entry.reportedActivityMs);
            o.insert(QStringLiteral("connectedSinceMs"), entry.connectedSinceMs);
            o.insert(QStringLiteral("awaySinceMs"), away ? entry.awaySinceMs : 0);
            o.insert(QStringLiteral("keyedSinceMs"), holds && keyed ? transmit.keyedSinceMs : 0);
        }
        list.append(o);
    }
    return QString::fromUtf8(QJsonDocument(list).toJson(QJsonDocument::Compact));
}

} // namespace NereusSDR
