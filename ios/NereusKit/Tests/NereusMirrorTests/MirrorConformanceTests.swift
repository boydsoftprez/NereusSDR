// NereusSDR for iOS: the link's session fixtures, played against the mirror, the settings proxy and commands
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkSessionTestSupport
import LinkTestSupport
import NereusLink
import Testing
@testable import NereusMirror

/// Link document section 16.3, an app's runner with the layers above the
/// session as the client: every behaviour write goes through
/// `MirrorStore.write`, every setting through `SettingsProxyClient`, every
/// command through `CommandClient`, and after each station message the
/// mirror, the capabilities and the settings cache must hold what the
/// Core's messages so far say.
@Suite struct MirrorConformanceTests {
    // Keep the existing player's producer identity opt-in bounded to these transcripts.
    private static let diversityProducerSessions = ["session-diversity-control-pattern", "session-diversity-control-no-pattern"]
    @Test func everyAppFixturePassesThroughTheMirror() async throws {
        let classes = try SessionFixtures.mirrorClasses()
        let fixtures = try SessionFixtures.forApp()
        #expect(fixtures.count == 85)
        #expect(Set(fixtures.map(\.id)).count == fixtures.count)
        for id in Self.diversityProducerSessions {
            #expect(fixtures.contains { $0.id == id }, "\(id)")
        }
        #expect(!fixtures.contains { $0.id == "session-radio-mic-source" })
        // MON's route on the media connection (monitor-audio, trunk 4582efc90):
        // media.control passes through the mirror's client untouched.
        #expect(fixtures.contains { $0.id == "session-monitor-audio" })
        // A second ADC's attenuation per slice, now the app declares adcAttenuators 1.
        #expect(fixtures.contains { $0.id == "session-adc-attenuators" })
        // The whole CFC profile through cfc.setProfile, now the app declares cfcProfile 1.
        #expect(fixtures.contains { $0.id == "session-cfc-set-profile" })
        // The read-only TX EQ curve, now the app declares txEqCurve, and its
        // change through txEq.setCurve, now it declares txEqCurve 2.
        #expect(fixtures.contains { $0.id == "session-tx-eq-curve" })
        #expect(fixtures.contains { $0.id == "session-tx-eq-set-curve" })
        // The settings check and a version-1 Setup description, now the app declares them.
        for id in ["session-verbs-settings-hygiene", "session-settings-hygiene-token", "session-verbs-two-tone-preset"] {
            #expect(fixtures.contains { $0.id == id }, "\(id)")
        }
        #expect(fixtures.contains { $0.id == "session-verbs-band-select" })
        // The Core's own TCI server settings, now the app declares stationTciSettings 1.
        #expect(fixtures.contains { $0.id == "session-verbs-station-tci-settings" })
        // The band plan the Core follows, record streams with spots, and the
        // Core's radio from a device came with the trunk at 4f8ec22c.
        for id in ["session-settings-band-plan", "session-verbs-records", "session-verbs-spots",
                   "session-verbs-station-radios", "session-station-radio-confirm"] {
            #expect(fixtures.contains { $0.id == id }, "\(id)")
        }
        #expect(fixtures.contains { $0.id == "session-verbs-notch-at-slice" })
        // The HL2 I/O board (radioHardwareVersion 7) and the DSP facts
        // (dspInfoVersion 1) came with the trunk's seventh stable point.
        #expect(fixtures.contains { $0.id == "session-verbs-io-board" })
        #expect(fixtures.contains { $0.id == "session-verbs-dsp-info" })
        for _ in try SessionFixtures.linkMajors() {
            for fixture in fixtures {
                let client = await MirrorFixtureClient()
                let failures = try await SessionFixturePlayer(fixture: fixture, mirrorClasses: classes,
                                                              client: client.fixtureClient,
                    producerDeviceIdentityReferences: Self.diversityProducerSessions.contains(fixture.id)).play()
                for failure in failures {
                    Issue.record("\(fixture.id): \(failure)")
                }
            }
        }
    }

