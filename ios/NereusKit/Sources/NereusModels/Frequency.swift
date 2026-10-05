// NereusSDR for iOS: a radio frequency in whole hertz
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// A radio frequency held as whole hertz.
public struct Frequency: Hashable, Comparable, Sendable {
    /// The frequency in hertz.
    public var hz: Int64

    public init(hz: Int64) {
        self.hz = hz
    }

    /// The frequency in megahertz with six decimal places, for example "14.074000".
    public var megahertzText: String {
        let sign = hz < 0 ? "-" : ""
        let magnitude = hz.magnitude
        let whole = magnitude / 1_000_000
        let fraction = String(magnitude % 1_000_000)
        let padded = String(repeating: "0", count: 6 - fraction.count) + fraction
        return "\(sign)\(whole).\(padded)"
    }

    public static func < (lhs: Frequency, rhs: Frequency) -> Bool {
        lhs.hz < rhs.hz
    }
}
