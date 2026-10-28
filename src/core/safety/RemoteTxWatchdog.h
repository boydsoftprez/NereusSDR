// no-port-check: NereusSDR-original.
// =================================================================
// src/core/safety/RemoteTxWatchdog.h  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 37 (R-IOS-13; remote design section 12.1, "drop MOX
// on link loss"; pairing design section 9.7; spec section 4.6 item 1): the
// Core holds a transmitting device's RF silent when the link to it goes
// quiet, and stops transmitting if the silence lasts (hold then cut).
//
// ---- The numbers, stated together (remote design sections 8.3, 12.3;
// hold then cut design 5.1) ----
// A device that is keyed, or has VOX armed, sends tx.keepalive every
// 100 ms (kKeepaliveIntervalMs). Once more than 400 ms (kLinkHoldMs) pass
// with none, a keyed device's RF is held silent while it stays keyed; once
// more than 3000 ms (kLinkCutMs) pass, the Core stops transmitting. The
// microphone starvation deadline is 250 ms (RemoteMicConfig::kStarvationMs)
// and the transmit jitter buffer at most 120 ms
// (RemoteMicConfig::kMaxDepthMs); a client's first reconnect comes 1000 ms
// after a loss (kFirstReconnectMs, the desktop's
// StationClient::kDefaultReconnectBackoffUnitMs). So
// 120 < 250 < 400 < 1000 < 3000, checked below at compile time: RF is off
// before a client's first reconnect, and a short outage is held, not cut.
//
// ---- The rules ----
// - A device is watched while it is keyed (MOX, TUNE, two-tone, a program's
//   key through its window, or a VOX key that is its) or has VOX armed.
//   Watching starts afresh (its silence counted from the watch's start);
//   the first keepalive may come any time within the hold's 400 ms.
// - A keepalive counts when its sequence is newer than the last one that
//   counted from that device (a copy, or one overtaken on an unordered
//   path, changes nothing) and its epoch is not older than the device's
//   key now on (the epoch the key's answer gave it; a key the device was
//   never answered for, VOX's, has none, so any epoch counts). Sequences
//   start again whenever watching starts.
// - A keepalive is heard when it came off the network (its wait at the
//   Core since its receipt is taken off; TX mic thread, JJ approved
//   2026-10-01), so a stall of the Core's event loop, which holds both the
//   keepalives and this watchdog, is never taken for a quiet link. The
//   watchdog cannot run during such a stall: a link that went quiet in it
//   is caught when the stall ends, as the silence is counted from the last
//   keepalive's receipt. A check that fires late (more than
//   kLateCheckSlackMs behind its time: a stall) first lets the keepalives
//   waiting behind it in, one turn of the event loop, and then judges by
//   when they came. That turn is given once for each device's overdue
//   period (fix round 2): the check after it judges the device, whatever
//   else rescheduled the timer in between. Hold then cut: the hold and the
//   cut are judged apart, each with its own one turn. A check already due is never
//   restarted (TX watch follow-up): it runs in the turn it came due in,
//   whatever order the event loop gives timers due together, so a
//   device's keepalives on every turn cannot put it off.
// - The hold: more than 400 ms without one while keyed: hold(id, true)
//   and holdStarted. The key stays on and nothing is stopped; the Core
//   sends silence in place of the device's audio. A device watched only for
//   VOX armed is never held (there is no RF to hold).
// - A keepalive that counts, heard within 400 ms of now, ends the hold
//   (hold(id, false), holdEnded Heard) and the device stays watched; a new
//   hold needs a fresh 400 ms of silence. One that does not count, or one
//   that counts but is older than that (heard behind a lagging main
//   thread), ends nothing.
// - A hold is bounded (fix wave A): one that has not ended is cut once it
//   has lasted more than 2600 ms (kLinkHoldMaxMs, kLinkCutMs - kLinkHoldMs),
//   or at the 3000 ms cut below, whichever comes first. Stale keepalives
//   that count keep moving the 3000 ms cut back, so without this bound a
//   main thread lagging by more than 400 ms would hold a key forever. The
//   bound's check gets one late turn per hold.
// - The cut: more than 3000 ms without one (or a hold past its bound): stop(), which the Core makes
//   StopAllTx with "The link to <device> went quiet, so the Core stopped
//   transmitting." and the VOX that device armed turned off. The device is
//   no longer watched. A hold it ends is released after the stop
//   (holdEnded Cut), so no real sample leaves between them. tripQuiet, the
//   cut guard's judgement, is the cut too.
// - The link closing (the session ends) while the device is watched stops
//   the same way at once (holdEnded LinkClosed when it was held).
// - The device's own release (tx.unkey, TUNE or two-tone off) ends the
//   watch on its key at once, before MOX reads off, so a transmission that
//   ends on its own after the release (a RADE end-of-over tail, built after
//   R4) is never taken for a lost link. A hold it ends ends with no stop
//   (holdEnded KeyEnded).
//
// ---- Paths ----
// The rules do not depend on how a keepalive travelled. On the session's
// WebSocket it is the tx.keepalive verb; on a media connection's data
// channel labelled "tx" (unordered, never retransmitted, so a lost one is
// simply overtaken by the next) it is channelKeepalive()'s 13 bytes. A
// transport added later (the rendezvous, a relay, the separate control
// connection) hands its keepalives to keepalive() with its own Path and
// changes nothing else. Path is for the log only.
//
// Everything that reaches the clock, the timer and the radio is injected
// (Hooks), so the rules are tested with fake ones.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-27: Add approved remote CW hold/activity integration for
//               J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-09-25: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 37 (R-IOS-13), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-10-01: TX mic thread (JJ approved): keepalives heard at their
//               receipt (keepalive's ageMs), and a late check lets the
//               waiting keepalives in first. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-10-01: TX mic thread fix round 2: the late check's turn is one
//               per device per overdue period (another device's keepalives
//               can no longer hold a dead link's key), and only for a check
//               more than kLateCheckSlackMs late. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-10-01: TX watch follow-up: a check already due is left to run
//               rather than restarted. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-10-15: tripQuiet, the cut guard's judgement
//               tripping a watched device once. J.J. Boyd (KG4VCF), AI-
//               assisted via Anthropic Claude Code.
//   2026-10-16: tripQuiet carries the silence the cut guard judged, so a
//               stop it made never reports less than the guard measured.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26: Remote transmit hold then cut: two stages per watch, a
//               hold of a keyed device's RF after 400 ms of silence
//               (kLinkHoldMs, the hold hook, holdStarted and holdEnded) and
//               the cut, today's stop, after 3000 ms (kLinkCutMs, replacing
//               kLinkLossDeadlineMs). J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-10-26: Remote transmit hold then cut, fix wave A: a hold that has
//               not ended is cut after kLinkHoldMaxMs (2600 ms), so stale
//               keepalives behind a lagging main thread cannot hold a key
//               forever. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
// =================================================================
#pragma once

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QString>

