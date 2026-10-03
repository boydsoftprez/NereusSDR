// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/DeviceSessionRegistry.cpp  (NereusSDR)
// =================================================================
// See DeviceSessionRegistry.h.
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 71 (R-IOS-02), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-25: iPhone app plan Task 73 (R-IOS-02): graceEnded. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-29: slice control plan Task 8: away generations and
//               isCurrentAbsence. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-29: slice control plan Task 17: numberNames leaves an empty
//               name empty. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
// =================================================================

#include "core/session/DeviceSessionRegistry.h"

#include "core/security/DeviceStore.h"

#include <QHostAddress>
#include <QPointer>
#include <QSet>

#include <algorithm>

namespace NereusSDR {

DeviceSessionRegistry::DeviceSessionRegistry(QObject* parent)
    : QObject(parent)
{
    m_monotonic.start();
}

void DeviceSessionRegistry::setClock(Clock clock)
{
    m_clock = std::move(clock);
}

qint64 DeviceSessionRegistry::now() const
{
    return m_clock ? m_clock() : m_monotonic.elapsed();
}

QByteArray DeviceSessionRegistry::nextTokenDeviceId()
{
    return QByteArrayLiteral("token:") + QByteArray::number(m_nextToken++);
}

int DeviceSessionRegistry::indexOf(const QByteArray& deviceId) const
{
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries.at(i).deviceId == deviceId) {
            return i;
        }
    }
    return -1;
}

void DeviceSessionRegistry::emitChanges(int placesBefore)
{
    ++m_revision;
    const QPointer<DeviceSessionRegistry> self(this);
    emit changed();
    if (!self) return;
    const int placesAfter = placesTaken();
    if (placesAfter != placesBefore) {
        emit placesTakenChanged(placesAfter);
    }
}

DeviceSessionRegistry::AdmitResult DeviceSessionRegistry::admit(const Entry& device,
                                                               const QObject* session)
{
    // A device whose 180 s have passed no longer holds a place, whether or
    // not the expiry has run yet.
    expireAway();

    AdmitResult result;
    const qint64 time = now();
    const int placesBefore = placesTaken();
    const int index = indexOf(device.deviceId);
    if (index >= 0) {
        // Ruling 4.8: the same device again, live or away. It keeps its
        // place and its order; the older live session is the caller's to
        // end with sameDevice. Nobody else is touched.
        Entry& held = m_entries[index];
        result.admission = Admission::SameDevice;
        result.replacedSession = held.state == State::Listening ? held.session : nullptr;
        held.session = session;
        held.state = State::Listening;
        held.awaySinceMs = 0;
        held.awayGeneration = 0;
        held.name = device.name;
        held.shortName = device.shortName;
        held.deviceKind = device.deviceKind;
        m_timeRanOut.remove(device.deviceId);
        if (m_placeTaken.contains(device.deviceId)) {
            result.placeTaken = m_placeTaken.take(device.deviceId);
        }
        emitChanges(placesBefore);
        return result;
    }
    if (!hasPlaceFree()) {
        // Every place is taken. From Task 41 a device that declared the
        // feature is asked the fifth-device question instead; here every
        // device meets the refusal.
        result.admission = Admission::Full;
        return result;
    }
    Entry admitted = device;
    admitted.state = State::Listening;
    admitted.order = m_nextOrder++;
    admitted.connectedSinceMs = time;
    admitted.awaySinceMs = 0;
    admitted.awayGeneration = 0;
    admitted.reportedActivityMs = time;
    admitted.lastActivityMs = time;
    admitted.session = session;
    m_entries.append(admitted);
    const auto ranOut = m_timeRanOut.constFind(device.deviceId);
    if (ranOut != m_timeRanOut.cend()) {
        result.timeRanOutAtMs = *ranOut;
        m_timeRanOut.erase(ranOut);
    }
    if (m_placeTaken.contains(device.deviceId)) {
        result.placeTaken = m_placeTaken.take(device.deviceId);
    }
    result.admission = Admission::Admitted;
    emitChanges(placesBefore);
    return result;
}

void DeviceSessionRegistry::sessionEnded(const QByteArray& deviceId, const QObject* session,
                                         EndKind kind)
{
    const int index = indexOf(deviceId);
    if (index < 0) {
        return;
    }
    Entry& held = m_entries[index];
    if (held.state != State::Listening || held.session != session || session == nullptr) {
        // Already replaced by a newer session of the same device, or away.
        return;
    }
    const int placesBefore = placesTaken();
    if (kind == EndKind::Left || held.kind != Kind::Paired) {
        // Ruling 4.12 and 4.10: leaving on purpose, and a token window that
        // can never be recognised again, free the place at once.
        m_entries.removeAt(index);
    } else {
        held.state = State::Away;
        held.session = nullptr;
        held.awaySinceMs = now();
        held.awayGeneration = m_nextAwayGeneration++;
    }
    emitChanges(placesBefore);
}

