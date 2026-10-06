// NereusSDR for iOS: the phone's own audio quality on the media connection: opusBitrate, lossless and the Core's refusal
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkSessionTestSupport
import LinkTestSupport
import NereusLink
import Testing
@testable import NereusMedia

/// R-IOS-09 (the media control document, "Per-device audio quality"): the
/// `audio` control carries `opusBitrate` beside `profile` only on a
/// connection to a Core that told this app `audioQualityVersion` 1 at
/// minor 11; `profile` is `lossless` while the phone wants lossless; and
/// the Core's `opusBitrateRefusal` reaches the app in its own words.
@Suite struct AudioQualityMediaTests {
    typealias Rig = MediaControlClientTests.Rig

    static let refusalWords = "This Core does not offer that audio quality. The audio stays as it was."

    static func capabilities(quality: Int64 = 1, profile: Int64 = 1) -> [String: Int64] {
        ["remoteMediaVersion": 1, "remoteAudioStatusVersion": 1, "audioProfileVersion": profile,
         "audioQualityVersion": quality]
    }

    /// An `audio-context` in the profile shape, as the Core writes it.
    static func context(_ id: String, revision: UInt32, generation: UInt32, profile: String = "opus",
                        bitrate: Int = 48000, bandwidthHz: Int = 20000, profileRefusal: String? = nil,
                        bitrateRefusal: String? = nil, firstSequence: UInt16 = 100) -> [String: LinkJSON] {
        var payload: [String: LinkJSON] = [
            "op": "audio-context", "connectionId": .string(id), "revision": .number(Double(revision)),
            "generation": .number(Double(generation)), "enabled": true,
            "ssrc": .number(Double(MediaControlClient.audioSsrc(forConnection: id))),
            "firstSequence": .number(Double(firstSequence)), "firstTimestamp": 0, "profile": .string(profile),
        ]
        if profile == "lossless" {
            payload["encoder"] = .object(["codec": "l16", "sampleRate": 48000, "channels": 2,
                                          "frameSamples": 192, "bitsPerSample": 16, "payloadType": 96])
        } else {
            payload["encoder"] = .object(["codec": "opus", "sampleRate": 48000, "channels": 2,
                                          "frameSamples": 1920, "targetBitrate": .number(Double(bitrate)),
                                          "audioBandwidthHz": .number(Double(bandwidthHz))])
        }
        if let profileRefusal {
            payload["profileRefusal"] = .string(profileRefusal)
        }
        if let bitrateRefusal {
            payload["opusBitrateRefusal"] = .string(bitrateRefusal)
        }
        return payload
    }

    /// The revision of the last `audio` the rig sent.
    static func lastRevision(_ rig: Rig) throws -> UInt32 {
        guard case .number(let value)? = rig.recorder.sent("audio").last?["revision"] else {
            throw AudioQualityTestError.noAudioSent
        }
        return UInt32(value)
    }

    static func audioContexts(_ rig: Rig) -> [MediaControlEvent.AudioContext] {
        rig.recorder.events.compactMap { event in
            if case .audioContext(let context) = event { return context }
            return nil
        }
    }

    enum AudioQualityTestError: Error { case noAudioSent }

    // MARK: The gate

    @Test(arguments: [
        (UInt16(11), Int64(1), Int64(1), true), (UInt16(11), Int64(2), Int64(1), true),
        (UInt16(11), Int64(0), Int64(1), false), (UInt16(10), Int64(1), Int64(1), false),
        (UInt16(11), Int64(1), Int64(0), false),
    ])
    func theGateNeedsMinor11TheVersionAndTheProfile(minor: UInt16, quality: Int64, profile: Int64, on: Bool) {
        let offered = Self.capabilities(quality: quality, profile: profile)
        let gates = MediaFeatureGates(agreedMinor: minor) { offered[$0] ?? 0 }
        #expect(gates.audioQuality == on)
        let mediaOff = MediaFeatureGates(agreedMinor: minor) { $0 == "remoteMediaVersion" ? 0 : offered[$0] ?? 0 }
        #expect(!mediaOff.audioQuality)
    }

    // MARK: opusBitrate

