// NereusSDR for iOS: one pairing connection to the fake Core, playing the Core's side of pairing
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkSessionTestSupport
import NereusLink

/// A connection dialled under the pairing trust to a ``FakeStation``. It
/// plays the Core's side of link document section 3.6: its `hello`
/// declares `pairing` 1 and names the fake's identity, and then one tap or
/// the code, with SPAKE2+EE and the confirmation boxes, the Core's refusals
/// and the close. Events reach the app one at a time and in order.
final class FakePairingConnection: LinkTransport, @unchecked Sendable {
    private enum Phase {
        case awaitingHello
        case awaitingStart
        case hashing
        case awaitingStep1
        case awaitingStep3
        case awaitingConfirm
        case done
    }

    /// The kinds a Core accepts.
    static let kinds: Set<String> = ["phone", "tablet", "computer"]

    private weak var station: FakeStation?
    private let lock = NSLock()
    private var queue: AsyncStream<LinkTransportEvent>.Continuation?
    private var pump: Task<Void, Never>?
    private var phase = Phase.awaitingHello
    private var device: LinkMessage.PairDevice?
    private var code = ""
    private var stored = Data()
    private let spake = SpakeExchange(role: .station)

    /// Through a mailbox on the remote access service there is no `hello`
    /// either way: `pair.start` comes first (link document section 19).
    private let mailbox: Bool

    init(station: FakeStation, mailbox: Bool = false) {
        self.station = station
        self.mailbox = mailbox
        if mailbox {
            phase = .awaitingStart
        }
    }

    deinit {
        queue?.finish()
        pump?.cancel()
    }

    // MARK: LinkTransport

    func open(onEvent: @escaping @Sendable (LinkTransportEvent) async -> Void) async throws -> Data {
        guard let station else {
            throw LinkTransportError.failed("the fake is gone")
        }
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
        if mailbox {
            return Data()
        }
        let hello = LinkMessage.Hello(major: 1, minor: LinkVersionPolicy.minor, settingsSchema: 0, peer: "nereusd",
                                      majors: [1], features: ["deviceAuth": 1, "pairing": 1],
                                      identity: try station.identity.claim(certificateSHA256: station.certificateSHA256),
                                      challenge: TestStationIdentity.newChallenge())
        enqueue(.hello(hello))
        return station.certificateSHA256
    }

    @discardableResult func send(_ text: String) -> Bool {
        guard let station, lock.withLock({ () -> Bool in
            guard queue != nil else { return false }
            if case .done = phase { return false }
            return true
        }) else { return false }
        guard let message = station.record(text) else { return true }
        let current = lock.withLock { phase }
        switch (current, message) {
        case (.awaitingHello, .hello):
            lock.withLock { phase = .awaitingStart }
        case (.awaitingStart, .pairStart(let start)):
            begin(start, station: station)
        case (.awaitingStep1, .pairSpake(let step)) where step.step == 1:
            answerStep1(step, station: station)
        case (.awaitingStep3, .pairSpake(let step)) where step.step == 3:
            checkStep3(step, station: station)
        case (.awaitingStep3, .pairFail):
            // The app's step 3 failed: the code is burned.
            refuse(station.burn(reason: FakeStation.wrongCodeReason))
        case (.awaitingConfirm, .pairConfirm(let confirm)):
            confirmDevice(confirm, station: station)
        default:
            end(LinkMessage.SessionEnd(reason: "The Core could not read what this app sent.", retryable: false,
                                       code: "protocolError"))
        }
        return true
    }

    func ping() {
        enqueueEvent(.pong)
    }

    func close() {
        let heldCode: Bool = lock.withLock {
            defer {
                phase = .done
                queue?.finish()
            }
            return phase == .awaitingStep3 || phase == .awaitingConfirm
        }
        // The connection ended after its step 1 took the code: burned.
        if heldCode {
            _ = station?.burn(reason: FakeStation.wrongCodeReason)
        }
    }

    // MARK: The Core's side

