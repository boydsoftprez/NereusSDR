#pragma once
// =================================================================
// src/core/session/SwitchableTransport.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original.
//
// iPhone app plan Task 29 (R-IOS-16; the pairing design, section 5.4; the
// link document, section 21.2): the transport beneath one session, which
// can move the session to another connection without ending it.
//
// StationServer and StationClient hold this, never the connection itself,
// for every session: it carries the session over one connection (its
// "inner" transport, a WebSocketTransport, a DataChannelTransport or a
// test's loopback) and forwards everything both ways, so from the
// session's side nothing changes. A move ("switch") is make-before-break
// and ordered by an in-band barrier, path.switch, the last message each end
// sends on the old connection:
//
//   Station (beginStationSwitch): the device's new connection has joined.
//     path.switch goes out on the old connection, and from then on the
//     session's messages go out on the new one. The old connection is
//     still read until the device's own path.switch arrives on it; what
//     the new connection brings meanwhile is held, in order. Then the old
//     connection closes and the held messages are delivered.
//
//   Client (beginClientSwitch): path.join has gone out on the new
//     connection. The session carries on over the old one, and what the
//     new one brings is held, until the station's path.switch arrives on
//     the old one. Then this end sends its own path.switch there, sends on
//     the new connection from then on, and delivers the held messages. The
//     old connection closes once the station has closed it, or after
//     kOldCloseMs.
//
// Messages keep their order across a move in both directions: each end
// reads the old connection up to the other's barrier and only then the new
// one. The barrier itself never reaches the session. Pings go out on the
// connection this end sends on; a pong on either counts.
//
// Failures: an old connection that closes before the barrier it waits for
// counts as that barrier. A client that hears no barrier within
// kSwitchDeadlineMs, or whose new connection closes first, gives the move
// up (switchFailed()) and stays on the old connection; a station that
// hears none within kSwitchDeadlineMs ends the session (closed()), since
// messages in flight on the old connection would otherwise be lost
// silently. Once the move is done, the new connection closing ends the
// session as any connection closing does (closed()). What is held is
// bounded (kMaxHeldCaps times the inbound cap); past it the session ends.
//
// A path.switch outside a move is dropped here (logged), never delivered.
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

#include "core/session/SessionTransport.h"

#include <QByteArray>
#include <QList>
#include <QPointer>
#include <QString>

QT_BEGIN_NAMESPACE
class QTimer;
QT_END_NAMESPACE

namespace NereusSDR {

class SwitchableTransport : public SessionTransport {
    Q_OBJECT

public:
    enum class Side {
        Station, ///< the Core's end: it sends the first path.switch
        Client,  ///< the device's end: it answers the station's
    };

    /// How long a client waits for the station's path.switch after its
    /// path.join, and how long a station waits for the client's after its
    /// own (the link document, section 21.2).
    static constexpr int kSwitchDeadlineMs = 10000;
    /// How long a client keeps its end of the old connection open after
    /// its own path.switch, waiting for the station to close it.
    static constexpr int kOldCloseMs = 5000;
    /// What may be held from the new connection during a move: this many
    /// times the inbound cap, as DataChannelTransport bounds its queues.
    static constexpr quint64 kMaxHeldCaps = 4;

    /// Takes `inner` (reparents it onto this object). `maxIncomingBytes` is
    /// the end's inbound cap (StationServer::kMaxIncomingMessageBytes or
    /// StationClient::kMaxIncomingMessageBytes), which bounds what a move
    /// holds.
    SwitchableTransport(SessionTransport* inner, Side side, quint64 maxIncomingBytes,
                        QObject* parent = nullptr);
    ~SwitchableTransport() override;

    SwitchableTransport(const SwitchableTransport&) = delete;
    SwitchableTransport& operator=(const SwitchableTransport&) = delete;

    /// The connection the session sends on now (the new one as soon as
    /// this end has sent its path.switch). Never null until takeInner().
    SessionTransport* inner() const { return m_sendOn; }
    /// Detaches the connection this object carries, before any move, and
    /// hands it back unparented; this object is empty after it and emits
    /// nothing more. What StationServer does to a connection that joins
    /// another session. Null during a move.
    SessionTransport* takeInner();

