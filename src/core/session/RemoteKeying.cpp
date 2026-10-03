// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/RemoteKeying.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 35 (R-IOS-13). See RemoteKeying.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 35 (R-IOS-13), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-25: iPhone app plan Task 36 (R-IOS-13): keying on a filled
//               microphone buffer. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-09-26: Transmit group fix wave C1: a voice or program key from a
//               device with no microphone line is refused micNotReady at
//               once (after the session gate and the holder), never keyed
//               on the Core's own microphone; setSessionGate. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-26: Transmit group fix wave: M4 a remote line with no sound
//               says to wait for the device's microphone; M5 the holder's
//               press during its own VOX key is that key; M10 keys name
//               their connection. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-26: iPhone app plan Task 77 (R-IOS-02, R-IOS-03, R-IOS-13):
//               tunerTune, a waiting autotune ended by TUNE off, the session
//               gate first for two-tone. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-26: Task 77 fix wave: I3 tunerTune refused on the air, and the
//               device's own keys refused while its autotune waits; I4
//               endAutotuneFor. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-26: iPhone app plan Task 77 fix round 4 (R-IOS-02, R-IOS-03,
//               R-IOS-13): a device's tunerTune that ends without keying
//               tells that device why (notice tuneEnded). J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-10-01: TX diagnostics lane: the keying lines name the device by
//               its id in hex, as the unkey line does, not its raw bytes.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-01: Tune-ended lane: a device's tunerTune that ends before its
//               carrier keyed is always told (the path's reason, else a
//               backstop), except when the device's own stop ended it; a
//               take's end names the take. J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
// =================================================================

#include "core/session/RemoteKeying.h"

#include "core/LogCategories.h"
#include "core/MoxController.h"
#include "core/TgxlConnection.h"
#include "core/TwoToneController.h"
#include "core/safety/TransmitHolder.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <QScopedValueRollback>

#include <limits>
#include <memory>

namespace NereusSDR {

namespace {

// Task 77: a Tuner Genius autotune that is running (its own words, plain).
constexpr char kTunerTuning[] = "The tuner is already tuning.";

// The refusal a key call reports, captured while it runs: the transmit
// refusal MoxController sends (moxRefused) and TUNE's own plain reason
// (tuneRefused), whichever came last.
class RefusalCapture {
public:
    RefusalCapture(MoxController* mox, RadioModel* model)
    {
        if (mox != nullptr) {
            m_mox = QObject::connect(mox, &MoxController::moxRefused,
                                     [this](const TxRefusal& refusal) { m_refusal = refusal; });
        }
        if (model != nullptr) {
            m_tune = QObject::connect(model, &RadioModel::tuneRefused,
                                      [this](const QString& reason) { m_reason = reason; });
        }
    }
    ~RefusalCapture()
    {
        QObject::disconnect(m_mox);
        QObject::disconnect(m_tune);
    }
    RefusalCapture(const RefusalCapture&) = delete;
    RefusalCapture& operator=(const RefusalCapture&) = delete;

