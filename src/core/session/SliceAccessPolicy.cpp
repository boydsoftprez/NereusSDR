// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/SliceAccessPolicy.cpp  (NereusSDR)
// =================================================================
//
// Slice control and shared listening plan Task 2: who may see, hear and
// change each slice. See SliceAccessPolicy.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-28: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), slice control and shared listening plan Task 2,
//               with AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include "core/session/SliceAccessPolicy.h"

#include "core/SliceOwnership.h"

namespace NereusSDR {

bool SliceAccessPolicy::maySee(const SliceOwnership& ownership, const QByteArray& device,
                               int sliceId)
{
    return ownership.isListening(device, sliceId);
}

bool SliceAccessPolicy::mayHear(const SliceOwnership& ownership, const QByteArray& device,
                                int sliceId)
{
    return ownership.isListening(device, sliceId);
}

bool SliceAccessPolicy::mayChange(const SliceOwnership& ownership, const QByteArray& device,
                                  int sliceId)
{
    // mark() is kept while a slice is being removed, so its removal still
    // reaches the device that controlled it.
    return !device.isEmpty() && ownership.mark(sliceId).owner == device;
}

bool SliceAccessPolicy::stationMayChangeUnclaimed(const SliceOwnership& ownership, int sliceId)
{
    // A slice that is not there has neither; what a write to it does is
    // the writer's own concern, as before.
    return ownership.mark(sliceId).owner.isEmpty() && ownership.listenersOf(sliceId).isEmpty();
}

bool SliceAccessPolicy::mayTransmitOn(const SliceOwnership& ownership, const QByteArray& device,
                                      int sliceId)
{
    // Whose slice it is on the wire (iPhone app plan Task 77, ruling 8.13):
    // a held slice counts for the absent device it is held for. A listener
    // is never its subject.
    return !device.isEmpty() && ownership.mark(sliceId).subject() == device;
}

} // namespace NereusSDR
