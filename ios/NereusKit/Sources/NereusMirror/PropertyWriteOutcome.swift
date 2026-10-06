// NereusSDR for iOS: what came of one property write to the Core
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// The answer to `MirrorStore.write` (link document section 7.3). The Core
/// decides every write: `reason` is its own wording, shown as sent, except
/// where the write never reached it or its answer never came back.
public struct PropertyWriteOutcome: Equatable, Sendable {
    /// The Core kept the value asked for.
    public let accepted: Bool
    /// Why not, in plain words; empty when accepted.
    public let reason: String
    /// The value the Core kept, when it said.
    public let value: MirrorValue?
    /// False when the write was not sent or the link was lost before the
    /// Core answered; the mirror then holds the Core's last known value.
    public let answeredByCore: Bool
    /// False for an answer to an edit already replaced by a newer touch.
    /// It retires the waiter without changing the current control's note.
    public let isCurrent: Bool

    public init(accepted: Bool, reason: String, value: MirrorValue?, answeredByCore: Bool = true, isCurrent: Bool = true) {
        self.accepted = accepted
        self.reason = reason
        self.value = value
        self.answeredByCore = answeredByCore
        self.isCurrent = isCurrent
    }

    /// The write could not be sent: no session is open.
    public static let notSent = PropertyWriteOutcome(
        accepted: false, reason: "This app is not connected to the Core, so nothing was changed.",
        value: nil, answeredByCore: false)

    /// The link was lost before the Core answered: the control shows the
    /// Core's value again and says so, once.
    public static let linkLost = PropertyWriteOutcome(
        accepted: false, reason: "The connection to the Core dropped before it confirmed this change.",
        value: nil, answeredByCore: false)

    /// No answer came within ``MirrorStore/answerDeadline``: the
    /// control returns to the latest Core value, marked not confirmed while
    /// its actual pending answer remains observable.
    public static let notConfirmed = PropertyWriteOutcome(
        accepted: false, reason: "The Core has not confirmed this change.",
        value: nil, answeredByCore: false)

    /// What a control says of this outcome: nothing once the Core kept the
    /// value; otherwise the Core's reason as sent, `refused` when a refusal
    /// came without words, or the phone's own words when no answer came.
    public func noteText(refused: String) -> String? {
        if !isCurrent || accepted || heldForQuestion {
            return nil
        }
        return reason.isEmpty ? refused : reason
    }

    /// The Core holds the change for this phone's question sheet: not a
    /// refusal (StationClient.cpp:7308-7317). The operator's value stays
    /// until the Core's next value of it.
    public var heldForQuestion: Bool {
        !accepted && answeredByCore && reason == SeveralDevices.waitingReason
    }

    func superseded() -> PropertyWriteOutcome {
        .init(accepted: accepted, reason: reason, value: value, answeredByCore: answeredByCore, isCurrent: false)
    }

    /// Without a result from the Core, a write that came back at another
    /// value than asked for.
    static func keptOther(_ value: MirrorValue) -> PropertyWriteOutcome {
        PropertyWriteOutcome(accepted: false, reason: "The Core kept a different value.", value: value)
    }
}
