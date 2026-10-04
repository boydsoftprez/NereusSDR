#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/StationPairingClient.h  (NereusSDR)
// =================================================================
//
// The desktop's side of pairing with a Core (iPhone app plan Task 18,
// R-IOS-08; the pairing design, docs/architecture/2026-08-02-remote-
// station-identity-and-pairing-design.md sections 4 and 11; the wire is
// the link document's section 3.6). It mirrors the iPhone app's
// PairingClient (plan Task 15): one tap on the Core's own network, or the
// code the Core shows, each ending in the Core's identity and label.
//
// One pairing runs on a connection of its own, to the Core's address
// directly, or (plan Task 27) through the remote access service's pairing
// mailbox, which carries the pair.* messages below and nothing else:
//
//   - the Core's hello, which must declare `features.pairing` 1;
//   - this computer's hello (declaring deviceAuth 1) and `pair.start`
//     with its device key, its name and kind `computer`;
//   - one tap: the Core's `pair.accept` carries its identity and label;
//   - the code: SPAKE2+EE steps 0 to 3 (SpakeExchange), then the two
//     confirmation boxes; this computer's box carries its key, name and
//     kind, the Core's carries its identity and label. The code's hash
//     (Argon2id) runs on a worker thread, so the window keeps drawing;
//   - either way the Core's certificate binding must verify for the
//     certificate this connection presented, so the identity saved is the
//     one that certificate belongs to. The Core then ends the connection,
//     and this computer signs in by key on a new one (StationClient).
//
// A code is checked against the word list before anything is sent
// (PairingCode::normalise), so a typing mistake never burns the Core's
// code. The code never reaches a log line, and it is wiped once used.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-26: iPhone app plan Task 27 (R-IOS-08): pairing by code
//               through the remote access service's mailbox. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-26: Task 28 fix wave (privacy): the mailbox's plain
//               pair.start names this computer only kMailboxPlainName; its
//               own name travels sealed. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include "core/NereusCoreExport.h"
#include <QByteArray>
#include <QList>
#include <QMetaType>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QUrl>

#include <functional>
#include <memory>

QT_BEGIN_NAMESPACE
class QThread;
class QTimer;
QT_END_NAMESPACE

namespace NereusSDR {

class ClientDeviceIdentity;
class RendezvousClient;
class SessionTransport;
class SpakeExchange;
struct SessionMessage;

/// A Core this computer has paired with, mirroring the iPhone app's
/// PairedStation. The Connections window turns it into a saved Core.
struct PairedStationRecord {
    /// The Core's identity key, SubjectPublicKeyInfo DER (91 bytes).
    QByteArray identityKey;
    /// SHA-256 of identityKey, 32 bytes: what the saved Core keeps.
    QByteArray identityFingerprint;
    /// The Core's label (StationLabel), as the Core sent it.
    QString label;
    /// The address this computer paired over.
    QString host;
    quint16 port = 0;
};

class NEREUS_CORE_EXPORT StationPairingClient : public QObject {
    Q_OBJECT

public:
    /// A Core's remote listener when an address names no port
    /// (DaemonConfig::kDefaultRemotePort).
    static constexpr quint16 kDefaultPort = 47910;
    /// How long one pairing may take, the code's hash included.
    static constexpr int kDefaultDeadlineMs = 60000;
    /// Largest message accepted on a pairing connection. Every pairing
    /// message is a few hundred bytes; a Core also sends nothing else here.
    static constexpr quint64 kMaxIncomingMessageBytes = 64 * 1024;

    /// Opens a connection to `url`, owned by the caller from then on
    /// (reparented onto this object). A test hands back an in-process link.
    using TransportFactory = std::function<SessionTransport*(const QUrl& url)>;

    /// `identity` is this computer's device key; `deviceName` is the name
    /// the Core lists it by.
    StationPairingClient(std::shared_ptr<const ClientDeviceIdentity> identity,
                         QString deviceName, QObject* parent = nullptr);
    ~StationPairingClient() override;

    StationPairingClient(const StationPairingClient&) = delete;
    StationPairingClient& operator=(const StationPairingClient&) = delete;

    /// One tap: pairs with an unclaimed Core on this network at
    /// `host`:`port` (an IPv4 address, an IPv6 literal or a name). Ends in
    /// paired() or failed().
    void pairOnThisNetwork(const QString& host, quint16 port);
    /// Pairs with the Core at `host`:`port` using the code it shows (for
    /// example "7-anvil-harbor"). A code that is not a number and two words
    /// of the list fails at once and is never sent.
    void pairByCode(const QString& code, const QString& host, quint16 port);