#include <functional>

#include "core/safety/TxKeepalive.h"
#include "core/NereusCoreExport.h"
#include "core/session/media/RemoteMicReceiver.h"

namespace NereusSDR {

class NEREUS_CORE_EXPORT RemoteTxWatchdog : public QObject {
    Q_OBJECT

public:
    /// How often a watched device sends tx.keepalive.
    static constexpr int kKeepaliveIntervalMs = 100;
    /// Hold then cut: how long without a keepalive before a keyed device's
    /// RF is held silent (RF off, still keyed).
    static constexpr int kLinkHoldMs = 400;    // RF off, still keyed
    /// How long without a keepalive before the Core stops transmitting.
    static constexpr int kLinkCutMs = 3000;    // today's stop
    /// Fix wave A: the longest a hold lasts before the cut, so a hold
    /// started at kLinkHoldMs of silence is cut by kLinkCutMs even when
    /// stale keepalives keep counting (each one moves kLinkCutMs back).
    static constexpr int kLinkHoldMaxMs = kLinkCutMs - kLinkHoldMs;   // 2600
    /// TX mic thread fix round 2: how late a check may fire and still be on
    /// time. The check is a precise timer (about 1 ms of slack) and the
    /// media transports hand over what they received every 2 ms, so a
    /// keepalive can wait up to 3 ms behind an event loop that is merely
    /// busy; a check later than 5 ms ran behind a stall, the only case
    /// whose waiting keepalives get a turn before the judgement. The
    /// bench's shortest logged stall was 30 ms, far above it.
    static constexpr int kLateCheckSlackMs = 5;
    /// A client's first reconnect after a loss (the desktop's backoff unit,
    /// StationClient::kDefaultReconnectBackoffUnitMs).
    static constexpr int kFirstReconnectMs = 1000;

