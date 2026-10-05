// NereusSDR for iOS: where the app's connection to its Core has got to
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusLink

/// The connection to the Core as the screens see it, from the session's
/// states and refusals. Finding Cores, pairing and remote access add their
/// own cases when they arrive.
enum ConnectionState: Sendable, Equatable {
    /// No Core is connected: none was chosen, or the operator disconnected.
    case notConnected
    /// Reaching the Core.
    case connecting
    /// Signing in to the Core.
    case signingIn
    /// Signed in; the Core's state is arriving.
    case loading
    /// Connected, with the Core's whole state.
    case connected
    /// The link was lost or the Core ended the session; the app tries again
    /// in `seconds`. `reason` is why, when the Core or the app said.
    case waitingToRetry(seconds: Int, reason: Refusal?)
    /// Stopped for a reason the app will not retry on its own.
    case refused(Refusal)
}
