// NereusSDR for iOS: sound only, from the app's media control client to the Core's own media peer and back
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

// Foundation's Process, which runs the station's helper, exists only on
// macOS; on the iOS simulator the interop tests are not built.
#if os(macOS)

import Foundation
import LinkTestSupport
import NereusLink
import Testing
@testable import NereusMedia

/// R-IOS-10, sound only: the app's ``MediaControlClient`` over a real
/// ``MediaPeer`` against `nereus_media_offerer --media-control`, which runs
/// the Core's own MediaPeer as the offerer under the app's connection ID.
/// Part of the serialized ``MediaPeerInteropTests`` suite, so it never runs
/// beside the other real-peer tests.
extension MediaPeerInteropTests {
    /// Carries media control between the client and the helper, as the
    /// session would, with every candidate on 127.0.0.1 (see ``InteropRelay``).
    private final class ControlRelay: @unchecked Sendable {
        private let lock = NSLock()
        private var reported: [MediaControlEvent] = []
        private var tasks: [Task<Void, Never>] = []

        init(client: MediaControlClient, offerer: OffererProcess) {
            let fromOfferer = Task { [client, offerer] in
                var handled = 0
                while !Task.isCancelled {
                    let lines = offerer.received
                    while handled < lines.count {
                        let line = lines[handled]
                        handled += 1
                        guard line.type == "media-control", var payload = line.payload else {
                            continue
                        }
                        if case .string(let candidate)? = payload["candidate"] {
                            guard let loopback = LoopbackCandidate.rewrite(candidate) else {
                                continue
                            }
                            payload["candidate"] = .string(loopback)
                        }
                        await client.handle(.message(.mediaControl(LinkMessage.MediaControl(payload: payload))))
                    }
                    try? await Task.sleep(for: .milliseconds(5))
                }
            }
            let events = Task { [weak self, client] in
                for await event in client.events {
                    self?.lock.withLock { self?.reported.append(event) }
                }
            }
            tasks = [fromOfferer, events]
        }

        deinit {
            tasks.forEach { $0.cancel() }
        }

        func stop() {
            tasks.forEach { $0.cancel() }
        }

        var events: [MediaControlEvent] {
            lock.withLock { reported }
        }
    }

    /// The app's operations to the helper, candidates on 127.0.0.1.
    private static func sender(to offerer: OffererProcess) -> MediaControlClient.Sender {
        { message in
            guard case .mediaControl(let control) = message else {
                return
            }
            var payload = control.payload
            if case .string(let candidate)? = payload["candidate"] {
                guard let loopback = LoopbackCandidate.rewrite(candidate) else {
                    return
                }
                payload["candidate"] = .string(loopback)
            }
            offerer.sendControl(payload)
        }
    }

    @Test func soundOnlyAStartWithNoDisplayAndAnAudioRequestGivesAudioAndNoDisplay() async throws {
        // A Core at minor 11 that offers media and nothing more, so the
        // audio context comes in its eight-key shape.
        let run = try await soundOnly(capabilities: ["remoteMediaVersion": 1])
        defer { run.stop() }
        #expect(run.context.profile == nil && run.context.encoder == nil)
        #expect(!run.offer.contains(" L16/"), "no lossless without the declaration")
    }

    /// The app declares audioProfileVersion, as the media control document
    /// requires of it, so a Core with lossless allowed offers L16 at payload
    /// type 96 on the Opus m-line. The app answers without its audio line
    /// being refused, Opus is what plays, and the payload type 96 packets
    /// the Core then sends on the same SSRC are dropped and counted, never
    /// decoded.
    @Test func anOfferCarryingLosslessIsAnsweredAndOpusPlays() async throws {
        let run = try await soundOnly(capabilities: ["remoteMediaVersion": 1, "remoteAudioStatusVersion": 1,
                                                     "audioProfileVersion": 1])
        defer { run.stop() }
        // The Core's own offer: Opus at 111 first, L16/48000/2 at 96 beside it.
        let audioLine = try #require(run.offer.components(separatedBy: "\r\n").first { $0.hasPrefix("m=audio") })
        #expect(audioLine.hasSuffix(" 111 96"), "\(audioLine)")
        #expect(run.offer.contains("a=rtpmap:111 opus/48000/2"))
        #expect(run.offer.contains("a=rtpmap:96 L16/48000/2"))
        // Negotiated: the audio-profile context says Opus with its encoder,
        // and the Opus stream played (checked in soundOnly).
        #expect(run.context.profile == .opus)
        #expect(run.context.encoder?.frameSamples == 1920)
        #expect(!run.context.lossless)

        let dropsBefore = run.playback.counters.otherPayloadDrops
        #expect(dropsBefore == 0)
        run.offerer.send(["type": "send-l16", "count": 2])
        let sent = try await run.offerer.waitFor("sent", occurrence: 1, timeout: .seconds(10), what: "l16",
                                                 count: 2)
        #expect(sent.numbers["count"] == 2)
        let playback = run.playback
        try await waitUntil("the payload type 96 packets to be dropped") {
            playback.counters.otherPayloadDrops == 2
        }
        // Dropped at the buffer's door, so never held and never decoded; the
        // Opus sequence numbers they took are concealed like any lost packet.
        #expect(playback.counters.otherPayloadDrops == 2)
        // Opus carries on after them: once it has refilled the buffer, the
        // next pulls play decoded audio.
        try await waitUntil("Opus to refill the buffer") {
            playback.withJitterBuffer { $0.depthMs } >= 400
        }
        let after = playback.withJitterBuffer { buffer in (0..<6).map { _ in buffer.pull() } }
        #expect(after.contains { if case .audio = $0 { return true } else { return false } })
    }

