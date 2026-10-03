#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/RendezvousClient.h  (NereusSDR)
// =================================================================
//
// One connection to the remote access service (the rendezvous,
// docs/architecture/2026-09-23-rendezvous-v1.md; iPhone app plan Task 27,
// R-IOS-08 and R-IOS-16; the pairing design, section 5.3). Two roles, one
// per object, fixed by the first thing it is asked to do:
//
//   Station (the Core):  registerStation() proves the Core holds its
//     station identity key and keeps it registered, reconnecting with a
//     backoff whenever the connection goes. An introduction from a client
//     is verified before anything else happens: the device must be one the
//     Core has paired (the lookup below; a revoked device is no longer in
//     it) and its signature over "NereusSDR introduce v1\n" || the Core's
//     rendezvous id || the introducing connection's hello nonce must
//     verify with that device's key. Anything else is dropped without a
//     reply and counted (droppedIntroductions()); only an accepted one is
//     reported (introduced()). claimNameplate() holds the number in the
//     Core's pairing code while its pairing window is open.
//
//   Client (the desktop):  introduce() asks a Core for a connection by its
//     rendezvous id, signed with this computer's device key; openMailbox()
//     opens the pairing mailbox on a nameplate.
//
// Servers are an ordered list (the operator's own first, the default
// behind it; the pairing design, section 5.3): each is tried in turn and
// the first that answers with its hello is used. A client that is told
// `offline` (or `nameplateUnknown`) by one server tries the next, since the
// Core may be registered with a server further down the list.
//
// The rendezvous only introduces. Nothing here carries a session, and
// nothing here ends one: a session that runs on a connection an
// introduction set up keeps running when the service goes away (section
// 6.3, "What an end means to the Core").
//
// A test run (QStandardPaths test mode, which every test binary turns on)
// never contacts a service that is not on this computer.
//
// Nothing secret is logged: no id in full, no key, nonce, signature, SDP,
// candidate, mailbox body or nameplate number (the number is part of the
// pairing code).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-26: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-26: iPhone app plan Task 28 (R-IOS-16): retireIntroduction()
//               and liveIntroductions(). J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-27: Task 28 tail (R-IOS-16): setPingIntervalMs() for the lossy
//               link tests; the missed-pong bound explained against the
//               service's. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-27: iPhone app plan Task 29 fix wave (R-IOS-16): the relay
//               grant (rendezvous section 12.1) decoded and kept for the
//               relay leg; nothing acts on it yet. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include "core/session/RendezvousWire.h"

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QMetaType>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <functional>
#include <optional>

QT_BEGIN_NAMESPACE
class QTimer;
class QWebSocket;
QT_END_NAMESPACE

namespace NereusSDR {

/// An introduction the Core accepted: a paired device whose signature
/// verified.
struct RendezvousIntroduction {
    /// The service's id for this introduction (16 bytes); what answer() and
    /// sendCandidate() name.
    QByteArray id;
    /// The device's id (SHA-256 of its key, 32 bytes) and its key.
    QByteArray deviceId;
    QByteArray deviceKey;
    /// The client's offer, untouched.
    QString offer;
};

class RendezvousClient : public QObject {
    Q_OBJECT

public:
    enum class Role { None, Station, Client };
    Q_ENUM(Role)

    /// Signs a transcript with a private key this object never sees.
    using Signer = std::function<QByteArray(const QByteArray& message)>;
    /// The key (SPKI DER) of the paired device with this id, or empty when
    /// the Core has no such device (never paired, or revoked).
    using DeviceLookup = std::function<QByteArray(const QByteArray& deviceId)>;

