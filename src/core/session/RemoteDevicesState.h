// 2026-10-01: Authenticated Core address inventory and reconnect learning.
// J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex. NereusSDR-original.

#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/RemoteDevicesState.h  (NereusSDR)
// =================================================================
//
// A remote window's copy of who else is on the Core (iPhone app plan Task
// 78, R-IOS-02, R-IOS-07, R-IOS-30; the several-devices design,
// docs/architecture/2026-09-24-several-devices-on-one-core-design.md,
// section 12). StationClient feeds it what a Core sends a device that
// declared `sessionHolder` 1 (the link document, sections 7.1 and 7.5):
//
//   connectedDevices   who holds a place now (listJson), the device limit
//   devices            the paired devices (listJson) and the Core's own
//                      facts a This Core page shows (label, key backup,
//                      pairing window and its code). The code is a secret:
//                      kept for the page to show, never logged
//   marker:<id>        another device's slice, read only
//   confirm.request    the Core's one open question for this window
//   notice             what another device did, or this window's own state
//   session.held       the Core is full: which device this window replaces
//                      (the link document, section 5.1 step 4; G-53)
//
// Plain state for the window's screens to draw; nothing here is ever
// written back. The questions' answers and Take it back go out through
// StationClient's verbs.
//
// Single thread: the StationClient's.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-26: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 78 (R-IOS-02, R-IOS-07,
//               R-IOS-30), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-28: session.held (the fifth-device choice, iPhone app plan
//               Task 78 item 7, G-53) and a slice's frequency; the devices
//               object's Core facts (iPhone app plan Task 25's This Core
//               page). J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-30: sliceHolderDevice(), the hosting desktop for the id
//               "station" (desktop listening lane). J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include "core/NereusCoreExport.h"
#include "core/session/MirrorSchema.h"
#include "core/session/SessionMessages.h"

#include <QDateTime>
#include <QList>
#include <QMap>
#include <QObject>
#include <QString>

#include <optional>

namespace NereusSDR {

/// One slice named in a device's entry: `{sliceId, letter, band, mode}`.
struct RemoteDeviceSlice {
    int sliceId = -1;
    QString letter;
    int band = -1;
    int mode = -1;
    /// Sent in session.held's entries (the slice's frequency now); 0 when
    /// the Core does not say.
    double frequencyHz = 0.0;
};

/// One entry of `connectedDevices`' list (the link document, section 7.1).
struct RemoteConnectedDevice {
    QString deviceId;
    QString name;
    QString shortName;
    QString kind;
    bool paired = false;
    bool hostsCore = false;
    bool revocable = false;
    /// `listening`, `transmitting` or `away`.
    QString state;
    bool holdsTransmit = false;
    qint64 lastActivitySeconds = 0;
    qint64 connectedForSeconds = 0;
    qint64 awayForSeconds = 0;
    qint64 transmittingForSeconds = 0;
    QList<RemoteDeviceSlice> listeningOn;
    std::optional<RemoteDeviceSlice> transmittingOn;
};

/// One entry of the `devices` object's list: a device paired with the Core.
struct RemotePairedDevice {
    QString id;
    QString name;
    QString shortName;
    QString kind;
    QString lastSeen;
    /// ISO 8601 UTC, "" when the Core does not say.
    QString pairedAt;
    bool connected = false;
};

/// The `devices` object's facts about the Core itself (StationDevicesFacade;
/// iPhone app plan Task 25, the This Core page).
struct RemoteCoreDevicesInfo {
    /// False until the Core sent the object.
    bool received = false;
    QString stationLabel;
    QString coreAddresses; // Authenticated devices value, never logged.

    bool claimed = false;
    bool tokenActive = false;
    bool keyBackupAcknowledged = false;
    /// Where the Core's key file is, on the Core's computer.
    QString keyPath;
    bool pairingWindowOpen = false;
    /// Sent only to a window signed in with this computer's key; "" to any
    /// other. Never logged.
    QString pairingCode;
};

/// A `marker:<id>`: another device's slice, as the Core sends it.
struct RemoteSliceMarker {
    int sliceId = -1;
    QString ownerDeviceId;
    QString ownerName;
    QString ownerShortName;
    QString ownerKind;
    bool ownerAway = false;
    double frequencyHz = 0.0;
    int dspMode = -1;
    int filterLowHz = 0;
    int filterHighHz = 0;
    bool txSlice = false;
    int band = -1;
    int streamIndex = -1;
    bool psPaused = false;

    /// 'A' + sliceId.
    QString letter() const;
};

/// One device a held window may replace (session.held's `devices`): a
/// connectedDevices entry plus `replaceable` (false for the desktop that
/// hosts the Core) and `from` (its address, or "relay").
struct RemoteHeldEntry {
    RemoteConnectedDevice device;
    bool replaceable = false;
    QString from;
};

/// session.held: the Core is full and asks which device this window takes
/// the place of (the link document, section 5.1 step 4). The entries come
/// in the Core's order: away devices first, longest away first, then by
/// how long each has been idle, the one on the air last.
struct RemoteHeldList {
    QList<RemoteHeldEntry> entries;
    quint32 revision = 0;
    /// placeTaken: another device took this window's place earlier.
    QString placeTakenByName;
    QString placeTakenById;
    std::optional<qint64> placeTakenSecondsAgo;
    /// placeFreed: this window's place was freed after its time away ran
    /// out, this many seconds ago.
    std::optional<qint64> placeFreedSecondsAgo;
    /// The entry to start on: the device that took this window's place,
    /// when it is still there and can be replaced; empty for the first
    /// replaceable entry.
    QString preselectId;
    /// This computer's clock when it arrived.
    QDateTime receivedAt;
};

/// A `confirm.request` or a `notice`, with its reason and when it arrived.
struct RemotePrompt {
    SessionPrompt prompt;
    QString reason;
    /// This computer's clock when it arrived.
    QDateTime receivedAt;

