// NereusSDR for iOS: plays one session fixture against the app's session in the client's role
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkTestSupport
import NereusLink

/// Link document section 16.3, an app's runner: it plays the station, gives
/// the session each station message, drives the client to each behaviour
/// step and matches what it sends, and holds it to each scripted step's
/// answers. The client is the session's link layer unless a
/// `SessionFixtureClient` puts a layer above it (the mirror's runner).
public struct SessionFixturePlayer {
    /// What carries the session: the WebSocket's stand-in, or the app's
    /// real control data-channel transport over a channel the runner plays
    /// (link document section 20), where every message crosses as chunks
    /// and the heartbeat is 5-byte pings and pongs.
    public enum TransportMode: Sendable {
        case webSocket
        case dataChannel
    }

    public let fixture: SessionFixture
    public let mirrorClasses: [String: [[String: Any]]]
    public let client: SessionFixtureClient
    public let transportMode: TransportMode
    /// The isolated producer transcript refers to the client's actual paired
    /// identity. Ordinary v1 fixtures keep their existing placeholder rules.
    public let producerDeviceIdentityReferences: Bool

    /// What the runner's client uses for a `"$string:<name>"` (its origin)
    /// when the runner fills a behaviour message itself.
    public static let origin = "app-conformance"

    public init(fixture: SessionFixture, mirrorClasses: [String: [[String: Any]]],
                client: SessionFixtureClient = .linkLayer, transportMode: TransportMode = .webSocket,
                producerDeviceIdentityReferences: Bool = false) {
        self.fixture = fixture
        self.mirrorClasses = mirrorClasses
        self.client = client
        self.transportMode = transportMode
        self.producerDeviceIdentityReferences = producerDeviceIdentityReferences
    }

