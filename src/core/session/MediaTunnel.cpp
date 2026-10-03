// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/MediaTunnel.cpp  (NereusSDR)
// =================================================================
//
// See MediaTunnel.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-30: LINK-I3: one retry timer while the link is full, in place
//               of a single-shot per flush. J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
//   2026-09-27: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-29: the direct media ladder: iceFor with the Core's STUN
//               server, and directIceFor. J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
//   2026-09-29: direct media fix wave: tunnelIceFor. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/session/MediaTunnel.h"

#include "core/session/RelayLeg.h"
#include "core/session/SessionTransport.h"

#include <QLoggingCategory>
#include <QNetworkDatagram>
#include <QTimer>
#include <QThread>
#include <QUdpSocket>
#include <QUuid>

#include <algorithm>

Q_LOGGING_CATEGORY(lcMediaTunnel, "nereus.session.mediatunnel")

namespace NereusSDR {

// One media connection's use of the tunnel.
class MediaTunnelSource final : public IceConfiguration::CandidateSource {
public:
    MediaTunnelSource(std::shared_ptr<MediaTunnel> tunnel, QByteArray id)
        : m_ownerThread(tunnel ? tunnel->thread() : nullptr), m_tunnel(std::move(tunnel)), m_id(std::move(id)) {}
    ~MediaTunnelSource() override { stop(); }

    void start(std::function<void(const QString&)> add) override
    {
        if (!m_tunnel || m_started) {
            return;
        }
        quint16 port = 0;
        m_claim = m_tunnel->claim(m_id, port);
        if (m_claim == 0) {
            return;
        }
        m_started = true;
        add(RelayLeg::candidateLine(MediaTunnel::kTagMedia, port));
    }

    void stop() override
    {
        if (m_started && m_tunnel) {
            m_tunnel->release(m_id, m_claim);
        }
        m_started = false;
    }