    static_assert(RemoteMicConfig::kMaxDepthMs < RemoteMicConfig::kStarvationMs,
                  "the transmit buffer is shorter than the starvation deadline");
    static_assert(RemoteMicConfig::kStarvationMs < kLinkHoldMs,
                  "the mic line holds before the link does");
    static_assert(kLinkHoldMs < kFirstReconnectMs,
                  "RF is off before a client's first reconnect (hold then cut design 5.1)");
    static_assert(kFirstReconnectMs < kLinkCutMs, "a short outage is held, not cut");
    static_assert(kKeepaliveIntervalMs * 3 < kLinkHoldMs,
                  "three keepalives in a row may be lost without a hold");

    /// Which path a keepalive came by (for the log; the rules are the same).
    enum class Path {
        /// The tx.keepalive verb on the session's WebSocket.
        Session,
        /// The media connection's "tx" data channel.
        TxChannel,
        /// A separately authenticated transmit-watch transport.
        Auxiliary,
    };

    /// Hold then cut: how a hold ended.
    enum class HoldEnd {
        /// A keepalive that counted.
        Heard,
        /// The cut (the watchdog's own, or tripQuiet's).
        Cut,
        /// The device's key ended (its release, or its key read off).
        KeyEnded,
        /// The device's session ended.
        LinkClosed,
    };
    Q_ENUM(HoldEnd)

    struct Hooks {
        /// The Core's monotonic clock, in milliseconds.
        std::function<qint64()> clock;
        /// (Re)starts the one single-shot check timer to fire onTimer()
        /// after `ms`; a later call replaces an earlier one.
        ///
        /// TX watch follow-up: the timer is the watchdog's alone. Nothing
        /// else starts or stops it, and every start ends in exactly one
        /// onTimer() unless stopTimer or a later start replaces it. The
        /// watchdog relies on this: a check it recorded as due is still
        /// pending in the timer, so reschedule() leaves it to run rather
        /// than restarting it. A timer stopped or restarted behind the
        /// watchdog's back would leave that check never run.
        std::function<void(int ms)> startTimer;
        /// Stops the check timer (the watchdog's alone, as above).
        std::function<void()> stopTimer;
        /// Stops transmitting for `deviceId` with `message` (StopAllTx, and
        /// the VOX that device armed turned off).
        std::function<void(const QByteArray& deviceId, const QString& message)> stop;
        /// The device's name as the Core shows it.
        std::function<QString(const QByteArray& deviceId)> deviceName;
        // The device's quiet link is held (true) or no longer (false). Main thread.
        std::function<void(const QByteArray& deviceId, bool held)> hold;
    };

    explicit RemoteTxWatchdog(QObject* parent = nullptr);

    void setHooks(Hooks hooks);

    /// The sentence the Core stops with.
    static QString stopMessage(const QString& deviceName);

    // ---- What is watched ----

    /// `deviceId`'s key is on (true) or ended (false). `epoch` is the
    /// epoch its key's answer gave the device, or 0 for a key the device
    /// was never answered for (VOX).
    void setKeyed(const QByteArray& deviceId, bool keyed, quint32 epoch = 0);
    /// `deviceId` has VOX armed (true) or no longer (false).
    void setVoxArmed(const QByteArray& deviceId, bool armed);
    /// The device's own release: its key is no longer watched (VOX armed
    /// still is).
    void released(const QByteArray& deviceId) { setKeyed(deviceId, false); }

    void setCwActive(const QByteArray& deviceId, bool active, quint32 cwEpoch);
    bool isWatching(const QByteArray& deviceId) const;
    bool isWatchingAny() const { return !m_devices.isEmpty(); }
    /// Hold then cut: `deviceId`'s quiet link holds its RF silent now.
    bool isHeld(const QByteArray& deviceId) const;

    // ---- From the link ----

    /// One keepalive. True when it counted (see the header comment); one
    /// that counts ends a hold.
    /// `ageMs` (TX mic thread): how long it waited at the Core since its
    /// receipt off the network; it is heard then.
    bool keepalive(const QByteArray& deviceId, quint64 sequence, quint32 epoch, Path path,
                   qint64 ageMs = 0);
    bool keepalive(const QByteArray& deviceId, const TxKeepalive& value, Path path,
                   qint64 ageMs = 0);
    /// The device's session ended: a watched device stops at once, and a
    /// hold ends after the stop.
    void linkClosed(const QByteArray& deviceId);

