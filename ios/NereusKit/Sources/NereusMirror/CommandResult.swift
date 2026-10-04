// NereusSDR for iOS: the Core's answer to one command
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusLink

/// One `command.result` (link document section 9.1).
public struct CommandResult: Equatable, Sendable {
    /// The Core did what was asked (or, for an interim answer, took it on).
    public let accepted: Bool
    /// The Core's reason, in its own words; empty on success.
    public let reason: String
    /// The object keys the command changed (the wire key `affected`).
    public let affectedKeys: [String]
    /// What a command that returns data returned, by name.
    public let values: [String: MirrorValue]
    /// The original typed entries, including ordinals, for commands whose
    /// reply contract requires more than a name lookup.
    public let valueEntries: [LinkMessage.PropertyEntry]
    /// A PureSignal action's phase: `accepted`, `pending`, `completed` or
    /// `failed`; nil for a command that answers once.
    public let phase: String?

    public init(accepted: Bool, reason: String, affectedKeys: [String], values: [String: MirrorValue],
                phase: String?, valueEntries: [LinkMessage.PropertyEntry] = []) {
        self.accepted = accepted
        self.reason = reason
        self.affectedKeys = affectedKeys
        self.values = values
        self.valueEntries = valueEntries
        self.phase = phase
    }

    /// The answer a `command.result` carries.
    public init(_ wire: LinkMessage.CommandResult) {
        var values: [String: MirrorValue] = [:]
        for entry in wire.values ?? [] {
            values[entry.name] = MirrorValue(entry.value)
        }
        var phase: String?
        if case .text(let text)? = values["phase"] {
            phase = text
        }
        self.init(accepted: wire.accepted, reason: wire.reason, affectedKeys: wire.affected, values: values,
                  phase: phase, valueEntries: wire.values ?? [])
    }

    /// True for the answer that ends a command: every answer but a
    /// PureSignal action's interim `accepted` or `pending`.
    public var isFinal: Bool {
        phase != "accepted" && phase != "pending"
    }
}
