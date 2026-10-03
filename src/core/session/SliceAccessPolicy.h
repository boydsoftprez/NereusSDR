#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/SliceAccessPolicy.h  (NereusSDR)
// =================================================================
//
// Who may see, hear and change each slice (slice control and shared
// listening plan Task 2; design docs/architecture/2026-09-28-slice-
// control-and-listening-design.md, "Keep three predicates separate").
//
// A slice has at most one controller (SliceOwnership's mark().owner) and
// any number of listeners, the controller always among them
// (SliceOwnership::listenersOf). Three questions are kept apart, so that a
// listener is never granted a write or transmit by accident:
//
//   - see:    the slice object, its pan and display; controller or listener.
//   - hear:   its audio; controller or listener.
//   - change: every write, verb, settings key, raw I/Q and transmit choice
//             on it; the controller only.
//
// The Core's own operating position (the station TCI server, the hosting
// desktop) may also change a slice nobody controls and nobody listens to:
// the slices a Core makes before any device arrives. A released slice that
// still has listeners is changeable by nobody until one takes control.
//
// Every access decision about a slice calls one of these by name; no call
// site compares mark().owner for access (tst_slice_access_policy scans the
// sources for that). Owner comparisons that decide anchors, saving and
// notices are not access decisions and stay where they are.
//
// Pure, no state. Single thread: RadioModel's, as SliceOwnership is.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-28: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), slice control and shared listening plan Task 2,
//               with AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include <QByteArray>

namespace NereusSDR {

class SliceOwnership;

class SliceAccessPolicy {
public:
    /// `device` may see the slice: its controller or a listener.
    static bool maySee(const SliceOwnership& ownership, const QByteArray& device, int sliceId);
    /// `device` may hear the slice: its controller or a listener.
    static bool mayHear(const SliceOwnership& ownership, const QByteArray& device, int sliceId);
    /// `device` may change the slice: its controller only (for a held slice
    /// the station device, which runs it).
    static bool mayChange(const SliceOwnership& ownership, const QByteArray& device, int sliceId);
    /// The Core's own operating position may change a slice with no
    /// controller and no listeners (a slice made before any device came).
    static bool stationMayChangeUnclaimed(const SliceOwnership& ownership, int sliceId);
    /// `device` may carry transmit on the slice: whose slice it is (its
    /// controller, or the device a held slice is held for), never a
    /// listener. The transmit flag, the TX marks and the holder's fallback
    /// on a close follow this.
    static bool mayTransmitOn(const SliceOwnership& ownership, const QByteArray& device,
                              int sliceId);
};

} // namespace NereusSDR
