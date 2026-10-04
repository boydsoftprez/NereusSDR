// NereusSDR for iOS: the CFC band editor (Setup description 19): its row, its profile and its sends through cfc.setProfile
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import LinkSessionTestSupport
import LinkTestSupport
import NereusLink
import Testing
@testable import NereusMirror

/// A clock that moves only when a test moves it, so the CFC editor's pause
/// ends when the test says and never on the wall clock.
final class CfcTestClock: Clock, @unchecked Sendable {
    struct Instant: InstantProtocol {
        var offset: Duration
        func advanced(by duration: Duration) -> Instant { Instant(offset: offset + duration) }
        func duration(to other: Instant) -> Duration { other.offset - offset }
        static func < (lhs: Instant, rhs: Instant) -> Bool { lhs.offset < rhs.offset }
    }

    private struct Sleeper {
        let id: UInt64
        let deadline: Instant
        let continuation: CheckedContinuation<Void, Error>
    }

    private let lock = NSLock()
    private var current = Instant(offset: .zero)
    private var sleepers: [Sleeper] = []
    private var cancelled: Set<UInt64> = []
    private var nextId: UInt64 = 0
    private var arrivals: [(count: Int, continuation: CheckedContinuation<Void, Never>)] = []
    private var arrived = 0

    var now: Instant { lock.withLock { current } }
    var minimumResolution: Duration { .zero }

    /// Sleeps begun so far, cancelled or not.
    var sleepsBegun: Int { lock.withLock { arrived } }

    func sleep(until deadline: Instant, tolerance: Duration?) async throws {
        let id = lock.withLock { () -> UInt64 in
            nextId &+= 1
            return nextId
        }
        try await withTaskCancellationHandler {
            try await withCheckedThrowingContinuation { (continuation: CheckedContinuation<Void, Error>) in
                let (outcome, due) = lock.withLock { () -> (Result<Void, Error>?, [CheckedContinuation<Void, Never>]) in
                    arrived += 1
                    let due = arrivals.filter { $0.count <= arrived }.map(\.continuation)
                    arrivals.removeAll { $0.count <= arrived }
                    if cancelled.remove(id) != nil {
                        return (.failure(CancellationError()), due)
                    }
                    if deadline.offset <= current.offset {
                        return (.success(()), due)
                    }
                    sleepers.append(Sleeper(id: id, deadline: deadline, continuation: continuation))
                    return (nil, due)
                }
                for waiter in due { waiter.resume() }
                if let outcome { continuation.resume(with: outcome) }
            }
        } onCancel: {
            let sleeper = lock.withLock { () -> Sleeper? in
                guard let index = sleepers.firstIndex(where: { $0.id == id }) else {
                    cancelled.insert(id)
                    return nil
                }
                return sleepers.remove(at: index)
            }
            sleeper?.continuation.resume(throwing: CancellationError())
        }
    }

    /// Returns once `count` sleeps have begun in all.
    func waitForSleeps(_ count: Int) async {
        await withCheckedContinuation { (continuation: CheckedContinuation<Void, Never>) in
            let now = lock.withLock { () -> Bool in
                if arrived >= count { return true }
                arrivals.append((count, continuation))
                return false
            }
            if now { continuation.resume() }
        }
    }

    /// Moves the clock on and wakes every sleep that has come due.
    func advance(by duration: Duration) {
        let due = lock.withLock { () -> [Sleeper] in
            current = current.advanced(by: duration)
            let due = sleepers.filter { $0.deadline.offset <= current.offset }
            sleepers.removeAll { $0.deadline.offset <= current.offset }
            return due
        }
        for sleeper in due { sleeper.continuation.resume() }
    }
}

