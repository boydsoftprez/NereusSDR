// no-port-check: NereusSDR-original auxiliary session protocol.
#include "core/session/TxWatchServer.h"

#include "core/safety/RemoteTxWatchdog.h"
#include "core/session/SessionTransport.h"

#include <QTimer>

#include <algorithm>
#include <chrono>
#include <utility>

namespace NereusSDR {

TxWatchServer::TxWatchServer(Current current, Deliver deliver, QObject* parent, Clock clock)
    : QObject(parent), m_current(std::move(current)), m_deliver(std::move(deliver)),
      m_clock(std::move(clock))
{}

TxWatchServer::~TxWatchServer()
{
    retireAll();
}

qint64 TxWatchServer::now() const
{
    if (m_clock) {
        return m_clock();
    }
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

bool TxWatchServer::equalTicket(const QByteArray& left, const QByteArray& right)
{
    if (left.size() != kTicketBytes || right.size() != kTicketBytes) {
        return false;
    }
    quint8 difference = 0;
    for (int i = 0; i < kTicketBytes; ++i) {
        difference |= static_cast<quint8>(left.at(i) ^ right.at(i));
    }
    return difference == 0;
}

bool TxWatchServer::live(const Binding& binding) const
{
    const Current current = m_current;
    return binding.primary && current
        && current(binding.primary, binding.sessionId, binding.deviceId, binding.generation);
}

std::optional<TxWatchServer::Ticket> TxWatchServer::issue(
    SessionTransport* primary, quint64 sessionId, const QByteArray& deviceId,
    quint64 generation, const QByteArray& random32)
{
    if (primary == nullptr || sessionId == 0 || deviceId.isEmpty() || generation == 0
        || random32.size() != kTicketBytes || !m_current) {
        return std::nullopt;
    }
    const QPointer<TxWatchServer> self(this);
    const Current current = m_current;
    const bool eligible = current(primary, sessionId, deviceId, generation);
    if (!self || !eligible) {
        return std::nullopt;
    }
    const qint64 time = now();
    // Expiry frees capacity even when no attach socket has appeared.
    for (auto it = m_bindings.begin(); it != m_bindings.end();) {
        if (!it->ticket.isEmpty() && time >= it->deadlineMs) {
            it->ticket.clear();
        }
        if (it->ticket.isEmpty() && !it->auxiliary) {
            it = m_bindings.erase(it);
        } else {
            ++it;
        }
    }
    if (m_bindings.contains(primary) || m_bindings.size() >= kMaxBindings) {
        return std::nullopt;
    }
    if (m_lastIssued.contains(primary) && time - m_lastIssued.value(primary) < kIssueIntervalMs) {
        return std::nullopt;
    }
    for (const Binding& existing : std::as_const(m_bindings)) {
        if (!existing.ticket.isEmpty() && equalTicket(existing.ticket, random32)) {
            return std::nullopt;
        }
    }
    Binding binding;
    binding.primary = primary;
    binding.sessionId = sessionId;
    binding.deviceId = deviceId;
    binding.generation = generation;
    binding.ticket = random32;
    binding.deadlineMs = time + kTicketLifetimeMs;
    m_bindings.insert(primary, binding);
    m_lastIssued.insert(primary, time);
    return Ticket{random32};
}

int TxWatchServer::pendingSocketCount() const
{
    int count = 0;
    for (const Socket& socket : m_sockets) {
        if (!socket.primary) {
            ++count;
        }
    }
    return count;
}

bool TxWatchServer::hasLiveBinding(SessionTransport* primary, quint64 generation) const
{
    const auto binding = m_bindings.constFind(primary);
    return binding != m_bindings.cend() && binding->generation == generation
        && ((binding->auxiliary && binding->auxiliary->isOpen())
            || (!binding->ticket.isEmpty() && now() < binding->deadlineMs));
}

bool TxWatchServer::acceptTransport(SessionTransport* auxiliary, const QString& addressGroup)
{
    if (auxiliary == nullptr) {
        return false;
    }
    auxiliary->setParent(this);
    int sameAddress = 0;
    for (const Socket& socket : m_sockets) {
        if (!socket.primary && socket.addressGroup == addressGroup) {
            ++sameAddress;
        }
    }
    if (pendingSocketCount() >= kMaxPendingSockets || sameAddress >= kMaxPendingPerAddress
        || !auxiliary->isOpen()) {
        const QPointer<SessionTransport> guard(auxiliary);
        auxiliary->closeLink(QStringLiteral("watch attachment unavailable"));
        if (guard) {
            guard->deleteLater();
        }
        return false;
    }
    Socket socket;
    socket.transport = auxiliary;
    socket.addressGroup = addressGroup;
    socket.deadlineMs = now() + kAttachDeadlineMs;
    m_sockets.insert(auxiliary, socket);
    const QPointer<TxWatchServer> self(this);
    connect(auxiliary, &SessionTransport::binaryReceived, this,
            [self, auxiliary](const QByteArray& bytes) {
        if (self) {
            self->onBinary(auxiliary, bytes);
        }
    });
    connect(auxiliary, &SessionTransport::textReceived, this,
            [self, auxiliary](const QByteArray&) {
        if (self) {
            self->retireSocket(auxiliary, true);
        }
    });
    connect(auxiliary, &SessionTransport::closed, this, [self, auxiliary]() {
        if (self) {
            self->retireSocket(auxiliary, false);
        }
    });
    connect(auxiliary, &QObject::destroyed, this, [self, auxiliary]() {
        if (self) {
            self->retireSocket(auxiliary, false);
        }
    });
    const QPointer<SessionTransport> auxiliaryGuard(auxiliary);
    QTimer::singleShot(kAttachDeadlineMs, this, [self, auxiliaryGuard]() {
        if (self && auxiliaryGuard && self->m_sockets.contains(auxiliaryGuard.data())
            && !self->m_sockets.value(auxiliaryGuard.data()).primary) {
            self->retireSocket(auxiliaryGuard.data(), true);
        }
    });
    return true;
}

void TxWatchServer::onBinary(SessionTransport* auxiliary, const QByteArray& message)
{
    const QPointer<TxWatchServer> self(this);
    const QPointer<SessionTransport> auxiliaryGuard(auxiliary);
    auto socket = m_sockets.find(auxiliary);
    if (socket == m_sockets.end() || !socket->transport) {
        return;
    }
    const qint64 time = now();
    if (!socket->primary) {
        if (time >= socket->deadlineMs || message.size() != kAttachBytes
            || static_cast<quint8>(message.at(0)) != 1) {
            retireSocket(auxiliary, true);
            return;
        }
        const QByteArray candidate = message.mid(1);
        SessionTransport* matched = nullptr;
        // Scan all four bounded bindings. Only a matched ticket is consumed;
        // malformed guesses cannot burn another device's ticket.
        for (auto it = m_bindings.begin(); it != m_bindings.end(); ++it) {
            if (!it->ticket.isEmpty() && equalTicket(candidate, it->ticket)) {
                matched = it.key();
            }
        }
        if (matched == nullptr) {
            retireSocket(auxiliary, true);
            return;
        }
        auto binding = m_bindings.find(matched);
        binding->ticket.clear(); // first matched attempt consumes it
        const Binding candidateBinding = *binding;
        const bool eligible = time < candidateBinding.deadlineMs
            && !candidateBinding.auxiliary && live(candidateBinding);
        if (!self) {
            return;
        }
        socket = m_sockets.find(auxiliary);
        binding = m_bindings.find(matched);
        if (socket == m_sockets.end() || binding == m_bindings.end()) {
            return;
        }
        // backlogBytes() is supplied by the transport and may itself invoke
        // callbacks. Never retain an iterator across it.
        const qint64 backlog = auxiliaryGuard ? auxiliaryGuard->backlogBytes()
                                              : kMaxOutboundBacklog;
        if (!self) {
            return;
        }
        socket = m_sockets.find(auxiliary);
        binding = m_bindings.find(matched);
        if (socket == m_sockets.end() || binding == m_bindings.end()) {
            return;
        }
        if (!eligible || !auxiliaryGuard || backlog + 2 > kMaxOutboundBacklog
            || binding->generation != candidateBinding.generation
            || !binding->ticket.isEmpty() || binding->auxiliary || socket->primary) {
            if (binding != m_bindings.end() && !binding->auxiliary) {
                m_bindings.erase(binding);
            }
            retireSocket(auxiliary, true);
            return;
        }
        socket->primary = matched;
        socket->generation = binding->generation;
        socket->refilledMs = time;
        binding->auxiliary = auxiliary;
        const bool ackSent = auxiliaryGuard->sendBinary(QByteArray::fromHex("0100"));
        if (!self) {
            return;
        }
        if (!ackSent || !auxiliaryGuard) {
            retireSocket(auxiliary, true);
        }
        return;
    }
    auto binding = m_bindings.find(socket->primary);
    if (binding == m_bindings.end() || binding->auxiliary != auxiliary
        || binding->generation != socket->generation) {
        retireSocket(auxiliary, true);
        return;
    }
    const Binding currentBinding = *binding;
    const bool eligible = live(currentBinding);
    if (!self) {
        return;
    }
    socket = m_sockets.find(auxiliary);
    binding = m_bindings.find(currentBinding.primary);
    if (socket == m_sockets.end() || binding == m_bindings.end()
        || binding->auxiliary != auxiliary
        || binding->generation != currentBinding.generation || !eligible) {
        retireSocket(auxiliary, true);
        return;
    }
    const qint64 elapsed = std::max<qint64>(0, time - socket->refilledMs);
    socket->tokens = std::min<double>(kBurstFrames,
                                     socket->tokens + elapsed * (kFramesPerSecond / 1000.0));
    socket->refilledMs = time;
    if (socket->tokens < 1.0 || message.size() != RemoteTxWatchdog::kChannelKeepaliveBytes) {
        retireSocket(auxiliary, true);
        return;
    }
    socket->tokens -= 1.0;
    quint64 sequence = 0;
    quint32 epoch = 0;
    if (!RemoteTxWatchdog::readChannelKeepalive(message, &sequence, &epoch)) {
        retireSocket(auxiliary, true);
        return;
    }
    const QByteArray deviceId = binding->deviceId;
    const Deliver deliver = m_deliver;
    deliver(deviceId, sequence, epoch);
}

void TxWatchServer::retireSocket(SessionTransport* auxiliary, bool close)
{
    auto socket = m_sockets.find(auxiliary);
    if (socket == m_sockets.end()) {
        return;
    }
    const QPointer<SessionTransport> guard = socket->transport;
    SessionTransport* primary = socket->primary;
    m_sockets.erase(socket); // callbacks from close may reenter
    if (primary != nullptr) {
        auto binding = m_bindings.find(primary);
        if (binding != m_bindings.end() && binding->auxiliary == auxiliary) {
            m_bindings.erase(binding);
        }
    }
    if (guard) {
        if (close) {
            guard->closeLink(QStringLiteral("watch attachment ended"));
        }
        if (guard) {
            guard->deleteLater();
        }
    }
}

void TxWatchServer::retire(SessionTransport* primary, bool primaryEnded)
{
    auto binding = m_bindings.find(primary);
    if (binding == m_bindings.end()) {
        if (primaryEnded) {
            m_lastIssued.remove(primary);
        }
        return;
    }
    SessionTransport* auxiliary = binding->auxiliary;
    m_bindings.erase(binding); // invalidate before close callback
    if (primaryEnded) {
        m_lastIssued.remove(primary);
    }
    if (auxiliary) {
        retireSocket(auxiliary, true);
    }
}

void TxWatchServer::retireAll()
{
    const QPointer<TxWatchServer> self(this);
    const QList<SessionTransport*> primaries = m_bindings.keys();
    for (SessionTransport* primary : primaries) {
        retire(primary);
        if (!self) {
            return;
        }
    }
    const QList<SessionTransport*> sockets = m_sockets.keys();
    for (SessionTransport* auxiliary : sockets) {
        retireSocket(auxiliary, true);
        if (!self) {
            return;
        }
    }
    m_lastIssued.clear();
}

} // namespace NereusSDR