    /// The fixtures reach every part of the mirror at least once.
    @Test func theFixturesExerciseWritesSettingsAndCommands() async throws {
        let classes = try SessionFixtures.mirrorClasses()
        var writes = 0
        var settings = 0
        var commands = 0
        var phases = 0
        for fixture in try SessionFixtures.forApp() {
            let client = await MirrorFixtureClient()
            _ = try await SessionFixturePlayer(fixture: fixture, mirrorClasses: classes,
                                               client: client.fixtureClient,
                    producerDeviceIdentityReferences: Self.diversityProducerSessions.contains(fixture.id)).play()
            let counts = await client.counts
            writes += counts.writes
            settings += counts.settings
            commands += counts.commands
            phases += counts.phases
        }
        #expect(writes >= 1)
        #expect(settings >= 2)
        #expect(commands >= 50)
        #expect(phases >= 2)
    }

    /// The runner checks the mirror only once the client has handled every
    /// event the session produced for a station message, however late those
    /// events reach it: here each one is held up on its way to the client.
    @Test func aClientSlowToHandleEventsStillPasses() async throws {
        let classes = try SessionFixtures.mirrorClasses()
        let ids = ["session-verbs-four-o3a", "session-property-write", "session-settings-write",
                   "session-unknown-kind", "session-wrong-token"]
        let fixtures = try SessionFixtures.forApp().filter { ids.contains($0.id) }
        #expect(fixtures.count == ids.count)
        for fixture in fixtures {
            let client = await MirrorFixtureClient(eventDelay: .milliseconds(10))
            let failures = try await SessionFixturePlayer(fixture: fixture, mirrorClasses: classes,
                                                          client: client.fixtureClient).play()
            for failure in failures {
                Issue.record("\(fixture.id): \(failure)")
            }
        }
    }

    /// The runner notices when the mirror differs from the Core's messages.
    @Test func aMirrorThatDiffersFails() async throws {
        let fixture = try #require(try SessionFixtures.forApp().first { $0.id == "session-verbs-slices" })
        let client = await MirrorFixtureClient()
        await client.corruptOnSnapshotComplete()
        let failures = try await SessionFixturePlayer(fixture: fixture, mirrorClasses: SessionFixtures.mirrorClasses(),
                                                      client: client.fixtureClient).play()
        #expect(failures.contains { $0.contains("slice:0") }, "\(failures)")
    }
}

/// The client under test: a store, a settings proxy and a command client
/// fed by the session's events, with an independent reading of the Core's
/// messages to hold them to.
@MainActor
final class MirrorFixtureClient {
    let store: MirrorStore
    let settings: SettingsProxyClient
    let commands: CommandClient
    /// The command client's own clock; nothing moves it unless a command
    /// is left without an answer.
    private let commandClock = ManualLinkClock()
    private let session = SessionBox()
    /// How long each session event is held up before the client handles
    /// it, to stand in for a busy machine.
    private let eventDelay: Duration?

    // What the Core's messages say, read independently.
    private var expectedKeys: [String] = []
    private var expectedObjects: [String: (className: String, values: [String: MirrorValue])] = [:]
    private var expectedCapabilities: [String: MirrorValue] = [:]
    private var expectedSettings: [String: String] = [:]
    private var settingsSnapshotReplaces = false
    private var snapshotSeen = false
    /// The Core ended the session; from then the mirror may turn stale.
    private var ended = false
    private var corrupt = false

    // What the client was told to do, in order, and what it answered.
    private var writeAnswers: [Box<PropertyWriteOutcome>] = []
    private var expectedWriteAnswers: [UInt32: PropertyWriteOutcome] = [:]
    private var settingsAnswers: [(key: String, answer: Box<SettingsWriteOutcome>)] = []
    private var expectedSettingsAnswers: [SettingsWriteOutcome] = []
    private var commandAnswers: [Box<Result<CommandResult, any Error>>] = []
    private var expectedCommandAnswers: [UInt32: CommandResult] = [:]
    private(set) var counts = (writes: 0, settings: 0, commands: 0, phases: 0)

    init(eventDelay: Duration? = nil) {
        self.eventDelay = eventDelay
        let session = self.session
        let sender: MirrorStore.Sender = { message in
            guard let live = session.value else {
                throw LinkSendError.notConnected
            }
            try await live.send(message)
        }
        store = MirrorStore(send: sender)
        settings = SettingsProxyClient(origin: "mirror-conformance", send: sender)
        commands = CommandClient(clock: commandClock, send: sender)
    }

    func corruptOnSnapshotComplete() {
        corrupt = true
    }

    nonisolated var fixtureClient: SessionFixtureClient {
        SessionFixtureClient(
            forward: { [self] event in await forward(event) },
            drive: { [self] session, message, label in try await drive(session, message, label) },
            stationMessageHandled: { [self] message, label in await check(after: message, label) },
            closed: { [self] label in await closed(label) },
            finished: { [self] in await finished() })
    }

