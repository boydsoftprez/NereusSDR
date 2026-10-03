// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/DnsSdAdvertiser.h  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 16 (D36, R-IOS-16): the Core advertises itself over
// Bonjour (DNS-SD) beside its LAN announcement, because iOS lets an app
// receive custom multicast only with a permission Apple grants on request,
// while a Bonjour browse needs no such permission. The link document's
// section 14.2 is the authority for the record:
//
//   service type  _nereus-station._tcp, on the listener's port
//   TXT, in order v=1
//                 id=<the identity fingerprint, first 22 base64url characters>
//                 claimed=0|1
//                 pair=click|code|closed
//                 name=<the Core's label>
//                 devices=0..4 (iPhone app Task 71: places taken on the Core)
//
// Bonjour follows the announcer's rule: it advertises only where the
// listener serves (dnsSdInterfaceForListener), so a loopback-only listener
// is never advertised.
//
// One class, one backend per platform, chosen at build time:
//   DnsSdAdvertiserApple.cpp    dns_sd.h (macOS; part of the system)
//   DnsSdAdvertiserAvahi.cpp    the Avahi daemon over D-Bus (Linux, when
//                               Qt6::DBus is found; no Avahi development
//                               package is needed)
//   DnsSdAdvertiserWindows.cpp  DnsServiceRegister, loaded from dnsapi.dll
//                               at run time (Windows 10 1903 or later)
// A platform with none gets a backend that is never available. A Core with
// no Bonjour still announces over the LAN datagram and says so once.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-25: iPhone app Task 71 (R-IOS-02): the sixth TXT entry,
//               `devices`. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-28: iPhone app plan Task 25 (R-IOS-16): the seventh TXT entry,
//               `radio`. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
// =================================================================
#pragma once

#include "StationLanAnnouncement.h"

#include <QByteArray>
#include <QHostAddress>
#include <QList>
#include <QObject>
#include <QPair>
#include <QString>
#include <QtGlobal>

#include <functional>
#include <memory>
#include <optional>

namespace NereusSDR {

inline constexpr char kDnsSdServiceType[] = "_nereus-station._tcp";
/// The TXT record's own version, its `v` key.
inline constexpr char kDnsSdTxtVersion[] = "1";
/// `id` is this many base64url characters of the 43 the fingerprint takes:
/// 132 bits, enough to tell Cores apart in a list, and short enough to read.
inline constexpr int kDnsSdIdentityPrefixChars = 22;
/// A DNS label, the most an instance name may take.
inline constexpr int kDnsSdMaxInstanceNameBytes = 63;
/// Every interface the computer has.
inline constexpr quint32 kDnsSdAllInterfaces = 0;
/// This computer only: the record never leaves it (Apple backend only; the
/// tests use it so nothing reaches a real network).
inline constexpr quint32 kDnsSdThisComputerOnly = 0xFFFFFFFFu;

struct DnsSdRecord {
    /// What a browser shows before it reads the TXT record; see
    /// dnsSdInstanceName(). The phone lists the TXT record's `name`.
    QString instanceName;
    /// `name`: the StationLabel display, as the announcement carries it.
    QString label;
    /// The identity fingerprint, kStationLanIdentityBytes raw bytes.
    QByteArray identity;
    bool claimed = false;
    StationLanPairing pairing = StationLanPairing::Closed;
    /// iPhone app Task 71 (ruling 10.4): `devices`, the places taken on the
    /// Core, 0 to kStationLanMaxDevicesConnected (0 on a Core no device has
    /// claimed). A number only: who is on the Core never goes here.
    int devicesConnected = 0;
    /// iPhone app plan Task 25 (R-IOS-16): `radio`, the station's radio
    /// state as the announcement carries it ("offline", "connected" or
    /// "waiting").
    StationLanRadio radio = StationLanRadio::Offline;
    /// kDnsSdAllInterfaces, an interface index, or kDnsSdThisComputerOnly.
    quint32 interfaceIndex = kDnsSdAllInterfaces;

