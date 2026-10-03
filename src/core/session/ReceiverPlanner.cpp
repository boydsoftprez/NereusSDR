// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/ReceiverPlanner.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 74 (R-IOS-02, R-IOS-30): what a pan move or a take
// would do to the other devices' slices. See ReceiverPlanner.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 74 (R-IOS-02, R-IOS-30),
//               with AI-assisted implementation via Anthropic Claude Code.
//   2026-09-26: Task 77 fix wave, M1: the choosers' txSlice is the TX mark
//               (ruling 5.4a), as on slice: and marker:. J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude Code.
//   2026-09-29: slice control plan Task 8, Amendment 8a: a slice of a
//               device that is not here is neither named as disturbed nor
//               offered to close. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-29: slice control plan Task 9: each disturbed slice's
//               listeners, named to a device that shares slices. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include "core/session/ReceiverPlanner.h"

#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>

#include <cmath>

#include "core/SliceOwnership.h"
#include "core/SliceStreamAllocator.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

namespace NereusSDR {

ReceiverPlanner::ReceiverPlanner(const RadioModel& model, Describe describe)
    : m_model(model)
    , m_describe(std::move(describe))
{
}

QByteArray ReceiverPlanner::subjectOf(int sliceId) const
{
    return m_model.sliceOwnership()->mark(sliceId).subject();
}

bool ReceiverPlanner::windowCovers(int stream, double centreHz, double frequencyHz) const
{
    const int rateHz = m_model.streamAllocator().streamSampleRateHz(stream);
    if (rateHz <= 0) {
        return false;
    }
    // Strict on both sides, as SliceStreamAllocator::windowContains.
    const double halfWindow = static_cast<double>(rateHz) / 2.0;
    const double offset = frequencyHz - centreHz;
    return offset > -halfWindow && offset < halfWindow;
}

// ── A window moving (rulings 6.4 and 6.5) ────────────────────────────────

ReceiverPlanner::WindowMove ReceiverPlanner::planWindowMove(int stream, double centreHz,
                                                           const QByteArray& requester,
                                                           int exemptSliceId) const
{
    WindowMove plan;
    const SliceStreamAllocator& live = m_model.streamAllocator();
    if (!live.isStreamActive(stream) || live.streamSampleRateHz(stream) <= 0) {
        return plan;
    }
    plan.valid = true;
    // The same steps moveStreamWindowFor takes, on a copy: the window
    // moves, then each slice it no longer covers is placed again in turn.
    SliceStreamAllocator copy = live;
    copy.activateStream(stream, centreHz, live.streamSampleRateHz(stream));
    for (const int id : m_model.slicesOnStream(stream)) {
        const SliceModel* slice = m_model.sliceById(id);
        if (slice == nullptr || id == exemptSliceId
            || windowCovers(stream, centreHz, slice->frequency())) {
            continue;
        }
        const QByteArray subject = subjectOf(id);
        if (!requester.isEmpty() && subject == requester) {
            plan.ownOutside.append(id);
            continue;
        }
        // Slice control plan Task 8, Amendment 8a: a slice of a device that
        // is not here is not named; the move reaches it as it reaches any.
        if (m_model.sliceOwnership()->isAwaySlice(id)) {
            continue;
        }
        Disturbed d;
        d.sliceId = id;
        d.device = subject;
        d.listeners = m_model.sliceOwnership()->listenersOf(id);
        const SliceStreamAllocator::Placement placement = copy.placeSlice(slice->frequency());
        if (placement.outcome == SliceStreamAllocator::Outcome::NewStream) {
            copy.activateStream(placement.streamIndex, placement.newStreamCentreHz,
                                live.streamSampleRateHz(stream));
        }
        d.effect = placement.outcome == SliceStreamAllocator::Outcome::Rejected ? Effect::Closes
                                                                                 : Effect::Moves;
        plan.disturbed.append(d);
    }
    return plan;
}

// ── Taking a receiver or a slice (section 6.4, rulings 6.7 to 6.9) ──────

QList<ReceiverPlanner::Choice> ReceiverPlanner::receiverChoices(const TakeRequest& request) const
{
    QList<Choice> choices;
    const SliceStreamAllocator& live = m_model.streamAllocator();
    const QSet<int> moving(request.moving.cbegin(), request.moving.cend());
    for (int stream = 0; stream < live.streamCount(); ++stream) {
        if (!live.isStreamActive(stream)) {
            continue;
        }
        Choice c;
        c.choice = static_cast<int>(choices.size());
        c.stream = stream;
        QList<int> ownOutside;
        bool ownThere = false;
        for (const int id : m_model.slicesOnStream(stream)) {
            const SliceModel* slice = m_model.sliceById(id);
            if (slice == nullptr) {
                continue;
            }
            if (subjectOf(id) != request.requester) {
                // Ruling 6.7: every other device's slice closes, even one
                // the new window would still cover.
                c.closes.append(id);
                continue;
            }
            ownThere = true;
            if (moving.contains(id)) {
                continue;
            }
            if (request.need == Need::Retune || request.need == Need::PanMove) {
                // Design ruling 6.5a: the requester's own slice the new
                // window would not cover would close; the Core never closes
                // it by surprise.
                if (!windowCovers(stream, request.centreHz, slice->frequency())) {
                    ownOutside.append(id);
                }
            }
        }
        if (!ownOutside.isEmpty()) {
            c.takeable = false;
            QStringList letters;
            for (int id : ownOutside) {
                letters.append(letterOf(id));
            }
            c.why = letters.size() == 1
                        ? QStringLiteral("Your slice %1 would close.").arg(letters.first())
                        : QStringLiteral("Your slices %1 would close.").arg(joinWords(letters));
        } else if (request.need == Need::AddPan && ownThere) {
            // A new panadapter wants a receiver of its own; one the
            // requester's own panadapter uses cannot give it that.
            c.takeable = false;
            c.why = QStringLiteral("Your panadapter already uses this receiver.");
        } else if (c.closes.isEmpty()) {
            // Only the requester's own slices: nobody to take it from, and
            // the change itself was refused for this receiver already.
            c.takeable = request.need == Need::Retune || request.need == Need::PanMove;
            if (!c.takeable) {
                c.why = QStringLiteral("Your panadapter already uses this receiver.");
            }
        }
        // Ruling 6.8 (D64): a receiver carrying the transmit slice of a
        // holder on the air is never takeable (Task 34's holder, joined at
        // the merge of the trunk into the transmit lane).
        if (m_onAir.sliceId >= 0 && m_onAir.holder != request.requester
            && m_model.slicesOnStream(stream).contains(m_onAir.sliceId)) {
            c.takeable = false;
            c.why = m_onAir.why;
        }
        choices.append(c);
    }
    return choices;
}

QList<ReceiverPlanner::Choice> ReceiverPlanner::sliceChoices(const QByteArray& requester) const
{
    QList<Choice> choices;
    for (const int id : m_model.sliceOwnership()->liveSlices()) {
        const QByteArray subject = subjectOf(id);
        if (subject.isEmpty() || subject == requester || m_model.sliceById(id) == nullptr) {
            continue;
        }
        // Slice control plan Task 8, Amendment 8a: a slice of a device that
        // is not here is not offered to close.
        if (m_model.sliceOwnership()->isAwaySlice(id)) {
            continue;
        }
        Choice c;
        c.choice = static_cast<int>(choices.size());
        c.sliceId = id;
        c.stream = m_model.sliceById(id)->streamIndex();
        c.closes = {id};
        choices.append(c);
    }
    return choices;
}

ReceiverPlanner::AddPlacement ReceiverPlanner::planAddAfterClosing(
    const QByteArray& requester, const QString& panId, const QList<int>& closes) const
{
    AddPlacement plan;
    const QSet<int> removed(closes.cbegin(), closes.cend());
    const QList<SliceModel*> liveSlices = m_model.slices();
    const int survivors = liveSlices.size() - removed.size();
    plan.mayClose = removed.isEmpty() || survivors > 0;
    plan.sliceSpace = survivors < m_model.sliceCapForDevices();
    if (!plan.mayClose) {
        plan.reason = QStringLiteral("The Core must keep its last receiver slice.");
        return plan;
    }
    SliceStreamAllocator copy = m_model.streamAllocator();
    for (int stream = 0; stream < copy.streamCount(); ++stream) {
        bool occupied = false;
        for (int id : m_model.slicesOnStream(stream)) {
            occupied = occupied || !removed.contains(id);
        }
        if (!occupied) {
            copy.deactivateStream(stream);
        }
    }

    bool ownPan = false;
    const SliceOwnership* ownership = m_model.sliceOwnership();
    const int ownActive = ownership->activeFor(requester);
    const SliceModel* seed = ownActive >= 0 && !removed.contains(ownActive)
        ? m_model.sliceById(ownActive) : nullptr;
    if (seed == nullptr) {
        const int stationActive = ownership->stationActiveSlice();
        if (stationActive >= 0 && !removed.contains(stationActive)) {
            seed = m_model.sliceById(stationActive);
        } else if (stationActive >= 0) {
            // beginRemove selects that owner's next active slice before
            // RadioModel::applyActiveSlices chooses the post-close seed.
            const QByteArray previousOwner = ownership->mark(stationActive).owner;
            for (const SliceModel* slice : liveSlices) {
                if (slice != nullptr && !removed.contains(slice->sliceIndex())
                    && ownership->mark(slice->sliceIndex()).owner == previousOwner) {
                    seed = slice;
                    break;
                }
            }
        }
        if (seed == nullptr && m_model.activeSlice() != nullptr
            && !removed.contains(m_model.activeSlice()->sliceIndex())) {
            seed = m_model.activeSlice();
        }
    }
    for (const SliceModel* slice : liveSlices) {
        if (slice == nullptr || removed.contains(slice->sliceIndex())) {
            continue;
        }
        if (seed == nullptr) {
            seed = slice;
        }
        if (!panId.isEmpty() && slice->panKey() == panId
            && ownership->mark(slice->sliceIndex()).owner == requester) {
            ownPan = true;
        }
    }
    // The unsized local pool intentionally leaves a new slice unbound until
    // connectToRadio configures it, just as addSliceImpl does.
    if (copy.streamCount() <= 0) {
        plan.receiverFits = true;
        return plan;
    }
    const double frequency = seed != nullptr ? seed->frequency() : 14225000.0;
    const auto placement = copy.placeSlice(frequency, !panId.isEmpty() && !ownPan);
    plan.receiverFits = placement.outcome != SliceStreamAllocator::Outcome::Rejected;
    plan.reason = !plan.sliceSpace ? QStringLiteral("All slice places are in use.")
                                   : placement.reason;
    return plan;
}

bool ReceiverPlanner::panMoveFitsAfterClosing(int stream, double centreHz,
                                               const QList<int>& moving,
                                               const QList<int>& closes) const
{
    const SliceStreamAllocator& live = m_model.streamAllocator();
    if (moving.isEmpty() || !live.isStreamActive(stream) || !std::isfinite(centreHz)
        || centreHz <= 0.0) {
        return false;
    }
    const QSet<int> removed(closes.cbegin(), closes.cend());
    bool staysActive = false;
    for (int id : m_model.slicesOnStream(stream)) {
        staysActive = staysActive || !removed.contains(id);
    }
    const int rateHz = staysActive ? live.streamSampleRateHz(stream)
                                   : m_model.newStreamSampleRateHz();
    if (rateHz <= 0) {
        return false;
    }
    const double halfWindow = static_cast<double>(rateHz) / 2.0;
    for (int id : moving) {
        const SliceModel* slice = m_model.sliceById(id);
        if (slice == nullptr || removed.contains(id)) {
            return false;
        }
        const double offset = slice->frequency() - centreHz;
        if (!(offset > -halfWindow && offset < halfWindow)) {
            return false;
        }
    }
    return true;
}

ReceiverPlanner::RestorePlacement ReceiverPlanner::planRestoreAfterClosing(
    const QList<double>& frequencies, const QList<int>& closes) const
{
    RestorePlacement plan;
    if (frequencies.isEmpty()) {
        plan.reason = QStringLiteral("There are no saved slices to restore.");
        return plan;
    }
    const QSet<int> removed(closes.cbegin(), closes.cend());
    for (int id : removed) {
        if (m_model.sliceById(id) == nullptr) {
            plan.reason = QStringLiteral("That receiver changed since you asked.");
            return plan;
        }
    }
    const int survivors = m_model.slices().size() - removed.size();
    if (survivors <= 0 || survivors + frequencies.size() > m_model.sliceCapForDevices()) {
        plan.reason = survivors <= 0
            ? QStringLiteral("The Core must keep its last receiver slice.")
            : QStringLiteral("All slice places are in use.");
        return plan;
    }
    SliceStreamAllocator copy = m_model.streamAllocator();
    for (int stream = 0; stream < copy.streamCount(); ++stream) {
        bool occupied = false;
        for (int id : m_model.slicesOnStream(stream)) {
            occupied = occupied || !removed.contains(id);
        }
        if (!occupied) {
            copy.deactivateStream(stream);
        }
    }
    if (copy.streamCount() <= 0) {
        plan.fits = true;
        return plan;
    }
    const int newRateHz = m_model.newStreamSampleRateHz();
    if (newRateHz <= 0) {
        plan.reason = QStringLiteral("The receiver has no usable sample rate.");
        return plan;
    }
    for (double frequency : frequencies) {
        if (!std::isfinite(frequency) || frequency <= 0.0) {
            plan.reason = QStringLiteral("A saved slice has no usable frequency.");
            return plan;
        }
        const auto placement = copy.placeSlice(frequency);
        if (placement.outcome == SliceStreamAllocator::Outcome::Rejected) {
            plan.reason = placement.reason;
            return plan;
        }
        if (placement.outcome == SliceStreamAllocator::Outcome::NewStream) {
            copy.activateStream(placement.streamIndex, placement.newStreamCentreHz, newRateHz);
        }
    }
    plan.fits = true;
    return plan;
}

bool ReceiverPlanner::anotherDeviceHoldsAReceiver(const QByteArray& requester) const
{
    for (const SliceModel* slice : m_model.slices()) {
        if (slice != nullptr && slice->streamIndex() >= 0
            && subjectOf(slice->sliceIndex()) != requester
            && !subjectOf(slice->sliceIndex()).isEmpty()) {
            return true;
        }
    }
    return false;
}

QStringList ReceiverPlanner::namesHoldingReceivers(const QByteArray& requester) const
{
    QStringList names;
    for (const int id : m_model.sliceOwnership()->liveSlices()) {
        const SliceModel* slice = m_model.sliceById(id);
        const QByteArray subject = subjectOf(id);
        if (slice == nullptr || slice->streamIndex() < 0 || subject.isEmpty()
            || subject == requester) {
            continue;
        }
        const DeviceInfo info = m_describe(subject);
        if (info.known && !names.contains(info.name)) {
            names.append(info.name);
        }
    }
    return names;
}

// ── JSON ─────────────────────────────────────────────────────────────────

// Slice control plan Task 9: `listeners` as wire ids, in order, skipping a
// device the Core cannot name.
QJsonArray ReceiverPlanner::listenerIdsJson(const QList<QByteArray>& listeners) const
{
    QJsonArray ids;
    for (const QByteArray& device : listeners) {
        const DeviceInfo info = m_describe(device);
        if (info.known && !info.wireId.isEmpty()) {
            ids.append(info.wireId);
        }
    }
    return ids;
}

QJsonArray ReceiverPlanner::affectedJson(const QList<Disturbed>& disturbed) const
{
    QList<QByteArray> order;
    QHash<QByteArray, QJsonArray> slicesOf;
    for (const Disturbed& d : disturbed) {
        const SliceModel* slice = m_model.sliceById(d.sliceId);
        if (slice == nullptr) {
            continue;
        }
        if (!order.contains(d.device)) {
            order.append(d.device);
        }
        QJsonObject entry{
            {QStringLiteral("sliceId"), d.sliceId},
            {QStringLiteral("letter"), letterOf(d.sliceId)},
            {QStringLiteral("frequencyHz"), slice->frequency()},
            {QStringLiteral("band"), static_cast<int>(slice->band())},
            {QStringLiteral("mode"), static_cast<int>(slice->dspMode())},
            {QStringLiteral("adc"), m_model.adcForStream(slice->streamIndex())},
            {QStringLiteral("streamIndex"), slice->streamIndex()},
            {QStringLiteral("effect"), effectName(d.effect)},
        };
        if (m_showListeners) {
            entry.insert(QStringLiteral("listenerDeviceIds"), listenerIdsJson(d.listeners));
        }
        slicesOf[d.device].append(entry);
    }
    QJsonArray affected;
    for (const QByteArray& device : order) {
        const DeviceInfo info = m_describe(device);
        affected.append(QJsonObject{
            {QStringLiteral("deviceId"), info.wireId},
            {QStringLiteral("deviceName"), info.name},
            {QStringLiteral("deviceShortName"), info.shortName},
            {QStringLiteral("state"), info.state},
            // Transmit has no holder until Task 34.
            {QStringLiteral("holdsTransmit"), false},
            {QStringLiteral("slices"), slicesOf.value(device)},
        });
    }
    return affected;
}

QJsonArray ReceiverPlanner::receiverChoicesJson(const QList<Choice>& choices) const
{
    QJsonArray out;
    const SliceStreamAllocator& live = m_model.streamAllocator();
    for (const Choice& c : choices) {
        QJsonArray slices;
        QJsonArray devices;
        QList<QByteArray> seen;
        const QList<int> on = c.stream >= 0 && live.isStreamActive(c.stream)
                                  ? m_model.slicesOnStream(c.stream)
                                  : QList<int>{};
        for (const int id : on) {
            const SliceModel* slice = m_model.sliceById(id);
            if (slice == nullptr) {
                continue;
            }
            const QByteArray subject = subjectOf(id);
            const DeviceInfo info = m_describe(subject);
            QJsonObject entry{
                {QStringLiteral("sliceId"), id},
                {QStringLiteral("letter"), letterOf(id)},
                {QStringLiteral("deviceId"), info.wireId},
                {QStringLiteral("deviceName"), info.name},
                {QStringLiteral("frequencyHz"), slice->frequency()},
                {QStringLiteral("mode"), static_cast<int>(slice->dspMode())},
                {QStringLiteral("band"), static_cast<int>(slice->band())},
                {QStringLiteral("txSlice"), slice->txSliceMarked()},
            };
            if (m_showListeners) {
                entry.insert(QStringLiteral("listenerDeviceIds"),
                             listenerIdsJson(m_model.sliceOwnership()->listenersOf(id)));
            }
            slices.append(entry);
            if (info.known && !seen.contains(subject)) {
                seen.append(subject);
                devices.append(QJsonObject{
                    {QStringLiteral("deviceId"), info.wireId},
                    {QStringLiteral("name"), info.name},
                    {QStringLiteral("shortName"), info.shortName},
                    {QStringLiteral("state"), info.state},
                    {QStringLiteral("lastActivitySeconds"), info.lastActivitySeconds},
                });
            }
        }
        const bool active = c.stream >= 0 && live.isStreamActive(c.stream);
        const DeviceInfo anchor = active
            ? m_describe(m_model.sliceOwnership()->anchorOf(c.stream))
            : DeviceInfo{};
        out.append(QJsonObject{
            {QStringLiteral("choice"), c.choice},
            {QStringLiteral("streamIndex"), c.stream},
            {QStringLiteral("adc"), active ? m_model.adcForStream(c.stream) : 0},
            {QStringLiteral("centreHz"), active ? live.streamCentreHz(c.stream) : 0.0},
            {QStringLiteral("rateHz"), active ? live.streamSampleRateHz(c.stream) : 0},
            {QStringLiteral("anchorName"), anchor.name},
            {QStringLiteral("slices"), slices},
            {QStringLiteral("devices"), devices},
            {QStringLiteral("takeable"), c.takeable},
            {QStringLiteral("why"), c.why},
        });
    }
    return out;
}

QJsonArray ReceiverPlanner::sliceChoicesJson(const QList<Choice>& choices) const
{
    QJsonArray out;
    for (const Choice& c : choices) {
        const SliceModel* slice = m_model.sliceById(c.sliceId);
        if (slice == nullptr) {
            continue;
        }
        const DeviceInfo info = m_describe(subjectOf(c.sliceId));
        QJsonObject entry{
            {QStringLiteral("choice"), c.choice},
            {QStringLiteral("sliceId"), c.sliceId},
            {QStringLiteral("letter"), letterOf(c.sliceId)},
            {QStringLiteral("deviceId"), info.wireId},
            {QStringLiteral("deviceName"), info.name},
            {QStringLiteral("deviceShortName"), info.shortName},
            {QStringLiteral("state"), info.state},
            {QStringLiteral("frequencyHz"), slice->frequency()},
            {QStringLiteral("mode"), static_cast<int>(slice->dspMode())},
            {QStringLiteral("band"), static_cast<int>(slice->band())},
            {QStringLiteral("txSlice"), slice->txSliceMarked()},
            {QStringLiteral("streamIndex"), slice->streamIndex()},
            {QStringLiteral("adc"), m_model.adcForStream(slice->streamIndex())},
            {QStringLiteral("takeable"), c.takeable},
            {QStringLiteral("why"), c.why},
        };
        if (m_showListeners) {
            entry.insert(QStringLiteral("listenerDeviceIds"),
                         listenerIdsJson(m_model.sliceOwnership()->listenersOf(c.sliceId)));
        }
        out.append(entry);
    }
    return out;
}

QJsonArray ReceiverPlanner::noticeSlicesJson(const QList<int>& sliceIds) const
{
    QJsonArray out;
    for (const int id : sliceIds) {
        const SliceModel* slice = m_model.sliceById(id);
        if (slice == nullptr) {
            continue;
        }
        out.append(QJsonObject{
            {QStringLiteral("sliceId"), id},
            {QStringLiteral("letter"), letterOf(id)},
            {QStringLiteral("frequencyHz"), slice->frequency()},
            {QStringLiteral("mode"), static_cast<int>(slice->dspMode())},
            {QStringLiteral("band"), static_cast<int>(slice->band())},
        });
    }
    return out;
}

// ── Words ────────────────────────────────────────────────────────────────

QString ReceiverPlanner::bandWords(Band band)
{
    // "40m" -> "40 m" (the several-devices design, ruling 6.5: "40 m",
    // "20 m"); GEN, WWV and XVTR keep their own labels.
    static const QRegularExpression metres(QStringLiteral("^(\\d+)m$"));
    const QString label = bandLabel(band);
    const QRegularExpressionMatch match = metres.match(label);
    return match.hasMatch() ? match.captured(1) + QStringLiteral(" m") : label;
}

QString ReceiverPlanner::joinWords(const QStringList& words)
{
    if (words.isEmpty()) {
        return {};
    }
    if (words.size() == 1) {
        return words.first();
    }
    return QStringList(words.mid(0, words.size() - 1)).join(QStringLiteral(", "))
        + QStringLiteral(" and ") + words.last();
}

QString ReceiverPlanner::letterOf(int sliceId)
{
    return QString(QChar(QLatin1Char('A').unicode() + sliceId));
}

QString ReceiverPlanner::effectName(Effect effect)
{
    return effect == Effect::Moves ? QStringLiteral("moves") : QStringLiteral("closes");
}

} // namespace NereusSDR
