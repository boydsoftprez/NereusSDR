// =================================================================
// src/core/session/DataChannelTransport.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original.
//
// iPhone app plan Task 28 (R-IOS-16). See DataChannelTransport.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-26: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-26: Task 28 fix wave: the OpenSSL error scope (Important 3),
//               queues bounded by bytes, the close waits for the channel's
//               close (Minors 3 and 7). J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-27: setLibraryLogForTest() (R-R3-49). J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude Code.
//   2026-09-27: iPhone app plan Task 29 (R-IOS-16): the ICE settings'
//               other candidate sources start with gathering and stop with
//               the connection. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-27: iPhone app plan Task 29 step 2b (R-IOS-16, R-IOS-08): the
//               web relay's leg (RelayLeg) and its per-connection candidate
//               sources; the computer's own proxy settings (SystemProxy).
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-27: separate bounded transmit-watch DTLS channel; AI-assisted
//               implementation via OpenAI Codex for J.J. Boyd (KG4VCF).
//   2026-09-28: setWatchRelayClockForTest, the clock a watch relay grant's
//               expiry is read against. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-09-28: setLibraryLogForTest() moved out of the product class to
//               DataChannelLibraryLogTestHook (test builds only), no
//               behavior change. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-29: the opt-in ICE check log (IceDiagnostics, NEREUS_ICE_DIAG):
//               this peer's candidates, the ones it admits, its states and
//               its selected pair, redacted. J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
//   2026-09-30: LINK minor 9: the lingering close holds the application
//               through a QPointer. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-09-30: Fix round 1 (LINK minor 9): the lingering close posts
//               through a context object that the application's teardown
//               deletes under a lock (qAddPostRoutine), not a QPointer read
//               on libdatachannel's thread. J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
//   2026-10-01: the application's teardown deletes the lingering close's
//               context object after the lock, not under it, and a Linger
//               clears its peer's callbacks when it is destroyed: deleting
//               the object under the lock destroyed a lingering peer whose
//               state callback took the same lock, and the process hung at
//               exit (tst_station_tx_watch_relay). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-10-01: the teardown clears routineAdded, so a context object made
//               after it (a close later in the same teardown, or a later
//               application) is deleted too instead of leaking with the
//               peers it holds; lingerTargetExistsForTest(). J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-01: Control logging lane: a control message carries the time
//               its frame was reassembled on the library's thread, so the
//               Core's log can tell the wait for this thread from the
//               network's (deliveringMessageWaitUs()); linkDiagnostics();
//               the selected pair's candidate transports. Measurement
//               only: the queue and its order are unchanged. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/session/DataChannelTransport.h"
#include "core/session/RelayLeg.h"

#include <QDateTime>
#include "core/session/CandidateSourceLease.h"
#include "core/session/IceDiagnostics.h"

#include "core/security/OpenSslErrorScope.h"
#include "core/session/media/LibDataChannelMediaTransport.h"

#include <QCoreApplication>
#include <QHostAddress>
#include <QLoggingCategory>
#include <QMetaMethod>
#include <QMetaObject>
#include <QThread>
#include <QPointer>
#include <QRegularExpression>
#include <QTimer>
#include <QtEndian>

#include <rtc/rtc.hpp>

#include <chrono>

#include <atomic>
#include <cstddef>
#include <deque>
#include <mutex>
#include <string>
#include <utility>
#include <variant>
#include <vector>

Q_LOGGING_CATEGORY(lcControlChannel, "nereus.session.controlchannel")

namespace NereusSDR {

// ── The wire ────────────────────────────────────────────────────────────

QList<QByteArray> ControlFraming::chunk(const QByteArray& message)
{
    QList<QByteArray> chunks;
    qsizetype offset = 0;
    while (offset < message.size()) {
        const qsizetype length = qMin(kMaxChunkPayloadBytes, message.size() - offset);
        const bool last = offset + length == message.size();
        QByteArray frame;
        frame.reserve(length + 1);
        frame.append(static_cast<char>(last ? kChunkLast : kChunkMore));
        frame.append(message.constData() + offset, length);
        chunks.append(frame);
        offset += length;
    }
    return chunks;
}

namespace {

// The ICE check log's tag for this peer's lines.
const char* iceDiagPath(DataChannelTransport::Purpose purpose)
{
    return purpose == DataChannelTransport::Purpose::TxWatch ? "watch" : "control";
}

QByteArray heartbeatFrame(quint8 kind, quint32 id)
{
    QByteArray frame(ControlFraming::kPingBytes, Qt::Uninitialized);
    frame[0] = static_cast<char>(kind);
    qToBigEndian(id, frame.data() + 1);
    return frame;
}

} // namespace

QByteArray ControlFraming::ping(quint32 id)
{
    return heartbeatFrame(kPing, id);
}

QByteArray ControlFraming::pong(quint32 id)
{
    return heartbeatFrame(kPong, id);
}

ControlFraming::Reassembler::Result ControlFraming::Reassembler::refuse(const QString& reason)
{
    m_refused = true;
    m_reason = reason;
    m_pending.clear();
    m_pending.squeeze();
    return Result::Refused;
}

ControlFraming::Reassembler::Result ControlFraming::Reassembler::feed(const QByteArray& frame)
{
    if (m_refused) {
        return Result::Refused;
    }
    if (frame.isEmpty()) {
        return refuse(QStringLiteral("an empty message on the control channel"));
    }
    if (frame.size() > kMaxChunkBytes) {
        return refuse(QStringLiteral("a control channel message longer than %1 bytes")
                          .arg(kMaxChunkBytes));
    }
    const auto kind = static_cast<quint8>(frame.at(0));
    if (kind == kPing || kind == kPong) {
        if (frame.size() != kPingBytes) {
            return refuse(QStringLiteral("a ping or pong that is not %1 bytes").arg(kPingBytes));
        }
        m_id = qFromBigEndian<quint32>(frame.constData() + 1);
        return kind == kPing ? Result::Ping : Result::Pong;
    }
    if (kind != kChunkMore && kind != kChunkLast) {
        return refuse(QStringLiteral("a control channel message of unknown kind 0x%1")
                          .arg(kind, 2, 16, QLatin1Char('0')));
    }
    if (frame.size() < 2) {
        return refuse(QStringLiteral("a chunk with nothing in it"));
    }
    const quint64 total =
        static_cast<quint64>(m_pending.size()) + static_cast<quint64>(frame.size() - 1);
    if (total > m_maxMessageBytes) {
        return refuse(QStringLiteral("a message larger than %1 bytes").arg(m_maxMessageBytes));
    }
    m_pending.append(frame.constData() + 1, frame.size() - 1);
    if (kind == kChunkMore) {
        return Result::Pending;
    }
    m_message = m_pending;
    m_pending.clear();
    return Result::Message;
}

// ── The bridge from the library's threads ───────────────────────────────

namespace {

struct Event {
    enum class Kind {
        Description,
        Candidate,
        GatheringComplete,
        Open,
        Message,
        Pong,
        Refused,
        Closed,
        Failed,
    };
    Kind kind;
    QByteArray bytes;
    QString first;
    QString second;
    quint32 id = 0;
    /// Control logging lane: a control message's receipt (steady clock,
    /// nanoseconds), 0 where not measured. For the log only.
    qint64 receivedNs = 0;
};

qint64 steadyNowNs()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

// NereusSDR's own bounds on events waiting for this object's thread: how
// many, and the bytes of the messages among them
// (DataChannelTransport::kMaxQueuedCaps times the inbound cap). A burst
// past either means the owner thread has stopped; the connection ends then
// rather than grow without limit.
constexpr std::size_t kMaxPendingEvents = 4096;

} // namespace

struct DataChannelTransport::Bridge {
    explicit Bridge(DataChannelTransport* o, quint64 cap, DataChannelTransport::Purpose purpose)
        : owner(o)
        , purpose(purpose)
        , maxPendingBytes(purpose == Purpose::TxWatch
                              ? DataChannelTransport::kMaxWatchQueuedBytes
                              : cap * DataChannelTransport::kMaxQueuedCaps)
        , reassembler(cap)
    {
    }

