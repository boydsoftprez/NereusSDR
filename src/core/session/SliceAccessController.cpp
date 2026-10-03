// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/SliceAccessController.cpp  (NereusSDR)
// =================================================================
//
// Slice control and shared listening plan Task 4: listen in, stop
// listening, take control and release, checked and applied in one place.
// See SliceAccessController.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-28: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), slice control and shared listening plan Task 4,
//               with AI-assisted implementation via Anthropic Claude Code.
//   2026-09-28: fix wave for the Tasks 1-4 review by J.J. Boyd (KG4VCF):
//               a take clears the transmit selection for a slice with no
//               controller too, and a release of the Core's last slice is
//               refused while it transmits. AI-assisted via Anthropic
//               Claude Code.
//   2026-09-29: slice control plan Task 7 by J.J. Boyd (KG4VCF): the
//               release of the Core's last slice with nobody else on it
//               closes it. AI-assisted via Anthropic Claude Code.
//   2026-09-29: slice control plan Task 6 by J.J. Boyd (KG4VCF): each
//               listener's own level and mute, seeded from the slice's AF
//               (ruling Q4), and the station device's local listening.
//               AI-assisted via Anthropic Claude Code.
//   2026-09-29: slice control fix wave (whole-branch review, Critical 1)
//               by J.J. Boyd (KG4VCF): a release is refused while the
//               slice transmits before any close; the comments name the
//               last slice's close (Task 7). AI-assisted via Anthropic
//               Claude Code.
//   2026-09-30: core-slice take-over: the hand-off check is given the
//               taker. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
//   2026-09-30: JJ's wider ruling: a former controller that cannot stay
//               listening leaves the slice it lost. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-09-30: TX badge take: takeWhileTransmittingWords gives a window
//               the on-air refusal's words. J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
// =================================================================

#include "core/session/SliceAccessController.h"

#include "core/session/ObjectRegistry.h"
#include "core/session/SliceAccessPolicy.h"
#include "core/session/SliceAccessSet.h"
#include "core/AudioEngine.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <algorithm>
#include <cmath>

