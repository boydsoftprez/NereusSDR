#pragma once
// no-port-check: NereusSDR-original auxiliary session protocol.

#include <QByteArray>
#include <QHash>
#include <QPointer>
#include <QObject>
#include <QString>

#include <functional>
#include <optional>

namespace NereusSDR {

class SessionTransport;

/// Owns only auxiliary transports and their one-use tickets. The station
/// supplies the live primary authority and the existing watchdog sink.
class TxWatchServer final : public QObject {
public:
    static constexpr const char* kPath = "/tx-watch/v1";
    static constexpr int kTicketBytes = 32;
    static constexpr int kAttachBytes = 33;
    static constexpr int kTicketLifetimeMs = 10000;
    static constexpr int kIssueIntervalMs = 1000;
    static constexpr int kAttachDeadlineMs = 5000;
    static constexpr int kMaxBindings = 4;
    static constexpr int kMaxPendingSockets = 8;
    static constexpr int kMaxPendingPerAddress = 8;
    static constexpr int kBurstFrames = 20;
    static constexpr int kFramesPerSecond = 50;
    static constexpr int kMaxOutboundBacklog = 4096;

    struct Ticket {
        QByteArray value; // Raw OS-random bytes; caller encodes canonical base64url.
        int expiresInMs = kTicketLifetimeMs;
        QString path = QString::fromLatin1(kPath);
    };

    using Clock = std::function<qint64()>;
    using Current = std::function<bool(SessionTransport*, quint64, const QByteArray&, quint64)>;
    using Deliver = std::function<void(const QByteArray&, quint64, quint32)>;

    explicit TxWatchServer(Current current, Deliver deliver, QObject* parent = nullptr,
                           Clock clock = {});
    ~TxWatchServer() override;

    std::optional<Ticket> issue(SessionTransport* primary, quint64 sessionId,
                                const QByteArray& deviceId, quint64 generation,
                                const QByteArray& random32);
    /// The caller must cap each inbound message/frame to kAttachBytes before
    /// this method returns to the event loop. The transport is adopted here.
    bool acceptTransport(SessionTransport* auxiliary, const QString& addressGroup);
    void retire(SessionTransport* primary, bool primaryEnded = false);
    void retireAll();

    int bindingCount() const { return m_bindings.size(); }
    /// A pending ticket or attached auxiliary for this exact primary
    /// generation. Admission-grant expiry does not retire either one.
    bool hasLiveBinding(SessionTransport* primary, quint64 generation) const;
    int pendingSocketCount() const;

private:
    struct Binding {
        QPointer<SessionTransport> primary;
        quint64 sessionId = 0;
        QByteArray deviceId;
        quint64 generation = 0;
        QByteArray ticket;
        qint64 deadlineMs = 0;
        QPointer<SessionTransport> auxiliary;
    };
    struct Socket {
        QPointer<SessionTransport> transport;
        QString addressGroup;
        qint64 deadlineMs = 0;
        QPointer<SessionTransport> primary;
        quint64 generation = 0;
        double tokens = kBurstFrames;
        qint64 refilledMs = 0;
    };

    qint64 now() const;
    void onBinary(SessionTransport* auxiliary, const QByteArray& message);
    void retireSocket(SessionTransport* auxiliary, bool close);
    bool live(const Binding& binding) const;
    static bool equalTicket(const QByteArray& left, const QByteArray& right);

    Current m_current;
    Deliver m_deliver;
    Clock m_clock;
    QHash<SessionTransport*, Binding> m_bindings;
    QHash<SessionTransport*, Socket> m_sockets;
    QHash<SessionTransport*, qint64> m_lastIssued;
};

} // namespace NereusSDR