    @Test func theBitrateGoesBesideTheProfileToACoreThatOffersIt() async throws {
        let rig = try Rig()
        await rig.open(minor: 11, capabilities: Self.capabilities())
        try await rig.connect()
        let id = try await rig.connectionId
        await rig.client.setAudioRequest(MediaControlClient.AudioRequest(lossless: false, opusBitrate: 48000))
        await rig.client.setAudioEnabled(true)
        #expect(rig.recorder.sent("audio").last == [
            "op": "audio", "connectionId": .string(id), "revision": .number(Double(try Self.lastRevision(rig))),
            "enabled": true, "profile": "opus", "opusBitrate": 48000,
        ])
    }

    @Test(arguments: [
        (UInt16(11), Int64(0), Int64(1)), (UInt16(10), Int64(1), Int64(1)), (UInt16(11), Int64(1), Int64(0)),
    ])
    func noBitrateGoesToACoreThatDoesNotOfferIt(minor: UInt16, quality: Int64, profile: Int64) async throws {
        let rig = try Rig()
        await rig.open(minor: minor, capabilities: Self.capabilities(quality: quality, profile: profile))
        try await rig.connect()
        await rig.client.setAudioRequest(MediaControlClient.AudioRequest(lossless: false, opusBitrate: 48000))
        await rig.client.setAudioEnabled(true)
        let sent = rig.recorder.sent("audio")
        #expect(!sent.isEmpty)
        #expect(sent.allSatisfy { $0["opusBitrate"] == nil })
    }

    @Test func noBitrateGoesWhenThePhoneAsksForNone() async throws {
        let rig = try Rig()
        await rig.open(minor: 11, capabilities: Self.capabilities())
        try await rig.connect()
        await rig.client.setAudioRequest(MediaControlClient.AudioRequest(lossless: false, opusBitrate: nil))
        await rig.client.setAudioEnabled(true)
        #expect(rig.recorder.sent("audio").last.map { Set($0.keys) }
            == ["op", "connectionId", "revision", "enabled", "profile"])
    }

    /// The catalogue can come after audio is up: the first `audio` then
    /// went without a bitrate. When the bitrate it brings is what the Core
    /// already runs, sending it would only start the stream over (each
    /// accepted `audio` is a new context at the Core), so it waits for the
    /// next `audio`. A bitrate that changes what plays goes at once.
    @Test func aLateCatalogueBitrateTheCoreAlreadyRunsDoesNotStartTheStreamOver() async throws {
        let rig = try Rig()
        await rig.open(minor: 11, capabilities: Self.capabilities())
        try await rig.connect()
        let id = try await rig.connectionId
        await rig.client.setAudioRequest(MediaControlClient.AudioRequest(lossless: false, opusBitrate: nil))
        await rig.client.setAudioEnabled(true)
        await rig.deliver(Self.context(id, revision: try Self.lastRevision(rig), generation: 1, bitrate: 48000))
        await rig.recorder.settle { !Self.audioContexts(rig).isEmpty }
        let before = rig.recorder.sent("audio").count
        await rig.client.setAudioRequest(MediaControlClient.AudioRequest(lossless: false, opusBitrate: 48000))
        #expect(rig.recorder.sent("audio").count == before, "the Core already runs 48000")
        // The next audio carries it.
        await rig.client.setAudioEnabled(false)
        await rig.client.setAudioEnabled(true)
        #expect(rig.recorder.sent("audio").last?["opusBitrate"] == 48000)
        // A bitrate that changes what plays goes at once.
        await rig.deliver(Self.context(id, revision: try Self.lastRevision(rig), generation: 2, bitrate: 48000))
        let changed = rig.recorder.sent("audio").count
        await rig.client.setAudioRequest(MediaControlClient.AudioRequest(lossless: false, opusBitrate: 24000))
        #expect(rig.recorder.sent("audio").count == changed + 1)
        #expect(rig.recorder.sent("audio").last?["opusBitrate"] == 24000)
    }

    /// The catalogue can also come after the first `audio` went but before
    /// the Core answered it: the bitrate waits for that answer, and goes
    /// only if the Core does not already run it.
    @Test(arguments: [(48000, false), (24000, true)])
    func aCatalogueBitrateBeforeTheCoresAnswerWaitsForIt(coreRuns: Int, sent: Bool) async throws {
        let rig = try Rig()
        await rig.open(minor: 11, capabilities: Self.capabilities())
        try await rig.connect()
        let id = try await rig.connectionId
        await rig.client.setAudioRequest(MediaControlClient.AudioRequest(lossless: false, opusBitrate: nil))
        await rig.client.setAudioEnabled(true)
        let before = rig.recorder.sent("audio").count
        let revision = try Self.lastRevision(rig)
        await rig.client.setAudioRequest(MediaControlClient.AudioRequest(lossless: false, opusBitrate: 48000))
        #expect(rig.recorder.sent("audio").count == before, "the Core has not answered")
        await rig.deliver(Self.context(id, revision: revision, generation: 1, bitrate: coreRuns))
        await rig.recorder.settle { rig.recorder.sent("audio").count > before }
        #expect(rig.recorder.sent("audio").count == before + (sent ? 1 : 0))
        if sent {
            #expect(rig.recorder.sent("audio").last?["opusBitrate"] == 48000)
        }
    }

    @Test func aLateCatalogueBitrateWhileLosslessPlaysWaitsForTheFallBack() async throws {
        let rig = try Rig()
        await rig.open(minor: 11, capabilities: Self.capabilities())
        try await rig.connect()
        let id = try await rig.connectionId
        await rig.client.setAudioRequest(MediaControlClient.AudioRequest(lossless: true, opusBitrate: nil))
        await rig.client.setAudioEnabled(true)
        await rig.deliver(Self.context(id, revision: try Self.lastRevision(rig), generation: 1, profile: "lossless"))
        let before = rig.recorder.sent("audio").count
        await rig.client.setAudioRequest(MediaControlClient.AudioRequest(lossless: true, opusBitrate: 48000))
        #expect(rig.recorder.sent("audio").count == before, "lossless plays on; the bitrate is for the Opus after")
        // Opus asked for goes at once; a bitrate after it waits for the
        // Core's answer, and goes when that answer is still lossless.
        await rig.client.setAudioRequest(MediaControlClient.AudioRequest(lossless: false, opusBitrate: nil))
        #expect(rig.recorder.sent("audio").count == before + 1)
        await rig.client.setAudioRequest(MediaControlClient.AudioRequest(lossless: false, opusBitrate: 48000))
        #expect(rig.recorder.sent("audio").count == before + 1)
        await rig.deliver(Self.context(id, revision: try Self.lastRevision(rig), generation: 2, profile: "lossless"))
        await rig.recorder.settle { rig.recorder.sent("audio").count > before + 1 }
        #expect(rig.recorder.sent("audio").count == before + 2)
        #expect(rig.recorder.sent("audio").last?["opusBitrate"] == 48000)
    }

    @Test func aNewChoiceGoesAtOnceAndAgainOnTheNextConnection() async throws {
        let rig = try Rig()
        await rig.open(minor: 11, capabilities: Self.capabilities())
        await rig.client.setAudioRequest(MediaControlClient.AudioRequest(lossless: false, opusBitrate: 24000))
        await rig.client.setAudioEnabled(true)
        #expect(rig.recorder.sent("audio").isEmpty, "nothing goes before media is up")
        try await rig.connect()
        await rig.recorder.settle { !rig.recorder.sent("audio").isEmpty }
        #expect(rig.recorder.sent("audio").last?["opusBitrate"] == 24000)
        let before = rig.recorder.sent("audio").count
        await rig.client.setAudioRequest(MediaControlClient.AudioRequest(lossless: false, opusBitrate: 48000))
        #expect(rig.recorder.sent("audio").count == before + 1)
        #expect(rig.recorder.sent("audio").last?["opusBitrate"] == 48000)
        // The same request again sends nothing new.
        await rig.client.setAudioRequest(MediaControlClient.AudioRequest(lossless: false, opusBitrate: 48000))
        #expect(rig.recorder.sent("audio").count == before + 1)
    }

    @Test func losslessIsAskedForAsTheDesktopAsks() async throws {
        let rig = try Rig()
        await rig.open(minor: 11, capabilities: Self.capabilities())
        try await rig.connect()
        await rig.client.setAudioRequest(MediaControlClient.AudioRequest(lossless: true, opusBitrate: 48000))
        await rig.client.setAudioEnabled(true)
        #expect(rig.recorder.sent("audio").last?["profile"] == "lossless")
        #expect(rig.recorder.sent("audio").last?["opusBitrate"] == 48000,
                "the bitrate rides along for the Opus a refusal leaves")
    }

    // MARK: The Core's answer

    @Test func theCoresRefusalArrivesInItsWordsBesideTheEncoderItRuns() async throws {
        let rig = try Rig()
        await rig.open(minor: 11, capabilities: Self.capabilities())
        try await rig.connect()
        let id = try await rig.connectionId
        await rig.client.setAudioRequest(MediaControlClient.AudioRequest(lossless: false, opusBitrate: 48000))
        await rig.client.setAudioEnabled(true)
        await rig.deliver(Self.context(id, revision: try Self.lastRevision(rig), generation: 1, bitrate: 24000,
                                       bandwidthHz: 8000, bitrateRefusal: Self.refusalWords))
        await rig.recorder.settle { !Self.audioContexts(rig).isEmpty }
        let context = try #require(Self.audioContexts(rig).last)
        #expect(context.opusBitrateRefusal == Self.refusalWords)
        #expect(context.encoder?.targetBitrate == 24000)
        #expect(context.encoder?.audioBandwidthHz == 8000)
    }

    @Test func aRefusalOnAConnectionThatSentNoBitrateIsNotTaken() async throws {
        let rig = try Rig()
        await rig.open(minor: 11, capabilities: Self.capabilities(quality: 0))
        try await rig.connect()
        let id = try await rig.connectionId
        await rig.client.setAudioRequest(MediaControlClient.AudioRequest(lossless: false, opusBitrate: 48000))
        await rig.client.setAudioEnabled(true)
        let revision = try Self.lastRevision(rig)
        await rig.deliver(Self.context(id, revision: revision, generation: 1, bitrateRefusal: Self.refusalWords))
        #expect(rig.playback.anchor == nil)
        await rig.deliver(Self.context(id, revision: revision, generation: 1))
        #expect(rig.playback.anchor != nil)
    }

    @Test func theDecoderTakesTheRefusalOnlyWhereABitrateWasSent() {
        let payload = Self.context("c", revision: 1, generation: 1, bitrateRefusal: Self.refusalWords)
        #expect(MediaControlDecoder.audioContext(payload, detail: true, profile: true, bitrate: true)?
            .opusBitrateRefusal == Self.refusalWords)
        #expect(MediaControlDecoder.audioContext(payload, detail: true, profile: true, bitrate: false) == nil)
        let empty = Self.context("c", revision: 1, generation: 1, bitrateRefusal: "")
        #expect(MediaControlDecoder.audioContext(empty, detail: true, profile: true, bitrate: true) == nil)
        let lossless = Self.context("c", revision: 1, generation: 1, profile: "lossless",
                                    bitrateRefusal: Self.refusalWords)
        #expect(MediaControlDecoder.audioContext(lossless, detail: true, profile: true, bitrate: true)?
            .opusBitrateRefusal == Self.refusalWords, "the Core refuses the bitrate under lossless too")
        let plain = Self.context("c", revision: 1, generation: 1)
        #expect(MediaControlDecoder.audioContext(plain, detail: true, profile: true, bitrate: true)?
            .opusBitrateRefusal == nil)
    }

    // MARK: Lossless playback

    @Test func aLosslessContextPlaysL16AndAnOpusOneOpusAgain() async throws {
        let rig = try Rig()
        await rig.open(minor: 11, capabilities: Self.capabilities())
        try await rig.connect()
        let id = try await rig.connectionId
        await rig.client.setAudioRequest(MediaControlClient.AudioRequest(lossless: true, opusBitrate: 48000))
        await rig.client.setAudioEnabled(true)
        await rig.deliver(Self.context(id, revision: try Self.lastRevision(rig), generation: 1, profile: "lossless"))
        #expect(rig.playback.anchor?.format == .l16)
        #expect(rig.playback.anchor?.ssrc == MediaControlClient.audioSsrc(forConnection: id))
        await rig.client.setAudioRequest(MediaControlClient.AudioRequest(lossless: false, opusBitrate: 48000))
        await rig.deliver(Self.context(id, revision: try Self.lastRevision(rig), generation: 2))
        #expect(rig.playback.anchor?.format == .opus)
    }
}
