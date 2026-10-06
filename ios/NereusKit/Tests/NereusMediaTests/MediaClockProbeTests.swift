// NereusSDR for iOS: the current media owner's bounded Core clock exchange
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkSessionTestSupport
import NereusLink
import Testing
@testable import NereusMedia

@Suite struct MediaClockProbeTests {
    @Test func clockCapabilityUsesTheExistingMediaMinor() {
        let versions: [String: Int64] = ["remoteMediaVersion": 1, "audioClockVersion": 1]
        #expect(MediaFeatureGates(agreedMinor: 1) { versions[$0] ?? 0 }.audioClock)
        #expect(!MediaFeatureGates(agreedMinor: 0) { versions[$0] ?? 0 }.audioClock)
        #expect(!MediaFeatureGates(agreedMinor: 11) {
            $0 == "remoteMediaVersion" ? 1 : 0
        }.audioClock)
    }

    private final class TakenClock: LinkClock, @unchecked Sendable {
        private struct Entry {
            let id: Int
            let action: @Sendable () async -> Void
        }
        private struct Timer: LinkTimer {
            let clock: TakenClock
            let id: Int
            func cancel() { clock.lock.withLock { clock.entries.removeAll { $0.id == id } } }
        }
        private let lock = NSLock()
        private var entries: [Entry] = []
        private var serial = 0
        func schedule(after delay: Duration, _ action: @escaping @Sendable () async -> Void) -> any LinkTimer {
            lock.withLock {
                serial += 1
                entries.append(Entry(id: serial, action: action))
                return Timer(clock: self, id: serial)
            }
        }
        func take() -> (@Sendable () async -> Void)? {
            lock.withLock { entries.isEmpty ? nil : entries.removeFirst().action }
        }
        var count: Int { lock.withLock { entries.count } }
    }

    private struct SendFailure: Error {}
    private actor HeldProbeSends {
        private var suspended: [CheckedContinuation<Void, Never>] = []
        private var arrivals: [(Int, CheckedContinuation<Void, Never>)] = []
        func hold() async {
            await withCheckedContinuation { continuation in
                suspended.append(continuation)
                let ready = arrivals.filter { suspended.count >= $0.0 }
                arrivals.removeAll { suspended.count >= $0.0 }
                ready.forEach { $0.1.resume() }
            }
        }
        func waitFor(_ count: Int) async {
            if suspended.count >= count { return }
            await withCheckedContinuation { arrivals.append((count, $0)) }
        }
        func releaseFirst() { if !suspended.isEmpty { suspended.removeFirst().resume() } }
    }

    private struct Rig {
        let recorder = MediaControlRecorder()
        let timers = ManualLinkClock()
        let lifetime: UInt64 = 41
        let client: MediaControlClient
        let baseNs: Int64

        init(baseNs: Int64 = 20_000_000_000, probeIDSeed: UInt32 = 0) {
            self.baseNs = baseNs
            let recorder = recorder
            let timers = timers
            client = MediaControlClient(send: recorder.sender, peerFactory: recorder.peerFactory,
                                        nanosecondClock: { baseNs + timers.nowMilliseconds * 1_000_000 },
                                        timers: timers, clockProbeIDSeed: probeIDSeed)
            recorder.listen(to: client)
        }

        var nowNs: Int64 { baseNs + timers.nowMilliseconds * 1_000_000 }
        var id: String {
            get async throws { try #require(await client.connectionId) }
        }

        func open(capabilities: [String: Int64] = ["remoteMediaVersion": 1, "audioClockVersion": 1]) async throws {
            await client.handle(.stateChanged(.authenticating))
            await client.handle(.message(.hello(.init(major: 1, minor: 11, settingsSchema: 0,
                                                     peer: "nereusd", majors: [1], features: [:]))))
            await client.handle(.message(.capabilities(MediaControlClientTests.capabilities(capabilities))))
            await client.handle(.message(.snapshotComplete))
            await client.handle(.stateChanged(.ready))
            let connectedBefore = recorder.events.filter { $0 == .mediaState(.connected) }.count
            try #require(recorder.peers.last).become(.connected)
            #expect(await recorder.settle {
                recorder.events.filter { $0 == .mediaState(.connected) }.count > connectedBefore
            })
            await client.setAudioEnabled(true)
            let id = try await id
            await client.handle(.message(.mediaControl(.init(payload: MediaControlClientTests.audioContext(
                id, revision: 1, generation: 7,
                ssrc: MediaControlClient.audioSsrc(forConnection: id))))))
        }

        func activity(revision: UInt64, playing: Bool = true, generation: UInt32 = 7,
                      mediaID: String? = nil, lifetime: UInt64? = nil, epoch: UInt64 = 1,
                      observedNs: Int64? = nil) async throws -> Bool {
            let currentID = try await id
            let value = MediaClockPlaybackActivity(
                mediaID: mediaID ?? currentID, playbackLifetime: lifetime ?? self.lifetime,
                outputEpoch: epoch, revision: revision, generation: generation,
                observedNs: observedNs ?? nowNs, isPlaying: playing)
            return await client.setClockPlaybackActivity(value, owner: 0)
        }

        func echo(_ probe: [String: LinkJSON], generation: LinkJSON = 7,
                  extra: (String, LinkJSON)? = nil) async throws {
            var payload: [String: LinkJSON] = [
                "op": "clock-echo", "connectionId": .string(try await id),
                "id": try #require(probe["id"]), "t0": try #require(probe["t0"]),
                "t1": .number(Double(nowNs - 8_000_000)),
                "t2": .number(Double(nowNs - 7_000_000)),
                "generation": generation, "rtpTimestamp": 1920,
                "capturedNs": .number(Double(nowNs - 20_000_000)),
            ]
            if let extra { payload[extra.0] = extra.1 }
            await client.handle(.message(.mediaControl(.init(payload: payload))))
        }
    }

    @Test func unsupportedAndInactiveMediaSendNoProbe() async throws {
        let withoutMedia = Rig()
        #expect(!(await withoutMedia.client.setClockPlaybackActivity(.init(
            mediaID: UUID().uuidString.lowercased(), playbackLifetime: 1, outputEpoch: 1,
            revision: 1, generation: 7, observedNs: withoutMedia.nowNs,
            isPlaying: true), owner: 0)))
        await withoutMedia.timers.advance(by: 4_000)
        #expect(withoutMedia.recorder.sent("clock-probe").isEmpty)

        let unsupported = Rig()
        try await unsupported.open(capabilities: ["remoteMediaVersion": 1])
        #expect(!(try await unsupported.activity(revision: 1)))
        await unsupported.timers.advance(by: 4_000)
        #expect(unsupported.recorder.sent("clock-probe").isEmpty)
        #expect(!(await unsupported.client.selectedAudioClock(owner: 0).supported))

        let inactive = Rig()
        try await inactive.open()
        await inactive.timers.advance(by: 4_000)
        #expect(inactive.recorder.sent("clock-probe").isEmpty)
        #expect(try await inactive.activity(revision: 1, playing: false))
        await inactive.timers.advance(by: 4_000)
        #expect(inactive.recorder.sent("clock-probe").isEmpty)
    }

    @Test func activeProbeHasExactWireShapeAndFreshEchoProducesMatchingCapture() async throws {
        let rig = Rig()
        try await rig.open()
        #expect(try await rig.activity(revision: 1))
        await rig.timers.advance(by: 1_000)
        let probe = try #require(rig.recorder.sent("clock-probe").first)
        #expect(Set(probe.keys) == ["op", "connectionId", "id", "t0"])
        #expect(probe["connectionId"] == .string(try await rig.id))
        #expect(probe["t0"] == .number(Double(rig.nowNs)))
        await rig.timers.advance(by: 10)
        try await rig.echo(probe)
        let reading = await rig.client.selectedAudioClock(owner: 0)
        #expect(reading.supported && reading.active)
        #expect(reading.playingGeneration == 7)
        #expect(reading.offset != nil)
        #expect(reading.capture?.generation == 7)
        #expect(reading.lastEchoNs == rig.nowNs)
    }

    @Test func pendingIsBoundedAndMatchingRequiresExactIdTimeAndFields() async throws {
        let rig = Rig()
        try await rig.open()
        #expect(try await rig.activity(revision: 1))
        for revision in 2...11 {
            await rig.timers.advance(by: 1_000)
            #expect(try await rig.activity(revision: UInt64(revision)))
        }
        let probes = rig.recorder.sent("clock-probe")
        #expect(probes.count == 10)
        #expect(await rig.client.pendingClockProbeCount == 8)
        try await rig.echo(probes[0])
        #expect(await rig.client.selectedAudioClock(owner: 0).offset == nil)
        var wrong = probes[9]
        wrong["t0"] = .number(1)
        try await rig.echo(wrong)
        #expect(await rig.client.selectedAudioClock(owner: 0).offset == nil)
        try await rig.echo(probes[9], extra: ("unexpected", 1))
        #expect(await rig.client.selectedAudioClock(owner: 0).offset == nil)
        await rig.timers.advance(by: 10)
        try await rig.echo(probes[9])
        #expect(await rig.client.selectedAudioClock(owner: 0).offset != nil)
        #expect(await rig.client.pendingClockProbeCount == 7)
    }

    @Test func revisionLifetimeAndFreshnessWithdrawProbes() async throws {
        let rig = Rig()
        try await rig.open()
        #expect(try await rig.activity(revision: 1))
        #expect(try await rig.activity(revision: 2, playing: false))
        #expect(!(try await rig.activity(revision: 1)))
        #expect(!(try await rig.activity(revision: 2)))
        await rig.timers.advance(by: 1_000)
        #expect(rig.recorder.sent("clock-probe").isEmpty)
        #expect(try await rig.activity(revision: 3, lifetime: 42, epoch: 1))
        await rig.timers.advance(by: 1_000)
        #expect(rig.recorder.sent("clock-probe").count == 1)
        #expect(try await rig.activity(revision: 4, playing: false, lifetime: 42, epoch: 2))
        #expect(!(try await rig.activity(revision: 3, lifetime: rig.lifetime, epoch: 1)))
        #expect(try await rig.activity(revision: 5, observedNs: rig.nowNs - 3_000_000_001) == false)
        #expect(try await rig.activity(revision: 6, observedNs: rig.nowNs + 1) == false)
        #expect(try await rig.activity(revision: 7))
        await rig.timers.advance(by: 4_000)
        #expect(rig.recorder.sent("clock-probe").count == 4)
        #expect(!(await rig.client.selectedAudioClock(owner: 0).active))
        #expect(await rig.client.pendingClockProbeCount == 0)
    }

    @Test func malformedOldAndDuplicateEchoesCannotChangeEstimate() async throws {
        let rig = Rig()
        try await rig.open()
        #expect(try await rig.activity(revision: 1))
        await rig.timers.advance(by: 1_010)
        let probe = try #require(rig.recorder.sent("clock-probe").first)
        let id = try await rig.id
        let valid: [String: LinkJSON] = [
            "op": "clock-echo", "connectionId": .string(id),
            "id": try #require(probe["id"]), "t0": try #require(probe["t0"]),
            "t1": .number(Double(rig.nowNs - 8_000_000)),
            "t2": .number(Double(rig.nowNs - 7_000_000)),
            "generation": 7, "rtpTimestamp": 1920,
            "capturedNs": .number(Double(rig.nowNs - 20_000_000)),
        ]
        for (key, bad): (String, LinkJSON) in [
            ("id", .number(-1)), ("id", .number(Double(UInt32.max) + 1)),
            ("id", .bool(true)), ("t0", .string("1")),
            ("t1", .number(1.5)), ("t2", .number(.infinity)),
            ("generation", .number(-1)), ("rtpTimestamp", .number(1.5)),
            ("capturedNs", .number(-1)),
        ] {
            var malformed = valid
            malformed[key] = bad
            await rig.client.handle(.message(.mediaControl(.init(payload: malformed))))
            #expect(await rig.client.pendingClockProbeCount == 1)
        }
        var unknown = valid
        unknown["extra"] = 1
        await rig.client.handle(.message(.mediaControl(.init(payload: unknown))))
        var missing = valid
        missing.removeValue(forKey: "t2")
        await rig.client.handle(.message(.mediaControl(.init(payload: missing))))
        var oldConnection = valid
        oldConnection["connectionId"] = .string(UUID().uuidString.lowercased())
        await rig.client.handle(.message(.mediaControl(.init(payload: oldConnection))))
        #expect(await rig.client.pendingClockProbeCount == 1)
        await rig.client.handle(.message(.mediaControl(.init(payload: valid))))
        let accepted = await rig.client.selectedAudioClock(owner: 0)
        #expect(accepted.offset != nil)
        #expect(await rig.client.pendingClockProbeCount == 0)
        await rig.timers.advance(by: 20)
        await rig.client.handle(.message(.mediaControl(.init(payload: valid))))
        #expect(await rig.client.selectedAudioClock(owner: 0).lastEchoNs == accepted.lastEchoNs)
    }

    @Test func generationZeroClearsCaptureAndNewContextWaitsForItsEcho() async throws {
        let rig = Rig()
        try await rig.open()
        #expect(try await rig.activity(revision: 1))
        await rig.timers.advance(by: 1_010)
        let first = try #require(rig.recorder.sent("clock-probe").last)
        try await rig.echo(first)
        #expect(await rig.client.selectedAudioClock(owner: 0).capture?.generation == 7)
        await rig.timers.advance(by: 1_000)
        let second = try #require(rig.recorder.sent("clock-probe").last)
        try await rig.echo(second, generation: 0)
        #expect(await rig.client.selectedAudioClock(owner: 0).capture == nil)
        let id = try await rig.id
        await rig.client.setAudioEnabled(true)
        await rig.client.handle(.message(.mediaControl(.init(payload: MediaControlClientTests.audioContext(
            id, revision: 2, generation: 8, ssrc: MediaControlClient.audioSsrc(forConnection: id))))))
        #expect(await rig.client.selectedAudioClock(owner: 0).capture == nil)
        #expect(await rig.client.selectedAudioClock(owner: 0).offset == nil)
        #expect(try await rig.activity(revision: 2, generation: 8))
        await rig.timers.advance(by: 1_010)
        let third = try #require(rig.recorder.sent("clock-probe").last)
        try await rig.echo(third, generation: 8)
        #expect(await rig.client.selectedAudioClock(owner: 0).capture?.generation == 8)
    }

    @Test func latestEchoAndActivityHaveIndependentThreeSecondFreshness() async throws {
        let rig = Rig()
        try await rig.open()
        #expect(try await rig.activity(revision: 1))
        await rig.timers.advance(by: 1_010)
        try await rig.echo(try #require(rig.recorder.sent("clock-probe").last))
        #expect(await rig.client.selectedAudioClock(owner: 0).offset != nil)
        for revision in 2...5 {
            #expect(try await rig.activity(revision: UInt64(revision)))
            await rig.timers.advance(by: 1_000)
        }
        let reading = await rig.client.selectedAudioClock(owner: 0)
        #expect(reading.active)
        #expect(reading.offset == nil)
        #expect(reading.lastEchoNs != nil)
    }

    @Test func changingPlaybackLifetimeOrOutputEpochClearsOldClockContext() async throws {
        let rig = Rig()
        try await rig.open()
        #expect(try await rig.activity(revision: 1))
        await rig.timers.advance(by: 1_010)
        let first = try #require(rig.recorder.sent("clock-probe").last)
        try await rig.echo(first)
        #expect(await rig.client.selectedAudioClock(owner: 0).offset != nil)
        #expect(try await rig.activity(revision: 2, lifetime: 42))
        #expect(await rig.client.selectedAudioClock(owner: 0).playbackLifetime == 42)
        #expect(await rig.client.selectedAudioClock(owner: 0).offset == nil)
        #expect(await rig.client.selectedAudioClock(owner: 0).capture == nil)
        try await rig.echo(first)
        #expect(await rig.client.selectedAudioClock(owner: 0).offset == nil)
        #expect(try await rig.activity(revision: 3, lifetime: 42, epoch: 2))
        #expect(await rig.client.selectedAudioClock(owner: 0).outputEpoch == 2)
        #expect(await rig.client.pendingClockProbeCount == 0)
    }

    @Test func capabilityWithdrawalClearsOutstandingProbesAndEstimate() async throws {
        let rig = Rig()
        try await rig.open()
        #expect(try await rig.activity(revision: 1))
        await rig.timers.advance(by: 1_010)
        try await rig.echo(try #require(rig.recorder.sent("clock-probe").last))
        #expect(await rig.client.selectedAudioClock(owner: 0).offset != nil)
        await rig.client.handle(.message(.capabilities(MediaControlClientTests.capabilities(
            ["remoteMediaVersion": 1]))))
        let after = await rig.client.selectedAudioClock(owner: 0)
        #expect(!after.supported && !after.active)
        #expect(after.offset == nil && after.capture == nil)
        #expect(await rig.client.pendingClockProbeCount == 0)
        #expect(!(try await rig.activity(revision: 2)))
        await rig.timers.advance(by: 2_000)
        #expect(rig.recorder.sent("clock-probe").count == 1)
    }

    @Test func impossibleArithmeticConsumesOnlyItsOwnEchoAndKeepsEstimatorClean() async throws {
        let rig = Rig()
        try await rig.open()
        #expect(try await rig.activity(revision: 1))
        await rig.timers.advance(by: 1_010)
        let first = try #require(rig.recorder.sent("clock-probe").last)
        var impossible: [String: LinkJSON] = [
            "op": "clock-echo", "connectionId": .string(try await rig.id),
            "id": try #require(first["id"]), "t0": try #require(first["t0"]),
            "t1": .number(Double(rig.nowNs - 8_000_000)),
            "t2": .number(Double(rig.nowNs + 20_000_000)),
            "generation": 7, "rtpTimestamp": 1920, "capturedNs": 1,
        ]
        await rig.client.handle(.message(.mediaControl(.init(payload: impossible))))
        #expect(await rig.client.pendingClockProbeCount == 0)
        #expect(await rig.client.selectedAudioClock(owner: 0).offset == nil)
        impossible["t2"] = .number(Double(rig.nowNs - 7_000_000))
        await rig.client.handle(.message(.mediaControl(.init(payload: impossible))))
        #expect(await rig.client.selectedAudioClock(owner: 0).offset == nil)
        await rig.timers.advance(by: 1_000)
        await rig.timers.advance(by: 10)
        try await rig.echo(try #require(rig.recorder.sent("clock-probe").last))
        #expect(await rig.client.selectedAudioClock(owner: 0).offset != nil)
    }

    @Test func probeIdWrapsToZeroAndWireTimeMatchesLongUptimeQuantization() async throws {
        let rig = Rig(baseNs: 9_007_199_254_740_993, probeIDSeed: UInt32.max)
        try await rig.open()
        #expect(try await rig.activity(revision: 1))
        await rig.timers.advance(by: 1_000)
        let probe = try #require(rig.recorder.sent("clock-probe").first)
        #expect(probe["id"] == 0)
        #expect(probe["t0"] == .number(Double(rig.nowNs)))
        await rig.timers.advance(by: 10)
        try await rig.echo(probe)
        #expect(await rig.client.selectedAudioClock(owner: 0).offset != nil)

        let limit = Rig(baseNs: Int64.max - 1_000_000_001)
        try await limit.open()
        #expect(try await limit.activity(revision: 1))
        await limit.timers.advance(by: 1_000)
        #expect(limit.recorder.sent("clock-probe").isEmpty)
        #expect(!(await limit.client.selectedAudioClock(owner: 0).active))
    }

    @Test func takenOldTimerCannotSendOrRearmAfterMediaRetirement() async throws {
        let recorder = MediaControlRecorder()
        let timer = TakenClock()
        let client = MediaControlClient(send: recorder.sender, peerFactory: recorder.peerFactory,
                                        nanosecondClock: { 20_000_000_000 }, timers: timer)
        recorder.listen(to: client)
        func open(revision: UInt32) async throws -> String {
            await client.handle(.stateChanged(.authenticating))
            await client.handle(.message(.hello(.init(major: 1, minor: 11, settingsSchema: 0,
                                                     peer: "nereusd", majors: [1], features: [:]))))
            await client.handle(.message(.capabilities(MediaControlClientTests.capabilities(
                ["remoteMediaVersion": 1, "audioClockVersion": 1]))))
            await client.handle(.message(.snapshotComplete))
            await client.handle(.stateChanged(.ready))
            let connectedBefore = recorder.events.filter { $0 == .mediaState(.connected) }.count
            try #require(recorder.peers.last).become(.connected)
            #expect(await recorder.settle {
                recorder.events.filter { $0 == .mediaState(.connected) }.count > connectedBefore
            })
            if revision == 1 {
                await client.setAudioEnabled(true)
            } else {
                await recorder.waitUntilSent("audio", count: 2)
            }
            let id = try #require(await client.connectionId)
            await client.handle(.message(.mediaControl(.init(payload: MediaControlClientTests.audioContext(
                id, revision: revision, generation: 7,
                ssrc: MediaControlClient.audioSsrc(forConnection: id))))))
            return id
        }
        let old = try await open(revision: 1)
        #expect(await client.setClockPlaybackActivity(.init(
            mediaID: old, playbackLifetime: 1, outputEpoch: 1, revision: 1,
            generation: 7, observedNs: 20_000_000_000, isPlaying: true), owner: 0))
        let taken = try #require(timer.take())
        await client.handle(.stateChanged(.stopped))
        let new = try await open(revision: 2)
        #expect(new != old)
        #expect(await client.setClockPlaybackActivity(.init(
            mediaID: new, playbackLifetime: 2, outputEpoch: 1, revision: 2,
            generation: 7, observedNs: 20_000_000_000, isPlaying: true), owner: 0))
        #expect(timer.count == 1)
        await taken()
        #expect(recorder.sent("clock-probe").isEmpty)
        #expect(timer.count == 1)
    }

    @Test func oldLogicalOwnerCannotPublishActivityOrObserveNewClock() async throws {
        let recorder = MediaControlRecorder()
        let timers = ManualLinkClock()
        let client = MediaControlClient(send: recorder.sender, peerFactory: recorder.peerFactory,
                                        nanosecondClock: { 20_000_000_000 }, timers: timers)
        recorder.listen(to: client)
        let station = ScriptedStation()
        func session() -> StationSession {
            StationSession(endpoint: StationEndpoint(host: "shack.example.net"), trust: station.trust,
                           authenticator: TokenAuthenticator(token: "conformance-token"),
                           clock: ManualLinkClock(), transportFactory: station.factory)
        }
        func open(owner: UInt64, revision: UInt32) async throws -> String {
            #expect(await client.activateLogicalSession(session(), owner: owner))
            await client.handle(.stateChanged(.authenticating), owner: owner)
            await client.handle(.message(.hello(.init(major: 1, minor: 11, settingsSchema: 0,
                                                    peer: "nereusd", majors: [1], features: [:]))), owner: owner)
            await client.handle(.message(.capabilities(MediaControlClientTests.capabilities(
                ["remoteMediaVersion": 1, "audioClockVersion": 1]))), owner: owner)
            await client.handle(.message(.snapshotComplete), owner: owner)
            let before = recorder.events.filter { $0 == .mediaState(.connected) }.count
            try #require(recorder.peers.last).become(.connected)
            #expect(await recorder.settle {
                recorder.events.filter { $0 == .mediaState(.connected) }.count > before
            })
            await client.setAudioEnabled(true)
            let id = try #require(await client.connectionId)
            await client.handle(.message(.mediaControl(.init(payload: MediaControlClientTests.audioContext(
                id, revision: revision, generation: 7,
                ssrc: MediaControlClient.audioSsrc(forConnection: id))))), owner: owner)
            return id
        }
        let oldID = try await open(owner: 1, revision: 1)
        #expect(await client.setClockPlaybackActivity(.init(
            mediaID: oldID, playbackLifetime: 1, outputEpoch: 1, revision: 1,
            generation: 7, observedNs: 20_000_000_000, isPlaying: true), owner: 1))
        let newID = try await open(owner: 2, revision: 3)
        #expect(!(await client.setClockPlaybackActivity(.init(
            mediaID: oldID, playbackLifetime: 1, outputEpoch: 1, revision: 2,
            generation: 7, observedNs: 20_000_000_000, isPlaying: true), owner: 1)))
        #expect(await client.selectedAudioClock(owner: 1).mediaID == nil)
        #expect(await client.setClockPlaybackActivity(.init(
            mediaID: newID, playbackLifetime: 2, outputEpoch: 1, revision: 1,
            generation: 7, observedNs: 20_000_000_000, isPlaying: true), owner: 2))
        #expect(await client.selectedAudioClock(owner: 2).active)
    }

    @Test func oldFailedSendCannotRemoveNewPendingAfterReplacement() async throws {
        let recorder = MediaControlRecorder()
        let timers = ManualLinkClock()
        let held = HeldProbeSends()
        let client = MediaControlClient(send: { message in
            if case .mediaControl(let control) = message,
               control.payload["op"] == .string("clock-probe") {
                await held.hold()
                throw SendFailure()
            }
            try await recorder.sender(message)
        }, peerFactory: recorder.peerFactory,
        nanosecondClock: { 20_000_000_000 + timers.nowMilliseconds * 1_000_000 }, timers: timers)
        recorder.listen(to: client)
        await client.handle(.stateChanged(.authenticating))
        await client.handle(.message(.hello(.init(major: 1, minor: 11, settingsSchema: 0,
                                                 peer: "nereusd", majors: [1], features: [:]))))
        await client.handle(.message(.capabilities(MediaControlClientTests.capabilities(
            ["remoteMediaVersion": 1, "audioClockVersion": 1, "mediaReplaceVersion": 1]))))
        await client.handle(.message(.snapshotComplete))
        await client.handle(.stateChanged(.ready))
        try #require(recorder.peers.last).become(.connected)
        #expect(await recorder.settle { recorder.events.contains(.mediaState(.connected)) })
        await client.setAudioEnabled(true)
        let old = try #require(await client.connectionId)
        await client.handle(.message(.mediaControl(.init(payload: MediaControlClientTests.audioContext(
            old, revision: 1, generation: 7, ssrc: MediaControlClient.audioSsrc(forConnection: old))))))
        #expect(await client.setClockPlaybackActivity(.init(
            mediaID: old, playbackLifetime: 1, outputEpoch: 1, revision: 1,
            generation: 7, observedNs: 20_000_000_000, isPlaying: true), owner: 0))
        let first = Task { await timers.advance(by: 1_000) }
        await held.waitFor(1)
        #expect(await client.pendingClockProbeCount == 1)
        await client.controlRouteDidMove(peerFactory: recorder.peerFactory, connectDeadline: .seconds(5),
                                         safeToReplace: { true })
        let new = try #require(recorder.peers.last?.preparedConnectionId?.uuidString.lowercased())
        await client.handle(.message(.mediaControl(.init(payload: [
            "op": "replace", "connectionId": .string(new), "replaces": .string(old),
        ]))))
        #expect(await client.pendingClockProbeCount == 0)
        #expect(await client.setClockPlaybackActivity(.init(
            mediaID: new, playbackLifetime: 2, outputEpoch: 1, revision: 2,
            generation: 7, observedNs: 21_000_000_000, isPlaying: true), owner: 0))
        let second = Task { await timers.advance(by: 1_000) }
        await held.waitFor(2)
        #expect(await client.pendingClockProbeCount == 1)
        await client.handle(.message(.mediaControl(.init(payload: [
            "op": "clock-echo", "connectionId": .string(old), "id": 1,
            "t0": 21_000_000_000, "t1": 21_001_000_000, "t2": 21_002_000_000,
            "generation": 7, "rtpTimestamp": 1920, "capturedNs": 20_000_000_000,
        ]))))
        #expect(await client.pendingClockProbeCount == 1)
        #expect(await client.selectedAudioClock(owner: 0).offset == nil)
        await held.releaseFirst()
        await first.value
        #expect(await client.pendingClockProbeCount == 1)
        await held.releaseFirst()
        await second.value
        #expect(await client.pendingClockProbeCount == 0)
    }
}