    /// True from beginStationSwitch()/beginClientSwitch() until the move is
    /// done or given up.
    bool switching() const { return m_state != State::Idle; }
    /// Moves that completed on this session.
    int switches() const { return m_switches; }

    /// Station: `next` (a joined connection, unparented) takes over.
    /// False (and `next` untouched) while a move is already under way or
    /// this object is closed.
    bool beginStationSwitch(SessionTransport* next);
    /// Client: `next` (path.join already sent on it) takes over once the
    /// station's path.switch arrives. False as beginStationSwitch().
    bool beginClientSwitch(SessionTransport* next);

    // ---- SessionTransport ----
    void sendText(const QByteArray& wire) override;
    void ping() override;
    void closeLink(const QString& reason) override;
    bool isOpen() const override;
    QString peerDescription() const override;
    QString peerAddress() const override;
    QByteArray peerCertificateSha256() const override;
    std::optional<SessionTransportTelemetry> telemetry() const override;
    std::optional<NetworkPathSnapshot> networkPathSnapshot() const override;
    bool sendBinary(const QByteArray& message) override
    {
        return m_sendOn && m_sendOn->sendBinary(message);
    }
    bool carriesBinary() const override { return m_sendOn && m_sendOn->carriesBinary(); }
    qint64 backlogBytes() const override { return m_sendOn ? m_sendOn->backlogBytes() : 0; }
    SessionLinkDiagnostics linkDiagnostics() const override
    {
        return m_sendOn ? m_sendOn->linkDiagnostics() : SessionLinkDiagnostics{};
    }
    /// The connection whose message is being delivered says.
    std::optional<qint64> deliveringMessageWaitUs() const override
    {
        return m_delivering ? m_delivering->deliveringMessageWaitUs() : std::nullopt;
    }

    /// Test seam: the move's deadline (kSwitchDeadlineMs).
    void setSwitchDeadlineMsForTest(int ms);

    /// True when `wire` is exactly a path.switch message.
    static bool isPathSwitch(const QByteArray& wire);

signals:
    /// The move is done: the session runs on the new connection.
    void switched();
    /// Client: the move was given up; the session stays on the old
    /// connection. `why` is for the log.
    void switchFailed(const QString& why);

private:
    enum class State {
        Idle,
        StationAwaitingBarrier, ///< sent our path.switch; reading old
        ClientAwaitingBarrier,  ///< path.join sent on next; sending and reading old
    };

    void attach(SessionTransport* transport);
    void detach(SessionTransport* transport);
    void onText(SessionTransport* from, const QByteArray& wire);
    void onClosed(SessionTransport* from);
    void hold(const QByteArray& wire);
    /// The barrier this end waited for came (or the old connection is
    /// gone): read the new connection from now on.
    void finishSwitch();
    void abortClientSwitch(const QString& why);
    void retireOld(bool closeNow);
    void emitClosedOnce();

    Side m_side;
    quint64 m_maxHeldBytes = 0;
    State m_state = State::Idle;
    /// The connection the session sends on.
    QPointer<SessionTransport> m_sendOn;
    /// The connection the session reads (the old one during a move).
    QPointer<SessionTransport> m_readFrom;
    /// During a move: the new connection (station: m_sendOn already).
    QPointer<SessionTransport> m_next;
    /// After a move: the old connection, until it has closed.
    QPointer<SessionTransport> m_old;
    /// Control logging lane: set only while a message read from it is
    /// re-emitted.
    QPointer<SessionTransport> m_delivering;
    QList<QByteArray> m_held;
    quint64 m_heldBytes = 0;
    QTimer* m_deadline = nullptr;
    QTimer* m_oldClose = nullptr;
    int m_switches = 0;
    bool m_closed = false;
    bool m_closedEmitted = false;
};

} // namespace NereusSDR
