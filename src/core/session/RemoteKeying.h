// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/RemoteKeying.h  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 35 (R-IOS-13, R-IOS-02; spec section 4.6 item 7;
// D51, D58, D63; the several-devices design, section 2.2 and rulings 8.3,
// 8.5 and 8.14): keying from a remote device. The Core's side of the
// verbs tx.key {trigger}, tx.unkey {epoch}, tx.tune {on} and
// tx.twoTone {on} (remoteTxVersion 1).
//
// Every key goes through the keying gate Task 34 installed on the model's
// MoxController (MoxController::setMox(bool, const KeyerIdentity&),
// admitKey), so the session's own gate (remote_transmit, remoteTx, a
// paired device, snapshot.complete), the transmit holder, TX inhibit, the
// PA trip, the band plan and the interlock all decide as they do for every
// key. Nothing here keys around them.
//
// ---- The rules ----
// - A person's key (any trigger but "tci", and TUNE and two-tone, which
//   carry none) on unheld transmit makes its device the holder, unkeyed,
//   then keys (D63; TransmitHolder::askKey). The holder keys. Another
//   device's key while transmit is held is refused otherDeviceHolds.
// - A program's key (trigger "tci") keys only while its device already
//   holds transmit; unheld, programNeedsTransmit, and nobody becomes the
//   holder (D58).
// - tx.unkey, and TUNE or two-tone off, are the holder's (ruling 8.5): from
//   a device that does not hold transmit they are refused ("<holder> has
//   the transmitter. Take it to stop the transmission.") and the
//   transmission continues. A release unkeys only that device's own key.
//   The Core's safety stops unkey whoever is keyed; they are not here.
//
// ---- The keying epoch (pairing design section 9.6) ----
// Every accepted key has its own epoch (uint32, advancing; the model's
// keyingEpoch), returned with the accepted result. A device sends each key
// and each unkey three times as the same command (the same verb and id).
// The Core acts on the first: a copy of an accepted key is answered with
// the same epoch while that key is still on, and refused keyEnded once
// the key has ended, so a delayed copy never keys again after a safety
// stop; a copy of a refused key gets the same refusal. A tx.key with a new
// id while the device's own key is on is answered with that key's epoch
// and changes nothing. A tx.unkey whose epoch is older than the device's
// key now on is ignored; the Core answers it accepted and changes nothing.
// A session's commands are forgotten when it ends, so a reconnect never
// replays or resumes a key.
//
// ---- Who is keyed ----
// RadioModel::keyedBy() follows MOX: while it is on it names the transmit
// holder (for a key the Core's own position made, the station device, kind
// "station"), the trigger and the key's epoch; empty while unkeyed.
//
// ---- Keying on a filled buffer (Task 36) ----
// For a device whose media carries a microphone line, a tx.key in a mode
// that transmits the microphone (every mode but CWL and CWU) waits for the
// line's buffer to reach its target (30 ms on a steady link; R-IOS-13
// 2026-09-27) and then keys; if it has not
// within 250 ms it is refused micNotReady. The holder's own refusals come
// first, at once. Copies of the key, and a new-id key from the same device,
// wait with it and get its answer; a tx.unkey from the device while it
// waits cancels it (the key is answered keyEnded). A session that ends
// while its key waits is forgotten with it. TUNE and two-tone use no
// microphone and key at once, as does a device without a microphone line
// (the station's own source then, as before Task 36).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 35 (R-IOS-13), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-25: iPhone app plan Task 36 (R-IOS-13): a key waits for the
//               microphone line's buffer (MicUplink), answered later
//               through handle()'s reply. J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
//   2026-09-26: Transmit group fix wave C1: a voice or program key from a
//               device with no microphone line is refused micNotReady at
//               once (after the session gate and the holder), never keyed
//               on the Core's own microphone; setSessionGate. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-26: iPhone app plan Task 77 (R-IOS-02, R-IOS-03, R-IOS-13):
//               tx.tunerTune (the Tuner Genius autotune) as a key; the
//               session gate first for two-tone. J.J. Boyd (KG4VCF), with AI-
//               assisted implementation via Anthropic Claude Code.
//   2026-09-26: iPhone app plan Task 77 fix round 2: keyPending (the Core
//               never switches the Power Genius around it) and
//               pendingKeyEnded; tx.tunerTune refused while the device's
//               two-tone settles, its key waits for its microphone or any
//               start is pending; the device's own tx.key and two-tone
//               refused while its autotune carrier is up. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-26: iPhone app plan Task 77 fix round 4 (R-IOS-02, R-IOS-03,
//               R-IOS-13): a device's tunerTune that ends without keying
//               tells that device why (notice tuneEnded). J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-10-01: Tune-ended lane: tunerTuneEndedUnkeyed for every such
//               end but the device's own stop (m_endingOwnAutotune), with
//               a reason always. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
// =================================================================
#pragma once

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>