    /// Plays the fixture; returns what failed, each naming its step.
    public func play() async throws -> [String] {
        let station = ScriptedStation()
        let channelStation = ScriptedDataChannelStation(certificateSHA256: station.certificateSHA256)
        let identity = TestStationIdentity()
        var placeholders = FixturePlaceholders(mirrorClasses: mirrorClasses, stationIdentity: identity,
                                               certificateSHA256: station.certificateSHA256)
        // The station's token, chosen here at run time (section 16.3).
        let token = Self.randomToken()
        placeholders.record["token"] = .string(token)

        // A fixture whose sign-in carries a device block signs in with the
        // app's device key, trusting the station's identity; the rest sign
        // in by token, trusting its certificate.
        let trust: StationTrust
        let authenticator: any StationAuthenticator
        if fixture.signsInWithDevice {
            trust = identity.trust
            let device = try DeviceIdentity.load(store: InMemoryKeyStore())
            authenticator = try DeviceKeyAuthenticator(identity: device, name: Self.deviceName, kind: .phone)
            if producerDeviceIdentityReferences {
                // The producer exporter normalises this key fingerprint as
                // $ref:device:self (tst_link_conformance_session.cpp:1600).
                // Use the same real test key the session signs with; the
                // existing $device:signed cryptographic check is unchanged.
                guard ["session-diversity-control-pattern", "session-diversity-control-no-pattern"].contains(fixture.id),
                      fixture.declaredFeatures?["diversityControl"] == 1 else {
                    throw LinkFixtureLoader.Malformed(description: "producer identity references require a Diversity producer transcript")
                }
                placeholders.record["device:self"] = .string(device.id)
            }
        } else {
            guard !producerDeviceIdentityReferences else {
                throw LinkFixtureLoader.Malformed(description: "producer identity references require device sign-in")
            }
            trust = station.trust
            authenticator = TokenAuthenticator(token: token)
        }

        let clock = ManualLinkClock()
        let features = fixture.declaredFeatures ?? LinkFeatures.app
        let session: StationSession
        switch transportMode {
        case .webSocket:
            session = StationSession(endpoint: StationEndpoint(host: "conformance.invalid"), trust: trust,
                                     authenticator: authenticator, clock: clock, transportFactory: station.factory,
                                     features: features)
        case .dataChannel:
            session = StationSession(trust: trust, authenticator: authenticator, clock: clock,
                                     transport: channelStation.factory, features: features)
        }
        let recorder = EventRecorder(session, forward: client.forward)
        await session.connect()
        let dialled: (any ScriptedFarEnd)?
        let dialCount: @Sendable () -> Int
        switch transportMode {
        case .webSocket:
            dialled = station.latest
            dialCount = { station.dialCount }
        case .dataChannel:
            dialled = channelStation.latest
            dialCount = { channelStation.dialCount }
        }
        guard let transport = dialled else {
            return ["the session did not dial"]
        }

        var failures: [String] = []
        var counter = 1
        // The station messages the session has passed on as events so far.
        var passedOn = 0
        // The code of the last auth.result or session.end the station sent.
        var lastEndCode: String?

        for (index, step) in fixture.steps.enumerated() {
            let label = "\(fixture.id) step \(index)"
            let fail = { (text: String) in failures.append("step \(index): \(text)") }
            let allowed: Set<String> = ["from", "role", "message", "advanceMs", "expectClosed", "client", "to",
                                        "connect", "close"]
            if let key = Set(step.keys).subtracting(allowed).sorted().first {
                throw LinkFixtureLoader.Malformed(description: "\(label): unknown field \"\(key)\"")
            }
            // Several clients (section 16.1): the runner plays only its own
            // client, so another client's steps, the Core's messages to it
            // and its connect and close name nothing it plays.
            if Self.isAnotherClients(step) {
                continue
            }

            if let advance = step["advanceMs"] as? Int {
                await clock.advance(by: Int64(advance)) { await transport.answerPings() }
                if let extra = transport.pending.first {
                    fail("the client sent \(extra) while time passed")
                }
                continue
            }

            if let closed = step["expectClosed"] as? [String: Any] {
                guard let retryable = closed["retryable"] as? Bool else {
                    throw LinkFixtureLoader.Malformed(description: "\(label): expectClosed needs retryable")
                }
                if let extra = transport.pending.first {
                    fail("the client sent \(extra) before the close")
                }
                await transport.dropLink()
                let state = await session.state
                if retryable {
                    if case .waitingToRetry = state {} else {
                        fail("after a retryable close the session is \(state), not waiting to retry")
                    }
                } else {
                    if state != .stopped {
                        fail("after a final close the session is \(state), not stopped")
                    }
                    await clock.advance(by: 600_000)
                    if dialCount() != 1 {
                        fail("the session dialled again after a final close")
                    }
                }
                if !(await Self.clientHasHandled(recorder, messages: passedOn, state: state)) {
                    fail("the client had not handled the session's events for the close in time")
                }
                if case .ended(_, let endRetryable)? = recorder.refusals.last?.reason, endRetryable != retryable {
                    fail("the session reported the end as retryable \(endRetryable)")
                }
                let reportedCode = recorder.refusals.last?.code?.wireName
                if reportedCode != lastEndCode {
                    fail("the session reported the end's code as \(reportedCode ?? "none"), not \(lastEndCode ?? "none")")
                }
                if let extra = transport.pending.first {
                    fail("the client sent \(extra) after the close")
                }
                for failure in await client.closed(label) {
                    fail(failure)
                }
                continue
            }

            guard let from = step["from"] as? String, let raw = step["message"] else {
                throw LinkFixtureLoader.Malformed(description: "\(label): a step needs from and message")
            }
            let message = try LinkJSON(foundation: raw)

            if from == "station" {
                if let extra = transport.pending.first {
                    fail("the client sent \(extra), which no step expects")
                    break
                }
                let filled = try placeholders.fillStation(message, at: label)
                if case .object(let object) = filled, case .string(let type)? = object["type"],
                   type == "auth.result" || type == "session.end" {
                    if case .string(let code)? = object["code"] {
                        lastEndCode = code
                    } else {
                        lastEndCode = nil
                    }
                }
                // The session has handled the message once this returns.
                await transport.deliver(filled.compactText)
                let given = try? LinkCodec.decode(filled.compactText)
                if let given, given.kind.sentByStation {
                    passedOn += 1
                }
                let state = await session.state
                if !(await Self.clientHasHandled(recorder, messages: passedOn, state: state)) {
                    fail("the client had not handled the session's events for this message in time")
                }
                if case .object(let object) = filled, object["type"] == .string("snapshot.complete"), state != .ready {
                    fail("after snapshot.complete the session is \(state), not ready")
                }
                for failure in await client.stationMessageHandled(given, label) {
                    fail(failure)
                }
                continue
            }

            guard from == "client", let role = step["role"] as? String else {
                throw LinkFixtureLoader.Malformed(description: "\(label): a client step needs a role")
            }
            if role == "scripted" {
                // Nothing to send; the answers that follow come as if the
                // client had sent it.
                let identities = try placeholders.fillDiversityScriptedIdentity(message, at: label)
                if FixturePlaceholders.holdsPlaceholder(identities) {
                    throw LinkFixtureLoader.Malformed(description: "\(label): a scripted message holds a placeholder")
                }
                continue
            }
            guard role == "behaviour", case .object(let object) = message,
                  case .string(let type)? = object["type"] else {
                throw LinkFixtureLoader.Malformed(description: "\(label): unknown role or message")
            }
            if type != "hello" && type != "auth.request" {
                // Drive the client: tell it to send this.
                let filled = try placeholders.fillClient(message, counter: &counter, origin: Self.origin, at: label)
                if counter > 1000 {
                    throw LinkFixtureLoader.Malformed(description: "\(label): the client's ids reached 1000")
                }
                do {
                    try await client.drive(session, try LinkCodec.decode(filled.compactText), label)
                } catch {
                    fail("the client would not send \(filled.compactText): \(error)")
                    break
                }
                // A layer above the session may send from a task of its own,
                // on the main actor or elsewhere.
                _ = await transport.waitForSent(within: .seconds(30))
            }
            guard let sent = transport.takeSent() else {
                fail("the client sent nothing; expected \(message.compactText)")
                break
            }
            let sentJSON = try LinkJSON.parse(sent)
            if let difference = try placeholders.matchClient(message, sentJSON, at: label) {
                fail(difference)
                break
            }
            if let difference = Self.idAtOrAbove1000(sentJSON) {
                fail(difference)
                break
            }
        }

        if failures.isEmpty, let extra = transport.pending.first {
            failures.append("after the last step: the client sent \(extra)")
        }
        for failure in await client.finished() {
            failures.append("after the last step: \(failure)")
        }
        await session.disconnect()
        return failures
    }