    /// iPhone app plan Task 27 (R-IOS-08): pairs by the code over a
    /// pairing mailbox of the remote access service that is already open
    /// (`mailbox`, a RendezvousMailboxTransport, owned from then on). No
    /// hello travels through a mailbox, so pair.start goes at once; and a
    /// mailbox has no certificate, so the Core's certificate binding is
    /// checked at this computer's first sign-in (StationClient) instead.
    void pairByCodeOverMailbox(const QString& code, SessionTransport* mailbox);
    /// The name the mailbox's plain pair.start gives for this computer.
    /// The service never learns a device's name (the rendezvous document,
    /// section 1), and a name is often an operator's callsign, so only
    /// this neutral one travels in the clear; the computer's own name goes
    /// in the sealed confirmation box, which is the one the Core records
    /// (link document, section 3.6 step 5). The phone sends "iPhone".
    static constexpr const char* kMailboxPlainName = "Computer";
    /// Opens the mailbox on the code's number through the remote access
    /// service (`servers`, tried in order) and pairs over it.
    void pairByCodeFromAnywhere(const QString& code, const QList<QUrl>& servers);

    /// Stops a pairing in progress; no signal follows.
    void cancel();
    bool isPairing() const { return m_state != State::Idle; }

    /// wss://host:port, an IPv6 literal in brackets.
    static QUrl coreUrl(const QString& host, quint16 port);
    /// Reads a typed address: a name, an IPv4 address, an IPv6 literal
    /// (bare, or in brackets with an optional port), each with an optional
    /// ":port", or a ws:// or wss:// address. kDefaultPort when none is
    /// given. False when it is not an address.
    static bool parseAddress(const QString& text, QString* host, quint16* port);

    /// Test seams.
    void setTransportFactory(TransportFactory factory);
    void setDeadlineMs(int ms);

signals:
    /// The Core paired this computer and gave its identity and label.
    void paired(const NereusSDR::PairedStationRecord& record);
    /// Pairing did not happen. `reason` is plain words: the Core's own
    /// (from pair.fail) or this computer's.
    void failed(const QString& reason);

private:
    enum class State {
        Idle,
        OpeningMailbox,  // Task 27: waiting for the service to open the mailbox
        AwaitHello,
        AwaitAccept,     // one tap: pair.accept or pair.fail
        AwaitStep0,
        Hashing,         // step 1 is being computed on the worker
        AwaitStep2,
        AwaitConfirm,    // the Core's box
        AwaitCoreFail,   // this computer sent pair.fail; the Core answers with its own
    };

    void begin(bool lan, const QString& normalisedCode, const QString& host, quint16 port);
    /// Task 27: the code's exchange over an open mailbox.
    void startMailbox(const QString& normalisedCode, SessionTransport* mailbox);
    /// The checks before any pairing by code; false after failed().
    bool readyForCode();
    void onText(const QByteArray& wire);
    void onClosed();
    void handleHello(const SessionMessage& message);
    void handleStep0(const SessionMessage& message);
    void finishStep1(quint64 attempt, const QByteArray& response1);
    void handleStep2(const SessionMessage& message);
    void handleConfirm(const SessionMessage& message);
    /// Checks the Core's identity key and binding against this connection's
    /// certificate, then reports paired().
    void complete(const QString& publicKey, const QString& certBinding, const QString& label);
    void fail(const QString& reason);
    void send(const SessionMessage& message);
    void stopTransport();
    void wipeCode();

    std::shared_ptr<const ClientDeviceIdentity> m_identity;
    QString m_deviceName;
    TransportFactory m_factory;
    int m_deadlineMs = kDefaultDeadlineMs;

    State m_state = State::Idle;
    bool m_lan = false;
    QString m_code;
    QString m_host;
    quint16 m_port = 0;
    QByteArray m_helloIdentityKey;
    QPointer<SessionTransport> m_transport;
    /// Task 27: this pairing runs through a mailbox, and the connection to
    /// the remote access service that holds it (pairByCodeFromAnywhere()).
    bool m_mailbox = false;
    QPointer<RendezvousClient> m_rendezvous;
    std::shared_ptr<SpakeExchange> m_exchange;
    QTimer* m_deadlineTimer = nullptr;
    /// Bumped by every begin() and cancel(); a worker's result for an
    /// older attempt is dropped.
    quint64 m_attempt = 0;
    QPointer<QThread> m_worker;
};

} // namespace NereusSDR

Q_DECLARE_METATYPE(NereusSDR::PairedStationRecord)
