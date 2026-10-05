#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/StationDevicesFacade.h  (NereusSDR)
// =================================================================
//
// The mirrored `devices` object (iPhone app plan Task 13, R-IOS-08; spec
// section 5.2 item 8; the pairing design, docs/architecture/2026-08-02-
// remote-station-identity-and-pairing-design.md section 7, "Devices and
// revocation").
//
// What a paired device's Devices page shows about the Core, all station to
// client: the paired devices (listJson), the Core's label, whether it is
// claimed, whether the old pairing token still works, whether the Core's
// identity key backup was acknowledged, and where that key file is. Every
// change moves `revision` once (serial-number arithmetic, as NotchModel's).
//
// And the four things a device asks of it (SessionCommandDispatcher routes
// the verbs here; deviceAdminVersion 1):
//
//   devices.revoke {id}            remove a paired device. StationServer
//                                  ends that device's connection on
//                                  DeviceStore::deviceRemoved, whatever
//                                  removed it. The last device is refused
//                                  while no token is active (the Core would
//                                  be unclaimed again; only the console's
//                                  reset does that), and so is a computer
//                                  enrolled through the token while the
//                                  token works (it would enrol again).
//   station.rename {label}         store the label under the Core-owned
//                                  StationLabel setting.
//   station.acknowledgeKeyBackup   the operator has backed up the Core's
//                                  identity key. Stored as the key's
//                                  fingerprint under the Core-owned
//                                  StationKeyBackupAcknowledged setting, so
//                                  a replaced key asks again.
//   station.retireToken            stop accepting the old pairing token,
//                                  once a device is paired. StationServer
//                                  ends every connection signed in by token
//                                  on tokenRetired().
//
// The facade knows nothing about connections: StationServer tells it which
// devices are connected (setConnectedDevices) and ends connections itself.
// Up to four devices may be connected at once (iPhone app Task 71); the
// facade keeps a set for that reason.
//
// iPhone app Task 14 (R-IOS-08, pairingVersion 1): the pairing window.
// `pairingWindowOpen` and `pairingCode` follow the Core's PairingWindow,
// and two more verbs act on it:
//
//   pairing.open    reopen the window on a claimed Core; the result's
//                   values carry `code`.
//   pairing.close   close a reopened window.
//
// The code is a secret: StationServer sends `pairingCode` and the verb's
// `code` only to a connection signed in with a paired device's key, and
// "" to any other (a window signed in with the old pairing token). Nothing
// here logs it.
//
// The phone's direct addresses (coreAddressesVersion 1): `coreAddresses`
// is where a device can dial this Core's control listener, as
// StationServer's CoreAddressWatcher reads them (CoreAddresses.h). It has
// its own notify signal, so a change never moves `revision`. StationServer
// sends it only to a connection signed in with a paired device's own key
// whose hello declared coreAddresses 1, and to no other.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-24: iPhone app Task 14 (R-IOS-08): pairingWindowOpen,
//               pairingCode, openPairing() and closePairing(). J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-24: iPhone app Task 17 (R-IOS-08): resetUnclaimed(), the
//               console's `reset --unclaimed --yes`. J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude Code.
//   2026-09-24: Part C fix wave (R1-I1): the last device is not
//               revoked while no token is active. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-24: Part C fix wave (R1-I3): a computer enrolled
//               through the token is not revoked while the token works. J.J.
//               Boyd (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-25: iPhone app Task 71 (R-IOS-02): numbered names and
//               shortName in listJson. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-29: The phone's direct addresses: coreAddresses
//               (coreAddressesVersion 1). J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-29: slice control plan Task 8b: retireTokenAndRevoke. J.J.
//               Boyd (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-30: Fix wave LINK-I4: pairing opened at the Core
//               (openPairingAtCore) turns pairing through the service
//               back on, and its shut state is shown on the Core. J.J.
//               Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/NereusCoreExport.h"
#include <QByteArray>
#include <QObject>
#include <QSet>
#include <QString>

