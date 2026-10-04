// NereusSDR for iOS: the control data channel's framing fixtures, played against the app's chunking and its transport
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkSessionTestSupport
import LinkTestSupport
import Testing
@testable import NereusLink

/// Link document section 16.1's `framing/` fixtures (section 20's chunks
/// and heartbeat bytes), the manifest's fourth kind: an app's runner cuts
/// each message its sender would and feeds every fixture, frame by frame,
/// to a receiver with the inbound cap of the end the fixture names. Both
/// ends' fixtures run here, since the app's receiver takes a cap.
@Suite struct LinkConformanceFramingTests {
    /// One framing fixture, read by section 16.1's rules.
    struct Framing {
        let id: String
        let receiver: String
        let message: Data
        let frames: [Data]
        /// Whether each frame is a chunk (the only frames an encoding
        /// fixture holds).
        let allChunks: Bool
        let encodes: Bool
        let delivered: Bool
        let replies: [Data]

        /// The cap of the end the fixture names (section 12.3).
        var cap: Int {
            receiver == "station" ? StationSession.maxOutboundMessageBytes : DataChannelSessionTransport.maxInboundMessageBytes
        }
    }

    static let framingKeys: Set<String> = ["receiver", "message", "frames", "encodes", "outcome", "replies"]

    static func fixtures() throws -> [Framing] {
        try LinkFixtureLoader.manifest().filter { $0.kind == "framing" }.map { entry in
            let object = try LinkFixtureLoader.jsonObject(at: LinkFixtureLoader.root.appendingPathComponent(entry.file))
            return try parse(object, id: entry.id)
        }
    }

    static func parse(_ object: [String: Any], id: String) throws -> Framing {
        func malformed(_ why: String) -> LinkFixtureLoader.Malformed {
            LinkFixtureLoader.Malformed(description: "\(id): \(why)")
        }
        if let key = Set(object.keys).symmetricDifference(framingKeys).sorted().first {
            throw malformed("the key \"\(key)\" is missing or unknown")
        }
        guard let receiver = object["receiver"] as? String, ["station", "client"].contains(receiver),
              let pieces = object["message"] as? [[String: Any]], !pieces.isEmpty,
              let rawFrames = object["frames"] as? [[String: Any]],
              let encodes = object["encodes"] as? Bool,
              let outcome = object["outcome"] as? String, ["delivered", "refused"].contains(outcome),
              let rawReplies = object["replies"] as? [String] else {
            throw malformed("a key has the wrong shape")
        }
        var message = Data()
        for piece in pieces {
            if let text = piece["text"] as? String, piece.count == 1, !text.isEmpty {
                message.append(Data(text.utf8))
            } else if let text = piece["repeat"] as? String, let times = piece["times"] as? Int, piece.count == 2,
                      !text.isEmpty, times >= 1 {
                let bytes = Data(text.utf8)
                message.reserveCapacity(message.count + bytes.count * times)
                for _ in 0..<times {
                    message.append(bytes)
                }
            } else {
                throw malformed("a message piece is not text or repeat")
            }
        }
        var frames: [Data] = []
        var allChunks = true
        for frame in rawFrames {
            if let kind = frame["chunk"] as? String, let from = frame["from"] as? Int,
               let length = frame["length"] as? Int, frame.count == 3, ["more", "last"].contains(kind),
               from >= 0, length >= 0, from + length <= message.count {
                var bytes = Data([kind == "last" ? ControlChannelFraming.last : ControlChannelFraming.more])
                bytes.append(message.subdata(in: from..<(from + length)))
                frames.append(bytes)
            } else if let hex = frame["hex"] as? String, frame.count == 1, let bytes = Self.bytes(hex: hex) {
                frames.append(bytes)
                allChunks = false
            } else {
                throw malformed("a frame is not a chunk or hex")
            }
        }
        let replies = try rawReplies.map { hex -> Data in
            guard let bytes = Self.bytes(hex: hex) else {
                throw malformed("a reply is not lower-case hex")
            }
            return bytes
        }
        if encodes && !allChunks {
            throw malformed("an encoding fixture holds a frame that is not a chunk")
        }
        return Framing(id: id, receiver: receiver, message: message, frames: frames, allChunks: allChunks,
                       encodes: encodes, delivered: outcome == "delivered", replies: replies)
    }

    static func bytes(hex: String) -> Data? {
        guard hex.count.isMultiple(of: 2), hex.allSatisfy({ "0123456789abcdef".contains($0) }) else {
            return nil
        }
        var bytes = Data(capacity: hex.count / 2)
        var index = hex.startIndex
        while index < hex.endIndex {
            let next = hex.index(index, offsetBy: 2)
            guard let byte = UInt8(hex[index..<next], radix: 16) else {
                return nil
            }
            bytes.append(byte)
            index = next
        }
        return bytes
    }

    static func hex(_ bytes: Data) -> String {
        bytes.map { String(format: "%02x", $0) }.joined()
    }

    // MARK: The manifest

    /// The manifest's kinds are the four section 16.1 names, and every file
    /// under `framing/` is listed once, as a framing fixture.
    @Test func theManifestListsEveryFramingFixtureOnce() throws {
        let manifest = try LinkFixtureLoader.manifest()
        #expect(Set(manifest.map(\.kind)).isSubset(of: ["control", "session", "media", "framing"]))
        let listed = manifest.filter { $0.kind == "framing" }.map(\.file)
        #expect(Set(listed).count == listed.count)
        let directory = LinkFixtureLoader.root.appendingPathComponent("framing")
        let files = try FileManager.default.contentsOfDirectory(atPath: directory.path)
            .filter { $0.hasSuffix(".json") }.map { "framing/\($0)" }
        #expect(Set(files) == Set(listed))
        #expect(listed.count == 13)
        #expect(manifest.filter { $0.file.hasPrefix("framing/") }.allSatisfy { $0.kind == "framing" })
    }

