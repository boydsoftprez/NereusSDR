#pragma once
#include "core/session/NetworkPathSnapshot.h"
// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/IceConfiguration.h  (NereusSDR)
// =================================================================
//
// The ICE settings of a peer connection that came through the rendezvous
// (iPhone app plan Task 27, R-IOS-16; the rendezvous document,
// docs/architecture/2026-09-23-rendezvous-v1.md, sections 6.1, 6.3 and 8).
// A peer that did not come through the rendezvous has none, and gathers
// host candidates only, as before.
//
// Built for the pinned libdatachannel v0.24.5 over libjuice (@3c40a354),
// whose limits decide its shape:
//
//   - One STUN server. libdatachannel picks one STUN server from its
//     configuration at random (src/impl/icetransport.cpp:101-113), so this
//     carries exactly one.
//   - Relay servers: libdatachannel takes at most two (icetransport.cpp:39,
//     MAX_TURN_SERVERS_COUNT; libjuice agent.h:65, MAX_RELAY_ENTRIES_COUNT),
//     and libjuice resolves one address per TURN host, preferring IPv4
//     (agent.c:386-395), so each address family needs a name of its own:
//     the service lists an IPv6-only and an IPv4-only relay name (section
//     8). The relay is sized by allocations, four for each Core's id with
//     both ends of a session sharing them, so one end relays on one family
//     unless it asks for both (setRelay()'s `families`). Only UDP relays:
//     libjuice speaks TURN over UDP alone (icetransport.cpp:159-162).
//   - Which one (fix wave I2): the STUN server, and the one relay host,
//     are chosen by the address families this end has, never by the order
//     the service lists them in. The first entry whose name resolves (on
//     this end, so a DNS64 answer counts) to a family this end has a usable
//     address in; the service's first entry when this end has both
//     families, or when it cannot tell (no usable address seen, or no name
//     resolved). An IPv4-only end behind NAT so gets the IPv4-only name
//     whichever the service lists first, and an IPv6-only end the IPv6-only
//     one. The names are resolved before the choice (resolveHostFamilies());
//     an IP literal needs no lookup. An end with one family whose relay
//     names did not resolve gets both relay hosts rather than the first
//     (the follow-up to the Task 27 re-review, new Minor 1): libjuice fails
//     the unreachable family's allocation without taking a slot, so it
//     still relays on its own family, once.
//   - A full relay (TURN 486, Allocation Quota Reached) is an ordinary
//     outcome, not an error: libjuice marks that relay failed and finishes
//     gathering without it (agent.c:1941-1949), and the connection goes on
//     with the paths it has.
//   - Credentials are fixed when gathering starts: the peer is built with
//     automatic gathering off, and gathering starts once the credentials
//     are known (or known to be absent), with the TURN servers passed to
//     gatherLocalCandidates() (peerconnection.cpp:170-180).
//   - An MTU of 996 bytes: TURN's ChannelData header (4 bytes) then keeps
//     every relayed datagram at the 1000 bytes the pairing design caps
//     media at (section 9.3).
//   - Deadlines: libjuice gives up on a STUN or TURN server after 23.5 s
//     (agent.h:29, MAX_STUN_SERVER_RETRANSMISSION_COUNT) and on the
//     connectivity checks after 39.5 s (agent.h:43, ICE_PAC_TIMEOUT), so a
//     deadline for the whole connection covers both, one after the other.
//
// `relay = deny` in nereusd.conf keeps the Core off the relay: it asks for
// no credentials and refuses the far end's relay candidates, so its
// connections are direct or nothing (the pairing design, section 5.4).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-26: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-26: fix wave I2, the STUN server and relay host chosen by this
//               end's address families. J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
//   2026-09-26: Task 27 follow-up (new Minor 1): no default families, and
//               both relay hosts for a one-family end that cannot tell.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-27: iPhone app plan Task 29 (R-IOS-16): CandidateSource, the
//               seam where another source of the far end's candidates
//               joins one connection's ICE (link section 21.5). J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-27: Task 29 step 2b (R-IOS-16): the candidate source factory,
//               one source per connection and lane, gated on the relay.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29: the direct media ladder: setStunServer, so a media
//               connection's own settings carry the Core's STUN server.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29: direct media fix wave: setOnlySourceCandidates, for the
//               window's fallback onto the tunnel alone. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/session/RendezvousWire.h"

#include <QHash>
#include <QHostAddress>
#include <QList>
#include <QString>
#include <QStringList>

#include <functional>
#include <memory>
#include <optional>

QT_BEGIN_NAMESPACE
class QObject;
QT_END_NAMESPACE

namespace NereusSDR {

/// A STUN server, or a TURN server's address.
struct IceServerAddress {
    QString host;
    quint16 port = 0;

    bool operator==(const IceServerAddress&) const = default;
};

/// Which address families something has: this end's usable addresses, or
/// what a server's name resolved to. Neither set: cannot tell.
struct AddressFamilies {
    bool ipv4 = false;
    bool ipv6 = false;