    /// The service this build uses when nereusd.conf names none.
    static constexpr const char* kDefaultServer = "rv.nereussdr.com";
    /// How long one server may take to open and send its hello before the
    /// next is tried: the service's own handshake timer (section 9.1). A
    /// Core that has the hello gets as long again to be registered, or it
    /// leaves that connection and reconnects (fix wave I3).
    static constexpr int kHelloTimeoutMs = 10000;
    /// A registered Core pings the service this often, and reconnects when a
    /// ping is still unanswered at the next tick. These are the service's
    /// own timers: it pings every 20 s and closes a connection whose pong is
    /// 20 s late (rendezvous/server/nereus_rendezvous/config.py
    /// ping_interval_seconds = 20, ping_timeout_seconds = 20; section 9.2).
    /// With one ping allowed outstanding for one 20 s interval, the Core
    /// leaves a path that has carried no pong for 20 to 40 s, the same
    /// window in which the service gives up on it. On TCP a pong is never
    /// lost on its own, only held behind a stalled path, so a longer
    /// tolerance keeps nothing: until G-08 the Core allowed two missed
    /// pongs (40 to 60 s), which only left it up to 20 s longer on a
    /// connection the service had already closed.
    static constexpr int kPingIntervalMs = 20000;
    static constexpr int kMaxMissedPongs = 1;
    /// The most candidates the Core sends for one introduction (section
    /// 9.1's per-side cap).
    static constexpr int kMaxCandidatesPerIntroduction = 64;
    /// The most introductions the Core holds open at once. Each is a
    /// paired device asking for a connection, and the service ends one
    /// after 120 s (section 9.1), so a few are plenty; one past this is
    /// dropped and counted, as a service that replays accepted
    /// introductions could otherwise grow the list without end.
    static constexpr int kMaxLiveIntroductions = 16;

    explicit RendezvousClient(QObject* parent = nullptr);
    ~RendezvousClient() override;

    RendezvousClient(const RendezvousClient&) = delete;
    RendezvousClient& operator=(const RendezvousClient&) = delete;

    /// Reads nereusd.conf's `rendezvous_servers` entries: a host name
    /// (wss://name/), `host:port`, a bracketed IPv6 literal with an
    /// optional port, or a wss:// URL; a ws:// URL only to this computer
    /// (a service on the Core's own computer, or a test). Entries it cannot
    /// read go to `rejected`.
    static QList<QUrl> serverUrls(const QStringList& entries, QStringList* rejected = nullptr);

    /// The ordered list of servers. Takes effect on the next connection.
    void setServers(const QList<QUrl>& servers);
    QList<QUrl> servers() const { return m_servers; }

    /// Reconnect waits of the station role, in order, the last repeated.
    void setReconnectDelaysMs(const QList<int>& delays);
    /// The hello (and, for a Core, the registration) time in use:
    /// kHelloTimeoutMs unless a test shortened it. Applies from the next
    /// connection.
    void setHelloTimeoutMs(int ms);
    int helloTimeoutMs() const { return m_helloTimeoutMs; }
    /// The ping interval in use: kPingIntervalMs unless a test shortened
    /// it (the missed-pong rule is the same). Applies to the running timer.
    void setPingIntervalMs(int ms);
    int pingIntervalMs() const;

    /// Section 12.9: opt in only when this owner has a separate watch transport.
    /// Defaults off. Applies to the next service connection.
    void setWatchRelayEnabled(bool enabled) { m_watchRelayEnabled = enabled; }

    // ── Station role ──────────────────────────────────────────────────

    /// Registers the Core under the rendezvous id of `stationKey` (its
    /// station identity key, SPKI DER, never the TLS certificate's) and
    /// stays registered until stop(). `sign` signs with that key;
    /// `devices` looks up a paired device's key.
    void registerStation(const QByteArray& stationKey, Signer sign, DeviceLookup devices);
    /// Whether this Core asks for relay credentials when it answers
    /// (nereusd.conf `relay = allow|deny`, default allow).
    void setRelayAllowed(bool allowed) { m_relayAllowed = allowed; }
    bool relayAllowed() const { return m_relayAllowed; }

    /// Holds a nameplate while registered (again after each reconnect)
    /// until releaseNameplate(). nameplateClaimed() gives its number.
    void claimNameplate();
    void releaseNameplate();

    /// Answers an introduction this object reported, once. False when it
    /// is not one, was answered already, or the answer does not fit.
    bool answer(const QByteArray& introductionId, const QString& sdp);
    /// One candidate for an answered introduction; an empty one ends them.
    /// An `a=` in front is removed. False when it is not sent.
    bool sendCandidate(const QByteArray& introductionId, const QString& candidate);

