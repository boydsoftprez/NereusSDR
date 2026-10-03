// =================================================================
// src/core/TxSliceArbiter.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port. See TxSliceArbiter.h for header
// notes and design reference.
//
// Modification history (NereusSDR):
//   2026-09-25 iPhone app plan Task 34 (R-IOS-03): the handoff while keyed
//              waits for the unkey gate. J.J. Boyd (KG4VCF), AI-assisted via
//              Anthropic Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  iPhone app plan Task 77 (R-IOS-02,
//                                    R-IOS-03, R-IOS-13): owner lookup,
//                                    requestHandoff for a requester,
//                                    bindForHolder, the first bind among the
//                                    holder's slices, the freeze. AI-assisted
//                                    via Anthropic Claude Code.
//   2026-09-28  J.J. Boyd / KG4VCF  Slice control plan Task 2: the owner
//                                    lookup is a transmit access check, so
//                                    a listened slice is never bound for
//                                    its listener. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-28  J.J. Boyd / KG4VCF  Slice control fix wave (Critical 1): a
//                                    move waiting for the unkey gate is
//                                    checked again before the flag lands,
//                                    and pendingHandoffChanged announces
//                                    the waiting target. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  Slice control fix wave, round 2: a
//                                    dropped waiting move says whether
//                                    transmit or the slice changed hands.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  Slice control plan Task 7:
//                                    releaseBinding for a Core left with no
//                                    slice. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  Slice control fix wave (whole-branch
//                                    review, Critical 1): releaseBinding
//                                    unkeys through the unkey gate before
//                                    the binding ends. AI-assisted via
//                                    Anthropic Claude Code.
// =================================================================
#include "core/TxSliceArbiter.h"
#include "models/SliceModel.h"
#include "core/MoxController.h"
#include "core/AppSettings.h"
#include "core/safety/UnkeyGate.h"

