// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/StationReceivers.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 74 (R-IOS-02, R-IOS-30; the several-devices design,
// sections 6.1 to 6.4, 7.3, 7.4 and 10.7, rulings 6.2 to 6.10, design
// ruling 6.5a, rulings 7.4a and 10.2): StationServer's part in sharing the
// radio's receivers among up to four devices.
//
//   - The pin (ruling 6.3): a shared receiver's C-Tune pin is its
//     anchor's; another device's request is refused.
//   - The anchor moves its pan (ruling 6.4) or changes band (ruling 6.5):
//     when that would leave another device's slice outside the new window
//     the change is held and the Core asks (confirm.request panMove); on
//     proceed each such slice moves to another receiver or, with none
//     free, closes, and its device is told (sliceMoved, sliceClosed).
//     Design ruling 6.5a: when the anchor's own other slice would be
//     stranded, a band change is a plain retune instead.
//   - A device that does not anchor moves its pan (ruling 6.6) to a free
//     receiver of its own; nobody is asked.
//   - Taking (section 6.4, rulings 6.7 to 6.10): a request refused because
//     every receiver (or the slice cap) is in use is followed, for a device
//     with the feature, by a chooser (takeReceiver, takeSlice). On proceed
//     the Core checks again, closes every other device's slice on the
//     chosen receiver, tells each owner with Take it back (receiverTaken,
//     sliceTaken), and applies the held request on the freed receiver. An
//     older window's last slice taken ends its session (takenOver). Take
//     it back asks the same question the other way.
//   - The confirm step (7.3): nothing is applied before the answer; on
//     proceed the set is computed again and a set that names a device or
//     an effect the operator was not shown is asked again. The proceed's
//     answer carries the readback (ruling 7.4a).
//   - Notices (7.4) go to the device at once, or wait for an away device
//     and follow its snapshot.complete (after graceEnded when its 3
//     minutes ran out, then without Take it back).
//
// An older window (no sessionHolderVersion 1) is never asked (10.7): it
// gets the refusal, in words that name the devices involved.
//
// Transmit's holder (Task 34, joined at the merge of the trunk into the
// transmit lane): the on-air refusals of ruling 7.4 come through the
// shared-settings check (transmitForCheck), and the receiver of a holder's
// transmit slice is not takeable while it is on the air (ruling 6.8).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 74 (R-IOS-02, R-IOS-30),
//               with AI-assisted implementation via Anthropic Claude Code.
//   2026-09-26: Transmit group fix wave 2: every holder on the air counts,
//               the station device's own keys included (onAirHolder),
//               exempt by change not by holder; ruling 8.11's freeze on
//               every path (XIT, pan moves, a stored change at proceed); a
//               hosting desktop's key named after it. J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude Code.
//   2026-09-26: iPhone app plan Task 77 (R-IOS-02, R-IOS-03, R-IOS-13):
//               takeTransmit questions and Take it back for transmit. J.J.
//               Boyd (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-28: slice control and shared listening plan Task 1: a question
//               keeps each slice it names, offers or shows closing with its
//               incarnation, and a proceed whose slice id was reused by a
//               new slice acts on nothing. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-28: slice control and shared listening plan Task 2: a
//               slice's change checks are SliceAccessPolicy::mayChange
//               (changeRefusal), so a listener changes nothing. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-29: slice control plan Task 8: a C-Tune refusal names a
//               receiver's device that is not connected as such. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-29: slice control plan Task 9: a refused Add lists the slices
//               to listen to (usableSlices), a question names each slice's
//               listeners and asks again when one joined, and every
//               listener of a closed slice is told. J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude Code.
//   2026-09-29: slice control plan Task 10: the station device is a peer
//               (peerFor), so the hosting desktop's slice requests and
//               notices take the remote path. J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude Code.
//   2026-09-30: take-over parity: Take it back on controlTaken
//               (takeBackControl), offered to a peer at sliceAccessVersion
//               2 (sendNotice). J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-30: take-over fix wave (M-1): a peer below sliceAccess 2 is
//               sent controlTaken without the slice entry's incarnation and
//               controlRevision. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-30: desktop listening lane: Take it back of a slice released
//               or taken again since the notice answers "That can no
//               longer be taken back." J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
// =================================================================

#include "core/session/StationServer.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <utility>
#include <limits>

#include "core/AppSettings.h"
#include "core/DeviceLayoutStore.h"
#include "core/safety/TransmitHolder.h"
#include "core/safety/TxRefusal.h"
#include "core/SliceOwnership.h"
#include "core/SliceStreamAllocator.h"
#include "core/WdspEngine.h"
#include "core/session/ConfirmStep.h"
#include "core/session/ConnectedDevicesFacade.h"
#include "core/session/DeviceSessionRegistry.h"
#include "core/session/MirrorView.h"
#include "core/session/ObjectRegistry.h"
#include "core/session/ReceiverPlanner.h"
#include "core/session/SliceAccessController.h"
#include "core/session/SliceAccessPolicy.h"
#include "core/session/SessionCommandDispatcher.h"
#include "core/session/SessionTransport.h"
#include "models/Band.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