    std::mutex mutex;
    DataChannelTransport* owner = nullptr;
    bool cancelled = false;
    bool drainPosted = false;
    bool overflowed = false;
    std::deque<Event> events;
    const DataChannelTransport::Purpose purpose;
    const quint64 maxPendingBytes;
    quint64 pendingBytes = 0;
    quint64 watchOutstandingBytes = 0;
    qsizetype watchOutstandingFrames = 0;
    ControlFraming::Reassembler reassembler;
    std::shared_ptr<rtc::PeerConnection> peer;
    std::shared_ptr<rtc::DataChannel> channel;
    bool channelAssigned = false;
    std::atomic<bool> answersPings{true};
    std::atomic<quint64> pingsReceived{0};
    std::atomic<quint64> pongsSent{0};

    // Holds `mutex`.
    void postLocked(Event event)
    {
        if (cancelled) {
            return;
        }
        const quint64 bytes = static_cast<quint64>(event.bytes.size());
        const std::size_t eventLimit = purpose == Purpose::TxWatch
            ? DataChannelTransport::kMaxWatchEvents : kMaxPendingEvents;
        const bool watchMessage = purpose == Purpose::TxWatch
            && event.kind == Event::Kind::Message;
        const bool watchFramesFull = watchMessage
            && (watchOutstandingFrames >= DataChannelTransport::kMaxWatchQueuedFrames
                || watchOutstandingBytes + bytes > DataChannelTransport::kMaxWatchQueuedBytes);
        if (events.size() >= eventLimit || pendingBytes + bytes > maxPendingBytes
            || watchFramesFull) {
            if (!overflowed) {
                overflowed = true;
                events.clear();
                pendingBytes = 0;
                watchOutstandingBytes = 0;
                watchOutstandingFrames = 0;
                events.push_back({Event::Kind::Refused, {},
                                  purpose == Purpose::TxWatch
                                      ? QStringLiteral("the watch channel fell behind")
                                      : QStringLiteral("the control channel fell behind"), {}, 0});
            }
        } else if (!overflowed) {
            pendingBytes += bytes;
            if (watchMessage) {
                watchOutstandingBytes += bytes;
                ++watchOutstandingFrames;
            }
            events.push_back(std::move(event));
        }
        if (!drainPosted) {
            drainPosted = true;
            DataChannelTransport* target = owner;
            QMetaObject::invokeMethod(target, [target]() { target->drain(); },
                                      Qt::QueuedConnection);
        }
    }

    void post(Event event)
    {
        std::lock_guard lock(mutex);
        postLocked(std::move(event));
    }