    bool known() const { return ipv4 || ipv6; }
    bool both() const { return ipv4 && ipv6; }
    bool shares(const AddressFamilies& other) const
    {
        return (ipv4 && other.ipv4) || (ipv6 && other.ipv6);
    }
    bool operator==(const AddressFamilies&) const = default;
};

/// The families of the server names a list uses, as this end resolved
/// them, keyed by the name in lower case. A name missing here cannot be
/// told.
using HostFamilies = QHash<QString, AddressFamilies>;

/// A TURN server with the credentials the rendezvous minted (UDP only).
struct IceRelayServer {
    QString host;
    quint16 port = 0;
    QString username;
    QString password;

    bool operator==(const IceRelayServer&) const = default;
};

class IceConfiguration {
public:
    /// libdatachannel v0.24.5 src/impl/icetransport.cpp:39 and libjuice
    /// @3c40a354 src/agent.h:65.
    static constexpr int kMaxRelayServers = 2;
    /// 1000 bytes on the wire less TURN's 4-byte ChannelData header.
    static constexpr int kMtuBytes = 996;
    /// libjuice @3c40a354 src/agent.h:29 (a STUN or TURN server's last
    /// retransmission, 23.5 s in all) and src/agent.h:43 (ICE_PAC_TIMEOUT).
    static constexpr int kGatheringDeadlineMs = 23500;
    static constexpr int kConnectivityTimeoutMs = 39500;
    /// Gathering, then the connectivity checks: how long an ICE connection
    /// through the rendezvous may take before its failure is certain.
    static constexpr int kConnectDeadlineMs = kGatheringDeadlineMs + kConnectivityTimeoutMs;
    /// The port a STUN or TURN URL means when it names none (RFC 7064, RFC
    /// 7065).
    static constexpr quint16 kDefaultPort = 3478;
    /// How long resolveHostFamilies() waits for the names before it
    /// reports what it has (a name not back by then cannot be told, and the
    /// service's first entry is kept). NereusSDR's own bound: a fraction of
    /// kGatheringDeadlineMs, so a slow resolver costs little of the connect.
    static constexpr int kHostLookupTimeoutMs = 3000;

    /// A connection through the rendezvous: `stunUrls` as the service's
    /// hello listed them; `relayAllowed` the station's `relay` setting (a
    /// client allows it); `local` this end's usable address families
    /// (localAddressFamilies()); `hosts` the families of the names in the
    /// service's lists, resolved on this end. No defaults: every caller
    /// states what it knows, an empty AddressFamilies or HostFamilies when
    /// it knows nothing (the follow-up to the Task 27 re-review, new Minor
    /// 1). No relay servers until setRelay().
    static IceConfiguration throughRendezvous(const QStringList& stunUrls, bool relayAllowed,
                                              const AddressFamilies& local,
                                              const HostFamilies& hosts);

    /// Adds resolved names (the relay's, which arrive after the hello) for
    /// setRelay() to choose by.
    void addHostFamilies(const HostFamilies& hosts);

    /// The relay credentials the introduction brought (the service's
    /// `turn`; nullopt when it sent null). Ignored when the relay is not
    /// allowed. `families` is how many address families to relay on, with
    /// no default: 1, the host chosen by this end's address families
    /// (above), or 2, the first two the service lists (one for each
    /// address family, only where an end needs both); each allocation
    /// takes one of the relay's slots for this Core. With 1, an end that
    /// has one family and knows no relay name's family gets the first two
    /// hosts, since libjuice fails the other family's allocation without
    /// taking a slot. Returns how many relay servers the configuration now
    /// holds.
    int setRelay(const std::optional<RendezvousWire::Turn>& turn, int families);

    AddressFamilies localFamilies() const { return m_local; }

    /// This computer's usable address families: an IPv4 address that is
    /// not loopback or link-local (a private one behind NAT counts), an
    /// IPv6 address that is global unicast (2000::/3), on an interface that
    /// is up and not loopback.
    static AddressFamilies localAddressFamilies();
    static bool isUsableLocalAddress(const QHostAddress& address);
    /// The family of an IP literal; nothing for a name.
    static AddressFamilies literalFamilies(const QString& host);
    static AddressFamilies familiesOf(const QList<QHostAddress>& addresses);
    /// The names (not IP literals) the STUN and TURN URLs in `urls` use,
    /// each once, in lower case.
    static QStringList hostNames(const QStringList& urls);
    /// Resolves `names` and calls `done` once, on `context`'s thread, with
    /// the families each resolved to, within kHostLookupTimeoutMs (or
    /// `timeoutMs`). A name that did not resolve in time is left out. A
    /// test run (QStandardPaths test mode) resolves only this computer's
    /// own names, as RendezvousClient reaches only a service on it.
    static void resolveHostFamilies(const QStringList& names, QObject* context,
                                    std::function<void(const HostFamilies&)> done,
                                    int timeoutMs = kHostLookupTimeoutMs);

    /// These settings with no relay server of this end's own: the same
    /// STUN server, the far end's relay candidates still accepted where the
    /// relay is allowed. For media whose control connection found a path
    /// without the relay (the Task 28 safety review's Important 4).
    IceConfiguration withoutOwnRelay() const
    {
        IceConfiguration copy = *this;
        copy.m_relays.clear();
        return copy;
    }

