// NereusSDR for iOS: why pairing with a Core did not finish
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// Why ``PairingClient`` did not pair (link document section 3.6). The
/// Core's reasons are its own words, shown as sent; the other cases are the
/// app's to word.
public enum PairingError: Error, Sendable, Equatable {
    /// The typed text is not a code: not a number and two words of the
    /// list. Nothing was sent, so no code was burned.
    case notACode
    /// The Core does not pair devices (its `hello` declares no pairing, or
    /// shares no link version with this app).
    case cannotPair
    /// The Core named password hash settings other than the fixed ones, so
    /// the app did not hash the code or answer.
    case weakHashSettings
    /// The code was not the Core's, and the Core said so. The Core burned
    /// it; a new one appears after `retryAfter`. After the fifth wrong code
    /// in a row the Core closes pairing instead, and `retryAfter` is zero: a
    /// new code then needs pairing opened again, on the Core or on a paired
    /// device. Only the Core's own answer gives this case.
    case wrongCode(retryAfter: Duration, reason: String)
    /// The Core refused, with its reason and when to try again (zero: now,
    /// or not with this Core as it stands). A closed pairing window is one:
    /// a reopened window closes after 10 minutes, and every window closes
    /// after 5 wrong codes in a row.
    case refused(reason: String, retryAfter: Duration)
    /// The Core's identity in its answer is not the one its `hello` named,
    /// or does not bind this connection's certificate.
    case identityMismatch
    /// The connection to the Core never opened: nothing answered at the
    /// address, the opening was refused or failed, or the pairing's deadline
    /// came while it was still opening. `localNetworkDenied` is true when
    /// iOS did not let the app reach devices on this network, so the Core
    /// was never tried. Nothing reached the Core, so no code was burned.
    case didNotOpen(localNetworkDenied: Bool)
    /// The connection opened, but the Core did not finish its side of the
    /// pairing before the connect deadline (30 s from the dial), so the app
    /// closed it.
    case timedOut
    /// The connection closed before pairing finished: the Core's own words
    /// when it ended it with them, nil for a bare close, a lost link, or an
    /// exchange the app could not go on with. After the app found the code
    /// wrong and said so, an end without the Core's answer carries the
    /// app's own words, which say the code was burned.
    case ended(reason: String?)
    /// A pairing with this Core is already running in this app; nothing was
    /// dialled.
    case alreadyPairing
}
