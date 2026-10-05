// NereusSDR for iOS: how fast the S-meter's peak hold line falls, the desktop meter menu's Decay
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// The peak hold line's fall once its hold time is over (D86): Fast,
/// Medium or Slow, at the desktop's rates.
public enum SMeterPeakDecay: String, Equatable, Sendable, Codable, CaseIterable {
    case fast
    case medium
    case slow

    /// The fall in dB a second: 20, 10 and 5, the desktop's.
    public var dbPerSecond: Double {
        switch self {
        case .fast:
            return 20
        case .medium:
            return 10
        case .slow:
            return 5
        }
    }

    /// The menu's words, the desktop's: `Fast (20 dB/s)`.
    public var label: String {
        let name: String
        switch self {
        case .fast:
            name = "Fast"
        case .medium:
            name = "Medium"
        case .slow:
            name = "Slow"
        }
        return "\(name) (\(Int(dbPerSecond)) dB/s)"
    }
}
