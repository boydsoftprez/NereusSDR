// NereusSDR for iOS: the control session over a data channel: chunks, the cap, the heartbeat and the DTLS certificate
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkSessionTestSupport
import Testing
@testable import NereusLink

/// iPhone app plan Task 28a (R-IOS-16), link document section 20: a session
/// over ``DataChannelSessionTransport`` behaves as over the WebSocket, with
/// the chunking, the cap, the 5-byte heartbeat and the certificate the Core
/// presented in DTLS.
@Suite struct DataChannelSessionTransportTests {
    private final class ObservedChannel: ControlChannel, @unchecked Sendable {
        private let lock = NSLock()
        private var closed = false
        var selectedRouteObservation: SelectedRouteObservation {
            lock.withLock { closed ? .unavailable(.retired) : .fromSelectedICEPair(
                local: "candidate:a 1 UDP 1 192.0.2.1 5000 typ host",
                remote: "candidate:b 1 UDP 1 198.51.100.2 5001 typ host") }
        }
        func send(_ frame: Data) -> Bool { true }
        func close() { lock.withLock { closed = true } }
    }

    @Test func selectedObservationForwardsOnlyOpenCurrentChannel() async throws {
        let channel = ObservedChannel()
        let transport = DataChannelSessionTransport { _ in
            ControlChannelConnection(channel: channel, certificateSHA256: Data(repeating: 7, count: 32))
        }
        #expect(transport.selectedRouteObservation == .unavailable(.notReady))
        #expect(!transport.send("before open"))
        _ = try await transport.open { _ in }
        guard case .available(let route) = transport.selectedRouteObservation else {
            Issue.record("open channel observation unavailable")
            return
        }
        #expect(route.coreEndpoint?.address == "198.51.100.2")
        transport.close()
        #expect(transport.selectedRouteObservation == .unavailable(.retired))
    }

    // MARK: The transport alone

    /// A transport over a channel the test feeds, opened.
    private struct Opened {
        let far: FramingFarEnd
        let transport: DataChannelSessionTransport
        let events: EventLog
    }

    private func open(cap: Int = DataChannelSessionTransport.maxInboundMessageBytes,
                      beforeOpening: [ControlChannelEvent] = []) async throws -> Opened {
        let far = FramingFarEnd()
        let transport = DataChannelSessionTransport(maxInboundMessageBytes: cap) { onEvent in
            far.attach(onEvent)
            // What the Core sends as its end opens, before the dial returns.
            for event in beforeOpening {
                far.feed(event)
            }
            return ControlChannelConnection(channel: far, certificateSHA256: Data(repeating: 3, count: 32))
        }
        let events = EventLog()
        let digest = try await transport.open { event in
            events.append(event)
        }
        #expect(digest == Data(repeating: 3, count: 32))
        return Opened(far: far, transport: transport, events: events)
    }

    @Test func trafficCountsCompleteUtf8MessagesOnceAndExcludesHeartbeat() async throws {
        let opened = try await open()
        let first = try #require(opened.transport.trafficObservation)
        #expect(first.receivedPayloadBytes == 0 && first.acceptedPayloadBytes == 0)
        let message = String(repeating: "é", count: 40_000)
        let chunks = ControlChannelFraming.chunks(of: Data(message.utf8))
        #expect(chunks.count > 1)
        opened.far.feed(.binary(chunks[0]))
        #expect(opened.transport.trafficObservation?.receivedPayloadBytes == 0)
        opened.far.feed(.binary(ControlChannelFraming.ping(id: 9)))
        for chunk in chunks.dropFirst() { opened.far.feed(.binary(chunk)) }
        #expect(await opened.events.waitFor(count: 1))
        #expect(opened.transport.trafficObservation?.receivedPayloadBytes == UInt64(message.utf8.count))
        #expect(opened.transport.send("日本語"))
        #expect(opened.transport.trafficObservation?.acceptedPayloadBytes == UInt64("日本語".utf8.count))
        opened.transport.ping()
        #expect(opened.transport.trafficObservation?.acceptedPayloadBytes == UInt64("日本語".utf8.count))
        opened.transport.close()
        #expect(!opened.transport.send("closed"))
        #expect(opened.transport.trafficObservation?.lifetime == first.lifetime)
        #expect(opened.transport.trafficObservation?.active == false)
    }

