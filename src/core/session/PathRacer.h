#pragma once
// =================================================================
// src/core/session/PathRacer.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original.
//
// iPhone app plan Task 29 (R-IOS-16; the pairing design, section 5.4,
// "Attempt direct paths and both relay transports concurrently and take the
// first that succeeds, then keep upgrading in the background"; the link
// document, section 21.1): the race a device runs to reach a paired Core.
//
// Every path is a rung (Rung): the Core's WebSocket at one address
// (DirectPathRung), or the rendezvous introduction with its ICE
// (RendezvousPathRung); the floor, when it is chosen, is one more rung of
// rank 4 (link section 21.6), or a candidate source inside the rendezvous
// rung's ICE (IceConfiguration::CandidateSource). The racer starts every
// rung at once, an IPv4 address kIpv4DelayMs after the IPv6 ones, and
// reads the first message each opened connection brings: the Core's
// hello. A rung is ready when the vetter says that hello proves the Core
// (for a paired Core: its identity key and the certificate binding for
// that connection's certificate, StationClient::helloProvesPairedCore).
// A hello that does not ends the rung as "another computer answered".
//
//   - The first ready rung wins (won()): its connection and the hello it
//     brought go to the session, which signs in on it.
//   - A rung ready after the winner with a better (lower) rank is kept as
//     a standby (better()), for the session to move to (link section 21.2)
//     once it has reached snapshot.complete; any other ready rung closes.
//   - finish() stops what is still running and closes what was not taken.
//   - With every rung ended and no winner, failed() carries plain words.
//
// The racer keeps one line per rung for the connection's attempt record
// (lines()), which StationClient turns into StationConnectionAttempt.
//
// No rung signs in and nothing here sends a message: a race never signs in
// twice, so the same-device rule never ends the winner.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-27: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-27: iPhone app plan Task 29 step 2b (R-IOS-16, R-IOS-08): the
//               web relay's leg (RelayLeg) and its per-connection candidate
//               sources; the computer's own proxy settings (SystemProxy).
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/session/SessionMessages.h"

#include <QByteArray>
#include <QList>
#include <QSet>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QUrl>

#include <array>
#include <functional>
#include <memory>
#include <optional>

#include "core/session/media/IMediaTransport.h"

QT_BEGIN_NAMESPACE
class QTimer;
class QWebSocket;
QT_END_NAMESPACE

namespace NereusSDR {

class ClientDeviceIdentity;
class RendezvousDialer;
class SessionTransport;

/// How a rung ended, for the attempt record (PathRacer::Outcome).
enum class PathOutcome {
    Trying,
    Ready,        ///< the Core's hello proved it (the winner, a standby)
    NoAnswer,     ///< nothing answered
    TimedOut,     ///< it did not answer in time
    NotThisCore,  ///< another computer answered
    Failed,       ///< it answered, and did not connect
    Stopped,      ///< another path connected first
    RelayOff,     ///< the Core has the relay turned off
    CoreTooOld,   ///< the Core does not answer through the internet service
    WebRelayEnded, ///< step 2b: the web relay ended the leg (the line's reason says why)
};

/// Which kind of path a rung is, for the attempt record (PathRacer::PathKind).
enum class PathKind { ThisNetwork, Direct, Service, Relay, WebRelay };

/// One way to reach the Core (PathRacer::Rung). A rung makes one connection
/// at most and reports it with opened() (handing the transport over,
/// unparented) or ends with ended(). stop() cancels it; it emits nothing
/// after.
class PathRung : public QObject {
    Q_OBJECT
public:
    explicit PathRung(QObject* parent = nullptr) : QObject(parent) {}
    ~PathRung() override = default;
    virtual void start() = 0;
    virtual void stop() = 0;
    /// Its rank (PathRacer::Rank); a rendezvous rung knows whether it is
    /// relayed only once its connection opened.
    virtual int rank() const = 0;
    virtual PathKind kind() const = 0;
    /// host:port, or the service's host, as the operator would read it.
    virtual QString address() const = 0;
signals:
    void opened(NereusSDR::SessionTransport* transport);
    void ended(NereusSDR::PathOutcome outcome, const QString& reason);
    /// Task 29 fix wave (review Minor 6), step 2b: something the attempt
    /// record should say as its own line: the Core answered with the relay
    /// turned off (kind Relay), or the web relay ended the leg (kind
    /// WebRelay). Each kind and outcome is recorded once.
    void noted(NereusSDR::PathKind kind, NereusSDR::PathOutcome outcome, const QString& reason);
};

class PathRacer : public QObject {
    Q_OBJECT

public:
    using Outcome = PathOutcome;
    using PathKind = NereusSDR::PathKind;
    using Rung = PathRung;

