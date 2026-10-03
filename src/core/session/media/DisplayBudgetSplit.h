// =================================================================
// src/core/session/media/DisplayBudgetSplit.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. How the Core's one display budget is
// shared among the devices signed in to it (iPhone app plan Task 76; the
// several-devices design, ruling 9.3 and design ruling 9.3a; R-IOS-31,
// R-MC-17). A pure function of its inputs: no Qt objects, no clock.
//
// Modification history (NereusSDR):
//   2026-09-25 - Original implementation for NereusSDR by J.J. Boyd
//                (KG4VCF), with AI-assisted implementation via Anthropic
//                Claude Code (R-IOS-31, R-MC-17).
//   2026-09-25 - Fix wave 2: every device asks for at least one useful
//                pan (minimumRequest). J.J. Boyd (KG4VCF), with
//                AI-assisted implementation via Anthropic Claude Code.
//
// =================================================================

#pragma once

#include "core/session/media/DisplayBudget.h"

#include <QByteArray>
#include <QList>

#include <optional>

namespace NereusSDR {

/// One admitted network device that shares the display budget. A hosting
/// desktop's own window draws locally and never appears here.
struct DisplayBudgetSplitDevice {
    /// Stable identity for the caller (the Core uses the media session's
    /// epoch as a string). Order in the input list is the tie-break order.
    QByteArray id;
    /// What the device asks for: its demand, the charges of its displays
    /// as subscribed (before the budget clamps them, a display refused for
    /// the budget included; fix wave 3: at the pixels each window can
    /// carry), not what it was granted (fix wave I5, ruling 9.3). The Core
    /// reads it from the device's media controller.
    /// messagesPerSecond is not part of a budget and is ignored.
    DisplayBudgetCharge request;
    /// Limits and reason last published to this device, if any, so an
    /// unchanged share keeps its generation and a changed one moves on.
    std::optional<DisplayBudgetLimits> previous;
    std::optional<DisplayBudgetReason> previousReason;
};

/// Who holds transmit, as the split needs it (ruling 9.3 items 1 and 3).
/// Until TransmitHolder exists (Task 34) the Core always passes Unheld.
enum class DisplayBudgetHolderKind : quint8 {
    Unheld = 0,
    /// The station device (a hosting desktop's window, or the Core itself):
    /// it sends no display over the network.
    Station = 1,
    /// A network device, named by holderId.
    Device = 2,
};

struct DisplayBudgetSplitInput {
    /// The Core's total: the configured display allowance, the computed
    /// ceiling, or the governor's cut of either.
    DisplayBudgetLimits total;
    /// The governor's cut is in force (the total's own reason is CoreBusy).
    bool governorCut = false;
    QList<DisplayBudgetSplitDevice> devices;
    DisplayBudgetHolderKind holderKind = DisplayBudgetHolderKind::Unheld;
    QByteArray holderId;
    /// The holder is away (dropped, inside its 180 s): rule 3 applies.
    bool holderAway = false;
    /// The device the PureSignal display goes to, if any (rule 4).
    std::optional<QByteArray> ps3Subscriber;
    /// The least each device is counted as asking for, per dimension (fix
    /// wave 2, Critical 1): one useful pan (DisplayLoadGovernor::
    /// floorPanCharge()), so a device that has not subscribed yet, or an
    /// older client that plans inside its share, keeps at least the
    /// smaller of that and an equal part beside a device asking for the
    /// whole total, unless that device is a present holder (rule 1 leaves
    /// the others only what its request leaves; fix wave 3). The wire
    /// cannot tell a sound-only device from one that has not subscribed
    /// yet, so the Core floors every device. Zero floors nothing.
    DisplayBudgetCharge minimumRequest;
};

struct DisplayBudgetShare {
    QByteArray id;
    DisplayBudgetLimits limits;
    DisplayBudgetReason reason = DisplayBudgetReason::None;
};

/// Ruling 9.3, applied per budget dimension (application bytes and
/// spectrum sample units):
///  4. PureSignal's display is charged once: its charge comes off the
///     total first and is added to the subscriber's share alone.
///  1. A network holder that is present gets its whole request, up to what
///     is left.
///  2. What is left is shared among the other devices, max-min fair: equal
///     shares, and a device asking for less than its share leaves the
///     difference to the rest.
///  3. With transmit unheld, held by the station device or held by a device
///     that is away, every device shares the whole total that way.
///  Each device's request counts as at least minimumRequest (fix wave 2,
///  Critical 1). What no device asks for is then shared equally among all
///  of them, as room to grow into (fix wave I5): a device alone has the
///  whole total, and a device given its whole request keeps some headroom.
/// A share never reaches zero in either dimension (a budget of zero is not
/// a budget): at least 1, which is too small for any display, so the
/// device's client suspends its display with sound kept.
///
/// The reason (design ruling 9.3a): with more than one device and a share
/// below its request, SharedProcessing while the governor's cut is in
/// force, SharedConnection otherwise; alone, or given its whole request,
/// the total's own reason (CoreBusy or None).
///
/// Generations: an unchanged share (limits and reason) keeps the generation
/// last published. A changed one takes the total's generation when that is
/// newer than the last published, so a device alone on the Core sees the
/// same generations as before shares existed, and otherwise the last
/// published plus one. A device never published before takes the total's.
class DisplayBudgetSplit {
public:
    static QList<DisplayBudgetShare> split(const DisplayBudgetSplitInput& input);
    /// Serial-number order on 32-bit generations: `candidate` is newer
    /// than `reference` by less than half the number space.
    static bool newerGeneration(quint32 candidate, quint32 reference);
};

} // namespace NereusSDR