    /// The barrier before each check: waits until the client has handled
    /// every event the session produced for what was just delivered. The
    /// session produces them all before `deliver` returns, in this order:
    /// the message it passes on, a refusal if the message ended the
    /// session, and a state change if its state moved. So once the client
    /// has handled `messages` message events and the state the session is
    /// now in, it has handled them all.
    static func clientHasHandled(_ recorder: EventRecorder, messages: Int,
                                 state: StationSession.State) async -> Bool {
        await recorder.handled { events in
            var count = 0
            var last = StationSession.State.idle
            for event in events {
                switch event {
                case .message:
                    count += 1
                case .stateChanged(let next):
                    last = next
                case .refused:
                    break
                }
            }
            return count >= messages && last == state
        }
    }

    /// A step for another client the fixture plays beside its own: one
    /// that names a client or a station message's recipient, a `connect`
    /// or `close`, or an `expectClosed` that names a client.
    public static func isAnotherClients(_ step: [String: Any]) -> Bool {
        if step["client"] != nil || step["to"] != nil || step["connect"] != nil || step["close"] != nil {
            return true
        }
        if let closed = step["expectClosed"] as? [String: Any], closed["client"] != nil {
            return true
        }
        return false
    }

    /// The client keeps its own ids below the scripted ones, which start
    /// at 1000 (section 16.3).
    static func idAtOrAbove1000(_ sent: LinkJSON) -> String? {
        guard case .object(let object) = sent else {
            return nil
        }
        for key in ["id", "writeId"] {
            if case .number(let id)? = object[key], id >= 1000 {
                return "the client used \(key) \(Int64(id)), which a scripted message may use"
            }
        }
        return nil
    }