    std::optional<NetworkPathSnapshot> networkPathSnapshot() const override
    {
        if (m_ownerThread != QThread::currentThread() || !m_started || m_claim == 0 || !m_tunnel) {
            return std::nullopt;
        }
        const QPointer<SessionTransport> transport = m_tunnel->m_transport;
        if (!transport || transport->thread() != QThread::currentThread()
            || !transport->isOpen() || !transport->carriesBinary()) {
            return std::nullopt;
        }
        std::optional<NetworkPathSnapshot> path = transport->networkPathSnapshot();
        if (path) {
            path->mediaRidesControl = true;
        }
        return path;
    }

private:
    QThread* m_ownerThread = nullptr;
    std::shared_ptr<MediaTunnel> m_tunnel;
    bool m_started = false;
    quint64 m_claim = 0;
    QByteArray m_id;
};

MediaTunnel::MediaTunnel(SessionTransport* transport)
    : m_transport(transport)
{
    m_flushRetry = new QTimer(this);
    m_flushRetry->setSingleShot(true);
    m_flushRetry->setInterval(5);
    m_flushRetry->setTimerType(Qt::PreciseTimer);
    connect(m_flushRetry, &QTimer::timeout, this, &MediaTunnel::flush);
}

MediaTunnel::~MediaTunnel() = default;

std::shared_ptr<MediaTunnel> MediaTunnel::create(SessionTransport* transport)
{
    if (transport == nullptr) {
        return nullptr;
    }
    auto* tunnel = new MediaTunnel(transport);
    connect(transport, &SessionTransport::binaryReceived, tunnel, &MediaTunnel::onBinary);
    return std::shared_ptr<MediaTunnel>(tunnel, [](MediaTunnel* gone) { gone->deleteLater(); });
}

IceConfiguration MediaTunnel::directIceFor(std::optional<IceServerAddress> stun)
{
    // No relay (so none is gathered or taken) and no candidate source: host
    // candidates and the STUN server's reflexive one only.
    IceConfiguration ice = IceConfiguration::throughRendezvous(
        {}, /*relayAllowed=*/false, IceConfiguration::localAddressFamilies(), HostFamilies{});
    ice.setRelay(std::nullopt, 1);
    ice.setStunServer(std::move(stun));
    ice.setMediaRouting(false);
    return ice;
}

IceConfiguration MediaTunnel::iceFor(std::shared_ptr<MediaTunnel> tunnel,
                                     std::optional<IceServerAddress> stun)
{
    // The direct media ladder: host candidates, the STUN server's reflexive
    // one when known, and the tunnel last; no relay.
    IceConfiguration ice = directIceFor(std::move(stun));
    ice.setCandidateSourceFactory(
        [tunnel](int lane, const QString& connectionId,
                 bool) -> std::shared_ptr<IceConfiguration::CandidateSource> {
            if (lane != IceConfiguration::kMediaLane || !tunnel) {
                return nullptr;
            }
            const QUuid uuid = QUuid::fromString(connectionId);
            if (uuid.isNull() || uuid.toString(QUuid::WithoutBraces) != connectionId) {
                return nullptr;
            }
            return std::make_shared<MediaTunnelSource>(tunnel, uuid.toRfc4122());
        },
        /*needsRelay=*/false);
    ice.setMediaRouting(true);
    return ice;
}

IceConfiguration MediaTunnel::tunnelIceFor(std::shared_ptr<MediaTunnel> tunnel)
{
    // The fallback: the tunnel's source and nothing else. No STUN server,
    // and the connection takes and sends no other candidate
    // (IceConfiguration::onlySourceCandidates).
    IceConfiguration ice = iceFor(std::move(tunnel), std::nullopt);
    ice.setOnlySourceCandidates(true);
    return ice;
}

quint64 MediaTunnel::claim(const QByteArray& id, quint16& port)
{
    if (id.size() != 16 || m_routes.contains(id) || m_routes.size() >= 3) {
        return 0;
    }
    auto* socket = new QUdpSocket(this);
    if (!socket->bind(QHostAddress::LocalHost, 0)) {
        qCWarning(lcMediaTunnel) << "The media tunnel's local socket could not be opened:"
                                 << socket->errorString();
        delete socket;
        return 0;
    }
    port = socket->localPort();
    const quint64 claim = m_nextClaim++;
    m_routes.insert(id, Route{socket, {}, 0, claim});
    connect(socket, &QUdpSocket::readyRead, this, [this, id] { readLane(id); });
    return claim;
}

void MediaTunnel::release(const QByteArray& id, quint64 claim)
{
    auto it = m_routes.find(id);
    if (it == m_routes.end() || claim != it->claim) {
        return;
    }
    it->claim = 0;
    it->agentAddress.clear();
    it->agentPort = 0;
    std::erase_if(m_queue, [&id](const QByteArray& frame) {
        return frame.size() >= 17 && frame.mid(1, 16) == id;
    });
    m_queuedBytes = 0;
    for (const QByteArray& frame : m_queue) {
        m_queuedBytes += static_cast<int>(frame.size());
    }
    QUdpSocket* const socket = it->socket;
    socket->disconnect(this);
    socket->close();
    socket->deleteLater();
    m_routes.erase(it);
}

void MediaTunnel::readLane(const QByteArray& id)
{
    auto it = m_routes.find(id);
    if (it == m_routes.end()) {
        return;
    }
    QUdpSocket* socket = it->socket;
    while (socket->hasPendingDatagrams()) {
        const QNetworkDatagram datagram = socket->receiveDatagram(kMaxDatagramBytes + 1);
        if (it->claim == 0) {
            continue;
        }
        if (!datagram.senderAddress().isLoopback()) {
            continue;
        }
        const QByteArray payload = datagram.data();
        if (payload.isEmpty()) {
            continue;
        }
        if (payload.size() > kMaxDatagramBytes - 16) {
            ++m_droppedOversize;
            continue;
        }
        const QHostAddress sender = datagram.senderAddress();
        const quint16 senderPort = static_cast<quint16>(datagram.senderPort());
        if (it->agentPort != 0
            && (it->agentAddress != sender || it->agentPort != senderPort)) {
            ++m_droppedWrongSender;
            continue;
        }
        if (it->agentPort == 0) {
            it->agentAddress = sender;
            it->agentPort = senderPort;
        }
        QByteArray frame;
        frame.reserve(1 + 16 + payload.size());
        frame.append(static_cast<char>(kTagMedia));
        frame.append(id);
        frame.append(payload);
        m_queue.push_back(frame);
        m_queuedBytes += static_cast<int>(frame.size());
        while (static_cast<int>(m_queue.size()) > kQueueFrames || m_queuedBytes > kQueueBytes) {
            m_queuedBytes -= static_cast<int>(m_queue.front().size());
            m_queue.pop_front();
            ++m_droppedQueue;
        }
    }
    flush();
}

void MediaTunnel::flush()
{
    if (m_transport.isNull()) {
        m_queue.clear();
        m_queuedBytes = 0;
        m_flushRetry->stop();
        return;
    }
    ++m_flushPasses;
    while (!m_queue.empty() && m_transport->backlogBytes() < kWriteLimitBytes) {
        const QByteArray frame = m_queue.front();
        m_queue.pop_front();
        m_queuedBytes -= static_cast<int>(frame.size());
        if (m_transport->sendBinary(frame)) {
            ++m_sent;
        }
    }
    if (!m_queue.empty()) {
        // The socket is full: try again shortly (datagrams keep coming and
        // the oldest go first). One retry at a time: a datagram that
        // arrives meanwhile flushes now and leaves the pending retry be.
        if (!m_flushRetry->isActive()) {
            m_flushRetry->start();
        }
    } else {
        m_flushRetry->stop();
    }
}

void MediaTunnel::onBinary(const QByteArray& message)
{
    if (message.isEmpty() || static_cast<quint8>(message.at(0)) != kTagMedia) {
        return; // another tag, or nothing: not this tunnel's
    }
    if (message.size() > kMaxDatagramBytes + 1) {
        ++m_droppedOversize;
        return;
    }
    if (message.size() < 2) {
        ++m_droppedNoRoute;
        return;
    }
    if (message.size() <= 17) {
        ++m_droppedNoRoute;
        return;
    }
    const auto it = m_routes.constFind(message.mid(1, 16));
    if (it == m_routes.cend() || it->claim == 0 || it->agentPort == 0) {
        ++m_droppedNoRoute;
        return;
    }
    it->socket->writeDatagram(message.constData() + 17, message.size() - 17,
                              it->agentAddress, it->agentPort);
    ++m_delivered;
}

} // namespace NereusSDR