    @Test func partialChunkRefusalDoesNotCountACompleteOutgoingPayload() async throws {
        final class RefusingChannel: ControlChannel, @unchecked Sendable {
            private let lock = NSLock()
            private var sends = 0
            func send(_ frame: Data) -> Bool {
                lock.withLock {
                    sends += 1
                    return sends == 1
                }
            }
            func close() {}
        }
        let channel = RefusingChannel()
        let transport = DataChannelSessionTransport { _ in
            ControlChannelConnection(channel: channel, certificateSHA256: Data(repeating: 4, count: 32))
        }
        let events = EventLog()
        _ = try await transport.open { events.append($0) }
        #expect(!transport.send(String(repeating: "a", count: ControlChannelFraming.chunkPayloadBytes + 1)))
        #expect(await events.waitFor(count: 1))
        #expect(events.events == [.closed])
        #expect(transport.trafficObservation?.acceptedPayloadBytes == 0)
        #expect(transport.trafficObservation?.active == false)
    }

    /// A settings snapshot over 300 KiB goes out as chunks of the right
    /// sizes and comes back together byte for byte, both ways.
    @Test func aThreeHundredKilobyteSnapshotCrossesInChunksAndJoinsExactly() async throws {
        let settings = (0..<3_000).map { "\"Setting\(String(format: "%04d", $0))\":\"\(String(repeating: "v", count: 90))\"" }
        let snapshot = "{\"type\":\"settings.snapshot\",\"settings\":{" + settings.joined(separator: ",") + "}}"
        let bytes = Data(snapshot.utf8)
        #expect(bytes.count > 300 * 1024)

        let opened = try await open()
        opened.transport.send(snapshot)
        let frames = opened.far.sent
        let expectedChunks = (bytes.count + ControlChannelFraming.chunkPayloadBytes - 1) / ControlChannelFraming.chunkPayloadBytes
        #expect(frames.count == expectedChunks)
        #expect(frames.dropLast().allSatisfy { $0.count == ControlChannelFraming.chunkBytes && $0.first == 0x01 })
        #expect(frames.last?.first == 0x02)
        var joined = ControlChannelReassembler(maxMessageBytes: StationSession.maxOutboundMessageBytes)
        var received: Data?
        for frame in frames {
            if case .message(let message) = joined.feed(frame) {
                received = message
            }
        }
        #expect(received == bytes)

        // The Core's snapshot to the app, the same way round.
        for frame in ControlChannelFraming.chunks(of: bytes) {
            opened.far.feed(.binary(frame))
        }
        #expect(await opened.events.waitFor(count: 1))
        #expect(opened.events.events == [.text(snapshot)])
        opened.transport.close()
    }

    /// A message past the app's 8 MiB cap ends the connection the moment
    /// the joined bytes pass it, before its last chunk, as the WebSocket's
    /// cap does.
    @Test func aMessageOverTheCapEndsTheConnectionBeforeItsLastChunk() async throws {
        let opened = try await open()
        let cap = DataChannelSessionTransport.maxInboundMessageBytes
        #expect(cap == 8 * 1024 * 1024)
        let over = Data(repeating: 0x61, count: cap + 2 * ControlChannelFraming.chunkPayloadBytes)
        let chunks = ControlChannelFraming.chunks(of: over)
        for chunk in chunks.dropLast() {
            opened.far.feed(.binary(chunk))
        }
        #expect(await opened.events.waitFor(count: 1))
        #expect(opened.events.events == [.closed])
        #expect(opened.far.closed)
        // Nothing more is passed on, not even the last chunk.
        opened.far.feed(.binary(chunks[chunks.count - 1]))
        #expect(opened.events.events == [.closed])
    }

