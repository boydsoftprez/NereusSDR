// no-port-check: NereusSDR-original.
// =================================================================
// src/core/safety/TransmitHolder.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 34 (R-IOS-02). See TransmitHolder.h.
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
//   2026-09-26: Transmit group fix wave I3: assign and failTransfer clear
//               a dropped holder's fence, so it never outlives the
//               transfer that superseded it. J.J. Boyd (KG4VCF), with AI-
//               assisted implementation via Anthropic Claude Code.
//   2026-09-26: Transmit group fix wave 2: every holder on the air counts,
//               the station device's own keys included (onAirHolder),
//               exempt by change not by holder; ruling 8.11's freeze on
//               every path (XIT, pan moves, a stored change at proceed); a
//               hosting desktop's key named after it. J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude Code.
//   2026-09-26: Transmit group fix wave 2, Important 2: a refused TUNE or
//               two-tone takes nothing (admitKey asks TX inhibit, the PA
//               trip, receive only and the interlock before the gate; a
//               take whose key never starts is released). J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-26: iPhone app plan Task 77 (R-IOS-02, R-IOS-03): askTake;
//               releaseStationTake removed. J.J. Boyd (KG4VCF), with AI-
//               assisted implementation via Anthropic Claude Code.
//   2026-10-01: TX diagnostics lane: the device id prints as hex in the
//               log, not as raw bytes. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
// =================================================================

#include "core/safety/TransmitHolder.h"
#include <QPointer>
#include <QScopeGuard>

#include "core/LogCategories.h"