    /// Link section 21.1: an IPv4 address starts this long after the IPv6
    /// ones, when there is an IPv6 address to try (RFC 8305's connection
    /// attempt delay).
    static constexpr int kIpv4DelayMs = 250;
    /// Task 29 fix wave (review Minor 8): direct connections this computer
    /// has opening at once, all from one address as the Core sees it: the
    /// Core takes StationServer::kMaxHandshakesPerAddress (2) at a time,
    /// each until it is signed in, and refuses the rest, so the others wait
    /// their turn. A rung holds its turn until it ends, is let go, or
    /// finish() (the winner signed in).
    static constexpr int kMaxDirectOpening = 2;
    /// How long an opened connection may take to bring the Core's hello
    /// before its rung ends: the Core's connect deadline (link section
    /// 12.2).
    static constexpr int kHelloDeadlineMs = kStationHandshakeDeadlineMs;
    /// Link section 21.3: when a session on a rung worse than rank 0 looks
    /// for a better one, after it reached snapshot.complete; the last is
    /// repeated.
    static constexpr std::array<int, 4> kUpgradeRetryMs{5000, 30000, 120000, 300000};

    /// Link section 21.1's ranks; lower is better.
    enum Rank : int {
        ThisNetwork = 0,    ///< the Core's WebSocket on this device's own network
        Direct = 1,         ///< the Core's WebSocket at any other address
        ServiceDirect = 2,  ///< the rendezvous, a path without the relay
        ServiceRelayed = 3, ///< the rendezvous, through the relay
        Floor = 4,          ///< the relay over TCP 443 (reserved)
    };

    /// The Core's hello on `transport` proves it is the Core this device
    /// expects.
    using Vetter = std::function<bool(const SessionMessage& hello, SessionTransport* transport)>;

    /// A ready rung's connection: unparented, the caller's from here.
    struct Ready {
        QPointer<SessionTransport> transport;
        /// The Core's hello, as it arrived: the session reads it first.
        QByteArray hello;
        int rank = Floor;
        PathKind kind = PathKind::Direct;
        QString address;
        /// The address dialled, for a direct rung; empty through the
        /// service.
        QUrl url;
    };

    /// One line of the attempt record.
    struct Line {
        PathKind kind = PathKind::Direct;
        QString address;
        Outcome outcome = Outcome::Trying;
        /// The rung's own words when it ended (plain; for the window).
        QString reason;
    };

    explicit PathRacer(QObject* parent = nullptr);
    ~PathRacer() override;

    PathRacer(const PathRacer&) = delete;
    PathRacer& operator=(const PathRacer&) = delete;

    void setVetter(Vetter vetter) { m_vetter = std::move(vetter); }
    /// Adds a rung (taken, reparented) that starts `startDelayMs` after
    /// start(). Before start() only.
    void addRung(Rung* rung, int startDelayMs = 0);
    /// A line in the record for a rung that was not started (the relay the
    /// Core turned off, a Core too old for the service), with its words.
    void addNote(PathKind kind, const QString& address, Outcome outcome, const QString& reason);
    /// Direct rungs for `urls`, in order: a literal address at once (an
    /// IPv4 one kIpv4DelayMs later when the list has an IPv6 address), a
    /// host name resolved first and each of its addresses the same way.
    /// Before start() only; the lookups run from start().
    void addDirectUrls(const QList<QUrl>& urls, quint64 maxIncomingBytes);
    /// Only rungs better than `rank` count (an upgrade): a rung ready at a
    /// rank no better closes. Default: every rank counts.
    /// Before addDirectUrls(), so an address whose rank cannot beat it is
    /// never dialled (review Minor 8).
    void setBetterThan(int rank) { m_betterThan = rank; }

    void start();
    /// Stops everything, closes what was not taken; nothing more is
    /// signalled.
    void cancel();
    /// The winner reached snapshot.complete: every rung still running
    /// stops (as "another path connected first") and every ready one not
    /// taken closes, except the standby (takeStandby()).
    void finish();
    /// The best standby better than the winner, if one is ready. The caller
    /// takes it.
    std::optional<Ready> takeStandby();

    bool running() const { return m_started && !m_done; }
    bool hasWinner() const { return m_winnerRank.has_value(); }
    QList<Line> lines() const { return m_lines; }

    /// The rank a connection to `url` has (ThisNetwork or Direct).
    static int rankFor(const QUrl& url);
    /// Step 2b: the rank of a connection through the service from the pair
    /// it settled on: Floor through the web relay's loopback shim,
    /// ServiceRelayed through TURN, ServiceDirect otherwise (and while no
    /// pair is known).
    static int rankForPath(const std::optional<MediaIcePath>& path);