    /// A text data-channel message, a message that is not UTF-8, an empty
    /// one and a short ping each end the connection.
    @Test(arguments: [
        ControlChannelEvent.text,
        .binary(Data([0x02, 0xFF, 0xFE])),
        .binary(Data()),
        .binary(Data([0x10, 0, 0, 1])),
        .binary(Data([0x03, 0x7B, 0x7D])),
    ])
    func whatTheChannelMayNotCarryEndsTheConnection(_ event: ControlChannelEvent) async throws {
        let opened = try await open()
        opened.far.feed(event)
        #expect(await opened.events.waitFor(count: 1))
        #expect(opened.events.events == [.closed])
        #expect(opened.far.closed)
    }

    /// The Core's ping is answered at once with its own id, even between
    /// two chunks, and leaves the message being joined as it is; a pong
    /// the app never asked for is not taken as the link being alive.
    @Test func aPingIsAnsweredBetweenChunksAndOnlyOurOwnPongsCount() async throws {
        let opened = try await open()
        let message = "{\"type\":\"notice\",\"text\":\"" + String(repeating: "c", count: 70_000) + "\"}"
        let chunks = ControlChannelFraming.chunks(of: Data(message.utf8))
        #expect(chunks.count == 2)
        opened.far.feed(.binary(chunks[0]))
        opened.far.feed(.binary(ControlChannelFraming.ping(id: 7)))
        opened.far.feed(.binary(ControlChannelFraming.pong(id: 3)))
        opened.far.feed(.binary(chunks[1]))
        #expect(await opened.events.waitFor(count: 1))
        #expect(opened.far.sent == [ControlChannelFraming.pong(id: 7)])
        #expect(opened.events.events == [.text(message)])

        // The app's own pings: 1, then 2; a pong for 2 answers both.
        opened.transport.ping()
        opened.transport.ping()
        #expect(opened.far.sent.suffix(2) == [ControlChannelFraming.ping(id: 1), ControlChannelFraming.ping(id: 2)])
        opened.far.feed(.binary(ControlChannelFraming.pong(id: 2)))
        #expect(await opened.events.waitFor(count: 2))
        #expect(opened.events.events.last == .pong)
        // A pong for 1, already answered, counts for nothing.
        opened.far.feed(.binary(ControlChannelFraming.pong(id: 1)))
        opened.far.feed(.binary(ControlChannelFraming.chunks(of: Data("{}".utf8))[0]))
        #expect(await opened.events.waitFor(count: 3))
        #expect(opened.events.events == [.text(message), .pong, .text("{}")])
        opened.transport.close()
    }

    /// What the Core sends as its end opens, before the dial has returned,
    /// is passed on in order, and a ping among it is answered once the
    /// channel is handed over.
    @Test func whatArrivesBeforeTheDialReturnsIsHeldInOrder() async throws {
        let first = ControlChannelFraming.chunks(of: Data("{\"n\":1}".utf8))
        let second = ControlChannelFraming.chunks(of: Data("{\"n\":2}".utf8))
        let opened = try await open(beforeOpening: [.binary(first[0]), .binary(ControlChannelFraming.ping(id: 9)),
                                                    .binary(second[0])])
        #expect(await opened.events.waitFor(count: 2))
        #expect(opened.events.events == [.text("{\"n\":1}"), .text("{\"n\":2}")])
        #expect(opened.far.sent == [ControlChannelFraming.pong(id: 9)])
        opened.transport.close()
    }

    /// Closing a transport that is still dialling calls the dial off.
    @Test func closingWhileDiallingCallsTheDialOff() async throws {
        let dialling = EventLog()
        let cancelled = EventLog()
        let transport = DataChannelSessionTransport { _ in
            dialling.append(.pong)
            do {
                try await Task.sleep(for: .seconds(60))
            } catch {
                cancelled.append(.closed)
                throw error
            }
            throw LinkTransportError.failed("never")
        }
        let opening = Task {
            try await transport.open { _ in }
        }
        #expect(await dialling.waitFor(count: 1))
        transport.close()
        #expect(await cancelled.waitFor(count: 1))
        await #expect(throws: (any Error).self) { try await opening.value }
    }

