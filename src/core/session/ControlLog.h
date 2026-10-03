#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/ControlLog.h  (NereusSDR)
// =================================================================
//
// Control logging lane: the Core's log of each device's control traffic,
// so a phone control that acts late or snaps back can be traced on the
// bench. Logging only: nothing here changes a message, its handling, its
// order or what is sent, and nothing reads another thread's state.
//
// What it logs, per device (its id in hex, as the transmit lines do):
//   * one line per property.write and command.invoke, with the time the
//     transport received it, the time the main thread took it from the
//     queue, and the time since the device's previous control message;
//   * one line per answer, with accepted or the reason and the handling
//     time; accepted answers under kSlowAnswerMs for the same object and
//     properties are folded into one line a second, while every refusal
//     and every slow answer keeps its own line;
//   * a gap of kGapLogMs or more between two control messages, with both
//     messages' names and the link's buffers;
//   * the link the session runs on (carrier, selected candidate pair,
//     buffers, TCP figures on Linux, the media tunnel's counters), when it
//     changes and at each heartbeat;
//   * a gap of kKeepaliveGapLogMs or more between two transmit keepalives
//     on one channel while the device is watched, naming the channel (the
//     media connection's "tx" data channel, the control link, or the
//     transmit-watch link), kept apart from the control gap lines;
//   * (for DaemonMediaController) the media connection's selected pair;
//   * a transmit watchdog stop, with the channel and age of the last
//     keepalive and the control state it happened in.
//
// Before a device signs in only message kinds are logged. Names a device
// sends are logged only when they match a strict pattern; reasons have
// control characters replaced, so nothing a device sends can start a new
// line in the log. Per-device token buckets and a Core-wide cap bound the
// volume; what is skipped is counted and the count is logged at most once
// per kGapWindowMs while messages come, at each heartbeat and when the
// connection ends.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-01: Created (control logging lane). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QByteArray>
#include <QElapsedTimer>
#include <QHash>
#include <QList>
#include <QString>

#include <array>
#include <functional>
#include <optional>

namespace NereusSDR {

class MediaTunnel;
class SessionTransport;
struct MediaIcePath;
struct SessionMessage;

class ControlLog {
public:
    /// A gap between two control messages from one device at or over this
    /// is logged.
    static constexpr qint64 kGapLogMs = 250;
    /// At most this many gap lines per device in each gap window.
    static constexpr int kGapLinesPerWindow = 5;
    static constexpr qint64 kGapWindowMs = 10000;
    /// Each device's bucket: a burst of this many messages, refilled at
    /// kRefillPerSecond (a full-speed slider drag still logs every write).
    static constexpr int kBurstMessages = 50;
    static constexpr int kRefillPerSecond = 20;
    /// At most this many control lines a second from all devices together.
    static constexpr int kCoreLinesPerSecond = 100;
    static constexpr qint64 kCoreWindowMs = 1000;
    /// An answer this slow or slower keeps its own line, with the link.
    static constexpr qint64 kSlowAnswerMs = 50;
    /// Fast accepted answers for one object and properties fold into one
    /// line per this window.
    static constexpr qint64 kFoldWindowMs = 1000;
    /// The link is looked at no more than this often per device.
    static constexpr qint64 kLinkCheckMs = 1000;
    /// Writes and commands waiting for their answers, per device.
    static constexpr int kPendingLimit = 256;
    static constexpr qint64 kPendingStaleMs = 60000;
    /// A name a device sends is logged only when it is 1 to this many of
    /// A-Z, a-z, 0-9 and . _ : / -.
    static constexpr int kMaxNameChars = 64;

    /// A keepalive gap on one channel at or over this, while watched, is
    /// logged (the phone sends one every 100 ms).
    static constexpr qint64 kKeepaliveGapLogMs = 250;

    using Clock = std::function<qint64()>;

    /// Which channel a transmit keepalive came on.
    enum class KeepaliveChannel {
        /// The tx.keepalive verb on the device's control link.
        Control,
        /// The media connection's "tx" data channel (unordered, never
        /// retransmitted).
        MediaTx,
        /// A separately authenticated transmit-watch link.
        TxWatch,
    };

    /// What the server knows of the device behind a transport.
    struct PeerInfo {
        QByteArray deviceId;
        bool signedIn = false;
        /// Brought by the remote access service (a rendezvous session).
        bool introduced = false;
        const MediaTunnel* mediaTunnel = nullptr;
    };

    ControlLog();

    /// Milliseconds; a test sets its own. Unset, a monotonic clock.
    void setClock(Clock clock);
    qint64 now() const;