    /// Test seam: each rung's address and how long after start() it
    /// starts (names resolved so far included).
    QList<QPair<QString, int>> plannedStartsForTest() const;
    /// Review Minor 8: direct rungs opening now, and waiting their turn.
    int directOpeningForTest() const { return m_directOpening; }
    int directWaitingForTest() const { return static_cast<int>(m_directWaiting.size()); }

signals:
    /// The first ready rung.
    void won(const NereusSDR::PathRacer::Ready& ready);
    /// A rung better than the winner is ready (takeStandby() gives it).
    void better();
    /// Every rung ended with no winner; `reason` is plain words.
    void failed(const QString& reason);
    /// A line of the record changed.
    void linesChanged();

private:
    struct Entry;

    void startRung(Entry& entry);
    void onOpened(int index, SessionTransport* transport);
    void onHello(int index, const QByteArray& wire);
    void endRung(int index, Outcome outcome, const QString& reason);
    void checkAllEnded();
    void resolveAndAdd(const QUrl& url, quint64 maxIncomingBytes, bool ipv6Present);
    int addEntry(Rung* rung, int startDelayMs);
    void releaseTransport(Entry& entry, bool close);

    Vetter m_vetter;
    std::vector<std::unique_ptr<Entry>> m_entries;
    QList<Line> m_lines;
    QList<QUrl> m_pendingLookups;
    quint64 m_lookupMaxIncoming = 0;
    int m_lookupsRunning = 0;
    std::optional<int> m_winnerRank;
    std::optional<int> m_betterThan;
    /// Review Minor 8: every direct target (address and port) raced, so a
    /// name that resolves to an address already listed is not dialled
    /// twice; the direct rungs opening now and those waiting their turn.
    QSet<QString> m_directTargets;
    int m_directOpening = 0;
    QList<int> m_directWaiting;
    bool admitDirect(const QUrl& url);
    void startWaitingDirect();
    void releaseDirectTurn(Entry& entry);
    bool m_started = false;
    bool m_done = false;
    bool m_finished = false;
};

/// The Core's WebSocket at one address (link section 21.1, ranks 0 and 1).
/// TLS errors are left to the hello: a paired Core is proved by its
/// identity key and certificate binding, not by the certificate chain; an
/// expired or not yet valid certificate still fails, as on every other
/// path (StationClient::dialStation()).
class DirectPathRung : public PathRung {
    Q_OBJECT
public:
    DirectPathRung(const QUrl& url, quint64 maxIncomingBytes, QObject* parent = nullptr);
    ~DirectPathRung() override;
    void start() override;
    void stop() override;
    int rank() const override { return m_rank; }
    PathRacer::PathKind kind() const override;
    QString address() const override;
    QUrl url() const { return m_url; }

private:
    QUrl m_url;
    quint64 m_maxIncomingBytes = 0;
    int m_rank = PathRacer::Direct;
    QPointer<SessionTransport> m_transport;
    bool m_done = false;
};

/// The rendezvous introduction (link section 21.1, ranks 2 and 3): a
/// RendezvousDialer. `allowRelay` false (an upgrade, or a Core that has the
/// relay turned off) gathers no relay candidate and takes none of the
/// Core's.
class RendezvousPathRung : public PathRung {
    Q_OBJECT
public:
    RendezvousPathRung(const QList<QUrl>& servers, const QString& stationId,
                       std::shared_ptr<const ClientDeviceIdentity> device, bool allowRelay,
                       QObject* parent = nullptr);
    ~RendezvousPathRung() override;
    void start() override;
    void stop() override;
    int rank() const override { return m_rank; }
    PathRacer::PathKind kind() const override;
    QString address() const override;
    /// Test seam: the dialer's bounds.
    void setDialDeadlineMs(int ms) { m_dialDeadlineMs = ms; }
    void setAnswerDeadlineMs(int ms) { m_answerDeadlineMs = ms; }
    /// RendezvousDialer::setCoreAnswersIntroductions (review Minor 5).
    void setCoreAnswersIntroductions(bool answers) { m_coreAnswersIntroductions = answers; }

private:
    void noteRelay(const RendezvousDialer* dialer);
    QList<QUrl> m_servers;
    QString m_stationId;
    std::shared_ptr<const ClientDeviceIdentity> m_device;
    bool m_allowRelay = true;
    int m_rank = PathRacer::ServiceDirect;
    int m_dialDeadlineMs = 0;
    int m_answerDeadlineMs = 0;
    bool m_coreAnswersIntroductions = false;
    QPointer<RendezvousDialer> m_dialer;
    bool m_done = false;
};

} // namespace NereusSDR

Q_DECLARE_METATYPE(NereusSDR::PathOutcome)
Q_DECLARE_METATYPE(NereusSDR::PathRacer::Ready)
