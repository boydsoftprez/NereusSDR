// NereusSDR for iOS: pairs this device with a Core, by one tap on its network or by its code
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import os

/// Pairs this device with a Core (link document section 3.6), on a
/// connection of its own over `LinkTransport` under the pairing trust: the
/// connection accepts any certificate and reports its SHA-256, which the
/// Core's identity key binds. Nothing on it signs in; it ends once the
/// pairing does. After a pairing the caller keeps the ``PairedStation`` and
/// connects the normal way, signing in with the device key on a new
/// connection. One pairing runs at a time for each Core, across every
/// client in the process, so a pairing connection is never more than one of
/// the two an address may have still connecting (section 12.3): a second
/// pairing with the same Core while one runs is refused before it dials
/// (``PairingError/alreadyPairing``). The Core is known by its host,
/// whatever its case, trailing dot or brackets, and its port. Cancelling the
/// task that pairs, while it dials or later, closes the connection at once
/// and throws `CancellationError`, and the Core can be paired with again
/// straight away.
///
/// A pairing that does not finish says which of three ways it ended: the
/// connection never opened (``PairingError/didNotOpen(localNetworkDenied:)``),
/// it opened but the Core did not finish in time
/// (``PairingError/timedOut``), or it closed first
/// (``PairingError/ended(reason:)``, with the Core's own words whenever it
/// sent them).
///
/// The order, by code: the Core's `hello`; the app's `hello`; `pair.start`;
/// `pair.spake` step 0 (the Core), 1 (the app), 2 (the Core), 3 (the app);
/// the app's `pair.confirm`; the Core's `pair.confirm`; the close. When the
/// codes differ, the app's step 3 fails and it sends `pair.fail` in its
/// place; the Core answers with its own `pair.fail` and the wait. One tap:
/// the two `hello`s, `pair.start`, the Core's `pair.accept`, the close. A
/// `pair.fail` from the Core may come at any step; a `session.end` or a
/// bare close before the Core's answer is a failure.
///
/// Through the remote access service
/// (``PairingCarrier/rendezvous(server:nameplate:)``, link document section
/// 19) the same messages travel as mailbox bodies, with no `hello` either
/// way: `pair.start` in code mode is the app's first message, and the
/// exchange then runs as above. A mailbox has no certificate, so the Core's
/// identity key comes from its sealed box alone and its certificate binding
/// is checked at this device's first sign-in; the paired Core has no
/// address yet, and is reached next through the service or at an address
/// the app learns.
///
/// The connect deadline (section 12.2) covers the whole pairing from the
/// dial, the opening included: the Core sends step 0 once its own Argon2id
/// hash has run, and the app hashes the code (64 MiB) off this actor before
/// step 1. The code, the keys and the exchange never reach the log.
public actor PairingClient {
    /// The whole pairing, opening included, must finish this long after the
    /// app dials: one bound from the dial, as the desktop's StationClient
    /// arms kStationHandshakeDeadlineMs (30000) before the WebSocket upgrade
    /// (src/core/session/StationClient.cpp:1011-1021).
    public static let connectDeadline: Duration = StationSession.connectDeadline

    /// The reason in the app's own `pair.fail`, when the Core's step 2 shows
    /// it holds another code.
    static let wrongCodeText = "The pairing code was not right."
    /// The app's words when the connection ends after its own `pair.fail`
    /// without the Core's answer: the code was not right, and the Core has
    /// burned it, but how long until the next one is not known.
    static let endedAfterWrongCodeText =
        "The pairing code was not right, and the connection to the Core ended. A new code will appear on the Core."

    /// The Cores a pairing is running with now, across every client.
    private static let inProgress = PairingGate()

    private static let logger = Logger(subsystem: "NereusSDR", category: "link.pairing")

    private let identity: DeviceIdentity
    private let name: String
    private let kind: DeviceKeyAuthenticator.Kind
    private let clock: any LinkClock
    private let makeTransport: LinkTransportFactory
    private let makeRendezvousTransport: RendezvousTransportFactory

    /// `name` is the one the operator confirmed (D65); it goes in
    /// `pair.start` and in the code pairing's box. Throws
    /// `DeviceKeyAuthenticator.UnusableName` for a name `DeviceName.isUsable`
    /// refuses, so a pairing never carries a name the app would later refuse
    /// at sign-in.
    public init(identity: DeviceIdentity, name: String, kind: DeviceKeyAuthenticator.Kind,
                clock: any LinkClock = SystemLinkClock(),
                transportFactory: @escaping LinkTransportFactory = WebSocketLinkTransport.factory,
                rendezvousTransportFactory: @escaping RendezvousTransportFactory = RendezvousWebSocket.factory) throws {
        guard DeviceName.isUsable(name) else {
            throw DeviceKeyAuthenticator.UnusableName()
        }
        self.identity = identity
        self.name = name
        self.kind = kind
        self.clock = clock
        makeTransport = transportFactory
        makeRendezvousTransport = rendezvousTransportFactory
    }

    /// Pairs with one tap, with a Core on this device's network that no
    /// device has paired with yet.
    public func pairOnThisNetwork(endpoint: StationEndpoint) async throws -> PairedStation {
        let key = Self.gateText(endpoint)
        guard Self.inProgress.claim(key) else {
            throw PairingError.alreadyPairing
        }
        defer { Self.inProgress.release(key) }
        return try await run(endpoint: endpoint, code: nil)
    }

    /// Pairs with the code the Core shows, typed in any form
    /// ``PairingCodeText/normalise(_:)`` accepts. A text that is not a code
    /// throws ``PairingError/notACode`` before anything is sent.
    public func pair(code: String, via carrier: PairingCarrier) async throws -> PairedStation {
        guard let normalised = PairingCodeText.normalise(code) else {
            throw PairingError.notACode
        }
        switch carrier {
        case .direct(let endpoint):
            let key = Self.gateText(endpoint)
            guard Self.inProgress.claim(key) else {
                throw PairingError.alreadyPairing
            }
            defer { Self.inProgress.release(key) }
            return try await run(endpoint: endpoint, code: normalised)
        case .rendezvous(let servers, let nameplate):
            // The mailbox is the one on the code's own number.
            guard Self.nameplate(ofNormalised: normalised) == nameplate else {
                throw PairingError.notACode
            }
            let key = "rendezvous \(nameplate)"
            guard Self.inProgress.claim(key) else {
                throw PairingError.alreadyPairing
            }
            defer { Self.inProgress.release(key) }
            return try await runMailbox(servers: servers, nameplate: nameplate, code: normalised)
        }
    }

    /// The number a normalised code starts with: the Core's nameplate on
    /// the remote access service.
    public static func nameplate(ofNormalised code: String) -> Int? {
        code.split(separator: "-").first.flatMap { Int($0) }
    }

    // MARK: One pairing at a time

    /// What a pairing is running with, process-wide: an endpoint as
    /// ``gateKey(_:)`` spells it, so two spellings of one Core share a gate,
    /// or a mailbox's number.
    private final class PairingGate: @unchecked Sendable {
        private let lock = NSLock()
        private var keys: Set<String> = []

        /// True, and `key` held, when no pairing runs with it.
        func claim(_ key: String) -> Bool {
            lock.withLock { keys.insert(key).inserted }
        }

        func release(_ key: String) {
            _ = lock.withLock { keys.remove(key) }
        }
    }

    /// An endpoint's gate, as ``gateKey(_:)`` spells it.
    private static func gateText(_ endpoint: StationEndpoint) -> String {
        let key = gateKey(endpoint)
        return "direct \(key.host) \(key.port)"
    }

    /// The endpoint as the gate knows it: the host lowercased, without a
    /// trailing dot or the brackets around an IPv6 literal, and the port.
    static func gateKey(_ endpoint: StationEndpoint) -> StationEndpoint {
        endpoint.canonical
    }

    // MARK: The connection

    /// What the connection brings, in order, and the deadline.
    private enum Inbound: Sendable {
        case text(String)
        case closed
        case deadline
    }

    /// The connection's events and the deadline, queued in order for the
    /// pairing to take one at a time. After the connection ends, every
    /// further take is `.closed`.
    private final class Inbox: @unchecked Sendable {
        private let lock = NSLock()
        private var queued: [Inbound] = []
        private var waiter: CheckedContinuation<Inbound, Never>?
        private var finished = false

        func put(_ item: Inbound) {
            let resume: CheckedContinuation<Inbound, Never>? = lock.withLock {
                guard !finished else {
                    return nil
                }
                if let waiting = waiter {
                    waiter = nil
                    return waiting
                }
                queued.append(item)
                return nil
            }
            resume?.resume(returning: item)
        }

        func finish() {
            let resume: CheckedContinuation<Inbound, Never>? = lock.withLock {
                finished = true
                queued.removeAll()
                defer { waiter = nil }
                return waiter
            }
            resume?.resume(returning: .closed)
        }

        func take() async -> Inbound {
            await withCheckedContinuation { continuation in
                let ready: Inbound? = lock.withLock {
                    if !queued.isEmpty {
                        return queued.removeFirst()
                    }
                    if finished {
                        return .closed
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

    /// The next message from the Core that means something here.
    private enum Next {
        case message(LinkMessage)
        case ended(String?)
    }

    private func run(endpoint: StationEndpoint, code: String?) async throws -> PairedStation {
        try Task.checkCancellation()
        let transport = makeTransport(endpoint, .pairing)
        let inbox = Inbox()
        // One clock from the dial: at the deadline the connection closes,
        // which ends an opening still under way as a connection that never
        // opened, and a pairing already open reads the deadline next.
        let deadline = clock.schedule(after: Self.connectDeadline) {
            inbox.put(.deadline)
            transport.close()
        }
        let certificateSHA256: Data
        do {
            // A cancel while dialling closes the opening at once.
            certificateSHA256 = try await withTaskCancellationHandler {
                try await transport.open { event in
                    switch event {
                    case .text(let text):
                        inbox.put(.text(text))
                    case .closed:
                        inbox.put(.closed)
                    case .pong:
                        break
                    }
                }
            } onCancel: {
                transport.close()
            }
        } catch {
            deadline.cancel()
            transport.close()
            inbox.finish()
            if Task.isCancelled {
                throw CancellationError()
            }
            Self.logger.notice("Could not reach the Core to pair: \(String(describing: error), privacy: .public)")
            let denied = (error as? LinkTransportError) == .localNetworkDenied
            throw PairingError.didNotOpen(localNetworkDenied: denied)
        }
        defer {
            deadline.cancel()
            transport.close()
            inbox.finish()
        }
        // A cancelled pairing closes its connection at once, so it lets go
        // of this Core's gate at once rather than at the Core's end or the
        // deadline, and a retry is not refused as a pairing still running.
        do {
            return try await withTaskCancellationHandler {
                try await exchange(endpoint: endpoint, code: code, transport: transport, inbox: inbox,
                                   certificateSHA256: certificateSHA256)
            } onCancel: {
                transport.close()
                inbox.finish()
            }
        } catch {
            if Task.isCancelled {
                throw CancellationError()
            }
            throw error
        }
    }

    /// A code pairing through the remote access service's mailbox on
    /// `nameplate` (link document section 19): the mailbox opened on the
    /// first service that has the number, then `pair.start` and the code
    /// exchange inside its bodies, under the same deadline from the dial.
    private func runMailbox(servers: [RendezvousServer], nameplate: Int, code: String) async throws -> PairedStation {
        let rendezvous = RendezvousClient(servers: servers, clock: clock, transportFactory: makeRendezvousTransport,
                                          answerTimeout: Self.connectDeadline)
        let inbox = Inbox()
        // At the deadline a mailbox still opening ends as not answered in
        // time (with a service's words when one refused); an open one ends
        // as any pairing out of time does.
        let deadline = clock.schedule(after: Self.connectDeadline) {
            inbox.put(.deadline)
            await rendezvous.giveUp()
        }
        do {
            _ = try await withTaskCancellationHandler {
                try await rendezvous.openMailbox(nameplate: nameplate)
            } onCancel: {
                Task { await rendezvous.close() }
            }
        } catch {
            deadline.cancel()
            await rendezvous.close()
            inbox.finish()
            if Task.isCancelled {
                throw CancellationError()
            }
            Self.logger.info("Could not open a pairing mailbox: \(String(describing: error), privacy: .public)")
            throw Self.pairingError(error as? RendezvousError)
        }
        let transport = RendezvousMailboxTransport(client: rendezvous)
        _ = try await transport.open { event in
            switch event {
            case .text(let text):
                inbox.put(.text(text))
            case .closed:
                inbox.put(.closed)
            case .pong:
                break
            }
        }
        defer {
            deadline.cancel()
            transport.close()
            inbox.finish()
        }
        do {
            return try await withTaskCancellationHandler {
                try await mailboxExchange(code: code, transport: transport, inbox: inbox)
            } onCancel: {
                transport.close()
                inbox.finish()
            }
        } catch {
            if Task.isCancelled {
                throw CancellationError()
            }
            throw error
        }
    }

    /// The code exchange in a mailbox: no `hello` either way, `pair.start`
    /// first, and the Core's identity from its sealed box alone.
    private func mailboxExchange(code: String, transport: any LinkTransport, inbox: Inbox) async throws -> PairedStation {
        let publicKey = Base64URL.encode(identity.publicKey)
        // The service never learns a device's name (the rendezvous
        // document, section 1), so the plain pair.start carries only the
        // model's word; the operator's name travels in the sealed box, whose
        // name and kind win at the Core (link section 3.6, step 5).
        guard transport.send(LinkCodec.encode(.pairStart(LinkMessage.PairStart(
            mode: .code, device: LinkMessage.PairDevice(publicKey: publicKey, name: kind.shortName,
                                                        kind: kind.rawValue))))) else {
            throw PairingError.ended(reason: nil)
        }
        let answer = try await codeExchange(code: code, publicKey: publicKey, transport: transport, inbox: inbox)
        // The box was sealed with the key only the code gives, so the key
        // in it is the Core's; its binding is checked against the
        // certificate of this device's first sign-in (link section 19).
        guard let stationKey = Base64URL.decode(answer.identity.publicKey), P256Wire.isCanonicalKey(stationKey),
              Base64URL.decode(answer.identity.certBinding)?.count == P256Wire.signatureLength else {
            Self.logger.warning("The Core's answer through the mailbox could not be read")
            throw PairingError.identityMismatch
        }
        Self.logger.info("Paired with a Core through the remote access service")
        return PairedStation(identityKey: stationKey, label: answer.label, endpoints: [])
    }

    /// The pairing's words for a mailbox that did not open: the service's
    /// own, as sent, when it gave them.
    private static func pairingError(_ error: RendezvousError?) -> PairingError {
        switch error {
        case .nameplateUnknown(let reason)?:
            return .refused(reason: reason, retryAfter: .zero)
        case .refused(_, let reason, let wait)?:
            return .refused(reason: reason, retryAfter: wait)
        case .unreachable?:
            return .ended(reason: unreachableServiceText)
        case .noAnswer?:
            return .ended(reason: noAnswerText)
        default:
            return .ended(reason: nil)
        }
    }

    /// The app's words when no remote access service answered.
    static let unreachableServiceText =
        "The remote access service could not be reached. Check this phone's internet connection and try again."
    /// The app's words when the service did not open the mailbox in time.
    static let noAnswerText =
        "The remote access service did not answer in time. Check this phone's internet connection and try again."

    /// The pairing on an open connection, from the Core's `hello` to its answer.
    private func exchange(endpoint: StationEndpoint, code: String?, transport: any LinkTransport, inbox: Inbox,
                          certificateSHA256: Data) async throws -> PairedStation {
        // The Core's hello: it pairs, and it names its key and binds this
        // connection's certificate with it.
        let hello: LinkMessage.Hello
        switch try await next(inbox) {
        case .message(.hello(let received)):
            hello = received
        case .message:
            throw PairingError.ended(reason: nil)
        case .ended(let reason):
            throw PairingError.ended(reason: reason)
        }
        guard (hello.features?["pairing"] ?? 0) >= 1, let claim = hello.identity,
              let stationKey = Base64URL.decode(claim.publicKey), P256Wire.isCanonicalKey(stationKey),
              let agreed = LinkVersionPolicy.agree(ours: LinkVersionPolicy.supportedMajors,
                                                   theirs: hello.supportedMajors) else {
            Self.logger.info("The Core does not pair with this app")
            throw PairingError.cannotPair
        }
        guard StationTrust.identityVerifies(claim, publicKey: stationKey, certificateSHA256: certificateSHA256) else {
            Self.logger.warning("The Core's identity does not bind this connection's certificate")
            throw PairingError.identityMismatch
        }

        let publicKey = Base64URL.encode(identity.publicKey)
        let ownHello = LinkMessage.Hello(major: agreed, minor: LinkVersionPolicy.minor,
                                         settingsSchema: StationSession.settingsSchema,
                                         peer: StationSession.peerName, majors: LinkVersionPolicy.supportedMajors,
                                         features: LinkFeatures.app)
        guard transport.send(LinkCodec.encode(.hello(ownHello))),
              transport.send(LinkCodec.encode(.pairStart(LinkMessage.PairStart(
            mode: code == nil ? .lan : .code,
            device: LinkMessage.PairDevice(publicKey: publicKey, name: name, kind: kind.rawValue))))) else {
            throw PairingError.ended(reason: nil)
        }

        let answer: PairingBox.StationContents
        if let code {
            answer = try await codeExchange(code: code, publicKey: publicKey, transport: transport, inbox: inbox)
        } else {
            switch try await next(inbox) {
            case .message(.pairAccept(let accept)):
                answer = PairingBox.StationContents(identity: accept.identity, label: accept.label)
            case .message(.pairFail(let fail)):
                throw Self.refused(fail)
            case .message:
                throw PairingError.ended(reason: nil)
            case .ended(let reason):
                throw PairingError.ended(reason: reason)
            }
        }
        // The Core's answer names the key its hello named, binding this
        // connection's certificate.
        guard Base64URL.decode(answer.identity.publicKey) == stationKey,
              StationTrust.identityVerifies(answer.identity, publicKey: stationKey,
                                            certificateSHA256: certificateSHA256) else {
            Self.logger.warning("The Core's answer names another identity than its hello")
            throw PairingError.identityMismatch
        }
        Self.logger.info("Paired with a Core")
        return PairedStation(identityKey: stationKey, label: answer.label, endpoints: [endpoint])
    }

    /// The code pairing, from `pair.start` to the Core's `pair.confirm`.
    private func codeExchange(code: String, publicKey: String, transport: any LinkTransport,
                              inbox: Inbox) async throws -> PairingBox.StationContents {
        let publicData = try await spakeStep(0, bytes: SpakeExchange.publicDataBytes, inbox: inbox)
        guard SpakeExchange.validatesPublicData(publicData) else {
            Self.logger.warning("The Core named password hash settings other than the fixed ones; nothing was hashed")
            throw PairingError.weakHashSettings
        }
        let spake = SpakeExchange(role: .device)
        // The Argon2id hash runs off this actor.
        let response1 = await Task.detached { spake.deviceStep1(publicData: publicData, code: code) }.value
        guard let response1 else {
            throw PairingError.ended(reason: nil)
        }
        guard transport.send(LinkCodec.encode(.pairSpake(LinkMessage.PairSpake(
            step: 1, data: Base64URL.encode(response1))))) else { throw PairingError.ended(reason: nil) }

        let response2 = try await spakeStep(2, bytes: SpakeExchange.response2Bytes, inbox: inbox)
        guard let response3 = spake.deviceStep3(response2: response2) else {
            // The Core holds another code. Say so in place of step 3; the
            // Core burns the code and answers with its own words and wait.
            guard transport.send(LinkCodec.encode(.pairFail(LinkMessage.PairFail(
                reason: Self.wrongCodeText, retryAfterMs: 0)))) else { throw PairingError.ended(reason: nil) }
            // Only the Core's pair.fail says how long until the next code
            // (zero: the fifth burn closed pairing); an end without it is an
            // end, never a wrong code with a wait nobody gave.
            switch try await next(inbox) {
            case .message(.pairFail(let fail)):
                throw PairingError.wrongCode(retryAfter: .milliseconds(fail.retryAfterMs), reason: fail.reason)
            case .ended(let reason?):
                throw PairingError.ended(reason: reason)
            case .ended(nil), .message:
                throw PairingError.ended(reason: Self.endedAfterWrongCodeText)
            }
        }
        guard transport.send(LinkCodec.encode(.pairSpake(LinkMessage.PairSpake(
            step: 3, data: Base64URL.encode(response3))))) else { throw PairingError.ended(reason: nil) }
        let contents = PairingBox.DeviceContents(publicKey: publicKey, name: name, kind: kind.rawValue)
        guard let box = spake.seal(PairingBox.encode(contents)) else {
            throw PairingError.ended(reason: nil)
        }
        guard transport.send(LinkCodec.encode(.pairConfirm(LinkMessage.PairConfirm(
            box: Base64URL.encode(box))))) else { throw PairingError.ended(reason: nil) }

        switch try await next(inbox) {
        case .message(.pairConfirm(let confirm)):
            guard let sealed = Base64URL.decode(confirm.box), let opened = spake.open(sealed),
                  let answer = PairingBox.stationContents(opened) else {
                Self.logger.warning("The Core's confirmation could not be read")
                throw PairingError.identityMismatch
            }
            return answer
        case .message(.pairFail(let fail)):
            throw Self.refused(fail)
        case .message:
            throw PairingError.ended(reason: nil)
        case .ended(let reason):
            throw PairingError.ended(reason: reason)
        }
    }

    /// The Core's `pair.spake` at `step`, its data of `bytes` bytes. A
    /// `pair.fail` is the Core's refusal; anything else ends the pairing.
    private func spakeStep(_ step: Int, bytes: Int,
                           inbox: Inbox) async throws -> Data {
        switch try await next(inbox) {
        case .message(.pairSpake(let spake)):
            guard spake.step == step, let data = Base64URL.decode(spake.data), data.count == bytes else {
                Self.logger.warning("The Core sent a pairing step out of turn")
                throw PairingError.ended(reason: nil)
            }
            return data
        case .message(.pairFail(let fail)):
            throw Self.refused(fail)
        case .message:
            throw PairingError.ended(reason: nil)
        case .ended(let reason):
            throw PairingError.ended(reason: reason)
        }
    }

    /// The next `hello`, `pair.*` or end. Frames it cannot read are
    /// ignored (section 13), and so is any other kind; a `session.end`
    /// ends it with the Core's words. The connect deadline throws
    /// ``PairingError/timedOut``.
    private func next(_ inbox: Inbox) async throws -> Next {
        while true {
            switch await inbox.take() {
            case .closed:
                return .ended(nil)
            case .deadline:
                Self.logger.warning("The Core did not finish pairing in time")
                throw PairingError.timedOut
            case .text(let text):
                let message: LinkMessage
                do {
                    message = try LinkCodec.decode(text)
                } catch {
                    Self.logger.info("Ignoring a message from the Core the app cannot read")
                    continue
                }
                switch message {
                case .sessionEnd(let end):
                    return .ended(end.reason)
                case .hello, .pairAccept, .pairSpake, .pairConfirm, .pairFail:
                    return .message(message)
                default:
                    Self.logger.info("Ignoring a \(message.kind.rawValue, privacy: .public) message while pairing")
                }
            }
        }
    }

    private static func refused(_ fail: LinkMessage.PairFail) -> PairingError {
        .refused(reason: fail.reason, retryAfter: .milliseconds(fail.retryAfterMs))
    }
}