    /// Every message a transport delivered, as the server takes it.
    void inbound(SessionTransport* transport, const PeerInfo& peer,
                 const SessionMessage& message);
    /// Every message the server hands a device's transport.
    void answer(SessionTransport* transport, const PeerInfo& peer,
                const SessionMessage& message);
    /// The heartbeat: skipped counts, folded answers and the link.
    void tick(SessionTransport* transport, const PeerInfo& peer);
    /// Once per heartbeat for the Core-wide skipped count.
    void tickCore();
    /// The connection ended: its counts and folded answers, then forgotten.
    void closed(SessionTransport* transport, const PeerInfo& peer);
    /// The transmit watchdog began watching `deviceId` (a key or VOX):
    /// keepalive gaps are measured afresh.
    void keepaliveWatchStarted(const QByteArray& deviceId);
    /// One transmit keepalive from `deviceId` on `channel`, `ageMs` after
    /// its transport received it, `watched` when the watchdog watched the
    /// device as it came. `transport` and `peer` are the device's control
    /// session when the server knows it (transport may be null).
    void keepalive(const QByteArray& deviceId, KeepaliveChannel channel, qint64 ageMs,
                   bool watched, SessionTransport* transport, const PeerInfo& peer);
    /// The transmit watchdog stopped `deviceId`'s key. `transport` and
    /// `peer` are its control session when known (transport may be null).
    void watchdogStopped(const QByteArray& deviceId, SessionTransport* transport,
                         const PeerInfo& peer, bool linkClosed, qint64 silentMs);

    /// `name` as the log may print it, or "an unrecognized name".
    static QString safeName(const QByteArray& name);
    /// `text` with every control or line-breaking character a space.
    static QString safeText(const QString& text);
    /// An address as the log prints one: IPv4 "*.*.*. N", IPv6 "*:last".
    static QString maskedAddress(const QString& address);
    /// A media connection's selected pair as the log prints it: direct or
    /// relayed, each candidate's type and transport, masked addresses and
    /// ports.
    static QString mediaPathText(const MediaIcePath& path);

private:
    struct Pending {
        qint64 receivedMs = 0;
        bool logged = false;
    };
    struct Fold {
        qint64 startMs = 0;
        int count = 0;
        qint64 slowestMs = 0;
        quint32 firstWriteId = 0;
        quint32 lastWriteId = 0;
        QString object;
        QString properties;
    };
    struct PeerState {
        qint64 lastInMs = -1;
        std::optional<qint64> lastReceiptUs;
        QString lastInName;
        QHash<QString, QList<Pending>> pending;
        int pendingCount = 0;
        double tokens = kBurstMessages;
        qint64 refillMs = -1;
        int skippedMessages = 0;
        qint64 gapWindowStartMs = -1;
        int gapLinesInWindow = 0;
        int skippedGaps = 0;
        /// When the skipped counts were last looked at (-1: never).
        qint64 skippedFlushedMs = -1;
        QHash<QString, Fold> folds;
        qint64 linkCheckedMs = -1;
        QString linkLogged;
        bool noDelayLogged = false;
    };
    struct KeepaliveState {
        /// When each channel's last keepalive was received (-1: none).
        std::array<qint64, 3> lastHeardMs{-1, -1, -1};
        KeepaliveChannel lastChannel = KeepaliveChannel::Control;
        qint64 lastHeardAnyMs = -1;
        qint64 gapWindowStartMs = -1;
        int gapLinesInWindow = 0;
        int skippedGaps = 0;
    };

    bool takeLine();
    void flushCoreSkipped();
    void flushSkipped(const QString& device, PeerState& state);
    void flushFolds(const QString& device, PeerState& state, bool all);
    void logFold(const QString& device, const Fold& fold);
    void checkLink(SessionTransport* transport, const PeerInfo& peer, PeerState& state,
                   bool always);
    QString linkText(SessionTransport* transport, const PeerInfo& peer) const;
    static QString device(const PeerInfo& peer);
    static QString describe(const SessionMessage& message, bool signedIn);
    static QString pendingKey(const SessionMessage& message);
    static QString channelName(KeepaliveChannel channel);
    void flushKeepaliveSkipped(const QByteArray& deviceId, KeepaliveState& state);

    Clock m_clock;
    QElapsedTimer m_monotonic;
    QHash<const SessionTransport*, PeerState> m_peers;
    QHash<QByteArray, KeepaliveState> m_keepalives;
    qint64 m_coreWindowStartMs = -1;
    int m_coreLines = 0;
    int m_coreSkipped = 0;
};

} // namespace NereusSDR
