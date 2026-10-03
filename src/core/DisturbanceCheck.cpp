// no-port-check: NereusSDR-original.
// =================================================================
// src/core/DisturbanceCheck.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 75 (R-IOS-30): which other devices a change to a
// radio-wide setting disturbs. See DisturbanceCheck.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 75 (R-IOS-30), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-26: Transmit group fix wave 2: every holder on the air counts,
//               the station device's own keys included (onAirHolder),
//               exempt by change not by holder; ruling 8.11's freeze on
//               every path (XIT, pan moves, a stored change at proceed); a
//               hosting desktop's key named after it. J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include "core/DisturbanceCheck.h"

namespace NereusSDR {

bool DisturbanceCheck::reaches(const Scope& scope, const SliceInfo& slice)
{
    if (scope.radio) {
        return true;
    }
    if (slice.stream >= 0 && scope.receivers.contains(slice.stream)) {
        return true;
    }
    if (slice.stream >= 0 && slice.adc >= 0 && scope.adcs.contains(slice.adc)) {
        return true;
    }
    // A passband holds a notch when the two overlap, edges included: the
    // same test NotchModel::notchesInBandwidth applies to a slice's filter.
    const double low = slice.frequencyHz + slice.filterLowHz;
    const double high = slice.frequencyHz + slice.filterHighHz;
    for (const Range& range : scope.ranges) {
        if (range.highHz >= low && range.lowHz <= high) {
            return true;
        }
    }
    return false;
}

QList<DisturbanceCheck::Affected> DisturbanceCheck::check(const Scope& scope,
                                                          const Topology& topology,
                                                          const QByteArray& requester)
{
    QList<Affected> affected;
    const auto entryFor = [&affected](const QByteArray& device) -> Affected& {
        for (Affected& a : affected) {
            if (a.device == device) {
                return a;
            }
        }
        affected.append(Affected{device, false, {}});
        return affected.last();
    };
    for (const SliceInfo& slice : topology.slices) {
        // The requester's own slices never count (ruling 7.3), and a slice
        // nobody owns has nobody to ask.
        if (slice.device.isEmpty() || slice.device == requester) {
            continue;
        }
        const auto planned = scope.planned.constFind(slice.sliceId);
        if (planned != scope.planned.cend()) {
            entryFor(slice.device).slices.append(AffectedSlice{slice.sliceId, *planned});
        } else if (reaches(scope, slice)) {
            entryFor(slice.device).slices.append(AffectedSlice{slice.sliceId, scope.effect});
        }
    }
    const QByteArray& holder = topology.transmit.holder;
    if (!holder.isEmpty()) {
        const bool touched = scope.transmitter && holder != requester;
        bool listed = false;
        for (Affected& a : affected) {
            if (a.device == holder) {
                a.holdsTransmit = true;
                listed = true;
            }
        }
        if (touched && !listed && topology.transmit.holderAskable) {
            // Ruling 7.8: counted only because the transmitter is touched.
            affected.append(Affected{holder, true, {}});
        }
    }
    return affected;
}

bool DisturbanceCheck::refusedOnAir(const Scope& scope, const Topology& topology,
                                    const QByteArray& requester,
                                    const QList<Affected>& affected)
{
    const Transmit& tx = topology.transmit;
    // A change the holder makes itself is not refused by this rule.
    if (tx.holder.isEmpty() || !tx.keyed || tx.holder == requester) {
        return false;
    }
    if (scope.transmitPath || scope.stopsDataFlow) {
        return true;
    }
    // Fix wave 2, Important 3: the transmit slice is judged whoever owns
    // it. While the radio's own PTT (or the Core's own key) transmits on
    // another device's slice, the holder owns none of its slices, and the
    // requester's own frozen slice counts too (ruling 8.11).
    Q_UNUSED(affected);
    for (const SliceInfo& slice : topology.slices) {
        const bool txSlice = slice.sliceId >= 0 && slice.sliceId == tx.txSliceId;
        const bool frozen = slice.sliceId >= 0 && slice.sliceId == tx.frozenSliceId;
        if (!txSlice && !frozen) {
            continue;
        }
        Effect effect = scope.effect;
        const auto planned = scope.planned.constFind(slice.sliceId);
        if (planned != scope.planned.cend()) {
            // A rate change's plan reaches it: the frozen slice may not be
            // touched at all.
            if (frozen) {
                return true;
            }
            effect = *planned;
        } else if (!reaches(scope, slice)) {
            continue;
        }
        if (effect == Effect::Moves || effect == Effect::Closes) {
            return true;
        }
    }
    return false;
}

QString DisturbanceCheck::effectName(Effect effect)
{
    switch (effect) {
    case Effect::Changes:
        return QStringLiteral("changes");
    case Effect::Moves:
        return QStringLiteral("moves");
    case Effect::Closes:
        return QStringLiteral("closes");
    case Effect::PausesWhileTransmitting:
        return QStringLiteral("pausesWhileTransmitting");
    }
    return QStringLiteral("changes");
}

} // namespace NereusSDR