    void watchDelivered(qsizetype bytes)
    {
        std::lock_guard lock(mutex);
        if (watchOutstandingFrames > 0) {
            --watchOutstandingFrames;
            watchOutstandingBytes -= static_cast<quint64>(bytes);
        }
    }
};

namespace {

void onFrame(const std::weak_ptr<DataChannelTransport::Bridge>& weak, rtc::binary data)
{
    const auto bridge = weak.lock();
    if (!bridge) {
        return;
    }
    if (bridge->purpose == DataChannelTransport::Purpose::TxWatch) {
        std::lock_guard lock(bridge->mutex);
        if (bridge->cancelled || bridge->overflowed) {
            return;
        }
        if (data.empty() || data.size() > DataChannelTransport::kMaxWatchFrameBytes) {
            bridge->postLocked({Event::Kind::Refused, {},
                                QStringLiteral("invalid watch frame size"), {}, 0});
        } else {
            bridge->postLocked({Event::Kind::Message,
                                QByteArray(reinterpret_cast<const char*>(data.data()),
                                           static_cast<qsizetype>(data.size())), {}, {}, 0});
        }
        return;
    }
    const QByteArray frame(reinterpret_cast<const char*>(data.data()),
                           static_cast<qsizetype>(data.size()));
    std::shared_ptr<rtc::DataChannel> answerOn;
    quint32 pongId = 0;
    {
        std::lock_guard lock(bridge->mutex);
        if (bridge->cancelled || bridge->overflowed) {
            return;
        }
        ControlFraming::Reassembler& reassembler = bridge->reassembler;
        switch (reassembler.feed(frame)) {
        case ControlFraming::Reassembler::Result::Pending:
            break;
        case ControlFraming::Reassembler::Result::Message:
            bridge->postLocked(
                {Event::Kind::Message, reassembler.message(), {}, {}, 0, steadyNowNs()});
            break;
        case ControlFraming::Reassembler::Result::Ping:
            bridge->pingsReceived.fetch_add(1, std::memory_order_relaxed);
            if (bridge->answersPings.load(std::memory_order_relaxed)) {
                answerOn = bridge->channel;
                pongId = reassembler.id();
            }
            break;
        case ControlFraming::Reassembler::Result::Pong:
            bridge->postLocked({Event::Kind::Pong, {}, {}, {}, reassembler.id()});
            break;
        case ControlFraming::Reassembler::Result::Refused:
            bridge->postLocked({Event::Kind::Refused, {}, reassembler.reason(), {}, 0});
            break;
        }
    }
    // Answered here, off this object's thread and outside the lock, as a
    // WebSocket stack answers a ping without the application.
    if (answerOn) {
        const QByteArray pong = ControlFraming::pong(pongId);
        try {
            answerOn->send(reinterpret_cast<const std::byte*>(pong.constData()),
                           static_cast<std::size_t>(pong.size()));
            bridge->pongsSent.fetch_add(1, std::memory_order_relaxed);
        } catch (const std::exception&) {
            // The channel closed under the ping; its close is reported.
        }
    }
}

void bindChannel(const std::shared_ptr<rtc::DataChannel>& channel,
                 const std::weak_ptr<DataChannelTransport::Bridge>& weak)
{
    channel->onOpen([weak]() {
        if (const auto bridge = weak.lock()) {
            bridge->post({Event::Kind::Open, {}, {}, {}, 0});
        }
    });
    channel->onClosed([weak]() {
        if (const auto bridge = weak.lock()) {
            bridge->post({Event::Kind::Closed, {}, QStringLiteral("the control channel closed"),
                          {}, 0});
        }
    });
    channel->onError([weak](std::string error) {
        if (const auto bridge = weak.lock()) {
            bridge->post({Event::Kind::Failed, {}, QString::fromStdString(error), {}, 0});
        }
    });
    channel->onMessage([weak](rtc::binary data) { onFrame(weak, std::move(data)); },
                       [weak](std::string) {
        if (const auto bridge = weak.lock()) {
            std::lock_guard lock(bridge->mutex);
            if (!bridge->overflowed) {
                bridge->postLocked({Event::Kind::Refused, {},
                                    bridge->purpose == DataChannelTransport::Purpose::TxWatch
                                        ? QStringLiteral("a text message on the watch channel")
                                        : QStringLiteral("a text message on the control channel"), {},
                                    0});
            }
        }
    });
}

QString typeName(const rtc::Candidate& candidate)
{
    switch (candidate.type()) {
    case rtc::Candidate::Type::Host:
        return QStringLiteral("host");
    case rtc::Candidate::Type::ServerReflexive:
        return QStringLiteral("srflx");
    case rtc::Candidate::Type::PeerReflexive:
        return QStringLiteral("prflx");
    case rtc::Candidate::Type::Relayed:
        return QStringLiteral("relay");
    default:
        return QString();
    }
}

} // namespace

// ── The transport ───────────────────────────────────────────────────────

DataChannelTransport::DataChannelTransport(QObject* parent)
    : SessionTransport(parent)
{
}

DataChannelTransport::~DataChannelTransport()
{
    stopPeer();
}

namespace {
DataChannelTransport::WatchRelayClock& watchRelayClock()
{
    static DataChannelTransport::WatchRelayClock clock;
    return clock;
}

// The time a watch relay grant's expiry is compared with: the test's clock
// when one is set, else the wall clock.
qint64 watchRelayNowSecs()
{
    return watchRelayClock() ? watchRelayClock()() : QDateTime::currentSecsSinceEpoch();
}
} // namespace

bool DataChannelTransport::setWatchRelayGrant(const WatchRelayGrant& grant)
{
    if (!m_started || m_closing || m_options.purpose != Purpose::Control || m_watchRelayGrant
        || !grant.url.isValid() || grant.url.scheme() != QLatin1String("wss")
        || grant.url.host().isEmpty() || grant.url.authority(QUrl::FullyEncoded).contains('@')
        || grant.url.hasFragment() || grant.token.isEmpty()
        || grant.token.toUtf8().size() > RendezvousWire::kMaxRelayTokenBytes
        || grant.expires <= watchRelayNowSecs() || grant.primaryLeg.expired()) {
        return false;
    }
    m_watchRelayGrant = grant;
    return true;
}

bool DataChannelTransport::hasWatchRelayRoute() const
{
    if (!isOpen() || m_closing || m_options.purpose != Purpose::Control || !m_watchRelayGrant) {
        return false;
    }
    const auto path = selectedPath();
    const auto primary = m_watchRelayGrant->primaryLeg.lock();
    return path && path->viaLoopbackShim() && primary
        && primary->state() == RelayLeg::State::Joined && primary->peerPresent();
}

bool DataChannelTransport::canOpenWatchRelay() const
{
    return hasWatchRelayRoute()
        && m_watchRelayGrant->expires > watchRelayNowSecs();
}

bool DataChannelTransport::start(const Options& options)
{
    if (m_started || options.maxIncomingBytes == 0
        || (options.role == Role::Answerer
            && options.certificatePemPath.isEmpty() != options.privateKeyPemPath.isEmpty())) {
        return false;
    }
    if (options.purpose == Purpose::TxWatch
        && (options.maxIncomingBytes != kMaxWatchFrameBytes || !options.ice
            || options.ice->stunServer() || !options.ice->relayAllowed()
            || !options.ice->relayKnown() || !options.ice->relayServers().isEmpty()
            || !options.ice->hasCandidateSourceFactory()
            || !options.ice->makeCandidateSource(IceConfiguration::kControlLane))) {
        return false;
    }
    m_options = options;
    m_ownedShimEndpoints.clear();
    m_bridge = std::make_shared<Bridge>(this, options.maxIncomingBytes, options.purpose);
    const std::weak_ptr<Bridge> weak = m_bridge;
    // libdatachannel reads the PEM files while the peer is made, on this
    // thread, and its loop over further certificates in the file ends on a
    // failed read that stays in this thread's OpenSSL error queue
    // (libdatachannel v0.24.5 src/impl/certificate.cpp:424-428); a bad or
    // missing file throws with its error queued. Qt's OpenSSL TLS backend
    // reads that queue after its own calls on the same thread, and a stale
    // error there ends a healthy wss:// connection: the Core's own
    // connection to the remote access service dropped the moment it
    // answered an introduction (the traversal harness, Linux). The scope
    // clears it on every way out, the throw included.
    const OpenSslErrorScope openSslErrors;

    try {
        rtc::Configuration config;
        config.mtu = static_cast<std::size_t>(IMediaTransport::kConfiguredMtuBytes);
        // Every control chunk fits; the library refuses larger ones. Admit
        // malformed watch frames through SCTP up to the hard outbound bound,
        // then reject them on the library callback before Qt queues.
        config.maxMessageSize = options.purpose == Purpose::TxWatch
            ? kMaxWatchOutboundBytes
            : static_cast<std::size_t>(ControlFraming::kMaxChunkBytes);
        config.disableAutoNegotiation = true;
        config.enableIceTcp = false;
        config.iceServers.clear();
        if (options.ice) {
            // As the media transport (Task 27): one STUN server, gathering
            // held until the relay is known, the MTU with room for TURN's
            // ChannelData header.
            config.mtu = static_cast<std::size_t>(IceConfiguration::kMtuBytes);
            if (const auto stun = options.ice->stunServer()) {
                config.iceServers.emplace_back(stun->host.toStdString(), stun->port);
            }
            config.disableAutoGathering = true;
            if (options.ice->relayKnown()) {
                m_gatherRequested = true;
                m_relays = options.ice->relayServers();
            }
            if (options.ice->hasCandidateSourceFactory()) {
                m_candidateSourceLease = CandidateSourceLease::create(*options.ice);
                config.iceTransportLifetime = m_candidateSourceLease;
            }
        }
        if (!options.certificatePemPath.isEmpty()) {
            // The Core's own persistent TLS certificate: the SHA-256 a
            // device sees in DTLS is then the one the hello's binding
            // covers (link section 3.4).
            config.certificatePemFile = options.certificatePemPath.toStdString();
            config.keyPemFile = options.privateKeyPemPath.toStdString();
        }

        // The process-wide SCTP limits come before the first peer of any
        // kind (LibDataChannelMediaTransport.h).
        applyMediaSctpSettingsOnce();
        auto peer = std::make_shared<rtc::PeerConnection>(std::move(config));
        m_bridge->peer = peer;
        peer->onLocalDescription([weak](rtc::Description description) {
            const auto bridge = weak.lock();
            if (!bridge) {
                return;
            }
            const std::string sdp = description.generateSdp();
            if (sdp.size() > static_cast<std::size_t>(IMediaTransport::kMaxDescriptionBytes)) {
                bridge->post({Event::Kind::Failed, {},
                              QStringLiteral("oversized local description"), {}, 0});
                return;
            }
            bridge->post({Event::Kind::Description, {}, QString::fromStdString(sdp),
                          QString::fromStdString(description.typeString()), 0});
        });
        peer->onLocalCandidate([weak](rtc::Candidate candidate) {
            const auto bridge = weak.lock();
            if (!bridge) {
                return;
            }
            const std::string value = candidate.candidate();
            if (value.size() > static_cast<std::size_t>(IMediaTransport::kMaxCandidateBytes)) {
                return;
            }
            if (IceDiagnostics::enabled()) {
                IceDiagnostics::logPath(iceDiagPath(bridge->purpose),
                                        QStringLiteral("local candidate %1")
                                            .arg(QString::fromStdString(value)));
            }
            bridge->post({Event::Kind::Candidate, {}, QString::fromStdString(value), {}, 0});
        });
        peer->onGatheringStateChange([weak](rtc::PeerConnection::GatheringState state) {
            const auto bridge = weak.lock();
            if (bridge && state == rtc::PeerConnection::GatheringState::Complete) {
                bridge->post({Event::Kind::GatheringComplete, {}, {}, {}, 0});
            }
        });
        if (IceDiagnostics::enabled()) {
            const char* path = iceDiagPath(options.purpose);
            peer->onIceStateChange([path](rtc::PeerConnection::IceState state) {
                IceDiagnostics::logPath(path, QStringLiteral("ICE state %1")
                                                  .arg(IceDiagnostics::stateName(state)));
            });
        }
        peer->onStateChange([weak](rtc::PeerConnection::State state) {
            const auto bridge = weak.lock();
            if (!bridge) {
                return;
            }
            if (IceDiagnostics::enabled()) {
                IceDiagnostics::logPath(iceDiagPath(bridge->purpose),
                                        QStringLiteral("peer state %1")
                                            .arg(IceDiagnostics::stateName(state)));
            }
            if (state == rtc::PeerConnection::State::Failed) {
                bridge->post({Event::Kind::Failed, {},
                              QStringLiteral("the control connection could not be made"), {}, 0});
            } else if (state == rtc::PeerConnection::State::Closed) {
                bridge->post({Event::Kind::Closed, {},
                              QStringLiteral("the control connection closed"), {}, 0});
            }
        });
        peer->onDataChannel([weak](std::shared_ptr<rtc::DataChannel> channel) {
            const auto bridge = weak.lock();
            if (!bridge) {
                channel->close();
                return;
            }
            const rtc::Reliability reliability = channel->reliability();
            // Only the one reliable, ordered channel labelled "control".
            const char* label = bridge->purpose == Purpose::TxWatch
                ? DataChannelTransport::kTxWatchLabel : DataChannelTransport::kLabel;
            const bool usable = channel->label() == label
                && !reliability.unordered && !reliability.maxRetransmits
                && !reliability.maxPacketLifeTime;
            bool take = false;
            {
                std::lock_guard lock(bridge->mutex);
                take = usable && !bridge->cancelled && !bridge->channelAssigned;
                if (take) {
                    bridge->channelAssigned = true;
                    bridge->channel = channel;
                }
            }
            if (!take) {
                channel->close();
                bridge->post({Event::Kind::Failed, {},
                              QStringLiteral("an unexpected data channel was refused"), {}, 0});
                return;
            }
            bindChannel(channel, weak);
            // An incoming channel is open when it is reported.
            bridge->post({Event::Kind::Open, {}, {}, {}, 0});
        });

        if (options.role == Role::Offerer) {
            // Reliable and ordered: the library's defaults.
            auto channel = peer->createDataChannel(options.purpose == Purpose::TxWatch
                                                       ? kTxWatchLabel : kLabel);
            {
                std::lock_guard lock(m_bridge->mutex);
                m_bridge->channel = channel;
                m_bridge->channelAssigned = true;
            }
            bindChannel(channel, weak);
        }
        m_started = true;
        if (options.role == Role::Offerer) {
            peer->setLocalDescription(rtc::Description::Type::Offer);
            gatherIfReady();
        }
        return true;
    } catch (const std::exception& error) {
        qCWarning(lcControlChannel) << "The control connection could not start:" << error.what();
        stopPeer();
        m_started = false;
        return false;
    }
}

bool DataChannelTransport::acceptDescription(const QString& sdp, const QString& type)
{
    if (!m_started || m_remoteDescriptionAccepted || !m_bridge || !m_bridge->peer) {
        return false;
    }
    const QByteArray sdpBytes = sdp.toUtf8();
    const QByteArray typeBytes = type.toUtf8().toLower();
    if (sdpBytes.isEmpty() || sdpBytes.size() > IMediaTransport::kMaxDescriptionBytes
        || sdpBytes.contains('\0')) {
        return false;
    }
    const QByteArray expected = m_options.role == Role::Offerer ? QByteArrayLiteral("answer")
                                                                : QByteArrayLiteral("offer");
    if (typeBytes != expected) {
        return false;
    }
    try {
        rtc::Description description(sdpBytes.toStdString(), typeBytes.toStdString());
        // Candidates come one at a time, through acceptCandidate() and its
        // rules, never inside the description.
        // A control connection is its one data channel and nothing else: a
        // description with any other line (audio, video) is not one.
        if (!description.candidates().empty() || !description.hasApplication()
            || description.mediaCount() != 1) {
            return false;
        }
        if (m_options.purpose == Purpose::TxWatch) {
            for (const QByteArray& line : sdpBytes.split('\n')) {
                const QByteArray trimmed = line.trimmed().toLower();
                if (trimmed.startsWith("a=candidate:")
                    || trimmed.startsWith("a=remote-candidates:")
                    || trimmed == "a=end-of-candidates") {
                    return false;
                }
            }
        }
        m_bridge->peer->setRemoteDescription(std::move(description));
        m_remoteDescriptionAccepted = true;
        if (m_options.role == Role::Answerer) {
            m_bridge->peer->setLocalDescription(rtc::Description::Type::Answer);
            gatherIfReady();
        }
        for (const QString& candidate : std::exchange(m_pendingSourceCandidates, {})) {
            if (m_options.purpose == Purpose::TxWatch) {
                if (!acceptOwnedWatchCandidate(candidate)) {
                    closeLink(QStringLiteral("invalid watch candidate source"));
                    return false;
                }
            } else {
                admitCandidate(candidate, true);
            }
        }
        return true;
    } catch (const std::exception& error) {
        qCWarning(lcControlChannel) << "A control connection description was refused:"
                                    << error.what();
        return false;
    }
}

bool DataChannelTransport::acceptCandidate(const QString& candidate)
{
    return admitCandidate(candidate, false);
}

bool DataChannelTransport::admitCandidate(const QString& candidate, bool fromOwnedSource)
{
    if (m_options.purpose == Purpose::TxWatch) {
        return false;
    }
    if (!m_started || !m_bridge || !m_bridge->peer
        || m_acceptedCandidates >= IMediaTransport::kMaxRemoteCandidates) {
        return false;
    }
    const QByteArray bytes = candidate.toUtf8();
    if (bytes.isEmpty() || bytes.size() > IMediaTransport::kMaxCandidateBytes
        || bytes.contains('\0')) {
        return false;
    }
    try {
        rtc::Candidate parsed(bytes.toStdString(), std::string());
        if (m_options.ice) {
            if (!m_options.ice->acceptsRemoteCandidate(QString::fromUtf8(bytes))) {
                logRemoteCandidate(candidate, fromOwnedSource, false);
                return false;
            }
        } else if (parsed.type() != rtc::Candidate::Type::Host) {
            logRemoteCandidate(candidate, fromOwnedSource, false);
            return false;
        }
        if (parsed.type() == rtc::Candidate::Type::Relayed) {
            // Where the far end's relay is, so a remote learned there as
            // peer-reflexive still reads as relayed (MediaIcePath).
            rtc::Candidate relay = parsed;
            if (relay.resolve(rtc::Candidate::ResolveMode::Simple) && relay.address()
                && relay.port()) {
                m_farEndRelays.append(
                    qMakePair(QString::fromStdString(*relay.address()), *relay.port()));
            }
        }
        std::optional<QPair<QString, quint16>> ownedEndpoint;
        if (fromOwnedSource) {
            rtc::Candidate source = parsed;
            if (source.resolve(rtc::Candidate::ResolveMode::Simple)
                && source.address() && source.port()) {
                ownedEndpoint = MediaIcePath::loopbackEndpoint(
                    QString::fromStdString(*source.address()), *source.port());
            }
        }
        logRemoteCandidate(candidate, fromOwnedSource, true);
        m_bridge->peer->addRemoteCandidate(std::move(parsed));
        ++m_acceptedCandidates;
        if (ownedEndpoint && !m_ownedShimEndpoints.contains(*ownedEndpoint)) {
            m_ownedShimEndpoints.append(*ownedEndpoint);
        }
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

void DataChannelTransport::logRemoteCandidate(const QString& candidate, bool fromOwnedSource,
                                              bool admitted) const
{
    if (!IceDiagnostics::enabled()) {
        return;
    }
    IceDiagnostics::logPath(iceDiagPath(m_options.purpose),
                            QStringLiteral("remote candidate%1 %2: %3")
                                .arg(fromOwnedSource ? QStringLiteral(" (tunnel)") : QString(),
                                     admitted ? QStringLiteral("admitted")
                                              : QStringLiteral("refused"),
                                     candidate));
}

bool DataChannelTransport::acceptOwnedWatchCandidate(const QString& candidate)
{
    if (m_options.purpose != Purpose::TxWatch || !m_started || !m_bridge
        || !m_bridge->peer || !m_remoteDescriptionAccepted
        || m_acceptedCandidates >= kMaxWatchQueuedFrames) {
        return false;
    }
    // RelayLeg's watch lane is the sole candidate source. Reject a source
    // accidentally configured for ordinary host, STUN or TURN candidates.
    static const QRegularExpression pattern(
        QStringLiteral(R"(^candidate:wsrelay1 1 UDP [0-9]+ 127\.0\.0\.1 ([0-9]+) typ host$)"));
    const QRegularExpressionMatch match = pattern.match(candidate);
    if (!match.hasMatch() || candidate.toUtf8().size() > IMediaTransport::kMaxCandidateBytes) {
        return false;
    }
    bool ok = false;
    const int port = match.captured(1).toInt(&ok);
    if (!ok || port < 1 || port > 65535) {
        return false;
    }
    try {
        rtc::Candidate parsed(candidate.toStdString(), std::string());
        logRemoteCandidate(candidate, true, true);
        m_bridge->peer->addRemoteCandidate(std::move(parsed));
        ++m_acceptedCandidates;
        const auto endpoint = MediaIcePath::loopbackEndpoint(QStringLiteral("127.0.0.1"),
                                                             static_cast<quint16>(port));
        if (endpoint && !m_ownedShimEndpoints.contains(*endpoint)) {
            m_ownedShimEndpoints.append(*endpoint);
        }
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool DataChannelTransport::gatherCandidates(const IceConfiguration& configured)
{
    if (!m_started || !m_options.ice || m_gatherRequested
        || m_options.purpose == Purpose::TxWatch) {
        return false;
    }
    m_gatherRequested = true;
    m_options.ice = configured;
    m_relays = configured.relayAllowed() ? configured.relayServers() : QList<IceRelayServer>();
    gatherIfReady();
    return true;
}

void DataChannelTransport::gatherIfReady()
{
    if (!m_started || !m_bridge || !m_bridge->peer || !m_options.ice || !m_gatherRequested
        || m_gatheringStarted || !m_bridge->peer->localDescription()
        || (m_options.purpose == Purpose::TxWatch && !m_localDescriptionEmitted)) {
        return;
    }
    m_gatheringStarted = true;
    std::vector<rtc::IceServer> relays;
    for (const IceRelayServer& relay : std::as_const(m_relays)) {
        if (relays.size() >= static_cast<std::size_t>(IceConfiguration::kMaxRelayServers)) {
            break;
        }
        relays.emplace_back(relay.host.toStdString(), relay.port, relay.username.toStdString(),
                            relay.password.toStdString(), rtc::IceServer::RelayType::TurnUdp);
    }
    try {
        m_bridge->peer->gatherLocalCandidates(std::move(relays));
    } catch (const std::exception& error) {
        qCWarning(lcControlChannel) << "Gathering failed:" << error.what();
    }
    // iPhone app plan Task 29 (link section 21.5), step 2b: this
    // connection's own source on the control lane (the web relay's leg)
    // joins its ICE now, unless the relay is not allowed.
    if (m_candidateSourceLease) {
        const QPointer<DataChannelTransport> self(this);
        // ICE may retain an old lease after this wrapper closes. Its queued
        // callback belongs only to the peer that installed that exact lease.
        const std::weak_ptr<CandidateSourceLease> expected = m_candidateSourceLease;
        m_candidateSourceLease->start(IceConfiguration::kControlLane, {},
                                      [self, expected](const QString& candidate) {
            const auto lease = expected.lock();
            if (!self || !lease || self->m_candidateSourceLease != lease
                || !self->m_started || self->m_closing) {
                return;
            }
            // Before the remote description the agent takes no remote
            // candidate: held until it comes.
            if (self->m_options.purpose == Purpose::TxWatch
                && !self->acceptOwnedWatchCandidate(candidate)
                && self->m_remoteDescriptionAccepted) {
                self->closeLink(QStringLiteral("invalid watch candidate source"));
                return;
            }
            if (!self->m_remoteDescriptionAccepted) {
                if (self->m_options.purpose == Purpose::TxWatch
                    && self->m_pendingSourceCandidates.size() >= kMaxWatchQueuedFrames) {
                    self->closeLink(QStringLiteral("watch candidate source fell behind"));
                    return;
                }
                self->m_pendingSourceCandidates.append(candidate);
                return;
            }
            if (self->m_options.purpose == Purpose::Control) {
                self->admitCandidate(candidate, true);
            }
        });
    }
}

namespace {
DataChannelTransport::SelectedPathOverride& selectedPathOverride()
{
    static DataChannelTransport::SelectedPathOverride override;
    return override;
}
} // namespace

void DataChannelTransport::setWatchRelayClockForTest(WatchRelayClock clock)
{
    watchRelayClock() = std::move(clock);
}

void DataChannelTransport::setSelectedPathOverrideForTest(SelectedPathOverride override)
{
    selectedPathOverride() = std::move(override);
}

std::optional<MediaIcePath> DataChannelTransport::selectedPath() const
{
    if (thread() != QThread::currentThread() || m_closing) {
        return std::nullopt;
    }
    if (selectedPathOverride()) {
        return selectedPathOverride()(this);
    }
    if (!m_bridge || !m_bridge->peer) {
        return std::nullopt;
    }
    try {
        rtc::Candidate local;
        rtc::Candidate remote;
        if (!m_bridge->peer->getSelectedCandidatePair(&local, &remote)) {
            return std::nullopt;
        }
        const auto transportName = [](const rtc::Candidate& candidate) {
            switch (candidate.transportType()) {
            case rtc::Candidate::TransportType::Udp:
                return QStringLiteral("udp");
            case rtc::Candidate::TransportType::TcpActive:
                return QStringLiteral("tcp-active");
            case rtc::Candidate::TransportType::TcpPassive:
                return QStringLiteral("tcp-passive");
            case rtc::Candidate::TransportType::TcpSo:
                return QStringLiteral("tcp-so");
            case rtc::Candidate::TransportType::TcpUnknown:
                return QStringLiteral("tcp");
            default:
                return QString();
            }
        };
        MediaIcePath path;
        path.localType = typeName(local);
        path.remoteType = typeName(remote);
        // Control logging lane: the candidates' transports, for the log.
        path.localTransport = transportName(local);
        path.remoteTransport = transportName(remote);
        path.localAddress = QString::fromStdString(local.address().value_or(std::string()));
        path.localPort = local.port().value_or(0);
        path.remoteAddress = QString::fromStdString(remote.address().value_or(std::string()));
        path.remotePort = remote.port().value_or(0);
        path.farEndRelays = m_farEndRelays;
        const auto endpoint = MediaIcePath::loopbackEndpoint(path.remoteAddress, path.remotePort);
        path.ownedLoopbackShim = endpoint && m_ownedShimEndpoints.contains(*endpoint);
        if (path.ownedLoopbackShim && m_candidateSourceLease) {
            path.ownedSourcePath = m_candidateSourceLease->networkPathSnapshot();
        }
        return path;
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

std::optional<NetworkPathSnapshot> DataChannelTransport::networkPathSnapshot() const
{
    if (thread() != QThread::currentThread() || !isOpen()) {
        return std::nullopt;
    }
    const std::optional<MediaIcePath> path = selectedPath();
    return path ? path->networkPathSnapshot() : std::nullopt;
}

void DataChannelTransport::sendText(const QByteArray& wire)
{
    if (m_options.purpose == Purpose::TxWatch || !isOpen() || wire.isEmpty()) {
        return;
    }
    std::shared_ptr<rtc::DataChannel> channel;
    {
        std::lock_guard lock(m_bridge->mutex);
        channel = m_bridge->channel;
    }
    if (!channel) {
        return;
    }
    try {
        for (const QByteArray& chunk : ControlFraming::chunk(wire)) {
            // Returns false when the library buffered it; nothing is lost.
            channel->send(reinterpret_cast<const std::byte*>(chunk.constData()),
                          static_cast<std::size_t>(chunk.size()));
            ++m_chunksSent;
        }
        ++m_messagesSent;
        m_telemetry.acceptedPayloadBytes += static_cast<quint64>(wire.size());
    } catch (const std::exception& error) {
        qCWarning(lcControlChannel) << "A control message was not sent:" << error.what();
    }
}

bool DataChannelTransport::sendBinary(const QByteArray& message)
{
    if (m_options.purpose != Purpose::TxWatch || !isOpen()) {
        return false;
    }
    if (message.isEmpty() || message.size() > kMaxWatchFrameBytes) {
        closeLink(QStringLiteral("invalid outbound watch frame"));
        return false;
    }
    std::shared_ptr<rtc::DataChannel> channel;
    {
        std::lock_guard lock(m_bridge->mutex);
        channel = m_bridge->channel;
    }
    if (!channel) {
        return false;
    }
    try {
        if (channel->bufferedAmount() + static_cast<std::size_t>(message.size())
            > kMaxWatchOutboundBytes) {
            closeLink(QStringLiteral("watch outbound backlog exceeded"));
            return false;
        }
        channel->send(reinterpret_cast<const std::byte*>(message.constData()),
                      static_cast<std::size_t>(message.size()));
        ++m_messagesSent;
        ++m_chunksSent;
        m_telemetry.acceptedPayloadBytes += static_cast<quint64>(message.size());
        return true;
    } catch (const std::exception&) {
        closeLink(QStringLiteral("watch frame send failed"));
        return false;
    }
}

qint64 DataChannelTransport::backlogBytes() const
{
    if (!m_bridge) {
        return 0;
    }
    std::shared_ptr<rtc::DataChannel> channel;
    {
        std::lock_guard lock(m_bridge->mutex);
        channel = m_bridge->channel;
    }
    return channel ? static_cast<qint64>(channel->bufferedAmount()) : 0;
}

SessionLinkDiagnostics DataChannelTransport::linkDiagnostics() const
{
    SessionLinkDiagnostics link;
    link.buffer = SessionLinkDiagnostics::Buffer::DataChannel;
    if (!m_bridge) {
        return link;
    }
    std::shared_ptr<rtc::PeerConnection> peer;
    std::shared_ptr<rtc::DataChannel> channel;
    {
        std::lock_guard lock(m_bridge->mutex);
        peer = m_bridge->peer;
        channel = m_bridge->channel;
    }
    if (channel) {
        link.bufferedBytes = static_cast<qint64>(channel->bufferedAmount());
    }
    if (peer) {
        if (const auto rtt = peer->rtt()) {
            link.sctpRttMs = static_cast<quint32>(std::max<qint64>(0, rtt->count()));
        }
    }
    return link;
}

bool DataChannelTransport::sendRawFrameForTest(const QByteArray& frame)
{
    if (!isOpen()) {
        return false;
    }
    std::shared_ptr<rtc::DataChannel> channel;
    {
        std::lock_guard lock(m_bridge->mutex);
        channel = m_bridge->channel;
    }
    if (!channel) {
        return false;
    }
    try {
        // False only means the library buffered it.
        channel->send(reinterpret_cast<const std::byte*>(frame.constData()),
                      static_cast<std::size_t>(frame.size()));
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool DataChannelTransport::sendRawTextForTest(const QByteArray& frame)
{
    if (!isOpen()) {
        return false;
    }
    std::shared_ptr<rtc::DataChannel> channel;
    {
        std::lock_guard lock(m_bridge->mutex);
        channel = m_bridge->channel;
    }
    try {
        return channel && channel->send(frame.toStdString());
    } catch (const std::exception&) {
        return false;
    }
}

bool DataChannelTransport::openUnexpectedChannelForTest(const QString& label, bool unordered)
{
    if (!isOpen() || !m_bridge || !m_bridge->peer) {
        return false;
    }
    try {
        rtc::Reliability reliability;
        reliability.unordered = unordered;
        return static_cast<bool>(m_bridge->peer->createDataChannel(
            label.toStdString(), rtc::DataChannelInit{reliability}));
    } catch (const std::exception&) {
        return false;
    }
}

void DataChannelTransport::ping()
{
    if (m_options.purpose == Purpose::TxWatch || !isOpen()) {
        return;
    }
    std::shared_ptr<rtc::DataChannel> channel;
    {
        std::lock_guard lock(m_bridge->mutex);
        channel = m_bridge->channel;
    }
    if (!channel) {
        return;
    }
    const QByteArray frame = ControlFraming::ping(m_nextPingId);
    try {
        channel->send(reinterpret_cast<const std::byte*>(frame.constData()),
                      static_cast<std::size_t>(frame.size()));
        ++m_nextPingId;
        if (m_nextPingId == 0) {
            m_nextPingId = 1;
        }
        ++m_pingsSent;
        m_pingSentAt.start();
    } catch (const std::exception&) {
        // Closing; the close is reported.
    }
}

void DataChannelTransport::closeLink(const QString& reason)
{
    if (m_closing || !m_started) {
        return;
    }
    m_closing = true;
    qCInfo(lcControlChannel) << "Closing the control connection:" << reason;
    // Review Minor 7: the channel and then the peer close gracefully, and
    // the connection is held on its own until the peer has closed (or for
    // kCloseDrainDeadlineMs), so a transport deleted straight after this
    // still delivers what it sent (StationServer::dropPeer's session.end).
    stopPeer(/*linger=*/true);
    // As a WebSocket's close: closed() follows, from the event loop.
    QMetaObject::invokeMethod(this, [this]() { finishClose(); }, Qt::QueuedConnection);
}

void DataChannelTransport::finishClose()
{
    if (m_closedEmitted) {
        return;
    }
    m_closedEmitted = true;
    const bool wasOpen = m_open;
    m_open = false;
    if (wasOpen) {
        emit closed();
    } else {
        emit failed(QStringLiteral("the control connection closed before it opened"));
    }
}

bool DataChannelTransport::isOpen() const
{
    return m_open && !m_closing;
}

QString DataChannelTransport::peerDescription() const
{
    const std::optional<MediaIcePath> path = selectedPath();
    if (!path) {
        return QStringLiteral("control channel");
    }
    if (path->relayed()) {
        return QStringLiteral("control channel through the relay");
    }
    return QStringLiteral("control channel %1:%2").arg(path->remoteAddress).arg(path->remotePort);
}

QString DataChannelTransport::peerAddress() const
{
    if (m_peerAddressForTest) {
        return *m_peerAddressForTest;
    }
    const std::optional<MediaIcePath> path = selectedPath();
    if (!path || path->relayed() || path->remoteAddress.isEmpty()) {
        return {};
    }
    QHostAddress address(path->remoteAddress);
    bool isV4 = false;
    const quint32 v4 = address.toIPv4Address(&isV4);
    if (isV4) {
        address = QHostAddress(v4);
    }
    address.setScopeId(QString());
    return address.isNull() ? QString() : address.toString();
}

QByteArray DataChannelTransport::peerCertificateSha256() const
{
    if (m_options.role != Role::Offerer || !m_bridge || !m_bridge->peer) {
        return {};
    }
    try {
        // The fingerprint of the certificate the DTLS handshake carried
        // (libdatachannel records it in its verifier, which also holds it
        // to the SDP's), never the SDP's own claim.
        const rtc::CertificateFingerprint fingerprint = m_bridge->peer->remoteFingerprint();
        if (fingerprint.algorithm != rtc::CertificateFingerprint::Algorithm::Sha256
            || fingerprint.value.empty()) {
            return {};
        }
        QByteArray hex = QByteArray::fromStdString(fingerprint.value);
        hex.replace(':', QByteArray());
        const QByteArray digest = QByteArray::fromHex(hex);
        return digest.size() == 32 ? digest : QByteArray();
    } catch (const std::exception&) {
        return {};
    }
}

std::optional<SessionTransportTelemetry> DataChannelTransport::telemetry() const
{
    if (!isOpen()) {
        return std::nullopt;
    }
    SessionTransportTelemetry snapshot = m_telemetry;
    if (m_pongAge.isValid()) {
        snapshot.pongAgeMs = m_pongAge.elapsed();
    }
    return snapshot;
}

void DataChannelTransport::setAnswersPingsForTest(bool answers)
{
    if (m_bridge) {
        m_bridge->answersPings.store(answers, std::memory_order_relaxed);
    }
}

quint64 DataChannelTransport::pendingBytesForTest() const
{
    if (!m_bridge) {
        return 0;
    }
    std::lock_guard lock(m_bridge->mutex);
    return m_bridge->pendingBytes;
}

DataChannelTransport::Counts DataChannelTransport::countsForTest() const
{
    Counts counts;
    counts.messagesSent = m_messagesSent;
    counts.messagesDelivered = m_messagesDelivered;
    counts.pingsSent = m_pingsSent;
    counts.pongsReceived = m_pongsReceived;
    counts.chunksSent = m_chunksSent;
    if (m_bridge) {
        counts.pingsReceived = m_bridge->pingsReceived.load(std::memory_order_relaxed);
        counts.pongsSent = m_bridge->pongsSent.load(std::memory_order_relaxed);
    }
    return counts;
}

void DataChannelTransport::drain()
{
    const QPointer<DataChannelTransport> self(this);
    std::deque<Event> events;
    if (m_bridge) {
        std::lock_guard lock(m_bridge->mutex);
        m_bridge->drainPosted = false;
        events.swap(m_bridge->events);
        m_bridge->pendingBytes = 0;
    }
    for (Event& event : events) {
        if (!self || !m_bridge || m_closedEmitted
            || (m_options.purpose == Purpose::TxWatch && m_closing)) {
            return;
        }
        switch (event.kind) {
        case Event::Kind::Description:
            emit localDescription(event.first, event.second);
            if (!self) {
                return;
            }
            m_localDescriptionEmitted = true;
            gatherIfReady();
            break;
        case Event::Kind::Candidate:
            if (m_options.purpose == Purpose::Control) {
                emit localCandidate(event.first);
            } else if (m_localCandidatesForTest.size() < IMediaTransport::kMaxRemoteCandidates) {
                m_localCandidatesForTest.append(event.first);
            }
            break;
        case Event::Kind::GatheringComplete:
            emit gatheringComplete();
            break;
        case Event::Kind::Open:
            handleOpen();
            break;
        case Event::Kind::Message:
            if (isOpen()) {
                m_telemetry.receivedPayloadBytes += static_cast<quint64>(event.bytes.size());
                // Held, in order, until something listens: the Core sends
                // its hello the moment its end opens, which can be before
                // the device's end has been handed to its session.
                const QMetaMethod signal = m_options.purpose == Purpose::TxWatch
                    ? QMetaMethod::fromSignal(&SessionTransport::binaryReceived)
                    : QMetaMethod::fromSignal(&SessionTransport::textReceived);
                if (!m_held.isEmpty() || !isSignalConnected(signal)) {
                    const quint64 bytes = static_cast<quint64>(event.bytes.size());
                    const quint64 heldLimit = m_options.purpose == Purpose::TxWatch
                        ? kMaxWatchQueuedBytes : m_options.maxIncomingBytes * kMaxQueuedCaps;
                    if (m_heldBytes + bytes > heldLimit
                        || (m_options.purpose == Purpose::TxWatch
                            && m_held.size() >= kMaxWatchQueuedFrames)) {
                        // Nobody is taking them: end rather than grow.
                        qCWarning(lcControlChannel)
                            << "Ending the control connection: messages waited for a "
                               "listener past the bound";
                        closeLink(m_options.purpose == Purpose::TxWatch
                                      ? QStringLiteral("the watch channel fell behind")
                                      : QStringLiteral("the control channel fell behind"));
                        return;
                    }
                    m_held.append(event.bytes);
                    m_heldBytes += bytes;
                    scheduleHeldDelivery();
                } else {
                    ++m_messagesDelivered;
                    if (m_options.purpose == Purpose::TxWatch) {
                        m_bridge->watchDelivered(event.bytes.size());
                        emit binaryReceived(event.bytes);
                    } else {
                        // Control logging lane: how long it waited for
                        // this thread, readable while it is delivered.
                        if (event.receivedNs > 0) {
                            m_deliveringWaitUs = (steadyNowNs() - event.receivedNs) / 1000;
                        }
                        emit textReceived(event.bytes);
                        if (self) {
                            m_deliveringWaitUs.reset();
                        }
                    }
                }
            }
            break;
        case Event::Kind::Pong:
            // A pong for a ping this end sent; any other is ignored.
            if (event.id != 0 && event.id < m_nextPingId) {
                ++m_pongsReceived;
                if (m_pingSentAt.isValid()) {
                    m_telemetry.pongRttMs = static_cast<quint64>(m_pingSentAt.elapsed());
                }
                m_pongAge.start();
                emit pongReceived();
            }
            break;
        case Event::Kind::Refused:
            // As the WebSocket's cap: the connection ends.
            qCWarning(lcControlChannel) << "Ending the control connection:" << event.first;
            closeLink(event.first);
            break;
        case Event::Kind::Closed:
        case Event::Kind::Failed:
            if (!m_closing) {
                if (event.kind == Event::Kind::Failed) {
                    qCInfo(lcControlChannel) << event.first;
                }
                m_closing = true;
                stopPeer();
                finishClose();
            }
            break;
        }
        if (!self) {
            return;
        }
    }
}

void DataChannelTransport::connectNotify(const QMetaMethod& signal)
{
    SessionTransport::connectNotify(signal);
    if ((m_options.purpose == Purpose::Control
         && signal == QMetaMethod::fromSignal(&SessionTransport::textReceived))
        || (m_options.purpose == Purpose::TxWatch
            && signal == QMetaMethod::fromSignal(&SessionTransport::binaryReceived))) {
        scheduleHeldDelivery();
    }
}

void DataChannelTransport::scheduleHeldDelivery()
{
    if (m_held.isEmpty() || m_heldDeliveryPosted) {
        return;
    }
    m_heldDeliveryPosted = true;
    QMetaObject::invokeMethod(this, [this]() { deliverHeld(); }, Qt::QueuedConnection);
}

void DataChannelTransport::deliverHeld()
{
    m_heldDeliveryPosted = false;
    const QMetaMethod signal = m_options.purpose == Purpose::TxWatch
        ? QMetaMethod::fromSignal(&SessionTransport::binaryReceived)
        : QMetaMethod::fromSignal(&SessionTransport::textReceived);
    const QPointer<DataChannelTransport> self(this);
    while (!m_held.isEmpty() && isOpen() && isSignalConnected(signal)) {
        const QByteArray message = m_held.takeFirst();
        m_heldBytes -= static_cast<quint64>(message.size());
        ++m_messagesDelivered;
        if (m_options.purpose == Purpose::TxWatch) {
            m_bridge->watchDelivered(message.size());
            emit binaryReceived(message);
        } else {
            emit textReceived(message);
        }
        if (!self) {
            return;
        }
    }
}

void DataChannelTransport::handleOpen()
{
    if (m_open || m_closing) {
        return;
    }
    m_open = true;
    if (IceDiagnostics::enabled() && m_bridge && m_bridge->peer) {
        IceDiagnostics::logSelectedPair(iceDiagPath(m_options.purpose), *m_bridge->peer);
    }
    emit opened();
}

namespace {

// A connection closing on its own after its transport let it go. The
// channel's close (a stream reset) is queued behind what was sent on it,
// and the peer's close flushes the SCTP send queue and shuts SCTP down
// gracefully (libdatachannel v0.24.5 src/impl/sctptransport.cpp:423-439,
// 572-592); dropping the last reference to the peer instead tears every
// transport down at once and loses what was queued. So the peer is held
// until it reports it has closed, or until the deadline. Only ever touched
// on the thread that made it.
struct Linger {
    std::shared_ptr<rtc::PeerConnection> peer;
    std::shared_ptr<rtc::DataChannel> channel;

    // A Linger dropped without release() (its deadline timer deleted with
    // the target at the application's teardown) still clears the peer's
    // callbacks before letting it go, so the peer's destructor reports its
    // close to no one.
    ~Linger() { release(); }

    void release()
    {
        if (!peer) {
            return;
        }
        try {
            peer->resetCallbacks();
        } catch (const std::exception&) {
            // Closing what is already closing.
        }
        channel.reset();
        peer.reset();
    }
};

// Fix round 1 (LINK minor 9): where a lingering close is finished. The
// state callback runs on libdatachannel's thread, so it never reads the
// application there: it posts to `target` under `mutex`, and the
// application's teardown (a post routine, run first in QCoreApplication's
// destructor) takes `target` out under the same lock and deletes it after
// the lock is released (endLingerContext says why). A post is then either
// made to a live object or not made at all; one still queued is dropped
// with the object. The teardown also clears `routineAdded`, so the next
// target, made by a close later in the same teardown or by a later
// application, adds the post routine again and is deleted in its turn.
struct LingerContext {
    std::mutex mutex;
    std::unique_ptr<QObject> target;
    bool routineAdded = false;
};

LingerContext& lingerContext()
{
    static LingerContext context;
    return context;
}

void endLingerContext()
{
    LingerContext& context = lingerContext();
    // The target is taken under the lock and deleted after it. Deleting it
    // destroys the deadline timers it owns, and a timer holding the last
    // reference to a Linger destroys the peer, whose destructor closes it
    // and runs the state callback on this thread; that callback takes the
    // same lock. Once the target is taken, a callback finds none and posts
    // nothing, and one that already posted did so under the lock, so its
    // event is queued to the live object and dropped with it.
    //
    // Qt runs the post routines once and forgets them, so the next target
    // adds this routine again: one made by a close later in this teardown
    // (Qt runs a routine added while the routines run, qt_call_post_routines
    // in Qt 6.8.3 and 6.11), or by a later application.
    std::unique_ptr<QObject> target;
    {
        std::lock_guard lock(context.mutex);
        target = std::move(context.target);
        context.routineAdded = false;
    }
    target.reset();
}

// The context's target, made the first time and kept on the
// application's thread.
QObject* lingerTarget()
{
    if (QCoreApplication::instance() == nullptr) {
        return nullptr;
    }
    LingerContext& context = lingerContext();
    std::lock_guard lock(context.mutex);
    if (!context.target) {
        context.target = std::make_unique<QObject>();
        // It works on the application's thread, as the application did.
        context.target->moveToThread(QCoreApplication::instance()->thread());
        if (!context.routineAdded) {
            context.routineAdded = true;
            qAddPostRoutine(&endLingerContext);
        }
    }
    return context.target.get();
}

// True when the connection was handed to a Linger; false when there is
// nothing to wait for (the caller closes the peer at once).
bool lingerUntilClosed(const std::shared_ptr<rtc::PeerConnection>& peer,
                       const std::shared_ptr<rtc::DataChannel>& channel)
{
    if (!peer || !channel || !channel->isOpen()) {
        return false;
    }
    QObject* const target = lingerTarget();
    if (target == nullptr) {
        return false;
    }
    auto linger = std::make_shared<Linger>();
    linger->peer = peer;
    linger->channel = channel;
    const std::weak_ptr<Linger> weak = linger;
    peer->onStateChange([weak](rtc::PeerConnection::State state) {
        if (state != rtc::PeerConnection::State::Closed
            && state != rtc::PeerConnection::State::Failed
            && state != rtc::PeerConnection::State::Disconnected) {
            return;
        }
        LingerContext& context = lingerContext();
        std::lock_guard lock(context.mutex);
        if (!context.target) {
            return;
        }
        QMetaObject::invokeMethod(context.target.get(), [weak]() {
            if (const auto held = weak.lock()) {
                held->release();
            }
        }, Qt::QueuedConnection);
    });
    // The timer's copy holds the connection until the deadline at most.
    QTimer::singleShot(DataChannelTransport::kCloseDrainDeadlineMs, target,
                       [linger]() { linger->release(); });
    channel->resetCallbacks();
    channel->close();
    peer->close();
    return true;
}

} // namespace

bool DataChannelTransport::lingerTargetExistsForTest()
{
    LingerContext& context = lingerContext();
    std::lock_guard lock(context.mutex);
    return context.target != nullptr;
}

void DataChannelTransport::stopPeer(bool linger)
{
    m_watchRelayGrant.reset();
    if (!m_bridge) {
        return;
    }
    // The ICE transport owns the final lease. Its teardown runs after
    // PeerConnection::close() and may outlive this wrapper.
    m_candidateSourceLease.reset();
    m_pendingSourceCandidates.clear();
    m_ownedShimEndpoints.clear();
    std::shared_ptr<rtc::PeerConnection> peer;
    std::shared_ptr<rtc::DataChannel> channel;
    {
        std::lock_guard lock(m_bridge->mutex);
        m_bridge->cancelled = true;
        m_bridge->events.clear();
        m_bridge->pendingBytes = 0;
        m_bridge->watchOutstandingBytes = 0;
        m_bridge->watchOutstandingFrames = 0;
        peer = std::move(m_bridge->peer);
        channel = std::move(m_bridge->channel);
    }
    try {
        if (peer) {
            peer->resetCallbacks();
        }
        if (linger && lingerUntilClosed(peer, channel)) {
            return;
        }
        if (channel) {
            channel->resetCallbacks();
            channel->close();
        }
        if (peer) {
            // Closing the peer ends ICE, which gives back any relay
            // allocation (cmake/NereusRemoteMedia.cmake's libjuice change).
            peer->close();
        }
    } catch (const std::exception&) {
        // Closing what is already closing.
    }
    // m_bridge stays, cancelled, for the counters and for selectedPath()
    // returning nothing.
}

} // namespace NereusSDR