#include <functional>
#include <optional>

#include "core/safety/TxRefusal.h"

namespace NereusSDR {

class RadioModel;
class TransmitHolder;
struct KeyerIdentity;

class RemoteKeying : public QObject {
    Q_OBJECT

public:
    /// TunerTune (Task 77): the Tuner Genius autotune, tx.tunerTune.
    enum class Verb { Key, Unkey, Tune, TwoTone, TunerTune };

    struct Command {
        Verb verb{Verb::Key};
        /// The device the session is for (the session registry's id).
        QByteArray deviceId;
        /// The session the command arrived on; copies are this session's.
        QString session;
        quint32 commandId{0};
        /// tx.key: "screen", "headset", "bluetooth", "actionButton" or
        /// "tci" (a program's key).
        QByteArray trigger;
        /// tx.tune, tx.twoTone, tx.tunerTune.
        bool on{true};
        /// tx.unkey: the epoch of the key it ends.
        quint32 epoch{0};
    };

    struct Result {
        bool accepted{false};
        /// An accepted key's epoch; 0 for tx.unkey and every stop, whose
        /// results carry none.
        quint32 epoch{0};
        /// Why it was refused, as a transmit refusal; empty for the reasons
        /// below.
        TxRefusal refusal;
        /// Why it was refused, in plain words, when no transmit refusal
        /// says it (the Core has no radio, two-tone cannot run).
        QString reason;
        /// Task 36: handle(const Command&) only: the key is waiting for the
        /// microphone line's buffer; its answer goes to the reply of
        /// handle(command, reply).
        bool pending{false};
    };

    /// Task 36: the microphone line of the device a key is for.
    struct MicUplink {
        /// True while `deviceId`'s media carries a microphone line.
        std::function<bool(const QByteArray& deviceId)> carriesMic;
        /// Puts the line in use for the key and calls done(true) once its
        /// buffer holds the target, or done(false) when it has not within
        /// the deadline.
        std::function<void(const QByteArray& deviceId, std::function<void(bool)> done)> prime;
        /// The key that primed it has keyed or been refused (or the wait was
        /// cancelled); from here the line follows who is keyed.
        std::function<void(const QByteArray& deviceId)> endPriming;
    };
    void setMicUplink(MicUplink uplink);

    /// Fix wave C1: the session's own transmit gate (remote_transmit, its
    /// hello, its pairing, its snapshot) for the connection `command` came
    /// on, as a question (nothing changes): empty when permitted. Asked
    /// before a key without a microphone line is refused, so a device that
    /// may not transmit is told that first. Unset, nothing is refused by it.
    using SessionGate = std::function<TxRefusal(const Command& command)>;
    void setSessionGate(SessionGate gate) { m_sessionGate = std::move(gate); }

    using Reply = std::function<void(const Result& result)>;

    /// The trigger names tx.key takes.
    static bool isTrigger(const QByteArray& trigger);
    static constexpr char kProgramTrigger[] = "tci";

    /// How many commands each session's copies are remembered for.
    static constexpr int kRememberedCommands = 32;

    /// `model` is the Core's Local RadioModel; `holder` the Core's transmit
    /// holder. Neither is owned.
    RemoteKeying(RadioModel* model, TransmitHolder* holder, QObject* parent = nullptr);

    Result handle(const Command& command);
    /// The same, answering through `reply`: at once, or (Task 36) once a
    /// key's microphone buffer has filled or timed out. `reply` runs once
    /// per command.
    void handle(const Command& command, Reply reply);

    /// The session ended: its commands are forgotten.
    void forgetSession(const QString& session);

    /// iPhone app plan Task 77 fix wave, I4: a take ends `deviceId`'s Tuner
    /// Genius autotune (keyed or still waiting for the amplifier) before
    /// the transfer; its pending epoch goes with it.
    void endAutotuneFor(const QByteArray& deviceId);