    /// iPhone app plan Task 28 (R-IOS-16): forgets an introduction this
    /// Core has finished with (its connection opened, failed or ran out of
    /// time), at once, so it no longer holds one of kMaxLiveIntroductions
    /// until the service ends it. Nothing goes on the wire (the rendezvous
    /// document, section 6.3: an end only ever means the introduction is
    /// over); later messages for it are ignored, and the same id introduced
    /// again is dropped. False when it was not live.
    bool retireIntroduction(const QByteArray& introductionId);
    int liveIntroductions() const { return static_cast<int>(m_liveIntroductions.size()); }
    /// Rendezvous section 12.1: the relay grant the service sent for an
    /// introduction this Core answered (station), or for this client's live
    /// introduction (client, `introductionId` ignored). Kept for the relay
    /// leg (iPhone app plan Task 29 step 2b); nothing acts on it yet.
    std::optional<RendezvousWire::RelayGrant> relayGrant(
        const QByteArray& introductionId = QByteArray()) const;

    bool isRegistered() const { return m_registered; }
    QString stationId() const { return m_stationId; }
    /// Introductions dropped without a reply: an unknown or revoked device,
    /// a signature that did not verify, or one past kMaxLiveIntroductions
    /// while that many are open.
    quint64 droppedIntroductions() const { return m_droppedIntroductions; }
    /// Reconnect waits scheduled in a row: back to 0 when the Core is
    /// registered, except after the service said `replaced` (another
    /// connection registered the same key), so two Cores that share a key
    /// back off instead of taking the registration from each other every
    /// second (fix wave).
    int reconnectAttempts() const { return m_reconnectAttempt; }

    // ── Client role ───────────────────────────────────────────────────

    /// Connects to the first server that answers, without asking for
    /// anything yet; connected() follows, with stunUrls() filled, so a
    /// peer connection can be built with the service's STUN server before
    /// its offer is made. The service closes a client that sends nothing
    /// within its handshake time (10 s, section 3).
    void connectToService();
    /// Asks the Core registered as `stationId` for a connection, as the
    /// device with key `deviceKey` (SPKI DER), signing with `sign`.
    void introduce(const QString& stationId, const QByteArray& deviceKey, Signer sign,
                   const QString& offer);
    /// One candidate for this client's introduction; an empty one ends
    /// them. An `a=` in front is removed.
    bool sendCandidate(const QString& candidate);
    /// Opens the pairing mailbox on `nameplate` (the number in a code).
    void openMailbox(int nameplate);

    // ── Both roles ────────────────────────────────────────────────────

    bool sendMailbox(const QString& body);
    void closeMailbox();
    bool isMailboxOpen() const { return m_mailboxOpen; }

    /// Closes the connection and stops reconnecting.
    void stop();

    Role role() const { return m_role; }
    /// The server in use, empty when none is.
    QUrl currentServer() const;
    /// The STUN URLs the service's hello listed.
    QStringList stunUrls() const { return m_stunUrls; }
    bool isConnected() const { return m_helloReceived; }

signals:
    /// The service's hello arrived (either role): stunUrls() is filled.
    void connected();
    /// Station: registered, and again after each reconnect.
    void registered();
    /// The connection to the service ended; a station reconnects by
    /// itself.
    void connectionLost();
    /// Station: an introduction from a paired device whose signature
    /// verified.
    void introduced(const NereusSDR::RendezvousIntroduction& introduction);
    /// Station: the relay credentials for an introduction it answered with
    /// the relay allowed (`relayOffered` false when the service has no
    /// relay).
    void credentialsReceived(const QByteArray& introductionId, bool relayOffered,
                             const NereusSDR::RendezvousWire::Turn& turn);
    /// A candidate from the far end (station: for this introduction; the
    /// client's introduction id is empty). Empty: the end of candidates.
    void candidateReceived(const QByteArray& introductionId, const QString& candidate);
    /// Rendezvous section 12.1: a relay grant arrived (station: for this
    /// answered introduction; client: empty id) and is held (relayGrant()).
    void relayGrantReceived(const QByteArray& introductionId);
    /// The introduction ended (`clientLeft`, `stationLeft`, `expired`, or a
    /// code this build does not know). A session it set up is not touched.
    void introductionEnded(const QByteArray& introductionId, const QString& code);
    /// Client: the Core answered, with relay credentials when
    /// `relayOffered`.
    void answerReceived(const QString& sdp, bool relayOffered,
                        const NereusSDR::RendezvousWire::Turn& turn);
    void nameplateClaimed(int nameplate);
    void nameplateReleased();
    void mailboxOpened(int nameplate);
    void mailboxReceived(const QString& body);
    void mailboxClosed(const QString& code);
    /// An error the service sent (section 7); `reason` is its plain words.
    void serviceError(const QString& code, const QString& reason, qint64 retryAfterMs);
    /// Client: no server answered, or every one said the Core or nameplate
    /// is not there. `reason` is plain words.
    void unreachable(const QString& reason);

private:
    enum class Pending { None, Introduce, OpenMailbox };

