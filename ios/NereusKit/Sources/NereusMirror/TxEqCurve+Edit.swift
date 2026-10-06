// NereusSDR for iOS: changing the TX EQ curve through the Core's txEq.setCurve and txEq.resetCurve
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusLink

/// Changing the curve (link document section 7.1, "Changing the curve",
/// `txEqCurveVersion` 2): the phone sends the whole curve as `curveJson`
/// through `txEq.setCurve`, or asks for `txEq.resetCurve` with no
/// arguments. The Core rounds, orders and stores what it takes and answers
/// with the `curve` it now holds, which is what the phone draws. A refusal
/// changes nothing and carries the Core's own words.
extension TxEqCurve {
    public static let setCurveVerb = "txEq.setCurve"
    public static let resetCurveVerb = "txEq.resetCurve"
    /// The one argument of `txEq.setCurve`.
    public static let curveArgumentName = "curveJson"
    /// The accepted answer's value: the curve the Core now holds.
    public static let returnedCurveName = "curve"
    /// The `txEqCurveVersion` from which the Core takes a change.
    public static let editableVersion: Int64 = 2
    /// Why the curve is read-only on a Core below `editableVersion`.
    public static let readOnlyReason = "This Core cannot change the TX EQ curve from here. Updating the Core may help."
    /// How long a change waits for the Core's answer.
    public static let answerTimeout: Duration = .seconds(5)

    /// Whether a Core that answered `capabilityVersion` takes a change.
    public static func canChange(capabilityVersion: Int64?) -> Bool {
        (capabilityVersion ?? 0) >= editableVersion
    }

    /// The curve as `txEq.setCurve` takes it: `parametric`, `preampDb`,
    /// `minHz`, `maxHz` and the points in the order held. `state` is the
    /// Core's to say, so it is left out.
    public var curveJson: String {
        let items: [LinkJSON] = points.map { point in
            .object(["frequencyHz": .number(point.frequencyHz), "gainDb": .number(point.gainDb),
                     "q": .number(point.q)])
        }
        return LinkJSON.object(["parametric": .bool(parametric), "preampDb": .number(preampDb),
                                "minHz": .number(minHz), "maxHz": .number(maxHz),
                                "points": .array(items)]).compactText
    }

    /// What became of a change.
    public enum EditOutcome: Equatable, Sendable {
        /// The Core took it. The curve it returned, `unavailable` when that
        /// does not read, nil when it returned none (the `transmit` delta's
        /// `txEqCurve` then carries it).
        case taken(TxEqCurve?)
        /// The Core refused it, in its own words; nothing changed.
        case refused(String)
    }

    /// A `txEq.setCurve` or `txEq.resetCurve` answer as the phone reads it.
    public static func outcome(_ result: CommandResult) -> EditOutcome {
        guard result.accepted else {
            return .refused(result.reason)
        }
        guard case .text(let json)? = result.values[returnedCurveName] else {
            return .taken(nil)
        }
        return .taken(TxEqCurve(json: json))
    }

    /// Sends `curve` through `txEq.setCurve` and returns the Core's answer.
    /// Throws `CommandError` when nothing was sent or no answer came.
    public static func set(_ curve: TxEqCurve, through commands: CommandClient,
                           timeout: Duration = answerTimeout,
                           onLateOutcome: (@Sendable (Result<EditOutcome, CommandError>) async -> Void)? = nil)
        async throws -> EditOutcome {
        let argument = CommandArgument(name: curveArgumentName, value: .text(curve.curveJson))
        return try await edit(setCurveVerb, arguments: [argument], through: commands,
                              timeout: timeout, onLateOutcome: onLateOutcome)
    }

    /// Asks for `txEq.resetCurve` and returns the Core's answer.
    public static func reset(through commands: CommandClient,
                             timeout: Duration = answerTimeout,
                             onLateOutcome: (@Sendable (Result<EditOutcome, CommandError>) async -> Void)? = nil)
        async throws -> EditOutcome {
        try await edit(resetCurveVerb, arguments: [], through: commands,
                        timeout: timeout, onLateOutcome: onLateOutcome)
    }

    private static func edit(_ verb: String, arguments: [CommandArgument], through commands: CommandClient,
                             timeout: Duration,
                             onLateOutcome: (@Sendable (Result<EditOutcome, CommandError>) async -> Void)?)
        async throws -> EditOutcome {
        if let onLateOutcome {
            return outcome(try await commands.invokeHeld(verb, arguments: arguments, timeout: timeout) { result in
                await onLateOutcome(result.map(outcome))
            })
        }
        // Existing callers keep the original command lifetime and API defaults.
        return outcome(try await commands.invoke(verb, arguments: arguments, timeout: timeout))
    }
}
