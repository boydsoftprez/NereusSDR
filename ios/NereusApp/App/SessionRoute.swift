// NereusSDR for iOS: the current session, which the app's clients send through
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink
import NereusMirror

/// The session the app's clients send through, which changes as the app
/// connects to a Core and disconnects. With no session, a send fails as a
/// session with no connection refuses one.
final class SessionRoute: @unchecked Sendable {
    private let lock = NSLock()
    private var current: StationSession?
    #if DEBUG
    private var bandFixturePropertySender: (@Sendable (LinkMessage.PropertyWrite) async throws -> Void)?

    func useBandFixturePropertySender(_ sender: @escaping @Sendable (LinkMessage.PropertyWrite) async throws -> Void) {
        lock.withLock { bandFixturePropertySender = sender }
    }

    private var commandHandoffHook: (@Sendable (LinkMessage) async -> Void)?

    func holdCommandHandoffForTesting(_ hook: (@Sendable (LinkMessage) async -> Void)?) {
        lock.withLock { commandHandoffHook = hook }
    }
    #endif

    var session: StationSession? {
        lock.withLock { current }
    }

    func use(_ session: StationSession?) {
        lock.withLock { current = session }
    }

    /// Sends one message through the current session.
    func send(_ message: LinkMessage) async throws {
        #if DEBUG
        if case .propertyWrite(let write) = message,
           let fixture = lock.withLock({ current == nil ? bandFixturePropertySender : nil }) {
            try await fixture(write)
            return
        }
        #endif
        guard let session else {
            throw LinkSendError.notConnected
        }
        try await session.send(message)
    }

    /// Capture a logical session for a command at admission. A physical
    /// upgrade keeps this StationSession object, while replacement changes it.
    func captureCommandSender() -> @Sendable (LinkMessage, CommandSendPermit) async throws -> Void {
        let admitted = session
        return { [weak self, admitted] message, permit in
            #if DEBUG
            let hook = self?.lock.withLock { self?.commandHandoffHook }
            await hook?(message)
            #endif
            guard let self, let admitted, self.session === admitted else {
                throw LinkSendError.notConnected
            }
            try await admitted.send(message, permit: permit)
        }
    }

    /// The same exact-session handoff for a VOX property write.
    func captureVoxSender() -> MirrorStore.BoundSender {
        captureCommandSender()
    }

    func captureHeartbeatSender() -> @Sendable (LinkMessage, TransmitHeartbeatGate) async throws -> Void {
        let admitted = session
        return { [weak self, admitted] message, gate in
            #if DEBUG
            let hook = self?.lock.withLock { self?.commandHandoffHook }
            await hook?(message)
            #endif
            guard let self, let admitted, self.session === admitted else {
                throw LinkSendError.notConnected
            }
            try await admitted.send(message, while: gate)
        }
    }

    /// The current session's media connection is up.
    func mediaConnectionUp() async {
        await session?.mediaConnectionUp()
    }
}
