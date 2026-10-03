// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/StationTransmitTake.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 77 (R-IOS-02, R-IOS-03; the several-devices design,
// sections 8.4 and 8.6, rulings 8.1, 8.2, 8.6 and 8.7, settled detail S4):
// StationServer's part in one device taking transmit from another.
//
//   - tx.take {holderEpoch, shownKeyed} never keys (ruling 8.6). Unheld,
//     it takes at once; the holder's own take changes nothing; with
//     another device holding, the requester is refused "Waiting for you to
//     confirm." and asked (confirm.request takeTransmit, with the holder's
//     entry: red when it is on the air, without red when it is away).
//     Asked on the device first (ruling 8.7), it takes at once while the
//     epoch it showed still names the holder and the holder is not on the
//     air unless it was shown so.
//   - The take is ruling 8.2's transfer: a keyed holder is unkeyed through
//     the unkey-confirmed gate before the new holder is assigned, unkeyed.
//     The result (of tx.take, or of the proceed) arrives when it ends; the
//     old holder is told (notice transmitTaken, with Take it back), waiting
//     for it after its snapshot.complete when it is away; a holder taken
//     from on the air has its stop recorded as takenOver.
//   - Take it back (section 8.6): notice.takeBack of a transmitTaken notice
//     is tx.take {} with its usual confirmation.
//   - Fix wave (Task 77 review): a take ends the old holder's Tuner Genius
//     autotune first (I4); a taker whose session ended during its take is
//     held away or released at its end (I2); copies of a tx.take (the same
//     command id) get the first one's answer and never take twice (M6).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-26: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 77 (R-IOS-02, R-IOS-03),
//               with AI-assisted implementation via Anthropic Claude Code.
//   2026-09-26: Task 77 fix wave (I2, I4, M6) by J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-28: slice control and shared listening plan Task 2: the TX
//               marks and the holder's last slice follow
//               SliceAccessPolicy::mayTransmitOn, never a listener. J.J.
//               Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-28: slice control and shared listening plan Task 4 (ruling
//               Q8): a new holder's binding prefers its slices other than
//               one it took control of and has not chosen. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-29: slice control plan Task 10: requesters are read through
//               peerFor, so the station device is one. J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude Code.
//   2026-09-29: slice control plan Task 11: transmitPreferenceFor (the
//               binding's preference, shared with the keying gate) and
//               takenSliceKeyRefusal. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-30: take-over fix wave (I-2): othersSliceKeyRefusal, a key
//               never lands on the slice a device lost. Re-review (N-1,
//               N-2): nor, for a keyer that shares slices
//               (keyerSharesSlices), on any other device's slice; the flag
//               moves only once the key is admitted. J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude Code.
//   2026-09-30: TX rulings (item 4): a key in the TX-to-RX tail, or
//               while the flag is frozen, with a slice of the keyer's own,
//               is refused as radioOnAir, not noTransmitSlice. Ruling
//               8.11 on a hosting desktop: radioPttKeyRefusal, the radio's
//               own PTT keys the desktop's active slice. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-30: Fix wave LINK minor 1 (TX path): proceedTakeTransmit
//               checks sessionTransmitRefusal first, so a take-over from
//               a session that may not transmit is refused. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/session/StationServer.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <memory>

#include "core/LogCategories.h"
#include "core/MoxController.h"
#include "core/safety/StationTxGate.h"
#include "core/safety/TransmitHolder.h"
#include "core/safety/TxRefusal.h"
#include "core/session/ConfirmStep.h"
#include "core/session/ConnectedDevicesFacade.h"
#include "core/session/DeviceSessionRegistry.h"
#include "core/session/RemoteKeying.h"
#include "core/session/SessionTransport.h"
#include "core/session/SliceAccessPolicy.h"
#include "core/session/TransmitStateFacade.h"
#include "core/SliceOwnership.h"
#include "core/TxSliceArbiter.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