    /// 32 random bytes as base64url without padding, as a Core's token is.
    public static func randomToken() -> String {
        Base64URL.encode(Data((0..<32).map { _ in UInt8.random(in: 0...255) }))
    }

    /// The name the runner's device signs in with, as if confirmed at pairing.
    public static let deviceName = "Conformance iPhone"
}

/// The client a fixture runs against, above the session's link layer.
public struct SessionFixtureClient: Sendable {
    /// Sees each session event before the runner records it.
    public var forward: (@Sendable (StationSession.Event) async -> Void)?
    /// Makes the client send one behaviour message (not `hello` or
    /// `auth.request`, which the session sends itself). It returns once the
    /// client has started sending; the runner then waits for the frame.
    public var drive: @Sendable (StationSession, LinkMessage, String) async throws -> Void
    /// Runs after each station message has reached the client; returns failures.
    public var stationMessageHandled: @Sendable (LinkMessage?, String) async -> [String]
    /// Runs after an `expectClosed` step; returns failures.
    public var closed: @Sendable (String) async -> [String]
    /// Runs after the last step, with the connection still up; returns failures.
    public var finished: @Sendable () async -> [String]

    public init(forward: (@Sendable (StationSession.Event) async -> Void)? = nil,
                drive: @escaping @Sendable (StationSession, LinkMessage, String) async throws -> Void,
                stationMessageHandled: @escaping @Sendable (LinkMessage?, String) async -> [String] = { _, _ in [] },
                closed: @escaping @Sendable (String) async -> [String] = { _ in [] },
                finished: @escaping @Sendable () async -> [String] = { [] }) {
        self.forward = forward
        self.drive = drive
        self.stationMessageHandled = stationMessageHandled
        self.closed = closed
        self.finished = finished
    }

    /// The session's link layer, told to send each behaviour message as is.
    public static let linkLayer = SessionFixtureClient(drive: { session, message, _ in
        try await session.send(message)
    })
}

/// One session fixture of the suite.
public struct SessionFixture: @unchecked Sendable {
    public let id: String
    public let runs: [String]
    public let steps: [[String: Any]]

    public init(id: String, runs: [String], steps: [[String: Any]]) {
        self.id = id
        self.runs = runs
        self.steps = steps
    }

    /// True when the fixture's client signs in with a device block.
    public var signsInWithDevice: Bool {
        steps.contains { step in
            guard !SessionFixturePlayer.isAnotherClients(step), step["from"] as? String == "client",
                  let message = step["message"] as? [String: Any] else {
                return false
            }
            return message["type"] as? String == "auth.request" && message["device"] != nil
        }
    }

    /// The hello features the fixture's own client declares by name
    /// (section 16.3, "Several clients": a fixture where the own client
    /// shares the Core names them, and the app's client declares them), or
    /// nil where its `features` is `"$object"`. The runner's session
    /// declares these, so a fixture written before a feature the app now
    /// declares still sees the hello it was written for.
    public var declaredFeatures: [String: Int]? {
        for step in steps where !SessionFixturePlayer.isAnotherClients(step) {
            guard step["from"] as? String == "client", let message = step["message"] as? [String: Any],
                  message["type"] as? String == "hello", let features = message["features"] as? [String: Any] else {
                continue
            }
            var declared: [String: Int] = [:]
            for (name, value) in features {
                if let version = value as? Int {
                    declared[name] = version
                }
            }
            return declared
        }
        return nil
    }

