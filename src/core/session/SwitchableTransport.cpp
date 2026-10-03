// =================================================================
// src/core/session/SwitchableTransport.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original.
//
// iPhone app plan Task 29 (R-IOS-16): see SwitchableTransport.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-30: LINK-I2: a station that hears no path.switch in time ends
//               the session instead of dropping what was in flight. J.J.
//               Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-27: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-10-01: Control logging lane: linkDiagnostics() and
//               deliveringMessageWaitUs() from the connection in use, for
//               the Core's log only. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
// =================================================================

#include "core/session/SwitchableTransport.h"

#include "core/session/SessionMessages.h"

#include <QLoggingCategory>
#include <QTimer>
#include <QThread>

namespace NereusSDR {

Q_LOGGING_CATEGORY(lcSwitchable, "nereus.session.switch")

SwitchableTransport::SwitchableTransport(SessionTransport* inner, Side side,
                                         quint64 maxIncomingBytes, QObject* parent)
    : SessionTransport(parent)
    , m_side(side)
    , m_maxHeldBytes(maxIncomingBytes * kMaxHeldCaps)
{
    m_deadline = new QTimer(this);
    m_deadline->setSingleShot(true);
    m_deadline->setInterval(kSwitchDeadlineMs);
    connect(m_deadline, &QTimer::timeout, this, [this] {
        if (m_state == State::ClientAwaitingBarrier) {
            abortClientSwitch(QStringLiteral("no path.switch from the Core in time"));
        } else if (m_state == State::StationAwaitingBarrier) {
            // The device's barrier never came: what it sent on the old
            // connection after the last message read there may be in
            // flight, and closing the old connection would drop it with no
            // error to either end. End the session instead, as a lost link,
            // so the device reconnects and takes a fresh snapshot rather
            // than carrying on with state the Core never saw.
            qCWarning(lcSwitchable) << "No path.switch from the device in time;"
                                    << "ending the session";
            closeLink(QStringLiteral("no path.switch from the device in time"));
        }
    });
    m_oldClose = new QTimer(this);
    m_oldClose->setSingleShot(true);
    m_oldClose->setInterval(kOldCloseMs);
    connect(m_oldClose, &QTimer::timeout, this, [this] { retireOld(/*closeNow=*/true); });

    if (inner != nullptr) {
        m_sendOn = inner;
        m_readFrom = inner;
        attach(inner);
    }
}

SwitchableTransport::~SwitchableTransport() = default;

void SwitchableTransport::setSwitchDeadlineMsForTest(int ms)
{
    m_deadline->setInterval(ms);
}

void SwitchableTransport::attach(SessionTransport* transport)
{
    transport->setParent(this);
    connect(transport, &SessionTransport::textReceived, this,
            [this, transport](const QByteArray& wire) { onText(transport, wire); });
    connect(transport, &SessionTransport::pongReceived, this,
            &SessionTransport::pongReceived);
    // Step 2b: the media tunnel's binary messages pass through, whichever
    // connection brings them (datagrams need no order across a move).
    connect(transport, &SessionTransport::binaryReceived, this,
            &SessionTransport::binaryReceived);
    connect(transport, &SessionTransport::closed, this,
            [this, transport]() { onClosed(transport); });
}

void SwitchableTransport::detach(SessionTransport* transport)
{
    if (transport == nullptr) {
        return;
    }
    disconnect(transport, nullptr, this, nullptr);
}

SessionTransport* SwitchableTransport::takeInner()
{
    if (switching() || m_sendOn.isNull()) {
        return nullptr;
    }
    SessionTransport* inner = m_sendOn;
    detach(inner);
    inner->setParent(nullptr);
    m_sendOn = nullptr;
    m_readFrom = nullptr;
    m_closed = true;
    m_closedEmitted = true;
    return inner;
}

bool SwitchableTransport::beginStationSwitch(SessionTransport* next)
{
    if (next == nullptr || switching() || m_closed || m_sendOn.isNull()) {
        return false;
    }
    SessionTransport* old = m_sendOn;
    // The barrier is the last message on the old connection.
    old->sendText(SessionMessages::encode(SessionMessages::pathSwitch()));
    attach(next);
    m_next = next;
    m_sendOn = next;
    m_readFrom = old;
    m_held.clear();
    m_heldBytes = 0;
    m_state = State::StationAwaitingBarrier;
    m_deadline->start();
    qCInfo(lcSwitchable) << "Moving a session from" << old->peerDescription() << "to"
                         << next->peerDescription();
    return true;
}

bool SwitchableTransport::beginClientSwitch(SessionTransport* next)
{
    if (next == nullptr || switching() || m_closed || m_sendOn.isNull()) {
        return false;
    }
    attach(next);
    m_next = next;
    m_held.clear();
    m_heldBytes = 0;
    m_state = State::ClientAwaitingBarrier;
    m_deadline->start();
    qCInfo(lcSwitchable) << "Waiting for the Core to move the session to"
                         << next->peerDescription();
    return true;
}

bool SwitchableTransport::isPathSwitch(const QByteArray& wire)
{
    // Cheap first: a session message that is not the barrier almost never
    // names it.
    if (!wire.contains("path.switch")) {
        return false;
    }
    SessionMessage message;
    return SessionMessages::decode(wire, &message)
        && message.kind == SessionMessageKind::PathSwitch;
}

void SwitchableTransport::onText(SessionTransport* from, const QByteArray& wire)
{
    if (m_closed) {
        return;
    }
    if (from == m_readFrom) {
        if (isPathSwitch(wire)) {
            if (m_state == State::StationAwaitingBarrier) {
                finishSwitch();
            } else if (m_state == State::ClientAwaitingBarrier) {
                // The Core's barrier: answer it on the old connection, the
                // last message this end sends there, then send on the new.
                from->sendText(SessionMessages::encode(SessionMessages::pathSwitch()));
                m_sendOn = m_next;
                finishSwitch();
            } else {
                qCInfo(lcSwitchable) << "A path.switch arrived outside a move; dropped";
            }
            return;
        }
        const QPointer<SwitchableTransport> self(this);
        m_delivering = from;
        emit textReceived(wire);
        if (self) {
            m_delivering = nullptr;
        }
        return;
    }
    if (from == m_next && switching()) {
        hold(wire);
        return;
    }
    // Anything else: the old connection after the move (nothing more is
    // expected there), or a connection this object no longer reads.
}

void SwitchableTransport::hold(const QByteArray& wire)
{
    m_heldBytes += static_cast<quint64>(wire.size());
    if (m_heldBytes > m_maxHeldBytes) {
        qCWarning(lcSwitchable) << "Too much arrived on the new connection during a move;"
                                << "ending the session";
        closeLink(QStringLiteral("too much held during a move"));
        return;
    }
    m_held.append(wire);
}

void SwitchableTransport::finishSwitch()
{
    m_deadline->stop();
    SessionTransport* old = m_readFrom;
    m_readFrom = m_next;
    m_next = nullptr;
    m_state = State::Idle;
    ++m_switches;
    if (old != nullptr && old != m_readFrom) {
        m_old = old;
        if (m_side == Side::Station) {
            // The device's barrier was the last thing it sends there.
            retireOld(/*closeNow=*/true);
        } else {
            // The Core closes the old connection once our barrier reaches
            // it; close our end then, or after kOldCloseMs.
            m_oldClose->start();
        }
    }
    const QList<QByteArray> held = std::exchange(m_held, {});
    m_heldBytes = 0;
    qCInfo(lcSwitchable) << "The session moved to" << peerDescription();
    const QPointer<SwitchableTransport> self(this);
    emit switched();
    for (const QByteArray& wire : held) {
        if (!self || m_closed) {
            return;
        }
        emit textReceived(wire);
    }
}

void SwitchableTransport::abortClientSwitch(const QString& why)
{
    if (m_state != State::ClientAwaitingBarrier) {
        return;
    }
    m_deadline->stop();
    m_state = State::Idle;
    SessionTransport* next = m_next;
    m_next = nullptr;
    m_held.clear();
    m_heldBytes = 0;
    if (next != nullptr) {
        detach(next);
        next->closeLink(QStringLiteral("move given up"));
        next->deleteLater();
    }
    qCInfo(lcSwitchable) << "The move was given up:" << why;
    emit switchFailed(why);
}

void SwitchableTransport::retireOld(bool closeNow)
{
    m_oldClose->stop();
    SessionTransport* old = m_old;
    m_old = nullptr;
    if (old == nullptr) {
        return;
    }
    detach(old);
    if (closeNow) {
        old->closeLink(QStringLiteral("the session moved to another connection"));
    }
    old->deleteLater();
}

void SwitchableTransport::onClosed(SessionTransport* from)
{
    if (m_closed) {
        return;
    }
    if (from == m_old) {
        // The old connection after a move: the Core closed it, as it
        // should. Nothing to tell the session.
        retireOld(/*closeNow=*/false);
        return;
    }
    switch (m_state) {
    case State::StationAwaitingBarrier:
        if (from == m_readFrom) {
            // The old connection went before the device's barrier: count it
            // as the barrier.
            qCInfo(lcSwitchable) << "The old connection closed during a move";
            finishSwitch();
            return;
        }
        break; // the new connection closed: the session is gone
    case State::ClientAwaitingBarrier:
        if (from == m_next) {
            abortClientSwitch(QStringLiteral("the new connection closed"));
            return;
        }
        if (from == m_readFrom) {
            // The old connection went before the Core's barrier came: the
            // Core may have moved already. Carry on over the new one; if
            // the Core had not taken the join, it ends that one.
            qCInfo(lcSwitchable) << "The old connection closed before the Core moved the"
                                 << "session; carrying on over the new one";
            m_sendOn = m_next;
            finishSwitch();
            return;
        }
        break;
    case State::Idle:
        if (from != m_readFrom && from != m_sendOn) {
            return;
        }
        break;
    }
    emitClosedOnce();
}

void SwitchableTransport::emitClosedOnce()
{
    if (m_closedEmitted) {
        return;
    }
    m_closed = true;
    m_closedEmitted = true;
    m_deadline->stop();
    emit closed();
}

void SwitchableTransport::sendText(const QByteArray& wire)
{
    if (m_closed || m_sendOn.isNull()) {
        return;
    }
    m_sendOn->sendText(wire);
}

void SwitchableTransport::ping()
{
    if (m_closed || m_sendOn.isNull()) {
        return;
    }
    m_sendOn->ping();
}

void SwitchableTransport::closeLink(const QString& reason)
{
    if (m_closed && m_closedEmitted && m_sendOn.isNull()) {
        return;
    }
    m_deadline->stop();
    m_oldClose->stop();
    const QList<QPointer<SessionTransport>> all{m_sendOn, m_readFrom, m_next, m_old};
    m_state = State::Idle;
    m_next = nullptr;
    m_held.clear();
    m_heldBytes = 0;
    for (const QPointer<SessionTransport>& transport : all) {
        if (transport && transport != m_sendOn && transport != m_readFrom) {
            detach(transport);
            transport->closeLink(reason);
            transport->deleteLater();
        }
    }
    if (m_old) {
        m_old = nullptr;
    }
    // The connection the session runs on closes the way it always has,
    // and its closed() reaches the session through onClosed().
    if (m_readFrom && m_readFrom != m_sendOn) {
        SessionTransport* reading = m_readFrom;
        m_readFrom = m_sendOn;
        detach(reading);
        reading->closeLink(reason);
        reading->deleteLater();
    }
    if (m_sendOn) {
        m_readFrom = m_sendOn;
        m_sendOn->closeLink(reason);
    }
}

bool SwitchableTransport::isOpen() const
{
    return !m_closed && m_sendOn && m_sendOn->isOpen();
}

QString SwitchableTransport::peerDescription() const
{
    return m_sendOn ? m_sendOn->peerDescription() : QString();
}

QString SwitchableTransport::peerAddress() const
{
    return m_sendOn ? m_sendOn->peerAddress() : QString();
}

QByteArray SwitchableTransport::peerCertificateSha256() const
{
    return m_sendOn ? m_sendOn->peerCertificateSha256() : QByteArray();
}

std::optional<SessionTransportTelemetry> SwitchableTransport::telemetry() const
{
    return m_sendOn ? m_sendOn->telemetry() : std::nullopt;
}

std::optional<NetworkPathSnapshot> SwitchableTransport::networkPathSnapshot() const
{
    if (thread() != QThread::currentThread() || m_closed) {
        return std::nullopt;
    }
    const QPointer<SessionTransport> current = m_sendOn;
    if (!current || !current->isOpen()) {
        return std::nullopt;
    }
    return current->networkPathSnapshot();
}

} // namespace NereusSDR