namespace NereusSDR {

namespace {

// The several-devices design, section 7.3 step 3.
constexpr const char* kWaitingReason = "Waiting for you to confirm.";

MirrorUpdate phaseNeedsConfirmation()
{
    return MirrorUpdate{0, QByteArrayLiteral("phase"), MirrorWireKind::Utf8,
                        QStringLiteral("needsConfirmation")};
}

SessionMessage refusalResult(const SessionMessage& invoke, const TxRefusal& refusal)
{
    return SessionMessages::commandResult(
        invoke.commandVerb, invoke.commandId, false, refusal.text, {},
        {{0, "refusalCode", MirrorWireKind::Utf8, QString::fromUtf8(refusal.code)},
         {0, "refusalFix", MirrorWireKind::Utf8, QString::fromUtf8(refusal.fix)}});
}

SessionMessage takenResult(const QByteArray& verb, quint32 commandId, quint64 holderEpoch)
{
    return SessionMessages::commandResult(
        verb, commandId, true, QString(), {},
        {{0, "holderEpoch", MirrorWireKind::Int64,
          QVariant(static_cast<qlonglong>(holderEpoch))}});
}

SessionMessage waitingResult(const SessionMessage& invoke)
{
    return SessionMessages::commandResult(invoke.commandVerb, invoke.commandId, false,
                                          QString::fromLatin1(kWaitingReason), {},
                                          {phaseNeedsConfirmation()});
}

QString takenReason(const QString& takerName)
{
    return QStringLiteral("%1 took transmit.").arg(takerName);
}

} // namespace

RadioModel* StationServer::radioModel() const
{
    return m_radioModel.data();
}

TxRefusal StationServer::sessionTransmitRefusal(SessionTransport* transport) const
{
    StationTxGate sessionOnly;
    sessionOnly.setRemoteTransmitAllowed(m_txGate.remoteTransmitAllowed());
    const TxDecision decision = sessionOnly.decide(peerInfoFor(transport));
    return decision.permitted ? TxRefusal{} : decision.refusal;
}

TransmitHolder::Words StationServer::stationTakerWords(TransmitHolder::Source source) const
{
    // Ruling 8.1: "Radio" after the radio's own PTT, on any Core; a hosting
    // desktop's own MOX or TUNE in its own words.
    if (source == TransmitHolder::Source::RadioPtt || m_stationWords.name.isEmpty()) {
        return {QStringLiteral("Radio"), QStringLiteral("Radio"), QStringLiteral("station")};
    }
    return {m_stationWords.name, m_stationWords.shortName, QStringLiteral("station")};
}

QJsonObject StationServer::holderEntryJson(const TransmitHolder::Holder& holder) const
{
    QJsonObject entry;
    const bool station = holder.deviceId == KeyerIdentity::kStationDeviceId;
    entry.insert(QStringLiteral("deviceId"),
                 station ? QString::fromLatin1(KeyerIdentity::kStationDeviceId) : QString());
    entry.insert(QStringLiteral("name"), holder.name);
    entry.insert(QStringLiteral("shortName"), holder.shortName);
    entry.insert(QStringLiteral("kind"), holder.kind);
    entry.insert(QStringLiteral("source"), holder.source == TransmitHolder::Source::RadioPtt
                                               ? QStringLiteral("radioPtt")
                                               : QStringLiteral("device"));
    entry.insert(QStringLiteral("state"), holder.away    ? QStringLiteral("away")
                                          : holder.keyed ? QStringLiteral("transmitting")
                                                         : QStringLiteral("listening"));
    entry.insert(QStringLiteral("keyed"), holder.keyed);
    const qint64 now = m_deviceSessions->now();
    entry.insert(QStringLiteral("connectedForSeconds"), 0);
    entry.insert(QStringLiteral("lastActivitySeconds"), 0);
    entry.insert(QStringLiteral("awayForSeconds"), 0);
    entry.insert(QStringLiteral("transmittingForSeconds"),
                 holder.keyed ? std::max<qint64>(0, (now - holder.keyedSinceMs) / 1000) : 0);
    if (!station) {
        // A device's entry as connectedDevices describes it (ruling 10.3's
        // durations, measured now).
        const std::optional<ConnectedDevicesFacade::DeviceWords> words =
            m_connectedDevices->describe(holder.deviceId);
        const QString wireId = words ? words->wireId : QString();
        entry.insert(QStringLiteral("deviceId"), wireId);
        const QJsonArray list =
            QJsonDocument::fromJson(m_connectedDevices->listJson().toUtf8()).array();
        for (const QJsonValue& value : list) {
            const QJsonObject o = value.toObject();
            if (o.value(QStringLiteral("deviceId")).toString() != wireId) {
                continue;
            }
            for (const char* key : {"connectedForSeconds", "lastActivitySeconds", "awayForSeconds",
                                    "transmittingForSeconds"}) {
                entry.insert(QLatin1String(key), o.value(QLatin1String(key)));
            }
            break;
        }
    }
    return entry;
}

void StationServer::askTakeTransmit(SessionTransport* transport, const SessionMessage& original,
                                    ConfirmStep::Held held, qint64 noticeId)
{
    const std::optional<TransmitHolder::Holder> holder = m_transmitHolder->holder();
    if (!holder) {
        return;
    }
    ConfirmStep::Question question;
    question.kind = QStringLiteral("takeTransmit");
    question.held = held;
    question.noticeId = noticeId;
    question.original = original;
    question.holderEpoch = m_transmitHolder->epoch();
    question.holderKeyed = holder->keyed;
    SessionPrompt prompt;
    // The holder is named in `holder`; nobody's slices are touched.
    prompt.affected = QJsonArray{};
    prompt.holder = holderEntryJson(*holder);
    sendQuestion(transport, question, prompt);
}

void StationServer::runTake(const QByteArray& taker, TransmitHolder::Source source,
                            std::function<void(bool assigned)> done)
{
    const std::optional<TransmitHolder::Holder> old = m_transmitHolder->holder();
    const bool station = taker == KeyerIdentity::kStationDeviceId;
    const QString takerName = station ? stationTakerWords(source).name : planDevice(taker).name;
    const bool oldOnAir = old && old->deviceId != taker
        && (old->keyed || (m_radioModel && m_radioModel->isTransmitting()));
    if (oldOnAir && m_transmitState != nullptr) {
        // The stop the take makes (txState stopReason takenOver).
        m_transmitState->recordStop(QByteArray(TransmitState::kStopTakenOver),
                                    TransmitState::takenOverText(takerName));
    }
    // Ruling 8.9: a PTT still held when transmit is taken from the station
    // device does not take it straight back; its next press does.
    if (old && old->deviceId == KeyerIdentity::kStationDeviceId && !station && m_radioModel
        && m_radioModel->moxController() != nullptr) {
        m_radioModel->moxController()->holdOffHeldMic();
    }
    // Fix wave I4: the old holder's Tuner Genius autotune ends before the
    // transfer, keyed or still waiting for the amplifier's standby, so it
    // never keys for a device that no longer holds transmit. The amplifier
    // goes back as it was only while nothing is keyed
    // (RadioModel::finishTgxlAutotuneCycle).
    if (old && old->deviceId != taker) {
        if (m_remoteKeying) {
            m_remoteKeying->endAutotuneFor(old->deviceId);
        } else if (m_radioModel) {
            m_radioModel->cancelTgxlAutotuneFor(old->deviceId);
        }
    }
    TransmitHolder::Holder next;
    next.deviceId = taker;
    next.source = source;
    qCInfo(lcDsp) << "Transmit being taken by" << taker
                  << "from" << (old ? old->deviceId : QByteArray("nobody"));
    const QPointer<StationServer> self(this);
    m_transmitHolder->transferTo(
        next, QStringLiteral("Transmit was taken by %1.").arg(takerName),
        [self, old, taker, source, takerName, station, done](bool assigned) {
            if (!self.isNull() && assigned && old && old->deviceId != taker
                && old->deviceId != KeyerIdentity::kStationDeviceId) {
                ConfirmStep::Notice notice;
                notice.device = old->deviceId;
                notice.prompt.kind = QStringLiteral("transmitTaken");
                notice.prompt.takeBack = true;
                notice.reason = takenReason(takerName);
                notice.taker = taker;
                if (station) {
                    const TransmitHolder::Words words = self->stationTakerWords(source);
                    notice.prompt.byDeviceId = QString::fromLatin1(KeyerIdentity::kStationDeviceId);
                    notice.prompt.byName = words.name;
                    notice.prompt.byShortName = words.shortName;
                    notice.prompt.byKind = words.kind;
                    notice.prompt.bySource = source == TransmitHolder::Source::RadioPtt
                                                 ? QStringLiteral("radioPtt")
                                                 : QStringLiteral("device");
                    self->tellDevice(notice, QByteArray());
                } else {
                    self->tellDevice(notice, taker);
                }
            }
            if (!self.isNull() && assigned && !station) {
                self->settleTakerWithoutSession(taker);
            }
            if (done) {
                done(assigned);
            }
        });
}

void StationServer::settleTakerWithoutSession(const QByteArray& taker)
{
    // Fix wave I2 (ruling 8.15, settled detail S4): dropPeer's release or
    // holderDropped did nothing while the take ran (the taker did not hold
    // transmit yet), so the end of the take does it now.
    for (auto it = m_peers.cbegin(); it != m_peers.cend(); ++it) {
        if (it->sessionDeviceId == taker) {
            return;
        }
    }
    if (!m_transmitHolder->isHeldBy(taker)) {
        return;
    }
    const std::optional<DeviceSessionRegistry::Entry> entry = m_deviceSessions->entry(taker);
    if (entry && entry->state == DeviceSessionRegistry::State::Away) {
        qCInfo(lcDsp) << "Transmit taken by" << taker << "which dropped during the take; held away";
        m_transmitHolder->holderDropped(taker, QStringLiteral("The device's link was lost."));
    } else if (!entry) {
        qCInfo(lcDsp) << "Transmit taken by" << taker << "which left during the take; released";
        m_transmitHolder->release(taker, QStringLiteral("The device left the Core."));
    }
}

void StationServer::takeTransmit(SessionTransport* transport, const SessionMessage& invoke,
                                 std::optional<quint64> shownEpoch, std::optional<bool> shownKeyed,
                                 std::function<void(const SessionMessage& result)> reply)
{
    const QByteArray requester = peerFor(transport).sessionDeviceId;
    if (transport == nullptr || requester.isEmpty() || !m_transmitHolder) {
        reply(refusalResult(invoke, TxRefusals::notReady()));
        return;
    }
    // Fix wave M6: a device sends each transmit command more than once as
    // the same command (the same verb and id), as it does tx.key. A copy
    // never asks again or takes a second time: while the first one's take
    // runs, that take's answer (one result for the id) answers it; after,
    // it gets the same answer again.
    QList<std::shared_ptr<TakeCopy>>& copies = m_takeCopies[peerFor(transport).sessionId];
    for (const std::shared_ptr<TakeCopy>& copy : copies) {
        if (copy->commandId != invoke.commandId) {
            continue;
        }
        if (copy->answered) {
            reply(copy->result);
        }
        return;
    }
    const auto first = std::make_shared<TakeCopy>();
    first->commandId = invoke.commandId;
    copies.append(first);
    while (copies.size() > kTakeCopiesKept) {
        copies.removeFirst();
    }
    reply = [first, reply = std::move(reply)](const SessionMessage& result) {
        first->answered = true;
        first->result = result;
        reply(result);
    };
    // A device that cannot be asked (no sessionHolderVersion 1) cannot take.
    if (!peerHasSessionHolderVersion(transport)) {
        reply(refusalResult(invoke, TxRefusals::appCannotTransmit()));
        return;
    }
    if (const TxRefusal refusal = sessionTransmitRefusal(transport); !refusal.isEmpty()) {
        reply(refusalResult(invoke, refusal));
        return;
    }
    const TransmitHolder::TakeAnswer answer =
        m_transmitHolder->askTake(requester, shownEpoch, shownKeyed);
    switch (answer.verdict) {
    case TransmitHolder::TakeVerdict::Refuse:
        reply(refusalResult(invoke, answer.refusal));
        return;
    case TransmitHolder::TakeVerdict::AlreadyHeld:
        reply(takenResult(invoke.commandVerb, invoke.commandId, m_transmitHolder->epoch()));
        return;
    case TransmitHolder::TakeVerdict::Ask:
        reply(waitingResult(invoke));
        askTakeTransmit(transport, invoke, ConfirmStep::Held::Command, 0);
        return;
    case TransmitHolder::TakeVerdict::AtOnce:
        break;
    }
    const QByteArray verb = invoke.commandVerb;
    const quint32 id = invoke.commandId;
    const QPointer<StationServer> self(this);
    runTake(requester, TransmitHolder::Source::Device,
            [self, reply, verb, id](bool assigned) {
                if (self.isNull()) {
                    return;
                }
                if (assigned) {
                    reply(takenResult(verb, id, self->m_transmitHolder->epoch()));
                } else {
                    SessionMessage invoke;
                    invoke.commandVerb = verb;
                    invoke.commandId = id;
                    reply(refusalResult(invoke, TxRefusals::stopNotConfirmed()));
                }
            });
}

TransmitHolder::TakeVerdict StationServer::takeTransmitForStation(std::optional<quint64> shownEpoch,
                                                                 std::optional<bool> shownKeyed,
                                                                 std::function<void(bool)> done)
{
    // Ruling 8.9a (D64): the hosting desktop's MOX or TUNE takes only
    // through tx.take's rules. Ask: the desktop shows its question from its
    // own TransmitHolder state and calls again with what it showed.
    const TransmitHolder::TakeAnswer answer = m_transmitHolder->askTake(
        QByteArray(KeyerIdentity::kStationDeviceId), shownEpoch, shownKeyed);
    if (answer.verdict == TransmitHolder::TakeVerdict::AtOnce) {
        runTake(QByteArray(KeyerIdentity::kStationDeviceId), TransmitHolder::Source::Device,
                std::move(done));
    }
    return answer.verdict;
}

SessionMessage StationServer::takeAnswering(SessionTransport* transport,
                                            const SessionMessage& invoke,
                                            std::function<void()> onTaken)
{
    struct Later {
        bool returned = false;
        std::optional<SessionMessage> result;
    };
    const auto later = std::make_shared<Later>();
    const QPointer<SessionTransport> to(transport);
    const quint64 session = peerFor(transport).sessionId;
    const QByteArray requester = peerFor(transport).sessionDeviceId;
    const QByteArray verb = invoke.commandVerb;
    const quint32 id = invoke.commandId;
    const QPointer<StationServer> self(this);
    runTake(requester, TransmitHolder::Source::Device,
            [self, later, to, session, verb, id, onTaken](bool assigned) {
                if (self.isNull()) {
                    return;
                }
                SessionMessage invoke;
                invoke.commandVerb = verb;
                invoke.commandId = id;
                const SessionMessage result =
                    assigned ? takenResult(verb, id, self->m_transmitHolder->epoch())
                             : refusalResult(invoke, TxRefusals::stopNotConfirmed());
                if (assigned && onTaken) {
                    onTaken();
                }
                if (!later->returned) {
                    later->result = result;
                    return;
                }
                // The answer that was owed: its route goes with it.
                self->m_resultRoutes.remove(ResultKey{session, verb, id});
                if (!to.isNull() && self->hasPeer(to.data())) {
                    self->sendToPeer(to.data(), result);
                }
            });
    later->returned = true;
    if (later->result) {
        return *later->result;
    }
    // The transfer is unkeying a holder on the air: the answer comes when
    // it ends; the one returned now is not sent.
    m_proceedAnsweredLater = ResultKey{session, verb, id};
    return waitingResult(invoke);
}

SessionMessage StationServer::proceedTakeTransmit(SessionTransport* transport,
                                                  const ConfirmStep::Question& question,
                                                  const SessionMessage& invoke)
{
    const QByteArray device = question.device;
    // LINK minor 1: the session's own transmit rights are checked again
    // here, as takeTransmit() and takeBackTransmit() check them: the
    // answer to the question may come after they changed.
    if (const TxRefusal refusal = sessionTransmitRefusal(transport); !refusal.isEmpty()) {
        return refusalResult(invoke, refusal);
    }
    // Ruling 8.7: a holder that was not keyed when asked and is keyed now,
    // or a different holder, is asked again; the operator always sees the
    // red question before a carrier is cut.
    const TransmitHolder::TakeAnswer answer =
        m_transmitHolder->askTake(device, question.holderEpoch, question.holderKeyed);
    const auto forgetTakeBack = [this, device, question]() {
        if (question.held == ConfirmStep::Held::TakeBack) {
            m_confirm->forgetTakeBack(device, question.noticeId);
        }
    };
    switch (answer.verdict) {
    case TransmitHolder::TakeVerdict::Refuse:
        return refusalResult(invoke, answer.refusal);
    case TransmitHolder::TakeVerdict::AlreadyHeld:
        forgetTakeBack();
        return takenResult(invoke.commandVerb, invoke.commandId, m_transmitHolder->epoch());
    case TransmitHolder::TakeVerdict::Ask:
        askTakeTransmit(transport, question.original, question.held, question.noticeId);
        return waitingResult(invoke);
    case TransmitHolder::TakeVerdict::AtOnce:
        break;
    }
    return takeAnswering(transport, invoke, forgetTakeBack);
}

SessionMessage StationServer::takeBackTransmit(SessionTransport* transport,
                                               const SessionMessage& invoke, qint64 noticeId)
{
    const QByteArray device = peerFor(transport).sessionDeviceId;
    if (const TxRefusal refusal = sessionTransmitRefusal(transport); !refusal.isEmpty()) {
        return refusalResult(invoke, refusal);
    }
    const TransmitHolder::TakeAnswer answer = m_transmitHolder->askTake(device);
    switch (answer.verdict) {
    case TransmitHolder::TakeVerdict::Refuse:
        return refusalResult(invoke, answer.refusal);
    case TransmitHolder::TakeVerdict::AlreadyHeld:
        m_confirm->forgetTakeBack(device, noticeId);
        return takenResult(invoke.commandVerb, invoke.commandId, m_transmitHolder->epoch());
    case TransmitHolder::TakeVerdict::Ask:
        askTakeTransmit(transport, invoke, ConfirmStep::Held::TakeBack, noticeId);
        return waitingResult(invoke);
    case TransmitHolder::TakeVerdict::AtOnce:
        break;
    }
    return takeAnswering(transport, invoke,
                         [this, device, noticeId]() { m_confirm->forgetTakeBack(device, noticeId); });
}

void StationServer::refreshTxMarks()
{
    if (m_radioModel.isNull() || m_radioModel->role() != RadioModel::Role::Local
        || !m_transmitHolder) {
        return;
    }
    const std::optional<TransmitHolder::Holder> holder = m_transmitHolder->holder();
    const QByteArray holderId = holder ? holder->deviceId : QByteArray();
    const SliceOwnership* ownership = m_radioModel->sliceOwnership();
    for (SliceModel* slice : m_radioModel->slices()) {
        if (slice == nullptr) {
            continue;
        }
        slice->setTxMarkAllowed(
            SliceAccessPolicy::mayTransmitOn(*ownership, holderId, slice->sliceIndex()));
    }
}

void StationServer::bindTransmitSliceForHolder()
{
    if (m_radioModel.isNull() || m_radioModel->role() != RadioModel::Role::Local
        || m_radioModel->txSliceArbiter() == nullptr || !m_transmitHolder
        || m_transmitHolder->state() != TransmitHolder::State::Held) {
        return;
    }
    const quint64 epoch = m_transmitHolder->epoch();
    if (epoch == m_txSliceBoundEpoch) {
        return;
    }
    m_txSliceBoundEpoch = epoch;
    const std::optional<TransmitHolder::Holder> holder = m_transmitHolder->holder();
    // Ruling 8.11: the radio's own PTT transmits where the flag is, on
    // another device's slice when the station owns none.
    if (!holder || holder->source == TransmitHolder::Source::RadioPtt) {
        return;
    }
    m_radioModel->txSliceArbiter()->bindForHolder(holder->deviceId,
                                                  transmitPreferenceFor(holder->deviceId));
}

int StationServer::transmitPreferenceFor(const QByteArray& device) const
{
    int preferred = m_chosenTxSlice.value(device, -1);
    if (m_radioModel.isNull()) {
        return preferred;
    }
    // Slice control plan Task 4 (ruling Q8): a slice the holder took
    // control of is its transmit slice only once it chooses it
    // (tx.setTxSlice). The binding goes to another of its slices instead:
    // its choice, else its active slice, else its first. With only taken
    // slices it keeps today's binding, so transmit never lands on a slice
    // that is not the holder's (Task 11 then refuses its key).
    const QSet<int> taken = m_takenNotChosenForTx.value(device);
    if (taken.isEmpty()) {
        return preferred;
    }
    const SliceOwnership* ownership = m_radioModel->sliceOwnership();
    const auto usable = [&](int id) {
        return id >= 0 && !taken.contains(id) && m_radioModel->sliceById(id) != nullptr
            && SliceAccessPolicy::mayTransmitOn(*ownership, device, id);
    };
    if (usable(preferred)) {
        return preferred;
    }
    if (const int active = ownership->activeFor(device); usable(active)) {
        return active;
    }
    for (int id : ownership->ownedBy(device)) {
        if (usable(id)) {
            return id;
        }
    }
    return preferred;
}

TxRefusal StationServer::takenSliceKeyRefusal(const QByteArray& device)
{
    if (device.isEmpty() || m_radioModel.isNull() || m_radioModel->txSliceArbiter() == nullptr
        || !m_transmitHolder) {
        return {};
    }
    const QSet<int> taken = m_takenNotChosenForTx.value(device);
    if (taken.isEmpty()) {
        return {};
    }
    const SliceOwnership* ownership = m_radioModel->sliceOwnership();
    TxSliceArbiter* arbiter = m_radioModel->txSliceArbiter();
    const auto mayTransmit = [&](int id) {
        return id >= 0 && m_radioModel->sliceById(id) != nullptr
            && SliceAccessPolicy::mayTransmitOn(*ownership, device, id);
    };
    const bool holds = m_transmitHolder->isHeldBy(device);
    // Where the key lands: the holder's bound slice; a new holder's
    // binding (bindTransmitSliceForHolder, as TxSliceArbiter::bindForHolder
    // picks), or where the flag is when that binds nothing.
    int landing = arbiter->txBoundSliceId();
    if (!holds) {
        const int preferred = transmitPreferenceFor(device);
        const int active = ownership->activeFor(device);
        if (mayTransmit(preferred)) {
            landing = preferred;
        } else if (mayTransmit(active)) {
            landing = active;
        }
    }
    if (!taken.contains(landing)) {
        return {};
    }
    int other = -1;
    for (int id : ownership->ownedBy(device)) {
        if (mayTransmit(id) && !taken.contains(id)) {
            other = id;
            break;
        }
    }
    if (other < 0) {
        return TxRefusals::chooseTransmitSlice();
    }
    // It has a slice of its own it may transmit on: an unkeyed holder's
    // flag moves there before the key (a new holder's binding goes there
    // by itself).
    MoxController* mox = m_radioModel->moxController();
    if (holds && (mox == nullptr || !mox->isMox())) {
        arbiter->bindForHolder(device, transmitPreferenceFor(device));
    }
    return {};
}

bool StationServer::keyerSharesSlices(const QByteArray& device) const
{
    // Take-over re-review (N-1): the keyers that share slices, the
    // hosting desktop once it takes its notices (Task 10) and a device
    // that declared sliceAccess. A legacy window and a headless station
    // keep today's keying.
    if (device == SliceOwnership::stationDevice()) {
        return liveTransportFor(device) != nullptr;
    }
    SessionTransport* transport = liveTransportFor(device);
    return transport != nullptr && peerHasSliceAccess(transport);
}

TxRefusal StationServer::othersSliceKeyRefusal(const QByteArray& device, int* moveTo)
{
    if (moveTo != nullptr) {
        *moveTo = -1;
    }
    if (device.isEmpty() || m_radioModel.isNull() || m_radioModel->txSliceArbiter() == nullptr
        || !m_transmitHolder) {
        return {};
    }
    const SliceOwnership* ownership = m_radioModel->sliceOwnership();
    TxSliceArbiter* arbiter = m_radioModel->txSliceArbiter();
    const auto mayTransmit = [&](int id) {
        return id >= 0 && m_radioModel->sliceById(id) != nullptr
            && SliceAccessPolicy::mayTransmitOn(*ownership, device, id);
    };
    // Where the key lands, as takenSliceKeyRefusal reads it: the holder's
    // bound slice; a new holder's binding, or the flag when that binds
    // nothing (TxSliceArbiter::bindForHolder leaves the flag where it is).
    int landing = arbiter->txBoundSliceId();
    if (!m_transmitHolder->isHeldBy(device)) {
        const int preferred = transmitPreferenceFor(device);
        const int active = ownership->activeFor(device);
        if (mayTransmit(preferred)) {
            landing = preferred;
        } else if (mayTransmit(active)) {
            landing = active;
        }
    }
    if (landing < 0 || m_radioModel->sliceById(landing) == nullptr) {
        return {};
    }
    // Ruling Q8: control of a slice grants no transmit. A slice nobody
    // owns, or one it controls, keeps today's keying.
    const QByteArray subject = ownership->mark(landing).subject();
    auto lost = m_lostTxSlice.find(device);
    const bool lostHere = lost != m_lostTxSlice.end() && lost->contains(landing);
    if (subject.isEmpty() || subject == device) {
        if (lostHere) {
            lost->remove(landing);
        }
        return {};
    }
    // Another device's slice. Take-over re-review (N-1): a keyer that
    // shares slices never keys there. Take-over fix wave (I-2): nor does
    // any keyer on the slice it lost, the flag's slice when control passed
    // from it (a keyer outside that scope, such as a legacy window).
    if (!keyerSharesSlices(device) && !lostHere) {
        return {};
    }
    // With a slice of its own it may transmit on, the unkeyed flag moves
    // there once the key is admitted (N-2); the caller moves it. A move
    // that could not happen now is refused. TX rulings (item 4): "unkeyed"
    // is the radio back in receive, as requestHandoff reads it, so a key
    // in the TX-to-RX tail or while the flag is frozen is told the radio
    // is on the air, not that it has no slice.
    int own = -1;
    for (int id : ownership->ownedBy(device)) {
        if (mayTransmit(id)) {
            own = id;
            break;
        }
    }
    if (own < 0) {
        return TxRefusals::noTransmitSlice();
    }
    MoxController* mox = m_radioModel->moxController();
    const bool inReceive = mox == nullptr || (!mox->isMox() && mox->state() == MoxState::Rx);
    if (!inReceive || arbiter->isFrozen()) {
        return TxRefusals::radioOnAir();
    }
    if (moveTo != nullptr) {
        *moveTo = own;
    }
    return {};
}

TxRefusal StationServer::radioPttKeyRefusal(int* moveTo)
{
    if (moveTo != nullptr) {
        *moveTo = -1;
    }
    const QByteArray station = SliceOwnership::stationDevice();
    // A Core with no hosting desktop keeps ruling 8.11 as it is: the
    // radio's own PTT transmits where the flag is.
    if (m_radioModel.isNull() || m_radioModel->txSliceArbiter() == nullptr
        || !keyerSharesSlices(station)) {
        return {};
    }
    const SliceOwnership* ownership = m_radioModel->sliceOwnership();
    TxSliceArbiter* arbiter = m_radioModel->txSliceArbiter();
    const int flag = arbiter->txBoundSliceId();
    if (flag < 0 || m_radioModel->sliceById(flag) == nullptr) {
        return {};
    }
    // TX rulings (JJ, 2026-09-30, ruling 8.11 for a hosting desktop): its
    // footswitch and mic PTT key the desktop's active slice, wherever it
    // is, even one another device controls. JJ (2026-09-30): the flag on
    // one of the desktop's own slices, a non-active one included (split
    // transmit), is its chosen transmit slice, keyed where it is; the flag
    // on a slice nobody controls stays too. Only a flag on another device's
    // slice moves.
    const QByteArray subject = ownership->mark(flag).subject();
    if (subject.isEmpty() || subject == station) {
        return {};
    }
    const int active = ownership->activeRxFor(station);
    if (active < 0 || m_radioModel->sliceById(active) == nullptr) {
        return TxRefusals::noTransmitSlice();
    }
    if (active == flag) {
        return {};
    }
    // The move happens only once the key is admitted, never while the
    // radio is on the air or the flag is frozen; a key that cannot move
    // it is refused rather than keying another device's slice.
    MoxController* mox = m_radioModel->moxController();
    const bool inReceive = mox == nullptr || (!mox->isMox() && mox->state() == MoxState::Rx);
    if (!inReceive || arbiter->isFrozen()) {
        return TxRefusals::radioOnAir();
    }
    if (moveTo != nullptr) {
        *moveTo = active;
    }
    return {};
}

void StationServer::onSliceClosedForHolder(int sliceId)
{
    if (m_radioModel.isNull() || m_radioModel->role() != RadioModel::Role::Local
        || !m_transmitHolder) {
        return;
    }
    const std::optional<TransmitHolder::Holder> holder = m_transmitHolder->holder();
    if (!holder || holder->deviceId == KeyerIdentity::kStationDeviceId
        || m_transmitHolder->state() != TransmitHolder::State::Held) {
        return;
    }
    const SliceOwnership* ownership = m_radioModel->sliceOwnership();
    if (!SliceAccessPolicy::mayTransmitOn(*ownership, holder->deviceId, sliceId)) {
        return;
    }
    // Ruling 8.12: with another of its slices left, the arbiter moved the
    // flag there (RadioModel::removeSlice). With none, transmit is released
    // through a transfer to nobody, which unkeys first.
    for (SliceModel* slice : m_radioModel->slices()) {
        if (slice != nullptr && slice->sliceIndex() != sliceId
            && SliceAccessPolicy::mayTransmitOn(*ownership, holder->deviceId,
                                                slice->sliceIndex())) {
            return;
        }
    }
    qCInfo(lcDsp) << "The holder's last slice closed; transmit is released";
    m_transmitHolder->release(holder->deviceId,
                              QStringLiteral("The last slice of the device holding transmit closed."));
}

} // namespace NereusSDR
