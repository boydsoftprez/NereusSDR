// no-port-check: NereusSDR-original.
// =================================================================
// src/core/safety/TransmitHolder.h  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 34 (R-IOS-02; the several-devices design, rulings
// 8.1, 8.2, 8.4 and 8.15): which device holds transmit, and how it changes
// hands.
//
// ---- The record (ruling 8.1) ----
// The holder's device id, name, short name and kind; its source (Device,
// or RadioPtt when the radio's own PTT took transmit); since when; whether
// it is keyed; whether it is away; and an epoch that advances with every
// change of holder. Three states: Unheld, Held (one device holds it, keyed
// or not, or away in its grace period and never keyed) and Transferring.
// It starts Unheld.
//
// ---- The transfer (ruling 8.2) ----
// Every change of holder and every release:
//   1. Transferring. Every key from every source is refused with
//      "Transmit is changing hands."; unkeying is never refused.
//   2. If the old holder is keyed, the unkey gate runs (Confirmed or
//      TimedOut). Then MOX is read: while it reads on no holder is assigned;
//      stopAllTx runs again and the transfer waits up to another 2000 ms.
//      Still on: the transfer ends with transmit unheld, and every key is
//      refused "The radio did not confirm it stopped transmitting." until
//      MOX reads off.
//   3. With MOX off: the new holder, unkeyed, or nobody; VOX disarmed; the
//      epoch advanced; changed() published.
//
// ---- When the holder goes away (ruling 8.15) ----
// holderDropped(): a keyed holder is unkeyed at once through step 2's fence
// (keys refused until MOX reads off) and transmit stays held for it, away
// and unkeyed, VOX disarmed. holderReturned(): the same device signed in
// again, still the holder, unkeyed. The end of its 180 s, leaving, a revoke
// or a fifth device's replacement release it through a transfer to nobody.
//
// ---- Keys (askKey, the keying gate's answer) ----
// While transferring or fenced: changingHands. After a failed transfer,
// until MOX reads off: stopNotConfirmed. Held: the holder's keys are
// admitted, anyone else's refused otherDeviceHolds naming the holder.
// Unheld: a program's key is refused programNeedsTransmit; any other key
// makes its device the holder (the station device with source RadioPtt for
// the radio's own PTT) and is admitted (D63).
//
// ---- Taking (Task 77, rulings 8.4, 8.6, 8.7, 8.9) ----
// askTake answers whether a device may take transmit now: at once, asked,
// already its own, or refused; the take is transferTo(). The radio's own
// PTT against another holder takes without a question (the Core runs the
// transfer from its keying gate). The station device keeps transmit after
// its key ends, as any holder does, until a device takes it.
//
// Everything that reaches the radio or the clock is injected (Hooks), so
// the rules are tested with fake ones.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 34 (R-IOS-02), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-26: Transmit group fix wave I1: releaseStationTake, the
//               station device's take ends with its key until Task 77.
//               J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-26: Transmit group fix wave 2, Important 2: a refused TUNE or
//               two-tone takes nothing (admitKey asks TX inhibit, the PA
//               trip, receive only and the interlock before the gate; a
//               take whose key never starts is released). J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-26: iPhone app plan Task 77 (R-IOS-02, R-IOS-03): askTake;
//               releaseStationTake removed (the take replaces it). J.J.
//               Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-30: TX badge take fix round 1: hooks(), so a window test can
//               read MOX as held on and reach a take the Core does not
//               assign. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
// =================================================================
#pragma once

#include "core/NereusCoreExport.h"
#include <QByteArray>
#include <QObject>
#include <QString>

#include <functional>
#include <optional>

#include "core/MoxController.h"
#include "core/safety/TxRefusal.h"
#include "core/safety/UnkeyGate.h"

namespace NereusSDR {

class NEREUS_CORE_EXPORT TransmitHolder : public QObject {
    Q_OBJECT

public:
    enum class State { Unheld, Held, Transferring };
    enum class Source { Device, RadioPtt };

    /// How long step 2 waits for MOX to read off after stopAllTx.
    static constexpr int kMoxOffWaitMs = 2000;

    struct Holder {
        QByteArray deviceId;
        QString name;
        QString shortName;
        /// "phone", "tablet", "computer", or "station" for the station
        /// device.
        QString kind;
        Source source{Source::Device};
        qint64 sinceMs{0};
        bool keyed{false};
        /// When the current key started (while keyed).
        qint64 keyedSinceMs{0};
        bool away{false};
    };

    /// The words a device is named by (Task 71's numbering).
    struct Words {
        QString name;
        QString shortName;
        QString kind;
    };

    struct Hooks {
        /// The Core's monotonic clock, in milliseconds.
        std::function<qint64()> clock;
        /// True while MOX reads on (the transmitter is not back in
        /// receive).
        std::function<bool()> moxOn;
        /// The unkey gate (UnkeyGate::unkey).
        std::function<void(const QString& reason, std::function<void(UnkeyOutcome)> done)> unkey;
        /// StopAllTx (Task 33).
        std::function<void(const QString& reason)> stopAllTx;
        /// Turns VOX off.
        std::function<void()> disarmVox;
        /// Runs `fire` once after `ms`.
        std::function<void(int ms, std::function<void()> fire)> schedule;
        /// A device's words, as the Core names it now.
        std::function<std::optional<Words>(const QByteArray& deviceId)> describe;
    };

    explicit TransmitHolder(QObject* parent = nullptr);