void DeviceSessionRegistry::remove(const QByteArray& deviceId)
{
    m_timeRanOut.remove(deviceId);
    m_placeTaken.remove(deviceId);
    const int index = indexOf(deviceId);
    if (index < 0) {
        return;
    }
    const int placesBefore = placesTaken();
    m_entries.removeAt(index);
    emitChanges(placesBefore);
}

void DeviceSessionRegistry::replace(const QByteArray& deviceId, const QByteArray& byId,
                                    const QString& byName)
{
    const int index = indexOf(deviceId);
    if (index < 0 || m_entries.at(index).kind == Kind::Hosting) {
        return;
    }
    const int placesBefore = placesTaken();
    m_entries.removeAt(index);
    m_timeRanOut.remove(deviceId);
    m_placeTaken.insert(deviceId, {byId, byName, now()});
    emitChanges(placesBefore);
}

std::optional<DeviceSessionRegistry::AdmitResult::TakenPlace>
DeviceSessionRegistry::placeTakenBy(const QByteArray& deviceId) const
{
    const auto it = m_placeTaken.constFind(deviceId);
    return it == m_placeTaken.cend() ? std::nullopt
                                     : std::optional<AdmitResult::TakenPlace>(*it);
}

QList<DeviceSessionRegistry::Entry>
DeviceSessionRegistry::replacementCandidates(const QByteArray& transmittingId) const
{
    QList<Entry> ordered = entries();
    std::sort(ordered.begin(), ordered.end(), [&transmittingId](const Entry& a, const Entry& b) {
        const int aGroup = a.state == State::Away ? 0
            : a.deviceId == transmittingId ? 2 : 1;
        const int bGroup = b.state == State::Away ? 0
            : b.deviceId == transmittingId ? 2 : 1;
        if (aGroup != bGroup) return aGroup < bGroup;
        const qint64 aSince = aGroup == 0 ? a.awaySinceMs : a.lastActivityMs;
        const qint64 bSince = bGroup == 0 ? b.awaySinceMs : b.lastActivityMs;
        return aSince != bSince ? aSince < bSince : a.order < b.order;
    });
    return ordered;
}

QList<QByteArray> DeviceSessionRegistry::expireAway()
{
    const qint64 time = now();
    QList<QByteArray> expired;
    QList<quint64> generations;
    const int placesBefore = placesTaken();
    for (int i = m_entries.size() - 1; i >= 0; --i) {
        const Entry& held = m_entries.at(i);
        if (held.state == State::Away && time - held.awaySinceMs >= kGraceMs) {
            expired.prepend(held.deviceId);
            generations.prepend(held.awayGeneration);
            // Ruling 4.11: kept for graceEnded (Task 74) and placeFreed
            // (Task 41) until its next admission, a revoke, or a restart.
            m_timeRanOut.insert(held.deviceId, time);
            m_entries.removeAt(i);
        }
    }
    if (!expired.isEmpty()) {
        emitChanges(placesBefore);
    }
    for (int i = 0; i < expired.size(); ++i) {
        emit graceEnded(expired.at(i), generations.at(i));
    }
    return expired;
}

bool DeviceSessionRegistry::isCurrentAbsence(const QByteArray& deviceId,
                                             quint64 awayGeneration) const
{
    const int index = indexOf(deviceId);
    if (index < 0) {
        return true;
    }
    const Entry& held = m_entries.at(index);
    return held.state == State::Away && held.awayGeneration == awayGeneration;
}

std::optional<qint64> DeviceSessionRegistry::nextExpiryMs() const
{
    std::optional<qint64> next;
    for (const Entry& held : m_entries) {
        if (held.state != State::Away) {
            continue;
        }
        const qint64 due = held.awaySinceMs + kGraceMs;
        if (!next || due < *next) {
            next = due;
        }
    }
    return next;
}

void DeviceSessionRegistry::noteActivity(const QByteArray& deviceId)
{
    const int index = indexOf(deviceId);
    if (index < 0) {
        return;
    }
    Entry& held = m_entries[index];
    const qint64 time = now();
    held.lastActivityMs = time;
    ++m_revision;
    if (time - held.reportedActivityMs >= kActivityResolutionMs) {
        held.reportedActivityMs = time;
        emit changed();
    }
}

void DeviceSessionRegistry::registerHostingDevice(const QByteArray& id, const QString& name,
                                                  const QString& shortName)
{
    const int placesBefore = placesTaken();
    m_entries.erase(std::remove_if(m_entries.begin(), m_entries.end(),
                                   [&id](const Entry& e) {
                                       return e.kind == Kind::Hosting || e.deviceId == id;
                                   }),
                    m_entries.end());
    const qint64 time = now();
    Entry hosting;
    hosting.deviceId = id;
    hosting.kind = Kind::Hosting;
    hosting.name = name;
    hosting.shortName = shortName;
    hosting.deviceKind = QStringLiteral("station");
    hosting.order = m_nextOrder++;
    hosting.connectedSinceMs = time;
    hosting.reportedActivityMs = time;
    hosting.lastActivityMs = time;
    m_entries.append(hosting);
    emitChanges(placesBefore);
}

