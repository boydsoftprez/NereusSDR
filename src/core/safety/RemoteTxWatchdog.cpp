// no-port-check: NereusSDR-original.
// =================================================================
// src/core/safety/RemoteTxWatchdog.cpp  (NereusSDR)
// =================================================================
//
// See RemoteTxWatchdog.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 37 (R-IOS-13), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-10-01: TX mic thread (JJ approved): keepalives heard at their
//               receipt; a late check lets the waiting keepalives in
//               first. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
//   2026-10-01: TX mic thread fix round 2: that turn is given once per
//               device per overdue period, and only to a check more than
//               kLateCheckSlackMs late. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-10-01: TX watch follow-up: a check already due is left to run
//               rather than restarted, so the stop bound no longer rests on
//               where the event loop puts a restarted check. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-01: TX diagnostics lane: the device id prints as hex in the
//               log, not as raw bytes. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
// =================================================================

#include "core/safety/RemoteTxWatchdog.h"

#include "core/LogCategories.h"

#include <QList>
#include <QPointer>

#include <algorithm>
#include <limits>
#include <utility>

namespace NereusSDR {

RemoteTxWatchdog::RemoteTxWatchdog(QObject* parent)
    : QObject(parent)
{
}

void RemoteTxWatchdog::setHooks(Hooks hooks)
{
    m_hooks = std::move(hooks);
}

QString RemoteTxWatchdog::stopMessage(const QString& deviceName)
{
    return QStringLiteral("The link to %1 went quiet, so the Core stopped transmitting.")
        .arg(deviceName);
}

qint64 RemoteTxWatchdog::now() const
{
    return m_hooks.clock ? m_hooks.clock() : 0;
}

bool RemoteTxWatchdog::isWatching(const QByteArray& deviceId) const
{
    return m_devices.contains(deviceId);
}

void RemoteTxWatchdog::setKeyed(const QByteArray& deviceId, bool keyed, quint32 epoch)
{
    update(deviceId, keyed, true, epoch, false, false);
}

void RemoteTxWatchdog::setVoxArmed(const QByteArray& deviceId, bool armed)
{
    update(deviceId, false, false, 0, armed, true);
}

void RemoteTxWatchdog::update(const QByteArray& deviceId, bool keyed, bool keyedChanged,
                              quint32 epoch, bool voxArmed, bool voxChanged)
{
    if (deviceId.isEmpty()) {
        return;
    }
    auto it = m_devices.find(deviceId);
    const bool wasWatched = it != m_devices.end();
    Watch watch = wasWatched ? it.value() : Watch{};
    if (keyedChanged) {
        watch.keyed = keyed;
        watch.epoch = keyed ? epoch : 0;
    }
    if (voxChanged) {
        watch.voxArmed = voxArmed;
    }
    if (!watch.keyed && !watch.voxArmed) {
        if (wasWatched) {
            m_devices.erase(it);
            qCDebug(lcDsp) << "Transmit watchdog: no longer watching" << deviceId.toHex().constData();
            reschedule();
        }
        return;
    }
    if (!wasWatched) {
        // A fresh 400 ms, and sequences start again.
        watch.lastMs = now();
        watch.lastSequence = 0;
        qCDebug(lcDsp) << "Transmit watchdog: watching" << deviceId
                      << (watch.keyed ? "(keyed)" : "(VOX armed)");
    }
    m_devices.insert(deviceId, watch);
    reschedule();
}

bool RemoteTxWatchdog::keepalive(const QByteArray& deviceId, quint64 sequence, quint32 epoch,
                                 Path path, qint64 ageMs)
{
    Q_UNUSED(path);
    auto it = m_devices.find(deviceId);
    if (it == m_devices.end()) {
        // Nothing watched: a keepalive before the key, or after its end.
        return false;
    }
    Watch& watch = it.value();
    // TX mic thread: heard when it came off the network.
    const qint64 heard = now() - std::max<qint64>(0, ageMs);
    // Socket input can run before an overdue timer on a busy event loop.
    // Once the watch expires, no arriving packet may renew it.
    if (heard - watch.lastMs > kLinkLossDeadlineMs) {
        trip(deviceId, false);
        return false; // The stop callback may have destroyed this object.
    }
    if (sequence <= watch.lastSequence) {
        // A copy (the same keepalive by another path) or one overtaken.
        return false;
    }
    if (watch.epoch != 0 && epoch < watch.epoch) {
        // From a key that has ended: it says nothing about this one.
        return false;
    }
    watch.lastSequence = sequence;
    // Task 29 step 2b: how long since the one before, for the measurement
    // of keyed-event tails on the web relay (nothing acts on it).
    const qint64 gap = watch.lastMs > 0 ? heard - watch.lastMs : -1;
    // A keepalive that came before the watch began (and waited at the Core
    // into it) leaves the watch's start as it is.
    watch.lastMs = std::max(watch.lastMs, heard);
    // Fix round 2: back within its deadline, the device's overdue period
    // (and the late check's one turn for it) is over.
    if (now() - watch.lastMs <= kLinkLossDeadlineMs) {
        watch.lateTurnGiven = false;
    }
    reschedule();
    emit keepaliveHeard(deviceId, gap);
    return true;
}

void RemoteTxWatchdog::linkClosed(const QByteArray& deviceId)
{
    if (!m_devices.contains(deviceId)) {
        return;
    }
    trip(deviceId, true);
}

void RemoteTxWatchdog::onTimer()
{
    const qint64 at = now();
    // TX mic thread: a check that fired late ran behind a stall of the
    // event loop, and keepalives that came during it may still wait in the
    // transports' queues. They go first (a check of 0 ms runs after the
    // timers already due, the transports' drains among them) and are heard
    // at their receipt; then the watch is judged. Fix round 2: only past
    // the timer's ordinary slack, and once per device per overdue period,
    // so nothing (another device's keepalives rescheduling the check on
    // every turn included) can put the judgement off again.
    // TX watch follow-up: this check is no longer pending once it runs.
    const qint64 due = std::exchange(m_checkDueMs, -1);
    const bool late = due >= 0 && at - due > kLateCheckSlackMs;
    const bool canTurn = late && static_cast<bool>(m_hooks.startTimer);
    QList<QByteArray> quiet;
    for (auto it = m_devices.begin(); it != m_devices.end(); ++it) {
        if (at - it.value().lastMs > kLinkLossDeadlineMs) {
            if (canTurn && !it.value().lateTurnGiven) {
                it.value().lateTurnGiven = true;   // judged at the next check
            } else {
                quiet.append(it.key());
            }
        }
    }
    const QPointer<RemoteTxWatchdog> self(this);
    for (const QByteArray& deviceId : std::as_const(quiet)) {
        if (!self) {
            return;
        }
        // A stop for one device may already have ended another's watch.
        if (m_devices.contains(deviceId)) {
            trip(deviceId, false);
        }
    }
    if (self) {
        reschedule();
    }
}

void RemoteTxWatchdog::trip(const QByteArray& deviceId, bool linkClosed)
{
    const Watch watch = m_devices.take(deviceId);
    const qint64 silentMs = now() - watch.lastMs;
    const QString name = m_hooks.deviceName ? m_hooks.deviceName(deviceId) : QString();
    const QString message = stopMessage(name.isEmpty() ? QStringLiteral("a device") : name);
    qCWarning(lcDsp).noquote() << "Transmit watchdog:"
                               << (linkClosed ? "the link closed" : "no keepalive for")
                               << (linkClosed ? QString() : QStringLiteral("%1 ms").arg(silentMs))
                               << "from" << QString::fromLatin1(deviceId.toHex())
                               << "-" << message;
    const QPointer<RemoteTxWatchdog> self(this);
    if (m_hooks.stop) {
        m_hooks.stop(deviceId, message);
    }
    if (!self) {
        return;
    }
    emit tripped(deviceId, linkClosed, silentMs);
    if (self) {
        reschedule();
    }
}

void RemoteTxWatchdog::reschedule()
{
    if (m_devices.isEmpty()) {
        m_checkDueMs = -1;
        if (m_hooks.stopTimer) {
            m_hooks.stopTimer();
        }
        return;
    }
    // TX watch follow-up: a check already due is left to run. Restarting
    // it would put it behind the timers already due (the transports'
    // drains among them), and a device whose keepalives arrive on every
    // drain would then put it off on every turn of the event loop. Left
    // alone it runs in this turn, whatever order the event loop gives
    // timers due together.
    if (m_checkDueMs >= 0 && m_checkDueMs <= now()) {
        return;
    }
    qint64 earliest = std::numeric_limits<qint64>::max();
    for (auto it = m_devices.cbegin(); it != m_devices.cend(); ++it) {
        // "More than 400 ms": the first millisecond past the deadline.
        earliest = std::min(earliest, it.value().lastMs + kLinkLossDeadlineMs + 1);
    }
    const qint64 wait =
        std::min<qint64>(std::max<qint64>(0, earliest - now()), kLinkLossDeadlineMs + 1);
    m_checkDueMs = now() + wait;
    if (m_hooks.startTimer) {
        m_hooks.startTimer(static_cast<int>(wait));
    }
}

QByteArray RemoteTxWatchdog::channelKeepalive(quint64 sequence, quint32 epoch)
{
    QByteArray message(kChannelKeepaliveBytes, '\0');
    message[0] = static_cast<char>(kChannelKeepaliveKind);
    for (int i = 0; i < 8; ++i) {
        message[1 + i] = static_cast<char>((sequence >> (56 - 8 * i)) & 0xFFu);
    }
    for (int i = 0; i < 4; ++i) {
        message[9 + i] = static_cast<char>((epoch >> (24 - 8 * i)) & 0xFFu);
    }
    return message;
}

bool RemoteTxWatchdog::readChannelKeepalive(const QByteArray& message, quint64* sequence,
                                            quint32* epoch)
{
    if (message.size() != kChannelKeepaliveBytes
        || static_cast<quint8>(message.at(0)) != kChannelKeepaliveKind) {
        return false;
    }
    quint64 s = 0;
    for (int i = 0; i < 8; ++i) {
        s = (s << 8) | static_cast<quint8>(message.at(1 + i));
    }
    quint32 e = 0;
    for (int i = 0; i < 4; ++i) {
        e = (e << 8) | static_cast<quint8>(message.at(9 + i));
    }
    if (s == 0) {
        return false;
    }
    if (sequence) {
        *sequence = s;
    }
    if (epoch) {
        *epoch = e;
    }
    return true;
}

} // namespace NereusSDR