namespace NereusSDR {

TxSliceArbiter::TxSliceArbiter(QObject* parent) : QObject(parent) {}

void TxSliceArbiter::setMoxController(MoxController* mox) { m_mox = mox; }

// Pure wiring: deliberately does NOT sync. An initial bind carries an RF
// guard that needs the MoxController, so binding as a side effect of
// whichever setter happened to be called first would make RF safety depend
// on wiring order. Callers hand over the list, finish wiring, then call
// syncToSliceList() (RadioModel does it from addSlice / removeSlice).
void TxSliceArbiter::setSliceList(QVector<SliceModel*>* slices) { m_slices = slices; }

void TxSliceArbiter::setTransmitAccess(TransmitAccess mayTransmit, ActiveLookup active)
{
    m_mayTransmit = std::move(mayTransmit);
    m_active = std::move(active);
}

bool TxSliceArbiter::requestHandoff(int sliceId, const QByteArray& requester)
{
    // iPhone app plan Task 77 (ruling 8.10): the holder's verb, for its own
    // slices. A slice another owner has, or one the requester only listens
    // to (slice control plan Task 2), is refused before anything moves.
    if (m_mayTransmit && !m_mayTransmit(requester, sliceId)) {
        emit handoffBlocked(sliceId, QStringLiteral("That slice is another device's."));
        return false;
    }
    return requestHandoffFrom(sliceId, requester);
}

bool TxSliceArbiter::pendingMayLand(int sliceId, const QByteArray& requester) const
{
    // Slice control fix wave (Critical 1): control of the slice may have
    // passed while the move waited for the unkey. The flag lands only on a
    // slice the holder, and the device that asked, may still transmit on.
    if (!m_mayTransmit) {
        return true;
    }
    const QByteArray holder = m_holder ? m_holder() : QByteArray();
    if (!holder.isEmpty() && !m_mayTransmit(holder, sliceId)) {
        return false;
    }
    return requester.isEmpty() || m_mayTransmit(requester, sliceId);
}

bool TxSliceArbiter::bindForHolder(const QByteArray& holder, int preferredSliceId)
{
    if (m_remote || !m_slices || holder.isEmpty() || !m_mayTransmit || isFrozen()) {
        return false;
    }
    int target = -1;
    if (preferredSliceId >= 0 && sliceWithId(preferredSliceId) != nullptr
        && m_mayTransmit(holder, preferredSliceId)) {
        target = preferredSliceId;
    } else if (m_active) {
        const int active = m_active(holder);
        if (active >= 0 && sliceWithId(active) != nullptr && m_mayTransmit(holder, active)) {
            target = active;
        }
    }
    if (target < 0) {
        return false;
    }
    return requestHandoff(target);
}

SliceModel* TxSliceArbiter::txBoundSlice() const
{
    if (!m_slices) { return nullptr; }
    for (SliceModel* slice : *m_slices) {
        if (slice && slice->sliceIndex() == m_txBoundSliceId) {
            return slice;
        }
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// syncToSliceList: the initial binding, and the invariant's repair arm.
//
// The defect this closes: requestHandoff was the ONLY writer of
// SliceModel::txSlice, and it early-returns when the requested index already
// equals m_txBoundSliceId. A default binding therefore has to be established
// took that arm and never raised the flag. A single-slice session, and any
// session where the operator never explicitly moved TX, therefore ran with
// isTxSlice() false on every slice for the life of the process, silently
// disabling every consumer that asks which slice transmits.
//
// The flag on the SliceModel is the authority for WHICH slice transmits;
// m_txBoundSliceId is a cache of that slice's stable identity. Deriving the
// ID from the flag (rather than the other way round) is what makes
// this safe to call after a removal: the flagged OBJECT survives the list
// mutation, only its position moves.
// ---------------------------------------------------------------------------
void TxSliceArbiter::syncToSliceList()
{
    // Remote-daemon R2 Task 5: on a remote client which slice transmits
    // is the daemon's decision, mirrored in by a later task, so this
    // whole method -- the initial-bind arm's setTxSlice(true) at the
    // bottom, and the defensive-normalisation arm's setTxSlice(false)
    // calls -- is a no-op. Gated at the top rather than around each
    // individual write so m_txBoundSliceId and the flags it names stay in
    // agreement (txBoundSlice()'s documented invariant); guarding only
    // the writes and letting the ID bookkeeping run regardless would
    // leave m_txBoundSliceId naming a slice whose isTxSlice() reads
    // false. See design addendum docs/architecture/
    // 2026-08-03-remote-daemon-r2-r3-design-addendum.md section 4.1.
    if (m_remote) { return; }

    if (!m_slices || m_slices->isEmpty()) {
        // No slices, so no binding to hold. The restored ID is left alone: it
        // may be carrying a value load() restored, and the first slice to
        // arrive should honour it.
        return;
    }

    SliceModel* firstFlagged = nullptr;
    int flaggedCount = 0;
    for (SliceModel* s : *m_slices) {
        if (s && s->isTxSlice()) {
            if (!firstFlagged) { firstFlagged = s; }
            ++flaggedCount;
        }
    }

    if (flaggedCount == 1) {
        // The transmitter is where it was; silently adopt its stable ID.
        // Deliberately silent: txBoundSliceChanged means "the transmitter
        // moved to a different slice", and subscribers act on it (re-badge
        // every VFO flag, re-push the transmit frequency, toast the
        // operator). Announcing a re-index would tell them a handoff
        // happened that did not.
        m_txBoundSliceId = firstFlagged->sliceIndex();
        return;
    }

    if (flaggedCount > 1) {
        // Defensive: nothing outside this class writes the flag today, so
        // this is a guard against a future second writer rather than a live
        // path. Keep the slice the stable ID already names if it is flagged,
        // otherwise the first flagged slice, and clear the rest.
        SliceModel* keep = txBoundSlice();
        if (!keep || !keep->isTxSlice()) {
            keep = firstFlagged;
        }
        for (SliceModel* slice : *m_slices) {
            if (slice && slice != keep) {
                slice->setTxSlice(false);
            }
        }
        m_txBoundSliceId = keep->sliceIndex();
        return;
    }

    // ── Initial bind ────────────────────────────────────────────────────
    // Nothing is flagged and slices exist, so the transmitter has no home.
    // Give it one. Honour the current ID (which load() may have restored
    // from AppSettings) when it names a live slice; otherwise fall back to
    // slice A, per design §6 "Restore on launch": "If that slice doesn't
    // exist post-restore (e.g. operator deleted it last session), default to
    // Slice A."
    SliceModel* slice = txBoundSlice();
    // iPhone app plan Task 77 (ruling 8.13): with a holder that owns
    // slices, the first bind is among them: the restored id when it is
    // one of them, otherwise the holder's active slice.
    const QByteArray holder = m_holder ? m_holder() : QByteArray();
    if (!holder.isEmpty() && m_mayTransmit) {
        if (slice == nullptr || !m_mayTransmit(holder, slice->sliceIndex())) {
            const int active = m_active ? m_active(holder) : -1;
            if (SliceModel* own = sliceWithId(active); own && m_mayTransmit(holder, active)) {
                slice = own;
            }
        }
    }
    if (!slice) {
        for (SliceModel* candidate : *m_slices) {
            if (candidate) {
                slice = candidate;
                break;
            }
        }
    }
    if (!slice) { return; }

    // RF-safe, same guard requestHandoff uses. Unreachable on the true first
    // bind (nothing to key from before a slice exists); present for the
    // degenerate keyed-but-unbound case.
    if (m_mox && m_mox->isMox()) {
        m_mox->setMox(false);
    }

    slice->setTxSlice(true);
    m_txBoundSliceId = slice->sliceIndex();
    // oldId -1: there was no previous binding, so this is not a handoff.
    // Subscribers that treat it as one (status-bar "TX > Slice X" toast)
    // check for the sentinel.
    emit txBoundSliceChanged(-1, m_txBoundSliceId);
}

bool TxSliceArbiter::requestHandoff(int sliceId)
{
    return requestHandoffFrom(sliceId, QByteArray());
}

bool TxSliceArbiter::requestHandoffFrom(int sliceId, const QByteArray& requester)
{
    // Remote-daemon R2 Task 5: same reasoning as syncToSliceList() above.
    // A local operator TX-slice click funnels through here; on a remote
    // client that click is a later task's job to forward to the daemon
    // as a command instead, not something to resolve locally against a
    // slice list the daemon does not know this process changed.
    //
    // Deliberately does not emit handoffBlocked, unlike every other
    // `return false` path below (e.g. "Slice ID not found") -- this is
    // not a rejected request, it is a request this class does not yet
    // have anywhere to route. Emitting the existing signal would read as
    // "your target slice was invalid," which is not true here and would
    // misdirect whatever UI reacts to it.
    if (m_remote) { return false; }

    SliceModel* target = nullptr;
    if (m_slices) {
        for (SliceModel* slice : *m_slices) {
            if (slice && slice->sliceIndex() == sliceId) {
                target = slice;
                break;
            }
        }
    }
    if (!target) {
        emit handoffBlocked(sliceId, QStringLiteral("Slice ID not found"));
        return false;
    }

    // iPhone app plan Task 77 (ruling 8.11): the flag never moves while
    // the station device is keyed; the freeze ends with the press.
    if (!target->isTxSlice() && isFrozen()) {
        emit handoffBlocked(sliceId, QStringLiteral("The radio is on the air."));
        return false;
    }

    if (target->isTxSlice()) {
        m_txBoundSliceId = sliceId;
        setPending(-1, QByteArray());   // a waiting move to elsewhere is dropped
        return true;  // already TX-bound, no-op
    }

    // RF-safe handoff: drop MOX before changing TX-bound slice.
    //
    // iPhone app plan Task 34 (R-IOS-03; remote design section 12.2): with
    // the unkey gate the flag moves only once MOX reached receive (or the
    // gate timed out and stopped transmit at once), for a local handoff as
    // for a remote one. Moving it on the next statement put the new slice's
    // transmit frequency on the wire before MOX off, so the down-slew tail
    // could go out on the new frequency (Task 33's report, concern 2).
    const bool keyed = m_mox && (m_mox->isMox() || m_mox->state() != MoxState::Rx);
    if (keyed && m_unkeyGate) {
        const bool alreadyWaiting = m_pendingHandoffId >= 0;
        setPending(sliceId, requester);
        if (!alreadyWaiting) {
            m_unkeyGate->unkey(QStringLiteral("The transmit slice moved."), this,
                               [this](UnkeyOutcome) {
                const int pending = m_pendingHandoffId;
                const QByteArray requester = m_pendingRequester;
                const QByteArray askedHolder = m_pendingHolder;
                setPending(-1, QByteArray());
                SliceModel* next = sliceWithId(pending);
                if (!next || next->isTxSlice()) {
                    return;
                }
                // Slice control fix wave (Critical 1): checked again now,
                // so a slice another device took while the key ended never
                // carries this holder's transmit.
                if (!pendingMayLand(pending, requester)) {
                    // Fix wave, round 2: say what changed hands.
                    const QByteArray holderNow = m_holder ? m_holder() : QByteArray();
                    const bool transmitPassed = holderNow != askedHolder;
                    emit handoffBlocked(
                        pending, transmitPassed
                            ? QStringLiteral("Transmit passed to another device, so the "
                                             "transmit slice did not move.")
                            : QStringLiteral("Another device controls that slice now, "
                                             "so the transmit slice did not move."));
                    return;
                }
                flipTo(next);
            });
        }
        return true;
    }
    if (m_mox && m_mox->isMox()) {
        m_mox->setMox(false);
        // Without a gate the flag moves now; MOX is already off in the
        // controller, the hardware at the end of the TX-to-RX walk.
    }
    flipTo(target);
    return true;
}

void TxSliceArbiter::releaseBinding()
{
    if (m_remote) {
        return;
    }
    setPending(-1, QByteArray());
    const int old = m_txBoundSliceId;
    if (old < 0) {
        return;
    }
    // Slice control fix wave (whole-branch review, Critical 1): the
    // binding never ends under a key. Keyed, the radio unkeys through the
    // gate first, as a handoff does, and the binding ends once it is in
    // receive (or the gate stopped transmit at once).
    const bool keyed = m_mox && (m_mox->isMox() || m_mox->state() != MoxState::Rx);
    if (keyed && m_unkeyGate) {
        m_unkeyGate->unkey(QStringLiteral("The transmit slice closed."), this,
                           [this, old](UnkeyOutcome) {
            // A new binding or a slice on the same id meanwhile: nothing
            // to end.
            if (m_txBoundSliceId != old || sliceWithId(old) != nullptr) {
                return;
            }
            m_txBoundSliceId = -1;
            emit txBoundSliceChanged(old, -1);
        });
        return;
    }
    if (m_mox && m_mox->isMox()) {
        m_mox->setMox(false);
    }
    m_txBoundSliceId = -1;
    emit txBoundSliceChanged(old, -1);
}

void TxSliceArbiter::setPending(int sliceId, const QByteArray& requester)
{
    m_pendingRequester = requester;
    // Fix wave, round 2: who held transmit when the move was asked, so a
    // dropped move can say whether transmit or the slice changed hands.
    m_pendingHolder = sliceId >= 0 && m_holder ? m_holder() : QByteArray();
    if (m_pendingHandoffId == sliceId) {
        return;
    }
    m_pendingHandoffId = sliceId;
    emit pendingHandoffChanged(sliceId);
}

SliceModel* TxSliceArbiter::sliceWithId(int sliceId) const
{
    if (!m_slices) {
        return nullptr;
    }
    for (SliceModel* slice : *m_slices) {
        if (slice && slice->sliceIndex() == sliceId) {
            return slice;
        }
    }
    return nullptr;
}

void TxSliceArbiter::flipTo(SliceModel* target)
{
    const int sliceId = target->sliceIndex();
    int oldId = m_txBoundSliceId;
    if (!txBoundSlice()) {
        for (SliceModel* slice : *m_slices) {
            if (slice && slice->isTxSlice()) {
                oldId = slice->sliceIndex();
                break;
            }
        }
    }

    // The ID cache may have been restored before the live flags were
    // normalised, so clear every other slice rather than trusting one
    // positional predecessor.
    for (SliceModel* slice : *m_slices) {
        if (slice && slice != target) {
            slice->setTxSlice(false);
        }
    }
    target->setTxSlice(true);

    m_txBoundSliceId = sliceId;
    emit txBoundSliceChanged(oldId, sliceId);
}

void TxSliceArbiter::save()
{
    if (m_mac.isEmpty()) { return; }
    auto& s = AppSettings::instance();
    const QString key = QStringLiteral("hardware/%1/TxBoundSliceId").arg(m_mac);
    s.setValue(key, m_txBoundSliceId);
}

void TxSliceArbiter::load()
{
    if (m_mac.isEmpty()) { return; }
    auto& s = AppSettings::instance();
    const QString idKey = QStringLiteral("hardware/%1/TxBoundSliceId").arg(m_mac);
    const QString legacyKey =
        QStringLiteral("hardware/%1/TxBoundSliceIndex").arg(m_mac);

    int restoredId = -1;
    if (s.contains(idKey)) {
        restoredId = s.value(idKey, -1).toInt();
    } else if (s.contains(legacyKey) && m_slices) {
        const int legacyPosition = s.value(legacyKey, -1).toInt();
        if (legacyPosition >= 0 && legacyPosition < m_slices->size()) {
            if (SliceModel* legacySlice = m_slices->at(legacyPosition)) {
                restoredId = legacySlice->sliceIndex();
                s.setValue(idKey, restoredId);
            }
        }
    }

    if (restoredId >= 0) {
        bool liveId = false;
        if (m_slices) {
            for (SliceModel* slice : *m_slices) {
                if (slice && slice->sliceIndex() == restoredId) {
                    liveId = true;
                    break;
                }
            }
        }
        if (liveId) {
            requestHandoff(restoredId);
        } else {
            m_txBoundSliceId = restoredId;
        }
    }
    syncToSliceList();
}

} // namespace NereusSDR