    const TxRefusal& refusal() const { return m_refusal; }
    const QString& reason() const { return m_reason; }

private:
    QMetaObject::Connection m_mox;
    QMetaObject::Connection m_tune;
    TxRefusal m_refusal;
    QString m_reason;
};

RemoteKeying::Result accepted(quint32 epoch)
{
    RemoteKeying::Result result;
    result.accepted = true;
    result.epoch = epoch;
    return result;
}

RemoteKeying::Result refused(const TxRefusal& refusal)
{
    RemoteKeying::Result result;
    result.refusal = refusal;
    result.reason = refusal.text;
    return result;
}

RemoteKeying::Result refusedPlain(const QString& reason)
{
    RemoteKeying::Result result;
    result.reason = reason;
    return result;
}

} // namespace

bool RemoteKeying::isTrigger(const QByteArray& trigger)
{
    return trigger == "screen" || trigger == "headset" || trigger == "bluetooth"
        || trigger == "actionButton" || trigger == kProgramTrigger;
}

RemoteKeying::RemoteKeying(RadioModel* model, TransmitHolder* holder, QObject* parent)
    : QObject(parent)
    , m_model(model)
    , m_holder(holder)
{
    if (m_model != nullptr && m_model->moxController() != nullptr) {
        connect(m_model->moxController(), &MoxController::stateChanged, this,
                [this](MoxState) { publishKeyedBy(); });
    }
    if (m_model != nullptr && m_model->twoToneController() != nullptr) {
        // A two-tone that ended (or never keyed) leaves no key to start.
        connect(m_model->twoToneController(), &TwoToneController::twoToneActiveChanged, this,
                [this](bool active) {
                    const TwoToneController* tt = m_model ? m_model->twoToneController() : nullptr;
                    if (!active && tt != nullptr && !tt->isActivationInFlight()
                        && m_pending.has_value() && m_pending->trigger == "twoTone") {
                        m_pending.reset();
                        emit pendingKeyEnded();
                    }
                });
    }
    if (m_model != nullptr) {
        // Task 77 fix round 2: a device's autotune that ended before its
        // carrier keyed (the amplifier never reported standby, MOX came on,
        // the carrier was refused) leaves no key to start.
        // Fix round 4: and when it ended for the amplifier's sake, the
        // device is told why (its tunerTune was answered accepted).
        connect(m_model, &RadioModel::tgxlAutotuneEnded, this,
                [this](const QByteArray& deviceId, const QString& unkeyedReason) {
            if (!deviceId.isEmpty() && m_pending.has_value() && m_pending->deviceId == deviceId
                && !moxKeyedFor(deviceId)) {
                m_pending.reset();
                // Tune-ended lane: every such end is told, so the device
                // never shows a tune that is over; its own stop is not (its
                // answer says it). An end with no reason gets the backstop.
                if (m_endingOwnAutotune != deviceId) {
                    QString reason = unkeyedReason;
                    if (reason.isEmpty()) {
                        qCWarning(lcDsp) << "Tuner autotune of" << deviceId.toHex().constData()
                                         << "ended before keying with no reason given";
                        reason = RadioModel::tunerTuneEndedReason(
                            RadioModel::TunerTuneEnd::NoReasonGiven);
                    }
                    emit tunerTuneEndedUnkeyed(deviceId, reason);
                }
                emit pendingKeyEnded();
            }
        });
    }
}

bool RemoteKeying::keyPending() const
{
    return !m_waiting.isEmpty()
        || (m_pending.has_value() && twoToneRunningFor(m_pending->deviceId));
}

void RemoteKeying::setMicUplink(MicUplink uplink)
{
    m_mic = std::move(uplink);
}

RemoteKeying::Result RemoteKeying::handle(const Command& command)
{
    // The answer, if it comes before this returns; a later one (a key
    // waiting for its microphone buffer) lands in state nobody reads.
    const auto answer = std::make_shared<std::optional<Result>>();
    handle(command, [answer](const Result& result) { *answer = result; });
    if (answer->has_value()) {
        return **answer;
    }
    Result pending;
    pending.pending = true;
    return pending;
}

void RemoteKeying::handle(const Command& command, Reply reply)
{
    if (m_model.isNull() || m_model->moxController() == nullptr || m_holder.isNull()) {
        reply(refusedPlain(QStringLiteral("The Core has no radio ready.")));
        return;
    }
    // Task 36: the device's key is waiting for its microphone buffer.
    if (const auto waiting = m_waiting.find(command.deviceId); waiting != m_waiting.end()) {
        if (command.verb == Verb::Key) {
            // A copy, or a new press from the same device: answered with
            // the waiting key's answer.
            waiting->replies.append(std::move(reply));
            return;
        }
        if (command.verb == Verb::Unkey) {
            // Released before it keyed: it never keys.
            finishWait(command.deviceId, refused(TxRefusals::keyEnded()));
        }
    }
    if (const std::optional<Result> copy = copyOf(command)) {
        reply(*copy);
        return;
    }
    // Task 77 fix wave, I3: while this device's Tuner Genius autotune waits
    // for the amplifier's standby, its own key, TUNE and two-tone are
    // refused: the cycle keys the tune carrier itself, and its pending
    // epoch is never overwritten by another start.
    // Fix round 2: once its carrier is up too, its tx.key and two-tone are
    // refused (a voice key or the tones never replace the tune carrier
    // while the tuner sweeps); its TUNE on is the carrier already on.
    const bool starts = command.verb == Verb::Key
        || ((command.verb == Verb::Tune || command.verb == Verb::TwoTone) && command.on);
    if (starts && autotuneRunningFor(command.deviceId)
        && (command.verb != Verb::Tune || !moxKeyedFor(command.deviceId))) {
        const Result result = refusedPlain(QString::fromLatin1(kTunerTuning));
        remember(command, result);
        reply(result);
        return;
    }
    // Fix wave C1: a key that carries the operator's voice (every mode but
    // CW) from a device whose media carries no microphone line is refused
    // at once; the Core never keys a remote device on its own microphone.
    // The session's own gate and the holder's refusal come first, as the
    // keying gate orders them. TUNE and two-tone need no microphone.
    if (keyNeedsMicrophone(command) && !moxKeyedFor(command.deviceId)
        && !(m_mic.carriesMic && m_mic.carriesMic(command.deviceId))) {
        TxRefusal refusal = m_sessionGate ? m_sessionGate(command) : TxRefusal{};
        if (refusal.isEmpty()) {
            refusal = m_holder->keyRefusalFor(command.deviceId, command.trigger == kProgramTrigger);
        }
        if (refusal.isEmpty()) {
            refusal = TxRefusals::micNotConnected();
            qCInfo(lcDsp) << "Key from" << command.deviceId.toHex().constData()
                          << "refused: its microphone line is not open";
        }
        const Result result = refused(refusal);
        remember(command, result);
        reply(result);
        return;
    }
    if (keyWaitsForMicrophone(command) && !moxKeyedFor(command.deviceId)) {
        // The holder's own refusal first, at once (a question only).
        if (const TxRefusal refusal =
                m_holder->keyRefusalFor(command.deviceId, command.trigger == kProgramTrigger);
            !refusal.isEmpty()) {
            const Result result = refused(refusal);
            remember(command, result);
            reply(result);
            return;
        }
        Waiting waiting;
        waiting.command = command;
        waiting.replies.append(std::move(reply));
        waiting.generation = ++m_waitGeneration;
        const quint64 generation = waiting.generation;
        const QByteArray deviceId = command.deviceId;
        m_waiting.insert(deviceId, std::move(waiting));
        qCInfo(lcDsp) << "Key from" << deviceId.toHex().constData() << "waits for its microphone";
        QPointer<RemoteKeying> self(this);
        m_mic.prime(deviceId, [self, deviceId, generation](bool ready) {
            if (self.isNull()) {
                return;
            }
            const auto it = self->m_waiting.constFind(deviceId);
            if (it == self->m_waiting.cend() || it->generation != generation) {
                return;
            }
            const Command waited = it->command;
            const Result result = ready ? self->keyNow(waited) : refused(TxRefusals::remoteMicNotReady());
            if (!ready) {
                qCInfo(lcDsp) << "Key from" << deviceId.toHex().constData()
                              << "refused: no microphone audio within the deadline";
            }
            self->finishWait(deviceId, result);
        });
        return;
    }
    Result result;
    switch (command.verb) {
    case Verb::Key:
        result = keyNow(command);
        break;
    case Verb::Unkey:
        result = unkey(command);
        break;
    case Verb::Tune:
        result = tune(command);
        break;
    case Verb::TwoTone:
        result = twoTone(command);
        break;
    case Verb::TunerTune:
        result = tunerTune(command);
        break;
    }
    remember(command, result);
    reply(result);
}

void RemoteKeying::endAutotuneFor(const QByteArray& deviceId)
{
    if (!autotuneRunningFor(deviceId)) {
        return;
    }
    // Tune-ended lane: the device is told its tune stopped for the take.
    m_model->cancelTgxlAutotuneFor(
        deviceId, RadioModel::tunerTuneEndedReason(RadioModel::TunerTuneEnd::TransmitTaken));
    if (m_pending.has_value() && m_pending->deviceId == deviceId) {
        m_pending.reset();
    }
    qCInfo(lcDsp) << "Tuner autotune of" << deviceId.toHex().constData() << "ended: transmit is being taken";
}

void RemoteKeying::forgetSession(const QString& session)
{
    m_sessions.remove(session);
    // Task 36: a key of this session still waiting never keys, and nobody
    // is left to answer.
    QList<QByteArray> devices;
    for (auto it = m_waiting.cbegin(); it != m_waiting.cend(); ++it) {
        if (it->command.session == session) {
            devices.append(it.key());
        }
    }
    for (const QByteArray& device : std::as_const(devices)) {
        m_waiting.remove(device);
        if (m_mic.endPriming) {
            m_mic.endPriming(device);
        }
    }
}

bool RemoteKeying::keyNeedsMicrophone(const Command& command) const
{
    if (command.verb != Verb::Key) {
        return false;
    }
    // Every mode but CW, whose key comes from the keyer. With no transmit
    // slice the mode is unknown, so the microphone is needed.
    const SliceModel* slice = m_model->txBoundSlice();
    if (slice == nullptr) {
        return true;
    }
    const DSPMode mode = slice->dspMode();
    return mode != DSPMode::CWL && mode != DSPMode::CWU;
}

bool RemoteKeying::keyWaitsForMicrophone(const Command& command) const
{
    if (command.verb != Verb::Key || !m_mic.carriesMic || !m_mic.prime
        || !m_mic.carriesMic(command.deviceId)) {
        return false;
    }
    // A mode that transmits the microphone: every mode but CW, whose key
    // comes from the keyer.
    const SliceModel* slice = m_model->txBoundSlice();
    if (slice == nullptr) {
        return false;
    }
    const DSPMode mode = slice->dspMode();
    return mode != DSPMode::CWL && mode != DSPMode::CWU;
}

void RemoteKeying::finishWait(const QByteArray& deviceId, const Result& result)
{
    const auto it = m_waiting.find(deviceId);
    if (it == m_waiting.end()) {
        return;
    }
    Waiting waiting = std::move(*it);
    m_waiting.erase(it);
    if (m_mic.endPriming) {
        m_mic.endPriming(deviceId);
    }
    remember(waiting.command, result);
    for (const Reply& reply : std::as_const(waiting.replies)) {
        if (reply) {
            reply(result);
        }
    }
    emit pendingKeyEnded();
}

// ---- Copies (the pairing design, section 9.6) ------------------------------

std::optional<RemoteKeying::Result> RemoteKeying::copyOf(const Command& command) const
{
    const auto it = m_sessions.constFind(command.session);
    if (it == m_sessions.cend()) {
        return std::nullopt;
    }
    for (const Remembered& earlier : *it) {
        if (earlier.verb != command.verb || earlier.commandId != command.commandId) {
            continue;
        }
        // A key the Core acted on: the same answer while that key is on;
        // once it has ended (a safety stop, a release, a take), the copy is
        // refused and never keys again.
        const bool startsAKey = command.verb == Verb::Key
            || ((command.verb == Verb::Tune || command.verb == Verb::TwoTone) && command.on);
        if (startsAKey && earlier.result.accepted
            && !keyLive(command.deviceId, earlier.result.epoch)) {
            return refused(TxRefusals::keyEnded());
        }
        return earlier.result;
    }
    return std::nullopt;
}

void RemoteKeying::remember(const Command& command, const Result& result)
{
    QList<Remembered>& list = m_sessions[command.session];
    list.append(Remembered{command.verb, command.commandId, result});
    while (list.size() > kRememberedCommands) {
        list.removeFirst();
    }
}

// ---- What is on ---------------------------------------------------------------

bool RemoteKeying::moxKeyedFor(const QByteArray& deviceId) const
{
    const MoxController* mox = m_model ? m_model->moxController() : nullptr;
    return mox != nullptr && mox->isMox() && mox->currentKeyer().deviceId == deviceId;
}

bool RemoteKeying::twoToneRunningFor(const QByteArray& deviceId) const
{
    const TwoToneController* tt = m_model ? m_model->twoToneController() : nullptr;
    return tt != nullptr && (tt->isActive() || tt->isActivationInFlight())
        && tt->keyer().deviceId == deviceId;
}

bool RemoteKeying::keyLive(const QByteArray& deviceId, quint32 epoch) const
{
    if (moxKeyedFor(deviceId)) {
        return epoch == 0 || m_model->keyedBy().epoch == epoch;
    }
    // A two-tone start admitted and on its way to keying (its settle), or
    // (Task 77) a Tuner Genius autotune waiting for the amplifier.
    if ((twoToneRunningFor(deviceId) || autotuneRunningFor(deviceId)) && m_pending.has_value()
        && m_pending->deviceId == deviceId) {
        return epoch == 0 || m_pending->epoch == epoch;
    }
    return false;
}

bool RemoteKeying::autotuneRunningFor(const QByteArray& deviceId) const
{
    return m_model && !deviceId.isEmpty() && m_model->tgxlAutotuneDeviceId() == deviceId;
}

// ---- The verbs ------------------------------------------------------------------

RemoteKeying::Result RemoteKeying::keyNow(const Command& command)
{
    MoxController* mox = m_model->moxController();
    // A key with a new id while this device's own key is on changes
    // nothing: the key already on answers.
    if (moxKeyedFor(command.deviceId)) {
        return accepted(m_model->keyedBy().epoch);
    }
    // Fix wave M5: the holder's press while its own VOX key is on (ruling
    // 8.4: that key is the holder's) is its key already on, answered with
    // its epoch; its tx.unkey then ends the VOX key, as for any key of its.
    if (mox->isMox() && mox->currentKeyer().isStation() && mox->currentKeyer().source == PttMode::Vox
        && m_holder->isHeldBy(command.deviceId) && m_model->keyedBy().deviceId == command.deviceId) {
        return accepted(m_model->keyedBy().epoch);
    }
    KeyerIdentity keyer;
    keyer.deviceId = command.deviceId;
    keyer.source = PttMode::None;
    // D58: a program's key never takes transmit (TransmitHolder::askKey).
    keyer.program = command.trigger == kProgramTrigger;
    keyer.session = command.session;   // fix wave M10

    // The key's epoch is the next one; it is spent only if the key keys.
    m_pending = Pending{command.deviceId, command.trigger, nextEpoch()};
    RefusalCapture capture(mox, nullptr);
    mox->setMox(true, keyer);
    if (moxKeyedFor(command.deviceId)) {
        m_pending.reset();
        qCInfo(lcDsp) << "Keyed for" << command.deviceId.toHex().constData() << "by" << command.trigger
                      << "epoch" << m_model->keyedBy().epoch;
        return accepted(m_model->keyedBy().epoch);
    }
    m_pending.reset();
    TxRefusal refusal = capture.refusal();
    if (refusal.isEmpty()) {
        refusal = mox->lastRefusal().isEmpty() ? TxRefusals::changingHands() : mox->lastRefusal();
    }
    return refused(refusal);
}

RemoteKeying::Result RemoteKeying::unkey(const Command& command)
{
    // The device's own key, or its two-tone on its way to keying.
    if (keyLive(command.deviceId)) {
        const quint32 live = moxKeyedFor(command.deviceId) ? m_model->keyedBy().epoch
                                                           : m_pending->epoch;
        if (command.epoch < live) {
            // An unkey for an earlier key of this device: ignored.
            return accepted(0);
        }
        return stopFrom(command.deviceId, true);
    }
    // A VOX key while this device holds transmit is this device's (ruling
    // 8.4), so its release stops it too.
    const MoxController* mox = m_model->moxController();
    if (m_holder->isHeldBy(command.deviceId) && mox->isMox() && mox->currentKeyer().isStation()
        && mox->currentKeyer().source == PttMode::Vox) {
        if (command.epoch < m_model->keyedBy().epoch) {
            return accepted(0);
        }
        m_model->moxController()->setMox(false);
        qCInfo(lcDsp) << "Unkeyed the VOX key of" << command.deviceId.toHex().constData();
        return accepted(0);
    }
    return stopFrom(command.deviceId, false);
}

RemoteKeying::Result RemoteKeying::stopFrom(const QByteArray& deviceId, bool deviceKeyOn)
{
    if (deviceKeyOn) {
        // Ruling 8.5: a release unkeys only this device's own key. TUNE and
        // two-tone end their own way, restoring what they changed.
        if (m_model->isTune() && moxKeyedFor(deviceId)) {
            m_model->setTune(false);
        } else if (twoToneRunningFor(deviceId)) {
            m_model->twoToneController()->setActive(false);
        } else {
            KeyerIdentity keyer;
            keyer.deviceId = deviceId;
            m_model->moxController()->setMox(false, keyer);
        }
        qCInfo(lcDsp) << "Unkeyed for" << deviceId.toHex().constData();
        return accepted(0);
    }
    // Nothing of this device's is on. From a device that does not hold
    // transmit, the holder's release is refused and the transmission
    // continues (ruling 8.5).
    if (const std::optional<TransmitHolder::Holder> holder = m_holder->holder()) {
        if (holder->deviceId != deviceId) {
            return refused(TxRefusals::otherDeviceHoldsStop(holder->name));
        }
    }
    return accepted(0);
}

RemoteKeying::Result RemoteKeying::tune(const Command& command)
{
    if (!command.on) {
        // Task 77: TUNE off also ends this device's Tuner Genius cycle,
        // keyed or still waiting for the amplifier.
        if (autotuneRunningFor(command.deviceId) && !moxKeyedFor(command.deviceId)) {
            // Tune-ended lane: the device's own stop; its answer tells it.
            const QScopedValueRollback<QByteArray> own(m_endingOwnAutotune, command.deviceId);
            m_model->cancelTgxlAutotuneFor(command.deviceId);
            if (m_pending.has_value() && m_pending->deviceId == command.deviceId) {
                m_pending.reset();
            }
            return accepted(0);
        }
        return stopFrom(command.deviceId, m_model->isTune() && moxKeyedFor(command.deviceId));
    }
    if (m_model->isTune() && moxKeyedFor(command.deviceId)) {
        return accepted(m_model->keyedBy().epoch);
    }
    MoxController* mox = m_model->moxController();
    KeyerIdentity keyer;
    keyer.deviceId = command.deviceId;
    keyer.source = PttMode::Manual;
    keyer.session = command.session;   // fix wave M10
    m_pending = Pending{command.deviceId, QByteArrayLiteral("tune"), nextEpoch()};
    RefusalCapture capture(mox, m_model.data());
    m_model->setTune(true, keyer);
    const bool keyed = m_model->isTune() && moxKeyedFor(command.deviceId);
    m_pending.reset();
    if (keyed) {
        return accepted(m_model->keyedBy().epoch);
    }
    if (!capture.refusal().isEmpty()) {
        return refused(capture.refusal());
    }
    if (!capture.reason().isEmpty()) {
        return refusedPlain(capture.reason());
    }
    return refusedPlain(QStringLiteral("The Core could not start TUNE."));
}

RemoteKeying::Result RemoteKeying::twoTone(const Command& command)
{
    TwoToneController* tt = m_model->twoToneController();
    if (!command.on) {
        return stopFrom(command.deviceId, tt != nullptr && twoToneRunningFor(command.deviceId));
    }
    if (tt == nullptr) {
        return refusedPlain(QStringLiteral("The two-tone test is not available on this Core."));
    }
    // Task 77: the session's own gate, then the holder's refusal (questions
    // only; nothing changes), so a device that cannot key hears why before
    // two-tone's own checks.
    if (m_sessionGate) {
        if (const TxRefusal refusal = m_sessionGate(command); !refusal.isEmpty()) {
            return refused(refusal);
        }
    }
    if (const TxRefusal refusal = m_holder->keyRefusalFor(command.deviceId);
        !refusal.isEmpty()) {
        return refused(refusal);
    }
    if (twoToneRunningFor(command.deviceId)) {
        return accepted(moxKeyedFor(command.deviceId) ? m_model->keyedBy().epoch
                        : m_pending.has_value()      ? m_pending->epoch
                                                     : 0);
    }
    MoxController* mox = m_model->moxController();
    KeyerIdentity keyer;
    keyer.deviceId = command.deviceId;
    keyer.source = PttMode::None;
    keyer.session = command.session;   // fix wave M10
    const quint32 epoch = nextEpoch();
    m_pending = Pending{command.deviceId, QByteArrayLiteral("twoTone"), epoch};
    RefusalCapture capture(mox, nullptr);
    tt->setActive(true, keyer);
    if (twoToneRunningFor(command.deviceId)) {
        // Two-tone keys after its own settle: its epoch is spent now, so
        // no other key takes it meanwhile.
        if (!moxKeyedFor(command.deviceId) && m_model->keyingEpoch() != epoch) {
            m_model->advanceKeyingEpoch();
        }
        // Two-tone keys after its own settle; its key takes this epoch.
        if (moxKeyedFor(command.deviceId)) {
            m_pending.reset();
            return accepted(m_model->keyedBy().epoch);
        }
        return accepted(epoch);
    }
    m_pending.reset();
    if (!capture.refusal().isEmpty()) {
        return refused(capture.refusal());
    }
    return refusedPlain(QStringLiteral("The two-tone test could not start on the Core."));
}

RemoteKeying::Result RemoteKeying::tunerTune(const Command& command)
{
    // iPhone app plan Task 77 (controller addition; rulings 7.7, 8.3): the
    // Tuner Genius autotune keys the radio at its tune power, so it is a
    // key for every rule: the session gate, TX inhibit, the PA trip,
    // receive only and the interlock, then the holder (a person's key on
    // unheld transmit takes it; another device's is refused). The carrier
    // keys as this device once the amplifier is in standby, so the
    // watchdog watches it as any key of the device's.
    if (!command.on) {
        if (autotuneRunningFor(command.deviceId)) {
            // Tune-ended lane: the device's own stop; its answer tells it.
            const QScopedValueRollback<QByteArray> own(m_endingOwnAutotune, command.deviceId);
            m_model->cancelTgxlAutotuneFor(command.deviceId);
            if (m_pending.has_value() && m_pending->deviceId == command.deviceId) {
                m_pending.reset();
            }
            qCInfo(lcDsp) << "Tuner autotune canceled by" << command.deviceId.toHex().constData();
            return accepted(0);
        }
        return stopFrom(command.deviceId, false);
    }
    if (autotuneRunningFor(command.deviceId)) {
        return accepted(moxKeyedFor(command.deviceId) ? m_model->keyedBy().epoch
                        : m_pending.has_value()      ? m_pending->epoch
                                                     : 0);
    }
    // The holder's refusal first (a question only; nothing changes), so a
    // device that cannot key hears who has the transmitter; then whether
    // the tuner can run, asked before the gate so it takes nothing.
    if (m_sessionGate) {
        if (const TxRefusal refusal = m_sessionGate(command); !refusal.isEmpty()) {
            return refused(refusal);
        }
    }
    if (const TxRefusal refusal = m_holder->keyRefusalFor(command.deviceId); !refusal.isEmpty()) {
        return refused(refusal);
    }
    // Fix wave I3: never started under RF. The cycle switches the amplifier
    // to standby first, so while the radio is on the air (this device's
    // own key, or MOX still walking) it is refused with the on-air words.
    if (const MoxController* moxNow = m_model->moxController();
        moxNow->isMox() || moxNow->state() != MoxState::Rx || m_model->isTune()) {
        return refused(TxRefusals::radioOnAir());
    }
    // Fix round 2 (Important 1): nor while a start of this device's is on
    // its way to keying (its two-tone settling, its key waiting for its
    // microphone): that key would land while the amplifier changes over.
    // Nothing pending is ever overwritten.
    if (twoToneRunningFor(command.deviceId) || m_waiting.contains(command.deviceId)) {
        return refused(TxRefusals::radioOnAir());
    }
    if (m_pending.has_value()) {
        return refusedPlain(QString::fromLatin1(kTunerTuning));
    }
    QString unavailable;
    TgxlConnection* tgxl = m_model->tgxlConnection();
    if (tgxl == nullptr || !tgxl->isConnected()) {
        return refusedPlain(QStringLiteral("No Tuner Genius is connected to the Core."));
    }
    if (m_model->isTgxlAutotuneInProgress() || m_model->isTune()) {
        return refusedPlain(QString::fromLatin1(kTunerTuning));
    }
    MoxController* mox = m_model->moxController();
    KeyerIdentity keyer;
    keyer.deviceId = command.deviceId;
    keyer.source = PttMode::Manual;
    keyer.session = command.session;
    RefusalCapture capture(mox, nullptr);
    // The gate: TX inhibit, the PA trip, receive only, the interlock, then
    // the session and the holder. On unheld transmit this takes it.
    if (!mox->admitKey(keyer)) {
        if (!capture.refusal().isEmpty()) {
            return refused(capture.refusal());
        }
        return refused(mox->lastRefusal().isEmpty() ? TxRefusals::changingHands()
                                                    : mox->lastRefusal());
    }
    const quint32 epoch = nextEpoch();
    m_pending = Pending{command.deviceId, QByteArrayLiteral("tune"), epoch};
    if (!m_model->startTgxlAutotuneFor(keyer, &unavailable)) {
        m_pending.reset();
        return refusedPlain(unavailable.isEmpty()
                                ? QStringLiteral("The tuner could not start tuning.")
                                : unavailable);
    }
    // The carrier keys after the amplifier's standby: its epoch is spent
    // now, so no other key takes it meanwhile (as two-tone's).
    if (!moxKeyedFor(command.deviceId) && m_model->keyingEpoch() != epoch) {
        m_model->advanceKeyingEpoch();
    }
    qCInfo(lcDsp) << "Tuner autotune started for" << command.deviceId.toHex().constData() << "epoch" << epoch;
    return accepted(moxKeyedFor(command.deviceId) ? m_model->keyedBy().epoch : epoch);
}

// ---- Who is keyed ---------------------------------------------------------------

quint32 RemoteKeying::nextEpoch() const
{
    const quint32 now = m_model->keyingEpoch();
    return now == std::numeric_limits<quint32>::max() ? 1 : now + 1;
}

QByteArray RemoteKeying::stationTrigger(const KeyerIdentity& keyer) const
{
    switch (keyer.source) {
    case PttMode::Mic:
        return QByteArrayLiteral("radioPtt");
    case PttMode::Vox:
        return QByteArrayLiteral("vox");
    case PttMode::Tci:
        return QByteArrayLiteral("tci");
    case PttMode::Cat:
        return QByteArrayLiteral("cat");
    default:
        break;
    }
    if (m_model && m_model->isTune()) {
        return QByteArrayLiteral("tune");
    }
    return QByteArrayLiteral("station");
}

void RemoteKeying::publishKeyedBy()
{
    const MoxController* mox = m_model ? m_model->moxController() : nullptr;
    if (mox == nullptr) {
        return;
    }
    if (!mox->isMox()) {
        m_liveKeyer.clear();
        m_model->setKeyedBy({});
        return;
    }
    const KeyerIdentity& keyer = mox->currentKeyer();
    if (!m_liveKeyer.isEmpty() && m_liveKeyer == keyer.deviceId) {
        return;   // the same key, further along its walk
    }
    m_liveKeyer = keyer.deviceId;
    RadioModel::KeyedBy keyedBy;
    if (m_pending.has_value() && m_pending->deviceId == keyer.deviceId) {
        keyedBy.trigger = m_pending->trigger;
        keyedBy.epoch = m_pending->epoch;
        // Spent now (a two-tone's was spent when it was admitted).
        if (m_model->keyingEpoch() != keyedBy.epoch) {
            m_model->advanceKeyingEpoch();
        }
        // A two-tone's key, or (Task 77) an autotune's, has now started.
        if (m_pending->trigger == "twoTone" || autotuneRunningFor(m_pending->deviceId)) {
            m_pending.reset();
        }
    } else {
        keyedBy.trigger = keyer.isStation() ? stationTrigger(keyer) : QByteArrayLiteral("station");
        keyedBy.epoch = m_model->advanceKeyingEpoch();
    }
    // Section 2.2: keyedBy names the holder (VOX and a program's key name
    // the device that holds transmit).
    const std::optional<TransmitHolder::Holder> holder =
        m_holder ? m_holder->holder() : std::nullopt;
    if (holder.has_value()) {
        keyedBy.deviceId = holder->deviceId;
        keyedBy.deviceName = holder->name;
        keyedBy.deviceKind = holder->kind;
    } else if (keyer.isStation()) {
        keyedBy.deviceId = QByteArray(KeyerIdentity::kStationDeviceId);
        keyedBy.deviceName = QStringLiteral("Radio");
        keyedBy.deviceKind = QStringLiteral("station");
    } else {
        keyedBy.deviceId = keyer.deviceId;
    }
    m_model->setKeyedBy(keyedBy);
}

} // namespace NereusSDR
