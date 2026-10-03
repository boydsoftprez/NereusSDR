// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/ControlLog.cpp  (NereusSDR)
// =================================================================
//
// Control logging lane: see ControlLog.h. Logging only.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-01: Created (control logging lane). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/session/ControlLog.h"

#include "core/session/MediaTunnel.h"
#include "core/session/NetworkPathSnapshot.h"
#include "core/session/SessionMessages.h"
#include "core/session/SessionTransport.h"
#include "core/session/media/IMediaTransport.h"

#include <QDateTime>
#include <QHostAddress>
#include <QLoggingCategory>
#include <QStringList>

#include <algorithm>
#include <utility>

namespace NereusSDR {

namespace {

// The station's own category name, so the station's log filter covers it.
Q_LOGGING_CATEGORY(lcControlLog, "nereus.station")

QString clockTime(qint64 msAgo)
{
    return QDateTime::currentDateTime().addMSecs(-msAgo).toString(QStringLiteral("HH:mm:ss.zzz"));
}

QString milliseconds(qint64 us)
{
    return QString::number(static_cast<double>(us) / 1000.0, 'f', 1);
}

bool nameByte(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.'
        || c == '_' || c == ':' || c == '/' || c == '-';
}

QString candidateTypes(const NetworkPathSnapshot& path)
{
    const auto type = [](const QString& value) {
        return value.isEmpty() ? QStringLiteral("not known") : ControlLog::safeName(value.toUtf8());
    };
    return QStringLiteral("candidates local %1, remote %2")
        .arg(type(path.localCandidateType), type(path.remoteCandidateType));
}

} // namespace

ControlLog::ControlLog()
{
    m_monotonic.start();
}

void ControlLog::setClock(Clock clock)
{
    m_clock = std::move(clock);
}

qint64 ControlLog::now() const
{
    return m_clock ? m_clock() : m_monotonic.elapsed();
}

QString ControlLog::safeName(const QByteArray& name)
{
    if (name.isEmpty() || name.size() > kMaxNameChars
        || !std::all_of(name.cbegin(), name.cend(), nameByte)) {
        return QStringLiteral("an unrecognized name");
    }
    return QString::fromLatin1(name);
}

QString ControlLog::safeText(const QString& text)
{
    constexpr qsizetype kMaxTextChars = 300;
    QString out = text.left(kMaxTextChars);
    for (QChar& c : out) {
        const QChar::Category category = c.category();
        if (category == QChar::Other_Control || category == QChar::Other_Format
            || category == QChar::Separator_Line || category == QChar::Separator_Paragraph
            || category == QChar::Other_Surrogate || category == QChar::Other_NotAssigned) {
            c = QLatin1Char(' ');
        }
    }
    return out;
}

QString ControlLog::maskedAddress(const QString& address)
{
    if (address.isEmpty()) {
        return QStringLiteral("none");
    }
    const QHostAddress parsed(address);
    if (parsed.protocol() == QAbstractSocket::IPv4Protocol) {
        return QStringLiteral("*.*.*. %1").arg(parsed.toIPv4Address() & 0xFFu);
    }
    if (parsed.protocol() == QAbstractSocket::IPv6Protocol) {
        const QString text = parsed.toString();
        return QStringLiteral("*:") + text.section(QLatin1Char(':'), -1);
    }
    return QStringLiteral("an unrecognized address");
}

QString ControlLog::mediaPathText(const MediaIcePath& path)
{
    const auto candidate = [](const QString& type, const QString& transport) {
        QString text = type.isEmpty() ? QStringLiteral("not known") : safeName(type.toUtf8());
        if (!transport.isEmpty()) {
            text += QLatin1Char(' ') + safeName(transport.toUtf8());
        }
        return text;
    };
    QString text = path.relayed() ? QStringLiteral("relayed pair") : QStringLiteral("direct pair");
    if (path.viaLoopbackShim()) {
        text += QStringLiteral(" through the loopback relay shim");
    }
    text += QStringLiteral(", candidates local %1, remote %2, local %3 port %4, remote %5 port %6")
                .arg(candidate(path.localType, path.localTransport),
                     candidate(path.remoteType, path.remoteTransport),
                     maskedAddress(path.localAddress))
                .arg(path.localPort)
                .arg(maskedAddress(path.remoteAddress))
                .arg(path.remotePort);
    return text;
}

QString ControlLog::device(const PeerInfo& peer)
{
    return peer.deviceId.isEmpty() ? QStringLiteral("a device not signed in")
                                   : QString::fromLatin1(peer.deviceId.toHex());
}

QString ControlLog::describe(const SessionMessage& message, bool signedIn)
{
    const QString kind = QString::fromLatin1(SessionMessages::kindName(message.kind));
    if (!signedIn) {
        return kind;
    }
    if (message.kind == SessionMessageKind::CommandInvoke) {
        return safeName(message.commandVerb);
    }
    if (message.kind == SessionMessageKind::PropertyWrite) {
        QStringList properties;
        for (const MirrorUpdate& update : message.updates) {
            properties.append(safeName(update.name));
        }
        return QStringLiteral("%1 %2 %3")
            .arg(kind, safeName(message.objectKey), properties.join(QLatin1Char(',')));
    }
    return kind;
}

QString ControlLog::pendingKey(const SessionMessage& message)
{
    const bool write = message.kind == SessionMessageKind::PropertyWrite
        || message.kind == SessionMessageKind::PropertyResult;
    return write ? QStringLiteral("w:%1").arg(message.writeId)
                 : QStringLiteral("c:%1:%2")
                       .arg(QString::fromLatin1(message.commandVerb.toHex()))
                       .arg(message.commandId);
}

bool ControlLog::takeLine()
{
    const qint64 at = now();
    if (m_coreWindowStartMs < 0 || at - m_coreWindowStartMs >= kCoreWindowMs) {
        flushCoreSkipped();
        m_coreWindowStartMs = at;
        m_coreLines = 0;
    }
    if (m_coreLines < kCoreLinesPerSecond) {
        ++m_coreLines;
        return true;
    }
    ++m_coreSkipped;
    return false;
}

void ControlLog::flushCoreSkipped()
{
    const int skipped = std::exchange(m_coreSkipped, 0);
    if (skipped > 0) {
        qCInfo(lcControlLog).noquote()
            << QStringLiteral("Control log: %1 control lines from all devices not logged "
                              "(over %2 a second)")
                   .arg(skipped)
                   .arg(kCoreLinesPerSecond);
    }
}

void ControlLog::flushSkipped(const QString& who, PeerState& state)
{
    const int messages = std::exchange(state.skippedMessages, 0);
    const int gaps = std::exchange(state.skippedGaps, 0);
    if (messages > 0 || gaps > 0) {
        qCInfo(lcControlLog).noquote()
            << QStringLiteral("Control log for %1: %2 control messages with their answers and %3 "
                              "gaps not logged (over this device's limit)")
                   .arg(who)
                   .arg(messages)
                   .arg(gaps);
    }
}

void ControlLog::logFold(const QString& who, const Fold& fold)
{
    if (fold.count <= 0 || !takeLine()) {
        return;
    }
    qCInfo(lcControlLog).noquote()
        << QStringLiteral("Control out to %1: property.result %2 %3, %4 more accepted answers "
                          "under %5 ms each (writes %6 to %7), slowest %8 ms")
               .arg(who, fold.object, fold.properties)
               .arg(fold.count)
               .arg(kSlowAnswerMs)
               .arg(fold.firstWriteId)
               .arg(fold.lastWriteId)
               .arg(fold.slowestMs);
}

void ControlLog::flushFolds(const QString& who, PeerState& state, bool all)
{
    const qint64 at = now();
    for (auto fold = state.folds.begin(); fold != state.folds.end();) {
        if (all || at - fold->startMs >= kFoldWindowMs) {
            logFold(who, *fold);
            fold = state.folds.erase(fold);
        } else {
            ++fold;
        }
    }
}

QString ControlLog::linkText(SessionTransport* transport, const PeerInfo& peer) const
{
    const SessionLinkDiagnostics link = transport->linkDiagnostics();
    QString text;
    switch (link.buffer) {
    case SessionLinkDiagnostics::Buffer::DataChannel:
        text = QStringLiteral("control data channel buffer %1 bytes").arg(link.bufferedBytes);
        break;
    case SessionLinkDiagnostics::Buffer::WebSocket:
        text = QStringLiteral("WebSocket send buffer in Qt %1 bytes").arg(link.bufferedBytes);
        break;
    case SessionLinkDiagnostics::Buffer::Unknown:
        text = QStringLiteral("send backlog %1 bytes").arg(link.bufferedBytes);
        break;
    }
    if (link.sctpRttMs) {
        text += QStringLiteral(", SCTP rtt %1 ms").arg(*link.sctpRttMs);
    }
    if (link.tcpRttUs) {
        text += QStringLiteral(", TCP rtt %1 ms").arg(milliseconds(*link.tcpRttUs));
    }
    if (link.tcpUnacked) {
        text += QStringLiteral(", %1 segments unacknowledged").arg(*link.tcpUnacked);
    }
    if (link.tcpRetransmits) {
        text += QStringLiteral(", %1 retransmits in all").arg(*link.tcpRetransmits);
    }
    if (link.tcpNotSentBytes) {
        text += QStringLiteral(", %1 bytes not sent by the kernel").arg(*link.tcpNotSentBytes);
    }
    if (peer.mediaTunnel != nullptr) {
        text += QStringLiteral(", media tunnel %1 datagrams sent, %2 dropped with its queue full")
                    .arg(peer.mediaTunnel->datagramsSent())
                    .arg(peer.mediaTunnel->droppedQueueFull());
    }
    return text;
}

void ControlLog::checkLink(SessionTransport* transport, const PeerInfo& peer, PeerState& state,
                          bool always)
{
    const qint64 at = now();
    if (!always && state.linkCheckedMs >= 0 && at - state.linkCheckedMs < kLinkCheckMs) {
        return;
    }
    state.linkCheckedMs = at;
    const std::optional<NetworkPathSnapshot> snapshot = transport->networkPathSnapshot();
    QString path = peer.introduced ? QStringLiteral("through the remote access service")
                                   : QStringLiteral("direct to the Core");
    if (!snapshot) {
        path += QStringLiteral(", path not known");
    } else {
        const NetworkPathSnapshot& p = *snapshot;
        path += p.carrier == NetworkPathSnapshot::Carrier::WebSocket ? QStringLiteral(", WebSocket")
            : p.carrier == NetworkPathSnapshot::Carrier::WebRelay
                ? QStringLiteral(", web relay WebSocket")
                : QStringLiteral(", control data channel");
        path += p.kind == NetworkPathSnapshot::Kind::Direct    ? QStringLiteral(", direct pair")
            : p.kind == NetworkPathSnapshot::Kind::Relayed ? QStringLiteral(", relayed pair")
                                                           : QStringLiteral(", pair not known");
        if (p.endpoints == NetworkPathSnapshot::Endpoints::IceCandidates) {
            path += QStringLiteral(", ") + candidateTypes(p);
        }
        path += QStringLiteral(", local %1 port %2, remote %3 port %4")
                    .arg(maskedAddress(p.localAddress))
                    .arg(p.localPort)
                    .arg(maskedAddress(p.remoteAddress))
                    .arg(p.remotePort);
        if (p.mediaRidesControl) {
            path += QStringLiteral(", media rides this link");
        }
    }
    if (!always && path == state.linkLogged) {
        return;
    }
    const bool changed = !state.linkLogged.isEmpty() && path != state.linkLogged;
    state.linkLogged = path;
    QString noDelay;
    if (!state.noDelayLogged) {
        const SessionLinkDiagnostics link = transport->linkDiagnostics();
        if (link.noDelay) {
            noDelay = *link.noDelay ? QStringLiteral(", TCP_NODELAY on")
                                    : QStringLiteral(", TCP_NODELAY off");
            state.noDelayLogged = true;
        }
    }
    if (!takeLine()) {
        return;
    }
    qCInfo(lcControlLog).noquote()
        << QStringLiteral("Control link for %1%2: %3; %4%5")
               .arg(device(peer),
                    changed ? QStringLiteral(" (changed)") : QString(), path,
                    linkText(transport, peer), noDelay);
}

void ControlLog::inbound(SessionTransport* transport, const PeerInfo& peer,
                         const SessionMessage& message)
{
    PeerState& state = m_peers[transport];
    const qint64 at = now();
    const QString who = device(peer);
    const QString name = describe(message, peer.signedIn);
    const std::optional<qint64> waitUs = transport->deliveringMessageWaitUs();
    const std::optional<qint64> receiptUs = waitUs
        ? std::optional<qint64>(at * 1000 - std::max<qint64>(0, *waitUs))
        : std::nullopt;
    const qint64 sincePrevious = state.lastInMs >= 0 ? at - state.lastInMs : -1;

    // The gap since this device's last control message, rate-limited.
    if (sincePrevious >= kGapLogMs) {
        if (state.gapWindowStartMs < 0 || at - state.gapWindowStartMs >= kGapWindowMs) {
            state.gapWindowStartMs = at;
            state.gapLinesInWindow = 0;
        }
        if (state.gapLinesInWindow >= kGapLinesPerWindow) {
            ++state.skippedGaps;
        } else {
            ++state.gapLinesInWindow;
            if (takeLine()) {
                qCInfo(lcControlLog).noquote()
                    << QStringLiteral("Control gap from %1: %2 ms between taking %3 and %4 from "
                                      "the queue, %5; %6")
                           .arg(who)
                           .arg(sincePrevious)
                           .arg(state.lastInName, name,
                                receiptUs && state.lastReceiptUs
                                    ? QStringLiteral("%1 ms between their receipts")
                                          .arg(milliseconds(*receiptUs - *state.lastReceiptUs))
                                    : QStringLiteral("receipt times not measured on this link"),
                                linkText(transport, peer));
            }
        }
    }
    state.lastInMs = at;
    state.lastReceiptUs = receiptUs;
    state.lastInName = name;
    flushFolds(who, state, false);
    // Skipped counts, at most once per gap window while messages come.
    if (state.skippedFlushedMs < 0) {
        state.skippedFlushedMs = at;
    } else if (at - state.skippedFlushedMs >= kGapWindowMs) {
        state.skippedFlushedMs = at;
        flushSkipped(who, state);
    }

    if (!peer.signedIn) {
        return;
    }
    checkLink(transport, peer, state, false);

    // One line per write and command. The transmit keepalive (ten a second
    // while keyed) is left to the gap line.
    const bool write = message.kind == SessionMessageKind::PropertyWrite;
    const bool command = message.kind == SessionMessageKind::CommandInvoke
        && message.commandVerb != "tx.keepalive";
    if (!write && !command) {
        return;
    }
    if (state.refillMs < 0) {
        state.refillMs = at;
    }
    state.tokens = std::min<double>(
        kBurstMessages,
        state.tokens + static_cast<double>(at - state.refillMs) * kRefillPerSecond / 1000.0);
    state.refillMs = at;
    const bool logged = state.tokens >= 1.0;
    if (logged) {
        state.tokens -= 1.0;
    } else {
        ++state.skippedMessages;
    }
    // A write with writeId 0 is never answered.
    if (command || message.writeId != 0) {
        if (state.pendingCount >= kPendingLimit) {
            for (auto list = state.pending.begin(); list != state.pending.end();) {
                const qsizetype before = list->size();
                list->removeIf(
                    [at](const Pending& p) { return at - p.receivedMs >= kPendingStaleMs; });
                state.pendingCount -= static_cast<int>(before - list->size());
                list = list->isEmpty() ? state.pending.erase(list) : std::next(list);
            }
            if (state.pendingCount >= kPendingLimit) {
                state.pending.clear();
                state.pendingCount = 0;
            }
        }
        state.pending[pendingKey(message)].append(Pending{at, logged});
        ++state.pendingCount;
    }
    if (!logged || !takeLine()) {
        return;
    }
    const QString times = waitUs
        ? QStringLiteral("received by the transport at %1, taken from the queue at %2 (%3 ms "
                         "later)")
              .arg(clockTime(*waitUs / 1000), clockTime(0), milliseconds(*waitUs))
        : QStringLiteral("taken by the main thread at %1 (no receipt time for this message)")
              .arg(clockTime(0));
    const QString previous = sincePrevious < 0
        ? QStringLiteral("the device's first control message")
        : QStringLiteral("%1 ms after the previous control message").arg(sincePrevious);
    if (write) {
        qCInfo(lcControlLog).noquote()
            << QStringLiteral("Control in from %1: %2, write %3, %4, %5")
                   .arg(who, name)
                   .arg(message.writeId)
                   .arg(times, previous);
    } else {
        qCInfo(lcControlLog).noquote()
            << QStringLiteral("Control in from %1: command.invoke %2, command %3, %4, %5")
                   .arg(who, name)
                   .arg(message.commandId)
                   .arg(times, previous);
    }
}

void ControlLog::answer(SessionTransport* transport, const PeerInfo& peer,
                        const SessionMessage& message)
{
    const bool write = message.kind == SessionMessageKind::PropertyResult;
    if (!write && message.kind != SessionMessageKind::CommandResult) {
        return;
    }
    const auto state = m_peers.find(transport);
    if (state == m_peers.end()) {
        return;
    }
    const auto list = state->pending.find(pendingKey(message));
    if (list == state->pending.end() || list->isEmpty()) {
        return;
    }
    const Pending entry = list->takeFirst();
    --state->pendingCount;
    if (list->isEmpty()) {
        state->pending.erase(list);
    }
    if (!entry.logged) {
        return;
    }
    const qint64 at = now();
    const qint64 handledMs = at - entry.receivedMs;
    const bool slow = handledMs >= kSlowAnswerMs;
    const QString who = device(peer);
    if (!write) {
        if (!takeLine()) {
            return;
        }
        qCInfo(lcControlLog).noquote()
            << QStringLiteral("Control out to %1: command.result %2, command %3, %4, handled in "
                              "%5 ms%6")
                   // One multi-argument arg: a "%1" in the reason stays text.
                   .arg(who, safeName(message.commandVerb), QString::number(message.commandId),
                        message.accepted
                            ? QStringLiteral("accepted")
                            : QStringLiteral("refused (") + safeText(message.reason)
                                  + QLatin1Char(')'),
                        QString::number(handledMs),
                        slow ? QStringLiteral("; ") + linkText(transport, peer) : QString());
        return;
    }
    QStringList properties;
    QStringList refused;
    for (const SessionPropertyResult& result : message.propertyResults) {
        properties.append(safeName(result.property));
        if (!result.accepted) {
            refused.append(safeName(result.property) + QStringLiteral(": ")
                           + safeText(result.reason));
        }
    }
    const QString object = safeName(message.objectKey);
    const QString names = properties.join(QLatin1Char(','));
    if (refused.isEmpty() && !slow) {
        // Fast accepted answers for one object and properties: one line,
        // then the rest folded until the window ends.
        const QString key = object + QLatin1Char(' ') + names;
        auto fold = state->folds.find(key);
        if (fold != state->folds.end() && at - fold->startMs < kFoldWindowMs) {
            if (fold->count == 0) {
                fold->firstWriteId = message.writeId;
            }
            ++fold->count;
            fold->lastWriteId = message.writeId;
            fold->slowestMs = std::max(fold->slowestMs, handledMs);
            return;
        }
        if (fold != state->folds.end()) {
            logFold(who, *fold);
            state->folds.erase(fold);
        }
        Fold started;
        started.startMs = at;
        started.object = object;
        started.properties = names;
        state->folds.insert(key, started);
    }
    if (!takeLine()) {
        return;
    }
    qCInfo(lcControlLog).noquote()
        << QStringLiteral("Control out to %1: property.result %2 %3, write %4, %5, handled in "
                          "%6 ms%7")
               // One multi-argument arg: a "%1" in a reason stays text.
               .arg(who, object, names, QString::number(message.writeId),
                    refused.isEmpty()
                        ? QStringLiteral("accepted")
                        : QStringLiteral("refused (") + refused.join(QStringLiteral("; "))
                              + QLatin1Char(')'),
                    QString::number(handledMs),
                    slow ? QStringLiteral("; ") + linkText(transport, peer) : QString());
}

void ControlLog::tick(SessionTransport* transport, const PeerInfo& peer)
{
    const auto state = m_peers.find(transport);
    if (state == m_peers.end()) {
        return;
    }
    const QString who = device(peer);
    flushFolds(who, *state, true);
    flushSkipped(who, *state);
    if (peer.signedIn) {
        checkLink(transport, peer, *state, true);
    }
}

void ControlLog::tickCore()
{
    for (auto it = m_keepalives.begin(); it != m_keepalives.end(); ++it) {
        flushKeepaliveSkipped(it.key(), it.value());
    }
    flushCoreSkipped();
}

void ControlLog::closed(SessionTransport* transport, const PeerInfo& peer)
{
    // The device's keepalive state stays (a device has one entry, and its
    // next connection may carry on its watch); its skipped count is logged.
    if (const auto keepalives = m_keepalives.find(peer.deviceId);
        !peer.deviceId.isEmpty() && keepalives != m_keepalives.end()) {
        flushKeepaliveSkipped(peer.deviceId, *keepalives);
    }
    const auto state = m_peers.find(transport);
    if (state == m_peers.end()) {
        return;
    }
    const QString who = device(peer);
    flushFolds(who, *state, true);
    flushSkipped(who, *state);
    m_peers.erase(state);
}

QString ControlLog::channelName(KeepaliveChannel channel)
{
    switch (channel) {
    case KeepaliveChannel::Control:
        return QStringLiteral("the control link (tx.keepalive)");
    case KeepaliveChannel::MediaTx:
        return QStringLiteral("the media connection's tx data channel");
    case KeepaliveChannel::TxWatch:
        return QStringLiteral("the transmit-watch link");
    }
    return QStringLiteral("an unknown channel");
}

void ControlLog::flushKeepaliveSkipped(const QByteArray& deviceId, KeepaliveState& state)
{
    const int skipped = std::exchange(state.skippedGaps, 0);
    if (skipped > 0) {
        qCInfo(lcControlLog).noquote()
            << QStringLiteral("Keepalive log for %1: %2 keepalive gaps not logged (over this "
                              "device's limit)")
                   .arg(QString::fromLatin1(deviceId.toHex()))
                   .arg(skipped);
    }
}

void ControlLog::keepaliveWatchStarted(const QByteArray& deviceId)
{
    KeepaliveState& state = m_keepalives[deviceId];
    state.lastHeardMs.fill(-1);
    state.lastHeardAnyMs = -1;
}

void ControlLog::keepalive(const QByteArray& deviceId, KeepaliveChannel channel, qint64 ageMs,
                           bool watched, SessionTransport* transport, const PeerInfo& peer)
{
    if (deviceId.isEmpty()) {
        return;
    }
    KeepaliveState& state = m_keepalives[deviceId];
    const qint64 at = now();
    const qint64 heard = at - std::max<qint64>(0, ageMs);
    const auto index = static_cast<std::size_t>(channel);
    const qint64 previous = state.lastHeardMs[index];
    const qint64 previousAny = state.lastHeardAnyMs;
    const KeepaliveChannel previousChannel = state.lastChannel;
    if (!watched) {
        // Not watched: no key or VOX to measure; the next watch starts
        // afresh.
        state.lastHeardMs.fill(-1);
        state.lastHeardAnyMs = -1;
        return;
    }
    state.lastHeardMs[index] = std::max(previous, heard);
    if (heard >= previousAny) {
        state.lastHeardAnyMs = heard;
        state.lastChannel = channel;
    }
    if (previous < 0 || heard - previous < kKeepaliveGapLogMs) {
        return;
    }
    if (state.gapWindowStartMs < 0 || at - state.gapWindowStartMs >= kGapWindowMs) {
        flushKeepaliveSkipped(deviceId, state);
        state.gapWindowStartMs = at;
        state.gapLinesInWindow = 0;
    }
    if (state.gapLinesInWindow >= kGapLinesPerWindow) {
        ++state.skippedGaps;
        return;
    }
    ++state.gapLinesInWindow;
    if (!takeLine()) {
        return;
    }
    QString other;
    if (previousAny >= 0 && previousChannel != channel) {
        other = QStringLiteral("the last keepalive before it came on %1, %2 ms earlier")
                    .arg(channelName(previousChannel))
                    .arg(heard - previousAny);
    } else {
        other = QStringLiteral("no keepalive on another channel in between");
    }
    QString link = QStringLiteral("no control session known");
    if (transport != nullptr) {
        link = QStringLiteral("control link: ") + linkText(transport, peer);
    }
    qCInfo(lcControlLog).noquote()
        << QStringLiteral("Keepalive gap from %1 on %2: %3 ms between two keepalives as the "
                          "transport received them (a transmit keepalive, not a control "
                          "message); %4; %5")
               .arg(QString::fromLatin1(deviceId.toHex()), channelName(channel))
               .arg(heard - previous)
               .arg(other, link);
}

void ControlLog::watchdogStopped(const QByteArray& deviceId, SessionTransport* transport,
                                 const PeerInfo& peer, bool linkClosed, qint64 silentMs)
{
    const qint64 at = now();
    QString keepalive = QStringLiteral("no keepalive heard in this watch");
    if (const auto it = m_keepalives.constFind(deviceId);
        it != m_keepalives.cend() && it->lastHeardAnyMs >= 0) {
        keepalive = QStringLiteral("the last keepalive came on %1, received %2 ms ago")
                        .arg(channelName(it->lastChannel))
                        .arg(at - it->lastHeardAnyMs);
    }
    QString control = QStringLiteral("no control session known");
    if (transport != nullptr) {
        const auto state = m_peers.constFind(transport);
        control = state != m_peers.cend() && state->lastInMs >= 0
            ? QStringLiteral("the last control message was %1, taken from the queue %2 ms ago")
                  .arg(state->lastInName)
                  .arg(at - state->lastInMs)
            : QStringLiteral("no control message taken on this connection");
        control += QStringLiteral("; control link: ") + linkText(transport, peer);
    }
    qCInfo(lcControlLog).noquote()
        << QStringLiteral("Transmit watchdog stop for %1: %2; %3; %4")
               .arg(QString::fromLatin1(deviceId.toHex()),
                    linkClosed ? QStringLiteral("the link closed")
                               : QStringLiteral("no keepalive for %1 ms").arg(silentMs),
                    keepalive, control);
    if (const auto it = m_keepalives.find(deviceId); it != m_keepalives.end()) {
        it->lastHeardMs.fill(-1);
        it->lastHeardAnyMs = -1;
    }
}

} // namespace NereusSDR