    private func begin(_ start: LinkMessage.PairStart, station: FakeStation) {
        guard let key = Base64URL.decode(start.device.publicKey), P256Wire.isCanonicalKey(key),
              DeviceName.isUsable(start.device.name),
              Self.kinds.contains(start.device.kind) else {
            refuse(LinkMessage.PairFail(reason: FakeStation.unreadableDeviceReason, retryAfterMs: 0))
            return
        }
        switch start.mode {
        case .lan:
            if let refusal = station.oneTapRefusal(key: key) {
                refuse(refusal)
                return
            }
            station.addPairedDevice(key)
            enqueue(.pairAccept(LinkMessage.PairAccept(identity: stationClaim(station), label: station.label)))
            finish()
        case .code:
            let code: String
            switch station.codeForStart() {
            case .success(let shown):
                code = shown
            case .failure(let refusal):
                refuse(refusal.fail)
                return
            }
            lock.withLock {
                device = start.device
                self.code = code
                phase = .hashing
            }
            // The Core hashes the code off its event loop, then sends step 0.
            Task.detached { [self] in
                guard let stored = SpakeExchange.storedData(code: code),
                      let publicData = spake.stationStep0(stored: stored) else {
                    refuse(LinkMessage.PairFail(reason: FakeStation.cannotPairReason, retryAfterMs: 0))
                    return
                }
                let current: Phase = lock.withLock {
                    self.stored = stored
                    if phase == .hashing {
                        phase = .awaitingStep1
                    }
                    return phase
                }
                guard current == .awaitingStep1 else {
                    return
                }
                enqueue(.pairSpake(LinkMessage.PairSpake(step: 0, data: Base64URL.encode(publicData))))
            }
        }
    }

    private func answerStep1(_ step: LinkMessage.PairSpake, station: FakeStation) {
        // The Core takes the code here; from now on it pairs or burns.
        let (stored, code) = lock.withLock { (self.stored, self.code) }
        if let refusal = station.takeCode(code, holder: ObjectIdentifier(self)) {
            refuse(refusal)
            return
        }
        guard let response1 = Base64URL.decode(step.data),
              let response2 = spake.stationStep2(stored: stored, response1: response1) else {
            refuse(station.burn(reason: FakeStation.malformedStep1Reason))
            return
        }
        lock.withLock { phase = .awaitingStep3 }
        enqueue(.pairSpake(LinkMessage.PairSpake(step: 2, data: Base64URL.encode(response2))))
    }

    private func checkStep3(_ step: LinkMessage.PairSpake, station: FakeStation) {
        guard let response3 = Base64URL.decode(step.data), spake.stationStep4(response3: response3) else {
            refuse(station.burn(reason: FakeStation.wrongCodeReason))
            return
        }
        lock.withLock { phase = .awaitingConfirm }
    }

    private func confirmDevice(_ confirm: LinkMessage.PairConfirm, station: FakeStation) {
        let started = lock.withLock { device }
        guard let box = Base64URL.decode(confirm.box), let opened = spake.open(box),
              let contents = PairingBox.deviceContents(opened), contents.publicKey == started?.publicKey,
              let key = Base64URL.decode(contents.publicKey), P256Wire.isCanonicalKey(key),
              DeviceName.isUsable(contents.name), Self.kinds.contains(contents.kind) else {
            refuse(station.burn(reason: FakeStation.unreadableDeviceReason))
            return
        }
        station.addPairedDevice(key)
        station.codePaired()
        let answer = PairingBox.StationContents(identity: stationClaim(station), label: station.label)
        guard let sealed = spake.seal(PairingBox.encode(answer)) else {
            refuse(LinkMessage.PairFail(reason: FakeStation.cannotSaveReason, retryAfterMs: 0))
            return
        }
        enqueue(.pairConfirm(LinkMessage.PairConfirm(box: Base64URL.encode(sealed))))
        finish()
    }

    private func stationClaim(_ station: FakeStation) -> LinkMessage.StationIdentityClaim {
        // The fake's own key signs its own digest; it cannot fail.
        (try? station.identity.claim(certificateSHA256: station.certificateSHA256))
            ?? LinkMessage.StationIdentityClaim(publicKey: "", certBinding: "")
    }

    // MARK: Sending and ending

    private func refuse(_ fail: LinkMessage.PairFail) {
        enqueue(.pairFail(fail))
        finish()
    }

    private func end(_ end: LinkMessage.SessionEnd) {
        enqueue(.sessionEnd(end))
        finish()
    }

    /// The Core closes the connection after its last message.
    private func finish() {
        lock.withLock { phase = .done }
        enqueueEvent(.closed)
        station?.connectionChanged()
    }

    private func enqueue(_ message: LinkMessage) {
        enqueueEvent(.text(LinkCodec.encode(message)))
    }

    private func enqueueEvent(_ event: LinkTransportEvent) {
        _ = lock.withLock { queue }?.yield(event)
    }
}
