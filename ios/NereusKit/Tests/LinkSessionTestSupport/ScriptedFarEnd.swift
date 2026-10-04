// NereusSDR for iOS: the Core's end of a scripted connection, over a WebSocket or a control data channel
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// What a session fixture's runner does at the Core's end of a connection,
/// whichever transport carries it: ``ScriptedTransport`` (the WebSocket's
/// stand-in) or ``ScriptedDataChannel`` (the session's real data-channel
/// transport over a channel the test plays).
public protocol ScriptedFarEnd: AnyObject, Sendable {
    /// Session messages the app sent and the test has not taken yet.
    var pending: [String] { get }
    /// Waits until the app has sent a message the test has not taken yet.
    func waitForSent(within timeout: Duration) async -> Bool
    /// Takes the oldest message the app sent, if any.
    func takeSent() -> String?
    /// The Core sends one session message; returns once the session has handled it.
    func deliver(_ text: String) async
    /// Answers every ping the app sent so far.
    func answerPings() async
    /// The Core closes the connection, or the link drops.
    func dropLink() async
}

extension ScriptedTransport: ScriptedFarEnd {}

extension ScriptedDataChannel: ScriptedFarEnd {}
