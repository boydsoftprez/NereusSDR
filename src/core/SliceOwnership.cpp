// no-port-check: NereusSDR-original.
// =================================================================
// src/core/SliceOwnership.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 73 (R-IOS-02): whose each slice is, and each owner's
// active slice. See SliceOwnership.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 73 (R-IOS-02), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-25: iPhone app plan Task 74 (R-IOS-02, R-IOS-30): each
//               receiver's anchor. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-28: slice control and shared listening plan Task 1: each
//               slice's incarnation and control revision. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-28: slice control and shared listening plan Task 2: each
//               slice's listener set. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-28: slice control and shared listening plan Task 3: joining
//               and leaving, claims removal and each device's active
//               receive slice. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-28: slice control and shared listening plan Task 4: a lone
//               device adopts only unclaimed slices (ruling Q9). J.J.
//               Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-29: slice control and shared listening plan Task 8: a slice
//               released to nobody passes its receiver as ruling 6.2
//               does, and away devices (setAwayDevices, isAwaySlice) for
//               Amendment 8a. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
// =================================================================

#include "core/SliceOwnership.h"

#include <QRandomGenerator>

#include <algorithm>
#include <iterator>
#include <utility>

namespace NereusSDR {

const QByteArray& SliceOwnership::stationDevice()
{
    static const QByteArray id = QByteArrayLiteral("station");
    return id;
}

SliceOwnership::SliceOwnership(QObject* parent)
    : SliceOwnership(QRandomGenerator::system()->bounded(quint32{1} << kNonceBits), parent)
{
}

SliceOwnership::SliceOwnership(quint32 bootNonce, QObject* parent)
    : QObject(parent)
    , m_incarnationBase(quint64{bootNonce & ((quint32{1} << kNonceBits) - 1)} << 32)
{
}

// ── Lifecycle ───────────────────────────────────────────────────────────

void SliceOwnership::noteSliceAdded(int sliceId)
{
    const ActiveRxWatch watch(this);
    m_removing.remove(sliceId);
    m_order.removeAll(sliceId);
    m_order.append(sliceId);
    Mark mark;
    mark.owner = m_creator;
    m_marks.insert(sliceId, mark);
    // Task 1 (slice control plan): a new incarnation, never 0 (the counter
    // skips 0 should it ever wrap), and control revision 1.
    if (++m_incarnationCounter == 0) {
        m_incarnationCounter = 1;
    }
    m_incarnations.insert(sliceId, m_incarnationBase | m_incarnationCounter);
    m_revisions.insert(sliceId, 1);
    // Task 2 (slice control plan): a new slice has no listeners yet; Task 3:
    // its controller, when it has one, is joined.
    m_listeners.remove(sliceId);
    if (!mark.owner.isEmpty()) {
        m_listeners.insert(sliceId, QList<QByteArray>{mark.owner});
    }
    // Task 74: bound before it was noted, it claimed its receiver for
    // nobody; it is the first there, so the receiver is its owner's.
    const auto stream = m_streamOf.constFind(sliceId);
    if (stream != m_streamOf.cend() && m_anchor.value(*stream).isEmpty()
        && m_joinOrder.value(*stream).value(0, -1) == sliceId) {
        m_anchor.insert(*stream, mark.subject());
    }
    emit activeChanged();
}

void SliceOwnership::beginRemove(int sliceId)
{
    if (!isLive(sliceId)) {
        return;
    }
    const ActiveRxWatch watch(this);
    const QByteArray owner = m_marks.value(sliceId).owner;
    m_removing.insert(sliceId);
    if (m_mostRecent == sliceId) {
        // The station-level slice was this one: its owner's next, if any.
        m_mostRecent = activeFor(owner);
    }
    emit activeChanged();
}

void SliceOwnership::endRemove(int sliceId)
{
    const ActiveRxWatch watch(this);
    leaveStream(sliceId);
    m_removing.remove(sliceId);
    m_order.removeAll(sliceId);
    m_marks.remove(sliceId);
    m_incarnations.remove(sliceId);
    m_revisions.remove(sliceId);
    m_listeners.remove(sliceId);
    // Task 3: a receive choice never carries over to the id made again.
    for (auto it = m_chosenRx.begin(); it != m_chosenRx.end();) {
        it = it.value() == sliceId ? m_chosenRx.erase(it) : std::next(it);
    }
    if (m_mostRecent == sliceId) {
        m_mostRecent = -1;
    }
}

bool SliceOwnership::removalMatches(int sliceId, quint64 incarnation) const
{
    return incarnation != 0 && m_removing.contains(sliceId)
        && m_incarnations.value(sliceId, 0) == incarnation;
}

bool SliceOwnership::cancelRemove(int sliceId, quint64 incarnation)
{
    if (!removalMatches(sliceId, incarnation)) { return false; }
    const ActiveRxWatch watch(this);
    m_removing.remove(sliceId);
    emit activeChanged();
    return true;
}

bool SliceOwnership::completeRemove(int sliceId, quint64 incarnation)
{
    if (!removalMatches(sliceId, incarnation)) { return false; }
    endRemove(sliceId);
    return true;
}

void SliceOwnership::setOrder(const QList<int>& sliceIds)
{
    QList<int> order;
    for (int id : sliceIds) {
        if (isLive(id) && !order.contains(id)) {
            order.append(id);
        }
    }
    for (int id : std::as_const(m_order)) {
        if (!order.contains(id)) {
            order.append(id);
        }
    }
    if (order != m_order) {
        const ActiveRxWatch watch(this);
        m_order = order;
        emit activeChanged();
    }
}

bool SliceOwnership::isLive(int sliceId) const
{
    return m_marks.contains(sliceId) && !m_removing.contains(sliceId);
}

QList<int> SliceOwnership::liveSlices() const
{
    return matching([](const Mark&) { return true; });
}

// ── Listeners (slice control plan Task 2) ───────────────────────────────

bool SliceOwnership::isListening(const QByteArray& device, int sliceId) const
{
    if (device.isEmpty() || !m_marks.contains(sliceId)) {
        return false;
    }
    return m_marks.value(sliceId).owner == device
        || m_listeners.value(sliceId).contains(device);
}

QList<QByteArray> SliceOwnership::listenersOf(int sliceId) const
{
    QList<QByteArray> listeners;
    if (!m_marks.contains(sliceId)) {
        return listeners;
    }
    const QByteArray controller = m_marks.value(sliceId).owner;
    if (!controller.isEmpty()) {
        listeners.append(controller);
    }
    for (const QByteArray& device : m_listeners.value(sliceId)) {
        if (!device.isEmpty() && !listeners.contains(device)) {
            listeners.append(device);
        }
    }
    return listeners;
}

// ── Membership and the active receive slice (slice control plan Task 3) ─

bool SliceOwnership::join(const QByteArray& device, int sliceId)
{
    if (device.isEmpty() || !isLive(sliceId)) {
        return false;
    }
    if (isListening(device, sliceId)) {
        return true;
    }
    const ActiveRxWatch watch(this);
    m_listeners[sliceId].append(device);
    emit listenersChanged(sliceId);
    return true;
}

bool SliceOwnership::leave(const QByteArray& device, int sliceId)
{
    if (device.isEmpty() || !isLive(sliceId) || m_marks.value(sliceId).owner == device) {
        return false;
    }
    const auto joined = m_listeners.find(sliceId);
    if (joined == m_listeners.end() || !joined->contains(device)) {
        return false;
    }
    const ActiveRxWatch watch(this);
    joined->removeAll(device);
    if (joined->isEmpty()) {
        m_listeners.erase(joined);
    }
    if (m_chosenRx.value(device, -1) == sliceId) {
        m_chosenRx.remove(device);
    }
    emit listenersChanged(sliceId);
    return true;
}

QList<int> SliceOwnership::joinedBy(const QByteArray& device) const
{
    QList<int> ids;
    if (device.isEmpty()) {
        return ids;
    }
    for (int id : m_order) {
        if (isLive(id) && isListening(device, id)) {
            ids.append(id);
        }
    }
    return ids;
}

int SliceOwnership::activeRxFor(const QByteArray& device) const
{
    if (device.isEmpty()) {
        return -1;
    }
    const auto chosen = m_chosenRx.constFind(device);
    if (chosen != m_chosenRx.cend() && isLive(*chosen) && isListening(device, *chosen)) {
        return *chosen;
    }
    const int controlled = activeFor(device);
    if (controlled >= 0) {
        return controlled;
    }
    const QList<int> joined = joinedBy(device);
    return joined.isEmpty() ? -1 : joined.first();
}

bool SliceOwnership::setActiveRx(const QByteArray& device, int sliceId)
{
    if (device.isEmpty() || !isLive(sliceId) || !isListening(device, sliceId)) {
        return false;
    }
    const ActiveRxWatch watch(this);
    m_chosenRx.insert(device, sliceId);
    return true;
}

SliceOwnership::ClaimsRemoved SliceOwnership::removeClaims(const QByteArray& device)
{
    ClaimsRemoved removed;
    if (device.isEmpty()) {
        return removed;
    }
    const ActiveRxWatch watch(this);
    const QList<int> order = m_order;
    for (int id : order) {
        if (!isLive(id)) {
            continue;
        }
        const Mark mark = m_marks.value(id);
        const bool controls = (mark.owner == device && !mark.isHeld()) || mark.heldFor == device;
        if (controls) {
            changeMark(id, Mark{}, device);
            removed.releasedControl.append(id);
            continue;
        }
        const auto joined = m_listeners.find(id);
        if (joined != m_listeners.end() && joined->contains(device)) {
            joined->removeAll(device);
            if (joined->isEmpty()) {
                m_listeners.erase(joined);
            }
            emit listenersChanged(id);
            removed.leftListening.append(id);
        }
    }
    // It controls none now, so its active slice was already none.
    m_chosenRx.remove(device);
    m_chosen.remove(device);
    return removed;
}

void SliceOwnership::setAwayDevices(const QSet<QByteArray>& devices)
{
    if (devices == m_away) {
        return;
    }
    m_away = devices;
    emit awayChanged();
}

bool SliceOwnership::isAwaySlice(int sliceId) const
{
    if (!isLive(sliceId)) {
        return false;
    }
    const Mark mark = m_marks.value(sliceId);
    return mark.isHeld() || (!mark.owner.isEmpty() && m_away.contains(mark.owner));
}

QList<int> SliceOwnership::unclaimed() const
{
    QList<int> ids;
    for (int id : m_order) {
        if (isLive(id) && m_marks.value(id).owner.isEmpty() && listenersOf(id).isEmpty()) {
            ids.append(id);
        }
    }
    return ids;
}

SliceOwnership::ActiveRxWatch::ActiveRxWatch(SliceOwnership* ownership)
    : m_ownership(ownership)
{
    if (m_ownership->m_rxWatchDepth++ == 0) {
        m_ownership->m_rxBefore = m_ownership->activeRxSnapshot();
    }
}

SliceOwnership::ActiveRxWatch::~ActiveRxWatch()
{
    if (!m_ownership || --m_ownership->m_rxWatchDepth != 0) {
        return;
    }
    const QHash<QByteArray, int> before = std::exchange(m_ownership->m_rxBefore, {});
    const QHash<QByteArray, int> after = m_ownership->activeRxSnapshot();
    QList<QByteArray> devices = before.keys();
    for (auto it = after.cbegin(); it != after.cend(); ++it) {
        if (!before.contains(it.key())) {
            devices.append(it.key());
        }
    }
    std::sort(devices.begin(), devices.end());
    for (const QByteArray& device : std::as_const(devices)) {
        if (before.value(device, -1) != after.value(device, -1)) {
            if (!m_ownership) { return; }
            emit m_ownership->activeRxChanged(device);
        }
    }
}

QHash<QByteArray, int> SliceOwnership::activeRxSnapshot() const
{
    // Every device that could have an active receive slice: owners, those
    // a slice is held for, joined devices and those with a choice.
    QSet<QByteArray> devices;
    for (auto it = m_marks.cbegin(); it != m_marks.cend(); ++it) {
        devices.insert(it->owner);
        devices.insert(it->heldFor);
    }
    for (auto it = m_listeners.cbegin(); it != m_listeners.cend(); ++it) {
        for (const QByteArray& device : it.value()) {
            devices.insert(device);
        }
    }
    for (auto it = m_chosenRx.cbegin(); it != m_chosenRx.cend(); ++it) {
        devices.insert(it.key());
    }
    for (auto it = m_chosen.cbegin(); it != m_chosen.cend(); ++it) {
        devices.insert(it.key());
    }
    QHash<QByteArray, int> snapshot;
    for (const QByteArray& device : std::as_const(devices)) {
        const int id = activeRxFor(device);
        if (id >= 0) {
            snapshot.insert(device, id);
        }
    }
    return snapshot;
}

// ── Incarnation and control revision (slice control plan Task 1) ────────

quint64 SliceOwnership::incarnation(int sliceId) const
{
    return isLive(sliceId) ? m_incarnations.value(sliceId, 0) : 0;
}

quint64 SliceOwnership::controlRevision(int sliceId) const
{
    return isLive(sliceId) ? m_revisions.value(sliceId, 0) : 0;
}

bool SliceOwnership::matches(const SliceRef& ref) const
{
    return ref.incarnation != 0 && incarnation(ref.sliceId) == ref.incarnation;
}

SliceOwnership::SliceRef SliceOwnership::refOf(int sliceId) const
{
    return SliceRef{sliceId, incarnation(sliceId)};
}

// ── Marks ───────────────────────────────────────────────────────────────

SliceOwnership::Mark SliceOwnership::mark(int sliceId) const
{
    return m_marks.value(sliceId);
}

void SliceOwnership::setMark(int sliceId, const Mark& requested)
{
    changeMark(sliceId, requested, QByteArray());
}

bool SliceOwnership::changeMark(int sliceId, const Mark& requested, const QByteArray& leaving)
{
    if (!isLive(sliceId)) {
        return false;
    }
    Mark next = requested;
    if (next.isHeld()) {
        next.owner = stationDevice();
    }
    const Mark before = m_marks.value(sliceId);
    if (before == next) {
        return false;
    }
    const ActiveRxWatch watch(this);
    const QList<QByteArray> listenersBefore = listenersOf(sliceId);
    m_marks.insert(sliceId, next);
    // Task 3 (slice control plan): the new controller joins; nobody leaves
    // on a change of owner (the former controller stays a listener). The
    // station device running a slice held for another device does not join
    // by the hold. Only a claims removal names a device that leaves here.
    QList<QByteArray>& joined = m_listeners[sliceId];
    if (!leaving.isEmpty()) {
        joined.removeAll(leaving);
        if (m_chosenRx.value(leaving, -1) == sliceId) {
            m_chosenRx.remove(leaving);
        }
    }
    if (!next.owner.isEmpty() && !next.isHeld() && !joined.contains(next.owner)) {
        joined.append(next.owner);
    }
    if (joined.isEmpty()) {
        m_listeners.remove(sliceId);
    }
    // Task 74: the anchor goes with the slice to its new owner when it was
    // the old owner's only slice on the receiver.
    const auto stream = m_streamOf.constFind(sliceId);
    if (stream != m_streamOf.cend() && before.subject() != next.subject()
        && m_anchor.value(*stream) == before.subject()) {
        bool otherOfOld = false;
        for (int other : m_joinOrder.value(*stream)) {
            if (other != sliceId && subjectOf(other) == before.subject()) {
                otherOfOld = true;
                break;
            }
        }
        if (!otherOfOld) {
            // Slice control plan Task 8: released to nobody, the receiver
            // passes as ruling 6.2 passes it when a slice leaves, to the
            // device whose slice has been there longest.
            QByteArray nextAnchor = next.subject();
            if (nextAnchor.isEmpty()) {
                for (int other : m_joinOrder.value(*stream)) {
                    if (other != sliceId && !subjectOf(other).isEmpty()) {
                        nextAnchor = subjectOf(other);
                        break;
                    }
                }
            }
            m_anchor.insert(*stream, nextAnchor);
        }
    }
    // Task 1 (slice control plan): each change of owner is one control
    // revision; a change of heldFor alone is not.
    quint64 revision = 0;
    if (before.owner != next.owner) {
        revision = ++m_revisions[sliceId];
    }
    // Adoption is observable; its owner/model may retire in any notification.
    const QPointer<SliceOwnership> self(this);
    emit markChanged(sliceId, before.owner, before.heldFor);
    if (!self) { return true; }
    if (revision != 0) {
        emit controlRevisionChanged(sliceId, revision);
        if (!self) { return true; }
    }
    if (listenersOf(sliceId) != listenersBefore) {
        emit listenersChanged(sliceId);
        if (!self) { return true; }
    }
    emit activeChanged();
    return true;
}

void SliceOwnership::setOwner(int sliceId, const QByteArray& owner)
{
    setMark(sliceId, Mark{owner, QByteArray()});
}

void SliceOwnership::hold(int sliceId, const QByteArray& device)
{
    setMark(sliceId, Mark{stationDevice(), device});
}

QList<int> SliceOwnership::matching(const std::function<bool(const Mark&)>& test) const
{
    QList<int> ids;
    for (int id : m_order) {
        if (isLive(id) && test(m_marks.value(id))) {
            ids.append(id);
        }
    }
    return ids;
}

QList<int> SliceOwnership::ownedBy(const QByteArray& owner) const
{
    return matching([&owner](const Mark& mark) { return mark.owner == owner; });
}

QList<int> SliceOwnership::heldFor(const QByteArray& device) const
{
    if (device.isEmpty()) {
        return {};
    }
    return matching([&device](const Mark& mark) { return mark.heldFor == device; });
}

QList<int> SliceOwnership::unowned() const
{
    return matching([](const Mark& mark) { return mark.owner.isEmpty(); });
}

QList<int> SliceOwnership::returnHeld(const QByteArray& device)
{
    const ActiveRxWatch watch(this);
    const QList<int> ids = heldFor(device);
    for (int id : ids) {
        setOwner(id, device);
    }
    return ids;
}

QList<int> SliceOwnership::adoptUnowned(const QByteArray& device,
                                       const std::function<bool()>& continueAdoption)
{
    if (device.isEmpty()) {
        return {};
    }
    const QPointer<SliceOwnership> self(this);
    const ActiveRxWatch watch(this);
    // Slice control plan Task 4 (ruling Q9): never a slice others still
    // listen to after its controller released it; control of that one
    // comes only from Take control.
    const QList<int> ids = unclaimed();
    const int wasActive = activeFor(QByteArray());
    QList<int> adopted;
    for (int id : ids) {
        // A rightful bootstrap/host can retire during the preceding mark.
        // The optional predicate adds lifetime continuity to the established policy.
        if (continueAdoption) {
            const bool allowed = continueAdoption();
            if (!self) { return {}; }
            if (!allowed) { break; }
        }
        // Earlier notifications may explicitly claim/hold/remove this
        // remaining slice. Q9 applies to its CURRENT mark for every caller.
        if (!isLive(id) || !mark(id).owner.isEmpty() || !listenersOf(id).isEmpty()) { continue; }
        setOwner(id, device);
        if (!self) { return {}; }
        adopted.append(id);
    }
    if (continueAdoption) {
        const bool allowed = continueAdoption();
        if (!self) { return {}; }
        if (!allowed) { return adopted; }
    }
    if (adopted.contains(wasActive) && mark(wasActive).owner == device
        && !mark(wasActive).isHeld() && !m_chosen.contains(device)) {
        m_chosen.insert(device, wasActive);
        emit activeChanged();
    }
    return adopted;
}

// ── The active slice ────────────────────────────────────────────────────

int SliceOwnership::activeFor(const QByteArray& owner) const
{
    const auto chosen = m_chosen.constFind(owner);
    if (chosen != m_chosen.cend() && isLive(*chosen) && m_marks.value(*chosen).owner == owner) {
        return *chosen;
    }
    const QList<int> own = ownedBy(owner);
    return own.isEmpty() ? -1 : own.first();
}

bool SliceOwnership::isActive(int sliceId) const
{
    return isLive(sliceId) && activeFor(m_marks.value(sliceId).owner) == sliceId;
}

void SliceOwnership::setActive(const QByteArray& owner, int sliceId)
{
    if (!isLive(sliceId) || m_marks.value(sliceId).owner != owner) {
        return;
    }
    const ActiveRxWatch watch(this);
    m_chosen.insert(owner, sliceId);
    // Task 3 (slice control plan): a controller's choice is also its
    // receive choice.
    if (!owner.isEmpty()) {
        m_chosenRx.insert(owner, sliceId);
    }
    m_mostRecent = sliceId;
    emit activeChanged();
}

int SliceOwnership::stationActiveSlice() const
{
    if (!m_transmitHolder.isEmpty()) {
        const int holders = activeFor(m_transmitHolder);
        if (holders >= 0) {
            return holders;
        }
    }
    return isLive(m_mostRecent) ? m_mostRecent : -1;
}

void SliceOwnership::setTransmitHolder(const QByteArray& holder)
{
    if (m_transmitHolder == holder) {
        return;
    }
    m_transmitHolder = holder;
    emit activeChanged();
}

// ── Anchors (Task 74, rulings 6.2 and 6.3) ──────────────────────────────

void SliceOwnership::leaveStream(int sliceId)
{
    const auto found = m_streamOf.constFind(sliceId);
    if (found == m_streamOf.cend()) {
        return;
    }
    const int stream = *found;
    m_streamOf.remove(sliceId);
    QList<int>& order = m_joinOrder[stream];
    order.removeAll(sliceId);
    if (order.isEmpty()) {
        // The last slice left: the receiver is free.
        m_joinOrder.remove(stream);
        m_anchor.remove(stream);
        return;
    }
    const QByteArray anchor = m_anchor.value(stream);
    for (int other : std::as_const(order)) {
        if (subjectOf(other) == anchor) {
            return;  // the anchor still has a slice here
        }
    }
    // Ruling 6.2: the device whose slice has been here longest. Nobody is
    // asked or told; nothing on anyone's band moves.
    m_anchor.insert(stream, subjectOf(order.first()));
}

void SliceOwnership::noteStream(int sliceId, int stream)
{
    if (m_streamOf.value(sliceId, -1) == stream) {
        return;
    }
    leaveStream(sliceId);
    if (stream < 0) {
        return;
    }
    QList<int>& order = m_joinOrder[stream];
    if (order.isEmpty()) {
        // Ruling 6.2: this slice claimed the receiver (NewStream).
        m_anchor.insert(stream, subjectOf(sliceId));
    }
    order.append(sliceId);
    m_streamOf.insert(sliceId, stream);
}

QByteArray SliceOwnership::anchorOf(int stream) const
{
    return m_anchor.value(stream);
}

QList<int> SliceOwnership::slicesOnStreamInJoinOrder(int stream) const
{
    return m_joinOrder.value(stream);
}

// ── Creator scope ───────────────────────────────────────────────────────

SliceOwnership::CreatorScope::CreatorScope(SliceOwnership* ownership, const QByteArray& owner)
    : m_ownership(ownership)
{
    if (m_ownership != nullptr) {
        m_previous = m_ownership->m_creator;
        m_ownership->m_creator = owner;
    }
}

SliceOwnership::CreatorScope::~CreatorScope()
{
    if (m_ownership != nullptr) {
        m_ownership->m_creator = m_previous;
    }
}

} // namespace NereusSDR