    /// The hello features the fixture's client declares by name that the
    /// app does not declare yet (`LinkFeatures.app`), or at a higher version:
    /// a fixture for a device that answers what the app cannot yet, such as
    /// `sessionHolder` (Task 56c declares it). A client hello whose
    /// `features` is `"$object"` declares nothing by name.
    public var featuresTheAppLacks: [String] {
        var lacking: Set<String> = []
        for step in steps where !SessionFixturePlayer.isAnotherClients(step) {
            guard step["from"] as? String == "client", let message = step["message"] as? [String: Any],
                  message["type"] as? String == "hello", let features = message["features"] as? [String: Any] else {
                continue
            }
            for (name, value) in features {
                guard let version = value as? Int, let ours = LinkFeatures.app[name], ours >= version else {
                    lacking.insert(name)
                    continue
                }
            }
        }
        return lacking.sorted()
    }
}

/// The suite's session fixtures and the surface parts their runners read.
public enum SessionFixtures {
    public static func all() throws -> [SessionFixture] {
        try LinkFixtureLoader.manifest().filter { $0.kind == "session" }.map { fixture in
            let object = try LinkFixtureLoader.jsonObject(at: LinkFixtureLoader.root.appendingPathComponent(fixture.file))
            let unknown = Set(object.keys).subtracting(["runs", "stationSetup", "steps"])
            if let key = unknown.sorted().first {
                throw LinkFixtureLoader.Malformed(description: "\(fixture.id): unknown field \"\(key)\"")
            }
            guard let runs = object["runs"] as? [String], let steps = object["steps"] as? [[String: Any]] else {
                throw LinkFixtureLoader.Malformed(description: "\(fixture.id): runs or steps is missing")
            }
            return SessionFixture(id: fixture.id, runs: runs, steps: steps)
        }
    }

    /// The fixtures whose `runs` names "app" and whose client declares no
    /// hello feature the app lacks (``SessionFixture/featuresTheAppLacks``).
    public static func forApp() throws -> [SessionFixture] {
        try all().filter { $0.runs.contains("app") && $0.featuresTheAppLacks.isEmpty }
    }

    /// The fixtures whose `runs` names "app" that wait for a hello feature
    /// the app does not declare.
    public static func waitingForAFeature() throws -> [SessionFixture] {
        try all().filter { $0.runs.contains("app") && !$0.featuresTheAppLacks.isEmpty }
    }

    /// The Core's words for a refused command whose `refusalCode` is
    /// `code`, as the first session fixture that refuses with it sends
    /// them; nil when no fixture does. Tests that show the Core's words
    /// take them from here, so a reworded reason moves them with it.
    public static func refusalReason(code: String) throws -> String? {
        for fixture in try all() {
            for step in fixture.steps {
                guard let message = step["message"] as? [String: Any],
                      message["type"] as? String == "command.result",
                      let values = message["values"] as? [[String: Any]],
                      values.contains(where: { $0["name"] as? String == "refusalCode" && $0["value"] as? String == code }),
                      let reason = message["reason"] as? String, !reason.isEmpty else {
                    continue
                }
                return reason
            }
        }
        return nil
    }

    /// `surface.json`'s `mirrorClasses`: each class's properties.
    public static func mirrorClasses() throws -> [String: [[String: Any]]] {
        let surface = try LinkFixtureLoader.jsonObject(at: LinkFixtureLoader.root.appendingPathComponent("surface.json"))
        guard let classes = surface["mirrorClasses"] as? [String: [String: Any]] else {
            throw LinkFixtureLoader.Malformed(description: "surface.json: mirrorClasses is missing")
        }
        return try classes.mapValues { entry in
            guard let properties = entry["properties"] as? [[String: Any]] else {
                throw LinkFixtureLoader.Malformed(description: "surface.json: a class has no properties")
            }
            return properties
        }
    }

    /// The suite's majors, each of which the app must support.
    public static func linkMajors() throws -> [UInt16] {
        let manifest = try LinkFixtureLoader.jsonObject(at: LinkFixtureLoader.root.appendingPathComponent("manifest.json"))
        guard let majors = manifest["linkMajors"] as? [Int], !majors.isEmpty else {
            throw LinkFixtureLoader.Malformed(description: "manifest.json: linkMajors is missing")
        }
        return majors.map { UInt16($0) }
    }
}
