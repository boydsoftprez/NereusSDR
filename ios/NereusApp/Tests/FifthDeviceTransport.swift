// NereusSDR for iOS: a full Core in the app flow tests, with the snapshot held until an answer
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink

/// Adapts a paired FakeStation sign-in into the Core's fifth-device path.
/// After accepted auth it sends the held roster in place of capabilities,
/// then releases the fake's original snapshot only after explicit confirm.
final class FifthDeviceTransport: LinkTransport, @unchecked Sendable {
    private let inner: any LinkTransport
    private let question: LinkMessage.SessionHeld
    private let lock = NSLock()
    private var waiting = false
    private var queued: [LinkTransportEvent] = []
    private var handler: (@Sendable (LinkTransportEvent) async -> Void)?

    init(inner: any LinkTransport, question: LinkMessage.SessionHeld) {
        self.inner = inner
        self.question = question
    }

    var boundsItsOwnOpening: Bool { inner.boundsItsOwnOpening }

    func open(onEvent: @escaping @Sendable (LinkTransportEvent) async -> Void) async throws -> Data {
        lock.withLock { handler = onEvent }
        return try await inner.open { [self] event in
            if case .text(let text) = event, let message = try? LinkCodec.decode(text) {
                switch message {
                case .hello(var hello):
                    hello.features?["sessionHolder"] = 1
                    await onEvent(.text(LinkCodec.encode(.hello(hello))))
                    return
                case .authResult(let result) where result.accepted:
                    await onEvent(event)
                    lock.withLock { waiting = true }
                    await onEvent(.text(LinkCodec.encode(.sessionHeld(question))))
                    return
                default:
                    break
                }
            }
            let held = lock.withLock { () -> Bool in
                if waiting { queued.append(event) }
                return waiting
            }
            if !held { await onEvent(event) }
        }
    }

    @discardableResult func send(_ text: String) -> Bool {
        guard inner.send(text) else { return false }
        guard case .sessionTakeover(let answer)? = try? LinkCodec.decode(text) else { return true }
        let (deliver, pending) = lock.withLock { () -> ((@Sendable (LinkTransportEvent) async -> Void)?, [LinkTransportEvent]) in
            waiting = false
            let saved = queued
            queued.removeAll()
            return (handler, saved)
        }
        guard let deliver else { return true }
        Task {
            if answer.deviceId.isEmpty {
                await deliver(.text(LinkCodec.encode(.sessionEnd(.init(
                    reason: "The Core already has four devices connected.", retryable: false, code: "coreFull")))))
            } else {
                for event in pending { await deliver(event) }
            }
        }
        return true
    }

    func ping() { inner.ping() }
    func close() { inner.close() }
    func setBinaryReceiver(_ receiver: (@Sendable (Data) -> Void)?) { inner.setBinaryReceiver(receiver) }
    @discardableResult func sendBinary(_ frame: Data) -> Bool { inner.sendBinary(frame) }
    @discardableResult func sendBinary(_ frame: Data, ownership: BinaryMediaOwnership) -> Bool {
        inner.sendBinary(frame, ownership: ownership)
    }
    func discardBinary(ownership: BinaryMediaOwnership) { inner.discardBinary(ownership: ownership) }
}