    void setHooks(Hooks hooks);
    /// The hooks set now (a test replaces one and sets them back).
    const Hooks& hooks() const { return m_hooks; }

    State state() const { return m_state; }
    /// The holder, with its words read now; nullopt while unheld (and
    /// while a transfer runs, the old holder until it ends).
    std::optional<Holder> holder() const;
    bool isHeldBy(const QByteArray& deviceId) const;
    quint64 epoch() const { return m_epoch; }
    /// Keys are refused until MOX reads off (a dropped holder's stop).
    bool isFenced() const { return m_fenced; }
    /// A transfer ended with MOX still on; keys refused until it reads off.
    bool isStopUnconfirmed() const { return m_stopUnconfirmed; }
    /// The holder is on the air.
    bool isKeyed() const { return m_holder.has_value() && m_holder->keyed; }

    // ---- Keys ----

    struct KeyRequest {
        QByteArray deviceId;
        Source source{Source::Device};
        bool program{false};
        /// A VOX key: audio, not a person (ruling 8.4).
        bool vox{false};
    };
    /// The keying gate's answer for `request` (see the header comment). An
    /// admitted key on unheld transmit makes its device the holder first.
    KeyingAnswer askKey(const KeyRequest& request);

    /// Why a key from `deviceId` would be refused now, or an empty refusal
    /// when it would be admitted (a question only: nothing changes).
    TxRefusal keyRefusalFor(const QByteArray& deviceId, bool program = false) const;

    // ---- Taking transmit (Task 77; rulings 8.4, 8.6, 8.7) ----

    enum class TakeVerdict {
        /// Take now: transmit is unheld, or the device was shown the holder
        /// it would take from (ruling 8.7).
        AtOnce,
        /// The requester already holds transmit: nothing to take.
        AlreadyHeld,
        /// Another device holds transmit: ask the requester's operator.
        Ask,
        /// Refused: a transfer is running, or the radio did not confirm it
        /// stopped.
        Refuse,
    };
    struct TakeAnswer {
        TakeVerdict verdict{TakeVerdict::Refuse};
        TxRefusal refusal;
    };
    /// Whether `requester` may take transmit now (a question only). With
    /// `shownEpoch` (the holderEpoch the device showed its operator) and
    /// `shownKeyed` (whether it showed the holder on the air), a take from
    /// another holder is at once while the epoch still names that holder
    /// and the holder is not on the air unless it was shown so; otherwise
    /// it is asked. Taking never keys (ruling 8.6); the take itself is
    /// transferTo().
    TakeAnswer askTake(const QByteArray& requester, std::optional<quint64> shownEpoch = std::nullopt,
                       std::optional<bool> shownKeyed = std::nullopt) const;

    // ---- The transfer and the holder's comings and goings ----

    /// Ruling 8.2: from the holder now to `next` (nullopt: to nobody).
    /// `done(assigned)` runs when it ends: true when `next` (or nobody, for
    /// a release) was assigned, false when MOX did not read off. A transfer
    /// asked for while one runs is refused (done(false) at once).
    void transferTo(std::optional<Holder> next, const QString& reason,
                    std::function<void(bool assigned)> done = {});
    /// Synchronous final settlement of a device place. Nested calls keep
    /// keys and new holders blocked until the outermost callback returns.
    void runWithKeyingBlocked(const std::function<void()>& callback);

    /// Releases transmit when `deviceId` holds it (a transfer to nobody).
    /// Nothing when it does not.
    void release(const QByteArray& deviceId, const QString& reason);

    /// Ruling 8.15: the holder's link dropped. Unkeyed through the fence,
    /// held for it, away, VOX disarmed. Nothing for any other device.
    void holderDropped(const QByteArray& deviceId, const QString& reason);
    /// The same device signed in again within its 180 s.
    void holderReturned(const QByteArray& deviceId);

    /// Fix wave 2, Important 2: a take on unheld transmit whose key has not
    /// started yet (askKey took; setKeyed(true) has not followed).
    bool isTakeUnstarted() const { return m_takeUnstarted; }
    /// A refused TUNE or two-tone takes nothing: when the take is still
    /// unstarted, its holder unkeyed, MOX reads off and nothing is fenced,
    /// transmit is unheld again at once (nothing to unkey; VOX as it was).
    /// Nothing otherwise.
    void releaseUnstartedTake();

    /// The holder's key started or ended (the owner reports MOX with the
    /// keyer it is for).
    void setKeyed(bool keyed);
    /// MOX now reads on or off (MoxController left or reached receive).
    void onMoxReading(bool on);

signals:
    /// State, holder, keyed, away, the fence or the epoch changed.
    void changed();

private:
    int m_keyingBlockDepth = 0;
    void afterUnkey(quint64 generation);
    void assign(quint64 generation);
    void failTransfer(quint64 generation);
    void startFence(const QString& reason);
    void disarmVox();
    qint64 now() const;
    bool moxOn() const;

    Hooks m_hooks;
    State m_state{State::Unheld};
    std::optional<Holder> m_holder;
    std::optional<Holder> m_next;
    std::function<void(bool)> m_transferDone;
    QString m_transferReason;
    quint64 m_epoch{0};
    quint64 m_generation{0};
    bool m_waitingMoxOff{false};
    bool m_fenced{false};
    bool m_fenceWaitingMoxOff{false};
    bool m_stopUnconfirmed{false};
    bool m_takeUnstarted{false};
};

} // namespace NereusSDR
