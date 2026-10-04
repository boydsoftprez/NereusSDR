// NereusSDR for iOS: one of the keying verbs the app sends the Core
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// The keying verbs (link document section 18.6), under `remoteTxVersion` 1
/// at minor 11, and `tx.tunerTune` at `remoteTxVersion` 2. Each goes three
/// times as one command (one id).
public enum TransmitVerb: Equatable, Sendable {
    /// `tx.key {trigger}`: `"screen"` for the PTT and MOX on the screen.
    case key(trigger: String)
    /// `tx.unkey {epoch}`: releases the key with that epoch, or with
    /// 4294967295 when its answer has not come.
    case unkey(epoch: Int64)
    /// `tx.tune {on}`.
    case tune(on: Bool)
    /// `tx.twoTone {on}`.
    case twoTone(on: Bool)
    /// `tx.tunerTune {on}` (`remoteTxVersion` 2): the Core's Tuner Genius
    /// autotune, a key for every rule; `{on:false}` ends this device's
    /// cycle, keyed or still waiting for the amplifier.
    case tunerTune(on: Bool)

    /// The verb as the link names it.
    public var name: String {
        switch self {
        case .key:
            return "tx.key"
        case .unkey:
            return "tx.unkey"
        case .tune:
            return "tx.tune"
        case .twoTone:
            return "tx.twoTone"
        case .tunerTune:
            return "tx.tunerTune"
        }
    }

    /// A person's key on the phone's screen (the PTT and the TX panel's MOX).
    public static let screenTrigger = "screen"
}