    /// The check timer fired.
    void onTimer();
    /// Remote transmit cut off the main thread (2026-10-15): the Core's cut
    /// guard (RemoteTxCutGuard) judged `deviceId`'s link quiet off the main
    /// thread. A watched device trips now as a quiet link (not a closed
    /// one), with the same stop and `tripped` (the cut: a hold ends after
    /// it, holdEnded Cut), and is no longer watched, so
    /// a check for the same silence that comes later finds nothing to
    /// stop. False when it was not watched (its stop has already run).
    /// `judgedSilentMs` (2026-10-16): the silence the guard measured on its
    /// own clock. `tripped` reports the longer of it and this watchdog's
    /// own reading: the two clocks round to whole milliseconds at
    /// different instants, so this one can read 400 for a silence the
    /// guard judged at more than 400. -1: none given.
    bool tripQuiet(const QByteArray& deviceId, qint64 judgedSilentMs = -1);

    // ---- The "tx" data channel's message ----

    /// Byte 0 is 1 (a keepalive), bytes 1 to 8 the sequence and bytes 9 to
    /// 12 the epoch, both big-endian.
    static constexpr int kChannelKeepaliveBytes = 13;
    static constexpr quint8 kChannelKeepaliveKind = 1;
    static QByteArray channelKeepalive(quint64 sequence, quint32 epoch);
    /// False for anything that is not exactly one keepalive.
    static bool readChannelKeepalive(const QByteArray& message, quint64* sequence,
                                     quint32* epoch);

    static QByteArray channelKeepalive(const TxKeepalive& value);
    static bool readChannelKeepalive(const QByteArray& message, TxKeepalive* value);

signals:
    /// The link to `deviceId` went quiet (or closed) and the Core stopped,
    /// `silentMs` after the last keepalive that counted (or the watch's
    /// start).
    void tripped(const QByteArray& deviceId, bool linkClosed, qint64 silentMs);
    /// Hold then cut: `deviceId`'s keyed link went quiet `silentMs` after
    /// the last keepalive that counted (or the watch's start) and its RF is
    /// held silent.
    void holdStarted(const QByteArray& deviceId, qint64 silentMs);
    /// Hold then cut: the hold that began `lengthMs` ago ended, `how`.
    void holdEnded(const QByteArray& deviceId, qint64 lengthMs, RemoteTxWatchdog::HoldEnd how);
    /// Task 29 step 2b: a keepalive counted, `sinceLastMs` after the one
    /// before (-1 for a key's first). For measurement; nothing acts on it.
    void keepaliveHeard(const QByteArray& deviceId, qint64 sinceLastMs);

private:
    struct Watch {
        bool cwActive{false};
        quint32 cwEpoch{0};
        bool keyed{false};
        bool voxArmed{false};
        /// The epoch keepalives may not be older than (0: any).
        quint32 epoch{0};
        /// The last keepalive that counted, or when watching began.
        qint64 lastMs{0};
        quint64 lastSequence{0};
        // TX mic thread fix round 2: this overdue period's one turn for
        // the keepalives behind a stall has been given; the next check
        // judges. Cleared when a keepalive brings the device back within
        // its deadline. Hold then cut: this one is the cut's.
        bool lateTurnGiven{false};
        // Hold then cut: the same turn for the hold, given apart from the
        // cut's; cleared when a keepalive brings the device back within
        // kLinkHoldMs.
        bool lateHoldTurnGiven{false};
        // Hold then cut: the device's RF is held silent, since heldSinceMs.
        bool held{false};
        qint64 heldSinceMs{0};
        // Fix wave A: the hold bound's one late turn, given once per hold
        // (cleared only when a hold starts, never by a keepalive, so a
        // lagging main thread's stale keepalives cannot renew it).
        bool lateHoldMaxTurnGiven{false};
    };

    void update(const QByteArray& deviceId, bool keyed, bool keyedChanged, quint32 epoch,
                bool voxArmed, bool voxChanged);
    void trip(const QByteArray& deviceId, bool linkClosed, qint64 judgedSilentMs = -1);
    void startHold(const QByteArray& deviceId, qint64 at);
    // Calls the hold hook (false) and emits holdEnded. False when either
    // destroyed this object.
    bool notifyHoldEnded(const QByteArray& deviceId, qint64 lengthMs, HoldEnd how);
    void reschedule();
    qint64 now() const;

    Hooks m_hooks;
    QHash<QByteArray, Watch> m_devices;
    // TX mic thread: when the check timer was asked to fire (-1: no check
    // pending; cleared when the check runs, TX watch follow-up).
    qint64 m_checkDueMs{-1};
};

} // namespace NereusSDR
