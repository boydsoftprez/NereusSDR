// NereusSDR for iOS: what the PTT needs from the Core's transmit state
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// The parts of the Core's `txState` object (link document section 18.8)
/// the PTT follows, and whether its holder is this device, which the app
/// settles from its own device id.
public struct TransmitStateReport: Equatable, Sendable {
    /// The radio is on the air (MOX, TUNE or two-tone).
    public var keyed = false
    /// Only during a RADE end-of-over tail.
    public var txEnding = false
    /// Why the Core last stopped a transmission on its own, and its words.
    public var stopReason = ""
    public var stopText = ""
    /// Advances by one with each such stop; 0 before the first.
    public var stopSerial: Int64 = 0
    /// The epoch of the key that stop ended.
    public var stopEpoch: Int64 = 0
    /// Somebody holds transmit (`holderDeviceId` is not empty).
    public var held = false
    /// The holder is this device.
    public var heldHere = false
    public var holderName = ""
    public var holderShortName = ""
    /// `device`, or `radioPtt` after the radio's own PTT took transmit.
    public var holderSource = ""
    public var holderEpoch: Int64 = 0
    public var holderAway = false
    public var holderTransferring = false

    public init() {}

    /// The holder is another device, or the radio.
    public var heldElsewhere: Bool { held && !heldHere }

    /// The holder's name as the PTT shows it: "Radio" after the radio's own
    /// PTT took transmit, else its short name.
    public var holderLabel: String {
        if holderSource == Self.radioPttSource {
            return Self.radioLabel
        }
        return holderShortName.isEmpty ? holderName : holderShortName
    }

    /// `holderSource` after a take by the radio's own PTT.
    public static let radioPttSource = "radioPtt"
    /// The radio's own PTT, as the PTT names it.
    public static let radioLabel = "Radio"
}