    // MARK: Feeding

    private func forward(_ event: StationSession.Event) async {
        if let eventDelay {
            try? await Task.sleep(for: eventDelay)
        }
        store.handle(event)
        settings.handle(event)
        await commands.handle(event)
    }

    // MARK: Driving

    private func drive(_ live: StationSession, _ message: LinkMessage, _ label: String) async throws {
        session.value = live
        switch message {
        case .propertyWrite(let write):
            guard let entry = write.properties.first, write.properties.count == 1 else {
                throw LinkFixtureLoader.Malformed(description: "\(label): a write of other than one property")
            }
            counts.writes += 1
            let box = Box<PropertyWriteOutcome>()
            writeAnswers.append(box)
            Task { @MainActor in
                box.value = await store.write(write.key, property: entry.name, value: MirrorValue(entry.value))
            }
        case .settingsWrite(let write):
            guard let entry = write.properties.first, case .utf8(let text) = entry.value else {
                throw LinkFixtureLoader.Malformed(description: "\(label): a setting without a text value")
            }
            counts.settings += 1
            expectedSettings[write.key] = text
            let box = Box<SettingsWriteOutcome>()
            settingsAnswers.append((write.key, box))
            Task { @MainActor in
                box.value = await settings.write(write.key, text)
            }
        case .settingsRemove(let remove):
            counts.settings += 1
            expectedSettings[remove.key] = nil
            Task { @MainActor in
                await settings.remove(remove.key)
            }
        case .commandInvoke(let invoke):
            counts.commands += 1
            let arguments = invoke.args.map { CommandArgument(name: $0.name, value: MirrorValue($0.value)) }
            let box = Box<Result<CommandResult, any Error>>()
            commandAnswers.append(box)
            let commands = self.commands
            Task { @MainActor in
                do {
                    box.value = .success(try await commands.invoke(invoke.verb, arguments: arguments,
                                                                   timeout: .seconds(30)))
                } catch {
                    box.value = .failure(error)
                }
            }
        default:
            // Nothing above the session sends anything else.
            try await live.send(message)
        }
    }

    // MARK: Reading the Core's messages

    private func check(after message: LinkMessage?, _ label: String) async -> [String] {
        guard let message else {
            return []
        }
        switch message {
        case .authResult(let result):
            if result.accepted {
                settingsSnapshotReplaces = true
            }
        case .capabilities(let set):
            expectedCapabilities = [:]
            for entry in set.properties {
                expectedCapabilities[entry.name] = MirrorValue(entry.value)
            }
        case .objectCreate(let create):
            if expectedObjects[create.key] == nil {
                expectedKeys.append(create.key)
            }
            expectedObjects[create.key] = (create.className, Self.values(create.properties))
        case .objectDestroy(let destroy):
            expectedObjects[destroy.key] = nil
            expectedKeys.removeAll { $0 == destroy.key }
        case .delta(let delta):
            expectedObjects[delta.key]?.values.merge(Self.values(delta.properties)) { _, new in new }
        case .propertyResult(let result) where result.writeId < 1000:
            for entry in result.results {
                if let kept = entry.value {
                    expectedObjects[result.key]?.values[entry.property] = MirrorValue(kept.value)
                }
                expectedWriteAnswers[result.writeId] = PropertyWriteOutcome(
                    accepted: entry.accepted, reason: entry.reason, value: entry.value.map { MirrorValue($0.value) })
            }
        case .settingsSnapshot(let snapshot):
            if settingsSnapshotReplaces {
                expectedSettings = [:]
                settingsSnapshotReplaces = false
            }
            for entry in snapshot.properties {
                if case .utf8(let text) = entry.value {
                    expectedSettings[entry.name] = text
                }
            }
        case .settingsValue(let change):
            if change.origin == settings.origin {
                expectedSettingsAnswers.append(.accepted)
            }
            expectedSettings[change.key] = Self.text(change.properties)
        case .settingsReject(let reject):
            if settingsAnswers.contains(where: { $0.key == reject.key }) {
                expectedSettingsAnswers.append(.rejected(reason: reject.reason ?? ""))
            }
            expectedSettings[reject.key] = Self.text(reject.properties)
        case .commandResult(let wire) where wire.id < 1000:
            let result = CommandResult(wire)
            if result.isFinal {
                expectedCommandAnswers[wire.id] = result
            } else {
                counts.phases += 1
            }
        case .sessionEnd:
            ended = true
        case .snapshotComplete:
            snapshotSeen = true
            if corrupt {
                store.object("slice:0")?.merge(["frequency": .double(-1)])
            }
        default:
            break
        }
        return compare(label)
    }

