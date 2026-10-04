// NereusSDR for iOS: one named argument of a command to the Core
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// One argument of `CommandClient.invoke`. Arguments keep their order on
/// the wire (link document section 9.1).
public struct CommandArgument: Equatable, Sendable {
    public let name: String
    public let value: MirrorValue

    public init(name: String, value: MirrorValue) {
        self.name = name
        self.value = value
    }
}
