// NereusSDR for iOS: the first receiver input's filter state as the Core reports it, and why its low-pass is set for another slice
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// The first receiver input's filter chain as the Core reports it on the
/// `radio` object (link document section 7): `rxFilter0Mode`,
/// `rxFilter0Effective`, `rxFilter0Band` and `rxFilter0Reason`, read the
/// way the desktop's remote window reads them
/// (`RadioModel::applyStationFilterValue`): the state is there once all
/// four have arrived, a reason over 512 characters is not taken, and a
/// mode or effective state outside its values is not taken.
///
/// From a Core the app told it shows them (`rxFilterLowPass` 1), the same
/// object also carries `rxFilter0LowPassReason`, why the receive low-pass
/// on that shared input is set for one slice (or, on the Hermes Lite 2,
/// why the broadcast-band high-pass is off), and `rxFilter0LowPassSlice`,
/// that slice's id or -1. Both are optional: a Core that never sends them
/// still shows its chain.
public struct ReceiveFilterState: Equatable, Sendable {
    /// What the band-pass is doing now (the Core's `BpfEffective`, in its order).
    public enum Effective: Int64, Sendable {
        case filtered = 0
        case bypass = 1
        case widebandLocked = 2
    }

    public let effective: Effective
    /// The Core's words for the chain, as sent ("BYPASS (multi-band: 80m + 20m)").
    public let reason: String
    /// Why the receive low-pass on this input is set for one slice, as
    /// sent; nil when the Core sends none or sends it empty.
    public let lowPassReason: String?
    /// The slice the low-pass is set for; nil when the Core sends none or -1.
    public let lowPassSliceId: Int?

    /// `radio`'s properties the state is read from.
    public static let modeProperty = "rxFilter0Mode"
    public static let effectiveProperty = "rxFilter0Effective"
    public static let bandProperty = "rxFilter0Band"
    public static let reasonProperty = "rxFilter0Reason"
    public static let lowPassReasonProperty = "rxFilter0LowPassReason"
    public static let lowPassSliceProperty = "rxFilter0LowPassSlice"
    /// The longest reason the desktop takes.
    public static let reasonLimit = 512
    /// The Core's modes run from Auto (0) to Force bypass (2).
    static let modeRange: ClosedRange<Int64> = 0...2
    /// A slice id the desktop takes (it also takes -1, none).
    static let sliceRange: ClosedRange<Int64> = 0...63

    public init(effective: Effective, reason: String, lowPassReason: String? = nil, lowPassSliceId: Int? = nil) {
        self.effective = effective
        self.reason = reason
        self.lowPassReason = lowPassReason
        self.lowPassSliceId = lowPassSliceId
    }

    /// The state in `radio`'s values; nil until the four chain properties
    /// have all arrived in range.
    public init?(radio values: [String: MirrorValue]) {
        guard let mode = Self.whole(values[Self.modeProperty]), Self.modeRange.contains(mode),
              let effectiveNumber = Self.whole(values[Self.effectiveProperty]),
              let effective = Effective(rawValue: effectiveNumber),
              let band = Self.whole(values[Self.bandProperty]), band >= 0,
              case .text(let reason)? = values[Self.reasonProperty], reason.count <= Self.reasonLimit else {
            return nil
        }
        self.effective = effective
        self.reason = reason
        if case .text(let lowPass)? = values[Self.lowPassReasonProperty], !lowPass.isEmpty,
           lowPass.count <= Self.reasonLimit {
            lowPassReason = lowPass
        } else {
            lowPassReason = nil
        }
        if let slice = Self.whole(values[Self.lowPassSliceProperty]), Self.sliceRange.contains(slice) {
            lowPassSliceId = Int(slice)
        } else {
            lowPassSliceId = nil
        }
    }

    private static func whole(_ value: MirrorValue?) -> Int64? {
        switch value {
        case .int(let number)?, .enumeration(let number)?:
            return number
        default:
            return nil
        }
    }
}