    // MARK: A session over it

    private struct Rig {
        let station = ScriptedDataChannelStation()
        let clock = ManualLinkClock()
        let core = TestStationIdentity()
        let session: StationSession
        let recorder: EventRecorder

        init() throws {
            let device = try DeviceIdentity.load(store: InMemoryKeyStore())
            session = StationSession(trust: core.trust,
                                     authenticator: try DeviceKeyAuthenticator(identity: device, name: "Shack iPhone",
                                                                                kind: .phone),
                                     clock: clock, transport: station.factory)
            recorder = EventRecorder(session)
        }

        var channel: ScriptedDataChannel {
            get throws {
                try #require(station.latest)
            }
        }

        func hello(binding certificate: Data) throws -> LinkMessage {
            .hello(LinkMessage.Hello(major: 1, minor: 11, settingsSchema: 0, peer: "nereusd", majors: [1],
                                     features: ["deviceAuth": 1, "pairing": 1],
                                     identity: try core.claim(certificateSHA256: certificate),
                                     challenge: TestStationIdentity.newChallenge()))
        }
    }

    private func refused(_ rig: Rig) async -> [Refusal] {
        await rig.recorder.settle { events in
            events.contains { if case .refused = $0 { return true } else { return false } }
        }
        return rig.recorder.refusals
    }

