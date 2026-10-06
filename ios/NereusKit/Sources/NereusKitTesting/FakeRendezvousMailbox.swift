// NereusSDR for iOS: the remote access service's pairing mailbox, played in memory for the fake Core
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink

/// One connection to a stand-in for the remote access service, carrying a
/// pairing mailbox to a ``FakeStation`` (the rendezvous document, sections
/// 5 and 6.5; link document section 19). It greets with a `hello`, opens
/// the mailbox on the number of the fake's code now shown, and passes each
/// `mailbox` body to the fake's Core side of pairing, which answers without
/// a `hello` either way, and back. A number the fake does not show is
/// `nameplateUnknown`, and an introduction is `offline`, each in the
/// service's own words. Nothing reaches a network.
final class FakeRendezvousMailbox: LinkTransport, @unchecked Sendable {
    /// The service's words for a number no Core shows, and for a Core not on it.
    static let nameplateUnknownReason = "No Core is showing that pairing code right now. Check the code and try again."
    static let offlineReason =
        "The Core is not reachable right now. Check that it is running and connected to the internet."

    private weak var station: FakeStation?
    private let lock = NSLock()
    private var queue: AsyncStream<LinkTransportEvent>.Continuation?
    private var pump: Task<Void, Never>?
    private var pairing: FakePairingConnection?
    private var closed = false

    init(station: FakeStation) {
        self.station = station
    }

    deinit {
        queue?.finish()
        pump?.cancel()
    }

    func open(onEvent: @escaping @Sendable (LinkTransportEvent) async -> Void) async throws -> Data {
        let (stream, continuation) = AsyncStream.makeStream(of: LinkTransportEvent.self)
        let pumping = Task {
            for await event in stream {
                await onEvent(event)
            }
        }
        lock.withLock {
            queue = continuation
            pump = pumping
        }
        var nonce = Data(count: 32)
        for index in nonce.indices {
            nonce[index] = UInt8.random(in: 0...255)
        }
        enqueue(.hello(RendezvousMessage.Hello(version: 1, nonce: Base64URL.encode(nonce),
                                               stun: ["stun:stun.invalid:3478"])))
        return Data()
    }

    @discardableResult func send(_ text: String) -> Bool {
        guard lock.withLock({ !closed && queue != nil }),
              let message = try? RendezvousMessage.decode(text, direction: .toService) else { return false }
        switch message {
        case .mailboxOpen(let nameplate):
            openMailbox(nameplate)
            return true
        case .mailbox(let body):
            return lock.withLock { pairing }?.send(body) ?? false
        case .mailboxClose:
            endMailbox(code: "closed")
            return true
        case .introduce:
            enqueue(.error(RendezvousMessage.ServiceError(code: "offline", reason: Self.offlineReason,
                                                          retryAfterMs: 0)))
            return true
        case .candidate, .hello, .answer, .relayGrant, .introductionEnd, .mailboxOpened, .mailboxClosed, .error:
            return false
        }
    }

    func ping() {
        enqueueEvent(.pong)
    }

    func close() {
        let (open, pairing) = lock.withLock { () -> (Bool, FakePairingConnection?) in
            defer {
                closed = true
                queue?.finish()
            }
            return (!closed, self.pairing)
        }
        if open {
            pairing?.close()
        }
    }

    // MARK: The mailbox

    private func openMailbox(_ nameplate: Int) {
        guard let station, let shown = PairingClient.nameplate(ofNormalised: station.pairingCode),
              shown == nameplate else {
            enqueue(.error(RendezvousMessage.ServiceError(code: "nameplateUnknown", reason: Self.nameplateUnknownReason,
                                                          retryAfterMs: 0)))
            return
        }
        let connection = station.mailboxPairingConnection()
        lock.withLock { pairing = connection }
        enqueue(.mailboxOpened(nameplate: nameplate))
        Task { [weak self] in
            _ = try? await connection.open { [weak self] event in
                switch event {
                case .text(let body):
                    self?.enqueue(.mailbox(body: body))
                case .closed:
                    // The Core's side left the mailbox.
                    self?.endMailbox(code: "peerClosed")
                case .pong:
                    break
                }
            }
        }
    }

    private func endMailbox(code: String) {
        let ended = lock.withLock { () -> Bool in
            guard pairing != nil else {
                return false
            }
            pairing = nil
            return true
        }
        if ended {
            enqueue(.mailboxClosed(code: code))
        }
    }

    // MARK: Sending

    private func enqueue(_ message: RendezvousMessage) {
        enqueueEvent(.text(message.encoded))
    }

    private func enqueueEvent(_ event: LinkTransportEvent) {
        _ = lock.withLock { closed ? nil : queue }?.yield(event)
    }
}
