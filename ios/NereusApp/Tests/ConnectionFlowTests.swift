// NereusSDR for iOS: the connecting flow against the fake Core: an address, a code, the band, and every way it goes wrong
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import CryptoKit
import Foundation
import Network
import NereusKitTesting
import NereusLink
import NereusMedia
@testable import NereusSDR
import Testing

/// R-IOS-16, R-IOS-08, D19, D23, D36, D65, D69, D70, D71, D72: ``ConnectionFlow``
/// driven against ``FakeStation``. Codes and keys are made at run time;
/// nothing dials a real Core.
@Suite("Connection flow", .serialized)
@MainActor
struct ConnectionFlowTests {
    // MARK: The harness

    /// The microphone question, answered by the test.
    final class FakeMicrophone: MicrophoneAccess {
        var needsAsking = true
        private(set) var asked = 0

        func ask() async -> Bool {
            asked += 1
            needsAsking = false
            return true
        }
    }

    /// The phone's network, switched by the test.
    final class FakeNetwork: NetworkWatch {
        private var changed: (@MainActor (NetworkPath) -> Void)?

        func start(_ changed: @escaping @MainActor (NetworkPath) -> Void) {
            self.changed = changed
        }

        /// Online over Wi-Fi, or offline.
        func set(online: Bool) {
            set(online ? NetworkPath(online: true, interfaces: ["en0"]) : .offline)
        }

        func set(_ path: NetworkPath) {
            changed?(path)
        }
    }

    /// Whether a new connection reaches the Core: it does, it fails at once
    /// as with no network, or it hangs until closed.
    final class Reach: @unchecked Sendable {
        enum Way {
            case reaches
            case fails
            case hangs
        }

        private let lock = NSLock()
        private var current = Way.reaches

        var way: Way {
            get { lock.withLock { current } }
            set { lock.withLock { current = newValue } }
        }
    }

    /// A connection whose opening never answers until the app closes it.
    final class HangingTransport: LinkTransport, @unchecked Sendable {
        private let lock = NSLock()
        private var waiting: CheckedContinuation<Data, Error>?
        private var closed = false

        func open(onEvent: @escaping @Sendable (LinkTransportEvent) async -> Void) async throws -> Data {
            try await withCheckedThrowingContinuation { continuation in
                let already = lock.withLock { () -> Bool in
                    if closed {
                        return true
                    }
                    waiting = continuation
                    return false
                }
                if already {
                    continuation.resume(throwing: LinkTransportError.failed("closed"))
                }
            }
        }

        @discardableResult func send(_ text: String) -> Bool { false }
        func ping() {}

        func close() {
            let pending = lock.withLock { () -> CheckedContinuation<Data, Error>? in
                closed = true
                let taken = waiting
                waiting = nil
                return taken
            }
            pending?.resume(throwing: LinkTransportError.failed("closed"))
        }
    }

    /// What the connections the flow made did, in order.
    final class Log: @unchecked Sendable {
        private let lock = NSLock()
        private var entries: [String] = []

        func add(_ entry: String) {
            lock.withLock { entries.append(entry) }
        }

        var all: [String] { lock.withLock { entries } }
    }

    /// A connection passed through, its opening and closing logged.
    struct LoggedTransport: LinkTransport {
        let inner: any LinkTransport
        let name: String
        let log: Log

        func open(onEvent: @escaping @Sendable (LinkTransportEvent) async -> Void) async throws -> Data {
            log.add("open \(name)")
            return try await inner.open(onEvent: onEvent)
        }

        @discardableResult func send(_ text: String) -> Bool {
            inner.send(text)
        }

        func ping() {
            log.add("ping \(name)")
            inner.ping()
        }

        func close() {
            log.add("close \(name)")
            inner.close()
        }
    }

    /// A connection that never opens: a Core that isn't answering.
    struct DeadTransport: LinkTransport {
        let error: LinkTransportError

        func open(onEvent: @escaping @Sendable (LinkTransportEvent) async -> Void) async throws -> Data {
            throw error
        }

        @discardableResult func send(_ text: String) -> Bool { false }
        func ping() {}
        func close() {}
    }

    /// A connection that opens and then says nothing: a Core that doesn't
    /// answer in time.
    struct SilentTransport: LinkTransport {
        func open(onEvent: @escaping @Sendable (LinkTransportEvent) async -> Void) async throws -> Data {
            Data(repeating: 7, count: 32)
        }
        @discardableResult func send(_ text: String) -> Bool { false }
        func ping() {}
        func close() {}
    }

    /// A dial whose completion can arrive after the session has retired it.
    final class HeldOpen: LinkTransport, @unchecked Sendable {
        private let lock = NSLock()
        private var waiting: CheckedContinuation<Data, Error>?
        private var started = false
        var hasStarted: Bool { lock.withLock { started } }
        func open(onEvent: @escaping @Sendable (LinkTransportEvent) async -> Void) async throws -> Data {
            try await withCheckedThrowingContinuation { continuation in
                lock.withLock {
                    started = true
                    waiting = continuation
                }
            }
        }
        func finish() {
            let waiter = lock.withLock { () -> CheckedContinuation<Data, Error>? in
                let taken = waiting
                waiting = nil
                return taken
            }
            waiter?.resume(returning: Data(repeating: 7, count: 32))
        }
        @discardableResult func send(_ text: String) -> Bool { false }
        func ping() {}
        func close() {}
    }

    actor SnapshotGate {
        private var waiting: CheckedContinuation<Void, Never>?
        private var entered = false
        var hasEntered: Bool { entered }
        func hold() async {
            entered = true
            await withCheckedContinuation { waiting = $0 }
        }
        func release() {
            waiting?.resume()
            waiting = nil
        }
    }

    actor DisconnectGate {
        private var calls = 0
        private var held: [Int: CheckedContinuation<Void, Never>] = [:]
        func entered(_ number: Int) -> Bool { calls >= number }
        func hold() async {
            calls += 1
            let number = calls
            await withCheckedContinuation { held[number] = $0 }
        }
        func release(_ number: Int) {
            held.removeValue(forKey: number)?.resume()
        }
    }

    final class FailingStationItem: SecretItem, @unchecked Sendable {
        let backing = InMemorySecretItem()
        var failRemoval = false
        func read() throws -> Data? { try backing.read() }
        func add(_ data: Data) throws -> Bool { try backing.add(data) }
        func write(_ data: Data) throws {
            if failRemoval { throw Failure.denied }
            try backing.write(data)
        }
        func delete() throws {
            if failRemoval { throw Failure.denied }
            try backing.delete()
        }
        enum Failure: Error { case denied }
    }

    /// Suspends after the verified Core hello, before credentials can leave.
    actor HeldAuthenticator: StationAuthenticator {
        nonisolated let signsWithDeviceKey = true
        private var waiting: CheckedContinuation<Void, Never>?
        private var entered = false
        var hasEntered: Bool { entered }

        func authRequest(stationHello: LinkMessage.Hello, certificateSHA256: Data) async throws
            -> LinkMessage.AuthRequest {
            entered = true
            await withCheckedContinuation { waiting = $0 }
            return LinkMessage.AuthRequest(token: "should-never-be-sent")
        }

        func release() {
            waiting?.resume()
            waiting = nil
        }
    }

    /// Signs the first session normally, then suspends a Reconnect sign-in.
    actor HeldReconnectAuthenticator: StationAuthenticator {
        nonisolated let signsWithDeviceKey = true
        let signer: DeviceKeyAuthenticator
        private var calls = 0
        private var waiting: CheckedContinuation<Void, Never>?
        private var entered = false
        var hasEntered: Bool { entered }

        init(signer: DeviceKeyAuthenticator) { self.signer = signer }

        func authRequest(stationHello: LinkMessage.Hello, certificateSHA256: Data) async throws
            -> LinkMessage.AuthRequest {
            calls += 1
            if calls == 2 {
                entered = true
                await withCheckedContinuation { waiting = $0 }
            }
            return try await signer.authRequest(stationHello: stationHello,
                                                certificateSHA256: certificateSHA256)
        }

        func release() {
            waiting?.resume()
            waiting = nil
        }
    }

    struct SnapshotHeldTransport: LinkTransport {
        let inner: any LinkTransport
        let gate: SnapshotGate
        func open(onEvent: @escaping @Sendable (LinkTransportEvent) async -> Void) async throws -> Data {
            try await inner.open { event in
                if case .text(let text) = event,
                   let message = try? LinkCodec.decode(text), case .snapshotComplete = message {
                    await gate.hold()
                }
                await onEvent(event)
            }
        }
        @discardableResult func send(_ text: String) -> Bool { inner.send(text) }
        func ping() { inner.ping() }
        func close() { inner.close() }
    }

    /// Refuses a local text submission while its close notification waits
    /// outside the session actor, as a physical link can do during teardown.
    final class RefusingBeforeCloseTransport: LinkTransport, @unchecked Sendable {
        let inner: any LinkTransport
        let closeGate: SnapshotGate
        private let lock = NSLock()
        private var refusing = false
        private var rejected = 0

        init(inner: any LinkTransport, closeGate: SnapshotGate) {
            self.inner = inner
            self.closeGate = closeGate
        }

        var refusedCount: Int { lock.withLock { rejected } }
        func refuseText() { lock.withLock { refusing = true } }

        func open(onEvent: @escaping @Sendable (LinkTransportEvent) async -> Void) async throws -> Data {
            try await inner.open { [closeGate] event in
                if case .closed = event { await closeGate.hold() }
                await onEvent(event)
            }
        }

        @discardableResult func send(_ text: String) -> Bool {
            let refused = lock.withLock { () -> Bool in
                guard refusing else { return false }
                rejected += 1
                return true
            }
            return refused ? false : inner.send(text)
        }
        func ping() { inner.ping() }
        func close() { inner.close() }
    }

    final class DialCounter: @unchecked Sendable {
        private let lock = NSLock()
        private var made = 0
        func first() -> Bool {
            lock.withLock {
                made += 1
                return made == 1
            }
        }
    }

    final class AutomaticSwitchRoutes: @unchecked Sendable {
        let oldEndpoint = StationEndpoint(host: "198.51.100.8")
        let localEndpoint = StationEndpoint(host: "127.0.0.1")
        private let station: FakeStation
        private let hello: LinkMessage.Hello
        private let digest: Data
        private let lock = NSLock()
        private var localAvailable = false
        private var selectedOld: TicketAnsweringOldTransport?
        private var madeCandidates: [JoinedCandidateTransport] = []
        private let snapshotGate: SnapshotGate?
        private let beforeLocalHello: (@Sendable () async -> Void)?

        init(station: FakeStation, snapshotGate: SnapshotGate? = nil,
             beforeLocalHello: (@Sendable () async -> Void)? = nil) throws {
            self.station = station
            self.snapshotGate = snapshotGate
            self.beforeLocalHello = beforeLocalHello
            digest = Data(repeating: 0x93, count: 32)
            hello = LinkMessage.Hello(major: 1, minor: 11, settingsSchema: 0, peer: "nereusd", majors: [1],
                                      identity: try station.identity.claim(certificateSHA256: digest))
        }

        var old: TicketAnsweringOldTransport? { lock.withLock { selectedOld } }
        var candidates: [JoinedCandidateTransport] { lock.withLock { madeCandidates } }
        func allowLocal() { lock.withLock { localAvailable = true } }

        var factory: LinkTransportFactory {
            { [self] endpoint, trust in
                if endpoint.canonical == oldEndpoint.canonical {
                    let source = station.transportFactory(endpoint, trust)
                    let inner: any LinkTransport = snapshotGate.map { SnapshotHeldTransport(inner: source, gate: $0) }
                        ?? source
                    let route = TicketAnsweringOldTransport(inner: inner,
                                                            offersSwitch: true)
                    lock.withLock { if selectedOld == nil { selectedOld = route } }
                    return route
                }
                if endpoint.canonical == localEndpoint.canonical {
                    let (available, old) = lock.withLock { (localAvailable, selectedOld) }
                    guard available, let old else { return DeadTransport(error: .failed("local route not up")) }
                    let candidate = JoinedCandidateTransport(hello: hello, digest: digest, old: old,
                                                             beforeHello: beforeLocalHello)
                    lock.withLock { madeCandidates.append(candidate) }
                    return candidate
                }
                return DeadTransport(error: .failed("unknown route"))
            }
        }
    }

    /// A connection that reports a route, its traffic and binary frames,
    /// so a pass-through wrapper can be checked for dropping any of them.
    final class ReportingTransport: LinkTransport, @unchecked Sendable {
        static let route = SelectedRouteObservation.fromSelectedICEPair(
            local: "a=candidate:1 1 UDP 2122317823 192.0.2.10 50000 typ host",
            remote: "a=candidate:3 1 UDP 16777215 198.51.100.22 61000 typ relay raddr 0.0.0.0 rport 0")
        static let traffic = LinkTrafficObservation(lifetime: UUID(), active: true,
                                                    receivedPayloadBytes: 10, acceptedPayloadBytes: 20)
        private let lock = NSLock()
        private var receiver: (@Sendable (Data) -> Void)?
        private var sentFrames: [Data] = []
        private var discarded = 0
        var sent: [Data] { lock.withLock { sentFrames } }
        var discards: Int { lock.withLock { discarded } }
        func deliver(_ frame: Data) { lock.withLock { receiver }?(frame) }
        var selectedRouteObservation: SelectedRouteObservation { Self.route }
        var trafficObservation: LinkTrafficObservation? { Self.traffic }
        func setBinaryReceiver(_ receiver: (@Sendable (Data) -> Void)?) { lock.withLock { self.receiver = receiver } }
        @discardableResult func sendBinary(_ frame: Data) -> Bool {
            lock.withLock { sentFrames.append(frame) }
            return true
        }
        func discardBinary(ownership: BinaryMediaOwnership) { lock.withLock { discarded += 1 } }
        func open(onEvent: @escaping @Sendable (LinkTransportEvent) async -> Void) async throws -> Data {
            Data(repeating: 7, count: 32)
        }
        @discardableResult func send(_ text: String) -> Bool { false }
        func ping() {}
        func close() {}
    }

    final class ReceivedFrames: @unchecked Sendable {
        private let lock = NSLock()
        private var stored: [Data] = []
        var frames: [Data] { lock.withLock { stored } }
        func append(_ frame: Data) { lock.withLock { stored.append(frame) } }
    }

    /// Never answers; closing it ends its opening.
    final class Unanswered: LinkTransport, @unchecked Sendable {
        private let lock = NSLock()
        private var waiting: CheckedContinuation<Data, Error>?
        private var closed = false
        func open(onEvent: @escaping @Sendable (LinkTransportEvent) async -> Void) async throws -> Data {
            try await withCheckedThrowingContinuation { continuation in
                let already = lock.withLock { () -> Bool in
                    if closed { return true }
                    waiting = continuation
                    return false
                }
                if already { continuation.resume(throwing: LinkTransportError.failed("closed")) }
            }
        }
        @discardableResult func send(_ text: String) -> Bool { false }
        func ping() {}
        func close() {
            let taken = lock.withLock { () -> CheckedContinuation<Data, Error>? in
                closed = true
                let taken = waiting
                waiting = nil
                return taken
            }
            taken?.resume(throwing: LinkTransportError.failed("closed"))
        }
    }

    final class FixedServiceRoute: CoreServiceRoute, @unchecked Sendable {
        let path: ConnectionAttempt.Path
        init(_ path: ConnectionAttempt.Path) { self.path = path }
        func makeTransport() -> any SessionTransport { DeadTransport(error: .failed("unused")) }
        func mediaIceSettings() -> IceSettings? { nil }
        var lastTry: ConnectionAttempt.Try? {
            ConnectionAttempt.Try(path: path, address: FakeRemoteAccess.host, outcome: .connected)
        }
        var lastError: RendezvousDialError? { nil }
    }

    struct Harness {
        let app: AppModel
        let flow: ConnectionFlow
        let stations: PairedStationStore
        let stationItem: InMemorySecretItem
        let keyItem: InMemorySecretItem
        let microphone: FakeMicrophone
        let network: FakeNetwork
        let clock: TestLinkClock
        let log: Log
        let reach: Reach
    }

    /// The ICE settings each media peer for a session through the remote
    /// access service was made with, in order.
    final class RemotePeers: @unchecked Sendable {
        private let lock = NSLock()
        private var made: [IceSettings?] = []

        func add(_ ice: IceSettings?) {
            lock.withLock { made.append(ice) }
        }

        var all: [IceSettings?] { lock.withLock { made } }
    }

    /// `service` stands in for the remote access service (none when nil,
    /// so no connect goes that way), and `rendezvous` is the fake Core whose
    /// pairing mailbox the service carries.
    /// Lets the flow finish what a change set off, in place of a wait: the
    /// main queue's work queued so far, any redial it began, then the main
    /// queue again.
    private static func flowSettled(_ harness: Harness) async {
        await MainQueue.drained()
        await harness.flow.redialTaskForTesting?.value
        await MainQueue.drained()
    }

    private static func harness(factory: @escaping LinkTransportFactory, paired: [PairedStation] = [],
                                keyItem: InMemorySecretItem = InMemorySecretItem(), appMajors: [UInt16] = [1],
                                deviceName: String? = nil, browser: FakeStationBrowser? = nil,
                                media: FakeStation? = nil, service: FakeRemoteAccess? = nil,
                                rendezvous: FakeStation? = nil,
                                sessionAuthenticator: (any StationAuthenticator)? = nil,
                                remotePeers: RemotePeers = RemotePeers(),
                                failingStationItem: FailingStationItem? = nil,
                                networks: (@Sendable () -> LocalNetworks)? = nil) throws -> Harness {
        let suite = "ConnectionFlowTests-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        let settings = PhoneSettings(defaults: defaults)
        settings.deviceName = deviceName
        let app = media.map { station in
            AppModel(phoneSettings: settings, mediaPeerFactory: station.mediaPeerFactory,
                     remoteMediaPeerFactory: { ice in
                         remotePeers.add(ice)
                         return station.mediaPeerFactory()
                     })
        } ?? AppModel(phoneSettings: settings)
        let stationItem = failingStationItem?.backing ?? InMemorySecretItem()
        let stations = PairedStationStore(item: failingStationItem ?? stationItem)
        for station in paired {
            try stations.save(station)
        }
        let microphone = FakeMicrophone()
        let network = FakeNetwork()
        let clock = TestLinkClock()
        let log = Log()
        let reach = Reach()
        let logged: LinkTransportFactory = { endpoint, trust in
            let inner: any LinkTransport
            switch reach.way {
            case .reaches:
                inner = factory(endpoint, trust)
            case .fails:
                inner = DeadTransport(error: .failed("waiting: no network"))
            case .hangs:
                inner = HangingTransport()
            }
            return LoggedTransport(inner: inner, name: trust == .pairing ? "pairing" : "session", log: log)
        }
        var dependencies = ConnectionFlow.Dependencies(
            keyStore: KeychainKeyStore(item: keyItem), stations: stations, kind: .phone,
            transportFactory: logged, clock: clock, microphone: microphone, network: network,
            appMajors: appMajors, now: Date.init, browser: browser)
        // FakeStation uses .invalid hostnames as in-memory transport keys;
        // they have no OS DNS record and are resolved by the fake transport.
        dependencies.directResolver = { $0 }
        if let networks {
            dependencies.networks = networks
        }
        dependencies.serviceRoute = service?.maker
        dependencies.sessionAuthenticator = sessionAuthenticator
        if let rendezvous {
            // A mailbox pairing holds its code's number in PairingClient's
            // process-wide gate; a number no other test's pairing holds keeps
            // parallel suites from refusing each other as already pairing.
            rendezvous.showCodesOnMailboxNumbers()
            dependencies.rendezvousTransportFactory = rendezvous.rendezvousTransportFactory
        }
        let flow = ConnectionFlow(app: app, dependencies: dependencies)
        return Harness(app: app, flow: flow, stations: stations, stationItem: stationItem,
                       keyItem: keyItem, microphone: microphone,
                       network: network, clock: clock, log: log, reach: reach)
    }

    /// The Core-not-answering sheet's rows, place and result in turn.
    static func rows(_ pairs: (String, String)...) -> [ConnectionFlow.TriedRow] {
        pairs.map { ConnectionFlow.TriedRow(place: $0.0, result: $0.1) }
    }

    /// Nothing on this Wi-Fi, and no reply straight over the internet.
    static let directNoReply = rows(("This Wi-Fi", "Not here"), ("Direct, over the internet", "No reply"))

    /// The fake Core, listed as paired.
    private static func paired(_ station: FakeStation) -> PairedStation {
        PairedStation(identityKey: station.identity.publicKey, label: station.label, endpoints: [station.endpoint])
    }

    /// Waits, in real time, for `condition`; the fake Core answers on its own tasks.
    private func settle(seconds: Double = 30, _ condition: () -> Bool) async -> Bool {
        let deadline = Date().addingTimeInterval(seconds)
        while Date() < deadline {
            if condition() {
                return true
            }
            await Task.yield()
            try? await Task.sleep(for: .milliseconds(5))
        }
        return condition()
    }

    /// A code the fake does not hold: its words on a number no fake Core
    /// shows and no other pairing in the process holds, so on the mailbox it
    /// finds no Core rather than another test's pairing.
    private static func wrongCode(_ code: String) -> String {
        let parts = code.split(separator: "-")
        return "\(FakeStation.unshownNameplate())-\(parts[1])-\(parts[2])"
    }

    private func connectedToFake(_ fixture: String = "session-device-sign-in", appMajors: [UInt16] = [1],
                                 additions: FakeStation.Additions = []) async throws
        -> (FakeStation, Harness) {
        let station = try FakeStation(fixture: fixture, additions: additions)
        let harness = try Self.harness(factory: station.transportFactory, paired: [Self.paired(station)],
                                       appMajors: appMajors, media: additions.contains(.wideband) ? station : nil)
        let row = try #require(harness.flow.cores.first)
        await harness.flow.connect(to: row)
        #expect(await settle { harness.flow.screen == .band })
        return (station, harness)
    }

    @Test("the fifth-device question keeps Core order, selects the first eligible place and honours Take it back")
    func fifthDeviceSelection() async throws {
        let (_, harness) = try await connectedToFake()
        let flow = harness.flow
        let session = try #require(harness.app.session)
        func device(_ id: String, _ state: LinkMessage.SessionHeld.Device.State,
                    replaceable: Bool) -> LinkMessage.SessionHeld.Device {
            .init(deviceId: id, name: id, shortName: id, kind: "computer", state: state,
                  from: "relay", replaceable: replaceable, holdsTransmit: false,
                  lastActivitySeconds: 100, connectedForSeconds: 500, awayForSeconds: 0,
                  transmittingForSeconds: 0, listeningOn: [])
        }
        let held = LinkMessage.SessionHeld(devices: [
            device("host", .listening, replaceable: false),
            device("away", .away, replaceable: true),
            device("taker", .transmitting, replaceable: true),
        ], revision: 8)
        flow.presentHeld(held, from: session)
        #expect(flow.heldQuestion == held)
        #expect(flow.heldChoiceID == "away")
        #expect(FifthDeviceSheet.buttonTitle(for: held.devices[2]) == "Unkey and take taker's place")
        flow.selectHeldDevice("host")
        #expect(flow.heldChoiceID == "away")
        flow.selectHeldDevice("taker")
        #expect(flow.heldChoiceID == "taker")
        flow.presentHeld(.init(devices: held.devices, revision: 9), from: session)
        #expect(flow.heldChoiceID == "taker")
        flow.clearHeld(from: session)
        #expect(flow.heldQuestion == nil)
        await harness.app.disconnect()
    }