    /// The Core's hello must bind the certificate it presented in DTLS:
    /// bound to another, the session ends as identityChanged before the app
    /// sends anything at all, and does not dial again.
    @Test func aControlConnectionWhoseDtlsCertificateIsNotTheBoundOneSendsNothing() async throws {
        let rig = try Rig()
        await rig.session.connect()
        let channel = try rig.channel
        await channel.deliver(LinkCodec.encode(try rig.hello(binding: Data(repeating: 9, count: 32))))
        #expect(channel.sentFrames.isEmpty)
        #expect(channel.isClosedByApp)
        #expect(await refused(rig) == [Refusal(.authentication(StationSession.certificateMismatchText),
                                               code: .identityChanged)])
        #expect(await rig.session.state == .stopped)
        await rig.clock.advance(by: 600_000)
        #expect(rig.station.dialCount == 1)
    }

    /// Bound to the DTLS certificate, the app answers and signs in with its
    /// device key over the certificate the Core presented, every message as
    /// chunks.
    @Test func theBoundCertificateSignsInWithTheDeviceKey() async throws {
        let rig = try Rig()
        await rig.session.connect()
        let channel = try rig.channel
        let hello = try rig.hello(binding: rig.station.certificateSHA256)
        await channel.deliver(LinkCodec.encode(hello))
        let sent = channel.pending
        try #require(sent.count == 2)
        guard case .hello = try LinkCodec.decode(sent[0]),
              case .authRequest(let request) = try LinkCodec.decode(sent[1]),
              case .hello(let theirs) = hello, let challenge = theirs.challenge.flatMap(Base64URL.decode),
              case .object(let fields) = LinkCodec.json(.authRequest(request)), let block = fields["device"] else {
            Issue.record("expected hello then a device sign-in: \(sent)")
            return
        }
        #expect(request.token == "")
        #expect(DeviceBlockCheck.failure(block, challenge: challenge, certificateSHA256: rig.station.certificateSHA256,
                                         stationKey: rig.core.publicKey) == nil)
        #expect(channel.sentFrames.allSatisfy { $0.first == ControlChannelFraming.last })
        #expect(await rig.session.state == .authenticating)
    }

    /// Readied over the data channel, the heartbeat keeps the session's
    /// rule: answered pings keep it up; at a tick with two pings unanswered
    /// the link is lost, and a pong for a ping the app never sent does not
    /// keep it.
    @Test func theHeartbeatDeclaresTheLinkDeadAfterTwoMissedPongs() async throws {
        let rig = try Rig()
        await rig.session.connect()
        let channel = try rig.channel
        await channel.deliver(LinkCodec.encode(try rig.hello(binding: rig.station.certificateSHA256)))
        _ = channel.takeSent()
        _ = channel.takeSent()
        await channel.deliver(LinkCodec.encode(.authResult(LinkMessage.AuthResult(accepted: true, reason: "",
                                                                                 retryable: false))))
        await channel.deliver(LinkCodec.encode(.snapshotComplete))
        #expect(await rig.session.state == .ready)

        await rig.clock.advance(by: 60_000) { await channel.answerPings() }
        #expect(await rig.session.state == .ready)
        #expect(channel.pingCount == 3)

        await rig.clock.advance(by: 40_000)
        #expect(channel.pingCount == 5)
        #expect(await rig.session.state == .ready)
        // A pong the app never asked for.
        await channel.deliverFrames([ControlChannelFraming.pong(id: 999)], events: 0)
        await rig.clock.advance(by: 20_000)
        #expect(await rig.session.state == .waitingToRetry(seconds: 1))
        #expect(channel.isClosedByApp)
    }

    /// The dial through the service has its own bound, so the session's
    /// 30 s connect deadline counts from the channel opening, as the
    /// desktop arms it when its dialer hands the channel over: a dial
    /// slower than 30 s is not cut off, and once open the Core has 30 s to
    /// finish the connect sequence.
    @Test func theConnectDeadlineCountsFromTheChannelOpening() async throws {
        let gate = Gate()
        let far = FramingFarEnd()
        let clock = ManualLinkClock()
        let core = TestStationIdentity()
        let session = StationSession(trust: core.trust,
                                     authenticator: try DeviceKeyAuthenticator(
                                         identity: try DeviceIdentity.load(store: InMemoryKeyStore()),
                                         name: "Shack iPhone", kind: .phone),
                                     clock: clock,
                                     transport: DataChannelSessionTransport.factory { onEvent in
                                         far.attach(onEvent)
                                         await gate.wait()
                                         return ControlChannelConnection(channel: far,
                                                                         certificateSHA256: Data(repeating: 5, count: 32))
                                     })
        #expect(DataChannelSessionTransport { _ in throw LinkTransportError.failed("unused") }.boundsItsOwnOpening)
        let connecting = Task { await session.connect() }
        for _ in 0..<10_000 where await session.state != .connecting {
            await Task.yield()
        }
        #expect(await session.state == .connecting)
        await clock.advance(by: 45_000)
        #expect(await session.state == .connecting)
        await gate.open()
        await connecting.value
        #expect(await session.state == .connecting)
        await clock.advance(by: 29_999)
        #expect(await session.state == .connecting)
        await clock.advance(by: 1)
        #expect(await session.state == .waitingToRetry(seconds: 1))
        #expect(far.closed)
    }

    /// Opens once, for a dial held until the test lets it go.
    actor Gate {
        private var isOpen = false
        private var waiting: [CheckedContinuation<Void, Never>] = []

        func wait() async {
            guard !isOpen else {
                return
            }
            await withCheckedContinuation { waiting.append($0) }
        }

        func open() {
            isOpen = true
            waiting.forEach { $0.resume() }
            waiting = []
        }
    }

    /// The Core's session.end arrives over the channel, is shown in its own
    /// words, and ends the session as on the WebSocket.
    @Test func theCoresEndArrivesAsOnTheWebSocket() async throws {
        let rig = try Rig()
        await rig.session.connect()
        let channel = try rig.channel
        await channel.deliver(LinkCodec.encode(try rig.hello(binding: rig.station.certificateSHA256)))
        await channel.deliver(LinkCodec.encode(.sessionEnd(LinkMessage.SessionEnd(
            reason: "This device was removed from the Core.", retryable: false, code: "deviceRemoved"))))
        #expect(await refused(rig) == [Refusal(.ended("This device was removed from the Core.", retryable: false),
                                               code: .deviceRemoved)])
        #expect(await rig.session.state == .stopped)
    }
}
