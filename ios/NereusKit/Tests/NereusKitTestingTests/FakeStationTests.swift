// NereusSDR for iOS: the fake Core signs the app in, sends its snapshot and records what the app sends
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkSessionTestSupport
import NereusKitTesting
import NereusLink
import NereusMirror
import Testing

@Suite("FakeStation")
struct FakeStationTests {
    /// A clock that never moves, for the pairing clients: the connect
    /// deadline is not what these tests check, and both sides hash the code
    /// with Argon2id (64 MiB), which on a busy Mac can outlast the 30 second
    /// wall-clock deadline and end a pairing that would have finished.
    private static var noDeadline: ManualLinkClock { ManualLinkClock() }

    /// Feeds a session's events to a mirror, in order, until the session goes.
    @MainActor
    private static func feed(_ session: StationSession, into mirror: MirrorStore) -> Task<Void, Never> {
        Task { @MainActor in
            for await event in session.events {
                mirror.handle(event)
            }
        }
    }

    /// Waits, without sleeping, until the session is in `wanted`.
    private static func waitForState(_ session: StationSession, _ wanted: StationSession.State) async -> Bool {
        for _ in 0..<20_000 {
            if await session.state == wanted {
                return true
            }
            await Task.yield()
        }
        return await session.state == wanted
    }

    @Test("a session to the fake signs in and receives the whole snapshot")
    @MainActor
    func connectsToReady() async throws {
        let station = try FakeStation()
        let session = station.makeSession()
        let mirror = MirrorStore(session: session)
        let feeding = Self.feed(session, into: mirror)
        defer { feeding.cancel() }

        await session.connect()
        #expect(await station.waitUntilLive())
        #expect(await Self.waitForState(session, .ready))
        #expect(await session.agreedMajor == 1)

        // The app sent its hello and signed in with the fake's token.
        let kinds = station.messages.map(\.kind)
        #expect(kinds == [.hello, .authRequest])
        #expect(station.messages.contains(.authRequest(LinkMessage.AuthRequest(token: station.token))))

        // The mirror holds the fixture's Core: its radio, its slice, complete.
        let complete = await Self.poll { mirror.isSnapshotComplete }
        #expect(complete)
        #expect(mirror.capabilities["stationName"] == .text("ConnectableRadioModel fake"))
        #expect(mirror.object("radio")?.className == "RadioModel")
        #expect(mirror.objects(ofClass: "SliceModel").map(\.key) == ["slice:0"])
        await session.disconnect()
    }

    @Test("a sign-in with another token is refused")
    func wrongTokenIsRefused() async throws {
        let station = try FakeStation()
        let session = StationSession(endpoint: station.endpoint, trust: station.trust,
                                     authenticator: TokenAuthenticator(token: "not-the-token"),
                                     transportFactory: station.transportFactory)
        let refusal = Task { () -> Refusal? in
            for await event in session.events {
                if case .refused(let refusal) = event {
                    return refusal
                }
            }
            return nil
        }
        await session.connect()
        let refused = await refusal.value
        #expect(refused == Refusal(.authentication(FakeStation.wrongTokenReason)))
        #expect(!station.isLive)
        await session.disconnect()
    }