    @Test("a full Core sends a held list before the snapshot; confirm and cancel send only the visible answer")
    func fifthDeviceHandshake() async throws {
        func held() -> LinkMessage.SessionHeld {
            let first = LinkMessage.SessionHeld.Device(
                deviceId: "away", name: "iPad Pro", shortName: "iPad", kind: "tablet", state: .away,
                from: "relay", replaceable: true, holdsTransmit: false, lastActivitySeconds: 500,
                connectedForSeconds: 3_600, awayForSeconds: 45, transmittingForSeconds: 0, listeningOn: [])
            return .init(devices: [first], revision: 4)
        }
        for cancel in [false, true] {
            let station = try FakeStation(fixture: "session-device-sign-in")
            let question = held()
            let factory: LinkTransportFactory = { endpoint, trust in
                FifthDeviceTransport(inner: station.transportFactory(endpoint, trust), question: question)
            }
            let harness = try Self.harness(factory: factory, paired: [Self.paired(station)])
            let flow = harness.flow
            await flow.connect(to: try #require(flow.cores.first))
            #expect(await settle { flow.heldQuestion?.revision == 4 })
            #expect(flow.heldChoiceID == "away")
            #expect(!station.messages.contains { if case .sessionTakeover = $0 { true } else { false } })
            #expect(!harness.app.mirror.isSnapshotComplete)
            await flow.answerHeld(cancel: cancel)
            let answer = await station.waitForMessage { if case .sessionTakeover = $0 { true } else { false } }
            guard case .sessionTakeover(let sent)? = answer else {
                Issue.record("no takeover answer")
                return
            }
            #expect(sent.deviceId == (cancel ? "" : "away"))
            #expect(sent.revision == 4)
            if cancel {
                // The held question clears when the session ends, before the
                // ended dial puts the notice up, and the screen can already
                // be Your Cores while dialling: wait for the notice too.
                #expect(await settle {
                    flow.heldQuestion == nil && flow.screen == .cores && flow.notice != nil
                })
                #expect(flow.notice == .words("The Core already has four devices connected."))
            } else {
                #expect(await settle { flow.heldQuestion == nil && flow.screen == .band })
                #expect(harness.app.mirror.isSnapshotComplete)
                await station.deliver(.sessionEnd(.init(
                    reason: "iPad Pro took this device's place on the Core.", retryable: false,
                    code: "takenOver", takenOverBy: "iPad Pro", takenOverById: "away", secondsAgo: 9)))
                #expect(await settle {
                    if case .placeTaken = flow.notice { return true }
                    return false
                })
                await flow.takePlaceBack()
                #expect(await settle { flow.heldQuestion?.revision == 4 })
                #expect(flow.heldChoiceID == "away")
            }
            await harness.app.disconnect()
        }
    }

    // MARK: Enter an address (D69)

    @Test("an address and its port become an endpoint, and an unpaired Core goes on to the code", arguments: [
        ("2001:db8::10", "2001:db8::10"), ("[2001:db8::10]", "2001:db8::10"),
        ("192.0.2.10", "192.0.2.10"), ("core.example", "core.example"),
    ])
    func typedAddressGoesToTheCode(typed: String, host: String) async throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        let harness = try Self.harness(factory: station.transportFactory)
        let flow = harness.flow
        #expect(flow.screen == .welcome)
        flow.findMyCore()
        flow.enterAddress()
        #expect(flow.screen == .typeAddress)
        #expect(flow.portText == "47910")
        flow.addressText = typed
        await flow.connectToTypedAddress()
        #expect(flow.screen == .pairByCode)
        #expect(flow.pairTarget?.endpoint == StationEndpoint(host: host, port: 47910))
        #expect(flow.pairTarget?.back == .typeAddress)
        // One connection read which Core answers there; nothing signed in or paired.
        #expect(station.pairingConnectionCount == 1)
        #expect(station.pairedDeviceKeys.isEmpty)
        #expect(station.connectionCount == 0)
    }

    @Test("a pasted address moves its port into the port field")
    func pastedAddressSplits() throws {
        let station = try FakeStation()
        let flow = try Self.harness(factory: station.transportFactory).flow
        flow.enterAddress()
        flow.addressText = "[2001:db8::10]:50055"
        flow.addressChanged(from: "", to: "[2001:db8::10]:50055")
        #expect(flow.addressText == "2001:db8::10")
        #expect(flow.portText == "50055")
        // Typed one character at a time, nothing moves.
        flow.addressText = "core.example:5"
        flow.portText = "47910"
        flow.addressChanged(from: "core.example:", to: "core.example:5")
        #expect(flow.addressText == "core.example:5")
        #expect(flow.portText == "47910")
    }

    @Test("a bad port or address is refused in plain words before anything is sent")
    func badAddressRefused() async throws {
        let station = try FakeStation()
        let flow = try Self.harness(factory: station.transportFactory).flow
        flow.enterAddress()
        flow.addressText = "2001:db8::10"
        for port in ["0", "65536", "", "port"] {
            flow.portText = port
            await flow.connectToTypedAddress()
            #expect(flow.addressProblem == ConnectionFlow.portProblemText)
            #expect(flow.screen == .typeAddress)
        }
        flow.portText = "47910"
        for address in ["2001:db8::zz", "[192.0.2.10]", "http://core.example", "core example", ""] {
            flow.addressText = address
            await flow.connectToTypedAddress()
            #expect(flow.addressProblem == ConnectionFlow.addressProblemText)
            #expect(flow.screen == .typeAddress)
        }
        #expect(station.pairingConnectionCount == 0)
        #expect(station.connectionCount == 0)
    }

    // MARK: A Core's addresses (JJ, 2026-09-26)

    /// A paired Core kept with `endpoints`, under `station`'s identity.
    private static func paired(_ station: FakeStation, at endpoints: [StationEndpoint]) -> PairedStation {
        PairedStation(identityKey: station.identity.publicKey, label: station.label, endpoints: endpoints)
    }

    /// Nothing answers at `dead`; everything else reaches `station`.
    private static func factory(_ station: FakeStation, dead: StationEndpoint) -> LinkTransportFactory {
        { endpoint, trust in
            endpoint.canonical == dead.canonical ? DeadTransport(error: .failed("no route"))
                : station.transportFactory(endpoint, trust)
        }
    }

    @Test("a Core with two addresses where the first doesn't answer connects on the second, which goes first")
    func secondAddressConnectsAndGoesFirst() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        let dead = StationEndpoint(host: "192.0.2.10")
        let harness = try Self.harness(factory: Self.factory(station, dead: dead),
                                       paired: [Self.paired(station, at: [dead, station.endpoint])])
        let row = try #require(harness.flow.cores.first)
        await harness.flow.connect(to: row)
        #expect(await settle { harness.flow.screen == .band })
        #expect(harness.flow.trouble == nil)
        #expect(harness.flow.attempt.tries.map(\.address) == ["192.0.2.10", station.endpoint.host])
        // Both paths open together; the dead first socket may still be
        // pending when the second wins and closes it as a loser.
        #expect([.noAnswer, .cancelled].contains(harness.flow.attempt.tries[0].outcome))
        #expect(harness.flow.attempt.tries[1].outcome == .connected)
        let kept = try #require(try harness.stations.station(identityKey: station.identity.publicKey))
        #expect(kept.endpoints == [station.endpoint, dead])
        #expect(kept.isLastGood(station.endpoint))
        #expect(harness.flow.cores.first?.address == station.endpoint.host)
        await harness.app.disconnect()
    }

    @Test("each sign-in keeps whether the Core can be reached from anywhere, as it declared", arguments: [true, false])
    func controlChannelKeptAtSignIn(declared: Bool) async throws {
        // The suite's Core declares controlChannelVersion 1; an older one declares none.
        let station = try FakeStation(fixture: "session-device-sign-in",
                                      withoutCapabilities: declared ? [] : ["controlChannelVersion"])
        let harness = try Self.harness(factory: station.transportFactory, paired: [Self.paired(station)])
        // Paired, no session yet: offered.
        let before = try #require(try harness.stations.station(identityKey: station.identity.publicKey))
        #expect(before.controlChannelVersion == nil && before.reachableFromAnywhere)
        await harness.flow.connect(to: try #require(harness.flow.cores.first))
        #expect(await settle { harness.flow.screen == .band })
        let kept = try #require(try harness.stations.station(identityKey: station.identity.publicKey))
        #expect(kept.controlChannelVersion == (declared ? 1 : 0))
        #expect(kept.reachableFromAnywhere == declared)
        await harness.app.disconnect()
    }

    @Test("when none of a Core's addresses answers, each is tried and the sheet lists what was tried")
    func everyAddressTried() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        let first = StationEndpoint(host: "192.0.2.10")
        let second = StationEndpoint(host: "198.51.100.7", port: 50055)
        let harness = try Self.harness(factory: { _, _ in DeadTransport(error: .failed("no route")) },
                                       paired: [Self.paired(station, at: [first, second])])
        await harness.flow.connect(to: try #require(harness.flow.cores.first))
        #expect(await settle { harness.flow.trouble != nil })
        #expect(harness.flow.trouble == .notAnswering(core: station.label, tried: Self.directNoReply,
                                                       localNetworkDenied: false, note: nil))
        #expect(harness.flow.attempt.tries.map(\.address) == ["192.0.2.10", "198.51.100.7 port 50055"])
        #expect(harness.log.all.filter { $0 == "open session" }.count == 2)
        // Nothing moved: no address worked.
        #expect(try harness.stations.station(identityKey: station.identity.publicKey)?.endpoints == [first, second])
    }

    @Test("another Core at one of a Core's addresses is passed for the next, and never kept as this Core")
    func anotherCoreAtAnAddressIsPassed() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        let other = try FakeStation(fixture: "session-device-sign-in")
        let factory: LinkTransportFactory = { endpoint, trust in
            endpoint.canonical == other.endpoint.canonical ? other.transportFactory(endpoint, trust)
                : station.transportFactory(endpoint, trust)
        }
        let harness = try Self.harness(factory: factory,
                                       paired: [Self.paired(station, at: [other.endpoint, station.endpoint])])
        await harness.flow.connect(to: try #require(harness.flow.cores.first))
        #expect(await settle { harness.flow.screen == .band })
        // A matching second path may win before the other Core's hello is
        // inspected, closing that losing socket without authenticating it.
        #expect([.notThisCore, .cancelled].contains(harness.flow.attempt.tries[0].outcome))
        #expect(harness.flow.attempt.tries[1].outcome == .connected)
        let kept = try harness.stations.all()
        #expect(kept.map(\.identityKey) == [station.identity.publicKey])
        #expect(kept.first?.endpoints == [station.endpoint, other.endpoint])
        await harness.app.disconnect()
    }

    @Test("another Core at a Core's only address is refused in plain words, and the Core is kept as it was")
    func anotherCoreAtTheOnlyAddressIsRefused() async throws {
        let other = try FakeStation(fixture: "session-device-sign-in")
        let mine = P256.Signing.PrivateKey().publicKey.derRepresentation
        let kept = PairedStation(identityKey: mine, label: "Rock", endpoints: [other.endpoint])
        let harness = try Self.harness(factory: other.transportFactory, paired: [kept])
        await harness.flow.connect(to: try #require(harness.flow.cores.first))
        #expect(await settle { harness.flow.notice != nil })
        #expect(harness.flow.screen == .cores)
        #expect(harness.flow.attempt.tries.map(\.outcome) == [.notThisCore])
        let after = try harness.stations.all()
        #expect(after.map(\.identityKey) == [mine])
        #expect(after.first?.endpoints == [other.endpoint])
        #expect(other.pairedDeviceKeys.isEmpty)
        // Every address tried reached another Core: this one is marked to pair again.
        #expect(harness.flow.cores.first?.needsPairing == true)
        #expect(harness.flow.trouble == nil)
    }

    @Test("a Core with one dead address and one where another Core answers is not answering, in either order",
          arguments: [true, false])
    func deadAndOtherCoreIsNotAnswering(deadFirst: Bool) async throws {
        let other = try FakeStation(fixture: "session-device-sign-in")
        let mine = P256.Signing.PrivateKey().publicKey.derRepresentation
        let dead = StationEndpoint(host: "192.0.2.10")
        let endpoints = deadFirst ? [dead, other.endpoint] : [other.endpoint, dead]
        let harness = try Self.harness(factory: Self.factory(other, dead: dead),
                                       paired: [PairedStation(identityKey: mine, label: "Rock", endpoints: endpoints)])
        await harness.flow.connect(to: try #require(harness.flow.cores.first))
        #expect(await settle { harness.flow.trouble != nil })
        // The numeric address starts while the saved hostname resolves, so
        // the sheet's final direct row reports the wrong-Core answer.
        #expect(harness.flow.trouble == .notAnswering(
            core: "Rock", tried: Self.rows(("This Wi-Fi", "Not here"),
                                           ("Direct, over the internet", "Another Core answered")),
            localNetworkDenied: false, note: nil))
        #expect(harness.flow.cores.first?.needsPairing == false)
        #expect(harness.flow.notice == nil)
        #expect(Set(harness.flow.attempt.tries.map(\.outcome)) == [.notThisCore, .noAnswer])
        #expect(try harness.stations.all().first?.endpoints == endpoints)
    }

    @Test("a new address for a paired Core is added to its row and signs in, with no code")
    func typedAddressForAPairedCoreIsAdded() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        let old = StationEndpoint(host: "192.0.2.10")
        let harness = try Self.harness(factory: Self.factory(station, dead: old),
                                       paired: [Self.paired(station, at: [old])])
        let flow = harness.flow
        flow.enterAddress()
        flow.addressText = station.endpoint.host
        await flow.connectToTypedAddress()
        #expect(await settle { flow.screen == .band })
        #expect(flow.pairTarget == nil)
        #expect(station.pairedDeviceKeys.isEmpty)
        let kept = try #require(try harness.stations.station(identityKey: station.identity.publicKey))
        #expect(kept.endpoints == [station.endpoint, old])
        #expect(kept.isLastGood(station.endpoint))
        #expect(try harness.stations.all().count == 1)
        await harness.app.disconnect()
    }

    @Test("an address where an unknown Core answers goes on to the code, as before")
    func unknownIdentityGoesToTheCode() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        let someoneElse = PairedStation(identityKey: P256.Signing.PrivateKey().publicKey.derRepresentation, label: "Attic",
                                        endpoints: [StationEndpoint(host: "192.0.2.99")])
        let harness = try Self.harness(factory: station.transportFactory, paired: [someoneElse])
        let flow = harness.flow
        flow.enterAddress()
        flow.addressText = station.endpoint.host
        await flow.connectToTypedAddress()
        #expect(flow.screen == .pairByCode)
        #expect(flow.pairTarget?.endpoint == station.endpoint)
        #expect(flow.pairTarget?.replacing == nil)
        #expect(station.connectionCount == 0)
        // The other Core's addresses are untouched.
        #expect(try harness.stations.all() == [someoneElse])
    }

    @Test("a Core that no longer takes this phone's key says so and offers pairing again")
    func revokedKeyAtANewAddress() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in", requiresPairing: true)
        let old = StationEndpoint(host: "192.0.2.10")
        let harness = try Self.harness(factory: Self.factory(station, dead: old),
                                       paired: [Self.paired(station, at: [old])])
        let flow = harness.flow
        flow.enterAddress()
        flow.addressText = station.endpoint.host
        await flow.connectToTypedAddress()
        #expect(await settle { flow.notice != nil })
        #expect(flow.notice == .removed(core: "Fake Core"))
        #expect(flow.cores.first?.needsPairing == true)
        // The address was this Core's, so it is kept with it.
        #expect(try harness.stations.station(identityKey: station.identity.publicKey)?.endpoints
                == [station.endpoint, old])
    }

    @Test("nothing answering at a typed address says so on the address screen")
    func nothingAnswersAtATypedAddress() async throws {
        // From Your Cores, on to the code, as before this feature.
        let harness = try Self.harness(factory: { _, _ in DeadTransport(error: .failed("no route")) })
        let flow = harness.flow
        flow.enterAddress()
        flow.addressText = "192.0.2.10"
        await flow.connectToTypedAddress()
        #expect(flow.screen == .pairByCode)
        #expect(flow.pairTarget?.endpoint == StationEndpoint(host: "192.0.2.10"))
        #expect(!flow.identifying)
    }

    /// Waits for the identity check's deadline to be armed.
    private func checkArmed(_ harness: Harness) async -> Bool {
        await settle { !harness.clock.pendingDueTimes.isEmpty }
    }

    @Test("a silent address reaches the code screen after the short check, not the whole connect time")
    func silentAddressReachesTheCodeInSeconds() async throws {
        let harness = try Self.harness(factory: { _, _ in DeadTransport(error: .failed("unused")) })
        harness.reach.way = .hangs
        let flow = harness.flow
        flow.enterAddress()
        flow.addressText = "192.0.2.10"
        let connecting = Task { await flow.connectToTypedAddress() }
        #expect(await checkArmed(harness))
        #expect(harness.clock.pendingDueTimes == [5_000])
        #expect(flow.identifying)
        await harness.clock.advance(by: 4_999)
        #expect(flow.screen == .typeAddress)
        await harness.clock.advance(by: 1)
        await connecting.value
        #expect(flow.screen == .pairByCode)
    }

    @Test("Back during the identity check stops it, and its answer never lands")
    func backStopsTheCheck() async throws {
        let harness = try Self.harness(factory: { _, _ in DeadTransport(error: .failed("unused")) })
        harness.reach.way = .hangs
        let flow = harness.flow
        flow.enterAddress()
        flow.addressText = "192.0.2.10"
        let connecting = Task { await flow.connectToTypedAddress() }
        #expect(await checkArmed(harness))
        flow.back()
        await connecting.value
        #expect(flow.screen == .welcome || flow.screen == .cores)
        #expect(!flow.identifying)
        #expect(flow.pairTarget == nil)
        #expect(harness.clock.pendingDueTimes.isEmpty)
    }

    @Test("a check left behind by a new one on the same screen is dropped")
    func aLeftBehindCheckIsDropped() async throws {
        let harness = try Self.harness(factory: { _, _ in DeadTransport(error: .failed("unused")) })
        harness.reach.way = .hangs
        let flow = harness.flow
        flow.enterAddress()
        flow.addressText = "192.0.2.10"
        let first = Task { await flow.connectToTypedAddress() }
        #expect(await checkArmed(harness))
        flow.back()
        flow.enterAddress()
        flow.addressText = "192.0.2.20"
        await first.value
        // The first check's end left the new screen alone.
        #expect(flow.screen == .typeAddress)
        #expect(flow.pairTarget == nil)
        #expect(flow.addressProblem == nil)
    }

    @Test("Addresses: each address with Remove, never the last, and Add an address for this Core only")
    func addressesPage() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        let other = try FakeStation(fixture: "session-device-sign-in")
        let vpn = StationEndpoint(host: "198.51.100.7")
        let factory: LinkTransportFactory = { endpoint, trust in
            endpoint.canonical == other.endpoint.canonical ? other.transportFactory(endpoint, trust)
                : station.transportFactory(endpoint, trust)
        }
        let harness = try Self.harness(factory: factory,
                                       paired: [Self.paired(station, at: [station.endpoint])])
        let flow = harness.flow
        let row = try #require(flow.cores.first)
        flow.showAddresses(row)
        #expect(flow.screen == .addresses)
        #expect(flow.addressesStation?.endpoints == [station.endpoint])
        #expect(!flow.canRemoveAddress)
        flow.removeAddress(station.endpoint)
        #expect(flow.addressesStation?.endpoints == [station.endpoint])

        // Another Core's address is refused, and nothing is kept.
        flow.addAddress()
        #expect(flow.screen == .typeAddress)
        #expect(flow.addingAddressFor == "Fake Core")
        flow.addressText = other.endpoint.host
        await flow.connectToTypedAddress()
        #expect(flow.addressProblem == ConnectionFlow.otherCoreText("Fake Core"))
        #expect(flow.screen == .typeAddress)
        #expect(flow.addressesStation?.endpoints == [station.endpoint])

        // This Core's own address is added, and the page comes back without connecting.
        flow.addressText = vpn.host
        await flow.connectToTypedAddress()
        #expect(flow.screen == .addresses)
        #expect(flow.addressesStation?.endpoints == [vpn, station.endpoint])
        #expect(flow.canRemoveAddress)
        #expect(station.connectionCount == 0)
        flow.removeAddress(station.endpoint)
        #expect(flow.addressesStation?.endpoints == [vpn])
        #expect(!flow.canRemoveAddress)
        flow.back()
        #expect(flow.screen == .cores)
    }

    // MARK: Pairing by code, then the band

    @Test("a code pairs over the direct carrier, asks for the microphone once, and the band follows with no other tap")
    func firstPairingConnectsWithTheMicrophoneQuestion() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in", requiresPairing: true)
        let harness = try Self.harness(factory: station.transportFactory)
        let flow = harness.flow
        flow.enterAddress()
        flow.addressText = "2001:db8::10"
        await flow.connectToTypedAddress()
        #expect(flow.nameText == "iPhone")
        flow.codeText = station.pairingCode
        await flow.pair()
        #expect(flow.pairingProblem == nil)
        #expect(flow.screen == .microphone)
        #expect(flow.pairedWith?.label == station.label)
        #expect(try harness.stations.all().map(\.identityKey) == [station.identity.publicKey])
        // The connection is made straight away, behind the question. The flow
        // takes the connection one hop after the app does: wait for both.
        #expect(await settle { harness.app.connection == .connected && harness.flow.connectedCoreRow != nil })
        #expect(flow.screen == .microphone)
        await flow.answerMicrophone(allow: true)
        #expect(harness.microphone.asked == 1)
        #expect(flow.screen == .band)

        // It signed in with the device key, under the name confirmed on the code screen.
        let signIn = station.messages.compactMap { message -> LinkMessage.AuthRequest? in
            if case .authRequest(let request) = message {
                return request
            }
            return nil
        }.last
        #expect(signIn?.token == "")
        #expect(signIn?.device?.name == "iPhone")
        #expect(harness.app.phoneSettings.deviceName == "iPhone")
        // The pairing connection closed before the sign-in connection opened.
        let log = harness.log.all
        let closedPairing = try #require(log.firstIndex(of: "close pairing"))
        let openedSession = try #require(log.firstIndex(of: "open session"))
        #expect(closedPairing < openedSession)
        // The address step read which Core answers there, then the pairing.
        #expect(station.pairingConnectionCount == 2)
        #expect(station.connectionCount == 1)
        await harness.app.disconnect()
    }

    @Test("a later pairing goes straight to the band without the microphone question")
    func laterPairingSkipsTheQuestion() async throws {
        let first = try FakeStation(fixture: "session-device-sign-in")
        let station = try FakeStation(fixture: "session-device-sign-in", requiresPairing: true)
        let harness = try Self.harness(factory: station.transportFactory, paired: [Self.paired(first)],
                                       service: FakeRemoteAccess(.reaches(station)), rendezvous: station)
        let flow = harness.flow
        #expect(flow.screen == .cores)
        flow.pairWithCode()
        #expect(flow.pairTarget == nil)
        flow.codeText = station.pairingCode
        await flow.pair()
        #expect(await settle { flow.screen == .band })
        #expect(harness.microphone.asked == 0)
        #expect(try harness.stations.all().count == 2)
        await harness.app.disconnect()
    }

    @Test("a word not in the list is caught before sending, with the words it may be")
    func typingSlipCaught() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        let flow = try Self.harness(factory: station.transportFactory).flow
        flow.enterAddress()
        flow.addressText = "core.example"
        await flow.connectToTypedAddress()
        let word = PairingCodeText.words[3]
        let slip = word + "x"
        flow.codeText = "7 \(PairingCodeText.words[0]) \(slip)"
        await flow.pair()
        guard case .notACode(let caught?, let suggestions) = flow.pairingProblem else {
            Issue.record("the slip was not caught: \(String(describing: flow.pairingProblem))")
            return
        }
        #expect(caught == slip)
        #expect(suggestions.contains(word))
        // Only the address step's read of which Core answers, which sends nothing.
        #expect(station.pairingConnectionCount == 1)
        #expect(station.pairedDeviceKeys.isEmpty)
        flow.useSuggestion(word, for: slip)
        #expect(flow.codeText == "7-\(PairingCodeText.words[0])-\(word)")
        #expect(flow.pairingProblem == nil)
    }

    @Test("a name the Core would refuse is refused before sending", arguments: [
        "", "   ", "Shack\u{200B}iPhone", "Shack\niPhone", String(repeating: "\u{00E9}", count: 33),
    ])
    func unusableNameRefused(name: String) async throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        let flow = try Self.harness(factory: station.transportFactory).flow
        flow.enterAddress()
        flow.addressText = "core.example"
        await flow.connectToTypedAddress()
        flow.codeText = station.pairingCode
        flow.nameText = name
        await flow.pair()
        guard case .name = flow.pairingProblem else {
            Issue.record("the name was not refused")
            return
        }
        // Only the address step's read of which Core answers, which sends nothing.
        #expect(station.pairingConnectionCount == 1)
        #expect(station.pairedDeviceKeys.isEmpty)
    }

    @Test("a wrong code shows the Core's words and its wait, and Pair waits for the new code")
    func wrongCodeWaits() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in", requiresPairing: true)
        let harness = try Self.harness(factory: station.transportFactory)
        let flow = harness.flow
        flow.enterAddress()
        flow.addressText = "core.example"
        await flow.connectToTypedAddress()
        flow.codeText = Self.wrongCode(station.pairingCode)
        await flow.pair()
        #expect(flow.pairingProblem == .wrongCode(reason: FakeStation.wrongCodeReason, waitSeconds: 5))
        #expect(flow.pairHeld)
        // Tried again before the new code: the Core says it is waiting, with its wait.
        await harness.clock.advance(by: 5000)
        #expect(!flow.pairHeld)
        await flow.pair()
        guard case .refused(let reason, let seconds) = flow.pairingProblem else {
            Issue.record("the wait was not the Core's refusal: \(String(describing: flow.pairingProblem))")
            return
        }
        #expect(reason == FakeStation.waitingReason)
        #expect(seconds >= 1)
        #expect(flow.pairHeld)
    }

    @Test("the fifth wrong code in a row says pairing closed and has to be opened again at the Core")
    func fifthWrongCodeClosesPairing() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in", requiresPairing: true)
        let harness = try Self.harness(factory: station.transportFactory)
        let flow = harness.flow
        flow.enterAddress()
        flow.addressText = "core.example"
        await flow.connectToTypedAddress()
        for attempt in 1...5 {
            station.endPairingWait()
            await harness.clock.advance(by: 60_000)
            flow.codeText = Self.wrongCode(station.pairingCode)
            await flow.pair()
            if attempt < 5 {
                guard case .wrongCode(_, let seconds) = flow.pairingProblem else {
                    Issue.record("wrong code \(attempt) was not a wrong code")
                    return
                }
                #expect(seconds == 5 << (attempt - 1))
            }
        }
        #expect(flow.pairingProblem == .pairingClosed)
        // Pairing again finds it closed, in the Core's words.
        flow.clearPairingProblem()
        flow.codeText = "1-\(PairingCodeText.words[0])-\(PairingCodeText.words[1])"
        await flow.pair()
        #expect(flow.pairingProblem == .refused(reason: FakeStation.windowClosedReason, waitSeconds: 0))
        #expect(station.pairedDeviceKeys.isEmpty)
    }

    @Test("pairing with a Core that isn't answering says what was tried")
    func pairingUnreachable() async throws {
        let harness = try Self.harness(factory: { _, _ in DeadTransport(error: .failed("no route")) })
        let flow = harness.flow
        flow.enterAddress()
        flow.addressText = "2001:db8::10"
        flow.portText = "50055"
        await flow.connectToTypedAddress()
        flow.codeText = "7-\(PairingCodeText.words[0])-\(PairingCodeText.words[1])"
        await flow.pair()
        #expect(flow.trouble == .notAnswering(core: "2001:db8::10 port 50055", tried: Self.directNoReply,
                                               localNetworkDenied: false, note: nil))
    }

    // MARK: Connecting from anywhere (Task 56, R-IOS-16, R-IOS-08, D22)

    /// The fake Core as the service reaches it: claimed on this network, so
    /// a row found here is not taken for one to pair again.
    private static func claimedFound(_ station: FakeStation) -> FoundStation {
        var found = FoundStation(instanceName: station.label, label: station.label,
                                 identityPrefix: FoundStation.identityPrefix(of: station.identity.publicKey),
                                 claimed: true, pairing: .code)
        found.endpoint = station.endpoint
        return found
    }

    @Test("the ways are tried in order: the Core's addresses, then where it was found on this network, then the service")
    func reachPlanOrder() throws {
        let key = P256.Signing.PrivateKey().publicKey.derRepresentation
        let a = StationEndpoint(host: "192.0.2.10")
        let b = StationEndpoint(host: "198.51.100.7", port: 50055)
        let found = StationEndpoint(host: "core.local")
        var station = PairedStation(identityKey: key, label: "Rock", endpoints: [a, b])
        station.reached(b, by: .direct)
        #expect(ConnectionFlow.reachPlan(station, first: nil, found: found, throughService: true)
                == [.address(b), .address(a), .address(found), .throughService])
        // Typed first; a found address already kept is not tried twice; no service for an older Core.
        #expect(ConnectionFlow.reachPlan(station, first: a, found: b, throughService: false)
                == [.address(a), .address(b)])
        // A Core paired through the service: this network, then the service.
        let noAddress = PairedStation(identityKey: key, label: "Rock", endpoints: [])
        #expect(ConnectionFlow.reachPlan(noAddress, first: nil, found: found, throughService: true)
                == [.address(found), .throughService])
        #expect(ConnectionFlow.reachPlan(noAddress, first: nil, found: nil, throughService: true) == [.throughService])
    }

    // MARK: The Core's real addresses (R-IOS-16, part 2, 2026-09-29)

    nonisolated private static let rockEnd1 = StationEndpoint(host: "2001:db8:467f:66e7:ec1f:31ff:fe8e:15f2", port: 50055)
    nonisolated private static let rockWlan0 = StationEndpoint(host: "2001:db8:467f:66e7:8a00:44ff:fe00:4602", port: 50055)
    nonisolated private static let rockUniqueLocal = StationEndpoint(host: "fd4e:1a2b:3c4d:1:ec1f:31ff:fe8e:15f2", port: 50055)
    nonisolated private static let rockLinkLocal = StationEndpoint(host: "fe80::ec1f:31ff:fe8e:15f2%en0", port: 50055)
    nonisolated private static let rockLan = StationEndpoint(host: "192.168.109.106", port: 50055)

    nonisolated private static func v6(_ text: String) -> [UInt8] {
        var address = in6_addr()
        _ = inet_pton(AF_INET6, text, &address)
        return withUnsafeBytes(of: address) { Array($0) }
    }

    /// The phone on the Rock's home Wi-Fi: its IPv4, and its global, unique
    /// local and link-local IPv6 on the same links as the Core's.
    nonisolated private static let rockHome = LocalNetworks(entries: [
        LocalNetworks.Entry(address: [192, 168, 109, 40], prefixLength: 24),
        LocalNetworks.Entry(address: v6("2001:db8:467f:66e7::40"), prefixLength: 64),
        LocalNetworks.Entry(address: v6("fd4e:1a2b:3c4d:1::40"), prefixLength: 64),
        LocalNetworks.Entry(address: v6("fe80::40"), prefixLength: 64),
    ])
    /// The phone on cellular behind NAT64.
    nonisolated private static let cellular = LocalNetworks(entries: [
        LocalNetworks.Entry(address: v6("2001:db8:7700:48::5"), prefixLength: 64),
        LocalNetworks.Entry(address: [192, 0, 0, 2], prefixLength: 32),
    ])

    /// Bonjour's answer for the Rock's host: a global IPv6, a unique local, a
    /// link-local on en0 and its private IPv4, as the browser keeps them.
    nonisolated private static func rockBonjour(extra: [String] = []) -> [StationEndpoint] {
        let hosts = [rockLan.host, "fe80::ec1f:31ff:fe8e:15f2", rockUniqueLocal.host, rockEnd1.host] + extra
        return StationBrowser.endpoints(hosts.map { host in
            if let four = IPv4Address(host) {
                return BonjourAddress(bytes: Array(four.rawValue), interfaceName: "en0")
            }
            return BonjourAddress(bytes: v6(host), interfaceName: "en0")
        }, port: 50055)
    }

    /// Records every host dialled and under which trust.
    final class Dialled: @unchecked Sendable {
        private let lock = NSLock()
        private var list: [(host: String, trust: StationTrust)] = []
        func add(_ endpoint: StationEndpoint, _ trust: StationTrust) { lock.withLock { list.append((endpoint.host, trust)) } }
        var hosts: [String] { lock.withLock { list.map(\.host) } }
        var probed: [String] { lock.withLock { list.filter { $0.trust == .pairing }.map(\.host) } }
    }

    @Test("the ways include every address the Core was found at, global IPv6 first, and the Core's own list")
    func reachPlanTakesEveryFoundAddressAndTheCoresList() throws {
        let key = P256.Signing.PrivateKey().publicKey.derRepresentation
        var station = PairedStation(identityKey: key, label: "Rock", endpoints: [Self.rockLan])
        let found = Self.rockBonjour()
        #expect(found == [Self.rockEnd1, Self.rockUniqueLocal, Self.rockLinkLocal, Self.rockLan])
        #expect(ConnectionFlow.reachPlan(station, first: nil, found: found, throughService: false)
                == [.address(Self.rockLan), .address(Self.rockEnd1), .address(Self.rockUniqueLocal),
                    .address(Self.rockLinkLocal)])
        station.keepCoreAddresses([Self.rockEnd1, Self.rockWlan0])
        #expect(ConnectionFlow.reachPlan(station, first: nil, found: [], throughService: true)
                == [.address(Self.rockLan), .address(Self.rockEnd1), .address(Self.rockWlan0), .throughService])
    }

    /// Paired from its LAN address, found on its network with a global IPv6,
    /// a unique local, a link-local and its private IPv4: the race dials the
    /// global first, it wins, and it is kept both as where the Core was
    /// reached and as an address to dial from anywhere.
    @Test("on the Core's network the race dials every address Bonjour resolved, and its global IPv6 wins and is kept")
    func bonjourAddressesAreRacedAndTheGlobalOneKept() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        let browser = FakeStationBrowser()
        let dialled = Dialled()
        let harness = try Self.harness(factory: { endpoint, trust in
                                           dialled.add(endpoint, trust)
                                           return station.transportFactory(endpoint, trust)
                                       }, paired: [Self.paired(station, at: [Self.rockLan])], browser: browser,
                                       networks: { Self.rockHome })
        let flow = harness.flow
        #expect(await settle { await browser.isBrowsing })
        var found = Self.claimedFound(station)
        found.endpoint = Self.rockLan
        found.endpoints = Self.rockBonjour()
        await browser.announce([found])
        #expect(await settle { flow.cores.first?.found != nil })
        await flow.connect(to: try #require(flow.cores.first))
        #expect(await settle { flow.screen == .band })
        // Every address is a rung; the global IPv6 starts first and wins.
        #expect(flow.attempt.tries.map(\.address)
                == [Self.rockEnd1, Self.rockUniqueLocal, Self.rockLinkLocal, Self.rockLan].map(ConnectionFlow.addressText))
        #expect(dialled.hosts.first == Self.rockEnd1.host)
        #expect(flow.attempt.tries[0].outcome == .connected)
        #expect(flow.attempt.tries[0].path == .thisNetwork)
        #expect(await settle {
            (try? harness.stations.station(identityKey: station.identity.publicKey))?.directAddresses == [Self.rockEnd1]
        })
        let kept = try #require(try harness.stations.station(identityKey: station.identity.publicKey))
        #expect(kept.isLastGood(Self.rockEnd1))
        #expect(kept.endpoints == [Self.rockEnd1, Self.rockLan])
        await harness.app.disconnect()
    }

    /// Both of the Rock's global addresses are proved and kept, not only the
    /// winner; one where another Core answers is not.
    @Test("every global address the Core proves itself at is kept, not only the winner, and no other Core's")
    func everyProvenGlobalAddressIsKept() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        let other = try FakeStation(fixture: "session-device-sign-in")
        let stranger = StationEndpoint(host: "2001:db8:467f:66e7::99", port: 50055)
        let browser = FakeStationBrowser()
        let dialled = Dialled()
        let harness = try Self.harness(factory: { endpoint, trust in
                                           dialled.add(endpoint, trust)
                                           return endpoint == stranger ? other.transportFactory(endpoint, trust)
                                               : station.transportFactory(endpoint, trust)
                                       }, paired: [Self.paired(station, at: [Self.rockLan])], browser: browser,
                                       networks: { Self.rockHome })
        let flow = harness.flow
        #expect(await settle { await browser.isBrowsing })
        var found = Self.claimedFound(station)
        found.endpoint = Self.rockLan
        found.endpoints = Self.rockBonjour(extra: [Self.rockWlan0.host, stranger.host])
        await browser.announce([found])
        #expect(await settle { flow.cores.first?.found != nil })
        await flow.connect(to: try #require(flow.cores.first))
        #expect(await settle { flow.screen == .band })
        #expect(await settle {
            let kept = try? harness.stations.station(identityKey: station.identity.publicKey)
            return Set(kept?.directAddresses ?? []) == [Self.rockEnd1, Self.rockWlan0]
        })
        // The winner needed no second look; the other two were asked who answers, and nothing was sent.
        #expect(Set(dialled.probed) == [Self.rockWlan0.host, stranger.host])
        #expect(await settle { dialled.probed.count == 2 })
        let kept = try #require(try harness.stations.station(identityKey: station.identity.publicKey))
        #expect(!kept.directAddresses.contains(stranger))
        #expect(!kept.endpoints.contains(stranger))
        await harness.app.disconnect()
    }

    /// The Core's own list arrives at sign-in and is kept; a renumbering
    /// replaces it; an empty list changes nothing; the next race off the
    /// Core's network dials its global IPv6 and never its private address.
    @Test("the Core's own addresses are kept at sign-in, replaced by each new list, and dialled from cellular")
    func theCoresOwnAddressesAreKeptAndDialled() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in", additions: [.coreAddresses])
        station.setCoreAddresses("{\"addresses\":[\"[\(Self.rockEnd1.host)]:50055\",\"[\(Self.rockWlan0.host)]:50055\"]}")
        let dialled = Dialled()
        let harness = try Self.harness(factory: { endpoint, trust in
                                           dialled.add(endpoint, trust)
                                           return station.transportFactory(endpoint, trust)
                                       }, paired: [Self.paired(station, at: [Self.rockLan])],
                                       networks: { Self.cellular })
        let flow = harness.flow
        // Nothing kept yet: from cellular the private address is not raced.
        await flow.connect(to: try #require(flow.cores.first))
        #expect(flow.screen != .band)
        #expect(dialled.hosts.isEmpty)
        // Reached once through its LAN address (an operator's choice is dialled anyway).
        await flow.connect(to: try #require(flow.cores.first), first: Self.rockLan)
        #expect(await settle { flow.screen == .band })
        #expect(await settle {
            (try? harness.stations.station(identityKey: station.identity.publicKey))?.directAddresses
                == [Self.rockEnd1, Self.rockWlan0]
        })
        let renumbered = StationEndpoint(host: "2001:db8:467f:1:ec1f:31ff:fe8e:15f2", port: 50055)
        await station.deliverCoreAddresses("{\"addresses\":[\"[\(renumbered.host)]:50055\"]}")
        #expect(await settle {
            (try? harness.stations.station(identityKey: station.identity.publicKey))?.directAddresses == [renumbered]
        })
        await station.deliverCoreAddresses(FakeStation.noCoreAddresses)
        try await LinkBarrier.roundTrip(harness.app.commands)
        #expect(try harness.stations.station(identityKey: station.identity.publicKey)?.directAddresses == [renumbered])
        await flow.leaveBand()

        // From cellular the next connect dials the Core's own address directly.
        let before = dialled.hosts.count
        await flow.connect(to: try #require(flow.cores.first))
        #expect(await settle { flow.screen == .band })
        #expect(Array(dialled.hosts.dropFirst(before)) == [renumbered.host])
        #expect(flow.attempt.tries.map(\.address) == [ConnectionFlow.addressText(renumbered)])
        #expect(flow.attempt.tries.first?.path == .direct)
        await harness.app.disconnect()
    }

    @Test("a Core that sends no list of its addresses leaves the kept ones as they are")
    func anOlderCoreLeavesTheKeptAddresses() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        var paired = Self.paired(station)
        paired.keepCoreAddresses([Self.rockEnd1])
        let harness = try Self.harness(factory: station.transportFactory, paired: [paired])
        await harness.flow.connect(to: try #require(harness.flow.cores.first))
        #expect(await settle { harness.flow.screen == .band })
        try await LinkBarrier.roundTrip(harness.app.commands)
        #expect(try harness.stations.station(identityKey: station.identity.publicKey)?.directAddresses == [Self.rockEnd1])
        await harness.app.disconnect()
    }

    /// Addresses shows what the phone learned from the Core under its own
    /// label, after the typed ones, without repeating one the operator typed;
    /// a new list from the Core replaces the learned rows on the open page.
    @Test("Addresses lists the addresses learned from the Core apart from the typed ones, and a new list replaces them")
    func addressesListsTheLearnedAddresses() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in", additions: [.coreAddresses])
        var paired = Self.paired(station, at: [station.endpoint, Self.rockWlan0])
        paired.keepCoreAddresses([Self.rockEnd1, StationEndpoint(host: Self.rockWlan0.host.uppercased(), port: 50055)])
        #expect(ConnectionFlow.learnedAddresses(of: paired) == [Self.rockEnd1])
        var none = Self.paired(station, at: [])
        #expect(ConnectionFlow.learnedAddresses(of: none).isEmpty)
        none.keepCoreAddresses([Self.rockWlan0, Self.rockEnd1])
        #expect(ConnectionFlow.learnedAddresses(of: none) == [Self.rockWlan0, Self.rockEnd1])
        #expect(ConnectionFlow.learnedHeadingText == "Learned from the Core")

        station.setCoreAddresses("{\"addresses\":[\"[\(Self.rockEnd1.host)]:50055\"]}")
        let harness = try Self.harness(factory: station.transportFactory, paired: [Self.paired(station)])
        let flow = harness.flow
        await flow.connect(to: try #require(flow.cores.first))
        #expect(await settle { flow.screen == .band })
        #expect(await settle {
            (try? harness.stations.station(identityKey: station.identity.publicKey))?.directAddresses == [Self.rockEnd1]
        })
        await flow.leaveBand()
        flow.showAddresses(try #require(flow.cores.first))
        #expect(await settle { flow.addressesStation.map(ConnectionFlow.learnedAddresses(of:)) == [Self.rockEnd1] })
        await harness.app.disconnect()
    }

    /// The installed Core's value, in its exact shape (two bracketed global
    /// IPv6 addresses with the control port; documentation-range stand-ins),
    /// arrives at sign-in, is kept in the Core's order and shows on Addresses
    /// as learned; the typed LAN address stays apart.
    @Test("the installed Core's two bracketed IPv6 addresses are learned in its order and shown apart from the typed one")
    func theInstalledCoresAddressesAreLearned() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in", additions: [.coreAddresses])
        station.setCoreAddresses(#"{"addresses":["[2001:db8:467f:66e7:8a00:44ff:fe00:4602]:50055","[2001:db8:467f:66e7:ec1f:31ff:fe8e:15f2]:50055"]}"#)
        let harness = try Self.harness(factory: station.transportFactory, paired: [Self.paired(station)])
        let flow = harness.flow
        await flow.connect(to: try #require(flow.cores.first))
        #expect(await settle { flow.screen == .band })
        #expect(await settle {
            (try? harness.stations.station(identityKey: station.identity.publicKey))?.directAddresses
                == [Self.rockWlan0, Self.rockEnd1]
        })
        await flow.leaveBand()
        flow.showAddresses(try #require(flow.cores.first))
        #expect(await settle {
            flow.addressesStation.map(ConnectionFlow.learnedAddresses(of:)) == [Self.rockWlan0, Self.rockEnd1]
        })
        #expect(flow.addressesStation?.endpoints == [station.endpoint])
        await harness.app.disconnect()
    }

    /// With no typed address and the internet service out of reach, the
    /// addresses learned from the Core are still raced as direct rungs, so
    /// the phone reaches its Core straight when the service is down.
    @Test("with the service out of reach, a Core is reached on an address learned from it")
    func aLearnedAddressReachesTheCoreWithoutTheService() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        var paired = Self.paired(station, at: [])
        paired.keepCoreAddresses([Self.rockEnd1])
        let service = FakeRemoteAccess(.unreachable)
        let dialled = Dialled()
        let harness = try Self.harness(factory: { endpoint, trust in
                                           dialled.add(endpoint, trust)
                                           guard endpoint == Self.rockEnd1 else {
                                               return DeadTransport(error: .failed("no route"))
                                           }
                                           return station.transportFactory(endpoint, trust)
                                       }, paired: [paired], service: service, networks: { Self.cellular })
        await harness.flow.connect(to: try #require(harness.flow.cores.first))
        #expect(await settle { harness.flow.screen == .band })
        #expect(dialled.hosts.contains(Self.rockEnd1.host))
        #expect(harness.flow.attempt.tries.contains {
            $0.path == .direct && $0.address == ConnectionFlow.addressText(Self.rockEnd1) && $0.outcome == .connected
        })
        #expect(harness.flow.trouble == nil)
        await harness.app.disconnect()
    }

    /// JJ, 2026-09-29: "the phone should save the address so if rv were
    /// offline we could just try last known". On cellular the phone reaches
    /// the Core through the service, whose answer offers its global host
    /// addresses among others, and the connection then fails before sign-in,
    /// so the Core's own list never arrives. The service's word is not the
    /// Core's (link document section 7.1, `coreAddresses`: nothing
    /// unauthenticated names them), so each global host address is asked
    /// which Core answers there, and none that has not answered as this
    /// Core is kept in the Keychain. Until the app quits they are held in
    /// memory, with the port the phone dials the Core on (its saved
    /// address's), and the next connect, the service out of reach, reaches
    /// the Core straight at one; reached there, its identity checked, that
    /// one is kept.
    @Test("addresses the Core offers through the service are held, not kept, until the Core answers at one, which then reaches it without the service")
    func anIntroducedAddressIsKeptAndReachesTheCoreWithoutTheService() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        let saved = StationEndpoint(host: "192.0.2.10", port: 50055)
        let paired = Self.paired(station, at: [saved])
        let answer = """
            v=0
            a=candidate:1 1 UDP 2122317823 \(Self.rockEnd1.host) 50001 typ host
            a=candidate:2 1 UDP 1686052607 198.51.100.9 60001 typ srflx raddr 0.0.0.0 rport 0
            a=candidate:3 1 UDP 41885439 203.0.113.200 3478 typ relay raddr 0.0.0.0 rport 0
            a=candidate:4 1 UDP 2122187007 \(Self.rockUniqueLocal.host) 50003 typ host
            a=candidate:5 1 UDP 2122055935 \(Self.rockLan.host) 50005 typ host
            """
        let service = FakeRemoteAccess(.answersThenFails([answer,
            "candidate:6 1 UDP 2122121471 1.2.3.44 50004 typ host",
            "candidate:7 1 UDP 2122121471 8f6c1d2e-0b1a-4c3d-9e8f-123456789abc.local 50006 typ host"]))
        let reachable = Reachable()
        let dialled = Dialled()
        let harness = try Self.harness(factory: { endpoint, trust in
                                           dialled.add(endpoint, trust)
                                           guard reachable.open, endpoint == Self.rockEnd1 else {
                                               return DeadTransport(error: .failed("no route"))
                                           }
                                           return station.transportFactory(endpoint, trust)
                                       }, paired: [paired], service: service, networks: { Self.cellular })
        await harness.flow.connect(to: try #require(harness.flow.cores.first))
        let introduced = StationEndpoint(host: "1.2.3.44", port: 50055)
        // Each was asked which Core answers there; none answered.
        #expect(await settle { Set(dialled.probed) == [Self.rockEnd1.host, introduced.host] })
        #expect(service.dials >= 1)
        #expect(harness.app.connection != .connected)
        #expect(try harness.stations.station(identityKey: station.identity.publicKey)?.directAddresses.isEmpty == true)

        // The service is out of reach now; the address held reaches the Core.
        service.way = .unreachable
        reachable.open = true
        await harness.flow.connect(to: try #require(harness.flow.cores.first))
        #expect(await settle { harness.flow.screen == .band })
        #expect(harness.flow.attempt.tries.contains {
            $0.path == .direct && $0.address == ConnectionFlow.addressText(Self.rockEnd1) && $0.outcome == .connected
        })
        // Reached there, the Core's identity checked: kept now. The other
        // never answered as this Core and is not.
        #expect(await settle {
            (try? harness.stations.station(identityKey: station.identity.publicKey))?.directAddresses == [Self.rockEnd1]
        })
        await harness.app.disconnect()
    }

    /// An address the service's introduction offered is kept as soon as the
    /// Core proves its identity there (``CoreIdentityProbe``: its hello
    /// read, nothing sent), even when the connection through the service
    /// then fails; one where another Core answers is not.
    @Test("an address offered through the service is kept once the Core proves itself there, and never another Core's")
    func anIntroducedAddressIsKeptOnceTheCoreAnswersThere() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        let other = try FakeStation(fixture: "session-device-sign-in")
        let saved = StationEndpoint(host: "192.0.2.10", port: 50055)
        let stranger = StationEndpoint(host: "2001:db8:467f:66e7::99", port: 50055)
        let answer = """
            v=0
            a=candidate:1 1 UDP 2122317823 \(Self.rockEnd1.host) 50001 typ host
            a=candidate:2 1 UDP 2122317823 \(stranger.host) 50002 typ host
            """
        let service = FakeRemoteAccess(.answersThenFails([answer]))
        let dialled = Dialled()
        let harness = try Self.harness(factory: { endpoint, trust in
                                           dialled.add(endpoint, trust)
                                           // Only an identity check reaches either Core: the race's own
                                           // dials fail, so the connection never opens.
                                           guard trust == .pairing else {
                                               return DeadTransport(error: .failed("no route"))
                                           }
                                           return endpoint == stranger ? other.transportFactory(endpoint, trust)
                                               : endpoint == Self.rockEnd1 ? station.transportFactory(endpoint, trust)
                                               : DeadTransport(error: .failed("no route"))
                                       }, paired: [Self.paired(station, at: [saved])], service: service,
                                       networks: { Self.cellular })
        await harness.flow.connect(to: try #require(harness.flow.cores.first))
        #expect(await settle {
            (try? harness.stations.station(identityKey: station.identity.publicKey))?.directAddresses == [Self.rockEnd1]
        })
        #expect(await settle { Set(dialled.probed) == [Self.rockEnd1.host, stranger.host] })
        #expect(harness.app.connection != .connected)
        let kept = try #require(try harness.stations.station(identityKey: station.identity.publicKey))
        #expect(!kept.directAddresses.contains(stranger))
        await harness.app.disconnect()
    }

    final class Reachable: @unchecked Sendable {
        private let lock = NSLock()
        private var value = false
        var open: Bool {
            get { lock.withLock { value } }
            set { lock.withLock { value = newValue } }
        }
    }

    @Test("a Core with no address connects through the service, with audio and the band, and a relayed path says so")
    func noAddressConnectsThroughTheService() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in", additions: [.wideband])
        let service = FakeRemoteAccess(.reaches(station), relayed: true)
        let remotePeers = RemotePeers()
        let paired = PairedStation(identityKey: station.identity.publicKey, label: station.label, endpoints: [])
        let harness = try Self.harness(factory: { _, _ in DeadTransport(error: .failed("no route")) }, paired: [paired],
                                       media: station, service: service, remotePeers: remotePeers)
        let row = try #require(harness.flow.cores.first)
        #expect(row.address == ConnectionFlow.fromAnywhereText)
        await harness.flow.connect(to: row)
        #expect(await settle { harness.flow.screen == .band })
        #expect(service.stationIds == [paired.rendezvousId])
        #expect(harness.flow.attempt.tries == [ConnectionAttempt.Try(path: .relay, address: FakeRemoteAccess.host,
                                                                     outcome: .connected, throughService: true)])
        #expect(harness.flow.attempt.summary == "Tried relay through the internet service (rv.nereussdr.com): connected.")
        // Nothing was dialled straight: no address to dial.
        #expect(!harness.log.all.contains("open session"))
        // The path is shown plainly: Relay on the link chip and on the Radio tab.
        #expect(harness.app.linkRelayed)
        let link = LinkState(connection: harness.app.connection, roundTripMs: 71, relayed: harness.app.linkRelayed)
        #expect(link.pathWord == "Relay")
        #expect(RadioCoreCard.status(link) == "Connected \u{00B7} relay \u{00B7} 71 ms")
        // The path is kept as the store's words; no address moves.
        let kept = try #require(try harness.stations.station(identityKey: station.identity.publicKey))
        #expect(kept.lastPath == "relay")
        #expect(kept.endpoints.isEmpty && kept.lastGood == nil)
        // Media runs over the connection the service introduced, with its settings.
        #expect(await settle(seconds: 30) { await harness.app.media.connectionId != nil })
        #expect(remotePeers.all.last == .some(service.ice))
        #expect(!station.mediaPeers.isEmpty)
        await harness.app.disconnect()
        #expect(!harness.app.linkRelayed)
    }

    @Test("a service session retry makes a new route and reads that route's selected pair")
    func serviceRetryUsesFreshRoute() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        let service = FakeRemoteAccess(.reaches(station))
        let paired = PairedStation(identityKey: station.identity.publicKey, label: station.label, endpoints: [])
        let harness = try Self.harness(factory: { _, _ in DeadTransport(error: .failed("no address")) },
                                       paired: [paired], service: service)
        await harness.flow.connect(to: try #require(harness.flow.cores.first))
        #expect(await settle { harness.flow.screen == .band })
        let first = try #require(service.routes.first)
        #expect(first.dials == 1)
        #expect(!harness.app.linkRelayed)

        // The first dialer's mutable pair belongs to a retired connection.
        // A retry must create a separate dialer, then publish its own pair.
        service.relayed = true
        await station.dropLink()
        #expect(await settle { harness.flow.linkLost != nil })
        #expect(await settle { harness.clock.pendingDueTimes.contains(harness.clock.now + 1_000) })
        await harness.clock.advance(by: 1000)
        #expect(await settle { harness.app.connection == .connected && harness.app.linkRelayed })
        #expect(service.routes.count == 2)
        let second = try #require(service.routes.last)
        #expect(first !== second)
        #expect(first.dials == 1 && second.dials == 1)
        #expect(first.lastTry?.path == .direct)
        #expect(second.lastTry?.path == .relay)
        await harness.app.disconnect()
    }

    @Test("a pass-through connection keeps its route, traffic and binary frames")
    func passThroughTransportsForwardEverything() async throws {
        guard case .available(let expected) = ReportingTransport.route, expected.kind == .turnRelay else {
            Issue.record("the fixture pair is not a relayed route")
            return
        }
        let context = ServiceRouteContext(ice: nil, path: .relay)
        let observedInner = ReportingTransport()
        let contextualInner = ReportingTransport()
        let wrappers: [(any LinkTransport, ReportingTransport)] = [
            (ObservedTransport(inner: observedInner, failures: OpenFailures()), observedInner),
            (ContextualServiceTransport(inner: contextualInner, route: FixedServiceRoute(.relay),
                                        context: context, generation: context.beginRetry()), contextualInner),
        ]
        for (wrapper, inner) in wrappers {
            #expect(wrapper.selectedRouteObservation == ReportingTransport.route)
            #expect(wrapper.trafficObservation == ReportingTransport.traffic)
            let received = ReceivedFrames()
            wrapper.setBinaryReceiver { received.append($0) }
            inner.deliver(Data([2, 1]))
            #expect(received.frames == [Data([2, 1])])
            #expect(wrapper.sendBinary(Data([2, 2])))
            let ownership = BinaryMediaOwnership()
            #expect(wrapper.sendBinary(Data([2, 3]), ownership: ownership))
            wrapper.discardBinary(ownership: ownership)
            #expect(inner.sent == [Data([2, 2]), Data([2, 3])])
            #expect(inner.discards == 1)
        }
    }

    @Test("the direct try behind a connected relay is settled once sign-in completes")
    func directTryBehindRelaySettles() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        let quiet = StationEndpoint(host: "192.0.2.10")
        let service = FakeRemoteAccess(.reaches(station), relayed: true)
        let harness = try Self.harness(factory: { endpoint, trust in
            endpoint.canonical == quiet.canonical ? Unanswered() : station.transportFactory(endpoint, trust)
        }, paired: [Self.paired(station, at: [quiet])], service: service)
        await harness.flow.connect(to: try #require(harness.flow.cores.first))
        #expect(await settle { harness.flow.screen == .band })
        #expect(harness.flow.attempt.tries.contains {
            $0.address == FakeRemoteAccess.host && $0.path == .relay && $0.outcome == .connected
        })
        #expect(await settle { !harness.flow.attempt.tries.contains { $0.outcome == .trying } },
                "\(harness.flow.attempt.summary)")
        #expect(harness.flow.attempt.tries.first { $0.address.hasPrefix(quiet.host) }?.outcome == .cancelled)
        #expect(!harness.flow.attempt.summary.contains("still trying"))
        await harness.app.disconnect()
    }

    @Test("a retired service dial cannot replace the newer retry's route context")
    func retiredServiceDialDoesNotOverwriteNewRetry() async throws {
        let context = ServiceRouteContext(ice: nil, path: .direct)
        let old = HeldOpen()
        let newer = HeldOpen()
        let oldDial = ContextualServiceTransport(inner: old, route: FixedServiceRoute(.direct),
                                                 context: context, generation: context.beginRetry())
        let openingOld = Task { try await oldDial.open { _ in } }
        #expect(await settle { old.hasStarted })
        let newDial = ContextualServiceTransport(inner: newer, route: FixedServiceRoute(.relay),
                                                 context: context, generation: context.beginRetry())
        let openingNew = Task { try await newDial.open { _ in } }
        #expect(await settle { newer.hasStarted })
        newer.finish()
        _ = try await openingNew.value
        #expect(context.path == .relay)
        old.finish()
        _ = try await openingOld.value
        #expect(context.path == .relay)

        let retired = HeldOpen()
        let retiredDial = ContextualServiceTransport(inner: retired, route: FixedServiceRoute(.direct),
                                                     context: context, generation: context.beginRetry())
        let openingRetired = Task { try await retiredDial.open { _ in } }
        #expect(await settle { retired.hasStarted })
        retiredDial.close()
        retired.finish()
        _ = try await openingRetired.value
        #expect(context.path == .relay)
    }

    @Test("a winner closed before model adoption cannot leave a phantom connecting session")
    func closedWinnerReturnsFailedAdoption() async throws {
        let core = try FakeStation(fixture: "session-device-sign-in")
        let racer = PathRacer()
        let result = await racer.run(direct: [StationEndpoint(host: "127.0.0.1")], service: nil,
                                     trust: .identity(publicKey: core.identity.publicKey),
                                     transportFactory: core.transportFactory,
                                     clock: TestLinkClock(), serviceAddress: "")
        let winner = try #require(result.winner)
        winner.transport.close()
        let app = AppModel()
        let adopted = await app.connect(winner: winner, name: core.label,
                                        trust: .identity(publicKey: core.identity.publicKey),
                                        authenticator: TokenAuthenticator(token: "unused"),
                                        clock: TestLinkClock(), transportFactory: core.transportFactory)
        #expect(!adopted)
        #expect(app.session == nil)
        #expect(core.messages.isEmpty)
        racer.cancel()
    }

    @Test("a network change after adoption but before snapshot starts a fresh race")
    func networkChangeAfterAdoptionRestartsBeforeSnapshot() async throws {
        let core = try FakeStation(fixture: "session-device-sign-in")
        let gate = SnapshotGate()
        let counter = DialCounter()
        let factory: LinkTransportFactory = { endpoint, trust in
            let first = counter.first()
            let inner = core.transportFactory(endpoint, trust)
            return first ? SnapshotHeldTransport(inner: inner, gate: gate) : inner
        }
        let harness = try Self.harness(factory: factory, paired: [Self.paired(core)])
        let row = try #require(harness.flow.cores.first)
        let connecting = Task { await harness.flow.connect(to: row) }
        #expect(await settle { await gate.hasEntered })
        #expect(harness.app.session != nil)
        #expect(harness.flow.screen != .band)
        harness.network.set(NetworkPath(online: true, interfaces: ["pdp_ip0"]))
        await gate.release()
        await connecting.value
        #expect(await settle { core.connectionCount >= 2 && harness.flow.screen == .band })
        #expect(harness.app.connection == .connected)
        await harness.app.disconnect()
    }

    @Test("service races saved and found addresses while retaining address backups")
    func cachedAddressRacesTheService() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        let dead = StationEndpoint(host: "192.0.2.10")
        let service = FakeRemoteAccess(.reaches(station))
        let browser = FakeStationBrowser()
        let harness = try Self.harness(factory: Self.factory(station, dead: dead),
                                       paired: [Self.paired(station, at: [dead])], browser: browser, service: service)
        #expect(await settle { await browser.isBrowsing })
        await browser.announce([Self.claimedFound(station)])
        #expect(await settle { harness.flow.cores.first?.found != nil })
        await harness.flow.connect(to: try #require(harness.flow.cores.first))
        #expect(await settle { harness.flow.screen == .band })
        // The service launches immediately; the saved address remains a
        // fallback even when the service's verified hello wins first.
        #expect(service.dials == 1)
        #expect(harness.flow.attempt.connected)
        #expect(harness.flow.attempt.tries.contains { $0.address == FakeRemoteAccess.host })
        let kept = try #require(try harness.stations.station(identityKey: station.identity.publicKey))
        #expect(kept.endpoints.contains(dead))
        #expect(!harness.app.linkRelayed)
        await harness.app.disconnect()
    }

    @Test("with no address answering, the concurrent service connects")
    func serviceAlongsideTheAddresses() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        let dead = StationEndpoint(host: "192.0.2.10")
        let service = FakeRemoteAccess(.reaches(station))
        let harness = try Self.harness(factory: { _, _ in DeadTransport(error: .failed("no route")) },
                                       paired: [Self.paired(station, at: [dead])], service: service)
        await harness.flow.connect(to: try #require(harness.flow.cores.first))
        #expect(await settle { harness.flow.screen == .band })
        #expect(harness.flow.attempt.tries.contains {
            $0.address == FakeRemoteAccess.host && $0.outcome == .connected
        })
        #expect(harness.flow.attempt.tries.contains { $0.address == dead.host })
        let kept = try #require(try harness.stations.station(identityKey: station.identity.publicKey))
        #expect(kept.lastPath == "direct" && kept.endpoints == [dead])
        #expect(!harness.app.linkRelayed)
        await harness.app.disconnect()
    }

    @Test("an established direct link retries with a fresh direct and service race")
    func directFailureRacesServiceBackup() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        let service = FakeRemoteAccess(.offline)
        let harness = try Self.harness(factory: station.transportFactory,
                                       paired: [Self.paired(station)], service: service)
        await harness.flow.connect(to: try #require(harness.flow.cores.first))
        #expect(await settle { harness.app.connection == .connected })
        #expect(station.connectionCount == 1)
        harness.reach.way = .fails
        service.way = .reaches(station)
        await station.dropLink()
        #expect(await settle { harness.flow.linkLost != nil })
        #expect(await settle { harness.clock.pendingDueTimes.contains(harness.clock.now + 1_000) })
        await harness.clock.advance(by: 1_000)
        #expect(await settle { harness.app.connection == .connected })
        #expect(service.dials >= 2)
        #expect(await settle {
            harness.flow.attempt.tries.contains { $0.address == FakeRemoteAccess.host && $0.outcome == .connected }
        })
        #expect(station.connectionCount == 2)
        await harness.app.disconnect()
    }

    @Test("an established service link retries the saved manual address when rendezvous is down")
    func serviceFailureRacesManualBackup() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        let service = FakeRemoteAccess(.reaches(station))
        let harness = try Self.harness(factory: station.transportFactory,
                                       paired: [Self.paired(station)], service: service)
        harness.reach.way = .fails
        await harness.flow.connect(to: try #require(harness.flow.cores.first))
        #expect(await settle { harness.app.connection == .connected })
        #expect(await settle {
            harness.flow.attempt.tries.contains { $0.address == FakeRemoteAccess.host && $0.outcome == .connected }
        })
        service.way = .offline
        harness.reach.way = .reaches
        await station.dropLink()
        #expect(await settle { harness.flow.linkLost != nil })
        #expect(await settle { harness.clock.pendingDueTimes.contains(harness.clock.now + 1_000) })
        await harness.clock.advance(by: 1_000)
        #expect(await settle { harness.app.connection == .connected })
        #expect(await settle {
            harness.flow.attempt.tries.contains { $0.address == station.endpoint.host && $0.outcome == .connected }
        })
        #expect(station.connectionCount == 2)
        await harness.app.disconnect()
    }

    @Test("a saved Core reconnects at its new address after its old IP stops answering")
    func changedAddressSameIdentityOnRetry() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        let old = StationEndpoint(host: "192.0.2.10")
        let oldRoute = Reach()
        let newRoute = Reach()
        newRoute.way = .fails
        let factory: LinkTransportFactory = { endpoint, trust in
            if endpoint.canonical == old.canonical && oldRoute.way == .fails ||
                endpoint.canonical == station.endpoint.canonical && newRoute.way == .fails {
                return DeadTransport(error: .failed("address unavailable"))
            }
            return station.transportFactory(endpoint, trust)
        }
        let harness = try Self.harness(factory: factory,
                                       paired: [Self.paired(station, at: [old, station.endpoint])])
        await harness.flow.connect(to: try #require(harness.flow.cores.first))
        #expect(await settle { harness.app.connection == .connected })
        #expect(harness.app.coreHost == old.host)
        oldRoute.way = .fails
        newRoute.way = .reaches
        await station.dropLink()
        #expect(await settle { harness.flow.linkLost != nil })
        #expect(await settle { harness.clock.pendingDueTimes.contains(harness.clock.now + 1_000) })
        await harness.clock.advance(by: 1_000)
        #expect(await settle { harness.app.connection == .connected })
        #expect(harness.app.coreHost == station.endpoint.host)
        #expect(await settle {
            harness.flow.attempt.tries.contains { $0.address == station.endpoint.host && $0.outcome == .connected }
        })
        #expect(station.connectionCount == 2)
        await harness.app.disconnect()
    }

    @Test("a selected retry paused after availability cannot replace a newer session's route")
    func retiredRetryCannotChangeNewSessionAfterAvailability() async throws {
        let old = try FakeStation(fixture: "session-device-sign-in")
        let newer = try FakeStation(fixture: "session-device-sign-in")
        let factory: LinkTransportFactory = { endpoint, trust in
            endpoint.canonical == old.endpoint.canonical
                ? old.transportFactory(endpoint, trust)
                : newer.transportFactory(endpoint, trust)
        }
        let harness = try Self.harness(factory: factory,
                                       paired: [Self.paired(old), Self.paired(newer)])
        let oldRow = try #require(harness.flow.cores.first { $0.id == old.identity.publicKey })
        let newRow = try #require(harness.flow.cores.first { $0.id == newer.identity.publicKey })
        await harness.flow.connect(to: oldRow)
        #expect(await settle { harness.app.connection == .connected })
        let gate = SnapshotGate()
        harness.app.retryAvailabilityCheckedForTesting = { await gate.hold() }
        await old.dropLink()
        #expect(await settle { harness.flow.linkLost != nil })
        #expect(await settle { harness.clock.pendingDueTimes.contains(harness.clock.now + 1_000) })
        let retry = Task { await harness.clock.advance(by: 1_000) }
        #expect(await settle { await gate.hasEntered })
        await harness.flow.connect(to: newRow)
        #expect(await settle { harness.app.connection == .connected && harness.app.coreHost == newer.endpoint.host })
        let newSession = try #require(harness.app.session)
        await gate.release()
        await retry.value
        #expect(harness.app.session === newSession)
        #expect(harness.app.coreHost == newer.endpoint.host)
        #expect(newer.connectionCount == 1)
        harness.app.retryAvailabilityCheckedForTesting = nil
        await harness.app.disconnect()
    }

    @Test("two better-look triggers cannot both pass a suspended route refresh")
    func concurrentBetterLookTriggersHaveOneReservation() async throws {
        let (station, harness) = try await connectedToFake()
        let capabilities = harness.app.mirror.capabilities.map {
            LinkMessage.PropertyEntry(name: $0.key, value: $0.value.wireValue)
        } + [.init(name: "controlSwitchVersion", value: .i64(1))]
        await station.deliver(.capabilities(.init(properties: capabilities)))
        let session = try #require(harness.app.session)
        #expect(harness.app.supportsControlMove(on: session))
        let gate = SnapshotGate()
        harness.app.pathRefreshAfterHeartbeatForTesting = { await gate.hold() }
        harness.flow.triggerBetterLookForTesting()
        #expect(await settle { await gate.hasEntered })
        harness.flow.triggerBetterLookForTesting()
        await Task.yield()
        #expect(harness.flow.betterLookReservationsForTesting == 1)
        await gate.release()
        harness.app.pathRefreshAfterHeartbeatForTesting = nil
        await harness.app.disconnect()
    }

    @Test("a stale queued wakeup cannot suppress the next dial intent's wakeup")
    func staleQueuedWakeupDoesNotBlockNextIntent() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        let harness = try Self.harness(factory: station.transportFactory, paired: [Self.paired(station)])
        harness.flow.queueResumeForTesting()
        let old = try #require(harness.flow.queuedResumeIntentForTesting)
        harness.flow.advanceResumeIntentForTesting()
        harness.flow.queueResumeForTesting()
        #expect(harness.flow.queuedResumeIntentForTesting == old + 1)
        #expect(await settle { harness.flow.queuedResumeIntentForTesting == nil })
    }

    @Test("the five-second better look moves one authenticated session to local control")
    func scheduledBetterLookMovesWithoutSecondAuthentication() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        let routes = try AutomaticSwitchRoutes(station: station)
        let harness = try Self.harness(factory: routes.factory,
                                       paired: [Self.paired(station, at: [routes.oldEndpoint, routes.localEndpoint])])
        await harness.flow.connect(to: try #require(harness.flow.cores.first))
        #expect(await settle { harness.app.connection == .connected && harness.flow.screen == .band })
        #expect(harness.app.currentPathRank == .otherWebSocket)
        let session = try #require(harness.app.session)
        #expect(harness.app.supportsControlMove(on: session))
        routes.allowLocal()
        await harness.clock.advance(by: 4_999)
        #expect(routes.candidates.isEmpty)
        await harness.clock.advance(by: 1)
        // The app takes the local route during the move; the flow keeps it
        // as the last good address one hop later, once the move returns.
        #expect(await settle {
            harness.app.currentPathRank == .localWebSocket
                && (try? PairedStationStore(item: harness.stationItem)
                    .station(identityKey: station.identity.publicKey))?
                    .isLastGood(routes.localEndpoint) == true
        })
        #expect(harness.app.session === session)
        #expect(routes.old?.ticketRequests == 1)
        #expect(station.messages.filter { $0.kind == .authRequest }.count == 1)
        #expect(routes.candidates.contains { candidate in
            candidate.messages.contains { if case .pathJoin = $0 { return true }; return false }
        })
        #expect(harness.app.mirror.isSnapshotComplete)
        let reloaded = PairedStationStore(item: harness.stationItem)
        let kept = try #require(try reloaded.station(identityKey: station.identity.publicKey))
        #expect(kept.endpoints.first?.canonical == routes.localEndpoint.canonical)
        #expect(kept.isLastGood(routes.localEndpoint))
        #expect(kept.lastPath == ConnectionAttempt.Path.thisNetwork.rawValue)
        #expect(kept.endpoints.contains { $0.canonical == routes.oldEndpoint.canonical })
        await harness.app.disconnect()
    }

    @Test("the first authenticated route retains a better verified standby through snapshot")
    func initialStandbyMovesAfterSnapshotWithoutAnotherAuthentication() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        let snapshotGate = SnapshotGate()
        let localGate = SnapshotGate()
        let routes = try AutomaticSwitchRoutes(station: station, snapshotGate: snapshotGate,
                                                beforeLocalHello: { await localGate.hold() })
        routes.allowLocal()
        let harness = try Self.harness(factory: routes.factory,
                                       paired: [Self.paired(station, at: [routes.oldEndpoint, routes.localEndpoint])])
        let row = try #require(harness.flow.cores.first)
        let connection = Task { await harness.flow.connect(to: row) }
        #expect(await settle { await snapshotGate.hasEntered })
        #expect(await settle { await localGate.hasEntered })
        #expect(routes.old?.ticketRequests == 0)
        await localGate.release()
        #expect(await settle { routes.candidates.first != nil })
        // The racer holds the local route as a verified standby once it
        // books its one-second health check; only then may the snapshot end
        // the first race, which would otherwise close it still pending.
        #expect(await settle { harness.clock.pendingDueTimes.contains(harness.clock.now + 1_000) })
        await snapshotGate.release()
        await connection.value
        #expect(await settle { harness.app.currentPathRank == .localWebSocket })
        #expect(routes.old?.ticketRequests == 1)
        #expect(station.messages.filter { $0.kind == .authRequest }.count == 1)
        #expect(harness.app.mirror.isSnapshotComplete)
        // The record says the held path took over, on its own row, and
        // nothing is left reading as still trying.
        let local = ConnectionFlow.addressText(routes.localEndpoint)
        #expect(await settle { !harness.flow.attempt.tries.contains { $0.outcome == .trying } },
                "\(harness.flow.attempt.summary)")
        #expect(harness.flow.attempt.tries.filter { $0.address == local }.map(\.outcome) == [.connected])
        await harness.app.disconnect()
    }

    @Test("a temporarily refused ticket retries the same five-second look step")
    func ticketRefusalKeepsBetterLookStep() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        let routes = try AutomaticSwitchRoutes(station: station)
        let harness = try Self.harness(factory: routes.factory,
                                       paired: [Self.paired(station, at: [routes.oldEndpoint, routes.localEndpoint])])
        await harness.flow.connect(to: try #require(harness.flow.cores.first))
        #expect(await settle { harness.app.connection == .connected && harness.flow.screen == .band })
        let old = try #require(routes.old)
        old.refuseTickets(true)
        routes.allowLocal()
        await harness.clock.advance(by: 5_000)
        #expect(await settle { old.ticketRequests == 1 })
        #expect(harness.app.currentPathRank == .otherWebSocket)
        #expect(await settle { routes.candidates.first?.isClosed == true })
        // The refused move closes the candidate before its look task comes
        // back to book the next look on the clock. Wait for that booking,
        // five seconds from now, before moving time: advanced first, the
        // look lands five seconds after the advances and never falls due.
        #expect(await settle { harness.clock.pendingDueTimes.contains(harness.clock.now + 5_000) },
                "\(harness.clock.pendingDueTimes) at \(harness.clock.now)")
        old.refuseTickets(false)
        await harness.clock.advance(by: 4_999)
        #expect(old.ticketRequests == 1)
        await harness.clock.advance(by: 1)
        #expect(await settle { harness.app.currentPathRank == .localWebSocket })
        #expect(old.ticketRequests == 2)
        #expect(station.messages.filter { $0.kind == .authRequest }.count == 1)
        await harness.app.disconnect()
    }

    @Test("a due better look waits through keyed and VOX states before racing")
    func keyedAndVoxHoldScheduledLook() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        let routes = try AutomaticSwitchRoutes(station: station)
        let harness = try Self.harness(factory: routes.factory,
                                       paired: [Self.paired(station, at: [routes.oldEndpoint, routes.localEndpoint])])
        await harness.flow.connect(to: try #require(harness.flow.cores.first))
        #expect(await settle { harness.app.connection == .connected && harness.flow.screen == .band })
        routes.allowLocal()
        let capabilities = harness.app.mirror.capabilities.map {
            LinkMessage.PropertyEntry(name: $0.key, value: $0.value.wireValue)
        } + [.init(name: "txStateVersion", value: .i64(1))]
        harness.app.mirror.apply(.capabilities(.init(properties: capabilities)))
        harness.app.mirror.apply(.objectCreate(.init(key: "txState", className: "TxState", properties: [
            .init(name: "keyed", value: .bool(true)),
        ])))
        harness.app.main.transmit.refresh()
        #expect(!harness.app.safeToMoveControl(try #require(harness.app.session)))
        await harness.clock.advance(by: 5_000)
        #expect(routes.candidates.isEmpty)
        #expect(routes.old?.ticketRequests == 0)

        harness.app.mirror.apply(.delta(.init(key: "txState", properties: [
            .init(name: "keyed", value: .bool(false)),
        ])))
        harness.app.mirror.apply(.delta(.init(key: "transmit", properties: [
            .init(name: "voxEnabled", value: .bool(true)),
        ])))
        harness.app.main.transmit.refresh()
        #expect(!harness.app.safeToMoveControl(try #require(harness.app.session)))
        await Self.flowSettled(harness)
        #expect(routes.candidates.isEmpty)
        harness.app.mirror.apply(.delta(.init(key: "transmit", properties: [
            .init(name: "voxEnabled", value: .bool(false)),
        ])))
        harness.app.main.transmit.refresh()
        #expect(await settle { harness.app.currentPathRank == .localWebSocket })
        #expect(routes.old?.ticketRequests == 1)
        await harness.app.disconnect()
    }

    @Test("a tuning-only mirror change releases a held better look")
    func tuningOnlyReleaseResumesHeldLook() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        let routes = try AutomaticSwitchRoutes(station: station)
        let harness = try Self.harness(factory: routes.factory,
                                       paired: [Self.paired(station, at: [routes.oldEndpoint, routes.localEndpoint])])
        await harness.flow.connect(to: try #require(harness.flow.cores.first))
        #expect(await settle { harness.app.connection == .connected && harness.flow.screen == .band })
        routes.allowLocal()
        let capabilities = harness.app.mirror.capabilities.map {
            LinkMessage.PropertyEntry(name: $0.key, value: $0.value.wireValue)
        } + [.init(name: "txStateVersion", value: .i64(1))]
        await station.deliver(.capabilities(.init(properties: capabilities)))
        await station.deliver(.objectCreate(.init(key: "txState", className: "TxState", properties: [
            .init(name: "tuning", value: .bool(true)),
        ])))
        let session = try #require(harness.app.session)
        #expect(await settle { !harness.app.safeToMoveControl(session) })
        await harness.clock.advance(by: 5_000)
        #expect(routes.candidates.isEmpty)
        #expect(routes.old?.ticketRequests == 0)

        await station.deliver(.delta(.init(key: "txState", properties: [
            .init(name: "tuning", value: .bool(false)),
        ])))
        #expect(await settle { harness.app.currentPathRank == .localWebSocket })
        #expect(routes.old?.ticketRequests == 1)
        await harness.app.disconnect()
    }

    @Test("when nothing reaches the Core, the sheet lists this Wi-Fi, direct and relay, in the service's terms",
          arguments: [true, false])
    func nothingReachesTheCore(serviceAnswers: Bool) async throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        let service = FakeRemoteAccess(serviceAnswers ? .offline : .unreachable)
        let harness = try Self.harness(factory: { _, _ in DeadTransport(error: .failed("no route")) },
                                       paired: [Self.paired(station)], service: service)
        await harness.flow.connect(to: try #require(harness.flow.cores.first))
        #expect(await settle { harness.flow.trouble != nil })
        #expect(harness.flow.trouble == .notAnswering(
            core: station.label,
            tried: Self.rows(("This Wi-Fi", "Not here"), ("Direct, over the internet", "No reply"),
                             ("By relay", serviceAnswers ? "Core not checked in" : "Service not reached")),
            localNetworkDenied: false, note: serviceAnswers ? nil : RendezvousDialError.unreachableServiceText))
        #expect(service.dials == 1)
        #expect(harness.app.session == nil)
        // Nothing retries out of sight.
        await harness.clock.advance(by: 120_000)
        #expect(service.dials == 1)
        // Try again goes the same ways again.
        service.way = .reaches(station)
        await harness.flow.tryAgain()
        #expect(await settle { harness.flow.screen == .band })
        await harness.app.disconnect()
    }

    @Test("a Core too old for the service is not asked through it, and the sheet says to update it")
    func olderCoreNotThroughTheService() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        let service = FakeRemoteAccess(.reaches(station))
        var older = Self.paired(station)
        older.controlChannelVersion = 0
        older.controlChannelObservedAtUnixMs = Int64(Date().timeIntervalSince1970 * 1_000)
        let harness = try Self.harness(factory: { _, _ in DeadTransport(error: .failed("no route")) },
                                       paired: [older], service: service)
        await harness.flow.connect(to: try #require(harness.flow.cores.first))
        #expect(await settle { harness.flow.trouble != nil })
        #expect(harness.flow.trouble == .notAnswering(
            core: station.label,
            tried: Self.rows(("This Wi-Fi", "Not here"), ("Direct, over the internet", "No reply"),
                             ("By relay", "Needs a newer Core")),
            localNetworkDenied: false, note: PairedStation.updateToReachFromAnywhereText))
        #expect(service.dials == 0)
    }

    @Test("an actual network change invalidates a fresh negative capability once; failed discovery stays eligible")
    func changedNetworkRechecksNegativeCapability() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        let service = FakeRemoteAccess(.offline)
        var saved = Self.paired(station)
        saved.controlChannelVersion = 0
        saved.controlChannelObservedAtUnixMs = Int64(Date().timeIntervalSince1970 * 1_000)
        let harness = try Self.harness(factory: { _, _ in DeadTransport(error: .failed("no route")) },
                                       paired: [saved], service: service)
        let flow = harness.flow
        harness.network.set(NetworkPath(online: true, interfaces: ["en0", "pdp_ip0"]))
        await flow.connect(to: try #require(flow.cores.first))
        #expect(await settle { flow.trouble != nil })
        #expect(service.dials == 0)
        // Reordering the same interfaces is a duplicate observation.
        harness.network.set(NetworkPath(online: true, interfaces: ["pdp_ip0", "en0"]))
        await flow.tryAgain()
        #expect(await settle { flow.trouble != nil })
        #expect(service.dials == 0)
        harness.network.set(NetworkPath(online: true, interfaces: ["pdp_ip0"]))
        await flow.tryAgain()
        #expect(await settle { flow.trouble != nil })
        #expect(service.dials == 1)
        // No answer cannot refresh the five-minute negative cache or stop
        // ordinary later attempts in this real network generation.
        await flow.tryAgain()
        #expect(await settle { flow.trouble != nil })
        #expect(service.dials == 2)
        let kept = try #require(try harness.stations.station(identityKey: saved.identityKey))
        #expect(kept.controlChannelObservedAtUnixMs == saved.controlChannelObservedAtUnixMs)
    }

    @Test("a same-interface address change rechecks the negative capability once")
    func sameInterfaceChangeRechecksNegativeCapability() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        let service = FakeRemoteAccess(.offline)
        var saved = Self.paired(station)
        saved.controlChannelVersion = 0
        saved.controlChannelObservedAtUnixMs = Int64(Date().timeIntervalSince1970 * 1_000)
        let harness = try Self.harness(factory: { _, _ in DeadTransport(error: .failed("no route")) },
                                       paired: [saved], service: service)
        let flow = harness.flow
        let old = NetworkSignature(addresses: [.init(interface: "en0", bytes: [10, 0, 0, 2],
                                                    prefixLength: 24, scopeID: 0)],
                                   gateways: ["4:0a000001"], supportsIPv4: true, supportsIPv6: false)
        let new = NetworkSignature(addresses: [.init(interface: "en0", bytes: [10, 0, 1, 2],
                                                    prefixLength: 24, scopeID: 0)],
                                   gateways: ["4:0a000101"], supportsIPv4: true, supportsIPv6: false)
        harness.network.set(NetworkPath(online: true, interfaces: ["en0"], signature: old))
        await flow.connect(to: try #require(flow.cores.first))
        #expect(await settle { flow.trouble != nil })
        #expect(service.dials == 0)
        harness.network.set(NetworkPath(online: true, interfaces: ["en0"], signature: new))
        await flow.tryAgain()
        #expect(await settle { flow.trouble != nil })
        #expect(service.dials == 1)
        harness.network.set(NetworkPath(online: true, interfaces: ["en0"], signature: new))
        await flow.tryAgain()
        #expect(await settle { flow.trouble != nil })
        #expect(service.dials == 2)
    }

    @Test("a code alone pairs through the service's mailbox, then connects straight away through the service")
    func codeAlonePairsFromAnywhere() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in", requiresPairing: true)
        let service = FakeRemoteAccess(.reaches(station))
        let harness = try Self.harness(factory: station.transportFactory, service: service, rendezvous: station)
        harness.microphone.needsAsking = false
        let flow = harness.flow
        flow.findMyCore()
        flow.pairWithCode()
        #expect(flow.screen == .pairByCode)
        #expect(flow.pairTarget == nil)
        flow.codeText = station.pairingCode
        await flow.pair()
        #expect(await settle { flow.screen == .band })
        #expect(station.pairedDeviceKeys.count == 1)
        // The mailbox carried the pairing; no connection went straight to the Core.
        #expect(station.pairingConnectionCount == 1)
        #expect(!harness.log.all.contains("open pairing"))
        #expect(!harness.log.all.contains("open session"))
        let kept = try #require(try harness.stations.all().first)
        #expect(kept.identityKey == station.identity.publicKey)
        #expect(kept.endpoints.isEmpty)
        #expect(kept.lastPath == "direct")
        #expect(service.stationIds == [kept.rendezvousId])
        await harness.app.disconnect()
    }

    @Test("a fresh mailbox code for an already saved Core clears old version zero and retains addresses")
    func mailboxRepairOfSavedCoreConnectsImmediately() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in", requiresPairing: true)
        let service = FakeRemoteAccess(.reaches(station))
        let oldAddress = StationEndpoint(host: "192.0.2.10")
        var saved = Self.paired(station, at: [oldAddress])
        saved.controlChannelVersion = 0
        saved.controlChannelObservedAtUnixMs = Int64(Date().timeIntervalSince1970 * 1_000)
        let harness = try Self.harness(factory: { _, _ in DeadTransport(error: .failed("stale address")) },
                                       paired: [saved], service: service, rendezvous: station)
        harness.microphone.needsAsking = false
        let flow = harness.flow
        flow.pairWithCode()
        flow.codeText = station.pairingCode
        await flow.pair()
        #expect(await settle { flow.screen == .band })
        let kept = try #require(try harness.stations.station(identityKey: saved.identityKey))
        #expect(kept.endpoints == [oldAddress])
        #expect(kept.controlChannelVersion != 0)
        #expect(kept.controlChannelObservedAtUnixMs == nil)
        #expect(service.dials == 1)
        await harness.app.disconnect()
    }

    @Test("a code no Core shows on the service is refused in the service's words, and nothing is paired")
    func codeNoCoreShows() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in", requiresPairing: true)
        let harness = try Self.harness(factory: station.transportFactory, service: FakeRemoteAccess(.reaches(station)),
                                       rendezvous: station)
        let flow = harness.flow
        flow.pairWithCode()
        flow.codeText = Self.wrongCode(station.pairingCode)
        await flow.pair()
        guard case .refused(let reason, 0)? = flow.pairingProblem else {
            Issue.record("not refused: \(String(describing: flow.pairingProblem))")
            return
        }
        #expect(reason.hasPrefix("No Core is showing that pairing code"))
        #expect(station.pairedDeviceKeys.isEmpty)
        #expect(try harness.stations.all().isEmpty)
    }

    @Test("Pair on a Core with no address opens the code for it, to pair again from anywhere")
    func pairAgainWithNoAddress() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        let paired = PairedStation(identityKey: station.identity.publicKey, label: station.label, endpoints: [])
        let harness = try Self.harness(factory: station.transportFactory, paired: [paired])
        let row = try #require(harness.flow.cores.first)
        await harness.flow.pairAgain(row)
        #expect(harness.flow.screen == .pairByCode)
        #expect(harness.flow.pairTarget == ConnectionFlow.PairTarget(endpoint: nil, label: station.label,
                                                                     replacing: row.id, back: .cores))
    }

    @Test("the link lost while keyed says the Core stops transmitting on its own")
    func linkLostWhileKeyedCover() async throws {
        let (station, harness) = try await connectedToFake(additions: [.remoteTx, .wideband])
        let transmit = harness.app.main.transmit
        #expect(await station.waitUntilLive())
        await station.deliver(.objectCreate(LinkMessage.ObjectCreate(
            key: "txState", className: "TransmitState",
            properties: TransmitScreenTests.holder("", short: "", keyed: false)
                + [.init(ordinal: 17, name: "stopSerial", value: .i64(0))])))
        await TransmitScreenTests.fillTransmit(station)
        #expect(await settle(seconds: 5) { transmit.permitted })
        transmit.tapPtt()
        #expect(await settle(seconds: 5) { transmit.ptt.state.isKeyed })
        await station.dropLink()
        #expect(await settle(seconds: 5) { harness.flow.linkLost?.keyed == true })
        let lost = try #require(harness.flow.linkLost)
        #expect(LinkLostBanner.lostLead(lost) == "You were transmitting when the link dropped.")
        #expect(LinkLostBanner.lostText(lost)
                == "The Core stops transmitting on its own when the link goes. Your frequency and settings stay on the Core.")
        // Back on the air (the redial waits on the test's clock), transmit
        // stays off until PTT is tapped.
        await harness.clock.advance(by: 30_000)
        #expect(await settle(seconds: 15) { harness.flow.screen == .band && harness.flow.linkLost == nil })
        #expect(harness.flow.backOnAir)
        #expect(await settle(seconds: 5) { transmit.ptt.state == .idle })
        await harness.app.disconnect()
    }

    @Test("the link lost while listening keeps the listening words")
    func linkLostWhileListening() {
        let lost = ConnectionFlow.LinkLost(attempt: 1, retryAt: nil, stopped: false)
        #expect(LinkLostBanner.lostLead(lost) == "The link to the Core dropped.")
        #expect(LinkLostBanner.lostText(lost) == "The phone keeps trying. Your frequency and settings stay on the Core.")
    }

    @Test("a coded link loss while keyed keeps both the Core reason and the unkeying promise")
    func codedLinkLostWhileKeyed() {
        let lost = ConnectionFlow.LinkLost(attempt: 1, retryAt: nil, stopped: false,
                                           words: "The radio is changing.", keyed: true)
        let visible = LinkLostBanner.lostLead(lost) + " " + LinkLostBanner.lostText(lost)
        #expect(visible.contains("The radio is changing."))
        #expect(visible.contains(LinkLostBanner.keyedLead))
        #expect(visible.contains(PttController.linkLostText))
    }

    @Test("pairing with a Core that opens and then says nothing says it didn't answer in time")
    func pairingTimedOut() async throws {
        // The address check fails at once; the pairing's connection opens and stays silent.
        let dials = Log()
        let harness = try Self.harness(factory: { _, _ in
            dials.add("dial")
            return dials.all.count == 1 ? DeadTransport(error: .failed("no route")) : SilentTransport()
        })
        let flow = harness.flow
        flow.enterAddress()
        flow.addressText = "2001:db8::10"
        flow.portText = "50055"
        await flow.connectToTypedAddress()
        flow.codeText = "7-\(PairingCodeText.words[0])-\(PairingCodeText.words[1])"
        let pairing = Task { await flow.pair() }
        // Dialled, with its 30 s from the dial armed.
        #expect(await settle {
            dials.all.count == 2 && harness.clock.pendingDueTimes.contains(harness.clock.now + 30_000)
        })
        await harness.clock.advance(by: 30_000)
        await pairing.value
        #expect(flow.pairingProblem == .failed(ConnectionFlow.timedOutText))
        #expect(ConnectionFlow.timedOutText == "The Core didn't answer in time, so pairing stopped. Try again.")
        #expect(flow.trouble == nil)
    }

    // MARK: The trouble screens

    @Test("a paired Core that isn't answering: what was tried, and no retries out of sight")
    func coreNotAnswering() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        let harness = try Self.harness(factory: { _, _ in DeadTransport(error: .failed("no route")) },
                                       paired: [Self.paired(station)])
        let row = try #require(harness.flow.cores.first)
        await harness.flow.connect(to: row)
        #expect(await settle { harness.flow.trouble != nil })
        #expect(harness.flow.trouble == .notAnswering(core: station.label, tried: Self.directNoReply,
                                                       localNetworkDenied: false, note: nil))
        #expect(harness.app.session == nil)
        await harness.clock.advance(by: 120_000)
        #expect(harness.log.all.filter { $0 == "open session" }.count == 1)
    }

    @Test("when iOS refuses the local network, the sheet says so and where to allow it")
    func localNetworkRefused() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        let harness = try Self.harness(factory: { _, _ in DeadTransport(error: .localNetworkDenied) },
                                       paired: [Self.paired(station)])
        let row = try #require(harness.flow.cores.first)
        await harness.flow.connect(to: row)
        #expect(await settle { harness.flow.trouble != nil })
        #expect(harness.flow.trouble == .notAnswering(
            core: station.label, tried: Self.rows(("This Wi-Fi", "Not allowed to look"),
                                                  ("Direct, over the internet", "No reply")),
            localNetworkDenied: true, note: nil))
    }

    @Test("the radio off: the Core answers but can't hear it, at each capabilities resend")
    func radioOff() async throws {
        let (station, harness) = try await connectedToFake()
        #expect(harness.flow.radioOff == nil)
        await station.deliver(.capabilities(LinkMessage.Capabilities(properties: [
            .init(name: "radioModel", value: .utf8("ANAN-G2")),
            .init(name: "radioConnected", value: .bool(false)),
            .init(name: "radioAddress", value: .utf8("192.0.2.20")),
        ])))
        #expect(await settle { harness.flow.radioOff != nil })
        let off = try #require(harness.flow.radioOff)
        #expect(off.radio == "ANAN-G2")
        #expect(off.address == "192.0.2.20")
        #expect(off.since != nil)
        #expect(LinkLostBanner.radioText(off, now: off.shownAt.addingTimeInterval(120))
            == "The last frame from the ANAN-G2 at 192.0.2.20 came 2 minutes ago. The Core tries again every 5 seconds. Is the radio on?")
        #expect(LinkLostBanner.radioRetryText(off, now: off.shownAt) == "Radio link lost: next try in 5 s")
        #expect(LinkLostBanner.radioRetryText(off, now: off.shownAt.addingTimeInterval(5)) == "Radio link lost: trying now")
        await station.deliver(.capabilities(LinkMessage.Capabilities(properties: [
            .init(name: "radioConnected", value: .bool(true)),
        ])))
        #expect(await settle { harness.flow.radioOff == nil })
        await harness.app.disconnect()
    }

    @Test("a Core with no radio says why: its words on the cover, and the retry count gives way")
    func radioWaitingInTheCoresWords() async throws {
        let (station, harness) = try await connectedToFake()
        await station.deliver(.capabilities(LinkMessage.Capabilities(properties: [
            .init(name: "radioConnected", value: .bool(false)),
        ])))
        #expect(await settle { harness.flow.radioOff != nil })
        #expect(harness.flow.radioOff?.waiting == nil)
        let words = "The Core can see more than one radio. Choose which one it runs."
        await station.deliver(.delta(LinkMessage.Delta(key: "radio", properties: [
            .init(ordinal: 25, name: "stationRadioWaiting", value: .utf8(words)),
        ])))
        #expect(await settle { harness.flow.radioOff?.waiting == words })
        let off = try #require(harness.flow.radioOff)
        #expect(LinkLostBanner.radioText(off, now: off.shownAt) == words)
        // Empty again: the phone's own words come back.
        await station.deliver(.delta(LinkMessage.Delta(key: "radio", properties: [
            .init(ordinal: 25, name: "stationRadioWaiting", value: .utf8("")),
        ])))
        #expect(await settle { harness.flow.radioOff != nil && harness.flow.radioOff?.waiting == nil })
        await station.deliver(.capabilities(LinkMessage.Capabilities(properties: [
            .init(name: "radioConnected", value: .bool(true)),
        ])))
        #expect(await settle { harness.flow.radioOff == nil })
        await harness.app.disconnect()
    }

    @Test("the Core changing its radio: its words on the LINK LOST cover, and the phone reconnects by itself")
    func radioChangingReconnects() async throws {
        let (station, harness) = try await connectedToFake()
        let flow = harness.flow
        let words = "The Core is switching to Bench G2. This app reconnects by itself."
        await station.deliver(.sessionEnd(LinkMessage.SessionEnd(reason: words, retryable: true, code: "radioChanging")))
        #expect(await settle { flow.linkLost != nil })
        #expect(flow.linkLost?.words == words)
        #expect(flow.screen == .band)
        #expect(flow.notice == nil)
        #expect(await settle { harness.clock.pendingDueTimes.contains(harness.clock.now + 1_000) })
        await harness.clock.advance(by: 1_000)
        #expect(await settle { harness.app.connection == .connected })
        #expect(await settle { flow.linkLost == nil })
        #expect(station.connectionCount >= 2)
        // A link that simply drops has no words of the Core's.
        await station.dropLink()
        #expect(await settle { flow.linkLost != nil })
        #expect(flow.linkLost?.words == nil)
        await harness.app.disconnect()
    }

    @Test("a Core two majors apart needs updating: both versions, and its reason as sent")
    func coreNeedsUpdating() async throws {
        let station = try FakeStation(fixture: "session-version-app-two-ahead")
        let harness = try Self.harness(factory: station.transportFactory, paired: [Self.paired(station)])
        let row = try #require(harness.flow.cores.first)
        await harness.flow.connect(to: row)
        #expect(await settle { harness.flow.trouble != nil })
        guard case .needsUpdating(let core, let coreSpeaks, let appSpeaks, let reason, let older, _)? =
            harness.flow.trouble else {
            Issue.record("no needs-updating sheet: \(String(describing: harness.flow.trouble))")
            return
        }
        #expect(core == station.label)
        #expect(coreSpeaks == "Remote link 1.11")
        #expect(appSpeaks == "Remote link 1.x")
        #expect(reason == "This Core runs link version 1 and this app runs version 3. Update the Core.")
        #expect(older)
        #expect(harness.flow.screen == .cores)
        #expect(ConnectionFlow.appSpeaks([2, 3]) == "Remote link 3.x, and 2.x for older Cores")
        #expect(TroubleSheet.behindText(gap: 2)
            == "The Core\u{2019}s NereusSDR is two versions behind this app, too far apart to talk. Nothing has changed on the Core.")
    }

    @Test("an older Core connects, and Tools says what it can't do yet")
    func olderCoreConnects() async throws {
        let (_, harness) = try await connectedToFake(appMajors: [1, 2])
        #expect(harness.app.connection == .connected)
        #expect(harness.flow.olderCore == "Fake Core")
        await harness.app.disconnect()
    }

    @Test("this phone offline: the band waits for a network, and dials at once when one is back")
    func phoneOffline() async throws {
        let (station, harness) = try await connectedToFake()
        harness.network.set(online: false)
        #expect(await settle { harness.flow.offline })
        #expect(LinkState(connection: harness.app.connection, roundTripMs: 12, offline: true).text == "Offline")
        await station.dropLink()
        #expect(await settle { harness.flow.linkLost != nil })
        #expect(harness.flow.offline)
        // Back on a network: no waiting out the schedule.
        harness.network.set(online: true)
        #expect(await settle { harness.app.connection == .connected })
        // The flow reads the model's states on its own task.
        #expect(await settle { harness.flow.backOnAir })
        #expect(!harness.flow.offline)
        #expect(station.connectionCount == 2)
        await harness.app.disconnect()
    }

    /// How many session connections the flow has dialled so far.
    private static func sessionOpens(_ harness: Harness) -> Int {
        harness.log.all.filter { $0 == "open session" }.count
    }

    /// The wait the session is on, when it is waiting to retry.
    private static func retryWait(_ harness: Harness) -> Int? {
        if case .waitingToRetry(let seconds, _) = harness.app.connection {
            return seconds
        }
        return nil
    }

    /// Nothing that could key the radio went to the Core.
    private static func nothingKeyed(_ station: FakeStation) -> Bool {
        !station.messages.contains { $0.kind == .propertyWrite || $0.kind == .commandInvoke }
    }

    @Test("offline while listening shows NO NETWORK whichever comes first, spends no tries, and dials once back",
          arguments: [true, false])
    func offlineWhileListening(pathFirst: Bool) async throws {
        let (station, harness) = try await connectedToFake()
        let flow = harness.flow
        harness.reach.way = .fails
        if pathFirst {
            harness.network.set(online: false)
            #expect(await settle { flow.offline })
            await station.dropLink()
            #expect(await settle { flow.linkLost != nil })
        } else {
            await station.dropLink()
            #expect(await settle { flow.linkLost != nil })
            // A link that drops while the phone is online is LINK LOST.
            #expect(!flow.offline)
            harness.network.set(online: false)
        }
        #expect(await settle { flow.offline })
        #expect(flow.linkLost?.stopped == false)
        // No tries while offline: they cannot succeed.
        let opens = Self.sessionOpens(harness)
        await harness.clock.advance(by: 120_000)
        #expect(Self.sessionOpens(harness) == opens)
        #expect(flow.offline)
        // Back online: the reconnecting state, and a try at once.
        harness.network.set(online: true)
        #expect(await settle { !flow.offline })
        #expect(await settle { Self.sessionOpens(harness) == opens + 1 })
        #expect(flow.linkLost?.stopped == false)
        #expect(await settle { Self.retryWait(harness) == 1 })
        await harness.app.disconnect()
    }

    @Test("offline while a Reconnect is still dialling is NO NETWORK too, and back online dials at once")
    func offlineWhileReconnectDials() async throws {
        let (station, harness) = try await connectedToFake()
        let flow = harness.flow
        await station.dropLink()
        #expect(await settle { flow.linkLost != nil })
        await flow.cancelReconnecting()
        harness.reach.way = .hangs
        let reconnecting = Task { await flow.reconnect() }
        #expect(await settle { Self.sessionOpens(harness) == 2 && flow.connectingTo != nil })
        #expect(flow.linkLost?.stopped == false)
        harness.network.set(online: false)
        #expect(await settle { flow.offline })
        // The hung try reaches its deadline; nothing more is tried offline.
        await harness.clock.advance(by: 120_000)
        await reconnecting.value
        #expect(Self.sessionOpens(harness) == 2)
        #expect(flow.offline)
        harness.reach.way = .reaches
        harness.network.set(online: true)
        #expect(await settle { harness.app.connection == .connected })
        // The model is connected before the flow reads it: the flow takes
        // the model's states in order on its own task, so wait for the
        // flow's own sign that it has, back on the air.
        #expect(await settle { flow.backOnAir })
        #expect(!flow.offline)
        #expect(flow.linkLost == nil)
        #expect(Self.nothingKeyed(station))
        await harness.app.disconnect()
    }

    @Test("the network back after 25 s offline dials within a second, not at the schedule's next step")
    func networkBackDialsAtOnce() async throws {
        let (station, harness) = try await connectedToFake()
        let flow = harness.flow
        harness.reach.way = .fails
        harness.network.set(online: false)
        #expect(await settle { flow.offline })
        await station.dropLink()
        #expect(await settle { flow.linkLost != nil })
        let opens = Self.sessionOpens(harness)
        await harness.clock.advance(by: 25_000)
        #expect(Self.sessionOpens(harness) == opens)
        harness.reach.way = .reaches
        let back = harness.clock.now
        harness.network.set(online: true)
        // The clock does not move: the dial goes at once.
        #expect(await settle { harness.app.connection == .connected })
        #expect(harness.clock.now - back < 1000)
        #expect(station.connectionCount == 2)
        #expect(await settle { flow.backOnAir })
        #expect(Self.nothingKeyed(station))
        await harness.app.disconnect()
    }

    @Test("a new network while LINK LOST shows tries at once, from the schedule's first step")
    func pathChangeWhileLinkLost() async throws {
        let (station, harness) = try await connectedToFake()
        let flow = harness.flow
        harness.network.set(online: true)
        // The first network the flow hears is a change: its redial goes
        // out and ends while the link is up, not after the drop.
        #expect(await settle { flow.redialsForTesting == 1 })
        await flow.redialTaskForTesting?.value
        harness.reach.way = .fails
        await station.dropLink()
        #expect(await settle { flow.linkLost != nil })
        #expect(await settle { harness.clock.pendingDueTimes.contains(harness.clock.now + 1_000) })
        // Tries at 1, 3, 8 and 18 s; the next waits 30 s.
        await harness.clock.advance(by: 18_000)
        #expect(await settle { Self.retryWait(harness) == 30 })
        let opens = Self.sessionOpens(harness)
        // Cellular comes up beside Wi-Fi: a try at once, and the schedule starts over.
        harness.network.set(NetworkPath(online: true, interfaces: ["en0", "pdp_ip0"]))
        #expect(await settle { Self.sessionOpens(harness) == opens + 1 })
        #expect(await settle { Self.retryWait(harness) == 1 })
        #expect(!flow.offline)
        // A try left hanging on the old network is dropped for one on the new.
        harness.reach.way = .hangs
        harness.network.set(NetworkPath(online: true, interfaces: ["pdp_ip0"]))
        #expect(await settle { Self.sessionOpens(harness) == opens + 2 && harness.app.connection == .connecting })
        harness.reach.way = .reaches
        harness.network.set(NetworkPath(online: true, interfaces: ["en1", "pdp_ip0"]))
        #expect(await settle { harness.app.connection == .connected })
        #expect(Self.sessionOpens(harness) == opens + 3)
        #expect(station.connectionCount == 2)
        #expect(Self.nothingKeyed(station))
        await harness.app.disconnect()
    }

    @Test("same-interface topology change restarts lost-link retry, duplicate does not")
    func sameInterfaceChangeWhileLinkLost() async throws {
        let (station, harness) = try await connectedToFake()
        let first = NetworkSignature(addresses: [.init(interface: "en0", bytes: [10, 0, 0, 2],
                                                      prefixLength: 24, scopeID: 0)],
                                     gateways: ["4:0a000001"], supportsIPv4: true, supportsIPv6: false)
        let changed = NetworkSignature(addresses: first.addresses!, gateways: ["4:0a0000fe"],
                                       supportsIPv4: true, supportsIPv6: false)
        harness.network.set(NetworkPath(online: true, interfaces: ["en0"], signature: first))
        #expect(await settle { harness.flow.redialsForTesting == 1 })
        await harness.flow.redialTaskForTesting?.value
        harness.reach.way = .fails
        await station.dropLink()
        #expect(await settle { harness.flow.linkLost != nil })
        #expect(await settle { harness.clock.pendingDueTimes.contains(harness.clock.now + 1_000) })
        await harness.clock.advance(by: 18_000)
        #expect(await settle { Self.retryWait(harness) == 30 })
        let opens = Self.sessionOpens(harness)
        harness.network.set(NetworkPath(online: true, interfaces: ["en0"], signature: changed))
        #expect(await settle { Self.sessionOpens(harness) == opens + 1 })
        #expect(await settle { Self.retryWait(harness) == 1 })
        harness.network.set(NetworkPath(online: true, interfaces: ["en0"], signature: changed))
        await Self.flowSettled(harness)
        #expect(Self.sessionOpens(harness) == opens + 1)
        #expect(Self.retryWait(harness) == 1)
        await harness.app.disconnect()
    }

    @Test("same-interface address change checks the connected path once")
    func sameInterfaceChangeWhileConnected() async throws {
        let (_, harness) = try await connectedToFake()
        let first = NetworkSignature(addresses: [.init(interface: "en0", bytes: [10, 0, 0, 2],
                                                      prefixLength: 24, scopeID: 0)],
                                     gateways: [], supportsIPv4: true, supportsIPv6: false)
        let changed = NetworkSignature(addresses: [.init(interface: "en0", bytes: [10, 0, 0, 3],
                                                        prefixLength: 24, scopeID: 0)],
                                       gateways: [], supportsIPv4: true, supportsIPv6: false)
        harness.network.set(NetworkPath(online: true, interfaces: ["en0"], signature: first))
        #expect(await settle { harness.log.all.contains("ping session") })
        let pings = harness.log.all.filter { $0 == "ping session" }.count
        harness.network.set(NetworkPath(online: true, interfaces: ["en0"], signature: changed))
        #expect(await settle { harness.log.all.filter { $0 == "ping session" }.count == pings + 1 })
        harness.network.set(NetworkPath(online: true, interfaces: ["en0"], signature: changed))
        await Self.flowSettled(harness)
        #expect(harness.log.all.filter { $0 == "ping session" }.count == pings + 1)
        let unknown = NetworkSignature(addresses: [NetworkSignature.Address]?.none,
                                       gateways: [], supportsIPv4: true, supportsIPv6: false)
        harness.network.set(NetworkPath(online: true, interfaces: ["en0"], signature: unknown))
        harness.network.set(NetworkPath(online: true, interfaces: ["en0"], signature: unknown))
        await Self.flowSettled(harness)
        #expect(harness.log.all.filter { $0 == "ping session" }.count == pings + 1)
        await harness.app.disconnect()
    }

    @Test("after Cancel, nothing about the network dials again")
    func cancelHoldsThroughNetworkChanges() async throws {
        let (station, harness) = try await connectedToFake()
        let flow = harness.flow
        await station.dropLink()
        #expect(await settle { flow.linkLost != nil })
        await flow.cancelReconnecting()
        let opens = Self.sessionOpens(harness)
        harness.network.set(online: false)
        harness.network.set(online: true)
        harness.network.set(NetworkPath(online: true, interfaces: ["pdp_ip0"]))
        await harness.clock.advance(by: 120_000)
        // Let the flow read every change before looking.
        await Self.flowSettled(harness)
        #expect(Self.sessionOpens(harness) == opens)
        #expect(station.connectionCount == 1)
        #expect(flow.linkLost?.stopped == true)
        #expect(!flow.offline)
        #expect(harness.app.session == nil)
    }

    @Test("a first connection while offline waits with the note, spends no tries, and dials at once when back")
    func firstConnectionOffline() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        let harness = try Self.harness(factory: station.transportFactory, paired: [Self.paired(station)])
        let flow = harness.flow
        harness.reach.way = .fails
        harness.network.set(online: false)
        let row = try #require(flow.cores.first)
        await flow.connect(to: row)
        #expect(await settle { flow.connectingNote == ConnectionFlow.offlineWaitText })
        #expect(flow.trouble == nil)
        let opens = Self.sessionOpens(harness)
        await harness.clock.advance(by: 120_000)
        #expect(Self.sessionOpens(harness) == opens)
        #expect(flow.connectingNote == ConnectionFlow.offlineWaitText)
        harness.reach.way = .reaches
        harness.network.set(online: true)
        #expect(await settle { flow.screen == .band })
        #expect(harness.app.connection == .connected)
        await harness.app.disconnect()
    }

    @Test("offline then online callbacks before a tap leave one current connection")
    func queuedOfflineOnlineBeforeTap() async throws {
        let core = try FakeStation(fixture: "session-device-sign-in")
        let harness = try Self.harness(factory: core.transportFactory, paired: [Self.paired(core)])
        harness.network.set(online: false)
        harness.network.set(online: true)
        await harness.flow.connect(to: try #require(harness.flow.cores.first))
        #expect(await settle { harness.flow.screen == .band })
        #expect(harness.flow.trouble == nil)
        #expect(harness.app.connection == .connected)
        await harness.app.disconnect()
    }

    @Test("online then offline callbacks before a tap hold all routes until online")
    func queuedOnlineOfflineBeforeTap() async throws {
        let core = try FakeStation(fixture: "session-device-sign-in")
        let harness = try Self.harness(factory: core.transportFactory, paired: [Self.paired(core)])
        harness.network.set(online: true)
        harness.network.set(online: false)
        await harness.flow.connect(to: try #require(harness.flow.cores.first))
        #expect(await settle { harness.flow.connectingNote == ConnectionFlow.offlineWaitText })
        #expect(Self.sessionOpens(harness) == 0)
        #expect(harness.flow.trouble == nil)
        harness.network.set(online: true)
        #expect(await settle { harness.flow.screen == .band })
        await harness.app.disconnect()
    }

    @Test("Cancel during a paused first race prevents a queued online restart")
    func cancelBeforeQueuedOnlineRestart() async throws {
        let core = try FakeStation(fixture: "session-device-sign-in")
        let harness = try Self.harness(factory: core.transportFactory, paired: [Self.paired(core)])
        harness.network.set(online: false)
        await harness.flow.connect(to: try #require(harness.flow.cores.first))
        #expect(harness.flow.connectingNote == ConnectionFlow.offlineWaitText)
        harness.reach.way = .hangs
        harness.network.set(online: true)
        await harness.flow.cancelConnecting()
        let opens = Self.sessionOpens(harness)
        await Self.flowSettled(harness)
        #expect(Self.sessionOpens(harness) == opens)
        #expect(harness.app.session == nil)
        #expect(core.messages.isEmpty)
    }

    @Test("an offline callback during a stalled first dial suppresses no-answer trouble")
    func offlineBeforeHeldRaceFailure() async throws {
        let core = try FakeStation(fixture: "session-device-sign-in")
        let held = HangingTransport()
        let counter = DialCounter()
        let factory: LinkTransportFactory = { endpoint, trust in
            if counter.first() { held } else { core.transportFactory(endpoint, trust) }
        }
        let harness = try Self.harness(factory: factory, paired: [Self.paired(core)])
        let row = try #require(harness.flow.cores.first)
        let connecting = Task { await harness.flow.connect(to: row) }
        #expect(await settle { Self.sessionOpens(harness) == 1 })
        harness.network.set(online: false)
        held.close()
        await connecting.value
        #expect(await settle { harness.flow.connectingNote == ConnectionFlow.offlineWaitText })
        #expect(harness.flow.trouble == nil)
        harness.network.set(online: true)
        #expect(await settle { harness.flow.screen == .band })
        await harness.app.disconnect()
    }

    @Test("a pending network restart cannot outlive an explicit end during sign-in",
          arguments: ["Cancel", "Disconnect", "Leave"])
    func pendingRestartEndsWithUserIntent(action: String) async throws {
        let core = try FakeStation(fixture: "session-device-sign-in")
        let signer = HeldAuthenticator()
        let harness = try Self.harness(factory: core.transportFactory, paired: [Self.paired(core)],
                                       sessionAuthenticator: signer)
        let row = try #require(harness.flow.cores.first)
        let connecting = Task { await harness.flow.connect(to: row) }
        #expect(await settle { await signer.hasEntered })
        #expect(Self.sessionOpens(harness) == 1)
        harness.network.set(NetworkPath(online: true, interfaces: ["pdp_ip0"]))
        #expect(await settle { harness.log.all.contains("close session") })

        switch action {
        case "Cancel": await harness.flow.cancelConnecting()
        case "Disconnect": await harness.flow.disconnect()
        default: await harness.flow.leaveBand()
        }
        await signer.release()
        await connecting.value
        await Self.flowSettled(harness)
        #expect(Self.sessionOpens(harness) == 1)
        #expect(core.messages.isEmpty)
        #expect(harness.app.session == nil)
        #expect(harness.flow.screen == .cores)
    }

    @Test("Cancel Reconnect clears a pending network restart during sign-in")
    func cancelReconnectClearsPendingRestart() async throws {
        let core = try FakeStation(fixture: "session-device-sign-in")
        let keyItem = InMemorySecretItem()
        let identity = try DeviceIdentity.load(store: KeychainKeyStore(item: keyItem))
        let real = try DeviceKeyAuthenticator(identity: identity, name: "iPhone", kind: .phone)
        let signer = HeldReconnectAuthenticator(signer: real)
        let harness = try Self.harness(factory: core.transportFactory, paired: [Self.paired(core)],
                                       keyItem: keyItem, sessionAuthenticator: signer)
        await harness.flow.connect(to: try #require(harness.flow.cores.first))
        #expect(await settle { harness.flow.screen == .band })
        await core.dropLink()
        #expect(await settle { harness.flow.linkLost != nil })
        await harness.flow.cancelReconnecting()
        let reconnecting = Task { await harness.flow.reconnect() }
        #expect(await settle { await signer.hasEntered })
        let opened = Self.sessionOpens(harness)
        #expect(opened == 2)
        let closes = harness.log.all.filter { $0 == "close session" }.count
        harness.network.set(NetworkPath(online: true, interfaces: ["pdp_ip0"]))
        #expect(await settle { harness.log.all.filter { $0 == "close session" }.count > closes })
        await harness.flow.cancelReconnecting()
        await signer.release()
        await reconnecting.value
        await Self.flowSettled(harness)
        #expect(Self.sessionOpens(harness) == opened)
        #expect(core.messages.filter { $0.kind == .authRequest }.count == 1)
        #expect(harness.flow.linkLost?.stopped == true)
        #expect(harness.app.session == nil)
    }

    @Test("overlapping Flow teardowns retain their own ending and dial intent", arguments: [true, false])
    func overlappingFlowTeardowns(oldFinishesFirst: Bool) async throws {
        let core = try FakeStation(fixture: "session-device-sign-in")
        let harness = try Self.harness(factory: core.transportFactory, paired: [Self.paired(core)])
        let row = try #require(harness.flow.cores.first)
        await harness.flow.connect(to: row)
        #expect(await settle { harness.flow.screen == .band })
        let gate = DisconnectGate()
        harness.app.disconnectAfterSessionStopForTesting = { await gate.hold() }

        let oldStartup = Task { await harness.flow.connect(to: row) }
        #expect(await settle { await gate.entered(1) })
        let cancel = Task { await harness.flow.cancelConnecting() }
        #expect(await settle { await gate.entered(2) })
        if oldFinishesFirst {
            await gate.release(1)
            await oldStartup.value
            #expect(harness.flow.endingSessionForTesting)
            await gate.release(2)
            await cancel.value
            #expect(!harness.flow.endingSessionForTesting)
            #expect(!harness.flow.diallingForTesting)
        } else {
            await gate.release(2)
            await cancel.value
            harness.app.disconnectAfterSessionStopForTesting = nil
            harness.reach.way = .hangs
            let currentStartup = Task { await harness.flow.connect(to: row) }
            #expect(await settle { Self.sessionOpens(harness) == 2 })
            #expect(harness.flow.diallingForTesting)
            await gate.release(1)
            await oldStartup.value
            #expect(harness.flow.diallingForTesting)
            #expect(Self.sessionOpens(harness) == 2)
            await harness.flow.cancelConnecting()
            await currentStartup.value
        }
        harness.app.disconnectAfterSessionStopForTesting = nil
        #expect(harness.app.session == nil)
    }

    @Test("obsolete teardown cannot suppress a new Flow connection or its network transition",
          arguments: ["Startup", "Cancel", "Disconnect", "Leave"], [true, false])
    func obsoleteTeardownKeepsCurrentFlow(action: String, oldFinishesFirst: Bool) async throws {
        let core = try FakeStation(fixture: "session-device-sign-in", additions: [.sessionLeave])
        let harness = try Self.harness(factory: core.transportFactory, paired: [Self.paired(core)])
        let row = try #require(harness.flow.cores.first)
        await harness.flow.connect(to: row)
        #expect(await settle { harness.flow.screen == .band })
        harness.network.set(online: true)
        #expect(await settle { harness.log.all.contains("ping session") })
        let gate = DisconnectGate()
        harness.app.disconnectAfterSessionStopForTesting = { await gate.hold() }
        let old = Task {
            switch action {
            case "Startup": await harness.flow.connect(to: row)
            case "Cancel": await harness.flow.cancelConnecting()
            case "Disconnect": await harness.flow.disconnect()
            default: await harness.flow.leaveBand()
            }
        }
        #expect(await settle { await gate.entered(1) })
        let cancel = Task { await harness.flow.cancelConnecting() }
        #expect(await settle { await gate.entered(2) })
        if oldFinishesFirst {
            await gate.release(1)
            await old.value
            #expect(harness.flow.endingSessionForTesting)
        }
        await gate.release(2)
        await cancel.value
        harness.app.disconnectAfterSessionStopForTesting = nil
        await harness.flow.connect(to: row)
        #expect(await settle { harness.app.connection == .connected })
        // Screen alone is insufficient: Cancel can leave the previous band visible.
        #expect(await settle(seconds: 2) { harness.flow.connectingTo == nil })
        #expect(harness.flow.screen == .band)
        #expect(harness.flow.attempt.tries.contains { $0.outcome == .connected })
        #expect(!harness.flow.disconnecting)
        let currentSession = try #require(harness.app.session)
        let pings = harness.log.all.filter { $0 == "ping session" }.count
        harness.network.set(NetworkPath(online: true, interfaces: ["pdp_ip0"]))
        #expect(await settle(seconds: 2) {
            harness.log.all.filter { $0 == "ping session" }.count > pings
        })
        if !oldFinishesFirst {
            await gate.release(1)
            await old.value
        }
        let laterPings = harness.log.all.filter { $0 == "ping session" }.count
        harness.network.set(NetworkPath(online: true, interfaces: ["en1"]))
        #expect(await settle(seconds: 2) {
            harness.log.all.filter { $0 == "ping session" }.count > laterPings
        })
        #expect(harness.app.session === currentSession)
        #expect(harness.app.ownsPublishedSessionForTesting)
        #expect(harness.flow.screen == .band)
        #expect(harness.flow.connectingTo == nil)
        #expect(!harness.flow.endingSessionForTesting)
        #expect(!harness.flow.diallingForTesting)
        #expect(Self.sessionOpens(harness) == 2)
        #expect(core.messages.filter { $0.kind == .authRequest }.count == 2)
        #expect(Self.leaves(core).count == (action == "Disconnect" ? 1 : 0))
        await harness.flow.disconnect()
    }

    @Test("an obsolete Disconnect cannot disable a new connection or enable a newer Disconnect")
    func obsoleteDisconnectKeepsCurrentButtonOwnership() async throws {
        let (core, harness) = try await connectedToFake(additions: [.sessionLeave])
        let row = try #require(harness.flow.cores.first)
        let gate = DisconnectGate()
        harness.app.disconnectAfterSessionStopForTesting = { await gate.hold() }
        let old = Task { await harness.flow.disconnect() }
        #expect(await settle { await gate.entered(1) })
        #expect(harness.flow.disconnecting)
        let cancel = Task { await harness.flow.cancelConnecting() }
        #expect(await settle { await gate.entered(2) })
        await gate.release(2)
        await cancel.value
        harness.app.disconnectAfterSessionStopForTesting = nil
        await harness.flow.connect(to: row)
        #expect(await settle { harness.flow.connectingTo == nil && harness.flow.screen == .band })
        #expect(!harness.flow.disconnecting)

        harness.app.disconnectAfterSessionStopForTesting = { await gate.hold() }
        let current = Task { await harness.flow.disconnect() }
        #expect(await settle(seconds: 2) { await gate.entered(3) })
        #expect(harness.flow.disconnecting)
        await gate.release(1)
        await old.value
        #expect(harness.flow.disconnecting)
        #expect(harness.flow.endingSessionForTesting)
        await gate.release(3)
        await current.value
        harness.app.disconnectAfterSessionStopForTesting = nil
        #expect(!harness.flow.disconnecting)
        #expect(!harness.flow.endingSessionForTesting)
        #expect(harness.flow.screen == .cores)
        #expect(harness.app.session == nil)
        #expect(Self.leaves(core).count == 2)
        #expect(core.messages.filter { $0.kind == .authRequest }.count == 2)
        await harness.app.disconnect()
    }

    @Test("an old model disconnect cannot erase a newly published session")
    func oldModelDisconnectKeepsNewSession() async throws {
        let core = try FakeStation(fixture: "session-device-sign-in")
        let harness = try Self.harness(factory: core.transportFactory, paired: [Self.paired(core)])
        await harness.flow.connect(to: try #require(harness.flow.cores.first))
        #expect(await settle { harness.flow.screen == .band })
        let gate = DisconnectGate()
        harness.app.disconnectAfterSessionStopForTesting = { await gate.hold() }
        let old = Task { await harness.app.disconnect() }
        #expect(await settle { await gate.entered(1) })
        let newer = Task { await harness.app.disconnect() }
        #expect(await settle { await gate.entered(2) })
        await gate.release(2)
        await newer.value
        #expect(harness.app.session == nil)

        harness.app.disconnectAfterSessionStopForTesting = nil
        let identity = try DeviceIdentity.load(store: KeychainKeyStore(item: harness.keyItem))
        let authenticator = try DeviceKeyAuthenticator(identity: identity, name: "iPhone", kind: .phone)
        await harness.app.connect(to: core.endpoint, trust: .identity(publicKey: core.identity.publicKey),
                                  authenticator: authenticator, transportFactory: core.transportFactory,
                                  clock: harness.clock)
        #expect(await settle { harness.app.connection == .connected })
        let newSession = try #require(harness.app.session)
        await gate.release(1)
        await old.value
        #expect(harness.app.session === newSession)
        #expect(harness.app.ownsPublishedSessionForTesting)
        #expect(harness.app.connection == .connected)
        await harness.app.disconnect()
    }

    // MARK: Link lost while listening

    @Test("the link lost while listening retries with Cancel, and back on the air the band resumes")
    func linkLostAndBack() async throws {
        let (station, harness) = try await connectedToFake()
        let flow = harness.flow
        await station.dropLink()
        #expect(await settle { flow.linkLost != nil })
        let lost = try #require(flow.linkLost)
        #expect(lost.attempt == 1)
        #expect(!lost.stopped)
        #expect(flow.screen == .band)
        #expect(LinkLostBanner.retryText(lost, now: lost.retryAt!.addingTimeInterval(-1)) == "Reconnecting, try 1: next in 1 s")
        // The redial goes when the wait is over, one connection at a time.
        await harness.clock.advance(by: 1000)
        #expect(await settle { harness.app.connection == .connected })
        #expect(await settle { flow.linkLost == nil })
        #expect(flow.backOnAir)
        #expect(station.connectionCount == 2)
        await harness.clock.advance(by: 4000)
        #expect(!flow.backOnAir)
        await harness.app.disconnect()
    }

    @Test("Cancel stops the retries; Reconnect starts them again")
    func cancelStopsRetrying() async throws {
        let (station, harness) = try await connectedToFake()
        let flow = harness.flow
        await station.dropLink()
        #expect(await settle { flow.linkLost != nil })
        await flow.cancelReconnecting()
        #expect(flow.linkLost?.stopped == true)
        #expect(harness.app.session == nil)
        await harness.clock.advance(by: 120_000)
        #expect(station.connectionCount == 1)
        await flow.reconnect()
        #expect(await settle { harness.app.connection == .connected })
        // The flow reads the model's states on its own task.
        #expect(await settle { flow.backOnAir })
        #expect(flow.linkLost == nil)
        #expect(station.connectionCount == 2)
        await harness.app.disconnect()
    }

    // MARK: Disconnect, on the Radio tab

    /// The `session.leave` requests the fake Core received.
    private static func leaves(_ station: FakeStation) -> [LinkMessage.CommandInvoke] {
        station.messages.compactMap { message in
            if case .commandInvoke(let invoke) = message, invoke.verb == ConnectionFlow.leaveVerb {
                return invoke
            }
            return nil
        }
    }

    @Test("Disconnect leaves the Core with session.leave, stops the session and media, shows Your Cores, and never redials")
    func disconnectLeaves() async throws {
        let (station, harness) = try await connectedToFake(additions: [.sessionLeave, .wideband])
        let flow = harness.flow
        #expect(await station.waitUntilLive())
        #expect(await settle { harness.app.connection == .connected && harness.flow.connectedCoreRow != nil })
        #expect(ConnectionFlow.leaves(harness.app.mirror))
        #expect(await settle(seconds: 30) { await harness.app.media.connectionId != nil })
        await flow.disconnect()
        #expect(flow.screen == .cores)
        let leaves = Self.leaves(station)
        #expect(leaves.count == 1)
        #expect(leaves.first?.args.isEmpty == true)
        #expect(harness.app.session == nil)
        #expect(harness.app.connection == .notConnected)
        #expect(await harness.app.media.connectionId == nil)
        #expect(flow.linkLost == nil)
        #expect(!flow.disconnecting)
        // The Core closing after the leave is the end the phone asked for:
        // nothing redials, then or later.
        await harness.clock.advance(by: 600_000)
        #expect(station.connectionCount == 1)
        #expect(flow.screen == .cores)
        #expect(harness.app.session == nil)

        // Connect, from Your Cores, works again.
        await flow.connect(to: try #require(flow.cores.first))
        #expect(await settle { flow.screen == .band })
        #expect(station.connectionCount == 2)
        await harness.app.disconnect()
    }

    @Test("cancelling after session.leave reaches the Core cannot undo that leave")
    func sleepExpiryCancelledAfterLeaveHandoff() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in", additions: [.sessionLeave])
        let replies = HeldCommandReplies()
        replies.hold(ConnectionFlow.leaveVerb)
        let harness = try Self.harness(factory: { endpoint, trust in
            ReplyHoldingTransport(inner: station.transportFactory(endpoint, trust), replies: replies)
        }, paired: [Self.paired(station)])
        let row = try #require(harness.flow.cores.first)
        await harness.flow.connect(to: row)
        #expect(await settle { harness.flow.screen == .band && harness.app.connection == .connected })
        let original = try #require(harness.app.session)
        let expiry = CommandSendPermit()
        let leaving = Task { await harness.flow.disconnect(ifCurrent: original, authority: expiry,
                                                            stillAllowed: { !expiry.isRevoked }) }
        #expect(await settle(seconds: 5) { replies.count(ConnectionFlow.leaveVerb) == 1 })
        expiry.revoke()
        await replies.release(ConnectionFlow.leaveVerb)
        _ = await leaving.value
        #expect(Self.leaves(station).count == 1)
        #expect(harness.app.session == nil)
        #expect(harness.flow.screen == .cores)
        await harness.app.disconnect()
    }

    @Test("a sleep leave already enqueued still finishes its old session when cancellation precedes sender return")
    func sleepExpiryCancelledAfterEnqueueBeforeSenderReturn() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in", additions: [.sessionLeave])
        let replies = HeldCommandReplies()
        replies.hold(ConnectionFlow.leaveVerb)
        let harness = try Self.harness(factory: { endpoint, trust in
            ReplyHoldingTransport(inner: station.transportFactory(endpoint, trust), replies: replies)
        }, paired: [Self.paired(station)])
        await harness.flow.connect(to: try #require(harness.flow.cores.first))
        #expect(await settle { harness.flow.screen == .band && harness.app.connection == .connected })
        let original = try #require(harness.app.session)
        let expiry = CommandSendPermit()
        let gate = SnapshotGate()
        let leaveVerb = ConnectionFlow.leaveVerb
        await original.holdAfterCommandHandoffForTesting { message in
            if case .commandInvoke(let invoke) = message, invoke.verb == leaveVerb {
                expiry.revoke()
                await gate.hold()
            }
        }
        let leaving = Task { await harness.flow.disconnect(ifCurrent: original, authority: expiry,
                                                            stillAllowed: { !expiry.isRevoked }) }
        #expect(await settle { await gate.hasEntered && Self.leaves(station).count == 1 })
        await replies.release(leaveVerb)
        #expect(await settle(seconds: 5) { !harness.flow.disconnecting })
        await original.holdAfterCommandHandoffForTesting(nil)
        await gate.release()
        #expect(await leaving.value)
        #expect(harness.app.session == nil)
        #expect(harness.flow.screen == .cores)
    }

    @Test("a cancelled unsent sleep leave replays the lost link into the retained retry UI")
    func sleepExpiryCancelledWhileLinkDrops() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in", additions: [.sessionLeave])
        let harness = try Self.harness(factory: station.transportFactory, paired: [Self.paired(station)])
        await harness.flow.connect(to: try #require(harness.flow.cores.first))
        #expect(await settle { harness.flow.screen == .band && harness.app.connection == .connected })
        let original = try #require(harness.app.session)
        let expiry = CommandSendPermit()
        let gate = SnapshotGate()
        let resultGate = SnapshotGate()
        harness.flow.sleepLeaveResultForTesting = { await resultGate.hold() }
        let leaveVerb = ConnectionFlow.leaveVerb
        harness.app.commandRouteForTesting.holdCommandHandoffForTesting { message in
            if case .commandInvoke(let invoke) = message, invoke.verb == leaveVerb { await gate.hold() }
        }
        let leaving = Task { await harness.flow.disconnect(ifCurrent: original, authority: expiry,
                                                            stillAllowed: { !expiry.isRevoked }) }
        #expect(await settle { await gate.hasEntered })
        await station.dropLink()
        #expect(await settle(seconds: 5) { await resultGate.hasEntered })
        #expect(await settle(seconds: 5) { if case .waitingToRetry = harness.app.connection { return true }; return false })
        expiry.revoke()
        harness.app.commandRouteForTesting.holdCommandHandoffForTesting(nil)
        await gate.release()
        await resultGate.release()
        #expect(await !leaving.value)
        #expect(await settle { harness.flow.linkLost != nil })
        #expect(harness.app.session === original)
        harness.flow.sleepLeaveResultForTesting = nil
        await harness.app.disconnect()
    }

    @Test("a locally refused sleep leave has no handoff and retains link loss after cancellation")
    func sleepExpiryRefusedBeforeCloseReachesSession() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in", additions: [.sessionLeave])
        let closeGate = SnapshotGate()
        let route = RefusingBeforeCloseTransport(inner: station.transportFactory(station.endpoint,
                                                     .identity(publicKey: station.identity.publicKey)),
                                                 closeGate: closeGate)
        let harness = try Self.harness(factory: { _, _ in route }, paired: [Self.paired(station)])
        await harness.flow.connect(to: try #require(harness.flow.cores.first))
        #expect(await settle { harness.flow.screen == .band && harness.app.connection == .connected })
        let original = try #require(harness.app.session)
        let expiry = CommandSendPermit()
        let resultGate = SnapshotGate()
        harness.flow.sleepLeaveResultForTesting = { await resultGate.hold() }

        route.refuseText()
        let dropping = Task { await station.dropLink() }
        #expect(await settle { await closeGate.hasEntered })
        let leaving = Task { await harness.flow.disconnect(ifCurrent: original, authority: expiry,
                                                            stillAllowed: { !expiry.isRevoked }) }
        #expect(await settle { route.refusedCount == 1 })
        #expect(Self.leaves(station).isEmpty)
        await harness.clock.advance(by: 2_000)
        #expect(await settle { await resultGate.hasEntered })
        expiry.revoke()
        await closeGate.release()
        await dropping.value
        #expect(await settle { if case .waitingToRetry = harness.app.connection { return true }; return false })
        await resultGate.release()
        #expect(await !leaving.value)
        #expect(await settle { harness.flow.linkLost != nil })
        #expect(harness.app.session === original)
        #expect(Self.leaves(station).isEmpty)
        harness.flow.sleepLeaveResultForTesting = nil
        await harness.app.disconnect()
    }

    @Test("loss and recovery during an unsent sleep leave still show Back on Air after cancellation")
    func sleepExpiryCancelledAfterHeldLossAndRecovery() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in", additions: [.sessionLeave])
        let harness = try Self.harness(factory: station.transportFactory, paired: [Self.paired(station)])
        await harness.flow.connect(to: try #require(harness.flow.cores.first))
        #expect(await settle { harness.flow.screen == .band && harness.app.connection == .connected })
        let original = try #require(harness.app.session)
        let expiry = CommandSendPermit()
        let gate = SnapshotGate()
        let resultGate = SnapshotGate()
        harness.flow.sleepLeaveResultForTesting = { await resultGate.hold() }
        let leaveVerb = ConnectionFlow.leaveVerb
        harness.app.commandRouteForTesting.holdCommandHandoffForTesting { message in
            if case .commandInvoke(let invoke) = message, invoke.verb == leaveVerb { await gate.hold() }
        }
        let leaving = Task { await harness.flow.disconnect(ifCurrent: original, authority: expiry,
                                                            stillAllowed: { !expiry.isRevoked }) }
        #expect(await settle { await gate.hasEntered })
        await station.dropLink()
        #expect(await settle(seconds: 5) { await resultGate.hasEntered })
        #expect(await settle(seconds: 5) { if case .waitingToRetry = harness.app.connection { return true }; return false })
        #expect(await settle { harness.clock.pendingDueTimes.contains(harness.clock.now + 1_000) })
        await harness.clock.advance(by: 1_000)
        #expect(await settle(seconds: 5) { harness.app.connection == .connected })
        expiry.revoke()
        harness.app.commandRouteForTesting.holdCommandHandoffForTesting(nil)
        await gate.release()
        await resultGate.release()
        #expect(await !leaving.value)
        #expect(await settle { harness.flow.backOnAir })
        #expect(harness.app.session === original)
        harness.flow.sleepLeaveResultForTesting = nil
        await harness.app.disconnect()
    }

    @Test("an old replay held before the flow pump cannot report a link loss on a replacement")
    func sleepExpiryReplayRejectsReplacementAtConsumption() async throws {
        let old = try FakeStation(fixture: "session-device-sign-in", additions: [.sessionLeave])
        let newer = try FakeStation(fixture: "session-device-sign-in", additions: [.sessionLeave])
        let harness = try Self.harness(factory: old.transportFactory, paired: [Self.paired(old)])
        await harness.flow.connect(to: try #require(harness.flow.cores.first))
        #expect(await settle { harness.app.connection == .connected && harness.flow.connectedCoreRow != nil })
        let original = try #require(harness.app.session)
        let expiry = CommandSendPermit()
        let sendGate = SnapshotGate()
        let resultGate = SnapshotGate()
        let replayGate = SnapshotGate()
        let decision = AsyncStream.makeStream(of: Bool.self)
        harness.flow.sleepLeaveResultForTesting = { await resultGate.hold() }
        harness.flow.retainedSleepInputForTesting = { await replayGate.hold() }
        harness.flow.retainedSleepDecisionForTesting = { decision.continuation.yield($0) }
        let leaveVerb = ConnectionFlow.leaveVerb
        harness.app.commandRouteForTesting.holdCommandHandoffForTesting { message in
            if case .commandInvoke(let invoke) = message, invoke.verb == leaveVerb { await sendGate.hold() }
        }
        let leaving = Task { await harness.flow.disconnect(ifCurrent: original, authority: expiry,
                                                            stillAllowed: { !expiry.isRevoked }) }
        #expect(await settle { await sendGate.hasEntered })
        await old.dropLink()
        #expect(await settle(seconds: 5) { await resultGate.hasEntered })
        #expect(await settle(seconds: 5) { if case .waitingToRetry = harness.app.connection { return true }; return false })
        expiry.revoke()
        harness.app.commandRouteForTesting.holdCommandHandoffForTesting(nil)
        await sendGate.release()
        await resultGate.release()
        #expect(await !leaving.value)
        #expect(await settle(seconds: 5) { await replayGate.hasEntered })
        await harness.app.connect(to: newer.endpoint, trust: newer.trust,
                                  authenticator: newer.authenticator, transportFactory: newer.transportFactory)
        #expect(await settle(seconds: 5) { harness.app.connection == .connected && harness.app.session !== original })
        await replayGate.release()
        var observed: Bool?
        for await current in decision.stream { observed = current; break }
        #expect(observed == false)
        #expect(harness.flow.linkLost == nil)
        harness.flow.sleepLeaveResultForTesting = nil
        harness.flow.retainedSleepInputForTesting = nil
        harness.flow.retainedSleepDecisionForTesting = nil
        decision.continuation.finish()
        await harness.app.disconnect()
    }

    @Test("an old sleep leave completion cannot erase a replacement's retained link loss")
    func oldSleepLeaveCompletionKeepsNewRetryNews() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in", additions: [.sessionLeave])
        let harness = try Self.harness(factory: station.transportFactory, paired: [Self.paired(station)])
        let row = try #require(harness.flow.cores.first)
        await harness.flow.connect(to: row)
        #expect(await settle { harness.app.connection == .connected && harness.flow.connectedCoreRow != nil })
        let original = try #require(harness.app.session)
        let resultGate = DisconnectGate()
        harness.flow.sleepLeaveResultForTesting = { await resultGate.hold() }
        let oldExpiry = CommandSendPermit()
        let oldLeave = Task { await harness.flow.disconnect(ifCurrent: original, authority: oldExpiry,
                                                             stillAllowed: { !oldExpiry.isRevoked }) }
        #expect(await settle { await resultGate.entered(1) })

        await harness.flow.connect(to: row)
        #expect(await settle { harness.app.connection == .connected && harness.app.session !== original && harness.flow.connectedCoreRow != nil })
        let replacement = try #require(harness.app.session)
        let sendGate = SnapshotGate()
        let leaveVerb = ConnectionFlow.leaveVerb
        harness.app.commandRouteForTesting.holdCommandHandoffForTesting { message in
            if case .commandInvoke(let invoke) = message, invoke.verb == leaveVerb {
                await sendGate.hold()
            }
        }
        let newExpiry = CommandSendPermit()
        let newLeave = Task { await harness.flow.disconnect(ifCurrent: replacement, authority: newExpiry,
                                                             stillAllowed: { !newExpiry.isRevoked }) }
        #expect(await settle { await sendGate.hasEntered })
        await station.dropLink()
        #expect(await settle(seconds: 5) {
            if case .waitingToRetry = harness.app.connection { return true }
            return false
        })
        newExpiry.revoke()
        harness.app.commandRouteForTesting.holdCommandHandoffForTesting(nil)
        await sendGate.release()
        #expect(await settle { await resultGate.entered(2) })

        await resultGate.release(1)
        #expect(await !oldLeave.value)
        await resultGate.release(2)
        #expect(await !newLeave.value)
        #expect(await settle { harness.flow.linkLost != nil })
        #expect(harness.app.session === replacement)
        #expect(Self.leaves(station).count == 1, "only the old session handed leave to the Core")
        harness.flow.sleepLeaveResultForTesting = nil
        await harness.app.disconnect()
    }

    @Test("cancelled queued sleep leave preserves the old session's retry and cannot reach a replacement")
    func sleepExpiryCancelledBeforeLeaveHandoff() async throws {
        let old = try FakeStation(fixture: "session-device-sign-in", additions: [.sessionLeave])
        let newer = try FakeStation(fixture: "session-device-sign-in", additions: [.sessionLeave])
        let harness = try Self.harness(factory: old.transportFactory, paired: [Self.paired(old)])
        let row = try #require(harness.flow.cores.first)
        await harness.flow.connect(to: row)
        #expect(await settle { harness.flow.screen == .band && harness.app.connection == .connected })
        let original = try #require(harness.app.session)
        let expiry = CommandSendPermit()
        let gate = SnapshotGate()
        let leaveVerb = ConnectionFlow.leaveVerb
        harness.app.commandRouteForTesting.holdCommandHandoffForTesting { message in
            if case .commandInvoke(let invoke) = message, invoke.verb == leaveVerb {
                await gate.hold()
            }
        }
        let leaving = Task { await harness.flow.disconnect(ifCurrent: original, authority: expiry,
                                                            stillAllowed: { !expiry.isRevoked }) }
        for _ in 0..<50_000 { if await gate.hasEntered { break }; await Task.yield() }
        #expect(await gate.hasEntered)
        expiry.revoke()
        harness.app.commandRouteForTesting.holdCommandHandoffForTesting(nil)
        await gate.release()
        #expect(await !leaving.value)
        #expect(Self.leaves(old).isEmpty)
        #expect(harness.app.session === original)
        await old.dropLink()
        #expect(await settle { harness.flow.linkLost != nil })
        #expect(await settle { harness.clock.pendingDueTimes.contains(harness.clock.now + 1_000) })
        await harness.clock.advance(by: 1_000)
        #expect(await settle { harness.app.connection == .connected })
        #expect(old.connectionCount == 2)
        await harness.app.disconnect()
        await harness.app.connect(to: newer.endpoint, trust: newer.trust,
                                  authenticator: newer.authenticator, transportFactory: newer.transportFactory)
        #expect(await settle { harness.app.connection == .connected })
        #expect(Self.leaves(newer).isEmpty)
        await harness.app.disconnect()
    }

    @Test("Disconnect from a Core without session.leave closes the connection and sends nothing it can't take")
    func disconnectCloses() async throws {
        let (station, harness) = try await connectedToFake()
        let flow = harness.flow
        #expect(await station.waitUntilLive())
        #expect(!ConnectionFlow.leaves(harness.app.mirror))
        await flow.disconnect()
        #expect(flow.screen == .cores)
        #expect(Self.leaves(station).isEmpty)
        #expect(harness.app.session == nil)
        #expect(harness.log.all.last == "close session")
        await harness.clock.advance(by: 600_000)
        #expect(station.connectionCount == 1)
    }

    @Test("Disconnect while the link is being retried cancels the retries and shows Your Cores")
    func disconnectWhileReconnecting() async throws {
        let (station, harness) = try await connectedToFake()
        let flow = harness.flow
        harness.reach.way = .hangs
        await station.dropLink()
        #expect(await settle { flow.linkLost != nil })
        // The retry's dial hangs, so the clock's step returns only once it is closed.
        #expect(await settle { harness.clock.pendingDueTimes.contains(harness.clock.now + 1_000) })
        let retry = Task { await harness.clock.advance(by: 1000) }
        #expect(await settle { harness.log.all.filter { $0 == "open session" }.count == 2 })
        #expect(flow.screen == .band)
        await flow.disconnect()
        await retry.value
        #expect(flow.screen == .cores)
        #expect(flow.linkLost == nil)
        #expect(flow.connectingTo == nil)
        #expect(harness.app.session == nil)
        await harness.clock.advance(by: 600_000)
        #expect(harness.log.all.filter { $0 == "open session" }.count == 2)
        #expect(Self.leaves(station).isEmpty)
    }

    @Test("Disconnect while a first connection is still being made cancels it")
    func disconnectWhileConnecting() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        let harness = try Self.harness(factory: station.transportFactory, paired: [Self.paired(station)])
        let flow = harness.flow
        harness.reach.way = .hangs
        let row = try #require(flow.cores.first)
        let connecting = Task { await flow.connect(to: row) }
        #expect(await settle { flow.connectingTo != nil && harness.log.all.contains("open session") })
        await flow.disconnect()
        await connecting.value
        #expect(flow.connectingTo == nil)
        #expect(flow.screen == .cores)
        #expect(harness.app.session == nil)
        #expect(flow.trouble == nil)
        await harness.clock.advance(by: 600_000)
        #expect(harness.log.all.filter { $0 == "open session" }.count == 1)
    }

    @Test("Repeated Remove Core cancellation publishes once and leaves its saved record local")
    func repeatedRemoveCancellationPublishesOnce() throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        let harness = try Self.harness(factory: station.transportFactory, paired: [Self.paired(station)])
        let id = station.identity.publicKey
        let saved = try harness.stationItem.read()
        harness.flow.requestRemove(identityKey: id)
        #expect(harness.flow.removeCandidate?.id == id)

        var candidateUpdates: [Data?] = []
        let watch = harness.flow.$removeCandidate.dropFirst().sink { candidateUpdates.append($0?.id) }
        defer { watch.cancel() }
        harness.flow.cancelRemove()
        #expect(harness.flow.removeCandidate == nil)
        #expect(candidateUpdates == [nil])
        harness.flow.cancelRemove()
        harness.flow.cancelRemove()
        #expect(candidateUpdates == [nil], "dismissal callbacks must not republish an already cleared candidate")
        #expect(harness.flow.cores.map(\.id) == [id])
        #expect(try harness.stations.station(identityKey: id) != nil)
        #expect(try harness.stationItem.read() == saved)
        #expect(station.connectionCount == 0)
        #expect(harness.app.session == nil)

        let afterCancellation = candidateUpdates.count
        harness.flow.confirmRemove()
        #expect(candidateUpdates.count == afterCancellation)
        #expect(try harness.stationItem.read() == saved)
        harness.flow.requestRemove(identityKey: id)
        #expect(harness.flow.removeCandidate?.id == id)
        #expect(candidateUpdates.last.flatMap { $0 } == id)
        harness.flow.cancelRemove()
        #expect(harness.flow.removeCandidate == nil)
        #expect(try harness.stationItem.read() == saved)
    }

    @Test("Remove Core leaves an active session before deleting its last saved record")
    func removeConnectedCore() async throws {
        let (station, harness) = try await connectedToFake(additions: [.sessionLeave])
        let id = station.identity.publicKey
        #expect(await station.waitUntilLive())
        #expect(harness.flow.radioRemoveReason == nil)
        #expect(await harness.flow.removeCore(identityKey: id))
        #expect(Self.leaves(station).count == 1)
        #expect(harness.app.session == nil)
        #expect(harness.flow.screen == .cores)
        #expect(harness.flow.cores.isEmpty)
        #expect(try harness.stations.all().isEmpty)
        #expect(try harness.stationItem.read() == nil)
        await harness.clock.advance(by: 600_000)
        #expect(station.connectionCount == 1)
    }

    @Test("Removing another saved Core leaves the active one and its key untouched")
    func removeUnrelatedCore() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        let otherID = Data(repeating: 91, count: 32)
        let other = PairedStation(identityKey: otherID, label: "Other",
                                  endpoints: [StationEndpoint(host: "other.invalid")])
        let harness = try Self.harness(factory: station.transportFactory,
                                       paired: [Self.paired(station), other])
        await harness.flow.connect(to: try #require(harness.flow.cores.first))
        #expect(await settle { harness.flow.screen == .band })
        let session = try #require(harness.app.session)
        let key = try #require(try harness.keyItem.read())
        #expect(await harness.flow.removeCore(identityKey: otherID))
        #expect(harness.app.session === session)
        #expect(Self.leaves(station).isEmpty)
        #expect(try harness.stations.station(identityKey: otherID) == nil)
        #expect(try harness.stations.station(identityKey: station.identity.publicKey) != nil)
        #expect(try harness.keyItem.read() == key)
        await harness.app.disconnect()
    }

    @Test("Remove Core cancels a pending dial and a delayed opening cannot restore it")
    func removeConnectingCore() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        let held = HeldOpen()
        let harness = try Self.harness(factory: { _, _ in held }, paired: [Self.paired(station)])
        let id = station.identity.publicKey
        let connecting = Task { await harness.flow.connect(to: harness.flow.cores[0]) }
        #expect(await settle { held.hasStarted && harness.flow.connectingTo == id })
        #expect(await harness.flow.removeCore(identityKey: id))
        held.finish()
        await connecting.value
        #expect(try harness.stations.station(identityKey: id) == nil)
        #expect(harness.flow.cores.isEmpty)
        #expect(harness.app.session == nil)
    }

    @Test("An offline saved Core can be removed without starting a connection")
    func removeOfflineCore() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        let harness = try Self.harness(factory: station.transportFactory, paired: [Self.paired(station)])
        harness.network.set(online: false)
        #expect(harness.flow.radioRemoveReason == ConnectionFlow.removeBusyText)
        #expect(await harness.flow.removeCore(identityKey: station.identity.publicKey))
        #expect(harness.flow.cores.isEmpty)
        #expect(try harness.stations.all().isEmpty)
        #expect(station.connectionCount == 0)
        #expect(harness.app.session == nil)
    }

    @Test("Remove Core cancels a reconnect already opening")
    func removeReconnectingCore() async throws {
        let (station, harness) = try await connectedToFake()
        harness.reach.way = .hangs
        await station.dropLink()
        #expect(await settle { harness.flow.linkLost != nil })
        #expect(await settle { harness.clock.pendingDueTimes.contains(harness.clock.now + 1_000) })
        let retry = Task { await harness.clock.advance(by: 1000) }
        #expect(await settle { harness.log.all.filter { $0 == "open session" }.count == 2 })
        #expect(await harness.flow.removeCore(identityKey: station.identity.publicKey))
        await retry.value
        #expect(harness.app.session == nil)
        #expect(harness.flow.linkLost == nil)
        #expect(try harness.stations.station(identityKey: station.identity.publicKey) == nil)
        await harness.clock.advance(by: 600_000)
        #expect(harness.log.all.filter { $0 == "open session" }.count == 2)
    }

    @Test("Remove Core stays disabled while this phone transmits")
    func removeTransmittingCoreIsGated() async throws {
        let (station, harness) = try await connectedToFake(additions: [.remoteTx, .wideband])
        let transmit = harness.app.main.transmit
        #expect(await station.waitUntilLive())
        await station.deliver(.objectCreate(LinkMessage.ObjectCreate(
            key: "txState", className: "TransmitState",
            properties: TransmitScreenTests.holder("", short: "", keyed: false)
                + [.init(ordinal: 17, name: "stopSerial", value: .i64(0))])))
        await TransmitScreenTests.fillTransmit(station)
        #expect(await settle(seconds: 5) { transmit.permitted })
        transmit.tapPtt()
        #expect(await settle(seconds: 5) { transmit.ptt.state.isKeyed })
        let session = try #require(harness.app.session)
        #expect(harness.flow.removeDisabledReason(identityKey: station.identity.publicKey)
                == ConnectionFlow.removeTransmitText)
        #expect(harness.flow.radioRemoveReason == ConnectionFlow.removeTransmitText)
        #expect(!(await harness.flow.removeCore(identityKey: station.identity.publicKey)))
        #expect(harness.app.session === session)
        #expect(try harness.stations.station(identityKey: station.identity.publicKey) != nil)
        transmit.tapPtt()
        #expect(await settle(seconds: 5) { transmit.ptt.state == .idle })
        await harness.flow.disconnect()
    }

    @Test("Failed removal keeps the saved row and flags until a visible retry succeeds")
    func removeStorageFailureAndRetry() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        let item = FailingStationItem()
        let harness = try Self.harness(factory: station.transportFactory, paired: [Self.paired(station)],
                                       failingStationItem: item)
        let id = station.identity.publicKey
        let encoded = Base64URL.encode(id)
        harness.app.phoneSettings.coresNeedingPairing = [encoded]
        harness.app.phoneSettings.coresWithoutRename = [encoded]
        item.failRemoval = true
        #expect(!(await harness.flow.removeCore(identityKey: id)))
        #expect(harness.flow.removeProblem?.id == id)
        #expect(harness.flow.cores.count == 1)
        #expect(try harness.stations.station(identityKey: id) != nil)
        #expect(harness.app.phoneSettings.coresNeedingPairing.contains(encoded))
        #expect(harness.app.phoneSettings.coresWithoutRename.contains(encoded))
        item.failRemoval = false
        #expect(await harness.flow.removeCore(identityKey: id))
        #expect(harness.flow.removeProblem == nil)
        #expect(harness.flow.cores.isEmpty)
        #expect(!harness.app.phoneSettings.coresNeedingPairing.contains(encoded))
        #expect(!harness.app.phoneSettings.coresWithoutRename.contains(encoded))
    }

    @Test("A failed multi-Core Keychain write preserves both saved entries")
    func removeWriteFailure() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        let other = PairedStation(identityKey: Data(repeating: 73, count: 32), label: "Other",
                                  endpoints: [StationEndpoint(host: "other.invalid")])
        let item = FailingStationItem()
        let harness = try Self.harness(factory: station.transportFactory,
                                       paired: [Self.paired(station), other], failingStationItem: item)
        item.failRemoval = true
        #expect(!(await harness.flow.removeCore(identityKey: station.identity.publicKey)))
        #expect(harness.flow.removeProblem?.id == station.identity.publicKey)
        #expect(harness.flow.cores.count == 2)
        #expect(try harness.stations.all().count == 2)
        item.failRemoval = false
        #expect(await harness.flow.removeCore(identityKey: station.identity.publicKey))
        #expect(try harness.stations.all() == [other])
    }

    @Test("A Core's revoked notice and Pair flag leave with its saved entry")
    func removeRevokedCore() async throws {
        let (station, harness) = try await connectedToFake()
        await station.deliver(.sessionEnd(LinkMessage.SessionEnd(
            reason: "This device was removed from the Core.", retryable: false, code: "deviceRemoved")))
        #expect(await settle { harness.flow.notice == .removed(core: "Fake Core") })
        #expect(harness.flow.cores.first?.needsPairing == true)
        #expect(await harness.flow.removeCore(identityKey: station.identity.publicKey))
        #expect(harness.flow.notice == nil)
        #expect(harness.flow.cores.isEmpty)
        #expect(!harness.app.phoneSettings.coresNeedingPairing.contains(Base64URL.encode(station.identity.publicKey)))
    }

    @Test("Discovery cannot resave a removed Core; deliberate pairing can")
    func removedCoreDiscoveryAndPair() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in", requiresPairing: true)
        let browser = FakeStationBrowser()
        let harness = try Self.harness(factory: station.transportFactory,
                                       paired: [Self.paired(station)], browser: browser)
        let flow = harness.flow
        #expect(await settle { await browser.isBrowsing })
        #expect(await flow.removeCore(identityKey: station.identity.publicKey))
        #expect(try harness.stations.all().isEmpty)
        await browser.announce([station.foundStation])
        #expect(await settle { flow.nearby.count == 1 })
        #expect(flow.cores.isEmpty)
        #expect(try harness.stations.all().isEmpty)
        let offer = try #require(flow.nearby.first)
        #expect(offer.offer == .oneTap)
        harness.microphone.needsAsking = false
        await flow.pairNearby(offer)
        #expect(await settle { flow.screen == .band })
        #expect(try harness.stations.all().map(\.identityKey) == [station.identity.publicKey])
        await harness.app.disconnect()
    }

    @Test("An obsolete safe disconnect cannot remove the saved Core")
    func obsoleteRemoveKeepsCore() async throws {
        let (station, harness) = try await connectedToFake(additions: [.sessionLeave])
        let gate = DisconnectGate()
        harness.app.disconnectAfterSessionStopForTesting = { await gate.hold() }
        let removal = Task { await harness.flow.removeCore(identityKey: station.identity.publicKey) }
        #expect(await settle(seconds: 2) { await gate.entered(1) })
        harness.flow.advanceResumeIntentForTesting()
        await gate.release(1)
        #expect(!(await removal.value))
        #expect(try harness.stations.station(identityKey: station.identity.publicKey) != nil)
        #expect(harness.flow.cores.count == 1)
        harness.app.disconnectAfterSessionStopForTesting = nil
    }

    @Test("Remove waits for old Core retirement while a different dial owns the intent")
    func removeOldCoreDuringNewDial() async throws {
        let old = try FakeStation(fixture: "session-device-sign-in")
        let next = try FakeStation(fixture: "session-device-sign-in")
        let harness = try Self.harness(factory: { endpoint, trust in
            endpoint == old.endpoint ? old.transportFactory(endpoint, trust)
                : next.transportFactory(endpoint, trust)
        }, paired: [Self.paired(old), Self.paired(next)])
        await harness.flow.connect(to: try #require(harness.flow.cores.first))
        #expect(await settle { harness.flow.screen == .band })
        let gate = DisconnectGate()
        harness.app.disconnectAfterSessionStopForTesting = { await gate.hold() }
        let nextRow = try #require(harness.flow.cores.first { $0.id == next.identity.publicKey })
        let dialNext = Task { await harness.flow.connect(to: nextRow) }
        #expect(await settle(seconds: 2) { await gate.entered(1) })
        #expect(harness.flow.removeDisabledReason(identityKey: old.identity.publicKey)
                == ConnectionFlow.removeBusyText)
        #expect(!(await harness.flow.removeCore(identityKey: old.identity.publicKey)))
        await gate.release(1)
        await dialNext.value
        harness.app.disconnectAfterSessionStopForTesting = nil
        #expect(await settle { harness.flow.screen == .band
            && harness.flow.selectedCoreIdentity == next.identity.publicKey })
        let session = try #require(harness.app.session)
        #expect(await harness.flow.removeCore(identityKey: old.identity.publicKey))
        #expect(harness.app.session === session)
        #expect(try harness.stations.station(identityKey: old.identity.publicKey) == nil)
        #expect(try harness.stations.station(identityKey: next.identity.publicKey) != nil)
        await harness.app.disconnect()
    }

    @Test("Remove of new dial target cancels that intent without deleting the old Core")
    func removeNewCoreDuringOldRetirement() async throws {
        let old = try FakeStation(fixture: "session-device-sign-in")
        let next = try FakeStation(fixture: "session-device-sign-in")
        let harness = try Self.harness(factory: { endpoint, trust in
            endpoint == old.endpoint ? old.transportFactory(endpoint, trust)
                : next.transportFactory(endpoint, trust)
        }, paired: [Self.paired(old), Self.paired(next)])
        await harness.flow.connect(to: try #require(harness.flow.cores.first))
        #expect(await settle { harness.flow.screen == .band })
        let gate = DisconnectGate()
        harness.app.disconnectAfterSessionStopForTesting = { await gate.hold() }
        let nextRow = try #require(harness.flow.cores.first { $0.id == next.identity.publicKey })
        let dialNext = Task { await harness.flow.connect(to: nextRow) }
        #expect(await settle(seconds: 2) { await gate.entered(1) })
        let removal = Task { await harness.flow.removeCore(identityKey: next.identity.publicKey) }
        #expect(await settle(seconds: 2) { await gate.entered(2) })
        await gate.release(1)
        await dialNext.value
        await gate.release(2)
        #expect(await removal.value)
        harness.app.disconnectAfterSessionStopForTesting = nil
        #expect(try harness.stations.station(identityKey: next.identity.publicKey) == nil)
        #expect(try harness.stations.station(identityKey: old.identity.publicKey) != nil)
        #expect(harness.app.session == nil)
    }

    @Test("the Radio tab's Core and link: the name or the address, and no round trip until one is measured")
    func radioCoreCardWords() {
        #expect(RadioCoreCard.name(coreName: "KG4VCF/shack", coreHost: "192.0.2.40") == "KG4VCF/shack")
        #expect(RadioCoreCard.name(coreName: nil, coreHost: "192.0.2.40") == "192.0.2.40")
        let dot = "\u{00B7}"
        #expect(RadioCoreCard.status(LinkState(connection: .connected, roundTripMs: nil)) == "Connected \(dot) direct")
        #expect(RadioCoreCard.status(LinkState(connection: .connected, roundTripMs: 38))
                == "Connected \(dot) direct \(dot) 38 ms")
        #expect(RadioCoreCard.status(LinkState(connection: .waitingToRetry(seconds: 2, reason: nil), roundTripMs: nil))
                == "Reconnecting")
        #expect(RadioCoreCard.status(LinkState(connection: .connecting, roundTripMs: nil)) == "Connecting")
        #expect(RadioCoreCard.status(LinkState(connection: .connected, roundTripMs: 38, offline: true)) == "Offline")
        #expect(RadioCoreCard.status(LinkState(connection: .notConnected, roundTripMs: nil)) == "Not connected")
    }

    // MARK: Sign-in ends

    @Test("a Core that removed this phone keeps it listed with Pair, and pairing again replaces what was kept",
          arguments: [true, false])
    func removedPhonePairsAgain(withCode: Bool) async throws {
        let (station, harness) = try await connectedToFake()
        let flow = harness.flow
        await station.deliver(.sessionEnd(LinkMessage.SessionEnd(reason: "This device was removed from the Core.",
                                                                 retryable: false,
                                                                 code: withCode ? "deviceRemoved" : nil)))
        #expect(await settle { flow.screen == .cores })
        #expect(flow.notice == .removed(core: "Fake Core"))
        let row = try #require(flow.cores.first)
        #expect(row.needsPairing)
        // Connect is Pair now, and opens the code with the address filled in.
        await flow.connect(to: row)
        #expect(flow.screen == .pairByCode)
        #expect(flow.pairTarget?.endpoint == station.endpoint)
        #expect(flow.pairTarget?.replacing == row.id)
        flow.codeText = station.pairingCode
        await flow.pair()
        #expect(await settle { flow.screen == .band })
        #expect(flow.cores.map(\.needsPairing) == [false])
        #expect(try harness.stations.all().count == 1)
        await harness.app.disconnect()
    }

    @Test("a Core that forgot this phone is the same notice")
    func forgottenPhone() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in", requiresPairing: true)
        let harness = try Self.harness(factory: station.transportFactory, paired: [Self.paired(station)])
        let row = try #require(harness.flow.cores.first)
        await harness.flow.connect(to: row)
        #expect(await settle { harness.flow.notice != nil })
        #expect(harness.flow.notice == .removed(core: "Fake Core"))
        #expect(harness.flow.cores.first?.needsPairing == true)
    }

    @Test("taken over: the Core's words, and the phone never redials on its own")
    func takenOverNeverRedials() async throws {
        let (station, harness) = try await connectedToFake()
        let words = "Another app at 192.0.2.5:50000 connected to the Core and took over. Connect again to take it back."
        await station.deliver(.sessionEnd(LinkMessage.SessionEnd(reason: words, retryable: false, code: "takenOver")))
        #expect(await settle { harness.flow.screen == .cores })
        #expect(harness.flow.notice == .words(words))
        await harness.clock.advance(by: 600_000)
        #expect(station.connectionCount == 1)
        #expect(harness.flow.cores.first?.needsPairing == false)
    }

    @Test("sign-ins limited for a while: the flow waits, tries once more, then shows the Core's words")
    func signInLimited() async throws {
        let station = try FakeStation(fixture: "session-lockout")
        let harness = try Self.harness(factory: station.transportFactory, paired: [Self.paired(station)])
        let flow = harness.flow
        let row = try #require(flow.cores.first)
        await flow.connect(to: row)
        let words = "The Core is refusing pairing tokens for a while after too many wrong ones. Try again later."
        #expect(await settle { flow.connectingNote == words })
        #expect(station.connectionCount == 1)
        // Nothing more until the wait is over.
        await harness.clock.advance(by: 59_000)
        #expect(station.connectionCount == 1)
        await harness.clock.advance(by: 1000)
        #expect(await settle { flow.notice == .words(words) })
        #expect(station.connectionCount == 2)
        await harness.clock.advance(by: 600_000)
        #expect(station.connectionCount == 2)
    }

    // MARK: The key (D72)

    @Test("a key the phone can't read is never replaced on its own; Make a new key replaces it, and every Core offers Pair")
    func unreadableKey() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        let garbage = Data([1, 2, 3])
        let item = InMemorySecretItem(garbage)
        let harness = try Self.harness(factory: station.transportFactory, paired: [Self.paired(station)], keyItem: item)
        let flow = harness.flow
        #expect(flow.screen == .cores)
        #expect(flow.notice == .keyUnreadable)
        #expect(flow.keyUnreadable)
        let row = try #require(flow.cores.first)
        #expect(row.needsPairing)
        await flow.connect(to: row)
        #expect(station.connectionCount == 0)
        #expect(try item.read() == garbage)

        flow.makeNewKey()
        #expect(try item.read() != garbage)
        #expect(!flow.keyUnreadable)
        #expect(flow.notice == nil)
        #expect(flow.madeNewKey)
        #expect(flow.cores.map(\.needsPairing) == [true])
        // Pair opens the code for that Core; afterwards the note goes.
        await flow.connect(to: try #require(flow.cores.first))
        #expect(flow.screen == .pairByCode)
        flow.codeText = station.pairingCode
        await flow.pair()
        #expect(await settle { flow.screen == .band })
        #expect(!flow.madeNewKey)
        await harness.app.disconnect()
    }

    // MARK: Finding a Core on this network (D36, D71)

    private static func found(_ prefix: String, claimed: Bool, pairing: FoundStation.Pairing,
                              host: String = "192.0.2.40", label: String = "") -> FoundStation {
        FoundStation(instanceName: "Core \(prefix)", label: label, identityPrefix: prefix, claimed: claimed,
                     pairing: pairing, endpoint: StationEndpoint(host: host))
    }

    @Test("On this network lists every Core that takes a new device, and a paired Core by its key")
    func listingFollowsD71() throws {
        let key = Data((0..<91).map { UInt8($0) })
        let paired = PairedStation(identityKey: key, label: "KG4VCF/shack", endpoints: [StationEndpoint(host: "shack")])
        let prefix = FoundStation.identityPrefix(of: key)
        var unresolved = Self.found("u", claimed: false, pairing: .click)
        unresolved.endpoint = nil
        let found = [
            Self.found(prefix, claimed: true, pairing: .closed),
            Self.found("a", claimed: false, pairing: .click),
            Self.found("b", claimed: false, pairing: .code),
            Self.found("c", claimed: true, pairing: .code),
            Self.found("d", claimed: false, pairing: .closed),
            Self.found("e", claimed: true, pairing: .closed),
            Self.found("f", claimed: true, pairing: .click),
            unresolved,
        ]
        let listing = ConnectionFlow.listing(found, paired: [paired])
        #expect(listing.nearby.map(\.id) == ["a", "b", "c", "d"])
        #expect(listing.nearby.map(\.offer) == [.oneTap, .code, .code, .closed])
        #expect(listing.matched[key]?.identityPrefix == prefix)
        // A row shows the instance name when the label is empty.
        #expect(listing.nearby.first?.label == "Core a")
        #expect(listing.nearby.first?.address == "192.0.2.40")
    }

    @Test("Find my Core looks on this Wi-Fi only on the connecting screens, and says when iOS won't let it")
    func browsingFollowsTheScreens() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        let browser = FakeStationBrowser()
        let flow = try Self.harness(factory: station.transportFactory, browser: browser).flow
        #expect(flow.screen == .welcome)
        await MainQueue.drained()
        await MainQueue.drained()
        #expect(await browser.starts == 0)
        flow.findMyCore()
        #expect(flow.screen == .findCore)
        #expect(await settle { await browser.isBrowsing })
        await browser.announce([], localNetworkDenied: true)
        #expect(await settle { flow.lookingDenied })
        // A code and an address still work.
        flow.enterAddress()
        #expect(flow.screen == .typeAddress)
        flow.back()
        #expect(flow.screen == .findCore)
        flow.back()
        #expect(flow.screen == .welcome)
        #expect(await settle { await !browser.isBrowsing })
        #expect(!flow.lookingDenied)
        #expect(await browser.starts == 1)
    }

    /// Settles on an async condition.
    private func settle(seconds: Double = 30, _ condition: () async -> Bool) async -> Bool {
        let deadline = Date().addingTimeInterval(seconds)
        while Date() < deadline {
            if await condition() {
                return true
            }
            try? await Task.sleep(for: .milliseconds(5))
        }
        return await condition()
    }

    @Test("Found it claims an unclaimed Core in one tap and reaches the band with no other tap")
    func foundItClaimsInOneTap() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in", requiresPairing: true)
        let browser = FakeStationBrowser()
        let harness = try Self.harness(factory: station.transportFactory, browser: browser)
        harness.microphone.needsAsking = false
        let flow = harness.flow
        flow.findMyCore()
        #expect(await settle { await browser.isBrowsing })
        await browser.announce([station.foundStation])
        #expect(await settle { flow.nearby.count == 1 })
        let row = try #require(flow.nearby.first)
        #expect(row.offer == .oneTap)
        #expect(row.label == station.label)
        #expect(row.endpoint == station.endpoint)
        await flow.pairNearby(row)
        #expect(await settle { flow.screen == .band })
        #expect(try harness.stations.all().map(\.identityKey) == [station.identity.publicKey])
        #expect(station.pairedDeviceKeys.count == 1)
        #expect(station.pairingConnectionCount == 1)
        // The pairing closed before the sign-in opened, both to the found address.
        let log = harness.log.all
        #expect(try #require(log.firstIndex(of: "close pairing")) < (try #require(log.firstIndex(of: "open session"))))
        #expect(flow.cores.first?.station.endpoints == [station.endpoint])
        // Found again, now claimed, it names the paired Core and isn't offered.
        await flow.leaveBand()
        await browser.announce([station.foundStation])
        #expect(await settle { flow.cores.first?.found != nil })
        #expect(flow.nearby.isEmpty)
        #expect(flow.cores.first?.needsPairing == false)
    }

    @Test("a found Core taking the code opens the code with its address filled in")
    func foundItWithTheCode() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in", requiresPairing: true)
        station.allowsOneTap = false
        let browser = FakeStationBrowser()
        let harness = try Self.harness(factory: station.transportFactory, browser: browser)
        let flow = harness.flow
        flow.findMyCore()
        #expect(await settle { await browser.isBrowsing })
        await browser.announce([station.foundStation])
        #expect(await settle { flow.nearby.first?.offer == .code })
        await flow.pairNearby(try #require(flow.nearby.first))
        #expect(flow.screen == .pairByCode)
        #expect(flow.pairTarget?.endpoint == station.endpoint)
        #expect(flow.pairTarget?.label == station.label)
        #expect(flow.pairTarget?.back == .findCore)
        #expect(station.pairingConnectionCount == 0)
        flow.codeText = station.pairingCode
        await flow.pair()
        #expect(flow.screen == .microphone)
        await flow.answerMicrophone(allow: false)
        #expect(await settle { flow.screen == .band })
        await harness.app.disconnect()
    }

    @Test("each one-tap refusal shows the Core's words and offers the code", arguments: [
        FakeStation.oneTapClaimedReason, FakeStation.oneTapDeniedReason, FakeStation.oneTapOffNetworkReason,
    ])
    func oneTapRefusals(reason: String) async throws {
        let station = try FakeStation(fixture: "session-device-sign-in", requiresPairing: true)
        // The record as found says one tap; the Core has moved on since.
        let announced = station.foundStation
        #expect(announced.pairing == .click && !announced.claimed)
        switch reason {
        case FakeStation.oneTapClaimedReason:
            // Another phone claims it first.
            let first = try Self.harness(factory: station.transportFactory, browser: FakeStationBrowser())
            first.microphone.needsAsking = false
            first.flow.findMyCore()
            await first.flow.pairNearby(ConnectionFlow.NearbyRow(station: announced, endpoint: station.endpoint,
                                                                 offer: .oneTap))
            #expect(await settle { first.flow.screen == .band })
            await first.app.disconnect()
            // Its pairing closed the window; the Core refuses a closed
            // window before a claimed one (StationServer.cpp:6837-6851), so
            // pairing is open again for one more.
            station.openPairing()
        case FakeStation.oneTapDeniedReason:
            station.allowsOneTap = false
        default:
            station.reachedOnItsOwnNetwork = false
        }
        let browser = FakeStationBrowser()
        let harness = try Self.harness(factory: station.transportFactory, browser: browser)
        let flow = harness.flow
        flow.findMyCore()
        #expect(await settle { await browser.isBrowsing })
        await browser.announce([announced])
        #expect(await settle { flow.nearby.count == 1 })
        await flow.pairNearby(try #require(flow.nearby.first))
        #expect(flow.nearbyProblem?.words == reason)
        #expect(flow.nearbyProblem?.offersCode == true)
        #expect(flow.screen == .findCore)
        #expect(try harness.stations.all().isEmpty)
        flow.useCodeAfterRefusal()
        #expect(flow.screen == .pairByCode)
        #expect(flow.pairTarget?.endpoint == station.endpoint)
        #expect(flow.nearbyProblem == nil)
    }

    @Test("a paired Core found unclaimed has forgotten this phone: it offers Pair, and one tap claims it again")
    func pairedCoreFoundUnclaimed() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in", requiresPairing: true)
        let browser = FakeStationBrowser()
        let harness = try Self.harness(factory: station.transportFactory, paired: [Self.paired(station)],
                                       browser: browser)
        harness.microphone.needsAsking = false
        let flow = harness.flow
        #expect(flow.screen == .cores)
        #expect(await settle { await browser.isBrowsing })
        #expect(flow.cores.first?.needsPairing == false)
        await browser.announce([station.foundStation])
        #expect(await settle { flow.cores.first?.found != nil })
        let row = try #require(flow.cores.first)
        #expect(row.needsPairing)
        #expect(flow.nearby.isEmpty)
        // Connect goes to Pair: one tap, which replaces what the phone kept.
        await flow.connect(to: row)
        #expect(await settle { flow.screen == .band })
        #expect(station.pairedDeviceKeys.count == 1)
        #expect(try harness.stations.all().count == 1)
        await harness.app.disconnect()
    }
}
