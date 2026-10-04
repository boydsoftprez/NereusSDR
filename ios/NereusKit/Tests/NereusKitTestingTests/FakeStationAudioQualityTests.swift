// NereusSDR for iOS: the fake Core answers the phone's audio quality as the Core does: opusBitrate, its refusal, lossless and the microphone line
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkTestSupport
import NereusKitTesting
import NereusLink
import NereusMedia
import NereusMirror
import Testing

/// R-IOS-09: made with ``FakeStation/Additions/audioQuality`` the fake
/// plays a Core with the per-device audio quality (the media control
/// document, "Per-device audio quality"): it advertises
/// `audioQualityVersion` 1 beside the audio profile, answers each `audio`
/// with one `audio-context` in the profile shape, takes a bitrate in the
/// measured table and refuses any other in the Core's words with the
/// encoder left as it was, and gives lossless or refuses it as its
/// `audio_lossless` setting says.
@Suite("FakeStation, the phone's audio quality", .serialized)
@MainActor
struct FakeStationAudioQualityTests {
    final class MediaSeen {
        var payloads: [[String: LinkJSON]] = []
    }

    struct Rig {
        let media: MediaSeen
        let station: FakeStation
        let session: StationSession
        let mirror: MirrorStore
        let feeding: Task<Void, Never>
    }

    private func connected(_ additions: FakeStation.Additions,
                           lossless: FakeStation.LosslessAnswer = .given) async throws -> Rig {
        let station = try FakeStation(additions: additions)
        station.losslessAnswer = lossless
        let session = station.makeSession()
        let mirror = MirrorStore(session: session)
        let events = session.events
        let media = MediaSeen()
        let feeding = Task { @MainActor in
            for await event in events {
                mirror.handle(event)
                if case .message(.mediaControl(let control)) = event {
                    media.payloads.append(control.payload)
                }
            }
        }
        await session.connect()
        #expect(await station.waitUntilLive())
        #expect(await poll { mirror.isSnapshotComplete })
        return Rig(media: media, station: station, session: session, mirror: mirror, feeding: feeding)
    }

    private func poll(_ condition: @MainActor () -> Bool) async -> Bool {
        for _ in 0..<20_000 {
            if condition() {
                return true
            }
            await Task.yield()
        }
        return condition()
    }

    static let id = "0123456789abcdef0123456789abcdef"

    private func send(_ rig: Rig, _ payload: [String: LinkJSON]) async throws {
        try await rig.session.send(.mediaControl(LinkMessage.MediaControl(payload: payload)))
        #expect(await rig.station.waitForMessage { $0 == .mediaControl(LinkMessage.MediaControl(payload: payload)) } != nil)
    }

    private func audio(_ revision: Int, profile: String = "opus", bitrate: Int? = nil) -> [String: LinkJSON] {
        var payload: [String: LinkJSON] = ["op": .string("audio"), "connectionId": .string(Self.id),
                                           "revision": .number(Double(revision)), "enabled": .bool(true),
                                           "profile": .string(profile)]
        if let bitrate {
            payload["opusBitrate"] = .number(Double(bitrate))
        }
        return payload
    }

    /// The `audio-context` answers the fake sent, in order.
    private func contexts(_ rig: Rig) -> [[String: LinkJSON]] {
        rig.media.payloads.filter { $0["op"] == .string("audio-context") }
    }

    private func next(_ rig: Rig, after count: Int) async throws -> [String: LinkJSON] {
        #expect(await poll { contexts(rig).count > count })
        return try #require(contexts(rig).dropFirst(count).first)
    }

    @Test("it advertises the quality beside the audio profile, and not without the addition")
    func capabilities() async throws {
        let rig = try await connected([.audioQuality])
        defer { rig.feeding.cancel() }
        for name in ["remoteMediaVersion", "remoteAudioStatusVersion", "audioProfileVersion", "audioQualityVersion"] {
            #expect(rig.mirror.capabilityVersion(name) == 1, "\(name)")
        }
        await rig.session.disconnect()

        let older = try await connected([.wideband])
        defer { older.feeding.cancel() }
        #expect(older.mirror.capabilityVersion("audioQualityVersion") == 0)
        try await send(older, audio(1, bitrate: 24000))
        for _ in 0..<5_000 {
            await Task.yield()
        }
        #expect(contexts(older).isEmpty, "an older fake leaves audio to the test")
        await older.session.disconnect()
    }

