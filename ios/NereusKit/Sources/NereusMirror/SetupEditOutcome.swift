// NereusSDR for iOS: how an edit of a described Setup control ended
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// The end of one edit. `refused` carries the owner's reason (the Core's
/// words); `notSent` says why nothing left the phone.
public enum SetupEditOutcome: Equatable, Sendable {
    case applied
    case refused(String)
    case notSent(String)
    /// The Core holds the change for the question it asks next ("Waiting
    /// for you to confirm.", the link document, section 7.3): neither taken
    /// nor refused, and never shown as a refusal. The question carries it on.
    case awaitingConfirmation
}