namespace NereusSDR {

namespace {
Q_LOGGING_CATEGORY(lcReceivers, "nereus.station.receivers")

// The several-devices design, section 7.3 step 3.
constexpr const char* kWaitingReason = "Waiting for you to confirm.";
constexpr const char* kNoQuestionReason = "That question is no longer open. Make the change again.";
constexpr const char* kNoChoiceReason = "That choice is not in the list. Make the change again.";
constexpr const char* kChangedReason = "What this change reaches has changed. Make the change again.";
constexpr const char* kNoTakeBackReason = "That can no longer be taken back.";
// Ruling 7.5 (iPhone app Task 75).
constexpr const char* kExpiredReason = "That question has expired. Make the change again.";
constexpr const char* kCentreRefusedReason =
    "C-Tune cannot centre there while other receivers share this spectrum.";
constexpr const char* kPanNeedsReceiverReason =
    "All the radio's receivers are in use, so this panadapter cannot move to a receiver of its own.";
// Ruling 10.2.
constexpr const char* kCapFullAtAdmissionReason =
    "All the radio's slices are in use. Try again when another device closes one.";
constexpr const char* kReceiversFullAtAdmissionReason =
    "All the radio's receivers are in use. Try again when another device frees one.";

QString shownVictimKey(int choice, int sliceId, const QByteArray& subject)
{
    return QStringLiteral("%1|%2|%3").arg(choice).arg(sliceId)
        .arg(QString::fromLatin1(subject.toHex()));
}

// Match the two Add handlers' first named argument (including their
// QVariant-to-string conversion). A missing required argument is refused by
// the dispatcher and must never turn into a destructive capacity question.
std::optional<QString> addPanIdFor(const SessionMessage& command)
{
    const QByteArray name = command.commandVerb == "addSliceOnPan"
        ? QByteArrayLiteral("panId") : QByteArrayLiteral("initialPanId");
    for (const MirrorUpdate& argument : command.arguments) {
        if (argument.name == name) {
            return argument.value.toString();
        }
    }
    return std::nullopt;
}

// Ruling 6.3.
QString pinReason(const QString& anchorName)
{
    return QStringLiteral("This panadapter shows %1's receiver. Its C-Tune setting is %1's.")
        .arg(anchorName);
}

// Slice control plan Task 8: the same, when that device is not connected.
QString awayPinReason(const QString& anchorName)
{
    return QStringLiteral("This panadapter shows the receiver of %1, which is not connected "
                          "now. Its C-Tune setting stays with %1 until it is back or its "
                          "three minutes are up.")
        .arg(anchorName);
}

// Section 7.3, the older window (D59).
QString olderWindowAskReason(const QString& names)
{
    return QStringLiteral("This change would affect %1. Update NereusSDR to confirm changes "
                          "that affect other devices.")
        .arg(names);
}

// Ruling 6.10.
QString takenOverReason(const QString& takerName)
{
    return QStringLiteral("%1 took the receiver this app was using. Update NereusSDR to share "
                          "the Core.")
        .arg(takerName);
}

// Section 6.4: the refusal names the devices that hold the receivers.
QString holdersSentence(const QString& names)
{
    return QStringLiteral("The radio's receivers are in use by %1.").arg(names);
}

MirrorUpdate phaseNeedsConfirmation()
{
    return MirrorUpdate{0, QByteArrayLiteral("phase"), MirrorWireKind::Utf8,
                        QStringLiteral("needsConfirmation")};
}

// Section 7.4's notices. `name` is the other device's numbered name, the
// operator's own words (ruling 4.3); `letter` and `letterWords` are slice
// letters ("B", "B and C").
QString sliceMovedReason(const QString& name, const QStringList& letters)
{
    const QString letterWords = ReceiverPlanner::joinWords(letters);
    return letters.size() == 1
               ? QStringLiteral("%1 moved their panadapter. Your slice %2 moved to another "
                                "receiver.")
                     .arg(name, letterWords)
               : QStringLiteral("%1 moved their panadapter. Your slices %2 moved to another "
                                "receiver.")
                     .arg(name, letterWords);
}

QString sliceClosedReason(const QString& name, const QStringList& letters)
{
    const QString letterWords = ReceiverPlanner::joinWords(letters);
    return letters.size() == 1
               ? QStringLiteral("%1 moved their panadapter. Your slice %2 closed: no receiver was "
                                "free.")
                     .arg(name, letterWords)
               : QStringLiteral("%1 moved their panadapter. Your slices %2 closed: no receiver "
                                "was free.")
                     .arg(name, letterWords);
}

QString receiverTakenReason(const QString& name, const QStringList& letters)
{
    const QString letterWords = ReceiverPlanner::joinWords(letters);
    return letters.size() == 1
               ? QStringLiteral("%1 took the receiver your slice %2 was on.").arg(name, letterWords)
               : QStringLiteral("%1 took the receiver your slices %2 were on.")
                     .arg(name, letterWords);
}

QString sliceTakenReason(const QString& name, const QStringList& letters)
{
    const QString letterWords = ReceiverPlanner::joinWords(letters);
    return letters.size() == 1 ? QStringLiteral("%1 took your slice %2.").arg(name, letterWords)
                               : QStringLiteral("%1 took your slices %2.").arg(name, letterWords);
}

// Slice control plan Task 9: to a device listening to a slice another
// device closed.
QString listenedClosedReason(const QString& why, const QString& name, const QStringList& letters)
{
    const QString letterWords = ReceiverPlanner::joinWords(letters);
    const bool one = letters.size() == 1;
    if (why == QLatin1String("receiverTaken")) {
        return one ? QStringLiteral("%1 took the receiver slice %2 was on. You were listening "
                                    "to it.")
                         .arg(name, letterWords)
                   : QStringLiteral("%1 took the receiver slices %2 were on. You were listening "
                                    "to them.")
                         .arg(name, letterWords);
    }
    if (why == QLatin1String("sliceTaken")) {
        return one ? QStringLiteral("%1 took slice %2, which you were listening to.")
                         .arg(name, letterWords)
                   : QStringLiteral("%1 took slices %2, which you were listening to.")
                         .arg(name, letterWords);
    }
    return one ? QStringLiteral("%1 moved their panadapter. Slice %2, which you were listening "
                                "to, closed: no receiver was free.")
                     .arg(name, letterWords)
               : QStringLiteral("%1 moved their panadapter. Slices %2, which you were listening "
                                "to, closed: no receiver was free.")
                     .arg(name, letterWords);
}

// "Receiver 1" for stream 0 (the several-devices design, 5.4 and 7.3).
// Slice control plan Task 1: whether `refs` recorded `sliceId` with an
// incarnation it no longer has (closed, or its id reused by a new slice).
// A slice `refs` does not record has not changed by this test.
bool incarnationChanged(const SliceOwnership* ownership,
                        const QList<SliceOwnership::SliceRef>& refs, int sliceId)
{
    for (const SliceOwnership::SliceRef& ref : refs) {
        if (ref.sliceId == sliceId) {
            return !ownership->matches(ref);
        }
    }
    return false;
}

// Slice control fix wave: whether the question recorded `sliceId` with a
// control revision it no longer has. Control that passed to another device
// and back leaves the same controller but a listener nobody was shown.
bool revisionChanged(const SliceOwnership* ownership, const ConfirmStep::Question& question,
                     int sliceId)
{
    const auto it = question.askedRevisions.constFind(sliceId);
    return it != question.askedRevisions.constEnd()
        && ownership->controlRevision(sliceId) != it.value();
}

void recordRevisions(ConfirmStep::Question* question, const SliceOwnership* ownership)
{
    question->askedRevisions.clear();
    for (const QList<SliceOwnership::SliceRef>* refs :
         {&question->namedRefs, &question->choiceRefs, &question->shownRefs}) {
        for (const SliceOwnership::SliceRef& ref : *refs) {
            if (ref.sliceId >= 0 && ownership->isLive(ref.sliceId)) {
                question->askedRevisions.insert(ref.sliceId,
                                                ownership->controlRevision(ref.sliceId));
            }
        }
    }
}

// Slice control plan Task 9: the listeners of every slice recordRevisions
// records, for a device that shares slices.
void recordListeners(ConfirmStep::Question* question, const SliceOwnership* ownership)
{
    question->askedListeners.clear();
    question->listenersShown = true;
    for (const QList<SliceOwnership::SliceRef>* refs :
         {&question->namedRefs, &question->choiceRefs, &question->shownRefs}) {
        for (const SliceOwnership::SliceRef& ref : *refs) {
            if (ref.sliceId >= 0 && ownership->isLive(ref.sliceId)) {
                question->askedListeners.insert(ref.sliceId, ownership->listenersOf(ref.sliceId));
            }
        }
    }
}

// Slice control plan Task 9: whether `sliceId` has a listener now that the
// question did not show. A listener who left is no reason to ask again; a
// slice the question did not record is left to the other checks.
bool listenersGrew(const SliceOwnership* ownership, const ConfirmStep::Question& question,
                   int sliceId)
{
    if (!question.listenersShown) {
        return false;
    }
    const auto it = question.askedListeners.constFind(sliceId);
    if (it == question.askedListeners.constEnd()) {
        return false;
    }
    for (const QByteArray& device : ownership->listenersOf(sliceId)) {
        if (!it.value().contains(device)) {
            return true;
        }
    }
    return false;
}

void appendRefOnce(QList<SliceOwnership::SliceRef>* refs, const SliceOwnership* ownership,
                   int sliceId)
{
    for (const SliceOwnership::SliceRef& ref : std::as_const(*refs)) {
        if (ref.sliceId == sliceId) {
            return;
        }
    }
    refs->append(ownership->refOf(sliceId));
}

QString receiverLabel(int stream)
{
    const int number = stream + 1;
    return QStringLiteral("Receiver %1").arg(number);
}

bool readInt(const QList<MirrorUpdate>& args, const char* name, int* out)
{
    for (const MirrorUpdate& a : args) {
        if (a.name == name) {
            bool ok = false;
            const qlonglong v = a.value.toLongLong(&ok);
            if (!ok || a.kind != MirrorWireKind::Int64 || v < std::numeric_limits<int>::min()
                || v > std::numeric_limits<int>::max()) {
                return false;
            }
            *out = static_cast<int>(v);
            return true;
        }
    }
    return false;
}

bool readDouble(const QList<MirrorUpdate>& args, const char* name, double* out)
{
    for (const MirrorUpdate& a : args) {
        if (a.name == name) {
            bool ok = false;
            const double v = a.value.toDouble(&ok);
            if (!ok || !std::isfinite(v)) {
                return false;
            }
            *out = v;
            return true;
        }
    }
    return false;
}

QJsonArray savedSlicesJson(const QList<SavedSlice>& saved)
{
    QJsonArray out;
    for (const SavedSlice& s : saved) {
        out.append(QJsonObject{
            {QStringLiteral("sliceId"), s.id},
            {QStringLiteral("letter"), ReceiverPlanner::letterOf(s.id)},
            {QStringLiteral("frequencyHz"), s.frequencyHz},
            {QStringLiteral("mode"), static_cast<int>(s.dspMode)},
            {QStringLiteral("band"), static_cast<int>(bandFromFrequency(s.frequencyHz))},
        });
    }
    return out;
}

QStringList lettersOf(const QList<SavedSlice>& saved)
{
    QStringList letters;
    for (const SavedSlice& s : saved) {
        letters.append(ReceiverPlanner::letterOf(s.id));
    }
    return letters;
}

} // namespace

QString StationServer::olderWindowReason(const QString& names)
{
    return olderWindowAskReason(names);
}

// ── Who is who ───────────────────────────────────────────────────────────

ReceiverPlanner::DeviceInfo StationServer::planDevice(const QByteArray& deviceId) const
{
    ReceiverPlanner::DeviceInfo info;
    if (deviceId.isEmpty()) {
        return info;
    }
    const auto words = m_connectedDevices->describe(deviceId);
    if (!words) {
        return info;
    }
    info.known = true;
    info.wireId = words->wireId;
    info.name = words->name;
    info.shortName = words->shortName;
    info.kind = words->kind;
    if (const auto entry = m_deviceSessions->entry(deviceId)) {
        info.state = entry->state == DeviceSessionRegistry::State::Away ? QStringLiteral("away")
                                                                        : QStringLiteral("listening");
        // Task 34's holder: a device on the air is transmitting.
        if (info.state != QLatin1String("away") && m_transmitHolder) {
            const std::optional<TransmitHolder::Holder> holder = m_transmitHolder->holder();
            if (holder && holder->keyed && holder->deviceId == deviceId) {
                info.state = QStringLiteral("transmitting");
            }
        }
        info.lastActivitySeconds =
            std::max<qint64>(0, (m_deviceSessions->now() - entry->lastActivityMs) / 1000);
    } else {
        info.state = QStringLiteral("away");
    }
    return info;
}

ReceiverPlanner StationServer::receiverPlanner() const
{
    ReceiverPlanner planner(*m_radioModel,
                            [this](const QByteArray& id) { return planDevice(id); });
    // Ruling 6.8 (D64), Task 34's holder: the receiver of a holder's
    // transmit slice is not takeable while it is on the air.
    // Fix wave 2: the same holder on the air as the refusals and the
    // freeze (onAirHolder).
    if (const std::optional<TransmitHolder::Holder> holder = onAirHolder()) {
        if (const SliceModel* txSlice = m_radioModel->txBoundSlice()) {
            ReceiverPlanner::OnAirTransmit onAir;
            onAir.sliceId = txSlice->sliceIndex();
            onAir.holder = holder->deviceId;
            onAir.why = onAirWords(*holder).text;
            planner.setOnAirTransmit(onAir);
        }
    }
    return planner;
}

ReceiverPlanner StationServer::questionPlanner(SessionTransport* transport) const
{
    ReceiverPlanner planner = receiverPlanner();
    planner.setShowListeners(peerHasSliceAccess(transport));
    return planner;
}

SessionTransport* StationServer::liveTransportFor(const QByteArray& deviceId) const
{
    // Slice control plan Task 10: the hosting desktop, once it takes its
    // notices, is here as the station device.
    if (deviceId == SliceOwnership::stationDevice() && m_stationNotice) {
        return m_stationTransport.get();
    }
    for (auto it = m_peers.cbegin(); it != m_peers.cend(); ++it) {
        if (it->sessionDeviceId == deviceId && it->snapshotComplete && !it->view.isNull()) {
            return it.key();
        }
    }
    return nullptr;
}

QString StationServer::usableSlicesJson() const
{
    const SliceOwnership* ownership = m_radioModel->sliceOwnership();
    QList<int> ids = ownership->liveSlices();
    std::sort(ids.begin(), ids.end());
    QJsonArray usable;
    for (const int id : std::as_const(ids)) {
        usable.append(QJsonObject{
            {QStringLiteral("sliceId"), id},
            {QStringLiteral("incarnation"), static_cast<qint64>(ownership->incarnation(id))},
            {QStringLiteral("letter"), ReceiverPlanner::letterOf(id)},
            {QStringLiteral("controllerDeviceId"), planDevice(ownership->mark(id).owner).wireId},
        });
    }
    return QString::fromUtf8(QJsonDocument(usable).toJson(QJsonDocument::Compact));
}

QString StationServer::withHolderNames(const QString& reason, const QByteArray& requester) const
{
    const QStringList names = receiverPlanner().namesHoldingReceivers(requester);
    if (names.isEmpty()) {
        return reason;
    }
    // A refusal that ends in its own sentence gets the names as one more.
    QString base = reason.trimmed();
    if (!base.isEmpty() && !base.endsWith(QLatin1Char('.'))) {
        base += QLatin1Char('.');
    }
    return base + QLatin1Char(' ') + holdersSentence(ReceiverPlanner::joinWords(names));
}

void StationServer::answerHere(SessionTransport* transport, const SessionMessage& result)
{
    const Peer* peer = peerPtr(transport);
    const quint64 sessionId = peer != nullptr ? peer->sessionId : 0;
    if (!hasReplySession(transport, sessionId)) { return; }
    const QPointer<SessionTransport> to(transport);
    const ResultKey key{sessionId, result.commandVerb, result.commandId};
    for (InvokeFrame* frame = m_invokeFrame; frame != nullptr; frame = frame->parent) {
        if (frame->transport == to && frame->key == key && !frame->terminalResultSent) {
            frame->resultSent = true;
            frame->terminalResultSent = isLastResult(result);
            break;
        }
    }
    send(to, result);
}

// ── Commands: the pin, a C-Tune move, adding a slice or a pan ────────────

bool StationServer::handleReceiverCommand(SessionTransport* transport, const SessionMessage& message)
{
    if (m_radioModel.isNull() || m_radioModel->role() != RadioModel::Role::Local
        || m_radioModel->streamAllocator().streamCount() <= 0) {
        return false;
    }
    const QByteArray requester = peerFor(transport).sessionDeviceId;
    if (requester.isEmpty()) {
        return false;
    }
    const QByteArray& verb = message.commandVerb;
    if (verb == "requestStreamCtunPinned") {
        int sliceId = -1;
        if (!readInt(message.arguments, "sliceId", &sliceId)) {
            return false;
        }
        const SliceModel* slice = m_radioModel->sliceById(sliceId);
        if (slice == nullptr) {
            return false;
        }
        QString refusal = changeRefusal(requester, sliceId);
        if (refusal.isEmpty()) {
            // Ruling 6.3: the pin holds the whole window, so it is the
            // anchor's.
            const QByteArray anchor =
                m_radioModel->sliceOwnership()->anchorOf(slice->streamIndex());
            if (!anchor.isEmpty() && anchor != requester) {
                // Slice control plan Task 8 (JJ's bench): a device that is
                // not connected is named as such, never by a bare name the
                // operator reads as this computer's own.
                const bool notConnected = anchor != SliceOwnership::stationDevice()
                    && liveTransportFor(anchor) == nullptr;
                if (notConnected) {
                    refusal = awayPinReason(planDevice(anchor).name);
                } else {
                    refusal = pinReason(planDevice(anchor).name);
                }
            }
        }
        if (refusal.isEmpty()) {
            return false;
        }
        answerHere(transport, SessionMessages::commandResult(verb, message.commandId, false,
                                                             refusal, {}));
        return true;
    }
    if (verb == "requestStreamCentre") {
        return handleCentreMove(transport, message, requester);
    }
    if (verb == "addSlice" || verb == "addSliceOnPan") {
        return handleAddWithTake(transport, message, requester);
    }
    return false;
}

StationServer::PanMoveCheck StationServer::checkPanMove(const QByteArray& requester,
                                                        const SessionMessage& original) const
{
    PanMoveCheck check;
    const bool write = original.kind == SessionMessageKind::PropertyWrite;
    int sliceId = -1;
    double centreHz = 0.0;
    if (write) {
        bool ok = false;
        sliceId = original.objectKey.startsWith("slice:") ? original.objectKey.mid(6).toInt(&ok) : -1;
        if (!ok || !readDouble(original.updates, "frequency", &centreHz)) {
            return check;
        }
    } else if (!readInt(original.arguments, "sliceId", &sliceId)
               || !readDouble(original.arguments, "centreHz", &centreHz)) {
        return check;
    }
    const SliceModel* slice = m_radioModel->sliceById(sliceId);
    const SliceOwnership* ownership = m_radioModel->sliceOwnership();
    if (slice == nullptr || !SliceAccessPolicy::mayChange(*ownership, requester, sliceId)) {
        return check;
    }
    const int stream = slice->streamIndex();
    const SliceStreamAllocator& live = m_radioModel->streamAllocator();
    if (stream < 0 || !live.isStreamActive(stream)) {
        return check;
    }
    const QByteArray anchor = ownership->anchorOf(stream);
    const QVector<int> members = m_radioModel->slicesOnStream(stream);
    if (write) {
        // Ruling 6.5: a write to one of the anchor's slices that leaves its
        // receiver's window while a slice not its own shares it.
        const double half = live.streamSampleRateHz(stream) / 2.0;
        const double offset = centreHz - live.streamCentreHz(stream);
        const bool inWindow = offset > -half && offset < half;
        bool othersThere = false;
        for (int id : members) {
            othersThere = othersThere || ownership->mark(id).subject() != requester;
        }
        if (inWindow || members.size() <= 1 || anchor != requester || !othersThere) {
            return check;
        }
    } else if (!anchor.isEmpty() && anchor != requester) {
        return check;  // ruling 6.6, not a pan move
    }
    check.stream = stream;
    check.centreHz = centreHz;
    check.exemptSliceId = write ? sliceId : -1;
    check.plan = receiverPlanner().planWindowMove(stream, centreHz, requester, check.exemptSliceId);
    if (!check.plan.valid || !check.plan.ownOutside.isEmpty()) {
        // Today's C-Tune refusal; design ruling 6.5a for a band change.
        return check;
    }
    // Fix wave 2, Important 3 (rulings 7.4 and 8.11): a pan move that
    // would move or close the transmit slice of a holder on the air waits,
    // whoever owns that slice.
    if (const std::optional<TransmitHolder::Holder> holder = onAirHolder();
        holder && holder->deviceId != requester) {
        const SliceModel* txSlice = m_radioModel->txBoundSlice();
        for (const ReceiverPlanner::Disturbed& d : check.plan.disturbed) {
            if (txSlice != nullptr && d.sliceId == txSlice->sliceIndex()) {
                check.kind = PanMoveCheck::Kind::OnAir;
                check.onAir = onAirWords(*holder);
                return check;
            }
        }
    }
    for (const ReceiverPlanner::Disturbed& d : check.plan.disturbed) {
        if (!d.device.isEmpty()) {
            check.named.append(d);
        }
    }
    if (!check.named.isEmpty()) {
        check.kind = PanMoveCheck::Kind::Ask;
    } else if (!check.plan.disturbed.isEmpty() || write) {
        check.kind = PanMoveCheck::Kind::Apply;
    }
    return check;
}

bool StationServer::handleCentreMove(SessionTransport* transport, const SessionMessage& message,
                                     const QByteArray& requester)
{
    int sliceId = -1;
    double centreHz = 0.0;
    if (!readInt(message.arguments, "sliceId", &sliceId)
        || !readDouble(message.arguments, "centreHz", &centreHz)) {
        return false;
    }
    const SliceModel* slice = m_radioModel->sliceById(sliceId);
    if (slice == nullptr) {
        return false;
    }
    const QString refusal = changeRefusal(requester, sliceId);
    if (!refusal.isEmpty()) {
        answerHere(transport, SessionMessages::commandResult(message.commandVerb,
                                                             message.commandId, false, refusal, {}));
        return true;
    }
    const int stream = slice->streamIndex();
    const SliceStreamAllocator& live = m_radioModel->streamAllocator();
    if (stream < 0 || !live.isStreamActive(stream)) {
        return false;
    }
    const QByteArray anchor = m_radioModel->sliceOwnership()->anchorOf(stream);
    if (anchor.isEmpty() || anchor == requester) {
        // Ruling 6.4: the anchor's move.
        const PanMoveCheck check = checkPanMove(requester, message);
        if (check.kind == PanMoveCheck::Kind::OnAir) {
            answerHere(transport, SessionMessages::commandResult(
                message.commandVerb, message.commandId, false, check.onAir.text, {},
                {{0, "refusalCode", MirrorWireKind::Utf8, QString::fromUtf8(check.onAir.code)},
                 {0, "refusalFix", MirrorWireKind::Utf8, QString::fromUtf8(check.onAir.fix)}}));
            return true;
        }
        if (check.kind == PanMoveCheck::Kind::Ask) {
            if (!peerHasSessionHolderVersion(transport)) {
                answerHere(transport, SessionMessages::commandResult(
                    message.commandVerb, message.commandId, false,
                    olderWindowAskReason(namesOf(check.named)), {}));
                return true;
            }
            askPanMove(transport, message, check, std::nullopt);
            return true;
        }
        if (check.kind == PanMoveCheck::Kind::Apply) {
            // Only slices nobody owns stand in the way: they move or close,
            // and today's verb then finds the window already there.
            applyPanMove(check, requester);
        }
        return false;
    }

    // Ruling 6.6: a device that does not anchor takes its pan, with its
    // slices, to a free receiver centred where it asked.
    QList<int> own;
    for (int id : m_radioModel->slicesOnStream(stream)) {
        if (m_radioModel->sliceOwnership()->mark(id).subject() == requester) {
            own.append(id);
        }
    }
    // Fix wave 2, Important 3 (ruling 8.11): the requester's own slices
    // move with its pan, so a frozen transmit slice among them keeps the
    // pan where it is until the station device's key ends.
    for (int id : own) {
        const TxRefusal frozen = stationFreezeRefusal(id);
        if (!frozen.isEmpty()) {
            answerHere(transport, SessionMessages::commandResult(
                message.commandVerb, message.commandId, false, frozen.text, {},
                {{0, "refusalCode", MirrorWireKind::Utf8, QString::fromUtf8(frozen.code)},
                 {0, "refusalFix", MirrorWireKind::Utf8, QString::fromUtf8(frozen.fix)}}));
            return true;
        }
    }
    const double half = live.streamSampleRateHz(stream) / 2.0;
    for (int id : own) {
        const double offset = m_radioModel->sliceById(id)->frequency() - centreHz;
        if (!(offset > -half && offset < half)) {
            answerHere(transport, SessionMessages::commandResult(
                message.commandVerb, message.commandId, false,
                QString::fromLatin1(kCentreRefusedReason), {}));
            return true;
        }
    }
    int free = -1;
    for (int s = 0; s < live.streamCount() && free < 0; ++s) {
        if (!live.isStreamActive(s)) {
            free = s;
        }
    }
    if (free >= 0) {
        QList<QByteArray> affected;
        const bool moved = m_radioModel->moveSlicesToStream(own, free, centreHz);
        for (int id : own) {
            affected.append(ObjectRegistry::keyForSlice(id));
        }
        answerHere(transport, SessionMessages::commandResult(
            message.commandVerb, message.commandId, moved,
            moved ? QString() : QString::fromLatin1(kCentreRefusedReason), moved ? affected
                                                                                : QList<QByteArray>{}));
        return true;
    }
    answerHere(transport, SessionMessages::commandResult(
        message.commandVerb, message.commandId, false,
        withHolderNames(QString::fromLatin1(kPanNeedsReceiverReason), requester), {}));
    if (peerHasSessionHolderVersion(transport)
        && receiverPlanner().anotherDeviceHoldsAReceiver(requester)) {
        ReceiverPlanner::TakeRequest request;
        request.need = ReceiverPlanner::Need::PanMove;
        request.requester = requester;
        request.centreHz = centreHz;
        request.moving = own;
        askTake(transport, message, request);
    }
    return true;
}

bool StationServer::handleAddWithTake(SessionTransport* transport, const SessionMessage& message,
                                      const QByteArray& requester)
{
    const bool capFull = m_radioModel->slices().size() >= m_radioModel->sliceCapForDevices();
    const SliceStreamAllocator& live = m_radioModel->streamAllocator();
    SessionMessage answer;
    bool answered = false;
    const auto previous = m_resultHook;
    m_resultHook = [&](SessionMessage& result) {
        if (result.commandVerb == message.commandVerb && result.commandId == message.commandId) {
            const bool receiversFull = live.activeStreamCount() >= live.streamCount();
            if (!result.accepted && !capFull && receiversFull) {
                // Section 6.4: the refusal names the devices holding them.
                result.reason = withHolderNames(result.reason, requester);
            }
            if (!result.accepted && (capFull || receiversFull) && peerHasSliceAccess(transport)) {
                // Slice control plan Task 9: the live slices it could
                // listen to instead (slice.listen), each with its
                // incarnation and controller.
                result.updates.append(MirrorUpdate{0, QByteArrayLiteral("usableSlices"),
                                                   MirrorWireKind::Utf8, usableSlicesJson()});
            }
            answer = result;
            answered = true;
        }
        return previous ? previous(result) : true;
    };
    m_dispatcher->dispatch(message);
    m_resultHook = previous;
    if (!answered || answer.accepted || !peerHasSessionHolderVersion(transport)) {
        return true;
    }
    const std::optional<QString> panId = addPanIdFor(message);
    if (!panId) {
        return true;  // The dispatcher already answered the malformed Add.
    }
    const ReceiverPlanner planner = receiverPlanner();
    const bool newPan = !panId->isEmpty() && !m_radioModel->panHasSlicesFor(*panId, requester);
    ReceiverPlanner::TakeRequest request;
    request.need = newPan ? ReceiverPlanner::Need::AddPan : ReceiverPlanner::Need::AddSlice;
    request.requester = requester;
    if (capFull) {
        // When the requested new pan also needs a DDC, taking one slice
        // would free a slot but could leave both receivers occupied.
        if (!planner.planAddAfterClosing(requester, *panId, {}).receiverFits
            && planner.anotherDeviceHoldsAReceiver(requester)) {
            askTake(transport, message, request);
            return true;
        }
        const QList<ReceiverPlanner::Choice> choices = planner.sliceChoices(requester);
        if (!choices.isEmpty()) {
            askTakeSlice(transport, message, choices);
        }
        return true;
    }
    if (live.activeStreamCount() >= live.streamCount()
        && planner.anotherDeviceHoldsAReceiver(requester)) {
        askTake(transport, message, request);
    }
    return true;
}

// ── A slice retune leaving a shared receiver (ruling 6.5) ────────────────

bool StationServer::handleSliceRetune(SessionTransport* transport, const SessionMessage& message)
{
    if (m_radioModel.isNull() || m_radioModel->role() != RadioModel::Role::Local
        || !message.objectKey.startsWith("slice:")
        || m_radioModel->streamAllocator().streamCount() <= 0) {
        return false;
    }
    const QByteArray requester = peerFor(transport).sessionDeviceId;
    double frequencyHz = 0.0;
    if (requester.isEmpty() || !readDouble(message.updates, "frequency", &frequencyHz)) {
        return false;
    }
    bool ok = false;
    const int sliceId = message.objectKey.mid(6).toInt(&ok);
    const SliceModel* slice = ok ? m_radioModel->sliceById(sliceId) : nullptr;
    if (slice == nullptr
        || !SliceAccessPolicy::mayChange(*m_radioModel->sliceOwnership(), requester, sliceId)) {
        return false;
    }
    const PanMoveCheck check = checkPanMove(requester, message);
    if (check.kind == PanMoveCheck::Kind::OnAir) {
        answerWrite(transport, message, check.onAir.text);
        return true;
    }
    if (check.kind == PanMoveCheck::Kind::Ask) {
        if (!peerHasSessionHolderVersion(transport)) {
            answerWrite(transport, message, olderWindowAskReason(namesOf(check.named)));
            return true;
        }
        QJsonObject change{
            {QStringLiteral("label"),
             receiverLabel(check.stream)},
            {QStringLiteral("from"), ReceiverPlanner::bandWords(slice->band())},
            {QStringLiteral("to"), ReceiverPlanner::bandWords(bandFromFrequency(frequencyHz))},
        };
        askPanMove(transport, message, check, change);
        return true;
    }
    if (check.kind == PanMoveCheck::Kind::Apply) {
        // Nobody else would be disturbed: the receiver follows the anchor.
        applyPanMove(check, requester);
        applyPropertyWrite(transport, message, true, {});
        return true;
    }

    // Today's retune. Refused when every receiver is in use: the refusal
    // names who holds them and, for a device with the feature, the chooser
    // follows (section 6.4; design ruling 6.5a's case among them).
    const int stream = slice->streamIndex();
    const SliceStreamAllocator& live = m_radioModel->streamAllocator();
    if (stream < 0 || !live.isStreamActive(stream)) {
        return false;
    }
    SliceStreamAllocator copy = live;
    const bool sole = m_radioModel->slicesOnStream(stream).size() <= 1;
    const auto placement =
        copy.retuneSlice(stream, sole, slice->streamCtunPinned(), frequencyHz);
    if (placement.outcome != SliceStreamAllocator::Outcome::Rejected) {
        return false;
    }
    applyPropertyWrite(transport, message, true,
                       [this, &requester](QList<SessionPropertyResult>& results) {
                           for (SessionPropertyResult& r : results) {
                               if (!r.accepted && r.property == "frequency") {
                                   r.reason = withHolderNames(r.reason, requester);
                               }
                           }
                       });
    if (peerHasSessionHolderVersion(transport)
        && receiverPlanner().anotherDeviceHoldsAReceiver(requester)) {
        ReceiverPlanner::TakeRequest request;
        request.need = ReceiverPlanner::Need::Retune;
        request.requester = requester;
        request.centreHz = frequencyHz;
        request.moving = {sliceId};
        askTake(transport, message, request);
    }
    return true;
}

void StationServer::answerWrite(SessionTransport* transport, const SessionMessage& write,
                                const QString& reason)
{
    if (peerFor(transport).agreedMinor < kDspControlSessionProtocolMinor
        || write.writeId == 0) {
        return;
    }
    // The value the Core keeps, so the device can show it again.
    QHash<QByteArray, MirrorUpdate> current;
    for (const MirrorUpdate& u : m_mirror->snapshot(write.objectKey)) {
        current.insert(u.name, u);
    }
    QList<SessionPropertyResult> results;
    QSet<QByteArray> reported;
    for (const MirrorUpdate& update : write.updates) {
        if (reported.contains(update.name)) {
            continue;
        }
        reported.insert(update.name);
        SessionPropertyResult result;
        result.property = update.name;
        result.accepted = false;
        result.reason = reason;
        result.hasValue = current.contains(update.name);
        if (result.hasValue) {
            result.value = current.value(update.name);
        }
        results.append(result);
    }
    send(transport, SessionMessages::propertyResult(write.objectKey, write.writeId, results));
}

QString StationServer::namesOf(const QList<ReceiverPlanner::Disturbed>& disturbed) const
{
    QStringList names;
    for (const ReceiverPlanner::Disturbed& d : disturbed) {
        const QString name = planDevice(d.device).name;
        if (!name.isEmpty() && !names.contains(name)) {
            names.append(name);
        }
    }
    return ReceiverPlanner::joinWords(names);
}

// ── Asking (7.3) ─────────────────────────────────────────────────────────

void StationServer::refuseWhileAsking(SessionTransport* transport, const SessionMessage& original)
{
    if (original.kind == SessionMessageKind::PropertyWrite) {
        answerWrite(transport, original, QString::fromLatin1(kWaitingReason));
    } else if (original.kind == SessionMessageKind::CommandInvoke) {
        answerHere(transport, SessionMessages::commandResult(
            original.commandVerb, original.commandId, false,
            QString::fromLatin1(kWaitingReason), {}, {phaseNeedsConfirmation()}));
    } else if (original.kind == SessionMessageKind::SettingsWrite
               || original.kind == SessionMessageKind::SettingsRemove) {
        // iPhone app Task 75 (link section 8.1): the Core's own value goes
        // back with the reason, so the device shows it until it proceeds
        // (a removal too, fix wave I3).
        const QString key = QString::fromUtf8(original.objectKey);
        const QVariant kept = m_settings.value(key);
        send(transport, SessionMessages::settingsReject(key, kept.isValid(), kept.toString(),
                                                        QString::fromLatin1(kWaitingReason)));
    }
}

void StationServer::sendQuestion(SessionTransport* transport, ConfirmStep::Question question,
                                 SessionPrompt prompt)
{
    question.id = m_confirm->nextId();
    question.device = peerFor(transport).sessionDeviceId;
    question.askedAtMs = m_deviceSessions->now();
    prompt.id = question.id;
    prompt.kind = question.kind;
    prompt.expiresInMs = ConfirmStep::kExpiryMs;
    if (question.original.kind == SessionMessageKind::CommandInvoke) {
        prompt.forCommandId = question.original.commandId;
    } else if (question.original.kind == SessionMessageKind::PropertyWrite
               && question.original.writeId != 0) {
        prompt.forWriteId = question.original.writeId;
    } else if (question.original.kind == SessionMessageKind::SettingsWrite
               || question.original.kind == SessionMessageKind::SettingsRemove) {
        prompt.forSettingsKey = QString::fromUtf8(question.original.objectKey);
    }
    question.namedSlices = slicesNamedBy(question);
    // Slice control plan Task 1: each slice as it is now, so a proceed
    // after its id was reused acts on nothing.
    const SliceOwnership* ownership = m_radioModel->sliceOwnership();
    question.namedRefs.clear();
    for (int id : std::as_const(question.namedSlices)) {
        question.namedRefs.append(ownership->refOf(id));
    }
    question.choiceRefs.clear();
    const bool slicesOffered = question.kind == QLatin1String("takeSlice");
    for (int target : std::as_const(question.choiceTargets)) {
        question.choiceRefs.append(slicesOffered && target >= 0 ? ownership->refOf(target)
                                                                : SliceOwnership::SliceRef{});
    }
    for (const QSet<int>& closes : std::as_const(question.shownChoices)) {
        for (int id : closes) {
            appendRefOnce(&question.shownRefs, ownership, id);
        }
    }
    recordRevisions(&question, ownership);
    if (peerHasSliceAccess(transport)) {
        recordListeners(&question, ownership);
    }
    m_confirm->ask(question);
    const SessionMessage request =
        SessionMessages::confirmRequest(prompt, QString::fromLatin1(kWaitingReason));
    if (m_holdQuestions) {
        // An answer to confirm.proceed or notice.takeBack goes out first;
        // the question it raised follows it (onTransportText).
        m_heldQuestions.append(qMakePair(transport, request));
        return;
    }
    send(transport, request);
}

void StationServer::sendHeldQuestions()
{
    m_holdQuestions = false;
    const QList<QPair<SessionTransport*, SessionMessage>> held = std::exchange(m_heldQuestions, {});
    for (const auto& [transport, request] : held) {
        if (hasPeer(transport)) {
            send(transport, request);
        }
    }
}

void StationServer::askPanMove(SessionTransport* transport, const SessionMessage& original,
                               const PanMoveCheck& check, const std::optional<QJsonObject>& change)
{
    refuseWhileAsking(transport, original);
    ConfirmStep::Question question;
    question.kind = QStringLiteral("panMove");
    question.held = original.kind == SessionMessageKind::PropertyWrite
                        ? ConfirmStep::Held::PropertyWrite
                        : ConfirmStep::Held::Command;
    question.original = original;
    question.stream = check.stream;
    question.centreHz = check.centreHz;
    question.exemptSliceId = check.exemptSliceId;
    for (const ReceiverPlanner::Disturbed& d : check.named) {
        question.shown.insert(QStringLiteral("%1|%2|%3").arg(QString::fromLatin1(d.device.toHex()))
                                  .arg(d.sliceId)
                                  .arg(ReceiverPlanner::effectName(d.effect)));
        appendRefOnce(&question.shownRefs, m_radioModel->sliceOwnership(), d.sliceId);
    }
    SessionPrompt prompt;
    prompt.affected = questionPlanner(transport).affectedJson(check.named);
    prompt.change = change;
    sendQuestion(transport, question, prompt);
}

bool StationServer::askTake(SessionTransport* transport, const SessionMessage& original,
                            const ReceiverPlanner::TakeRequest& request)
{
    const ReceiverPlanner planner = questionPlanner(transport);
    QList<ReceiverPlanner::Choice> choices = planner.receiverChoices(request);
    if (choices.isEmpty()) {
        return false;
    }
    if (request.need == ReceiverPlanner::Need::AddSlice
        || request.need == ReceiverPlanner::Need::AddPan) {
        const std::optional<QString> panId = addPanIdFor(original);
        if (!panId) {
            return false;
        }
        bool anyTakeable = false;
        bool initiallyTakeable = false;
        for (ReceiverPlanner::Choice& candidate : choices) {
            if (candidate.takeable) {
                initiallyTakeable = true;
                const ReceiverPlanner::AddPlacement placement =
                    planner.planAddAfterClosing(request.requester, *panId, candidate.closes);
                if (!placement.fits()) {
                    candidate.takeable = false;
                    candidate.why = placement.reason;
                }
            }
            anyTakeable = anyTakeable || candidate.takeable;
        }
        if (initiallyTakeable && !anyTakeable) {
            return false;
        }
    }
    ConfirmStep::Question question;
    question.kind = QStringLiteral("takeReceiver");
    question.held = original.kind == SessionMessageKind::PropertyWrite
                        ? ConfirmStep::Held::PropertyWrite
                        : ConfirmStep::Held::Command;
    question.original = original;
    question.need = static_cast<int>(request.need);
    question.centreHz = request.centreHz;
    question.moving = request.moving;
    for (const ReceiverPlanner::Choice& c : choices) {
        question.choiceTargets.append(c.stream);
        question.choiceTakeable.append(c.takeable);
        question.shownChoices.append(QSet<int>(c.closes.cbegin(), c.closes.cend()));
        for (int id : c.closes) {
            question.shown.insert(shownVictimKey(
                c.choice, id, m_radioModel->sliceOwnership()->mark(id).subject()));
        }
    }
    SessionPrompt prompt;
    prompt.choices = planner.receiverChoicesJson(choices);
    sendQuestion(transport, question, prompt);
    return true;
}

bool StationServer::askTakeSlice(SessionTransport* transport, const SessionMessage& original,
                                 const QList<ReceiverPlanner::Choice>& choices)
{
    const std::optional<QString> panId = addPanIdFor(original);
    if (!panId) {
        return false;
    }
    QList<ReceiverPlanner::Choice> offered = choices;
    const ReceiverPlanner planner = questionPlanner(transport);
    const QByteArray requester = peerFor(transport).sessionDeviceId;
    bool anyTakeable = false;
    for (ReceiverPlanner::Choice& candidate : offered) {
        const ReceiverPlanner::AddPlacement placement =
            planner.planAddAfterClosing(requester, *panId, {candidate.sliceId});
        if (!placement.fits()) {
            candidate.takeable = false;
            candidate.why = placement.reason;
        }
        anyTakeable = anyTakeable || candidate.takeable;
    }
    if (!anyTakeable) {
        return false;
    }
    ConfirmStep::Question question;
    question.kind = QStringLiteral("takeSlice");
    question.held = ConfirmStep::Held::Command;
    question.original = original;
    for (const ReceiverPlanner::Choice& c : offered) {
        question.choiceTargets.append(c.sliceId);
        question.choiceTakeable.append(c.takeable);
        question.shownChoices.append(QSet<int>(c.closes.cbegin(), c.closes.cend()));
        question.shownOwners.append(m_radioModel->sliceOwnership()->mark(c.sliceId).subject());
    }
    SessionPrompt prompt;
    prompt.choices = planner.sliceChoicesJson(offered);
    sendQuestion(transport, question, prompt);
    return true;
}

// ── Applying ─────────────────────────────────────────────────────────────

void StationServer::applyPanMove(const PanMoveCheck& check, const QByteArray& requester)
{
    // Ruling 6.4: each other device's slice outside the new window closes
    // when no receiver would take it, then the window moves and the rest
    // are placed again (moveStreamWindowFor), as planned.
    QHash<QByteArray, QList<SavedSlice>> closedBy;
    QHash<QByteArray, QList<int>> movedBy;
    // Slice control plan Task 9: every other listener of a closed slice is
    // told too.
    QHash<QByteArray, QList<SavedSlice>> listenedBy;
    const auto close = [&](int id, const QByteArray& device) {
        const QList<QByteArray> listeners = m_radioModel->sliceOwnership()->listenersOf(id);
        SavedSlice closed;
        if (closeSliceFor(id, saveForAbsentSubject(id), &closed)) {
            closedBy[device].append(closed);
            for (const QByteArray& listener : listeners) {
                if (listener != device) {
                    listenedBy[listener].append(closed);
                }
            }
        }
    };
    for (const ReceiverPlanner::Disturbed& d : check.plan.disturbed) {
        if (d.effect == ReceiverPlanner::Effect::Closes) {
            close(d.sliceId, d.device);
        }
    }
    const QList<int> unplaced =
        m_radioModel->moveStreamWindowFor(check.stream, check.centreHz, check.exemptSliceId);
    for (const int id : unplaced) {
        close(id, m_radioModel->sliceOwnership()->mark(id).subject());
    }
    for (const ReceiverPlanner::Disturbed& d : check.plan.disturbed) {
        if (d.effect == ReceiverPlanner::Effect::Moves && !unplaced.contains(d.sliceId)
            && m_radioModel->sliceById(d.sliceId) != nullptr) {
            movedBy[d.device].append(d.sliceId);
        }
    }
    const ReceiverPlanner planner = receiverPlanner();
    for (auto it = movedBy.cbegin(); it != movedBy.cend(); ++it) {
        QStringList letters;
        for (int id : it.value()) {
            letters.append(ReceiverPlanner::letterOf(id));
        }
        ConfirmStep::Notice notice;
        notice.device = it.key();
        notice.prompt.kind = QStringLiteral("sliceMoved");
        notice.prompt.slices = planner.noticeSlicesJson(it.value());
        notice.reason = sliceMovedReason(planDevice(requester).name, letters);
        tellDevice(notice, requester);
    }
    for (auto it = closedBy.cbegin(); it != closedBy.cend(); ++it) {
        ConfirmStep::Notice notice;
        notice.device = it.key();
        notice.prompt.kind = QStringLiteral("sliceClosed");
        notice.prompt.slices = savedSlicesJson(it.value());
        notice.reason = sliceClosedReason(planDevice(requester).name, lettersOf(it.value()));
        tellDevice(notice, requester);
    }
    tellListeners(listenedBy, requester, QStringLiteral("panMove"));
    endOlderWindowsWithoutSlices(closedBy.keys(), requester);
}

SessionMessage StationServer::runHeldCommand(const SessionMessage& original)
{
    SessionMessage captured;
    bool got = false;
    const auto previous = m_resultHook;
    // Fix wave I1: the held command's own result, by the session running
    // it now (the proceeder's) with its verb and id.
    const quint64 session = sessionIdOfOwner(m_dispatcher->resultOwner());
    m_resultHook = [&](SessionMessage& result) {
        const ResultKey key = resultKeyOf(result);
        if (key.sessionId == session && key.verb == original.commandVerb
            && key.commandId == original.commandId) {
            captured = result;
            got = true;
            return false;
        }
        return previous ? previous(result) : true;
    };
    m_dispatcher->dispatch(original);
    m_resultHook = previous;
    if (!got) {
        return SessionMessages::commandResult(original.commandVerb, original.commandId, false,
                                              QString::fromLatin1(kChangedReason), {});
    }
    return captured;
}

SessionMessage StationServer::applyHeld(SessionTransport* transport,
                                        const ConfirmStep::Question& question, int stream,
                                        const SessionMessage& invoke)
{
    const SessionMessage& original = question.original;
    // Ruling 7.4a: the proceed's answer carries what the original answer
    // would have carried had it applied at once.
    if (original.kind == SessionMessageKind::PropertyWrite) {
        const QList<SessionPropertyResult> results =
            applyPropertyWrite(transport, original, false, {});
        bool accepted = true;
        QString reason;
        QList<MirrorUpdate> values{MirrorUpdate{0, QByteArrayLiteral("objectKey"),
                                                MirrorWireKind::Utf8,
                                                QString::fromUtf8(original.objectKey)}};
        for (const SessionPropertyResult& r : results) {
            if (!r.accepted) {
                accepted = false;
                if (reason.isEmpty()) {
                    reason = r.reason;
                }
            }
            if (r.hasValue) {
                values.append(r.value);
            }
        }
        return SessionMessages::commandResult(invoke.commandVerb, invoke.commandId, accepted,
                                              reason, {original.objectKey}, values);
    }
    if (question.need == static_cast<int>(ReceiverPlanner::Need::PanMove)) {
        // Ruling 6.6 on the receiver the take freed (or one free by now).
        const bool moved =
            stream >= 0 && m_radioModel->moveSlicesToStream(question.moving, stream,
                                                            question.centreHz);
        QList<QByteArray> affected;
        for (int id : question.moving) {
            affected.append(ObjectRegistry::keyForSlice(id));
        }
        return SessionMessages::commandResult(
            invoke.commandVerb, invoke.commandId, moved,
            moved ? QString() : QString::fromLatin1(kCentreRefusedReason),
            moved ? affected : QList<QByteArray>{});
    }
    const SessionMessage result = runHeldCommand(original);
    if (original.commandVerb.startsWith("ps3.") && result.accepted && !isLastResult(result)) {
        // Fix wave I1: a PureSignal action re-run on proceed answers in
        // later phases; they are the requester's.
        m_resultRoutes.insert(ResultKey{peerFor(transport).sessionId, original.commandVerb,
                                        original.commandId},
                              QPointer<SessionTransport>(transport));
    }
    return SessionMessages::commandResult(invoke.commandVerb, invoke.commandId, result.accepted,
                                          result.reason, result.affectedKeys, result.updates);
}

bool StationServer::heldFitsNow(const ConfirmStep::Question& question) const
{
    const SliceStreamAllocator& live = m_radioModel->streamAllocator();
    int free = 0;
    for (int s = 0; s < live.streamCount(); ++s) {
        free += live.isStreamActive(s) ? 0 : 1;
    }
    switch (static_cast<ReceiverPlanner::Need>(question.need)) {
    case ReceiverPlanner::Need::AddSlice:
    case ReceiverPlanner::Need::AddPan: {
        const std::optional<QString> panId = addPanIdFor(question.original);
        return panId && receiverPlanner().planAddAfterClosing(question.device, *panId, {}).fits();
    }
    case ReceiverPlanner::Need::PanMove:
        return free > 0;
    case ReceiverPlanner::Need::Retune: {
        const int id = question.moving.value(0, -1);
        const SliceModel* slice = m_radioModel->sliceById(id);
        if (slice == nullptr || slice->streamIndex() < 0) {
            return false;
        }
        SliceStreamAllocator copy = live;
        const bool sole = m_radioModel->slicesOnStream(slice->streamIndex()).size() <= 1;
        return copy.retuneSlice(slice->streamIndex(), sole, slice->streamCtunPinned(),
                                question.centreHz)
                   .outcome
            != SliceStreamAllocator::Outcome::Rejected;
    }
    }
    return false;
}

QHash<QByteArray, QList<SavedSlice>> StationServer::closeForTake(
    const QList<int>& sliceIds, QHash<QByteArray, QList<SavedSlice>>* listenedBy)
{
    QHash<QByteArray, QList<SavedSlice>> closedBy;
    for (const int id : sliceIds) {
        const QByteArray device = m_radioModel->sliceOwnership()->mark(id).subject();
        const QList<QByteArray> listeners = m_radioModel->sliceOwnership()->listenersOf(id);
        SavedSlice closed;
        // Ruling 5.2's last paragraph: a slice held for a device that has
        // left is saved in its layout store; its notice has nobody to
        // reach, so the store is where its owner finds it again.
        if (closeSliceFor(id, saveForAbsentSubject(id), &closed)) {
            closedBy[device].append(closed);
            if (listenedBy != nullptr) {
                for (const QByteArray& listener : listeners) {
                    if (listener != device) {
                        (*listenedBy)[listener].append(closed);
                    }
                }
            }
        }
    }
    return closedBy;
}

void StationServer::tellListeners(const QHash<QByteArray, QList<SavedSlice>>& listenedBy,
                                  const QByteArray& by, const QString& why)
{
    const QString name = planDevice(by).name;
    for (auto it = listenedBy.cbegin(); it != listenedBy.cend(); ++it) {
        if (it.key() == by) {
            continue;
        }
        ConfirmStep::Notice notice;
        notice.device = it.key();
        notice.prompt.kind = QStringLiteral("sliceClosed");
        notice.prompt.slices = savedSlicesJson(it.value());
        notice.reason = listenedClosedReason(why, name, lettersOf(it.value()));
        tellDevice(notice, by);
    }
}

void StationServer::tellTaken(const QHash<QByteArray, QList<SavedSlice>>& closedBy,
                              const QByteArray& taker, const QString& kind, int stream,
                              int takerSlice)
{
    const QString takerName = planDevice(taker).name;
    for (auto it = closedBy.cbegin(); it != closedBy.cend(); ++it) {
        ConfirmStep::Notice notice;
        notice.device = it.key();
        notice.prompt.kind = kind;
        notice.prompt.takeBack = true;
        notice.prompt.slices = savedSlicesJson(it.value());
        const QStringList letters = lettersOf(it.value());
        const bool receiver = kind == QLatin1String("receiverTaken");
        notice.reason = receiver ? receiverTakenReason(takerName, letters)
                                 : sliceTakenReason(takerName, letters);
        notice.taker = taker;
        notice.takenStream = stream;
        notice.takerSlice = takerSlice;
        notice.closed = it.value();
        tellDevice(notice, taker);
    }
    endOlderWindowsWithoutSlices(closedBy.keys(), taker);
}

void StationServer::endOlderWindowsWithoutSlices(const QList<QByteArray>& devices,
                                                 const QByteArray& taker)
{
    QList<SessionTransport*> ending;
    for (const QByteArray& device : devices) {
        SessionTransport* transport = liveTransportFor(device);
        if (transport != nullptr && !peerHasSessionHolderVersion(transport)
            && m_radioModel->sliceOwnership()->ownedBy(device).isEmpty()) {
            ending.append(transport);
        }
    }
    const QString reason = takenOverReason(planDevice(taker).name);
    for (SessionTransport* transport : ending) {
        // Ruling 6.10: an older window cannot hold no slice. Its place is
        // freed: it left, in effect, when its last slice went.
        if (m_peers.contains(transport)) {
            m_peers[transport].leaving = true;
            dropPeer(transport, reason, true, /*retryable=*/false,
                     QString::fromLatin1(SessionEndCode::kTakenOver));
        }
    }
}

// ── Notices (7.4) ────────────────────────────────────────────────────────

void StationServer::tellDevice(ConfirmStep::Notice notice, const QByteArray& by)
{
    if (notice.device.isEmpty() || !m_deviceSessions->entry(notice.device)) {
        return;
    }
    notice.id = m_confirm->nextId();
    notice.happenedAtMs = m_deviceSessions->now();
    notice.prompt.id = notice.id;
    if (!by.isEmpty()) {
        const ReceiverPlanner::DeviceInfo who = planDevice(by);
        notice.prompt.byDeviceId = who.wireId;
        notice.prompt.byName = who.name;
        notice.prompt.byShortName = who.shortName;
        notice.prompt.byKind = who.kind;
        notice.prompt.bySource = QStringLiteral("device");
    }
    if (notice.prompt.takeBack) {
        m_confirm->keepTakeBack(notice);
    }
    SessionTransport* transport = liveTransportFor(notice.device);
    if (transport != nullptr) {
        if (peerHasSessionHolderVersion(transport)) {
            sendNotice(transport, notice);
        }
        return;
    }
    // An away device's notice waits for its return.
    m_confirm->keepPending(notice);
}

void StationServer::sendNotice(SessionTransport* transport, const ConfirmStep::Notice& notice)
{
    SessionPrompt prompt = notice.prompt;
    prompt.secondsAgo = std::max<qint64>(0, (m_deviceSessions->now() - notice.happenedAtMs) / 1000);
    // Take-over parity: Take it back on controlTaken is offered only to a
    // peer at sliceAccessVersion 2; any other is told as before.
    // Take-over fix wave (M-1): as before exactly, so the slice entry's
    // incarnation and controlRevision, which only Take it back reads, go
    // too.
    if (prompt.kind == QLatin1String("controlTaken") && !peerTakesControlBack(transport)) {
        prompt.takeBack = false;
        if (prompt.slices.has_value()) {
            QJsonArray entries;
            for (const QJsonValue& value : *prompt.slices) {
                QJsonObject entry = value.toObject();
                entry.remove(QStringLiteral("incarnation"));
                entry.remove(QStringLiteral("controlRevision"));
                entries.append(entry);
            }
            prompt.slices = entries;
        }
    }
    send(transport, SessionMessages::notice(prompt, notice.reason));
}

QString StationServer::notRestoredSentence(qsizetype count)
{
    return QStringLiteral("%1 of your slices could not be restored: all the radio's "
                          "receivers are in use.")
        .arg(count);
}

void StationServer::deliverAdmissionNotices(SessionTransport* transport,
                                            std::optional<qint64> timeRanOutAtMs)
{
    const auto peer = m_peers.constFind(transport);
    if (peer == m_peers.cend()) {
        return;
    }
    const QByteArray device = peer->sessionDeviceId;
    const QList<ConfirmStep::Notice> waiting = m_confirm->takePending(device);
    if (device.isEmpty() || !peerHasSessionHolderVersion(transport)) {
        return;  // an older window is told nothing (10.7)
    }
    const QList<SavedSlice> notRestored = peer->returning ? QList<SavedSlice>{}
                                                          : m_slicesNotRestored.value(device);
    if (timeRanOutAtMs) {
        // D62: back after its 3 minutes. (", and transmit was freed" joins
        // it when it held transmit, from Task 34.)
        ConfirmStep::Notice notice;
        notice.id = m_confirm->nextId();
        notice.device = device;
        notice.happenedAtMs = *timeRanOutAtMs;
        notice.prompt.id = notice.id;
        notice.prompt.kind = QStringLiteral("graceEnded");
        notice.prompt.slices = savedSlicesJson(notRestored);
        // Fix wave (Minor, 4.5): with a saved slice that did not fit, the
        // notice names what was not restored, as slicesNotRestored does,
        // and does not say the slices are back.
        notice.reason = notRestored.isEmpty()
            ? QStringLiteral("You were away for more than 3 minutes. Your slices are back.")
            : QStringLiteral("You were away for more than 3 minutes. %1")
                  .arg(notRestoredSentence(notRestored.size()));
        sendNotice(transport, notice);
    } else if (!notRestored.isEmpty()) {
        ConfirmStep::Notice notice;
        notice.id = m_confirm->nextId();
        notice.device = device;
        notice.happenedAtMs = m_deviceSessions->now();
        notice.prompt.id = notice.id;
        notice.prompt.kind = QStringLiteral("slicesNotRestored");
        notice.prompt.slices = savedSlicesJson(notRestored);
        notice.reason = notRestoredSentence(notRestored.size());
        sendNotice(transport, notice);
    }
    for (ConfirmStep::Notice notice : waiting) {
        // After its 3 minutes they still arrive, without Take it back: the
        // end of the 180 s ended it and saved the slice
        // (saveTakenSlicesFor). Fix wave 3: one whose record is still kept
        // (no radio was connected to save in then) keeps Take it back.
        if (timeRanOutAtMs && notice.prompt.takeBack
            && !m_confirm->takeBackRecord(device, notice.id)) {
            notice.prompt.takeBack = false;
        }
        sendNotice(transport, notice);
    }
}

// ── Ruling 10.2: an older window's slice at admission ───────────────────

QString StationServer::olderWindowWithoutSliceReason(SessionTransport* transport) const
{
    if (m_radioModel.isNull() || m_radioModel->role() != RadioModel::Role::Local
        || peerHoldsSessions(transport)) {
        return {};
    }
    const QByteArray device = peerFor(transport).sessionDeviceId;
    if (device.isEmpty() || !m_radioModel->sliceOwnership()->ownedBy(device).isEmpty()) {
        return {};
    }
    return m_radioModel->slices().size() >= m_radioModel->sliceCapForDevices()
               ? QString::fromLatin1(kCapFullAtAdmissionReason)
               : QString::fromLatin1(kReceiversFullAtAdmissionReason);
}

// ── Answers: confirm.proceed, confirm.cancel, notice.takeBack ───────────

QList<int> StationServer::slicesNamedBy(const ConfirmStep::Question& question) const
{
    QList<int> ids;
    const SessionMessage& original = question.original;
    if (original.kind == SessionMessageKind::PropertyWrite
        && original.objectKey.startsWith("slice:")) {
        bool ok = false;
        const int id = original.objectKey.mid(6).toInt(&ok);
        if (ok) {
            ids.append(id);
        }
    } else if (original.kind == SessionMessageKind::CommandInvoke
               && question.held != ConfirmStep::Held::TakeBack) {
        int id = -1;
        if (readInt(original.arguments, "sliceId", &id) && id >= 0) {
            ids.append(id);
        }
    }
    for (int id : question.moving) {
        if (!ids.contains(id)) {
            ids.append(id);
        }
    }
    return ids;
}

void StationServer::dropQuestionsNaming(int sliceId)
{
    if (m_confirm) {
        m_confirm->dropQuestionsNaming(sliceId);
    }
}

QString StationServer::changedSinceAskedReason(const QString& kind)
{
    return kind == QLatin1String("sharedSetting") ? sharedTargetChangedReason()
                                                  : QString::fromLatin1(kChangedReason);
}

SessionMessage StationServer::answerConfirm(const SessionMessage& invoke, int id, int choice)
{
    const auto refuse = [&invoke](const QString& reason) {
        return SessionMessages::commandResult(invoke.commandVerb, invoke.commandId, false, reason,
                                              {});
    };
    SessionTransport* transport = m_dispatchingTransport;
    const QByteArray device = peerFor(transport).sessionDeviceId;
    // A question this answer raises follows the answer itself.
    m_holdQuestions = true;
    if (transport == nullptr || device.isEmpty() || m_radioModel.isNull()) {
        return refuse(QString::fromLatin1(kNoQuestionReason));
    }
    if (invoke.commandVerb == "notice.takeBack") {
        return askTakeBack(transport, invoke, id);
    }
    // Fix wave I2: a question dropped because a slice it named closed or
    // changed owner is answered as changed; cancelling it changes nothing.
    if (const std::optional<QString> dropped = m_confirm->takeDroppedAsChanged(device, id)) {
        if (invoke.commandVerb == "confirm.cancel") {
            return SessionMessages::commandResult(invoke.commandVerb, invoke.commandId, true,
                                                  QString(), {});
        }
        return refuse(changedSinceAskedReason(*dropped));
    }
    const std::optional<ConfirmStep::Question> question = m_confirm->answer(device, id);
    if (!question) {
        return refuse(QString::fromLatin1(kNoQuestionReason));
    }
    if (invoke.commandVerb == "confirm.cancel") {
        // Cancel changes nothing.
        return SessionMessages::commandResult(invoke.commandVerb, invoke.commandId, true, QString(),
                                              {});
    }
    // Ruling 7.5 (iPhone app Task 75): a question expires 60 s after it
    // was sent; a later proceed changes nothing.
    if (ConfirmStep::expired(*question, m_deviceSessions->now())) {
        return refuse(QString::fromLatin1(kExpiredReason));
    }
    // Fix wave I2: every slice the question names is still the
    // requester's. A slice id is reused (lowest free first), so a slice
    // closed meanwhile may be another device's now under the same id.
    const SliceOwnership* ownership = m_radioModel->sliceOwnership();
    for (int sliceId : question->namedSlices) {
        if (!ownership->isLive(sliceId)
            || !SliceAccessPolicy::mayChange(*ownership, device, sliceId)) {
            return refuse(changedSinceAskedReason(question->kind));
        }
    }
    // Slice control plan Task 1: and still the same slice, not a new one
    // made in the requester's name under the reused id.
    for (const SliceOwnership::SliceRef& ref : question->namedRefs) {
        if (!ownership->matches(ref) || revisionChanged(ownership, *question, ref.sliceId)) {
            return refuse(changedSinceAskedReason(question->kind));
        }
    }
    // Fix wave 2, Important 3 (rulings 7.4 and 8.11): a stored change
    // confirmed while a holder is on the air asks the freeze and the
    // on-air take rule again before anything of it applies.
    {
        const TxRefusal frozen = proceedOnAirRefusal(*question, choice, device);
        if (!frozen.isEmpty()) {
            return SessionMessages::commandResult(
                invoke.commandVerb, invoke.commandId, false, frozen.text, {},
                {{0, "refusalCode", MirrorWireKind::Utf8, QString::fromUtf8(frozen.code)},
                 {0, "refusalFix", MirrorWireKind::Utf8, QString::fromUtf8(frozen.fix)}});
        }
    }
    // iPhone app plan Task 77: taking transmit, from tx.take or from Take
    // it back.
    if (question->kind == QLatin1String("takeTransmit")) {
        return proceedTakeTransmit(transport, *question, invoke);
    }
    if (question->kind == QLatin1String("sharedSetting")) {
        return proceedSharedSetting(transport, *question, invoke);
    }
    if (question->kind == QLatin1String("panMove")) {
        return proceedPanMove(transport, *question, invoke);
    }
    if (question->held == ConfirmStep::Held::TakeBack) {
        return proceedTakeBack(transport, *question, choice, invoke);
    }
    if (question->kind == QLatin1String("takeReceiver")) {
        return proceedTakeReceiver(transport, *question, choice, invoke);
    }
    if (question->kind == QLatin1String("takeSlice")) {
        return proceedTakeSlice(transport, *question, choice, invoke);
    }
    return refuse(QString::fromLatin1(kNoQuestionReason));
}

TxRefusal StationServer::proceedOnAirRefusal(const ConfirmStep::Question& question, int choice,
                                             const QByteArray& device) const
{
    // The stored change itself (a slice property write, removeSlice,
    // slice.selectBand) and the slices a pan move carries.
    TxRefusal refusal = freezeRefusalFor(question.original);
    for (int id : question.moving) {
        if (refusal.isEmpty()) {
            refusal = stationFreezeRefusal(id);
        }
    }
    if (!refusal.isEmpty()) {
        return refusal;
    }
    // A take whose receiver, or slice, carries the transmit slice of a
    // holder on the air (ruling 6.8), which the chooser showed takeable
    // before the key started.
    const std::optional<TransmitHolder::Holder> holder = onAirHolder();
    const SliceModel* txSlice = m_radioModel->txBoundSlice();
    if (!holder || holder->deviceId == device || txSlice == nullptr || choice < 0
        || choice >= question.choiceTargets.size()) {
        return {};
    }
    const int target = question.choiceTargets.at(choice);
    const bool touches = question.kind == QLatin1String("takeSlice")
        ? target == txSlice->sliceIndex()
        : question.kind == QLatin1String("takeReceiver")
            && m_radioModel->slicesOnStream(target).contains(txSlice->sliceIndex());
    return touches ? onAirWords(*holder) : TxRefusal{};
}

SessionMessage StationServer::askAgain(const SessionMessage& invoke)
{
    return SessionMessages::commandResult(invoke.commandVerb, invoke.commandId, false,
                                          QString::fromLatin1(kWaitingReason), {},
                                          {phaseNeedsConfirmation()});
}

SessionMessage StationServer::proceedPanMove(SessionTransport* transport,
                                             const ConfirmStep::Question& question,
                                             const SessionMessage& invoke)
{
    const QByteArray requester = question.device;
    // Step 5: the set again.
    const PanMoveCheck check = checkPanMove(requester, question.original);
    if (check.kind == PanMoveCheck::Kind::OnAir) {
        return SessionMessages::commandResult(
            invoke.commandVerb, invoke.commandId, false, check.onAir.text, {},
            {{0, "refusalCode", MirrorWireKind::Utf8, QString::fromUtf8(check.onAir.code)},
             {0, "refusalFix", MirrorWireKind::Utf8, QString::fromUtf8(check.onAir.fix)}});
    }
    if (check.kind == PanMoveCheck::Kind::None || check.stream != question.stream) {
        return SessionMessages::commandResult(invoke.commandVerb, invoke.commandId, false,
                                              QString::fromLatin1(kChangedReason), {});
    }
    // Slice control plan Task 1: a slice shown under an id that a new
    // slice has taken since: nothing moves or closes.
    for (const ReceiverPlanner::Disturbed& d : check.named) {
        if (incarnationChanged(m_radioModel->sliceOwnership(), question.shownRefs, d.sliceId)) {
            return SessionMessages::commandResult(invoke.commandVerb, invoke.commandId, false,
                                                  QString::fromLatin1(kChangedReason), {});
        }
    }
    if (check.kind == PanMoveCheck::Kind::Ask) {
        bool grew = false;
        for (const ReceiverPlanner::Disturbed& d : check.named) {
            const QString key = QStringLiteral("%1|%2|%3")
                                    .arg(QString::fromLatin1(d.device.toHex()))
                                    .arg(d.sliceId)
                                    .arg(ReceiverPlanner::effectName(d.effect));
            // Slice control fix wave: control that changed hands and came
            // back counts as grown; its listeners were never shown.
            grew = grew || !question.shown.contains(key)
                || revisionChanged(m_radioModel->sliceOwnership(), question, d.sliceId)
                || listenersGrew(m_radioModel->sliceOwnership(), question, d.sliceId);
        }
        if (grew) {
            // A device or an effect the operator was not shown: ask again,
            // apply nothing.
            SessionPrompt prompt;
            prompt.affected = questionPlanner(transport).affectedJson(check.named);
            if (question.original.kind == SessionMessageKind::PropertyWrite) {
                const SliceModel* slice = m_radioModel->sliceById(check.exemptSliceId);
                if (slice != nullptr) {
                    prompt.change = QJsonObject{
                        {QStringLiteral("label"), receiverLabel(check.stream)},
                        {QStringLiteral("from"), ReceiverPlanner::bandWords(slice->band())},
                        {QStringLiteral("to"),
                         ReceiverPlanner::bandWords(bandFromFrequency(check.centreHz))},
                    };
                }
            }
            ConfirmStep::Question again = question;
            again.shown.clear();
            again.shownRefs.clear();
            for (const ReceiverPlanner::Disturbed& d : check.named) {
                again.shown.insert(QStringLiteral("%1|%2|%3")
                                       .arg(QString::fromLatin1(d.device.toHex()))
                                       .arg(d.sliceId)
                                       .arg(ReceiverPlanner::effectName(d.effect)));
                appendRefOnce(&again.shownRefs, m_radioModel->sliceOwnership(), d.sliceId);
            }
            again.centreHz = check.centreHz;
            sendQuestion(transport, again, prompt);
            return askAgain(invoke);
        }
    }
    applyPanMove(check, requester);
    return applyHeld(transport, question, check.stream, invoke);
}

SessionMessage StationServer::proceedTakeReceiver(SessionTransport* transport,
                                                  const ConfirmStep::Question& question, int choice,
                                                  const SessionMessage& invoke)
{
    if (choice < 0 || choice >= question.choiceTargets.size()) {
        return SessionMessages::commandResult(invoke.commandVerb, invoke.commandId, false,
                                              QString::fromLatin1(kNoChoiceReason), {});
    }
    const int target = question.choiceTargets.at(choice);
    // A receiver free by now disturbs nobody: the held request as it was.
    if (heldFitsNow(question)) {
        int free = -1;
        for (int s = 0; s < m_radioModel->streamAllocator().streamCount() && free < 0; ++s) {
            if (!m_radioModel->streamAllocator().isStreamActive(s)) {
                free = s;
            }
        }
        return applyHeld(transport, question, free, invoke);
    }
    ReceiverPlanner::TakeRequest request;
    request.need = static_cast<ReceiverPlanner::Need>(question.need);
    request.requester = question.device;
    request.centreHz = question.centreHz;
    request.moving = question.moving;
    const QList<ReceiverPlanner::Choice> choices = receiverPlanner().receiverChoices(request);
    const ReceiverPlanner::Choice* now = nullptr;
    for (const ReceiverPlanner::Choice& c : choices) {
        if (c.stream == target) {
            now = &c;
        }
    }
    // Slice control plan Task 1: a closing slice shown under an id that a
    // new slice has taken since: nothing closes.
    if (now != nullptr) {
        for (int id : now->closes) {
            if (incarnationChanged(m_radioModel->sliceOwnership(), question.shownRefs, id)) {
                return SessionMessages::commandResult(invoke.commandVerb, invoke.commandId, false,
                                                      QString::fromLatin1(kChangedReason), {});
            }
        }
    }
    const QSet<int> shown = question.shownChoices.value(choice);
    bool grew = now == nullptr || !now->takeable;
    if (now != nullptr) {
        for (int id : now->closes) {
            // Slice control fix wave: control that changed hands and came
            // back counts as grown; its listeners were never shown.
            grew = grew || !shown.contains(id)
                || !question.shown.contains(shownVictimKey(
                    choice, id, m_radioModel->sliceOwnership()->mark(id).subject()))
                || revisionChanged(m_radioModel->sliceOwnership(), question, id)
                || listenersGrew(m_radioModel->sliceOwnership(), question, id);
        }
    }
    if (grew) {
        return askTake(transport, question.original, request) ? askAgain(invoke)
            : SessionMessages::commandResult(invoke.commandVerb, invoke.commandId, false,
                                             QString::fromLatin1(kChangedReason), {});
    }
    if (request.need == ReceiverPlanner::Need::AddSlice
        || request.need == ReceiverPlanner::Need::AddPan) {
        const std::optional<QString> panId = addPanIdFor(question.original);
        const ReceiverPlanner::AddPlacement placement = panId
            ? receiverPlanner().planAddAfterClosing(question.device, *panId, now->closes)
            : ReceiverPlanner::AddPlacement{};
        if (!panId || !placement.fits()) {
            if (panId && askTake(transport, question.original, request)) {
                return askAgain(invoke);
            }
            return SessionMessages::commandResult(
                invoke.commandVerb, invoke.commandId, false,
                panId ? placement.reason : QStringLiteral("The Core could not read this request."), {});
        }
    }
    if (request.need == ReceiverPlanner::Need::PanMove
        && !receiverPlanner().panMoveFitsAfterClosing(
            target, question.centreHz, question.moving, now->closes)) {
        return SessionMessages::commandResult(invoke.commandVerb, invoke.commandId, false,
                                              QString::fromLatin1(kCentreRefusedReason), {});
    }
    if (now->closes.size() >= m_radioModel->slices().size()) {
        return SessionMessages::commandResult(invoke.commandVerb, invoke.commandId, false,
                                              QStringLiteral("The Core must keep its last receiver slice."), {});
    }
    // Ruling 6.7: every other device's slice on it closes, then the held
    // request is applied on the freed receiver, before anyone can claim it.
    QHash<QByteArray, QList<SavedSlice>> listenedBy;
    const QHash<QByteArray, QList<SavedSlice>> closedBy = closeForTake(now->closes, &listenedBy);
    if (!m_radioModel->slicesOnStream(target).isEmpty()
        && (request.need == ReceiverPlanner::Need::Retune
            || request.need == ReceiverPlanner::Need::PanMove)) {
        // The taker's own slices there stay; the window centres where the
        // change asked. A retuned slice goes with its own write.
        m_radioModel->moveStreamWindowFor(
            target, question.centreHz,
            request.need == ReceiverPlanner::Need::Retune ? question.moving.value(0, -1) : -1);
    }
    const SessionMessage result = applyHeld(transport, question, target, invoke);
    tellTaken(closedBy, question.device, QStringLiteral("receiverTaken"), target, -1);
    tellListeners(listenedBy, question.device, QStringLiteral("receiverTaken"));
    return result;
}

SessionMessage StationServer::proceedTakeSlice(SessionTransport* transport,
                                               const ConfirmStep::Question& question, int choice,
                                               const SessionMessage& invoke)
{
    if (choice < 0 || choice >= question.choiceTargets.size()) {
        return SessionMessages::commandResult(invoke.commandVerb, invoke.commandId, false,
                                              QString::fromLatin1(kNoChoiceReason), {});
    }
    if (heldFitsNow(question)) {
        return applyHeld(transport, question, -1, invoke);
    }
    const int target = question.choiceTargets.at(choice);
    const SliceOwnership* ownership = m_radioModel->sliceOwnership();
    // Slice control plan Task 1: the slice offered was closed and a new
    // one has its id, whoever it belongs to: nothing closes.
    const SliceOwnership::SliceRef offered = question.choiceRefs.value(choice);
    if (ownership->isLive(target) && offered.sliceId == target && !ownership->matches(offered)) {
        return SessionMessages::commandResult(invoke.commandVerb, invoke.commandId, false,
                                              QString::fromLatin1(kChangedReason), {});
    }
    // Slice control fix wave: control that changed hands and came back
    // asks again like an owner change; its listeners were never shown.
    if (!ownership->isLive(target)
        || ownership->mark(target).subject() != question.shownOwners.value(choice)
        || revisionChanged(ownership, question, target)
        || listenersGrew(ownership, question, target)) {
        const QList<ReceiverPlanner::Choice> choices =
            receiverPlanner().sliceChoices(question.device);
        if (!choices.isEmpty() && askTakeSlice(transport, question.original, choices)) {
            return askAgain(invoke);
        }
        return SessionMessages::commandResult(invoke.commandVerb, invoke.commandId, false,
                                              QString::fromLatin1(kChangedReason), {});
    }
    const std::optional<QString> panId = addPanIdFor(question.original);
    const ReceiverPlanner planner = receiverPlanner();
    const ReceiverPlanner::AddPlacement placement = panId
        ? planner.planAddAfterClosing(question.device, *panId, {target})
        : ReceiverPlanner::AddPlacement{};
    if (!panId || !placement.fits()) {
        if (panId && !placement.receiverFits
            && planner.anotherDeviceHoldsAReceiver(question.device)) {
            ReceiverPlanner::TakeRequest request;
            request.need = !panId->isEmpty()
                    && !m_radioModel->panHasSlicesFor(*panId, question.device)
                ? ReceiverPlanner::Need::AddPan : ReceiverPlanner::Need::AddSlice;
            request.requester = question.device;
            if (askTake(transport, question.original, request)) {
                return askAgain(invoke);
            }
        }
        return SessionMessages::commandResult(
            invoke.commandVerb, invoke.commandId, false,
            panId ? placement.reason : QStringLiteral("The Core could not read this request."), {});
    }
    QHash<QByteArray, QList<SavedSlice>> listenedBy;
    const QHash<QByteArray, QList<SavedSlice>> closedBy = closeForTake({target}, &listenedBy);
    const SessionMessage result = applyHeld(transport, question, -1, invoke);
    int takerSlice = -1;
    for (const QByteArray& key : result.affectedKeys) {
        if (key.startsWith("slice:")) {
            takerSlice = key.mid(6).toInt();
        }
    }
    tellTaken(closedBy, question.device, QStringLiteral("sliceTaken"), -1, takerSlice);
    tellListeners(listenedBy, question.device, QStringLiteral("sliceTaken"));
    return result;
}

// ── Take it back (section 6.4) ───────────────────────────────────────────

SessionMessage StationServer::takeBackControl(SessionTransport* transport,
                                              const SessionMessage& invoke,
                                              const ConfirmStep::Notice& record)
{
    const QByteArray device = peerFor(transport).sessionDeviceId;
    if (!peerTakesControlBack(transport) || m_sliceAccessController == nullptr
        || !record.prompt.slices || record.prompt.slices->isEmpty()) {
        return SessionMessages::commandResult(invoke.commandVerb, invoke.commandId, false,
                                              QString::fromLatin1(kNoTakeBackReason), {});
    }
    // The slice, its incarnation and the control revision the notice named:
    // a slice made again under the letter, or one whose control moved on
    // since, is refused as slice.takeControl refuses it.
    const QJsonObject entry = record.prompt.slices->first().toObject();
    SliceOwnership::SliceRef ref;
    ref.sliceId = entry.value(QStringLiteral("sliceId")).toInt(-1);
    ref.incarnation =
        static_cast<quint64>(entry.value(QStringLiteral("incarnation")).toInteger(0));
    const quint64 revision =
        static_cast<quint64>(entry.value(QStringLiteral("controlRevision")).toInteger(0));
    // The same slice, but its control moved since the notice: the device
    // that took it released it, or it was taken again. Nothing is left to
    // take back, so the record is void and the answer says so, not the
    // stale-revision words slice.takeControl keeps for a real race.
    const SliceOwnership* current =
        m_radioModel.isNull() ? nullptr : m_radioModel->sliceOwnership();
    if (current != nullptr && current->matches(ref)
        && current->controlRevision(ref.sliceId) != revision) {
        m_confirm->forgetTakeBack(device, record.id);
        return SessionMessages::commandResult(invoke.commandVerb, invoke.commandId, false,
                                              QString::fromLatin1(kNoTakeBackReason), {});
    }
    // The same checks and change as slice.takeControl: refused while the
    // slice transmits; the taker's transmit selection of it is cleared and
    // its transmit binding does not pick it up (ruling Q8).
    const SliceAccessController::Result result =
        m_sliceAccessController->takeControl(device, ref, revision);
    const SliceOwnership* ownership =
        m_radioModel.isNull() ? nullptr : m_radioModel->sliceOwnership();
    if (result.accepted || ownership == nullptr || !ownership->matches(ref)
        || ownership->controlRevision(ref.sliceId) != revision) {
        // Taken back, or it never can be now.
        m_confirm->forgetTakeBack(device, record.id);
    }
    QList<MirrorUpdate> values;
    if (result.accepted) {
        values.append(MirrorUpdate{0, QByteArrayLiteral("controlRevision"), MirrorWireKind::Int64,
                                   QVariant(static_cast<qlonglong>(result.controlRevision))});
    }
    return SessionMessages::commandResult(
        invoke.commandVerb, invoke.commandId, result.accepted, result.reason,
        result.accepted ? result.affected : QList<QByteArray>{}, values);
}

SessionMessage StationServer::askTakeBack(SessionTransport* transport, const SessionMessage& invoke,
                                          int noticeId)
{
    const QByteArray device = peerFor(transport).sessionDeviceId;
    const std::optional<ConfirmStep::Notice> record = m_confirm->takeBackRecord(device, noticeId);
    if (!record) {
        return SessionMessages::commandResult(invoke.commandVerb, invoke.commandId, false,
                                              QString::fromLatin1(kNoTakeBackReason), {});
    }
    // iPhone app plan Task 77 (section 8.6): Take it back for transmit is
    // tx.take with its usual confirmation.
    if (record->prompt.kind == QLatin1String("transmitTaken")) {
        return takeBackTransmit(transport, invoke, noticeId);
    }
    // Take-over parity: Take it back for control is slice.takeControl the
    // other way.
    if (record->prompt.kind == QLatin1String("controlTaken")) {
        return takeBackControl(transport, invoke, *record);
    }
    const ReceiverPlanner planner = questionPlanner(transport);
    const SliceStreamAllocator& live = m_radioModel->streamAllocator();
    QList<double> savedFrequencies;
    for (const SavedSlice& saved : record->closed) {
        savedFrequencies.append(saved.frequencyHz);
    }
    ConfirmStep::Question question;
    question.held = ConfirmStep::Held::TakeBack;
    question.noticeId = noticeId;
    question.original = invoke;
    SessionPrompt prompt;
    if (record->prompt.kind == QLatin1String("receiverTaken")) {
        // The same question the other way: the receiver the taker holds
        // now first, then any receiver free by then.
        question.kind = QStringLiteral("takeReceiver");
        QList<ReceiverPlanner::Choice> choices;
        if (record->takenStream >= 0 && live.isStreamActive(record->takenStream)) {
            ReceiverPlanner::Choice c;
            c.stream = record->takenStream;
            for (int id : m_radioModel->slicesOnStream(record->takenStream)) {
                if (m_radioModel->sliceOwnership()->mark(id).subject() != device) {
                    c.closes.append(id);
                }
            }
            choices.append(c);
        }
        for (int s = 0; s < live.streamCount(); ++s) {
            if (!live.isStreamActive(s)) {
                ReceiverPlanner::Choice c;
                c.stream = s;
                choices.append(c);
            }
        }
        for (int i = 0; i < choices.size(); ++i) {
            choices[i].choice = i;
            const ReceiverPlanner::RestorePlacement placement =
                planner.planRestoreAfterClosing(savedFrequencies, choices.at(i).closes);
            choices[i].takeable = placement.fits;
            choices[i].why = placement.reason;
            question.choiceTargets.append(choices.at(i).stream);
            question.choiceTakeable.append(choices.at(i).takeable);
            question.shownChoices.append(
                QSet<int>(choices.at(i).closes.cbegin(), choices.at(i).closes.cend()));
            for (int id : choices.at(i).closes) {
                question.shown.insert(shownVictimKey(
                    i, id, m_radioModel->sliceOwnership()->mark(id).subject()));
            }
        }
        prompt.choices = planner.receiverChoicesJson(choices);
    } else {
        question.kind = QStringLiteral("takeSlice");
        QList<ReceiverPlanner::Choice> choices;
        const SliceOwnership* ownership = m_radioModel->sliceOwnership();
        if (record->takerSlice >= 0 && ownership->isLive(record->takerSlice)
            && ownership->mark(record->takerSlice).subject() == record->taker) {
            ReceiverPlanner::Choice c;
            c.sliceId = record->takerSlice;
            c.closes = {record->takerSlice};
            choices.append(c);
        }
        for (int i = 0; i < choices.size(); ++i) {
            choices[i].choice = i;
            const ReceiverPlanner::RestorePlacement placement =
                planner.planRestoreAfterClosing(savedFrequencies, choices.at(i).closes);
            choices[i].takeable = placement.fits;
            choices[i].why = placement.reason;
        }
        QJsonArray json = planner.sliceChoicesJson(choices);
        for (const ReceiverPlanner::Choice& c : choices) {
            question.choiceTargets.append(c.sliceId);
            question.choiceTakeable.append(c.takeable);
            question.shownChoices.append(QSet<int>{c.sliceId});
            question.shownOwners.append(record->taker);
            question.shown.insert(shownVictimKey(
                c.choice, c.sliceId, m_radioModel->sliceOwnership()->mark(c.sliceId).subject()));
        }
        if (m_radioModel->slices().size() < m_radioModel->sliceCapForDevices()) {
            // A slice free by then disturbs nobody.
            const ReceiverPlanner::RestorePlacement placement =
                planner.planRestoreAfterClosing(savedFrequencies, {});
            json.append(QJsonObject{{QStringLiteral("choice"), json.size()},
                                    {QStringLiteral("sliceId"), -1},
                                    {QStringLiteral("takeable"), placement.fits},
                                    {QStringLiteral("why"), placement.reason}});
            question.choiceTargets.append(-1);
            question.choiceTakeable.append(placement.fits);
            question.shownChoices.append(QSet<int>{});
            question.shownOwners.append(QByteArray());
        }
        prompt.choices = json;
    }
    if (question.choiceTargets.isEmpty()) {
        return SessionMessages::commandResult(invoke.commandVerb, invoke.commandId, false,
                                              QString::fromLatin1(kNoTakeBackReason), {});
    }
    sendQuestion(transport, question, prompt);
    return SessionMessages::commandResult(invoke.commandVerb, invoke.commandId, false,
                                          QString::fromLatin1(kWaitingReason), {},
                                          {phaseNeedsConfirmation()});
}

SessionMessage StationServer::proceedTakeBack(SessionTransport* transport,
                                              const ConfirmStep::Question& question, int choice,
                                              const SessionMessage& invoke)
{
    const QByteArray device = question.device;
    const std::optional<ConfirmStep::Notice> record =
        m_confirm->takeBackRecord(device, question.noticeId);
    if (!record) {
        return SessionMessages::commandResult(invoke.commandVerb, invoke.commandId, false,
                                              QString::fromLatin1(kNoTakeBackReason), {});
    }
    if (choice < 0 || choice >= question.choiceTargets.size()) {
        return SessionMessages::commandResult(invoke.commandVerb, invoke.commandId, false,
                                              QString::fromLatin1(kNoChoiceReason), {});
    }
    const int target = question.choiceTargets.at(choice);
    const bool receiver = record->prompt.kind == QLatin1String("receiverTaken");
    // Slice control plan Task 1: the taker's slice offered was closed and a
    // new one has its id: nothing closes.
    {
        const SliceOwnership* ownership = m_radioModel->sliceOwnership();
        const SliceOwnership::SliceRef offered = question.choiceRefs.value(choice);
        if (!receiver && target >= 0 && ownership->isLive(target) && offered.sliceId == target
            && !ownership->matches(offered)) {
            return SessionMessages::commandResult(invoke.commandVerb, invoke.commandId, false,
                                                  QString::fromLatin1(kChangedReason), {});
        }
    }
    QList<int> closes;
    if (receiver && target >= 0 && m_radioModel->streamAllocator().isStreamActive(target)) {
        for (int id : m_radioModel->slicesOnStream(target)) {
            if (m_radioModel->sliceOwnership()->mark(id).subject() != device) {
                closes.append(id);
            }
        }
    } else if (!receiver && target >= 0 && m_radioModel->sliceOwnership()->isLive(target)) {
        closes.append(target);
    } else if (!receiver && target < 0
               && m_radioModel->slices().size() >= m_radioModel->sliceCapForDevices()) {
        // The free slice went meanwhile.
        return SessionMessages::commandResult(invoke.commandVerb, invoke.commandId, false,
                                              QString::fromLatin1(kChangedReason), {});
    }
    for (int id : closes) {
        if (incarnationChanged(m_radioModel->sliceOwnership(), question.shownRefs, id)) {
            return SessionMessages::commandResult(invoke.commandVerb, invoke.commandId, false,
                                                  QString::fromLatin1(kChangedReason), {});
        }
    }
    const QSet<int> shown = question.shownChoices.value(choice);
    for (int id : closes) {
        if (!shown.contains(id)
            || !question.shown.contains(shownVictimKey(
                choice, id, m_radioModel->sliceOwnership()->mark(id).subject()))
            || revisionChanged(m_radioModel->sliceOwnership(), question, id)
            || listenersGrew(m_radioModel->sliceOwnership(), question, id)) {
            // Something the operator was not shown (slice control fix wave:
            // including control that changed hands and came back): ask the
            // other way again.
            const SessionMessage again =
                askTakeBack(transport, invoke, static_cast<int>(question.noticeId));
            return again.reason == QLatin1String(kWaitingReason) ? askAgain(invoke) : again;
        }
    }
    QList<double> savedFrequencies;
    for (const SavedSlice& saved : record->closed) {
        savedFrequencies.append(saved.frequencyHz);
    }
    const ReceiverPlanner::RestorePlacement placement =
        receiverPlanner().planRestoreAfterClosing(savedFrequencies, closes);
    if (!placement.fits) {
        return SessionMessages::commandResult(invoke.commandVerb, invoke.commandId, false,
                                              placement.reason, {});
    }
    QHash<QByteArray, QList<SavedSlice>> listenedBy;
    const QHash<QByteArray, QList<SavedSlice>> closedBy = closeForTake(closes, &listenedBy);
    // On proceed the Core recreates the device's closed slices at their
    // frequencies, modes and pans, with their settings (5.3).
    QList<QByteArray> affected;
    AppSettings& store = AppSettings::instance();
    const QString mac = m_radioModel->currentRadioMac();
    int firstRestored = -1;
    for (const SavedSlice& entry : record->closed) {
        const int sliceId = m_radioModel->sliceById(entry.id) == nullptr
                                    && entry.id < WdspEngine::kMaxSliceChannels
                                ? entry.id
                                : m_radioModel->lowestFreeSliceId();
        if (sliceId < 0) {
            continue;
        }
        if (!mac.isEmpty()) {
            DeviceLayoutStore::writeSliceSettings(store, mac, sliceId, entry.settings);
        }
        const ReceiveSliceState state{sliceId, entry.panKey, entry.frequencyHz, entry.dspMode, {},
                                      {}};
        QString reason;
        if (m_radioModel->restoreSliceFor(device, sliceId, state, &reason) < 0) {
            qCInfo(lcReceivers) << "A slice taken back did not fit:" << reason;
            if (!mac.isEmpty()) {
                DeviceLayoutStore::clearSliceSettings(store, mac, sliceId);
            }
            continue;
        }
        if (firstRestored < 0) {
            firstRestored = sliceId;
        }
        affected.append(ObjectRegistry::keyForSlice(sliceId));
    }
    m_radioModel->requestSettingsSave();
    m_confirm->forgetTakeBack(device, question.noticeId);
    tellTaken(closedBy, device,
              receiver ? QStringLiteral("receiverTaken") : QStringLiteral("sliceTaken"),
              receiver ? target : -1, receiver ? -1 : firstRestored);
    tellListeners(listenedBy, device,
                  receiver ? QStringLiteral("receiverTaken") : QStringLiteral("sliceTaken"));
    m_connectedDevices->refresh();
    return SessionMessages::commandResult(invoke.commandVerb, invoke.commandId, !affected.isEmpty(),
                                          affected.isEmpty()
                                              ? QString::fromLatin1(kChangedReason)
                                              : QString(),
                                          affected);
}

} // namespace NereusSDR
