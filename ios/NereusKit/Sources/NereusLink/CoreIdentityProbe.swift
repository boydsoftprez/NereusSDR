// NereusSDR for iOS: which Core answers at an address, read from its hello before anything is sent
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import os

/// Reads which Core answers at an address (link document sections 3.4 and
/// 5.1): a connection of its own under the pairing trust, which accepts any
/// certificate and reports its SHA-256, the Core's `hello`, and the close.
/// The Core is known only when its `hello` names an identity key whose
/// binding verifies for the certificate this connection presented, the same
/// check a session makes before it signs in. The phone sends nothing on it,
/// so nothing is paired, signed in or used up.
///
/// A known identity only tells the phone which of its Cores to sign in to:
/// the sign-in then runs on a connection of its own under that Core's
/// identity trust, which checks the Core again, so an address never lets an
/// unknown Core in.
public enum CoreIdentityProbe {
    public enum Outcome: Sendable, Equatable {
        /// The Core's identity key, its 91-byte SubjectPublicKeyInfo DER.
        case identified(publicKey: Data)
        /// Something answered, but named no identity key that binds this
        /// connection's certificate (an older Core, or not a Core).
        case unidentified
        /// Nothing answered in time; `localNetworkDenied` when iOS would not
        /// let the app try.
        case notReached(localNetworkDenied: Bool)
    }

    private static let logger = Logger(subsystem: "NereusSDR", category: "link.identity")

    /// Reads the identity of the Core at `endpoint`, within `deadline` of
    /// the dial (the link's connect deadline unless the caller bounds it).
    /// Cancelling the task closes the connection at once and reports
    /// ``Outcome/notReached(localNetworkDenied:)``.
    public static func identify(_ endpoint: StationEndpoint, transportFactory: LinkTransportFactory,
                                clock: any LinkClock,
                                deadline: Duration = StationSession.connectDeadline) async -> Outcome {
        let transport = transportFactory(endpoint, .pairing)
        let first = FirstText()
        return await withTaskCancellationHandler {
            await read(transport, first: first, clock: clock, deadline: deadline)
        } onCancel: {
            first.put(nil)
            transport.close()
        }
    }

    private static func read(_ transport: any LinkTransport, first: FirstText, clock: any LinkClock,
                             deadline: Duration) async -> Outcome {
        if Task.isCancelled {
            transport.close()
            return .notReached(localNetworkDenied: false)
        }
        let timer = clock.schedule(after: deadline) {
            first.put(nil)
            transport.close()
        }
        defer {
            timer.cancel()
            transport.close()
        }
        let certificateSHA256: Data
        do {
            certificateSHA256 = try await transport.open { event in
                switch event {
                case .text(let text):
                    first.put(text)
                case .closed:
                    first.put(nil)
                case .pong:
                    break
                }
            }
        } catch LinkTransportError.localNetworkDenied {
            return .notReached(localNetworkDenied: true)
        } catch {
            logger.info("Nothing answered at the address to identify")
            return .notReached(localNetworkDenied: false)
        }
        guard let text = await first.take() else {
            return .notReached(localNetworkDenied: false)
        }
        guard case .hello(let hello)? = try? LinkCodec.decode(text), let claim = hello.identity,
              let key = Base64URL.decode(claim.publicKey), P256Wire.isCanonicalKey(key),
              StationTrust.identityVerifies(claim, publicKey: key, certificateSHA256: certificateSHA256) else {
            logger.info("The answer at the address named no identity that binds its certificate")
            return .unidentified
        }
        return .identified(publicKey: key)
    }

    /// The first text, or nil when the connection ended or ran out of time
    /// before one came; later ones are dropped.
    private final class FirstText: @unchecked Sendable {
        private let lock = NSLock()
        private var value: String??
        private var waiter: CheckedContinuation<String?, Never>?

        func put(_ text: String?) {
            let resume: CheckedContinuation<String?, Never>? = lock.withLock {
                guard value == nil else {
                    return nil
                }
                value = .some(text)
                defer { waiter = nil }
                return waiter
            }
            resume?.resume(returning: text)
        }

        func take() async -> String? {
            await withCheckedContinuation { continuation in
                let ready: String?? = lock.withLock {
                    if let value {
                        return value
                    }
                    waiter = continuation
                    return nil
                }
                if let ready {
                    continuation.resume(returning: ready)
                }
            }
        }
    }
}