    @Test("a paired device signs in to the fake by its key, trusting the fake's identity")
    func signsInByDeviceKey() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        let device = try DeviceIdentity.load(store: InMemoryKeyStore())
        let session = StationSession(endpoint: station.endpoint, trust: station.identityTrust,
                                     authenticator: try DeviceKeyAuthenticator(identity: device, name: "Shack iPhone",
                                                                               kind: .phone),
                                     transportFactory: station.transportFactory)
        await session.connect()
        #expect(await station.waitUntilLive())
        #expect(await Self.waitForState(session, .ready))
        let request = station.messages.compactMap { message -> LinkMessage.AuthRequest? in
            if case .authRequest(let request) = message {
                return request
            }
            return nil
        }.first
        #expect(request?.device?.id == device.id)
        await session.disconnect()
    }

    @Test("a device key that trusts another Core never signs in to the fake")
    func refusesAnotherIdentity() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        let device = try DeviceIdentity.load(store: InMemoryKeyStore())
        let session = StationSession(endpoint: station.endpoint, trust: .identity(publicKey: Data(repeating: 4, count: 91)),
                                     authenticator: try DeviceKeyAuthenticator(identity: device, name: "Shack iPhone",
                                                                               kind: .phone),
                                     transportFactory: station.transportFactory)
        let refusal = Task { () -> Refusal? in
            for await event in session.events {
                if case .refused(let refusal) = event {
                    return refusal
                }
            }
            return nil
        }
        await session.connect()
        #expect(await refusal.value?.code == .identityChanged)
        #expect(station.messages.isEmpty)
        await session.disconnect()
    }

    @Test("once live, the fake records what the app sends and delivers what the test sends")
    @MainActor
    func recordsAndDelivers() async throws {
        let station = try FakeStation()
        let session = station.makeSession()
        let mirror = MirrorStore(session: session)
        let feeding = Self.feed(session, into: mirror)
        defer { feeding.cancel() }
        await session.connect()
        #expect(await station.waitUntilLive())
        #expect(await Self.waitForState(session, .ready))

        let write = LinkMessage.propertyWrite(LinkMessage.PropertyWrite(
            key: "slice:0", properties: [LinkMessage.PropertyEntry(ordinal: 1, name: "frequency", value: .f64(14_074_000))]))
        try await session.send(write)
        let sent = await station.waitForMessage { $0 == write }
        #expect(sent == write)

        await station.deliver(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            LinkMessage.PropertyEntry(ordinal: 1, name: "frequency", value: .f64(14_074_000)),
        ])))
        let tuned = await Self.poll { mirror.object("slice:0")?["frequency"] == .double(14_074_000) }
        #expect(tuned)

        await station.dropLink()
        let stale = await Self.poll { mirror.isStale }
        #expect(stale)
        await session.disconnect()
    }

    @Test("an unknown fixture is an error")
    func unknownFixture() {
        #expect(throws: (any Error).self) {
            _ = try FakeStation(fixture: "no-such-fixture")
        }
    }

    /// Waits, without sleeping, until `condition` holds. Each turn lets the
    /// main actor run what is queued on it, where the mirror is fed.
    @MainActor
    private static func poll(_ condition: @MainActor () -> Bool) async -> Bool {
        for _ in 0..<20_000 {
            if condition() {
                return true
            }
            await Task.yield()
        }
        return condition()
    }

    // MARK: Pairing

    /// A session that signs in by `device`'s key, trusting `paired`.
    private static func deviceSession(_ station: FakeStation, _ device: DeviceIdentity,
                                      trusting paired: PairedStation) throws -> StationSession {
        StationSession(endpoint: station.endpoint, trust: paired.trust,
                       authenticator: try DeviceKeyAuthenticator(identity: device, name: "Shack iPhone", kind: .phone),
                       transportFactory: station.transportFactory)
    }

    private static func firstRefusal(_ session: StationSession) -> Task<Refusal?, Never> {
        Task {
            for await event in session.events {
                if case .refused(let refusal) = event {
                    return refusal
                }
            }
            return nil
        }
    }

    @Test("the app pairs with the fake by its code, then signs in by key on a new connection")
    func pairsByCodeThenSignsIn() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in", requiresPairing: true)
        let device = try DeviceIdentity.load(store: InMemoryKeyStore())
        let client = try PairingClient(identity: device, name: "Shack iPhone", kind: .phone, clock: Self.noDeadline,
                                       transportFactory: station.transportFactory)
        let paired = try await client.pair(code: station.pairingCode, via: .direct(station.endpoint))
        #expect(paired.identityKey == station.identity.publicKey)
        #expect(paired.label == station.label)
        #expect(station.pairedDeviceKeys == [device.publicKey])
        #expect(station.pairingConnectionCount == 1)
        #expect(station.connectionCount == 0)
        let kinds = station.messages.map(\.kind)
        #expect(kinds == [.hello, .pairStart, .pairSpake, .pairSpake, .pairConfirm])

        let session = try Self.deviceSession(station, device, trusting: paired)
        await session.connect()
        #expect(await station.waitUntilLive())
        #expect(await Self.waitForState(session, .ready))
        await session.disconnect()
    }

    /// The Core's order (src/core/session/StationServer.cpp:6829-6870): a
    /// device already paired first, then a closed window, then one tap's
    /// own refusals. The first device's pairing closes the window
    /// (PairingWindow::followDevices, ClosedClaimed), so a second device is
    /// told the Core is not taking new devices; with pairing open again for
    /// one more, it is told one tap pairs only an unclaimed Core.
    @Test("the app pairs with the fake by one tap, and one tap then refuses a second device in the Core's order")
    func pairsByOneTap() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in", requiresPairing: true)
        let device = try DeviceIdentity.load(store: InMemoryKeyStore())
        let client = try PairingClient(identity: device, name: "Shack iPhone", kind: .phone, clock: Self.noDeadline,
                                       transportFactory: station.transportFactory)
        let paired = try await client.pairOnThisNetwork(endpoint: station.endpoint)
        #expect(paired.identityKey == station.identity.publicKey)
        #expect(station.pairedDeviceKeys == [device.publicKey])

        let second = try PairingClient(identity: try DeviceIdentity.load(store: InMemoryKeyStore()),
                                       name: "Shack iPad", kind: .tablet, clock: Self.noDeadline,
                                       transportFactory: station.transportFactory)
        station.reachedOnItsOwnNetwork = false
        await #expect(throws: PairingError.refused(reason: FakeStation.windowClosedReason, retryAfter: .zero)) {
            try await second.pairOnThisNetwork(endpoint: station.endpoint)
        }
        station.openPairing()
        await #expect(throws: PairingError.refused(reason: FakeStation.oneTapClaimedReason, retryAfter: .zero)) {
            try await second.pairOnThisNetwork(endpoint: station.endpoint)
        }
        #expect(station.pairedDeviceKeys.count == 1)
    }

    /// A device the Core already has is refused one tap before anything
    /// else (src/core/session/StationServer.cpp:6829-6835), whether pairing
    /// is open or not; a code pairing is not refused for it.
    @Test("one tap from a device already paired is refused with the Core's words, before any other refusal")
    func oneTapFromAPairedDevice() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in", requiresPairing: true)
        let device = try DeviceIdentity.load(store: InMemoryKeyStore())
        let client = try PairingClient(identity: device, name: "Shack iPhone", kind: .phone, clock: Self.noDeadline,
                                       transportFactory: station.transportFactory)
        _ = try await client.pairOnThisNetwork(endpoint: station.endpoint)
        #expect(FakeStation.alreadyPairedReason
                    == "This device is already paired with this Core. Connect to it instead.")
        await #expect(throws: PairingError.refused(reason: FakeStation.alreadyPairedReason, retryAfter: .zero)) {
            try await client.pairOnThisNetwork(endpoint: station.endpoint)
        }
        station.openPairing()
        station.allowsOneTap = false
        await #expect(throws: PairingError.refused(reason: FakeStation.alreadyPairedReason, retryAfter: .zero)) {
            try await client.pairOnThisNetwork(endpoint: station.endpoint)
        }
        #expect(station.pairedDeviceKeys == [device.publicKey])
    }

    @Test("a wrong code burns it: the fake asks for the wait, and pairs nothing")
    func wrongCodeBurns() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in", requiresPairing: true)
        let client = try PairingClient(identity: try DeviceIdentity.load(store: InMemoryKeyStore()),
                                       name: "Shack iPhone", kind: .phone, clock: Self.noDeadline,
                                       transportFactory: station.transportFactory)
        let code = station.pairingCode
        let parts = code.split(separator: "-").map(String.init)
        let words = PairingCodeText.words
        let swapped = words[((words.firstIndex(of: parts[1]) ?? 0) + 1) % words.count]
        await #expect(throws: PairingError.wrongCode(retryAfter: .milliseconds(5000),
                                                     reason: FakeStation.wrongCodeReason)) {
            try await client.pair(code: "\(parts[0])-\(swapped)-\(parts[2])", via: .direct(station.endpoint))
        }
        #expect(station.pairedDeviceKeys.isEmpty)
        // Compared apart from the expectation, so a failure never prints a code.
        let burned = station.pairingCode != code
        #expect(burned)
    }

    @Test("the fake refuses a device key it has not paired")
    func refusesAnUnpairedKey() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in", requiresPairing: true)
        let device = try DeviceIdentity.load(store: InMemoryKeyStore())
        let paired = PairedStation(identityKey: station.identity.publicKey, label: "", endpoints: [station.endpoint])
        let session = try Self.deviceSession(station, device, trusting: paired)
        let refusal = Self.firstRefusal(session)
        await session.connect()
        let refused = await refusal.value
        #expect(refused?.code == .deviceNotPaired)
        #expect(refused == Refusal(.authentication(FakeStation.deviceNotPairedReason), code: .deviceNotPaired))
        #expect(!station.isLive)
        await session.disconnect()
    }

    // MARK: Pairing as the Core pairs

    /// `code` with its first word swapped for the next word in the list.
    private static func wrong(_ code: String) -> String {
        let parts = code.split(separator: "-").map(String.init)
        let words = PairingCodeText.words
        let index = words.firstIndex(of: parts[1]) ?? 0
        return "\(parts[0])-\(words[(index + 1) % words.count])-\(parts[2])"
    }

    /// A wrong code against `station`, returning how the pairing ended.
    private static func pairWrongly(_ station: FakeStation, code: String) async throws -> PairingError? {
        let client = try PairingClient(identity: try DeviceIdentity.load(store: InMemoryKeyStore()),
                                       name: "Shack iPhone", kind: .phone, clock: Self.noDeadline,
                                       transportFactory: station.transportFactory)
        do {
            _ = try await client.pair(code: wrong(code), via: .direct(station.endpoint))
            return nil
        } catch {
            return error as? PairingError
        }
    }

    @Test("burned codes wait 5, 10, 20 and 40 seconds, and the fifth closes pairing")
    func burnedCodesWaitLongerEachTime() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in", requiresPairing: true)
        let reason = FakeStation.wrongCodeReason
        // Results are taken apart from the expectations, so no failure prints a code.
        let first = station.pairingCode
        let burnedFirst = try await Self.pairWrongly(station, code: first)
        #expect(burnedFirst == .wrongCode(retryAfter: .milliseconds(5000), reason: reason))
        // No code is shown during the wait, and a code pairing is refused with it.
        let hiddenDuringWait = station.pairingCode.isEmpty
        #expect(hiddenDuringWait)
        let during = try await Self.pairWrongly(station, code: first)
        guard case .refused(let words, let retryAfter)? = during else {
            Issue.record("a code pairing during the wait was not refused: \(String(describing: during))")
            return
        }
        #expect(words == FakeStation.waitingReason)
        #expect(retryAfter > .zero && retryAfter <= .milliseconds(5000))

        for wait in [10_000, 20_000, 40_000] {
            station.endPairingWait()
            let burned = try await Self.pairWrongly(station, code: station.pairingCode)
            #expect(burned == .wrongCode(retryAfter: .milliseconds(wait), reason: reason))
        }
        station.endPairingWait()
        let fifth = station.pairingCode
        let burnedFifth = try await Self.pairWrongly(station, code: fifth)
        #expect(burnedFifth == .wrongCode(retryAfter: .zero, reason: reason))
        let hiddenWhenClosed = station.pairingCode.isEmpty
        #expect(hiddenWhenClosed)
        let whileClosed = try await Self.pairWrongly(station, code: fifth)
        #expect(whileClosed == .refused(reason: FakeStation.windowClosedReason, retryAfter: .zero))
        station.openPairing()
        let shownWhenOpened = !station.pairingCode.isEmpty
        #expect(shownWhenOpened)
    }

    /// One pairing connection to the fake driven by hand, up to its step 1.
    private final class HandPairing: @unchecked Sendable {
        let transport: any LinkTransport
        private let lock = NSLock()
        private var received: [LinkMessage] = []

        init(_ station: FakeStation) async throws {
            transport = station.transportFactory(station.endpoint, .pairing)
            _ = try await transport.open { [weak self] event in
                if case .text(let text) = event, let message = try? LinkCodec.decode(text) {
                    self?.lock.withLock { self?.received.append(message) }
                }
            }
        }

        var messages: [LinkMessage] { lock.withLock { received } }

        /// Waits for the fake's message matching `wanted`.
        func wait(for wanted: (LinkMessage) -> Bool) async throws -> LinkMessage? {
            for _ in 0..<3000 {
                if let found = messages.first(where: wanted) {
                    return found
                }
                try await Task.sleep(for: .milliseconds(10))
            }
            return nil
        }

        /// Sends the hello and a code pair.start, answers step 0 with a step 1
        /// for `code`, and returns the fake's answer to it, or its refusal of
        /// the start.
        func stepOne(code: String) async throws -> LinkMessage? {
            let device = try DeviceIdentity.load(store: InMemoryKeyStore())
            transport.send(LinkCodec.encode(.hello(LinkMessage.Hello(major: 1, minor: 11, settingsSchema: 0,
                                                                     peer: "hand", majors: [1],
                                                                     features: ["deviceAuth": 1]))))
            transport.send(LinkCodec.encode(.pairStart(LinkMessage.PairStart(
                mode: .code, device: LinkMessage.PairDevice(publicKey: Base64URL.encode(device.publicKey),
                                                            name: "Hand iPhone", kind: "phone")))))
            let first = try await wait(for: { $0.kind == .pairSpake || $0.kind == .pairFail })
            if case .pairFail? = first {
                // Refused at its start: another exchange holds the code.
                return first
            }
            guard case .pairSpake(let step0)? = first,
                  let publicData = Base64URL.decode(step0.data),
                  let response1 = SpakeExchange(role: .device).deviceStep1(publicData: publicData, code: code) else {
                return nil
            }
            let before = messages.count
            transport.send(LinkCodec.encode(.pairSpake(LinkMessage.PairSpake(step: 1,
                                                                             data: Base64URL.encode(response1)))))
            for _ in 0..<3000 where messages.count == before {
                try await Task.sleep(for: .milliseconds(10))
            }
            return messages.count > before ? messages.last : nil
        }
    }

    @Test("step 1 takes the code for one exchange, and a connection that ends after it burns the code")
    func stepOneTakesTheCode() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in", requiresPairing: true)
        let code = station.pairingCode
        let holder = try await HandPairing(station)
        let other = try await HandPairing(station)
        guard case .pairSpake(let step2)? = try await holder.stepOne(code: code) else {
            Issue.record("the first exchange's step 1 was not answered with step 2")
            return
        }
        #expect(step2.step == 2)
        // The other exchange starts after the code was taken, so the Core
        // refuses it at its start, as it would at its step 1.
        let refused = try await other.stepOne(code: code)
        #expect(refused == .pairFail(LinkMessage.PairFail(reason: FakeStation.anotherDeviceReason,
                                                           retryAfterMs: FakeStation.anotherDeviceRetryMs)))
        // While an exchange holds the code, the Core shows none, and a new
        // pairing is refused before its step 0.
        let heldHidden = station.pairingCode.isEmpty
        #expect(heldHidden)
        let refusedWhileHeld = try await Self.pairWrongly(station, code: code)
        #expect(refusedWhileHeld == .refused(reason: FakeStation.anotherDeviceReason,
                                             retryAfter: .milliseconds(FakeStation.anotherDeviceRetryMs)))

        // The holder goes away before step 3: the code is burned, and the wait begins.
        holder.transport.close()
        let hidden = station.pairingCode.isEmpty
        #expect(hidden)
        let refusedDuringWait = try await Self.pairWrongly(station, code: code)?.isRefusal == true
        #expect(refusedDuringWait)
        station.endPairingWait()
        let next = station.pairingCode
        let shown = !next.isEmpty
        #expect(shown)
        let changed = next != code
        #expect(changed)
    }
}

private extension PairingError {
    var isRefusal: Bool {
        if case .refused = self {
            return true
        }
        return false
    }
}