    void connectTo(int serverIndex);
    void tryNextServer();
    void scheduleReconnect();
    void resetConnection();
    void onConnected();
    void onDisconnected();
    void onText(const QString& text);
    void onHelloTimeout();
    void onPingTick();
    void handle(const RendezvousWire::Message& message);
    void handleIntroduction(const RendezvousWire::Message& message);
    void handleError(const RendezvousWire::Message& message);
    void sendPending();
    bool send(const RendezvousWire::Message& message);
    RendezvousWire::Direction outgoing() const;
    RendezvousWire::Direction incoming() const;

    Role m_role = Role::None;
    QList<QUrl> m_servers;
    int m_serverIndex = -1;
    /// Moves with every connection, so a stale socket's signal is ignored.
    quint64 m_generation = 0;
    QPointer<QWebSocket> m_socket;
    QTimer* m_helloTimer = nullptr;
    QTimer* m_reconnectTimer = nullptr;
    QTimer* m_pingTimer = nullptr;
    QList<int> m_reconnectDelaysMs;
    int m_reconnectAttempt = 0;
    int m_helloTimeoutMs = kHelloTimeoutMs;
    /// The last connection ended with `replaced`: the next registration
    /// keeps the backoff where it is.
    bool m_replaced = false;
    int m_missedPongs = 0;
    bool m_stopped = false;
    bool m_helloReceived = false;
    bool m_watchRelayEnabled = false;
    bool m_watchRelayNegotiated = false;
    QByteArray m_helloNonce;
    QStringList m_stunUrls;

    // Station.
    QByteArray m_stationKey;
    QString m_stationId;
    Signer m_stationSign;
    DeviceLookup m_devices;
    bool m_relayAllowed = true;
    bool m_registered = false;
    bool m_wantNameplate = false;
    quint64 m_droppedIntroductions = 0;
    /// Introductions reported and not yet ended, and those answered, with
    /// how many candidates each has had.
    QSet<QByteArray> m_liveIntroductions;
    QHash<QByteArray, int> m_answered;
    /// Task 29 fix wave: each answered introduction's relay grant.
    QHash<QByteArray, RendezvousWire::RelayGrant> m_relayGrants;
    /// Task 28: introductions retired here, newest last, at most
    /// kMaxLiveIntroductions * 4, so the service cannot hand one back.
    QList<QByteArray> m_retiredIntroductions;

    // Client.
    /// Task 29 fix wave: the live introduction's relay grant.
    std::optional<RendezvousWire::RelayGrant> m_clientRelayGrant;
    Pending m_pending = Pending::None;
    QString m_targetId;
    QByteArray m_deviceKey;
    Signer m_deviceSign;
    QString m_offer;
    int m_mailboxNameplate = 0;
    bool m_introductionLive = false;
    /// Why the last server said no, for unreachable().
    QString m_lastRefusal;
    // Step 2b: a sign-in page or an inspecting network, in plain words.
    QString m_networkTrouble;

    bool m_mailboxOpen = false;
};

} // namespace NereusSDR

Q_DECLARE_METATYPE(NereusSDR::RendezvousIntroduction)