namespace NereusSDR {

namespace {

// The words a device is refused with. Plain operator words (link section
// 18.3's style); the letter is the only thing inserted.
QString closedReason()
{
    return QStringLiteral("That slice has closed. Choose it again from the list.");
}

QString changedReason(const QString& letter)
{
    return QStringLiteral("Someone else changed who controls slice %1. Look again and try "
                          "once more.")
        .arg(letter);
}

QString takeWhileTransmittingReason(const QString& letter)
{
    return QStringLiteral("Slice %1 is transmitting. Take control once it stops.").arg(letter);
}

QString releaseWhileTransmittingReason(const QString& letter)
{
    return QStringLiteral("Slice %1 is transmitting. Release it once it stops.").arg(letter);
}

QString controllerStopsReason(const QString& letter)
{
    return QStringLiteral("You control slice %1. Use Release to leave it.").arg(letter);
}

QString notControllerReason(const QString& letter)
{
    return QStringLiteral("Only the device that controls slice %1 can release it.").arg(letter);
}

QString notListeningReason(const QString& letter)
{
    return QStringLiteral("You are not listening to slice %1. Listen in first.").arg(letter);
}

QString goneReason()
{
    return QStringLiteral("That receiver is no longer on the Core.");
}

QString notReadyReason()
{
    return QStringLiteral("The Core has no radio ready.");
}

} // namespace

SliceAccessController::SliceAccessController(RadioModel* radio, Hooks hooks, QObject* parent)
    : QObject(parent)
    , m_radio(radio)
    , m_hooks(std::move(hooks))
{
    // Slice control plan Task 6: listeners' levels follow who is on each
    // slice and who controls it.
    SliceOwnership* own = ownership();
    if (own != nullptr) {
        connect(own, &SliceOwnership::listenersChanged, this, [this](int sliceId) {
            reconcileListenLevels(sliceId);
            publishLocalListen(sliceId);
        });
        connect(own, &SliceOwnership::markChanged, this,
                [this](int sliceId, const QByteArray& oldOwner, const QByteArray&) {
                    reconcileListenLevels(sliceId);
                    reseedFormerController(sliceId, oldOwner);
                    publishLocalListen(sliceId);
                });
    }
    if (m_radio) {
        connect(m_radio.data(), &RadioModel::sliceRemoved, this, [this](int sliceId) {
            reconcileListenLevels(sliceId);
            publishLocalListen(sliceId);
        });
    }
}

QString SliceAccessController::letterOf(int sliceId)
{
    return QString(QChar(QLatin1Char('A').unicode() + sliceId));
}

QString SliceAccessController::takeWhileTransmittingWords(int sliceId)
{
    return takeWhileTransmittingReason(letterOf(sliceId));
}

SliceOwnership* SliceAccessController::ownership() const
{
    return m_radio ? m_radio->sliceOwnership() : nullptr;
}

SliceAccessController::Result SliceAccessController::refused(const QString& reason)
{
    Result result;
    result.reason = reason;
    return result;
}

QList<QByteArray> SliceAccessController::keysOf(int sliceId)
{
    return {ObjectRegistry::keyForSlice(sliceId), SliceAccessSet::keyFor(sliceId)};
}

bool SliceAccessController::closeIsRelease(const QByteArray& device, int sliceId) const
{
    const SliceOwnership* own = ownership();
    if (own == nullptr || device.isEmpty() || !own->isLive(sliceId)
        || !SliceAccessPolicy::mayChange(*own, device, sliceId)) {
        return false;
    }
    for (const QByteArray& other : own->listenersOf(sliceId)) {
        if (other != device) {
            return true;
        }
    }
    return false;
}

void SliceAccessController::closeIfNobodyIsOn(int sliceId)
{
    SliceOwnership* own = ownership();
    if (own == nullptr || !own->isLive(sliceId) || !own->mark(sliceId).owner.isEmpty()
        || !own->listenersOf(sliceId).isEmpty()) {
        return;
    }
    // Slice control plan Task 7: whatever the count, the Core's last slice
    // included.
    if (m_hooks.close) {
        m_hooks.close(sliceId);
    }
}

SliceAccessController::Result SliceAccessController::listen(const QByteArray& device,
                                                            SliceOwnership::SliceRef ref)
{
    SliceOwnership* own = ownership();
    if (own == nullptr) {
        return refused(notReadyReason());
    }
    if (device.isEmpty() || !own->matches(ref)) {
        return refused(closedReason());
    }
    // Nothing is allocated: joining changes only who hears and sees it.
    // Already joined is the same answer with no change.
    if (!own->join(device, ref.sliceId)) {
        return refused(closedReason());
    }
    Result result;
    result.accepted = true;
    result.controlRevision = own->controlRevision(ref.sliceId);
    result.affected = keysOf(ref.sliceId);
    return result;
}

SliceAccessController::Result SliceAccessController::stopListening(const QByteArray& device,
                                                                   SliceOwnership::SliceRef ref)
{
    SliceOwnership* own = ownership();
    if (own == nullptr) {
        return refused(notReadyReason());
    }
    if (device.isEmpty() || !own->matches(ref)) {
        return refused(closedReason());
    }
    const int sliceId = ref.sliceId;
    // Ruling Q5: the controller releases instead.
    if (SliceAccessPolicy::mayChange(*own, device, sliceId)) {
        return refused(controllerStopsReason(letterOf(sliceId)));
    }
    Result result;
    result.accepted = true;
    result.affected = keysOf(sliceId);
    if (!own->isListening(device, sliceId)) {
        // Not joined (left already): nothing to leave.
        return result;
    }
    // The device's receive choice moves to its next joined slice, in
    // creation order after this one (wrapping), when this was it.
    int next = -1;
    if (own->activeRxFor(device) == sliceId) {
        const QList<int> joined = own->joinedBy(device);
        const qsizetype at = joined.indexOf(sliceId);
        if (at >= 0 && joined.size() > 1) {
            next = joined.at((at + 1) % joined.size());
        }
    }
    own->leave(device, sliceId);
    if (next >= 0) {
        own->setActiveRx(device, next);
    }
    closeIfNobodyIsOn(sliceId);
    return result;
}

SliceAccessController::Result SliceAccessController::takeControl(const QByteArray& device,
                                                                 SliceOwnership::SliceRef ref,
                                                                 quint64 expectedRevision)
{
    SliceOwnership* own = ownership();
    if (own == nullptr) {
        return refused(notReadyReason());
    }
    if (device.isEmpty() || !own->matches(ref)) {
        return refused(closedReason());
    }
    const int sliceId = ref.sliceId;
    const QString letter = letterOf(sliceId);
    if (own->controlRevision(sliceId) != expectedRevision) {
        return refused(changedReason(letter));
    }
    const QByteArray former = own->mark(sliceId).owner;
    if (former == device) {
        Result result;
        result.accepted = true;
        result.controlRevision = own->controlRevision(sliceId);
        result.affected = keysOf(sliceId);
        return result;
    }
    // Checked at the change itself: a key that started since the device
    // looked stops the take.
    if (m_hooks.transmitting && m_hooks.transmitting(sliceId)) {
        return refused(takeWhileTransmittingReason(letter));
    }
    // Ruling Q7: the former controller must stay on as a listener.
    if (!former.isEmpty() && m_hooks.cannotHandOff) {
        const QString refusal = m_hooks.cannotHandOff(former, device, sliceId);
        if (!refusal.isEmpty()) {
            return refused(refusal);
        }
    }
    // Ruling Q8: the former controller's transmit selection of the slice is
    // cleared first, so no moment has the new controller's slice flagged
    // for the old one's transmit. Slice control fix wave: for a slice with
    // no controller too, since whoever holds transmit with the flag parked
    // on it loses that selection (the hook's third-holder case).
    if (m_hooks.clearTransmitSelection) {
        m_hooks.clearTransmitSelection(former, sliceId);
    }
    // One mark change: the taker joins, the former controller stays joined
    // (SliceOwnership), nothing is removed or made.
    own->setOwner(sliceId, device);
    // JJ's wider ruling (2026-09-30): a former controller that cannot stay
    // on as a listener loses the slice.
    if (!former.isEmpty() && m_hooks.staysListening && !m_hooks.staysListening(former)) {
        own->leave(former, sliceId);
    }
    if (m_hooks.tookControl) {
        m_hooks.tookControl(device, sliceId);
    }
    Result result;
    result.accepted = true;
    result.controlRevision = own->controlRevision(sliceId);
    result.affected = keysOf(sliceId);
    if (!former.isEmpty()) {
        emit controlTaken(sliceId, former, device);
    }
    return result;
}

SliceAccessController::Result SliceAccessController::release(const QByteArray& device,
                                                             SliceOwnership::SliceRef ref,
                                                             quint64 expectedRevision)
{
    SliceOwnership* own = ownership();
    if (own == nullptr) {
        return refused(notReadyReason());
    }
    if (device.isEmpty() || !own->matches(ref)) {
        return refused(closedReason());
    }
    const int sliceId = ref.sliceId;
    const QString letter = letterOf(sliceId);
    if (own->controlRevision(sliceId) != expectedRevision) {
        return refused(changedReason(letter));
    }
    if (!SliceAccessPolicy::mayChange(*own, device, sliceId)) {
        return refused(notControllerReason(letter));
    }
    // Slice control fix wave (whole-branch review, Critical 1): a release
    // never closes or hands off a slice on the air. Checked before the
    // close hook, which would otherwise close the transmitting slice.
    if (m_hooks.transmitting && m_hooks.transmitting(sliceId)) {
        return refused(releaseWhileTransmittingReason(letter));
    }
    Result result;
    result.accepted = true;
    result.affected = keysOf(sliceId);
    if (!closeIsRelease(device, sliceId)) {
        // Nobody else is on it: it closes, by the close path every slice
        // takes (the transmit flag moves as ruling 8.12 says).
        if (m_hooks.close && m_hooks.close(sliceId)) {
            return result;
        }
        // The Core's last slice, which a close of a controlled slice leaves
        // alone: a hand-off to nobody as below (not while it transmits,
        // refused above). The close above changed nothing.
        if (m_hooks.clearTransmitSelection) {
            m_hooks.clearTransmitSelection(device, sliceId);
        }
        own->setOwner(sliceId, QByteArray());
        own->leave(device, sliceId);
        // Slice control plan Task 7: nobody is on it now, so it closes and
        // the Core has no slice.
        closeIfNobodyIsOn(sliceId);
        return result;
    }
    // Kept for its listeners: a hand-off to nobody (not while it
    // transmits, refused above).
    if (m_hooks.clearTransmitSelection) {
        m_hooks.clearTransmitSelection(device, sliceId);
    }
    own->setOwner(sliceId, QByteArray());
    own->leave(device, sliceId);
    result.controlRevision = own->controlRevision(sliceId);
    return result;
}

double SliceAccessController::afLevelOf(int sliceId) const
{
    const SliceModel* slice = m_radio ? m_radio->sliceById(sliceId) : nullptr;
    if (slice == nullptr) {
        return 1.0;
    }
    return std::clamp(slice->afGain(), 0, 100) / 100.0;
}

void SliceAccessController::reconcileListenLevels(int sliceId)
{
    const SliceOwnership* own = ownership();
    const bool live = own != nullptr && own->isLive(sliceId);
    const quint64 incarnation = live ? own->incarnation(sliceId) : 0;
    QHash<QByteArray, StoredLevel>& levels = m_listenLevels[sliceId];
    QList<QByteArray> changed;
    // A slice made again under the same id starts over; a device that left
    // is dropped.
    for (auto it = levels.begin(); it != levels.end();) {
        if (!live || it.value().incarnation != incarnation
            || !own->isListening(it.key(), sliceId)) {
            changed.append(it.key());
            it = levels.erase(it);
        } else {
            ++it;
        }
    }
    if (live) {
        // Ruling Q4: a new listener starts at the slice's AF level.
        const QByteArray controller = own->mark(sliceId).owner;
        for (const QByteArray& device : own->listenersOf(sliceId)) {
            if (device == controller || levels.contains(device)) {
                continue;
            }
            StoredLevel stored;
            stored.incarnation = incarnation;
            stored.value.level = afLevelOf(sliceId);
            levels.insert(device, stored);
            changed.append(device);
        }
    }
    if (levels.isEmpty()) {
        m_listenLevels.remove(sliceId);
    }
    for (const QByteArray& device : changed) {
        emit listenLevelChanged(sliceId, device);
    }
}

void SliceAccessController::reseedFormerController(int sliceId, const QByteArray& former)
{
    const SliceOwnership* own = ownership();
    if (own == nullptr || former.isEmpty() || !own->isLive(sliceId)
        || SliceAccessPolicy::mayChange(*own, former, sliceId)
        || !own->isListening(former, sliceId)) {
        return;
    }
    // Ruling Q4: the former controller keeps hearing the slice at the AF
    // level it had, so the hand-off neither jumps nor goes quiet.
    StoredLevel stored;
    stored.incarnation = own->incarnation(sliceId);
    stored.value.level = afLevelOf(sliceId);
    m_listenLevels[sliceId].insert(former, stored);
    emit listenLevelChanged(sliceId, former);
}

void SliceAccessController::publishLocalListen(int sliceId)
{
    AudioEngine* engine = m_radio ? m_radio->audioEngine() : nullptr;
    if (engine == nullptr || sliceId < 0 || sliceId >= AudioEngine::kMaxSliceAudioViews) {
        return;
    }
    // The Core's own output plays the station device's slices already
    // (RadioModel's local output mask); a slice another device controls
    // that the station device listens to plays at the station's level.
    const SliceOwnership* own = ownership();
    const QByteArray& station = SliceOwnership::stationDevice();
    const QByteArray controller =
        own != nullptr && own->isLive(sliceId) ? own->mark(sliceId).owner : QByteArray();
    if (own != nullptr && own->isLive(sliceId) && !controller.isEmpty() && controller != station
        && own->isListening(station, sliceId)) {
        const ListenLevel level = listenLevel(station, sliceId);
        engine->setLocalListen(sliceId, static_cast<float>(level.level), level.muted);
    } else {
        engine->clearLocalListen(sliceId);
    }
}

SliceAccessController::ListenLevel SliceAccessController::listenLevel(const QByteArray& device,
                                                                      int sliceId) const
{
    const auto slice = m_listenLevels.constFind(sliceId);
    if (slice != m_listenLevels.constEnd()) {
        const auto found = slice->constFind(device);
        if (found != slice->constEnd()) {
            return found->value;
        }
    }
    ListenLevel fallback;
    fallback.level = afLevelOf(sliceId);
    return fallback;
}

SliceAccessController::Result SliceAccessController::setListenLevel(
    const QByteArray& device, SliceOwnership::SliceRef ref, double level, bool muted)
{
    SliceOwnership* own = ownership();
    if (own == nullptr) {
        return refused(notReadyReason());
    }
    if (device.isEmpty() || !own->matches(ref)) {
        return refused(closedReason());
    }
    const int sliceId = ref.sliceId;
    if (!own->isListening(device, sliceId)) {
        return refused(notListeningReason(letterOf(sliceId)));
    }
    StoredLevel stored;
    stored.incarnation = ref.incarnation;
    stored.value.level = std::isfinite(level) ? std::clamp(level, 0.0, 1.0) : 0.0;
    stored.value.muted = muted;
    m_listenLevels[sliceId].insert(device, stored);
    emit listenLevelChanged(sliceId, device);
    if (device == SliceOwnership::stationDevice()) {
        publishLocalListen(sliceId);
    }
    // Nothing mirrored changes: the level lives in the device's own mix.
    Result result;
    result.accepted = true;
    return result;
}

SliceAccessController::Result SliceAccessController::selectRx(const QByteArray& device,
                                                              int sliceId)
{
    SliceOwnership* own = ownership();
    if (own == nullptr || m_radio.isNull()) {
        return refused(notReadyReason());
    }
    if (m_radio->sliceById(sliceId) == nullptr || !own->isLive(sliceId)) {
        return refused(goneReason());
    }
    if (!own->isListening(device, sliceId)) {
        return refused(notListeningReason(letterOf(sliceId)));
    }
    if (!m_radio->setActiveRxFor(device, sliceId)) {
        return refused(goneReason());
    }
    Result result;
    result.accepted = true;
    result.affected = {ObjectRegistry::keyForSlice(sliceId)};
    return result;
}

} // namespace NereusSDR