namespace NereusSDR {

class AppSettings;
class DeviceStore;
class PairingWindow;
class StationIdentity;
class TokenStore;

/// What a device-administration request came to.
struct DeviceAdminResult {
    bool accepted = false;
    /// Plain operator words; empty when accepted.
    QString reason;
};

class NEREUS_CORE_EXPORT StationDevicesFacade final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString listJson READ listJson NOTIFY devicesStateChanged)
    Q_PROPERTY(quint32 revision READ revision NOTIFY devicesStateChanged)
    Q_PROPERTY(QString stationLabel READ stationLabel NOTIFY devicesStateChanged)
    Q_PROPERTY(bool claimed READ claimed NOTIFY devicesStateChanged)
    Q_PROPERTY(bool tokenActive READ tokenActive NOTIFY devicesStateChanged)
    Q_PROPERTY(bool keyBackupAcknowledged READ keyBackupAcknowledged
                   NOTIFY devicesStateChanged)
    Q_PROPERTY(QString keyPath READ keyPath NOTIFY devicesStateChanged)
    // iPhone app Task 14: after keyPath, so every earlier ordinal stays.
    Q_PROPERTY(bool pairingWindowOpen READ pairingWindowOpen NOTIFY devicesStateChanged)
    Q_PROPERTY(QString pairingCode READ pairingCode NOTIFY devicesStateChanged)
    // Direct addresses: after pairingCode, so every earlier ordinal stays.
    Q_PROPERTY(QString coreAddresses READ coreAddresses NOTIFY coreAddressesChanged)

