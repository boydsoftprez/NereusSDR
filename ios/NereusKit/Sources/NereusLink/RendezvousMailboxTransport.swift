// NereusSDR for iOS: a pairing carried through the remote access service's mailbox
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import os

/// A code pairing's connection when it runs through a mailbox (the
/// rendezvous document, section 6.5; the link document, sections 3.6 and
/// 19): each of the link's `pair.*` messages is the text of one mailbox
/// body, forwarded unchanged, and nothing else travels. It is a
/// ``LinkTransport`` so ``PairingClient`` runs the same exchange it runs on
/// a direct connection. A message that is not a `pair.*` one is dropped
/// here, since a mailbox carries pairing messages and nothing more.
///
/// A mailbox has no certificate: ``open(onEvent:)`` reports none, and the
/// Core's certificate binding is checked at this device's first sign-in
/// instead. The mailbox's end (either side closing it, the Core's
/// connection going, its 300 s lifetime) arrives as `.closed`.
final class RendezvousMailboxTransport: LinkTransport, @unchecked Sendable {
    private static let logger = Logger(subsystem: "NereusSDR", category: "rendezvous.mailbox")

    private enum Outgoing {
        case body(String)
        case close
    }

    private let client: RendezvousClient
    private let outgoing: AsyncStream<Outgoing>.Continuation
    private let admissionLock = NSLock()
    private var closed = false

    /// `client` holds an open mailbox; this transport closes it, and the
    /// client, when it closes.
    init(client: RendezvousClient) {
        self.client = client
        let (stream, continuation) = AsyncStream.makeStream(of: Outgoing.self)
        outgoing = continuation
        // One sender, so bodies leave in the order they were sent.
        Task {
            for await item in stream {
                switch item {
                case .body(let text):
                    await client.sendMailbox(text)
                case .close:
                    await client.closeMailbox()
                    await client.close()
                    return
                }
            }
        }
    }

    func open(onEvent: @escaping @Sendable (LinkTransportEvent) async -> Void) async throws -> Data {
        let events = client.events
        Task {
            for await event in events {
                switch event {
                case .mailbox(let body):
                    await onEvent(.text(body))
                case .mailboxClosed, .connectionLost:
                    await onEvent(.closed)
                    return
                case .candidate, .relayGrant, .introductionEnded, .serviceError:
                    break
                }
            }
            await onEvent(.closed)
        }
        return Data()
    }

    @discardableResult func send(_ text: String) -> Bool {
        guard Self.isPairingMessage(text) else {
            Self.logger.info("Not sending a message a pairing mailbox does not carry")
            return false
        }
        return admissionLock.withLock {
            guard !closed else { return false }
            if case .enqueued = outgoing.yield(.body(text)) { return true }
            return false
        }
    }

    /// The service's own pings keep the mailbox's connection alive.
    func ping() {}

    func close() {
        admissionLock.withLock {
            guard !closed else { return }
            closed = true
            outgoing.yield(.close)
            outgoing.finish()
        }
    }

    /// True when `text` is a JSON object whose `type` starts with "pair.".
    static func isPairingMessage(_ text: String) -> Bool {
        guard let object = try? JSONSerialization.jsonObject(with: Data(text.utf8)) as? [String: Any],
              let type = object["type"] as? String else {
            return false
        }
        return type.hasPrefix("pair.")
    }
}