/// R-IOS-18: DSP > CFC's band editor reads `transmit.cfcProfile`, edits it as
/// the desktop's TxCfcDialog does, and sends the whole profile through
/// `cfc.setProfile` after a pause in editing, one send at a time. The
/// Core's refusals are shown as it words them.
@MainActor
@Suite(.timeLimit(.minutes(1))) struct SetupCfcEditorTests {
    /// The profile the conformance fixture's Core holds (cfc-set-profile.json).
    static let coreProfile = #"{"bands":[{"compressionDb":2,"compressionQ":2,"frequencyHz":0,"postEqGainDb":-3,"postEqQ":3},{"compressionDb":4,"compressionQ":2,"frequencyHz":500,"postEqGainDb":-1,"postEqQ":3},{"compressionDb":6,"compressionQ":2,"frequencyHz":1000,"postEqGainDb":0,"postEqQ":3},{"compressionDb":8,"compressionQ":2,"frequencyHz":2000,"postEqGainDb":1,"postEqQ":3},{"compressionDb":10,"compressionQ":2,"frequencyHz":4000,"postEqGainDb":3,"postEqQ":3}],"maxHz":4000,"minHz":0,"parametric":true,"postEqGainDb":-2,"precompDb":4,"revision":"4dcd43a8aec093bf","state":"saved"}"#
    static let staleReason = "The CFC settings changed on the Core. Check the new values and try again."

    /// The Core's row as its DSP resource holds it.
    static func cfcRow() throws -> String {
        let source = URL(fileURLWithPath: #filePath).deletingLastPathComponent()
            .appendingPathComponent("../../../../resources/setup/dsp.json").standardizedFileURL
        let root = try #require(JSONSerialization.jsonObject(with: Data(contentsOf: source)) as? [String: Any])
        let pages = try #require(root["pages"] as? [[String: Any]])
        for page in pages {
            for section in page["sections"] as? [[String: Any]] ?? [] {
                for control in section["controls"] as? [[String: Any]] ?? [] where control["id"] as? String == "dsp.cfc.bands" {
                    return String(decoding: try JSONSerialization.data(withJSONObject: control), as: UTF8.self)
                }
            }
        }
        Issue.record("no CFC row")
        return "{}"
    }

    static func dsp(_ row: String, version: Int = 19) -> String {
        #"{"version":\#(version),"category":{"id":"dsp","title":"DSP","where":"station"},"pages":[{"id":"dsp.cfc","title":"CFC","where":"station","sections":[{"title":"CFC","controls":["#
            + row + "]}]}]}"
    }

    // MARK: The row

    @Test func theRowIsTheBandEditorWithTheCoresFieldsAndColumnsInOrder() throws {
        let control = try #require(try SetupDescription.parse(json: Self.dsp(try Self.cfcRow())).pages.first?.sections.first?.controls.first)
        #expect(control.label == "Configure CFC bands\u{2026}")
        #expect(control.metadataIssue == nil)
        #expect(control.unavailableReason == nil)
        #expect(control.specialized == .cfcBands)
        guard case .cfcProfile(let editor)? = control.binding else { Issue.record("not the editor"); return }
        #expect(editor.property == .init(object: "transmit", name: "cfcProfile"))
        #expect(editor.verb == "cfc.setProfile")
        #expect(editor.bandCounts == [5, 10, 18])
        #expect(editor.minSpanHz == 1000)
        #expect(editor.fields.map(\.label) == ["Low", "High", "Use Q Factors", "Pre-Comp", "Post-EQ"])
        #expect(editor.columns.map(\.label) == ["Freq", "Comp", "Comp Q", "Gain", "EQ Q"])
        #expect(editor.field("minHz")?.unit == "Hz")
        #expect(editor.field("precompDb")?.range == SetupDescription.Range(minimum: 0, maximum: 16, step: 0.1))
        #expect(editor.column("compressionQ")?.decimals == 2)
        #expect(editor.column("compressionQ")?.range == SetupDescription.Range(minimum: 0.2, maximum: 20, step: 0.01))
        #expect(control.gate?.offAir == nil)

        // An editor the phone cannot read stays greyed with a plain reason.
        var broken = try #require(JSONSerialization.jsonObject(with: Data(try Self.cfcRow().utf8)) as? [String: Any])
        broken.removeValue(forKey: "columns")
        let brokenRow = String(decoding: try JSONSerialization.data(withJSONObject: broken), as: UTF8.self)
        let greyed = try #require(try SetupDescription.parse(json: Self.dsp(brokenRow)).pages.first?.sections.first?.controls.first)
        #expect(greyed.specialized == nil)
        #expect(greyed.metadataIssue == nil)
        #expect(greyed.unavailableReason == SetupDescription.desktopOnlyReason)
    }

    // MARK: The profile

    @Test func theProfileReadsAsSentAndIsSentWithoutTheCoresStateAndRevision() throws {
        let profile = try #require(CfcProfile(json: Self.coreProfile))
        #expect(profile.state == .saved)
        #expect(profile.revision == "4dcd43a8aec093bf")
        #expect(profile.parametric)
        #expect(profile.bands.map(\.frequencyHz) == [0, 500, 1000, 2000, 4000])
        #expect(profile.bands[4].compressionDb == 10)
        let unknown = Self.coreProfile.replacingOccurrences(of: #""state":"saved""#, with: #""state":"future","extra":1"#)
        #expect(CfcProfile(json: unknown)?.state == .legacy)
        #expect(CfcProfile(json: "") == nil)
        #expect(CfcProfile(json: #"{"bands":[]}"#) == nil)

        let sent = try #require(CfcProfile(json: profile.profileJson.replacingOccurrences(
            of: "{\"bands\"", with: "{\"revision\":\"x\",\"bands\"")))
        #expect(sent.sameValues(as: profile))
        #expect(!profile.profileJson.contains("state"))
        #expect(!profile.profileJson.contains("revision"))
    }

    @Test func aNewBandCountSpreadsTheBandsEvenlyAndLowAndHighRescaleThem() throws {
        let profile = try #require(CfcProfile(json: Self.coreProfile))
        let ten = profile.withBandCount(10)
        #expect(ten.bands.count == 10)
        #expect(ten.bands.first?.frequencyHz == 0)
        #expect(ten.bands.last?.frequencyHz == 4000)
        let five = profile.withBandCount(5)
        #expect(five.bands.map(\.frequencyHz) == [0, 1000, 2000, 3000, 4000])
        #expect(five.bands.allSatisfy { $0.compressionDb == 0 && $0.postEqGainDb == 0 && $0.compressionQ == 4 && $0.postEqQ == 4 })
        #expect(five.precompDb == profile.precompDb)

        let wider = profile.withRange(minHz: 100, maxHz: 8100)
        #expect(wider.bands.map(\.frequencyHz) == [100, 1100, 2100, 4100, 8100])
        #expect(wider.bands[2].compressionDb == 6)
        let low = profile.withRange(minHz: 1000, maxHz: 4000)
        #expect(low.bands.first?.frequencyHz == 1000)
        #expect(low.bands.last?.frequencyHz == 4000)
    }

    // MARK: Sending

    @MainActor final class Rig {
        let route = TestRoute()
        let store: MirrorStore
        let settings: SettingsProxyClient
        let commands: CommandClient
        let feed: SetupDescriptionFeed
        let dispatcher: SetupControlDispatcher
        let clock = CfcTestClock()
        static let pause: Duration = .milliseconds(600)

        init(commandClock: any LinkClock = SystemLinkClock()) {
            let route = route
            store = MirrorStore(send: { _ in })
            settings = SettingsProxyClient(send: { _ in }, captureSender: { route.capture() })
            commands = CommandClient(clock: commandClock, send: { _ in }, captureSender: { route.capture() })
            feed = SetupDescriptionFeed(store: store)
            dispatcher = SetupControlDispatcher(store: store, feed: feed, settings: settings, commands: commands,
                                                captureSender: { route.capture() }, selectedSlice: { nil },
                                                selectionChanges: Just(nil).eraseToAnyPublisher(), phone: nil)
            dispatcher.cfcSendPause = Self.pause
            dispatcher.cfcClock = clock
        }

        func event(_ event: StationSession.Event) async {
            store.handle(event)
            settings.handle(event)
            await commands.handle(event)
        }

        func connect(transmitSettings: Int64 = 15, keyed: Bool = false) async throws {
            route.use(1)
            await event(.stateChanged(.receivingSnapshot))
            let messages: [LinkMessage] = [
                FixtureReplay.stationHello(minor: 11), FixtureReplay.accepted,
                .capabilities(.init(properties: [
                    .init(name: "setupDescriptionVersion", value: .i64(22)),
                    .init(name: "transmitSettingsVersion", value: .i64(transmitSettings)),
                    .init(name: "radioConnected", value: .bool(true)),
                    .init(name: "macAddress", value: .utf8("AA:BB:CC:DD:EE:01")),
                ])),
                .schema(.init(className: "SetupDescription", fields: [
                    .init(ordinal: 0, name: "revision", kind: .i64), .init(ordinal: 1, name: "dsp", kind: .utf8),
                ])),
                .objectCreate(.init(key: "setup", className: "SetupDescription", properties: [
                    .init(ordinal: 0, name: "revision", value: .i64(1)),
                    .init(ordinal: 1, name: "dsp", value: .utf8(SetupCfcEditorTests.dsp(try SetupCfcEditorTests.cfcRow(), version: 22))),
                ])),
                .objectCreate(.init(key: "transmit", className: "TransmitModel", properties: [
                    .init(name: "cfcProfile", value: .utf8(SetupCfcEditorTests.coreProfile)),
                ])),
                .objectCreate(.init(key: "txState", className: "TransmitState", properties: [
                    .init(name: "keyed", value: .bool(keyed)), .init(name: "tuning", value: .bool(false)),
                    .init(name: "twoTone", value: .bool(false)), .init(name: "txEnding", value: .bool(false)),
                ])),
                .settingsSnapshot(.init(properties: [])),
                .snapshotComplete,
            ]
            for message in messages { await event(.message(message)) }
            await event(.stateChanged(.ready))
            for _ in 0..<5 { await Task.yield() }
        }

        func control() throws -> SetupDescription.Control {
            let description = try #require(feed.description(for: "dsp"))
            return try #require(description.pages.flatMap(\.sections).flatMap(\.controls).first { $0.id == "dsp.cfc.bands" })
        }

        func panel() throws -> SetupCfcPanel {
            dispatcher.cfcPanel(try control(), in: "dsp")
        }

        /// An edit, as the editor's rows make it; one that begins a pause is counted.
        func edit(_ change: (inout CfcProfile) -> Void) throws {
            let before = dispatcher.cfc.pause
            dispatcher.editCfc(try control(), in: "dsp", change)
            if let after = dispatcher.cfc.pause, after != before {
                pausesBegun += 1
            }
        }

        /// Pauses begun by edits so far.
        private(set) var pausesBegun = 0

        /// Ends the pause after the edits so far: waits until the newest
        /// edit's pause has begun, then moves the clock past it. Returns the
        /// task that carries the send, which ends once the Core has answered.
        @discardableResult
        func endPause() async -> Task<Void, Never>? {
            let carrying = dispatcher.cfc.pause
            await clock.waitForSleeps(pausesBegun)
            clock.advance(by: Self.pause)
            return carrying
        }

        func invoke(_ index: Int) -> LinkMessage.CommandInvoke? {
            guard route.sent.count > index, case .commandInvoke(let invoke) = route.sent[index].message else { return nil }
            return invoke
        }

        func answer(_ invoke: LinkMessage.CommandInvoke, accepted: Bool, reason: String = "",
                    profile: String? = nil) async {
            await commands.receive(.commandResult(.init(
                verb: invoke.verb, id: invoke.id, accepted: accepted, reason: reason, affected: [],
                values: profile.map { [.init(name: CfcProfile.returnedProfileName, value: .utf8($0))] })))
            for _ in 0..<5 { await Task.yield() }
        }

        func push(_ profile: String) async {
            await event(.message(.delta(.init(key: "transmit", properties: [.init(name: "cfcProfile", value: .utf8(profile))]))))
            for _ in 0..<5 { await Task.yield() }
        }
    }

    static func argument(_ invoke: LinkMessage.CommandInvoke, _ name: String) -> String? {
        guard case .utf8(let text)? = invoke.args.first(where: { $0.name == name })?.value else { return nil }
        return text
    }

    @Test("an expired CFC submission restores Core without clearing a newer current-session draft", arguments: [false, true])
    func timeoutRestoresOnlySubmittedCfcDraft(newerDraft: Bool) async throws {
        let clock = ManualLinkClock()
        let rig = Rig(commandClock: clock)
        try await rig.connect()
        try rig.edit { $0.precompDb = 7 }
        let sending = await rig.endPause()
        await rig.route.waitForCount(1)
        let invoke = try #require(rig.invoke(0))
        let latest = Self.coreProfile.replacingOccurrences(of: #""precompDb":4"#, with: #""precompDb":6"#)
            .replacingOccurrences(of: "4dcd43a8aec093bf", with: "0000000000000002")
        await rig.push(latest)
        await clock.advance(by: 4_000)
        if newerDraft {
            try rig.edit { $0.precompDb = 9 }
            // This newer draft's own editing pause ends while the older
            // submitted command still waits; it has not been sent.
            let draftPause = await rig.endPause()
            await draftPause?.value
            #expect(rig.route.sent.count == 1)
        }
        await clock.advance(by: 999)
        #expect(try rig.panel().profile?.precompDb == (newerDraft ? 9 : 7))
        await clock.advance(by: 1)
        await sending?.value
        #expect(try rig.panel().profile?.precompDb == (newerDraft ? 9 : 6))
        #expect(try rig.panel().problem == (newerDraft ? nil : PropertyWriteOutcome.notConfirmed.reason))
        // The actual command is never replayed. Its late reason goes
        // only to the still-current submission, never to a newer draft.
        let words = newerDraft ? "Obsolete CFC refusal." : "Current CFC refusal."
        await rig.answer(invoke, accepted: false, reason: words)
        if !newerDraft {
            for _ in 0..<2_000 {
                if try rig.panel().problem == words { break }
                await Task.yield()
            }
        }
        #expect(try rig.panel().profile?.precompDb == (newerDraft ? 9 : 6))
        #expect(try rig.panel().problem == (newerDraft ? nil : words))
        #expect(rig.route.sent.filter { entry in
            guard case .commandInvoke(let command) = entry.message else { return false }
            return Self.argument(command, CfcProfile.profileArgumentName) == Self.argument(invoke, CfcProfile.profileArgumentName)
        }.count == 1)
        await rig.event(.stateChanged(.waitingToRetry(seconds: 1)))
        #expect(rig.dispatcher.cfc.draft == nil)
        await rig.event(.stateChanged(.receivingSnapshot))
        #expect(rig.dispatcher.cfc.draft == nil)
    }

    @Test func belowTransmitSettingsFifteenTheEditorIsGreyedWithTheCoresReason() async throws {
        let rig = Rig()
        try await rig.connect(transmitSettings: 14)
        let panel = try rig.panel()
        #expect(panel.reason == "This Core cannot change its transmit settings.")
        #expect(!panel.editable)
        #expect(panel.profile?.revision == "4dcd43a8aec093bf")
        try rig.edit { $0.precompDb = 9 }
        // A greyed editor begins no pause, so nothing can be sent later.
        #expect(rig.dispatcher.cfc.pause == nil)
        #expect(rig.clock.sleepsBegun == 0)
        rig.clock.advance(by: Rig.pause)
        #expect(rig.route.sent.isEmpty)
    }

    @Test func editsAreSentWholeAfterAPauseAndTheCoresProfileIsFollowedOnceTaken() async throws {
        let rig = Rig()
        try await rig.connect()
        #expect(try rig.panel().editable)
        try rig.edit { $0.precompDb = 5 }
        let first = rig.dispatcher.cfc.pause
        try rig.edit { $0.precompDb = 6 }
        let second = rig.dispatcher.cfc.pause
        try rig.edit { $0.bands[1].compressionDb = 7 }
        // Shown at once, sent once after the pause.
        #expect(try rig.panel().profile?.precompDb == 6)
        #expect(rig.route.sent.isEmpty)
        let carrying = await rig.endPause()
        await rig.route.waitForCount(1)
        // The pauses the later edits cancelled end without sending.
        await first?.value
        await second?.value
        #expect(rig.route.sent.count == 1)
        let invoke = try #require(rig.invoke(0))
        #expect(invoke.verb == "cfc.setProfile")
        #expect(Self.argument(invoke, "expectedRevision") == "4dcd43a8aec093bf")
        let sent = try #require(Self.argument(invoke, "profileJson").flatMap { CfcProfile(json: $0.replacingOccurrences(
            of: "{\"bands\"", with: "{\"revision\":\"\",\"bands\"")) })
        #expect(sent.precompDb == 6)
        #expect(sent.bands[1].compressionDb == 7)
        #expect(sent.bands.count == 5)
        #expect(try rig.panel().sending)

        // The Core's transmit delta comes first, then its answer.
        let stored = Self.coreProfile.replacingOccurrences(of: #""precompDb":4"#, with: #""precompDb":6"#)
            .replacingOccurrences(of: #""compressionDb":4,"#, with: #""compressionDb":7,"#)
            .replacingOccurrences(of: "4dcd43a8aec093bf", with: "0000000000000002")
        await rig.push(stored)
        await rig.answer(invoke, accepted: true, profile: stored)
        await carrying?.value
        let panel = try rig.panel()
        #expect(!panel.sending)
        #expect(panel.problem == nil)
        #expect(panel.profile == CfcProfile(json: stored))
    }

    @Test func anEditMadeWhileASendIsOutIsSentNextWithTheNewRevision() async throws {
        let rig = Rig(commandClock: ManualLinkClock())
        try await rig.connect()
        try rig.edit { $0.postEqGainDb = 1 }
        await rig.endPause()
        await rig.route.waitForCount(1)
        let first = try #require(rig.invoke(0))
        try rig.edit { $0.postEqGainDb = 2 }
        // Its pause ends while the first send is out: it sends nothing.
        let held = await rig.endPause()
        await held?.value
        #expect(rig.route.sent.count == 1)
        let stored = Self.coreProfile.replacingOccurrences(of: #""postEqGainDb":-2"#, with: #""postEqGainDb":1"#)
            .replacingOccurrences(of: "4dcd43a8aec093bf", with: "0000000000000003")
        await rig.push(stored)
        await rig.answer(first, accepted: true, profile: stored)
        await rig.route.waitForCount(2)
        let second = try #require(rig.invoke(1))
        #expect(Self.argument(second, "expectedRevision") == "0000000000000003")
        #expect(Self.argument(second, "profileJson")?.contains(#""postEqGainDb":2"#) == true)
    }

    @Test func anEditIsSentWithTheRevisionItWasMadeFromNotTheCoresNewerOne() async throws {
        // Another device changes the profile while this edit waits for its
        // pause: the edit names the revision it was made from, so the Core
        // refuses it rather than overwrite a change the phone has not seen.
        let rig = Rig()
        try await rig.connect()
        try rig.edit { $0.precompDb = 11 }
        let elsewhere = Self.coreProfile.replacingOccurrences(of: "4dcd43a8aec093bf", with: "00000000000000e1")
            .replacingOccurrences(of: #""postEqGainDb":-2"#, with: #""postEqGainDb":5"#)
        await rig.push(elsewhere)
        await rig.endPause()
        await rig.route.waitForCount(1)
        let invoke = try #require(rig.invoke(0))
        #expect(Self.argument(invoke, "expectedRevision") == "4dcd43a8aec093bf")
    }

    @Test func anEditHeldBehindATakenSendIsRebasedOnTheProfileTheCoreReturned() async throws {
        // The Core's answer arrives before its transmit delta: the held
        // edit follows on from the profile the answer carries.
        let rig = Rig(commandClock: ManualLinkClock())
        try await rig.connect()
        try rig.edit { $0.precompDb = 1 }
        await rig.endPause()
        await rig.route.waitForCount(1)
        let first = try #require(rig.invoke(0))
        try rig.edit { $0.precompDb = 2 }
        let held = await rig.endPause()
        await held?.value
        let stored = Self.coreProfile.replacingOccurrences(of: #""precompDb":4"#, with: #""precompDb":1"#)
            .replacingOccurrences(of: "4dcd43a8aec093bf", with: "00000000000000b2")
        await rig.answer(first, accepted: true, profile: stored)
        await rig.route.waitForCount(2)
        let second = try #require(rig.invoke(1))
        #expect(Self.argument(second, "expectedRevision") == "00000000000000b2")
        #expect(Self.argument(second, "profileJson")?.contains(#""precompDb":2"#) == true)
    }

    @Test func aRefusalIsShownAsSentAndTheEditorLoadsTheCoresProfile() async throws {
        let rig = Rig()
        try await rig.connect()
        try rig.edit { $0.precompDb = 12 }
        let carrying = await rig.endPause()
        await rig.route.waitForCount(1)
        let invoke = try #require(rig.invoke(0))
        let latest = Self.coreProfile.replacingOccurrences(of: "4dcd43a8aec093bf", with: "0000000000000009")
            .replacingOccurrences(of: #""precompDb":4"#, with: #""precompDb":3"#)
        await rig.push(latest)
        await rig.answer(invoke, accepted: false, reason: Self.staleReason)
        await carrying?.value
        let panel = try rig.panel()
        #expect(!panel.sending)
        #expect(panel.problem == Self.staleReason)
        #expect(panel.profile == CfcProfile(json: latest))
        #expect(panel.editable)

        // Any other refusal, word for word.
        try rig.edit { $0.bands[2].compressionQ = 0.2 }
        let next = await rig.endPause()
        await rig.route.waitForCount(2)
        let order = "Keep each band's frequency above the one before it."
        await rig.answer(try #require(rig.invoke(1)), accepted: false, reason: order)
        await next?.value
        #expect(try rig.panel().problem == order)
        #expect(try rig.panel().profile == CfcProfile(json: latest))
    }

    @Test func aLinkLostAfterASendDropsTheSendAndFollowsTheNextProfile() async throws {
        let rig = Rig()
        try await rig.connect()
        try rig.edit { $0.precompDb = 8 }
        await rig.endPause()
        await rig.route.waitForCount(1)
        #expect(try rig.panel().sending)
        await rig.event(.stateChanged(.waitingToRetry(seconds: 1)))
        for _ in 0..<5 { await Task.yield() }
        #expect(!rig.dispatcher.cfc.sending)
        #expect(rig.dispatcher.cfc.draft == nil)
        #expect(rig.dispatcher.cfc.problem == nil)

        // The next session: the Core's profile, then the next edit is sent normally.
        try await rig.connect()
        let next = Self.coreProfile.replacingOccurrences(of: "4dcd43a8aec093bf", with: "000000000000000a")
        await rig.push(next)
        #expect(try rig.panel().profile == CfcProfile(json: next))
        try rig.edit { $0.precompDb = 2 }
        await rig.endPause()
        await rig.route.waitForCount(2)
        let invoke = try #require(rig.invoke(1))
        #expect(Self.argument(invoke, "expectedRevision") == "000000000000000a")
    }

    @Test func theEditorStaysLiveOnTheAir() async throws {
        let rig = Rig()
        try await rig.connect(keyed: true)
        #expect(try rig.panel().editable)
        try rig.edit { $0.parametric = false }
        await rig.endPause()
        await rig.route.waitForCount(1)
        #expect(rig.route.sent.count == 1)
    }
}