void DeviceSessionRegistry::unregisterHostingDevice()
{
    const int placesBefore = placesTaken();
    const auto end = std::remove_if(m_entries.begin(), m_entries.end(),
                                    [](const Entry& e) { return e.kind == Kind::Hosting; });
    if (end == m_entries.end()) {
        return;
    }
    m_entries.erase(end, m_entries.end());
    emitChanges(placesBefore);
}

QList<DeviceSessionRegistry::Entry> DeviceSessionRegistry::entries() const
{
    return m_entries;
}

std::optional<DeviceSessionRegistry::Entry> DeviceSessionRegistry::entry(
    const QByteArray& deviceId) const
{
    const int index = indexOf(deviceId);
    if (index < 0) {
        return std::nullopt;
    }
    return m_entries.at(index);
}

std::optional<DeviceSessionRegistry::Entry> DeviceSessionRegistry::entryForSession(
    const QObject* session) const
{
    if (session == nullptr) {
        return std::nullopt;
    }
    for (const Entry& held : m_entries) {
        if (held.session == session) {
            return held;
        }
    }
    return std::nullopt;
}

std::optional<qint64> DeviceSessionRegistry::timeRanOutAtMs(const QByteArray& deviceId) const
{
    const auto it = m_timeRanOut.constFind(deviceId);
    if (it == m_timeRanOut.cend()) {
        return std::nullopt;
    }
    return *it;
}

QHash<QByteArray, DeviceSessionRegistry::NumberedName> DeviceSessionRegistry::numberNames(
    const QList<NameInput>& inOrder)
{
    // Every device's own words are reserved first, so a number never turns
    // one device's name into another's ("iPhone 2" paired after two
    // "iPhone"s keeps its name; the second "iPhone" becomes "iPhone 3").
    const auto numberOne = [](const QString& base, QSet<QString>* used,
                              const QSet<QString>& reserved) {
        // Slice control plan Task 17: no name is not a name to number; a
        // second nameless device would read " 2".
        if (base.isEmpty()) {
            return base;
        }
        if (!used->contains(base)) {
            used->insert(base);
            return base;
        }
        for (int n = 2;; ++n) {
            const QString candidate = QStringLiteral("%1 %2").arg(base).arg(n);
            if (!used->contains(candidate) && !reserved.contains(candidate)) {
                used->insert(candidate);
                return candidate;
            }
        }
    };
    QSet<QString> reservedNames;
    QSet<QString> reservedShort;
    for (const NameInput& input : inOrder) {
        reservedNames.insert(input.name);
        reservedShort.insert(input.shortName);
    }
    QSet<QString> usedNames;
    QSet<QString> usedShort;
    QHash<QByteArray, NumberedName> out;
    for (const NameInput& input : inOrder) {
        if (out.contains(input.key)) {
            continue;
        }
        NumberedName numbered;
        // A later device's own words are not reserved against an earlier
        // one: the earlier keeps its name.
        QSet<QString> nameReserve = reservedNames;
        nameReserve.remove(input.name);
        QSet<QString> shortReserve = reservedShort;
        shortReserve.remove(input.shortName);
        numbered.name = numberOne(input.name, &usedNames, nameReserve);
        numbered.shortName = numberOne(input.shortName, &usedShort, shortReserve);
        out.insert(input.key, numbered);
    }
    return out;
}

QString DeviceSessionRegistry::kindWord(const QString& deviceKind)
{
    if (deviceKind == QLatin1String("phone")) {
        return QStringLiteral("Phone");
    }
    if (deviceKind == QLatin1String("tablet")) {
        return QStringLiteral("Tablet");
    }
    return QStringLiteral("Computer");
}

QString DeviceSessionRegistry::usableShortName(const QString& shortName, const QString& deviceKind)
{
    return DeviceStore::isValidShortName(shortName) ? shortName : kindWord(deviceKind);
}

QString DeviceSessionRegistry::tokenWindowName(const QString& peerAddress)
{
    if (peerAddress.isEmpty()) {
        return QStringLiteral("Computer");
    }
    QHostAddress address(peerAddress);
    if (address.isNull()) {
        return QStringLiteral("Computer");
    }
    address.setScopeId(QString());
    bool mapped = false;
    const quint32 ipv4 = address.toIPv4Address(&mapped);
    if (mapped) {
        address = QHostAddress(ipv4);
    }
    return QStringLiteral("Computer at %1").arg(address.toString());
}

} // namespace NereusSDR