    /// When it happened, by this computer's clock (`secondsAgo` before it
    /// arrived).
    QDateTime happenedAt() const { return receivedAt.addSecs(-prompt.secondsAgo); }
};

class NEREUS_CORE_EXPORT RemoteDevicesState : public QObject {
    Q_OBJECT

public:
    explicit RemoteDevicesState(QObject* parent = nullptr);

    /// Whether `objectKey` is one this state keeps (connectedDevices,
    /// devices, marker:<id>).
    static bool holdsKey(const QByteArray& objectKey);
    /// The slice id a `marker:<id>` key names, or -1.
    static int markerSliceId(const QByteArray& objectKey);

    /// An object.create or delta for a key holdsKey() accepts.
    void applyObject(const QByteArray& objectKey, const QList<MirrorUpdate>& updates);
    /// An object.destroy.
    void destroyObject(const QByteArray& objectKey);

    /// A confirm.request: it replaces any question still open (a device has
    /// one open question, the link document section 7.5).
    void setQuestion(const SessionPrompt& prompt, const QString& reason);
    /// The open question was answered or dropped. Nothing when `id` is not
    /// the open one.
    void closeQuestion(qint64 id);
    std::optional<RemotePrompt> question() const { return m_question; }

    /// A notice. One with the id of a notice already held replaces it.
    void addNotice(const SessionPrompt& prompt, const QString& reason);
    /// The operator put the card away, or took it back.
    void dismissNotice(qint64 id);
    QList<RemotePrompt> notices() const { return m_notices; }
    std::optional<RemotePrompt> notice(qint64 id) const;

    QList<RemoteSliceMarker> markers() const { return m_markers.values(); }
    std::optional<RemoteSliceMarker> marker(int sliceId) const;
    QList<RemoteConnectedDevice> connectedDevices() const { return m_connected; }
    std::optional<RemoteConnectedDevice> connectedDevice(const QString& deviceId) const;
    /// The entry a slice-access id names (`controllerDeviceId`, a
    /// `listenerDeviceIds` entry). The id "station" is the Core's own
    /// position: it names the desktop that hosts the Core, the entry with
    /// `hostsCore`, never matched by id (its wire id is base64url of
    /// "station"). Nothing for "station" on a Core no desktop hosts.
    std::optional<RemoteConnectedDevice> sliceHolderDevice(const QString& wireId) const;
    int deviceLimit() const { return m_deviceLimit; }
    /// This window's own device id as the Core sends ids (for "this
    /// window" in the lists); set by StationClient per session.
    void setSelfDeviceId(const QString& id) { m_selfDeviceId = id; }
    QString selfDeviceId() const { return m_selfDeviceId; }
    QList<RemotePairedDevice> pairedDevices() const { return m_paired; }
    RemoteCoreDevicesInfo coreInfo() const { return m_coreInfo; }

    /// session.held arrived (it replaces any list still shown).
    void setHeld(const RemoteHeldList& held);
    /// The question was answered, or the session moved on or ended.
    void clearHeld();
    std::optional<RemoteHeldList> held() const { return m_held; }

    /// The session ended: everything here was that session's.
    void clear();

    /// Parsers, public for tests.
    static QList<RemoteConnectedDevice> parseConnectedList(const QString& listJson);
    static QList<RemotePairedDevice> parsePairedList(const QString& listJson);
    static RemoteConnectedDevice parseConnectedDevice(const QJsonObject& entry);
    /// session.held's fields; `preselectHint` names the device to start on
    /// when the Core sends no placeTaken (the device that took this
    /// window's place, from the end that stopped it).
    static RemoteHeldList parseHeld(const QJsonArray& devices, quint32 revision,
                                    const std::optional<QJsonObject>& placeTaken,
                                    const std::optional<QJsonObject>& placeFreed,
                                    const QString& preselectHint = QString());

signals:
    void markersChanged();
    void connectedDevicesChanged();
    void pairedDevicesChanged();
    /// coreInfo() changed.
    void coreInfoChanged();
    void questionChanged();
    /// session.held arrived, or its question closed.
    void heldChanged();
    /// A new notice arrived (its id), after noticesChanged.
    void noticeArrived(qint64 id);
    void noticesChanged();

private:
    QMap<int, RemoteSliceMarker> m_markers;
    QList<RemoteConnectedDevice> m_connected;
    QList<RemotePairedDevice> m_paired;
    RemoteCoreDevicesInfo m_coreInfo;
    int m_deviceLimit = 0;
    QString m_selfDeviceId;
    std::optional<RemotePrompt> m_question;
    std::optional<RemoteHeldList> m_held;
    QList<RemotePrompt> m_notices;
};

} // namespace NereusSDR