    /// iPhone app plan Task 77 fix round 2: a device's key is about to
    /// start: one waiting for its microphone, or a two-tone admitted and
    /// settling. (A device's Tuner Genius autotune waiting for the
    /// amplifier is not counted: it is the cycle switching it.)
    /// Synchronous voice/tune/two-tone admission is pending too, including
    /// callbacks before MOX or an asynchronous generated start commits.
    bool keyPending() const;

signals:
    /// Fix round 2: a pending key ended (keyed, refused or cancelled); the
    /// Core retries an amplifier switch it owes.
    void pendingKeyEnded();
    /// Task 77 fix round 4: `deviceId`'s tx.tunerTune (already answered
    /// accepted) ended without keying, for `reason`; the Core tells it.
    /// Tune-ended lane: for every such end but the device's own stop, and
    /// `reason` is never empty.
    void tunerTuneEndedUnkeyed(const QByteArray& deviceId, const QString& reason);

private:
    /// The key itself, after any wait: the gates, MOX, the epoch.
    Result keyNow(const Command& command);
    /// Fix wave C1: tx.key in a mode that transmits the operator's voice.
    bool keyNeedsMicrophone(const Command& command) const;
    /// Whether this key waits for `command.deviceId`'s microphone line.
    bool keyWaitsForMicrophone(const Command& command) const;
    void finishWait(const QByteArray& deviceId, const Result& result);
    void finishSynchronousAdmission();
    void notifyPendingKeyEnded();
    Result unkey(const Command& command);
    Result tune(const Command& command);
    Result twoTone(const Command& command);
    /// iPhone app plan Task 77: tx.tunerTune.
    Result tunerTune(const Command& command);
    /// The device's Tuner Genius autotune is running (keyed or not yet).
    bool autotuneRunningFor(const QByteArray& deviceId) const;
    Result stopFrom(const QByteArray& deviceId, bool deviceKeyOn);

    /// The device's key is on, or its two-tone is on its way to keying,
    /// with `epoch` (0: any epoch).
    bool keyLive(const QByteArray& deviceId, quint32 epoch = 0) const;
    /// The device's own key is on the air now (MOX keyed for it).
    bool moxKeyedFor(const QByteArray& deviceId) const;
    bool twoToneRunningFor(const QByteArray& deviceId) const;
    std::optional<Result> copyOf(const Command& command) const;
    void remember(const Command& command, const Result& result);
    void publishKeyedBy();
    QByteArray stationTrigger(const KeyerIdentity& keyer) const;
    /// The epoch the next key takes (spent only when it keys).
    quint32 nextEpoch() const;

    QPointer<RadioModel> m_model;
    QPointer<TransmitHolder> m_holder;

    struct Pending {
        QByteArray deviceId;
        QByteArray trigger;
        quint32 epoch{0};
    };
    /// The key being started for a device, whose epoch and trigger keyedBy
    /// takes when MOX comes on for it.
    std::optional<Pending> m_pending;
    // Separate from m_pending: the TGXL cycle owns a pending epoch while
    // waiting for the amplifier and must keep its existing exemption.
    bool m_synchronousAdmission{false};
    bool m_pendingEndDeferred{false};
    /// The keyer of the key now on (empty while unkeyed).
    QByteArray m_liveKeyer;

    struct Remembered {
        Verb verb{Verb::Key};
        quint32 commandId{0};
        Result result;
    };
    QHash<QString, QList<Remembered>> m_sessions;

    // Only reply collation: lexical generated admissions retain exact
    // copies until their one authoritative answer is known. No key/source
    // authority or pending epoch lives here. Scopes remain discoverable
    // during delivery so session teardown can drop remaining callbacks.
    struct GeneratedReplies {
        Command command;
        QList<Reply> copies;
        GeneratedReplies* previous{nullptr};
        bool joinable{true};
        bool forgotten{false};
    };
    GeneratedReplies* m_generatedReplies{nullptr};

    // Task 36: keys waiting for a microphone buffer, one per device.
    struct Waiting {
        Command command;
        QList<Reply> replies;
        quint64 generation{0};
    };
    QHash<QByteArray, Waiting> m_waiting;
    quint64 m_waitGeneration{0};
    MicUplink m_mic;
    SessionGate m_sessionGate;
    /// Tune-ended lane: set while the device's own tx.tune or tx.tunerTune
    /// off ends its autotune, which is not told (its answer says it).
    QByteArray m_endingOwnAutotune;
};

} // namespace NereusSDR