public:
    /// The Core-owned setting that holds the acknowledged key's fingerprint.
    static constexpr const char* kKeyBackupSettingsKey = "StationKeyBackupAcknowledged";

    /// None of the four is owned; each must outlive this object.
    /// iPhone app Task 14: `pairingWindow` (not owned, may be null) is the
    /// Core's pairing window, as setPairingWindow() takes it.
    StationDevicesFacade(DeviceStore& devices, TokenStore& tokens,
                         const StationIdentity& identity, AppSettings& settings,
                         QObject* parent = nullptr, PairingWindow* pairingWindow = nullptr);

    /// A JSON array of {id, name, shortName, kind, pairedAt, lastSeen,
    /// connected}, in pairing order. `id` is the device's key fingerprint
    /// in base64url; the times are ISO 8601 UTC ("" when never seen).
    /// iPhone app Task 71 (ruling 4.3): `name` and `shortName` are numbered
    /// on collisions by pairing order ("iPhone", "iPhone 2"), as
    /// `connectedDevices` numbers them, and `shortName` is the kind's word
    /// ("Phone", "Tablet", "Computer") when the device sent none usable.
    QString listJson() const { return m_state.listJson; }
    quint32 revision() const { return m_revision; }
    /// The Core's label as displayed; "" when it has none yet (no rename
    /// and no StationCallsign).
    QString stationLabel() const { return m_state.stationLabel; }
    bool claimed() const { return m_state.claimed; }
    bool tokenActive() const { return m_state.tokenActive; }
    bool keyBackupAcknowledged() const { return m_state.keyBackupAcknowledged; }
    QString keyPath() const { return m_state.keyPath; }
    /// iPhone app Task 14: the Core's pairing window is open, and its
    /// current code ("" while closed or while no code is shown). See the
    /// header comment for who receives the code.
    bool pairingWindowOpen() const { return m_state.pairingWindowOpen; }
    QString pairingCode() const { return m_state.pairingCode; }
    /// LINK-I4: pairing through the remote access service is shut after
    /// too many wrong codes (PairingWindow::isServiceShut()). Shown on the
    /// Core's own window and console; not mirrored.
    bool servicePairingShut() const { return m_state.servicePairingShut; }
    /// Where a device can dial this Core, as compact JSON
    /// {"addresses":["[2001:db8::5]:47910","203.0.113.7:47910"]}
    /// (CoreAddresses::toJson); an empty list while the Core does not
    /// listen or has no stable global address.
    QString coreAddresses() const { return m_coreAddresses; }
    /// StationServer's CoreAddressWatcher sets it; a change notifies once.
    void setCoreAddresses(const QString& json);

    /// iPhone app Task 14: the Core's pairing window, which the two
    /// properties follow and the two verbs act on. Not owned; must outlive
    /// this object. Without one, the window reads closed and both verbs
    /// are refused.
    void setPairingWindow(PairingWindow* window);

    DeviceAdminResult revoke(const QString& id);
    /// Slice control plan Task 8b: removes a computer that joined with the
    /// pairing token while the token still works, by first stopping the
    /// token (retireToken) and then removing it (revoke), as one action.
    /// Every guard of both is checked before anything changes, so a refusal
    /// by a guard changes nothing: the token keeps working and the device
    /// stays paired. The Core is never left without a paired device (Fix
    /// wave R1-I1). One failure is left after the token has stopped: the
    /// device list cannot be written. Then the token stays stopped (which
    /// cannot be undone), the device stays paired, and the Core stays
    /// claimed; a plain revoke() can remove it afterwards. Any other device
    /// is removed as revoke() removes it.
    DeviceAdminResult retireTokenAndRevoke(const QString& id);
    /// Task 8b: whether removing `id` needs the pairing token stopped first
    /// (a computer that joined with the token, while the token works).
    bool revokeStopsPairingToken(const QString& id) const;
    DeviceAdminResult rename(const QString& label);
    DeviceAdminResult acknowledgeKeyBackup();
    DeviceAdminResult retireToken();
    /// pairing.open: reopen the window (a no-op while it is open).
    DeviceAdminResult openPairing();
    /// LINK-I4: the Core's own console or window opens pairing. As
    /// openPairing(), and pairing through the service turns back on
    /// (PairingWindow::reopenAtCore()), also while the window is open.
    DeviceAdminResult openPairingAtCore();
    /// pairing.close: close a reopened window (a no-op while it is closed).
    DeviceAdminResult closePairing();
    /// iPhone app Task 17: the console's `reset --unclaimed --yes`, which no
    /// device can ask for. Moves a damaged token file and a damaged device
    /// list aside, retires the token, removes every paired device, so the
    /// Core is unclaimed and its pairing window open again with a new
    /// code. Every connection ends: each removed device's through
    /// DeviceStore::deviceRemoved, and every one signed in with the token
    /// through tokenRetired().
    DeviceAdminResult resetUnclaimed();

    /// The paired devices (by id) that hold an authenticated connection.
    void setConnectedDevices(const QSet<QByteArray>& ids);

    /// Re-reads everything; a change moves revision once and notifies.
    /// StationServer calls it when a setting the label follows changes.
    void refresh();

    /// Holds refresh() back until the matching resumeRefresh(), which
    /// refreshes once, so one event is one change.
    void holdRefresh();
    void resumeRefresh();

signals:
    void devicesStateChanged();
    /// The displayed label changed (a rename, or StationCallsign while no
    /// rename is stored). Task 16's announcement follows it.
    void stationLabelChanged(const QString& label);
    /// The token was retired through retireToken().
    void tokenRetired();
    /// coreAddresses changed. Not devicesStateChanged: revision stays.
    void coreAddressesChanged();

private:
    struct State {
        QString listJson;
        QString stationLabel;
        bool claimed = false;
        bool tokenActive = false;
        bool keyBackupAcknowledged = false;
        QString keyPath;
        bool pairingWindowOpen = false;
        QString pairingCode;
        bool servicePairingShut = false;

        bool operator==(const State&) const = default;
    };
    State compute() const;
    void attachPairingWindow(PairingWindow* window);

    DeviceStore& m_devices;
    TokenStore& m_tokens;
    const StationIdentity& m_identity;
    AppSettings& m_settings;
    PairingWindow* m_pairingWindow = nullptr;
    QSet<QByteArray> m_connected;
    State m_state;
    quint32 m_revision = 0;
    QString m_coreAddresses = QStringLiteral("{\"addresses\":[]}");
    int m_hold = 0;
    bool m_refreshWanted = false;
};

} // namespace NereusSDR
