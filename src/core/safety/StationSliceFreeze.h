#pragma once
// no-port-check: NereusSDR-original shared station-key slice freeze predicate.
// 2026-10-02 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include "core/safety/TxRefusal.h"

namespace NereusSDR::StationSliceFreeze {
// Facts come from the actual keyed holder, or standalone MOX/keyer.
// Only its bound transmit slice is frozen; unrelated slices stay usable.
inline int frozenSlice(bool stationKeyed, int txBoundSlice)
{
    return stationKeyed ? txBoundSlice : -1;
}
inline TxRefusal refusal(int sliceId, int frozenSliceId, const TxRefusal& onAirWords)
{
    return sliceId >= 0 && sliceId == frozenSliceId ? onAirWords : TxRefusal{};
}
} // namespace NereusSDR::StationSliceFreeze