    /// The chunk size the app keeps is the one the surface records.
    @Test func theChunkSizeIsTheSurfaces() throws {
        let surface = try LinkFixtureLoader.jsonObject(at: LinkFixtureLoader.root.appendingPathComponent("surface.json"))
        let limits = surface["limits"] as? [String: [String: Any]]
        #expect(limits?["controlChannelChunkBytes"]?["value"] as? Int == ControlChannelFraming.chunkBytes)
        #expect(ControlChannelFraming.chunkBytes == 61_440)
        #expect(ControlChannelFraming.chunkPayloadBytes == 61_439)
    }

    // MARK: The fixtures

    /// Where `encodes` is true, the app's sender cuts the message into
    /// exactly the fixture's frames.
    @Test func theSenderCutsEachEncodingFixtureExactly() throws {
        let encoding = try Self.fixtures().filter(\.encodes)
        #expect(encoding.count >= 4)
        for fixture in encoding {
            let chunks = ControlChannelFraming.chunks(of: fixture.message)
            #expect(chunks.count == fixture.frames.count, "\(fixture.id)")
            #expect(chunks == fixture.frames, "\(fixture.id)")
        }
    }

    /// Every fixture fed frame by frame to the app's reassembler with the
    /// named end's cap: one message equal to `message` and nothing left
    /// over, or the connection ended having delivered nothing; the pings
    /// among the frames answered with exactly `replies`.
    @Test func theReassemblerReceivesEachFixtureAsItSays() throws {
        let fixtures = try Self.fixtures()
        #expect(fixtures.count == 13)
        for fixture in fixtures {
            var reassembler = ControlChannelReassembler(maxMessageBytes: fixture.cap)
            var messages: [Data] = []
            var replies: [Data] = []
            var refused = false
            for frame in fixture.frames {
                switch reassembler.feed(frame) {
                case .pending, .pong:
                    break
                case .message(let bytes):
                    messages.append(bytes)
                case .ping(let id):
                    replies.append(ControlChannelFraming.pong(id: id))
                case .refused:
                    refused = true
                }
                if refused {
                    break
                }
            }
            if fixture.delivered {
                #expect(!refused, "\(fixture.id)")
                #expect(messages == [fixture.message], "\(fixture.id)")
                #expect(reassembler.pendingBytes == 0, "\(fixture.id): something left over")
            } else {
                #expect(refused, "\(fixture.id)")
                #expect(messages.isEmpty, "\(fixture.id)")
            }
            #expect(replies.map(Self.hex) == fixture.replies.map(Self.hex), "\(fixture.id)")
        }
    }

    /// The same, through the transport a session runs over: a delivered
    /// fixture reaches the session as its one message, a refused one ends
    /// the connection with nothing passed on, and the pongs the transport
    /// sends back are exactly `replies`.
    @Test func theTransportReceivesEachFixtureAsItSays() async throws {
        for fixture in try Self.fixtures() {
            let far = FramingFarEnd()
            let transport = DataChannelSessionTransport(maxInboundMessageBytes: fixture.cap) { onEvent in
                far.attach(onEvent)
                return ControlChannelConnection(channel: far, certificateSHA256: Data(repeating: 7, count: 32))
            }
            let events = EventLog()
            _ = try await transport.open { event in
                events.append(event)
            }
            for frame in fixture.frames {
                far.feed(.binary(frame))
            }
            // One event either way: the message, or the end.
            let arrived = await events.waitFor(count: 1)
            #expect(arrived, "\(fixture.id): nothing reached the session")
            let seen = events.events
            if fixture.delivered {
                let text = try #require(String(data: fixture.message, encoding: .utf8))
                #expect(seen == [.text(text)], "\(fixture.id)")
                #expect(!far.closed, "\(fixture.id)")
            } else {
                #expect(seen == [.closed], "\(fixture.id)")
                #expect(far.closed, "\(fixture.id): the channel was not closed")
            }
            #expect(far.sent.map(Self.hex) == fixture.replies.map(Self.hex), "\(fixture.id)")
            transport.close()
        }
    }
}

/// A control channel whose far end a framing test feeds.
final class FramingFarEnd: ControlChannel, @unchecked Sendable {
    private let lock = NSLock()
    private var onEvent: (@Sendable (ControlChannelEvent) -> Void)?
    private var frames: [Data] = []
    private var isClosed = false

    func attach(_ onEvent: @escaping @Sendable (ControlChannelEvent) -> Void) {
        lock.withLock { self.onEvent = onEvent }
    }

    func feed(_ event: ControlChannelEvent) {
        lock.withLock { onEvent }?(event)
    }

    func send(_ frame: Data) -> Bool {
        lock.withLock {
            guard !isClosed else {
                return false
            }
            frames.append(frame)
            return true
        }
    }

    func close() {
        lock.withLock { isClosed = true }
    }

    var sent: [Data] { lock.withLock { frames } }
    var closed: Bool { lock.withLock { isClosed } }
}

/// The events a transport passed on, for a test to wait for.
final class EventLog: @unchecked Sendable {
    private let lock = NSLock()
    private var log: [LinkTransportEvent] = []
    private let waiters = ConditionWaiters()

    func append(_ event: LinkTransportEvent) {
        lock.withLock { log.append(event) }
        waiters.release()
    }

    var events: [LinkTransportEvent] { lock.withLock { log } }

    func waitFor(count: Int, within timeout: Duration = .seconds(10)) async -> Bool {
        await waiters.wait(within: timeout) { [self] in events.count >= count }
    }
}