    /// The mirror, the capabilities and the settings cache against the
    /// Core's messages so far.
    private func compare(_ label: String) -> [String] {
        var failures: [String] = []
        if store.objectKeys != expectedKeys {
            failures.append("the mirror holds \(store.objectKeys), the Core created \(expectedKeys)")
        }
        for key in expectedKeys {
            guard let expected = expectedObjects[key], let object = store.object(key) else {
                failures.append("the mirror has no \(key)")
                continue
            }
            if object.className != expected.className {
                failures.append("\(key) is a \(object.className), not a \(expected.className)")
            }
            if object.values != expected.values {
                let differing = Set(object.values.keys).union(expected.values.keys)
                    .filter { object.values[$0] != expected.values[$0] }.sorted()
                failures.append("\(key) differs from the Core's messages in \(differing)")
            }
        }
        if store.capabilities != expectedCapabilities {
            failures.append("the capabilities differ from the last set the Core sent")
        }
        if settings.values != expectedSettings {
            failures.append("the settings cache holds \(settings.values), the Core's messages say \(expectedSettings)")
        }
        if snapshotSeen && !ended && (!store.isSnapshotComplete || store.isStale) {
            failures.append("after snapshot.complete the mirror is not current")
        }
        return failures
    }

    private func closed(_ label: String) async -> [String] {
        await settle()
        // The runner calls this once the client has handled the session's
        // report of the lost link, which comes after its refusal.
        var failures: [String] = []
        if store.isStale != snapshotSeen {
            failures.append("after the close the mirror's staleness is \(store.isStale)")
        }
        if await commands.waitingCount != 0 {
            failures.append("a command is still waiting after the close")
        }
        return failures
    }

    private func finished() async -> [String] {
        await settle()
        var failures: [String] = []
        for (index, box) in writeAnswers.enumerated() {
            let writeId = UInt32(index + 1)
            guard let answer = box.value else {
                failures.append("write \(writeId) has no answer")
                continue
            }
            if let expected = expectedWriteAnswers[writeId], answer != expected {
                failures.append("write \(writeId) answered \(answer), the Core said \(expected)")
            }
        }
        let settled = settingsAnswers.compactMap(\.answer.value)
        if settled != expectedSettingsAnswers {
            failures.append("the settings writes answered \(settled), the Core said \(expectedSettingsAnswers)")
        }
        for (index, box) in commandAnswers.enumerated() {
            let id = UInt32(index + 1)
            guard let answer = box.value else {
                failures.append("command \(id) has no final answer")
                continue
            }
            switch answer {
            case .success(let result):
                if result != expectedCommandAnswers[id] {
                    failures.append("command \(id) answered \(result), the Core said \(String(describing: expectedCommandAnswers[id]))")
                }
            case .failure(let error):
                failures.append("command \(id) failed: \(error)")
            }
        }
        if await commands.waitingCount != 0 {
            failures.append("a command is still waiting")
        }
        return failures
    }

    /// Lets the tasks the client started finish what they can.
    private func settle() async {
        for _ in 0..<2_000 {
            let pending = writeAnswers.contains { $0.value == nil }
                || settingsAnswers.contains { $0.answer.value == nil }
                || commandAnswers.contains { $0.value == nil }
            if !pending {
                return
            }
            await Task.yield()
        }
    }

    private static func values(_ entries: [LinkMessage.PropertyEntry]) -> [String: MirrorValue] {
        var out: [String: MirrorValue] = [:]
        for entry in entries {
            out[entry.name] = MirrorValue(entry.value)
        }
        return out
    }

    private static func text(_ entries: [LinkMessage.PropertyEntry]) -> String? {
        guard let entry = entries.first, case .utf8(let text) = entry.value else {
            return nil
        }
        return text
    }
}

/// A value set once a task finishes.
@MainActor
final class Box<Value> {
    var value: Value?
}

/// The session the fixture runs, known once the runner first drives the client.
final class SessionBox: @unchecked Sendable {
    private let lock = NSLock()
    private var session: StationSession?

    var value: StationSession? {
        get { lock.withLock { session } }
        set { lock.withLock { session = newValue } }
    }
}
