// NereusSDR for iOS: what the PTT needs from the Core's transmit state
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// Modification history (NereusSDR):
// 2026-10-27: Remote transmit hold then cut: the Core's RF hold (rfHeld,
//   rfHeldReason, rfHoldSerial, rfLastHoldMs) and the rule for showing it.
//   J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-10-27: Restore the decreasing-serial guard after interrupted mutation
//   verification. J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
// 2026-10-28: CW v2 lease, heartbeat and retirement integration.
//   J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.

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
    /// The Core is holding RF off for this key while its link or its
    /// microphone audio is quiet; the key itself stays on.
    public var rfHeld = false
    /// Why, while `rfHeld`: ``linkReason`` or ``micReason``; empty otherwise.
    public var rfHeldReason = ""
    /// Advances by one as each hold ends; 0 before the first. It starts again
    /// on another Core, so it can go down.
    public var rfHoldSerial: UInt64 = 0
    /// How long the last hold that ended lasted, in milliseconds.
    public var rfLastHoldMs: Int64 = 0

    /// The v2 tuple is decoded together after the entire txState delta applies.
    /// Appended server ordinals:54 UTF8,55 Int64,56 Bool; v1 is unchanged.
    public var remoteCw: RemoteCwActivity?

    public struct RemoteCwActivity: Equatable, Sendable {
        public let deviceId: String
        public let epoch: Int64
        public let active: Bool

        public init?(deviceId: String?, epoch: Int64?, active: Bool?) {
            guard let deviceId, let epoch, let active,
                  !deviceId.isEmpty, epoch > 0, epoch <= Int64(UInt32.max) else { return nil }
            self.deviceId = deviceId
            self.epoch = epoch
            self.active = active
        }
    }

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
    /// `rfHeldReason` for a hold while the link is quiet.
    public static let linkReason = "link"
    /// `rfHeldReason` for a hold while no microphone audio arrives.
    public static let micReason = "mic"

    /// Whether this device shows its key as held (spec section 5.3).
    ///
    /// Live: keyed and the Core says `rfHeld`. After: a hold this device
    /// never saw held, because it began and ended between two reports (the
    /// link was quiet, after all), is shown once the report that ends it
    /// arrives, for its length up to ``maxAfterHoldMs``.
    ///
    /// Each report is read whole: the hold's serial and its length come in
    /// one delta, and the rule reads them together once it has applied. A
    /// serial that goes down is another Core's count starting again: no
    /// hold ended there, and counting starts again from it.
    public struct HoldDisplay: Equatable, Sendable {
        /// The longest an after-hold display runs.
        public static let maxAfterHoldMs: Int64 = 3000

        /// The serial last read; nil before the first report.
        private var lastSerial: UInt64?
        /// `rfHeld` was read true since `lastSerial` was.
        private var sawHeld = false
        /// The running after-hold display's end, if one runs.
        public private(set) var afterHoldUntil: ContinuousClock.Instant?

        public init() {}

        /// Reads one whole report; `keyed` is this device's key being on.
        public mutating func observe(_ report: TransmitStateReport, keyed: Bool, now: ContinuousClock.Instant) {
            if !keyed || report.rfHeld {
                afterHoldUntil = nil
            }
            let serial = report.rfHoldSerial
            guard let last = lastSerial else {
                // What the first report reads is already seen.
                lastSerial = serial
                sawHeld = report.rfHeld
                return
            }
            if serial < last {
                afterHoldUntil = nil
                lastSerial = serial
                sawHeld = report.rfHeld
                return
            }
            if serial > last {
                // More than one hold ended: the last of them was never seen.
                let unseen = !sawHeld || serial - last > 1
                let ms = min(max(report.rfLastHoldMs, 0), Self.maxAfterHoldMs)
                if unseen, keyed, !report.rfHeld, ms > 0 {
                    afterHoldUntil = now.advanced(by: .milliseconds(ms))
                }
                lastSerial = serial
                sawHeld = report.rfHeld
                return
            }
            if report.rfHeld {
                sawHeld = true
            }
        }

        /// Forgets every report: the next one is read as a first.
        public mutating func reset() {
            self = HoldDisplay()
        }

        /// "Held" is shown at `now`.
        public func showsHeld(_ report: TransmitStateReport, keyed: Bool, now: ContinuousClock.Instant) -> Bool {
            guard keyed else {
                return false
            }
            if report.rfHeld {
                return true
            }
            if let until = afterHoldUntil {
                return now < until
            }
            return false
        }
    }
}