namespace NereusSDR {

namespace {

const QByteArray kStation = QByteArrayLiteral("station");

} // namespace

TransmitHolder::TransmitHolder(QObject* parent)
    : QObject(parent)
{
}

void TransmitHolder::setHooks(Hooks hooks)
{
    m_hooks = std::move(hooks);
}

qint64 TransmitHolder::now() const
{
    return m_hooks.clock ? m_hooks.clock() : 0;
}

bool TransmitHolder::moxOn() const
{
    return m_hooks.moxOn && m_hooks.moxOn();
}

void TransmitHolder::disarmVox()
{
    if (m_hooks.disarmVox) {
        m_hooks.disarmVox();
    }
}

std::optional<TransmitHolder::Holder> TransmitHolder::holder() const
{
    if (!m_holder.has_value()) {
        return std::nullopt;
    }
    Holder h = *m_holder;
    // Ruling 8.1: after a take by the radio's own PTT the holder is
    // "Radio", kind station, until the next change of holder. Otherwise
    // the words are the device's as the Core names it now (Task 71's
    // numbering), so a rename or a new numbering shows at once.
    if (h.source == Source::RadioPtt) {
        h.name = QStringLiteral("Radio");
        h.shortName = QStringLiteral("Radio");
        h.kind = QStringLiteral("station");
    } else if (h.deviceId == kStation) {
        // Ruling 8.1: a hosting desktop's own MOX or TUNE shows the
        // desktop's name (the owner describes the station device when a
        // desktop hosts the Core); on a Core with no desktop the station
        // device's own keys are the radio's.
        const std::optional<Words> words =
            m_hooks.describe ? m_hooks.describe(h.deviceId) : std::nullopt;
        if (words && !words->name.isEmpty()) {
            h.name = words->name;
            h.shortName = words->shortName.isEmpty() ? words->name : words->shortName;
        } else if (h.name.isEmpty()) {
            h.name = QStringLiteral("Radio");
            h.shortName = QStringLiteral("Radio");
        }
        h.kind = QStringLiteral("station");
    } else if (m_hooks.describe) {
        if (const std::optional<Words> words = m_hooks.describe(h.deviceId)) {
            h.name = words->name;
            h.shortName = words->shortName;
            h.kind = words->kind;
        }
    }
    return h;
}

bool TransmitHolder::isHeldBy(const QByteArray& deviceId) const
{
    return m_state == State::Held && m_holder.has_value() && m_holder->deviceId == deviceId;
}

TxRefusal TransmitHolder::keyRefusalFor(const QByteArray& deviceId, bool program) const
{
    // Step 2's end with MOX still on (a transfer's, or a dropped holder's
    // fence): until it reads off.
    if (m_stopUnconfirmed) {
        return TxRefusals::stopNotConfirmed();
    }
    // Ruling 8.2 step 1, and the dropped holder's fence (ruling 8.15).
    if (m_state == State::Transferring || m_fenced || m_keyingBlockDepth > 0) {
        return TxRefusals::changingHands();
    }
    if (m_state == State::Held && m_holder.has_value()) {
        if (m_holder->deviceId == deviceId) {
            return {};
        }
        // Ruling 8.3: another device's key names the holder, away or not.
        return TxRefusals::otherDeviceHolds(holder()->name);
    }
    // Unheld. A program never takes transmit (D58, D63).
    if (program) {
        return TxRefusals::programNeedsTransmit();
    }
    return {};
}

KeyingAnswer TransmitHolder::askKey(const KeyRequest& request)
{
    const TxRefusal refusal = keyRefusalFor(request.deviceId, request.program);
    if (!refusal.isEmpty()) {
        return {KeyingVerdict::Refuse, refusal};
    }
    if (m_state == State::Held) {
        return {KeyingVerdict::Admit, {}};
    }
    // Unheld: a person's key takes transmit and keys (D63); the station
    // device's own keys likewise. Nobody is keyed and MOX
    // reads off, so the transfer has nothing to unkey and ends at once.
    Holder next;
    next.deviceId = request.deviceId;
    next.source = request.source;
    ++m_epoch;
    next.sinceMs = now();
    m_holder = next;
    m_state = State::Held;
    // Fix wave 2: until its key starts, the take can be released.
    m_takeUnstarted = true;
    // Ruling 8.4: VOX is disarmed at every change of holder. The station
    // device's own VOX key is the one exception: disarming VOX would end
    // the very key being admitted. (Refusing to arm VOX without holding
    // transmit is Task 77's.)
    if (!request.vox) {
        disarmVox();
    }
    qCInfo(lcDsp) << "Transmit taken by" << next.deviceId.toHex().constData() << "(nobody held it)";
    emit changed();
    return {KeyingVerdict::Admit, {}};
}

TransmitHolder::TakeAnswer TransmitHolder::askTake(const QByteArray& requester,
                                                   std::optional<quint64> shownEpoch,
                                                   std::optional<bool> shownKeyed) const
{
    if (m_stopUnconfirmed) {
        return {TakeVerdict::Refuse, TxRefusals::stopNotConfirmed()};
    }
    if (m_state == State::Transferring || m_fenced || m_keyingBlockDepth > 0) {
        return {TakeVerdict::Refuse, TxRefusals::changingHands()};
    }
    if (m_state != State::Held || !m_holder.has_value()) {
        return {TakeVerdict::AtOnce, {}};
    }
    if (m_holder->deviceId == requester) {
        return {TakeVerdict::AlreadyHeld, {}};
    }
    // Ruling 8.7: asked on the device first. The operator always sees the
    // red question before a carrier is cut: a holder shown unkeyed that is
    // on the air now is asked again.
    if (shownEpoch.has_value() && *shownEpoch == m_epoch
        && (!m_holder->keyed || shownKeyed.value_or(false))) {
        return {TakeVerdict::AtOnce, {}};
    }
    return {TakeVerdict::Ask, {}};
}

void TransmitHolder::transferTo(std::optional<Holder> next, const QString& reason,
                                std::function<void(bool)> done)
{
    if (m_state == State::Transferring || (next.has_value() && m_keyingBlockDepth > 0)) {
        if (done) {
            done(false);
        }
        return;
    }
    // Step 1: transferring; every key refused from here to the end.
    m_state = State::Transferring;
    m_next = std::move(next);
    m_transferDone = std::move(done);
    m_transferReason = reason;
    const quint64 generation = ++m_generation;
    emit changed();

    // Step 2: a keyed holder is unkeyed through the unkey gate first.
    const bool keyed = (m_holder.has_value() && m_holder->keyed) || moxOn();
    if (keyed && m_hooks.unkey) {
        m_hooks.unkey(reason, [this, generation](UnkeyOutcome) { afterUnkey(generation); });
        return;
    }
    afterUnkey(generation);
}

void TransmitHolder::runWithKeyingBlocked(const std::function<void()>& callback)
{
    QPointer<TransmitHolder> self(this);
    ++m_keyingBlockDepth;
    auto exit = qScopeGuard([self]() {
        if (self) --self->m_keyingBlockDepth;
    });
    callback();
}

void TransmitHolder::afterUnkey(quint64 generation)
{
    if (generation != m_generation || m_state != State::Transferring) {
        return;
    }
    // Keyed or not, MOX is read: no holder is assigned while it reads on.
    if (!moxOn()) {
        assign(generation);
        return;
    }
    m_waitingMoxOff = true;
    if (m_hooks.stopAllTx) {
        m_hooks.stopAllTx(m_transferReason);
    }
    if (m_hooks.schedule) {
        m_hooks.schedule(kMoxOffWaitMs, [this, generation]() {
            if (generation != m_generation || !m_waitingMoxOff) {
                return;
            }
            if (moxOn()) {
                failTransfer(generation);
            } else {
                m_waitingMoxOff = false;
                assign(generation);
            }
        });
    }
}

void TransmitHolder::assign(quint64 generation)
{
    if (generation != m_generation) {
        return;
    }
    m_waitingMoxOff = false;
    m_takeUnstarted = false;
    // Fix wave I3: a transfer supersedes a dropped holder's fence (its
    // callbacks see the old generation); with MOX read off and the holder
    // changed, the fence has nothing left to guard.
    m_fenced = false;
    m_fenceWaitingMoxOff = false;
    // Step 3: the new holder, unkeyed, or nobody; VOX disarmed; the epoch
    // advanced; published.
    const bool hadHolder = m_holder.has_value();
    if (m_next.has_value()) {
        Holder next = *m_next;
        next.keyed = false;
        next.away = false;
        next.sinceMs = now();
        m_holder = next;
        m_state = State::Held;
    } else {
        m_holder.reset();
        m_state = State::Unheld;
    }
    m_next.reset();
    if (hadHolder || m_holder.has_value()) {
        ++m_epoch;
    }
    disarmVox();
    std::function<void(bool)> done = std::move(m_transferDone);
    m_transferDone = {};
    qCInfo(lcDsp) << "Transmit now held by"
                  << (m_holder.has_value() ? m_holder->deviceId : QByteArray("nobody"));
    emit changed();
    if (done) {
        done(true);
    }
}

void TransmitHolder::failTransfer(quint64 generation)
{
    if (generation != m_generation) {
        return;
    }
    // Step 2's end with MOX still on: transmit unheld, every key refused
    // until MOX reads off (stopNotConfirmed, which onMoxReading clears).
    // Fix wave I3: a dropped holder's fence ends here too; the refusal
    // that stands is "did not confirm", never "changing hands".
    m_waitingMoxOff = false;
    m_fenced = false;
    m_fenceWaitingMoxOff = false;
    m_takeUnstarted = false;
    const bool hadHolder = m_holder.has_value();
    m_holder.reset();
    m_next.reset();
    m_state = State::Unheld;
    m_stopUnconfirmed = true;
    if (hadHolder) {
        ++m_epoch;
    }
    disarmVox();
    qCWarning(lcDsp) << "The radio did not confirm it stopped transmitting; transmit is unheld"
                        " and every key is refused until it does.";
    std::function<void(bool)> done = std::move(m_transferDone);
    m_transferDone = {};
    emit changed();
    if (done) {
        done(false);
    }
}

void TransmitHolder::release(const QByteArray& deviceId, const QString& reason)
{
    if (!m_holder.has_value() || m_holder->deviceId != deviceId
        || m_state == State::Transferring) {
        return;
    }
    transferTo(std::nullopt, reason);
}

void TransmitHolder::releaseUnstartedTake()
{
    if (!m_takeUnstarted || m_state != State::Held || !m_holder.has_value() || m_holder->keyed
        || m_holder->away || m_fenced || m_stopUnconfirmed || moxOn()) {
        return;
    }
    const QByteArray who = m_holder->deviceId;
    m_holder.reset();
    m_state = State::Unheld;
    m_takeUnstarted = false;
    ++m_epoch;
    qCInfo(lcDsp) << "Transmit released: the key that took it by" << who << "never started";
    emit changed();
}

void TransmitHolder::holderDropped(const QByteArray& deviceId, const QString& reason)
{
    if (m_state != State::Held || !m_holder.has_value() || m_holder->deviceId != deviceId) {
        return;
    }
    // Ruling 8.15: held for it, away, VOX disarmed; not a change of holder.
    m_holder->away = true;
    m_takeUnstarted = false;
    disarmVox();
    if (m_holder->keyed || moxOn()) {
        startFence(reason);
    }
    emit changed();
}

void TransmitHolder::startFence(const QString& reason)
{
    // Step 2's fence without a change of holder: keys refused until MOX
    // reads off.
    m_fenced = true;
    const quint64 generation = ++m_generation;
    const auto afterStop = [this, generation]() {
        if (generation != m_generation || !m_fenced) {
            return;
        }
        if (!moxOn()) {
            m_fenced = false;
            if (m_holder.has_value()) {
                m_holder->keyed = false;
            }
            emit changed();
            return;
        }
        m_fenceWaitingMoxOff = true;
        if (m_hooks.stopAllTx) {
            m_hooks.stopAllTx(m_transferReason);
        }
        if (m_hooks.schedule) {
            m_hooks.schedule(kMoxOffWaitMs, [this, generation]() {
                if (generation != m_generation || !m_fenceWaitingMoxOff) {
                    return;
                }
                m_fenceWaitingMoxOff = false;
                if (moxOn()) {
                    // Still on: keys stay refused (now "did not confirm")
                    // until MOX reads off; the holder is kept.
                    m_stopUnconfirmed = true;
                } else {
                    m_fenced = false;
                }
                emit changed();
            });
        }
    };
    m_transferReason = reason;
    if (m_hooks.unkey) {
        m_hooks.unkey(reason, [afterStop](UnkeyOutcome) { afterStop(); });
    } else {
        afterStop();
    }
}

void TransmitHolder::holderReturned(const QByteArray& deviceId)
{
    if (!m_holder.has_value() || m_holder->deviceId != deviceId || !m_holder->away) {
        return;
    }
    m_holder->away = false;
    emit changed();
}

void TransmitHolder::setKeyed(bool keyed)
{
    if (!m_holder.has_value() || m_holder->keyed == keyed) {
        return;
    }
    m_holder->keyed = keyed;
    m_holder->keyedSinceMs = keyed ? now() : 0;
    if (keyed) {
        m_takeUnstarted = false;
    }
    emit changed();
}

void TransmitHolder::onMoxReading(bool on)
{
    if (on) {
        return;
    }
    bool changedNow = false;
    if (m_holder.has_value() && m_holder->keyed) {
        m_holder->keyed = false;
        m_holder->keyedSinceMs = 0;
        changedNow = true;
    }
    if (m_waitingMoxOff && m_state == State::Transferring) {
        m_waitingMoxOff = false;
        assign(m_generation);
        return;
    }
    if (m_fenced && (m_fenceWaitingMoxOff || m_stopUnconfirmed)) {
        m_fenceWaitingMoxOff = false;
        m_fenced = false;
        m_stopUnconfirmed = false;
        changedNow = true;
    }
    if (m_stopUnconfirmed) {
        m_stopUnconfirmed = false;
        changedNow = true;
    }
    if (changedNow) {
        emit changed();
    }
}

} // namespace NereusSDR