    @Test("a measured bitrate is taken; any other is refused in the Core's words and the encoder stays")
    func bitrates() async throws {
        let rig = try await connected([.audioQuality])
        defer { rig.feeding.cancel() }
        try await send(rig, audio(1))
        let first = try await next(rig, after: 0)
        // A new media peer starts at the Core's audio_bitrate, 48000.
        #expect(first["encoder"] == .object(["codec": .string("opus"), "sampleRate": .number(48000),
                                             "channels": .number(2), "frameSamples": .number(1920),
                                             "targetBitrate": .number(48000), "audioBandwidthHz": .number(20000)]))
        #expect(first["profile"] == .string("opus"))
        #expect(first["opusBitrateRefusal"] == nil)
        #expect(first["ssrc"] == .number(Double(MediaControlClient.audioSsrc(forConnection: Self.id))))
        let decoded = try #require(MediaControlDecoder.audioContext(first, detail: true, profile: true,
                                                                    bitrate: true))
        #expect(decoded.revision == 1)

        try await send(rig, audio(2, bitrate: 24000))
        let saving = try await next(rig, after: 1)
        if case .object(let encoder)? = saving["encoder"] {
            #expect(encoder["targetBitrate"] == .number(24000))
            #expect(encoder["audioBandwidthHz"] == .number(8000))
        } else {
            Issue.record("an Opus encoder")
        }

        try await send(rig, audio(3, bitrate: 32000))
        let refused = try await next(rig, after: 2)
        #expect(refused["opusBitrateRefusal"] == .string(FakeStation.opusBitrateRefusal))
        if case .object(let encoder)? = refused["encoder"] {
            #expect(encoder["targetBitrate"] == .number(24000), "the running encoder stays")
        } else {
            Issue.record("an Opus encoder")
        }
        #expect(FakeStation.opusBitrateRefusal
                == "This Core does not offer that audio quality. The audio stays as it was.")
        #expect(rig.station.audioRequests.map(\.opusBitrate) == [nil, 24000, 32000])
        await rig.session.disconnect()
    }

    @Test("lossless is given, or refused as not allowed or unavailable, with Opus running")
    func lossless() async throws {
        let rig = try await connected([.audioQuality])
        defer { rig.feeding.cancel() }
        try await send(rig, audio(1, profile: "lossless", bitrate: 48000))
        let given = try await next(rig, after: 0)
        #expect(given["profile"] == .string("lossless"))
        #expect(given["encoder"] == .object(["codec": .string("l16"), "sampleRate": .number(48000),
                                             "channels": .number(2), "frameSamples": .number(192),
                                             "bitsPerSample": .number(16), "payloadType": .number(96)]))
        #expect(MediaControlDecoder.audioContext(given, detail: true, profile: true, bitrate: true)?.lossless == true)
        await rig.session.disconnect()

        for (answer, code) in [(FakeStation.LosslessAnswer.notAllowed, "lossless-not-allowed"),
                               (.unavailable, "lossless-unavailable")] {
            let denied = try await connected([.audioQuality], lossless: answer)
            defer { denied.feeding.cancel() }
            try await send(denied, audio(1, profile: "lossless", bitrate: 24000))
            let context = try await next(denied, after: 0)
            #expect(context["profile"] == .string("opus"))
            #expect(context["profileRefusal"] == .string(code))
            if case .object(let encoder)? = context["encoder"] {
                #expect(encoder["targetBitrate"] == .number(24000), "the Opus the refusal leaves is the asked bitrate")
            } else {
                Issue.record("an Opus encoder")
            }
            await denied.session.disconnect()
        }
    }

    @Test("the microphone line carries lossless only where the Core gives it")
    func microphoneLine() throws {
        for (additions, answer, carried) in [
            (FakeStation.Additions.audioQuality, FakeStation.LosslessAnswer.given, true),
            (.audioQuality, .notAllowed, false), ([.wideband], .given, false),
        ] {
            let station = try FakeStation(additions: additions)
            station.losslessAnswer = answer
            let peer = station.mediaPeerFactory()
            #expect(peer.microphoneLosslessNegotiated == carried)
        }
    }
}
