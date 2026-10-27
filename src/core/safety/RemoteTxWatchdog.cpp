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
//   2026-10-15: tripQuiet. J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
//   2026-10-16: a stop from the cut guard reports the silence the guard
//               judged when this clock reads less. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-10-26: Remote transmit hold then cut: two stages per watch, the
//               hold after kLinkHoldMs and the cut after kLinkCutMs, each
//               with its own late turn; a counted keepalive, the key's end,
//               the cut and the link closing end a hold. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26: Remote transmit hold then cut, fix wave A: a hold is cut
//               once it has lasted more than kLinkHoldMaxMs, in the check,
//               in reschedule() and for a keepalive heard past it. J.J.
//               Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
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

bool RemoteTxWatchdog::isHeld(const QByteArray& deviceId) const
{
    const auto it = m_devices.constFind(deviceId);
    return it != m_devices.cend() && it.value().held;
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
    // Hold then cut: only a key's RF is held, so a key that ended ends its
    // hold (KeyEnded), with no stop, whether or not VOX armed still watches.
    const bool holdEnds = watch.held && !watch.keyed;
    const qint64 holdLengthMs = holdEnds ? now() - watch.heldSinceMs : 0;
    if (holdEnds) {
        watch.held = false;
    }
    if (!watch.keyed && !watch.voxArmed) {
        if (wasWatched) {
            m_devices.erase(it);
            qCDebug(lcDsp) << "Transmit watchdog: no longer watching" << deviceId.toHex().constData();
            reschedule();
            if (holdEnds) {
                notifyHoldEnded(deviceId, holdLengthMs, HoldEnd::KeyEnded);
            }
        }
        return;
    }
    if (!wasWatched) {
        // A fresh start, and sequences start again.
        watch.lastMs = now();
        watch.lastSequence = 0;
        qCDebug(lcDsp) << "Transmit watchdog: watching" << deviceId
                      << (watch.keyed ? "(keyed)" : "(VOX armed)");
    }
    m_devices.insert(deviceId, watch);
    reschedule();
    if (holdEnds) {
        notifyHoldEnded(deviceId, holdLengthMs, HoldEnd::KeyEnded);
    }
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
    // Once the watch expires (past the cut), no arriving packet may renew it.
    // Fix wave A: the same for a hold past its bound; a keepalive heard
    // after it cannot end the hold.
    if (heard - watch.lastMs > kLinkCutMs
        || (watch.held && heard - watch.heldSinceMs > kLinkHoldMaxMs)) {
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
    // (and the late check's one turn for it) is over. Hold then cut: each
    // stage's apart.
    const qint64 silentMs = now() - watch.lastMs;
    if (silentMs <= kLinkCutMs) {
        watch.lateTurnGiven = false;
    }
    const bool fresh = silentMs <= kLinkHoldMs;
    if (fresh) {
        watch.lateHoldTurnGiven = false;
    }
    // Hold then cut: a keepalive that counted, and heard within the hold's
    // 400 ms (not one that waited behind a stall longer than that), ends
    // the hold. Only now, past the sequence and epoch checks above.
    const bool holdEnds = watch.held && fresh;
    const qint64 holdLengthMs = holdEnds ? now() - watch.heldSinceMs : 0;
    if (holdEnds) {
        watch.held = false;
    }
    reschedule();
    if (holdEnds && !notifyHoldEnded(deviceId, holdLengthMs, HoldEnd::Heard)) {
        return true;
    }
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

bool RemoteTxWatchdog::tripQuiet(const QByteArray& deviceId, qint64 judgedSilentMs)
{
    if (!m_devices.contains(deviceId)) {
        return false;
    }
    trip(deviceId, false, judgedSilentMs);
    return true;
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
    // Hold then cut: the hold and the cut each get their own one turn; a
    // check that finds the cut due gives (or has given) the cut's turn and
    // judges the cut alone, as a cut ends any hold.
    const qint64 due = std::exchange(m_checkDueMs, -1);
    const bool late = due >= 0 && at - due > kLateCheckSlackMs;
    const bool canTurn = late && static_cast<bool>(m_hooks.startTimer);
    QList<QByteArray> quiet;
    QList<QByteArray> toHold;
    for (auto it = m_devices.begin(); it != m_devices.end(); ++it) {
        Watch& watch = it.value();
        const qint64 silentMs = at - watch.lastMs;
        if (silentMs > kLinkCutMs) {
            if (canTurn && !watch.lateTurnGiven) {
                watch.lateTurnGiven = true;        // judged at the next check
                watch.lateHoldTurnGiven = true;    // the same turn serves both
            } else {
                quiet.append(it.key());
            }
        } else if (watch.held && at - watch.heldSinceMs > kLinkHoldMaxMs) {
            // Fix wave A: a hold past its bound is the cut, whatever the
            // keepalives that counted without ending it did to lastMs.
            if (canTurn && !watch.lateHoldMaxTurnGiven) {
                watch.lateHoldMaxTurnGiven = true;   // judged at the next check
            } else {
                quiet.append(it.key());
            }
        } else if (watch.keyed && !watch.held && silentMs > kLinkHoldMs) {
            if (canTurn && !watch.lateHoldTurnGiven) {
                watch.lateHoldTurnGiven = true;    // judged at the next check
            } else {
                toHold.append(it.key());
            }
        }
    }
    const QPointer<RemoteTxWatchdog> self(this);
    // The cuts first: the stop is the safer act.
    for (const QByteArray& deviceId : std::as_const(quiet)) {
        if (!self) {
            return;
        }
        // A stop for one device may already have ended another's watch.
        if (m_devices.contains(deviceId)) {
            trip(deviceId, false);
        }
    }
    for (const QByteArray& deviceId : std::as_const(toHold)) {
        if (!self) {
            return;
        }
        startHold(deviceId, at);
    }
    if (self) {
        reschedule();
    }
}

void RemoteTxWatchdog::startHold(const QByteArray& deviceId, qint64 at)
{
    auto it = m_devices.find(deviceId);
    // A stop or a hold hook may already have ended the watch or its key.
    if (it == m_devices.end() || !it.value().keyed || it.value().held) {
        return;
    }
    it.value().held = true;
    it.value().heldSinceMs = at;
    it.value().lateHoldMaxTurnGiven = false;
    const qint64 silentMs = at - it.value().lastMs;
    qCInfo(lcDsp).noquote() << "Transmit watchdog: no keepalive for"
                            << QStringLiteral("%1 ms").arg(silentMs) << "from"
                            << QString::fromLatin1(deviceId.toHex()) << "- holding its RF silent";
    const QPointer<RemoteTxWatchdog> self(this);
    if (m_hooks.hold) {
        m_hooks.hold(deviceId, true);
    }
    if (!self) {
        return;
    }
    emit holdStarted(deviceId, silentMs);
}

bool RemoteTxWatchdog::notifyHoldEnded(const QByteArray& deviceId, qint64 lengthMs, HoldEnd how)
{
    const QPointer<RemoteTxWatchdog> self(this);
    if (m_hooks.hold) {
        m_hooks.hold(deviceId, false);
    }
    if (!self) {
        return false;
    }
    emit holdEnded(deviceId, lengthMs, how);
    return static_cast<bool>(self);
}

void RemoteTxWatchdog::trip(const QByteArray& deviceId, bool linkClosed, qint64 judgedSilentMs)
{
    const Watch watch = m_devices.take(deviceId);
    // The cut guard's measurement, when it made this stop, is never
    // reported lower by this clock's rounding.
    const qint64 silentMs = std::max(now() - watch.lastMs, judgedSilentMs);
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
    if (!self) {
        return;
    }
    // Hold then cut: a hold this stop ends is released only now, after the
    // stop, so no real sample leaves between them.
    if (watch.held
        && !notifyHoldEnded(deviceId, now() - watch.heldSinceMs,
                            linkClosed ? HoldEnd::LinkClosed : HoldEnd::Cut)) {
        return;
    }
    reschedule();
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
        // Hold then cut: each watch's earliest pending stage, the hold for
        // a keyed watch not yet held, else the cut. "More than": the first
        // millisecond past it.
        // Fix wave A: a held watch is due at the earlier of its cut and its
        // hold's bound.
        const Watch& watch = it.value();
        qint64 dueMs = 0;
        if (watch.keyed && !watch.held) {
            dueMs = watch.lastMs + kLinkHoldMs + 1;
        } else {
            dueMs = watch.lastMs + kLinkCutMs + 1;
            if (watch.held) {
                dueMs = std::min(dueMs, watch.heldSinceMs + kLinkHoldMaxMs + 1);
            }
        }
        earliest = std::min(earliest, dueMs);
    }
    const qint64 wait = std::max<qint64>(0, earliest - now());
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