    bool operator==(const DnsSdRecord&) const = default;
};

using DnsSdTxtEntries = QList<QPair<QByteArray, QByteArray>>;

/// The TXT entries, in order v, id, claimed, pair, name, devices (iPhone app
/// Task 71: the sixth, after name), radio (iPhone app plan Task 25: the
/// seventh); `v` stays 1, since a client ignores a key it does not know. Empty, with `error` set, when the record cannot be
/// advertised (an identity that is not 32 bytes, a label outside section
/// 14's alphabet or length, a device count outside 0 to 4).
DnsSdTxtEntries dnsSdTxtEntries(const DnsSdRecord& record, QString* error = nullptr);
/// The TXT record's bytes (RFC 6763 section 6: each `key=value` preceded by
/// its length in one byte). Empty, with `error` set, as above.
QByteArray encodeDnsSdTxtRecord(const DnsSdRecord& record, QString* error = nullptr);
/// Splits TXT bytes into their entries, in order. A key without '=' has an
/// empty value. nullopt, with `error` set, when a length runs past the end
/// or an entry is empty.
std::optional<DnsSdTxtEntries> decodeDnsSdTxtRecord(const QByteArray& bytes,
                                                    QString* error = nullptr);

/// An instance name from `displayName` (the label, else the Core name):
/// control characters dropped, cut to kDnsSdMaxInstanceNameBytes of UTF-8
/// at a character boundary; "NereusSDR Core" when nothing is left.
QString dnsSdInstanceName(const QString& displayName);

/// Where a listener on `listener` is advertised: kDnsSdAllInterfaces for a
/// listener on every address, the interface holding the address for one
/// bound to a single address, nullopt (never advertised) for a loopback,
/// multicast, broadcast or null listener, or an address no interface holds.
std::optional<quint32> dnsSdInterfaceForListener(const QHostAddress& listener);

/// One platform's registration. Called on the advertiser's thread; a
/// backend reports a registration that failed after it was accepted
/// through `failed`, which it may call from its own callbacks on that
/// thread.
class DnsSdBackend {
public:
    virtual ~DnsSdBackend() = default;

    /// True when this computer can advertise now.
    virtual bool isAvailable() = 0;
    /// Registers the service; false when it was refused at once.
    virtual bool registerService(quint16 port, const DnsSdRecord& record,
                                 const DnsSdTxtEntries& txt) = 0;
    /// Replaces the TXT record of the registered service; false when the
    /// backend cannot, and the advertiser registers again instead.
    virtual bool updateTxt(const DnsSdRecord& record, const DnsSdTxtEntries& txt) = 0;
    /// Withdraws the service; nothing to do when none is registered.
    virtual void unregisterService() = 0;

    std::function<void(const QString& reason)> failed;
};

/// The backend this build has for this platform.
std::unique_ptr<DnsSdBackend> createPlatformDnsSdBackend();

class DnsSdAdvertiser : public QObject {
    Q_OBJECT

public:
    /// Uses the platform's backend.
    explicit DnsSdAdvertiser(QObject* parent = nullptr);
    /// Uses `backend` (a test's).
    explicit DnsSdAdvertiser(std::unique_ptr<DnsSdBackend> backend, QObject* parent = nullptr);
    ~DnsSdAdvertiser() override;

    DnsSdAdvertiser(const DnsSdAdvertiser&) = delete;
    DnsSdAdvertiser& operator=(const DnsSdAdvertiser&) = delete;

    /// Advertises `record` on `port`, or moves an advertisement already
    /// running to them. False when the port is 0, the record cannot be
    /// advertised, Bonjour is not available here (said once, in plain
    /// words, the first time) or the platform refused it.
    bool start(quint16 port, const DnsSdRecord& record);
    /// Changes what is advertised: the TXT record in place, or a new
    /// registration when the instance name or interface changed. Nothing
    /// to do while stopped.
    void update(const DnsSdRecord& record);
    void stop();

    bool isAvailable() const;
    bool isActive() const { return m_active; }
    quint16 port() const { return m_port; }
    /// What is advertised; default-constructed while stopped.
    DnsSdRecord record() const { return m_record; }

    /// The line a Core logs when it cannot advertise itself for iPhones.
    static QString unavailableText();

signals:
    /// A registration the platform had accepted failed later (a name
    /// conflict it could not resolve, the daemon going away).
    void failed(const QString& reason);

private:
    bool registerNow(quint16 port, const DnsSdRecord& record, const DnsSdTxtEntries& txt);

    std::unique_ptr<DnsSdBackend> m_backend;
    DnsSdRecord m_record;
    quint16 m_port = 0;
    bool m_active = false;
    bool m_unavailableLogged = false;
};

} // namespace NereusSDR
