// NereusSDR for iOS: the media peer against the station's own media transport, across two processes
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

// Foundation's Process, which runs the station's helper, exists only on
// macOS; on the iOS simulator the interop tests are not built.
#if os(macOS)

import Foundation
import LinkTestSupport
import Testing
@testable import NereusMedia

/// R-IOS-01: the app's ``MediaPeer`` (libdatachannel with Mbed TLS) answers
/// the Core's `LibDataChannelMediaTransport` (libdatachannel with OpenSSL),
/// run by `nereus_media_offerer` in its own process, over 127.0.0.1. Run by
/// `ios/scripts/interop-test.sh`, which builds the helper and sets
/// `NEREUS_MEDIA_OFFERER`; without it these tests are skipped.
@Suite(.serialized, .enabled(if: OffererProcess.path != nil, "set NEREUS_MEDIA_OFFERER (ios/scripts/interop-test.sh)"))
struct MediaPeerInteropTests {
    private static func connected(wrongFingerprint: Bool = false) throws -> InteropRelay {
        InteropRelay(peer: MediaPeer(), offerer: try OffererProcess(wrongFingerprint: wrongFingerprint))
    }

    @Test func dtlsConnectsAndTheDisplayFixturesArriveAndDecode() async throws {
        let relay = try Self.connected()
        defer { relay.stop() }
        try await relay.waitUntilConnected()
        #expect(relay.refused.isEmpty, "\(relay.refused)")

        let vectors = try LinkFixtureLoader.mediaVectors()
        let full = try #require(vectors["media-nsdc1-full"])
        let delta = try #require(vectors["media-nsdc1-delta"])
        relay.offerer.send(["type": "send-display"])
        let sent = try await relay.offerer.waitFor("sent")
        #expect(sent.strings["what"] == "display")
        #expect(sent.numbers["count"] == 2)

        let datagrams = try await Self.take(2, from: relay.peer.displayDatagrams)
        // The channel is unordered; decode in the order the frames were made.
        #expect(Set(datagrams) == Set([full.bytes, delta.bytes]))
        let decoder = DisplayFrameDecoder()
        let first = decoder.decode(full.bytes)
        let second = decoder.decode(delta.bytes)
        #expect(first.disposition == .accepted)
        #expect(first.frame != nil)
        #expect(second.disposition == .accepted)
        #expect(second.frame != nil)
    }

