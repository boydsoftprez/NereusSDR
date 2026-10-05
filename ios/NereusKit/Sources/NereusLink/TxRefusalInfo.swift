// NereusSDR for iOS: why the Core would not key, or would not release, for this device
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// A transmit refusal (link document section 18.3): the Core's sentence,
/// shown as sent, its stable code, and the fix it offers, or none. A
/// refused `command.result` carries the sentence as its `reason` and the
/// code and fix as its `refusalCode` and `refusalFix` values; the
/// capabilities carry the same three as `txRefusalReason`, `txRefusalCode`
/// and `txRefusalFix` for why this device may not transmit now.
public struct TxRefusalInfo: Equatable, Sendable {
    /// The Core's words.
    public let reason: String
    /// `notReady`, `ampStandby`, `otherDeviceHolds`, `micNotReady` and the
    /// rest of section 18.3; empty when the Core sent none.
    public let code: String
    /// `operateAmp` or `takeTransmit`; empty when there is no fix.
    public let fix: String

    public init(reason: String, code: String = "", fix: String = "") {
        self.reason = reason
        self.code = code
        self.fix = fix
    }

    /// The fix that puts the amplifier in operate.
    public static let operateAmp = "operateAmp"
    /// The fix that takes transmit from its holder (Task 77's `tx.take`).
    public static let takeTransmit = "takeTransmit"
}