    /// iPhone app plan Task 29 (R-IOS-16; link section 21.5): another
    /// source of the far end's candidates, which the ICE agent races with
    /// every other inside one connection at the priority each candidate
    /// line carries: the web relay's leg (RelayLeg), a loopback address
    /// given as a low-priority remote candidate. A source starts when its
    /// connection gathers and hands each candidate (`candidate:...`, as the
    /// far end's) to `add` on the connection's thread; stop() ends it and
    /// it calls `add` no more.
    class CandidateSource {
    public:
        virtual ~CandidateSource() = default;
        virtual void start(std::function<void(const QString& candidate)> add) = 0;
        virtual void stop() = 0;
        virtual std::optional<NetworkPathSnapshot> networkPathSnapshot() const
        {
            return std::nullopt;
        }
    };
    /// The lanes (the rendezvous document, section 12.3's tags): the
    /// session's control connection, and its media connection.
    static constexpr int kControlLane = 1;
    static constexpr int kMediaLane = 2;
    /// Step 2b (the step 2a review's Minor 10): each connection makes its
    /// own source from the factory, for its lane, so no source is ever
    /// shared by two connections. The factory travels with these settings
    /// (the session's media settings are a copy of its control's), and
    /// `needsRelay` gates it on relayAllowed(): `relay = deny` stops the
    /// web relay as it stops TURN.
    using CandidateSourceFactory = std::function<std::shared_ptr<CandidateSource>(int lane,
                                                                                  const QString& connectionId,
                                                                                  bool routed)>;
    void setCandidateSourceFactory(CandidateSourceFactory factory, bool needsRelay)
    {
        m_sourceFactory = std::move(factory);
        m_sourceNeedsRelay = needsRelay;
    }
    bool hasCandidateSourceFactory() const { return static_cast<bool>(m_sourceFactory); }
    /// A new source for one connection on `lane`; null when there is no
    /// factory or it needs the relay and the relay is not allowed.
    std::shared_ptr<CandidateSource> makeCandidateSource(int lane,
                                                         const QString& connectionId = {}) const
    {
        if (!m_sourceFactory || (m_sourceNeedsRelay && !m_relayAllowed)) {
            return nullptr;
        }
        return m_sourceFactory(lane, connectionId, m_mediaRouting);
    }
    void setMediaRouting(bool routed) { m_mediaRouting = routed; }
    bool mediaRouting() const { return m_mediaRouting; }
    /// The direct media ladder's fallback: a connection that uses only its
    /// own candidate source (the tunnel). It takes none of the far end's
    /// signalled candidates and sends none of its own, so no direct pair can
    /// form and the far end never learns this computer's addresses.
    void setOnlySourceCandidates(bool only) { m_onlySourceCandidates = only; }
    bool onlySourceCandidates() const { return m_onlySourceCandidates; }

    std::optional<IceServerAddress> stunServer() const { return m_stun; }
    /// The direct media ladder: the STUN server a media connection gathers
    /// its server-reflexive candidate from (none: host candidates only).
    void setStunServer(std::optional<IceServerAddress> stun) { m_stun = std::move(stun); }
    QList<IceRelayServer> relayServers() const { return m_relays; }
    bool relayAllowed() const { return m_relayAllowed; }
    /// When the relay credentials arrived: gathering may start.
    bool relayKnown() const { return m_relayKnown; }

    /// Whether a candidate the far end sent may be used: a relay candidate
    /// (`typ relay`) only when the relay is allowed.
    bool acceptsRemoteCandidate(const QString& candidate) const;
    /// Test seam (step 2b): while set, every connection through the service
    /// takes only a loopback-shim candidate (the web relay's), so a test on
    /// one computer, where host pairs always work, runs over the web relay.
    static void setOnlyLoopbackShimCandidatesForTest(bool only);

    /// `stun:host[:port]` (RFC 7064), an IPv6 literal in brackets.
    static std::optional<IceServerAddress> parseStunUrl(const QString& url);
    /// `turn:host[:port][?transport=udp]` (RFC 7065). nullopt for `turns:`
    /// and for any transport but UDP, which the pinned libjuice cannot use.
    static std::optional<IceServerAddress> parseTurnUrl(const QString& url);
    /// The candidate's type (`host`, `srflx`, `prflx`, `relay`), empty when
    /// it has none.
    static QString candidateType(const QString& candidate);

private:
    /// The index in `hosts` (in the service's order) to use: see the file
    /// comment.
    int chooseByFamily(const QStringList& hosts) const;
    AddressFamilies familiesOfHost(const QString& host) const;

    AddressFamilies m_local;
    HostFamilies m_hosts;
    std::optional<IceServerAddress> m_stun;
    QList<IceRelayServer> m_relays;
    bool m_relayAllowed = true;
    bool m_relayKnown = false;
    CandidateSourceFactory m_sourceFactory;
    bool m_sourceNeedsRelay = true;
    bool m_mediaRouting = false;
    bool m_onlySourceCandidates = false;
};

} // namespace NereusSDR