    @Test func opusPacketsOnPayloadType111ArriveAndDecode() async throws {
        let relay = try Self.connected()
        defer { relay.stop() }
        try await relay.waitUntilConnected()

        let vectors = try LinkFixtureLoader.mediaVectors()
        var fixtures: [RtpPacket] = []
        for index in 1...4 {
            let vector = try #require(vectors["media-opus-\(index)"])
            fixtures.append(try #require(RtpPacket(parsing: vector.bytes)))
        }
        relay.offerer.send(["type": "send-opus"])
        let sent = try await relay.offerer.waitFor("sent")
        #expect(sent.numbers["count"] == 4)

        let packets = try await Self.take(4, from: relay.peer.audioPackets)
        let decoder = try OpusDecoder(channels: 2)
        for packet in packets.sorted(by: { $0.sequence < $1.sequence }) {
            #expect(packet.payloadType == MediaPeer.opusPayloadType)
            let fixture = try #require(fixtures.first { $0.sequence == packet.sequence })
            #expect(packet == fixture)
            let pcm = try decoder.decode(packet.payload)
            #expect(!pcm.isEmpty)
            #expect(pcm.count % 2 == 0)
        }
    }

    @Test func theAnswerCarriesNoCandidatesAndEachCandidateTrickles() async throws {
        let relay = try Self.connected()
        defer { relay.stop() }
        try await relay.waitUntilConnected()

        let answer = try #require(relay.answer)
        #expect(answer.hasPrefix("v=0"))
        #expect(!MediaPeer.embedsCandidates(answer))
        #expect(answer.contains("a=fingerprint:sha-256 "))
        let candidates = relay.localCandidates
        #expect(!candidates.isEmpty)
        for candidate in candidates {
            #expect(MediaPeer.candidateType(candidate.candidate) == "host")
            #expect(candidate.candidate.utf8.count <= MediaPeer.maxCandidateBytes)
            #expect(!candidate.mid.isEmpty && candidate.mid.utf8.count <= MediaPeer.maxMidBytes)
        }
    }

    @Test func aWrongFingerprintInTheOfferNeverConnects() async throws {
        let relay = try Self.connected(wrongFingerprint: true)
        defer { relay.stop() }
        try await relay.waitUntil("the media peer to fail", timeout: .seconds(30)) {
            let history = relay.stateHistory
            return history.contains(.failed) || history.contains(.closed)
        }
        // The Core's certificate did not match the offer's fingerprint, so
        // the DTLS handshake failed: neither side reached a usable
        // connection and no media can arrive.
        #expect(relay.stateHistory.contains(.failed), "\(relay.stateHistory)")
        #expect(!relay.stateHistory.contains(.connected), "\(relay.stateHistory)")
        #expect(!relay.offerer.received.contains { $0.type == "ready" })
        #expect(relay.refused.isEmpty, "\(relay.refused)")
        let counts = relay.peer.receiveCounts
        #expect(counts.audio.accepted == 0)
        #expect(counts.display.accepted == 0)
    }

    @Test func anExpectedSsrcDropsRtpFromEveryOtherSsrc() async throws {
        let relay = try Self.connected()
        defer { relay.stop() }
        try await relay.waitUntilConnected()
        let ssrcs = try await relay.offerer.waitFor("ssrc")
        let main = try #require(ssrcs.numbers["main"])
        let foreign = try #require(ssrcs.numbers["foreign"])
        #expect(main != foreign)
        relay.peer.setExpectedAudioSsrc(UInt32(main))

        relay.offerer.send(["type": "send-foreign-audio", "count": 20])
        let foreignSent = try await relay.offerer.waitFor("sent")
        #expect(foreignSent.numbers["count"] == 20)
        try await relay.waitUntil("the other stream's packets to arrive") {
            relay.peer.receiveCounts.foreignSsrcDropped == 20
        }
        relay.offerer.send(["type": "send-audio-burst", "count": 5])
        _ = try await relay.offerer.waitFor("sent", occurrence: 2)

        let packets = try await Self.take(5, from: relay.peer.audioPackets)
        #expect(packets.allSatisfy { $0.ssrc == UInt32(main) })
        #expect(relay.peer.receiveCounts.audio.accepted == 5)
    }

    @Test func underABurstTheQueuesHoldTheirBoundsByDroppingTheOldest() async throws {
        let relay = try Self.connected()
        defer { relay.stop() }
        try await relay.waitUntilConnected()
        let peer = relay.peer

        // Audio: 800 Opus packets, more than the 64 the queue holds for
        // Opus (640 for lossless), none drained until they are all in.
        relay.offerer.send(["type": "send-audio-burst", "count": 800])
        let audioLine = try await relay.offerer.waitFor("sent")
        let audioSent = try #require(audioLine.numbers["count"])
        #expect(audioSent == 800)
        try await relay.waitUntil("the audio burst to arrive") {
            peer.receiveCounts.audio.accepted == audioSent
        }
        let audio = peer.receiveCounts.audio
        #expect(audio.held == MediaPeer.opusAudioQueuePackets)
        #expect(audio.dropped == audioSent - MediaPeer.opusAudioQueuePackets)
        let kept = try await Self.take(MediaPeer.opusAudioQueuePackets, from: peer.audioPackets)
        #expect(kept.map(\.sequence) == Array(UInt16(audioSent - MediaPeer.opusAudioQueuePackets)..<UInt16(audioSent)))

        // Display by count: 20 small messages, 8 kept.
        relay.offerer.send(["type": "send-display-burst", "count": 20, "bytes": 1000])
        _ = try await relay.offerer.waitFor("sent", occurrence: 2)
        try await relay.waitUntil("the display burst to arrive") {
            peer.receiveCounts.display.accepted == 20
        }
        #expect(peer.receiveCounts.display.held == MediaPeer.displayQueueMessages)
        let byCount = try await Self.take(MediaPeer.displayQueueMessages, from: peer.displayDatagrams)
        #expect(byCount.map(Self.index).sorted() == Array(12..<20))

        // Display by size: 6 messages of 60000 bytes, of which 4 fit in 256 KiB.
        relay.offerer.send(["type": "send-display-burst", "count": 6, "bytes": 60000])
        _ = try await relay.offerer.waitFor("sent", occurrence: 3)
        try await relay.waitUntil("the large display burst to arrive") {
            peer.receiveCounts.display.accepted == 26
        }
        let held = peer.receiveCounts.display.held
        #expect(held == MediaPeer.displayQueueBytes / 60000)
        let bySize = try await Self.take(held, from: peer.displayDatagrams)
        #expect(bySize.map(Self.index).sorted() == Array((6 - held)..<6))
    }

    // MARK: Helpers

    /// The index a burst message starts with.
    private static func index(_ message: Data) -> Int {
        message.prefix(4).reduce(0) { $0 << 8 | Int($1) }
    }

    /// The next `count` elements of `stream`, or a failure after `timeout`.
    private static func take<Element: Sendable>(_ count: Int, from stream: AsyncStream<Element>,
                                                timeout: Duration = .seconds(10)) async throws -> [Element] {
        try await withThrowingTaskGroup(of: [Element]?.self) { group in
            group.addTask {
                var taken: [Element] = []
                for await element in stream {
                    taken.append(element)
                    if taken.count == count {
                        break
                    }
                }
                return taken
            }
            group.addTask {
                try await Task.sleep(for: timeout)
                return nil
            }
            defer { group.cancelAll() }
            guard let first = try await group.next(), let taken = first, taken.count == count else {
                throw OffererProcess.Failure(description: "fewer than \(count) elements within \(timeout)")
            }
            return taken
        }
    }
}

#endif
