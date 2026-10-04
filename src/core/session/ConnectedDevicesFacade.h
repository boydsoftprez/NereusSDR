#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/ConnectedDevicesFacade.h  (NereusSDR)
// =================================================================
//
// The mirrored `connectedDevices` object (iPhone app plan Task 71,
// R-IOS-02; the several-devices design, section 10.3, rulings 4.3 and
// 10.3): who is on the Core, the list a device's Devices page reads for
// "Connected now". Every property is outbound. StationServer sends it only
// at agreed minor 11 to a view whose hello declared `sessionHolder` 1 with
// `deviceAuth` 1 (sessionHolderVersion 1), so an older view never receives
// it.
//
//   listJson     a JSON array, one entry per device holding a place (live
//                or away), in admission order:
//                {deviceId, name, shortName, kind, paired, hostsCore,
//                 revocable, state, holdsTransmit, lastActivitySeconds,
//                 connectedForSeconds, awayForSeconds,
//                 transmittingForSeconds, listeningOn}
//                (transmittingOn is absent until Task 77.) Task 34:
//                holdsTransmit is true for the device holding transmit,
//                keyed or not; state is "transmitting" while it is on the
//                air; transmittingForSeconds is how long it has been.
//                listeningOn (Task 73) is every slice the device owns, an
//                away device's included: [{sliceId, letter, band, mode}],
//                from the listening provider the Core sets.
//   revision     moves by one with every change (serial-number arithmetic,
//                as `devices`' revision).
//   deviceLimit  4 (DeviceSessionRegistry::kMaxDeviceSessions).
//
// Names and short names are numbered by DeviceSessionRegistry::numberNames
// over the paired devices in pairing order, then a hosting desktop the
// store does not hold, then token windows in the order they connected: the
// same numbering the `devices` object uses, so one device reads the same on
// both lists.
//
// Durations are measured on the Core's monotonic clock (the registry's)
// each time listJson is read, which is when the mirror sends it (its
// object.create at attach, a delta on a change); never from the wall clock.
// The list changes, and is re-sent, only when something other than time
// passing changes in it; lastActivitySeconds moves at most once a minute
// per device (DeviceSessionRegistry::noteActivity).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 71 (R-IOS-02), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-25: iPhone app plan Task 73 (R-IOS-02): listeningOn, and one
//               device described the same way wherever the Core names it
//               (describe(), for markers and refusals). J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude Code.
//   2026-09-25: iPhone app plan Task 34 (R-IOS-02): holdsTransmit and state
//               "transmitting" (setTransmitProvider). J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude Code.
//   2026-09-26: iPhone app plan Task 77 (R-IOS-02, R-IOS-03, R-IOS-13):
//               transmittingOn. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
// =================================================================

#include "core/NereusCoreExport.h"
#include <QByteArray>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>

#include <functional>
#include <optional>

#include "core/session/DeviceSessionRegistry.h"

namespace NereusSDR {

class DeviceStore;

class NEREUS_CORE_EXPORT ConnectedDevicesFacade final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString listJson READ listJson NOTIFY connectedDevicesChanged)
    Q_PROPERTY(quint32 revision READ revision NOTIFY connectedDevicesChanged)
    Q_PROPERTY(int deviceLimit READ deviceLimit NOTIFY connectedDevicesChanged)

public:
    /// Neither is owned; both must outlive this object.
    ConnectedDevicesFacade(const DeviceSessionRegistry& registry, const DeviceStore& devices,
                           QObject* parent = nullptr);

    /// Measured now; see the header comment.
    QString listJson() const;
    quint32 revision() const { return m_revision; }
    int deviceLimit() const;

    /// Re-reads the registry and the paired devices; a change other than
    /// time passing moves revision once and notifies.
    void refresh();

    /// Task 73: what a device owns, for its listeningOn; the Core sets it.
    using ListeningProvider = std::function<QJsonArray(const QByteArray& deviceId)>;
    void setListeningProvider(ListeningProvider provider);

    /// iPhone app plan Task 34 (R-IOS-02): who holds transmit and whether it
    /// is on the air, for each entry's holdsTransmit and state
    /// "transmitting". The Core sets it.
    struct TransmitState {
        QByteArray holderDeviceId;
        bool keyed{false};
        /// On the Core's monotonic clock (the registry's), while keyed.
        qint64 keyedSinceMs{0};
        /// Task 77: the holder's transmit slice, {sliceId, letter, band,
        /// mode}, while it is on the air; empty otherwise.
        QJsonObject transmittingOn;
    };
    using TransmitProvider = std::function<TransmitState()>;
    void setTransmitProvider(TransmitProvider provider);

    /// Task 73: one device as the Core names it everywhere (ruling 4.3):
    /// its id on the wire (a paired device's key fingerprint in base64url,
    /// "token:<n>" for a token window), its numbered name and short name,
    /// and its kind. A paired device that holds no place is described from
    /// the device store. nullopt for an id the Core does not know.
    struct DeviceWords {
        QString wireId;
        QString name;
        QString shortName;
        QString kind;
    };
    std::optional<DeviceWords> describe(const QByteArray& deviceId) const;

    /// Holds refresh() back until the matching resumeRefresh(), which
    /// refreshes once: a sign-in (a new short name, then the admission) is
    /// one change.
    void holdRefresh() { ++m_hold; }
    void resumeRefresh();

signals:
    void connectedDevicesChanged();

private:
    /// The list without the durations measured from now: what decides
    /// whether it changed.
    QString stableForm() const;
    QString render(bool withDurations) const;
    /// Ruling 4.3's numbering over every device the Core names.
    QHash<QByteArray, DeviceSessionRegistry::NumberedName> numbered() const;

    const DeviceSessionRegistry& m_registry;
    const DeviceStore& m_devices;
    QString m_stable;
    quint32 m_revision = 0;
    ListeningProvider m_listening;
    TransmitProvider m_transmit;
    int m_hold = 0;
    bool m_refreshWanted = false;
};

} // namespace NereusSDR