    /// Task 55 (R-IOS-20, R-IOS-13): a Core that told the session
    /// remoteTxVersion gets `remoteTxVersion` in the start, so the Core's
    /// own MediaPeer offers the microphone line (with L16 beside Opus, as a
    /// Core with lossless allowed writes it) and the "tx" channel. The app
    /// answers the line with Opus and the offered L16 (R-IOS-09) on the
    /// Core's own derivation of its SSRC: the Core's MediaPeer takes the
    /// app's packets on the line, the line negotiates lossless, and the
    /// keepalive arrives on "tx" as the 13 bytes the media control
    /// document gives.
    @Test func theMicrophoneLineCarriesOpusOnItsSsrcAndTheKeepaliveGoesOnTx() async throws {
        let offerer = try OffererProcess(mediaControl: true)
        let client = MediaControlClient(send: Self.sender(to: offerer))
        let relay = ControlRelay(client: client, offerer: offerer)
        defer {
            relay.stop()
            offerer.stop()
        }
        let capabilities: [String: Int64] = ["remoteMediaVersion": 1, "remoteAudioStatusVersion": 1,
                                             "audioProfileVersion": 1, "remoteTxVersion": 1]
        await client.handle(.message(.hello(LinkMessage.Hello(major: 1, minor: 11, settingsSchema: 0,
                                                             peer: "nereusd", majors: [1], features: [:]))))
        await client.handle(.message(.capabilities(LinkMessage.Capabilities(properties: capabilities.keys.sorted().map {
            LinkMessage.PropertyEntry(name: $0, value: .i64(capabilities[$0] ?? 0))
        }))))
        await client.handle(.message(.snapshotComplete))
        await client.handle(.stateChanged(.ready))
        let id = try #require(await client.connectionId)

        let ssrcLine = try await offerer.waitFor("ssrc")
        let microphone = MediaControlClient.microphoneSsrc(forConnection: id)
        // The Core's own derivation of the line's SSRC is the app's.
        #expect(ssrcLine.numbers["mic"].map(UInt32.init) == microphone)
        let ready = try await offerer.waitFor("ready", timeout: .seconds(20))
        #expect(ready.numbers["micLossless"] == 1, "the app answers the line with the offered L16")
        try await waitUntil("the microphone line") {
            relay.events.contains(.microphoneLine(true))
        }
        let offer = try #require(relay.events.compactMap { event -> String? in
            if case .description(let description) = event { return description.sdp } else { return nil }
        }.first)
        #expect(offer.contains("a=mid:mic"))
        #expect(offer.contains("a=group:BUNDLE audio mic 0"))
        #expect(!offerer.received.contains { $0.type == "refused" }, "\(offerer.received.map(\.type))")

        // Ten 20 ms frames of a tone, through the app's own encoder.
        let encoder = try OpusEncoder(profile: .microphone)
        for frame in 0..<10 {
            let pcm = (0..<960).map { sample in
                Float(0.3 * sin(2 * Double.pi * 440 * Double(frame * 960 + sample) / 48000))
            }
            #expect(client.uplink.sendMicrophone(try encoder.encode(pcm)))
        }
        let tenth = try await offerer.waitFor("mic", timeout: .seconds(10), count: 10)
        #expect(tenth.numbers["ssrc"].map(UInt32.init) == microphone)
        #expect(tenth.numbers["pt"] == Int(MediaPeer.opusPayloadType))

        // The keepalive, once the Core's "tx" channel is open.
        try await waitUntil("the tx channel") {
            client.uplink.sendKeepalive(sequence: 7, epoch: 4_294_967_295)
        }
        let tx = try await offerer.waitFor("tx")
        #expect(tx.strings["hex"] == "010000000000000007ffffffff")
    }

    /// What one sound-only run leaves for its test to check.
    private struct SoundOnlyRun {
        let offerer: OffererProcess
        let relay: ControlRelay
        let playback: AudioPlaybackCore
        let offer: String
        let context: MediaControlEvent.AudioContext

        func stop() {
            relay.stop()
            offerer.stop()
        }
    }

    /// A start with no display subscriptions, then an audio request: audio
    /// plays and no display arrives.
    private func soundOnly(capabilities: [String: Int64]) async throws -> SoundOnlyRun {
        let offerer = try OffererProcess(mediaControl: true)
        let playback = try AudioPlaybackCore()
        let client = MediaControlClient(send: Self.sender(to: offerer), playback: playback)
        let relay = ControlRelay(client: client, offerer: offerer)
        var finished = false
        defer {
            if !finished {
                relay.stop()
                offerer.stop()
            }
        }

        await client.handle(.message(.hello(LinkMessage.Hello(major: 1, minor: 11, settingsSchema: 0,
                                                             peer: "nereusd", majors: [1], features: [:]))))
        await client.handle(.message(.capabilities(LinkMessage.Capabilities(properties: capabilities.keys.sorted().map {
            LinkMessage.PropertyEntry(name: $0, value: .i64(capabilities[$0] ?? 0))
        }))))
        await client.handle(.message(.snapshotComplete))
        await client.handle(.stateChanged(.ready))
        let id = try #require(await client.connectionId)

        let ssrcLine = try await offerer.waitFor("ssrc")
        // The helper's SSRC is the Core's own derivation from the app's ID.
        #expect(ssrcLine.numbers["main"].map(UInt32.init) == MediaControlClient.audioSsrc(forConnection: id))
        _ = try await offerer.waitFor("ready", timeout: .seconds(20))
        try await waitUntil("the app's media connection") {
            relay.events.contains(.mediaState(.connected))
        }
        let offer = try #require(relay.events.compactMap { event -> String? in
            if case .description(let description) = event { return description.sdp } else { return nil }
        }.first)

        // Audio stays off until the app asks: no audio op, nothing anchored.
        #expect(!offerer.received.contains { $0.type == "op" && $0.strings["op"] == "audio" })
        #expect(playback.anchor == nil)

        await client.setAudioEnabled(true)
        _ = try await offerer.waitFor("sent", what: "audio")
        try await waitUntil("the audio context") {
            relay.events.contains { if case .audioContext = $0 { return true } else { return false } }
        }
        let context = try #require(relay.events.compactMap { event -> MediaControlEvent.AudioContext? in
            if case .audioContext(let context) = event { return context } else { return nil }
        }.first)
        let anchor = try #require(playback.anchor)
        #expect(anchor.ssrc == MediaControlClient.audioSsrc(forConnection: id))
        #expect(anchor.firstSequence == 100)
        // Packets reach the jitter buffer and play as decoded audio.
        try await waitUntil("five packets buffered") {
            playback.withJitterBuffer { $0.depthMs } >= 200
        }
        // Packets sent before the context reached the app are not taken, so
        // the first pulls may conceal them; audio follows within the depth.
        let blocks = playback.withJitterBuffer { buffer in (0..<5).map { _ in buffer.pull() } }
        let played = blocks.compactMap { block -> [Float]? in
            if case .audio(let pcm) = block { return pcm } else { return nil }
        }
        #expect(!played.isEmpty, "\(blocks.count) pulls and none played")
        #expect(played.allSatisfy { $0.count == 1920 * 2 && $0.contains { $0 != 0 } })

        // No display: the helper saw only start, the signalling and audio,
        // and the app decoded no display frame.
        let ops = Set(offerer.received.filter { $0.type == "op" }.compactMap { $0.strings["op"] })
        #expect(ops == ["start", "description", "candidate", "audio"])
        #expect(!offerer.received.contains { $0.type == "refused" }, "\(offerer.received.map(\.type))")
        #expect(!relay.events.contains { if case .displayFrame = $0 { return true } else { return false } })
        #expect(await client.endpointIds.isEmpty)
        #expect(relay.events.contains(.mediaState(.connected)))
        #expect(!relay.events.contains(.mediaState(.failed)), "the audio line was not refused")
        finished = true
        return SoundOnlyRun(offerer: offerer, relay: relay, playback: playback, offer: offer, context: context)
    }

    private func waitUntil(_ what: String, timeout: Duration = .seconds(20),
                           _ condition: () -> Bool) async throws {
        let clock = ContinuousClock()
        let deadline = clock.now + timeout
        while !condition() {
            guard clock.now < deadline else {
                throw OffererProcess.Failure(description: "timed out waiting for \(what)")
            }
            try await Task.sleep(for: .milliseconds(10))
        }
    }
}

#endif
