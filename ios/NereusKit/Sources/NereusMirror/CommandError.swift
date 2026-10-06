// NereusSDR for iOS: why a command to the Core has no answer
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// Why `CommandClient.invoke` returned no answer. A refusal by the Core is
/// not an error: it comes back as a `CommandResult` that is not accepted.
public enum CommandError: Error, Equatable, Sendable {
    /// No session was open, so nothing was sent.
    case notSent
    /// The Core did not give its final answer in time.
    case timedOut
    /// The link was lost before the Core answered.
    case linkLost
}
