// NereusSDR for iOS: how a pairing reaches the Core
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// What carries a code pairing's messages to the Core (link document
/// section 3.6). The exchange is the same whichever carries it: the carrier
/// learns nothing it could test guesses against.
public enum PairingCarrier: Sendable, Hashable {
    /// A connection of its own straight to the Core's control port.
    case direct(StationEndpoint)
    /// The pairing mailbox on `nameplate`, the number in the code, through
    /// the remote access service (link document section 19; the rendezvous
    /// document, section 6.5). `server` is the ordered list of services,
    /// the operator's own first; the first that has the number is used.
    /// The `pair.*` messages travel unchanged inside the mailbox's bodies,
    /// and the code itself never does.
    case rendezvous(server: [RendezvousServer], nameplate: Int)
}
