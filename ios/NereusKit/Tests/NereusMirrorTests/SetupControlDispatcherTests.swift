// NereusSDR for iOS: each described Setup control writes to its owner, and a stale edit sends nothing
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import LinkSessionTestSupport
import NereusLink
import Testing
@testable import NereusMirror

/// Holds one send before its handoff until released (no sleeps).
actor HandoffHold {
    private var entered = false
    private var enteredWaiters: [CheckedContinuation<Void, Never>] = []
    private var releaseWaiter: CheckedContinuation<Void, Never>?
    private var released = false

    func hold() async {
        entered = true
        for waiter in enteredWaiters { waiter.resume() }
        enteredWaiters = []
        if released { return }
        await withCheckedContinuation { releaseWaiter = $0 }
    }

    func waitUntilEntered() async {
        if entered { return }
        await withCheckedContinuation { enteredWaiters.append($0) }
    }

    func release() {
        released = true
        releaseWaiter?.resume()
        releaseWaiter = nil
    }
}

/// The app's session route in miniature: a sender captured at admission
/// reaches only the session it was captured with, through its permit.
final class TestRoute: @unchecked Sendable {
    private let lock = NSLock()
    private var session = 1
    private var sentMessages: [(session: Int, message: LinkMessage)] = []
    private var hold: HandoffHold?
    private var waiters: [(Int, CheckedContinuation<Void, Never>)] = []

    func use(_ session: Int) { lock.withLock { self.session = session } }
    func holdNext(_ hold: HandoffHold) { lock.withLock { self.hold = hold } }
    var sent: [(session: Int, message: LinkMessage)] { lock.withLock { sentMessages } }

    private var awake = false

    /// Wakes every waiter, whatever has been sent: an edit ended first.
    private func wakeAll() {
        let all = lock.withLock { () -> [CheckedContinuation<Void, Never>] in
            awake = true
            defer { waiters = [] }
            return waiters.map(\.1)
        }
        for waiter in all { waiter.resume() }
    }

    /// Waits for `count` sends, or for `edit` to end without them.
    func waitForCount(_ count: Int, unless edit: Task<SetupEditOutcome, Never>) async -> Bool {
        lock.withLock { awake = false }
        let watcher = Task { _ = await edit.value; wakeAll() }
        await waitForCount(count)
        watcher.cancel()
        return sent.count >= count
    }

    func waitForCount(_ count: Int) async {
        await withCheckedContinuation { continuation in
            let now = lock.withLock { () -> Bool in
                if sentMessages.count >= count || awake { return true }
                waiters.append((count, continuation))
                return false
            }
            if now { continuation.resume() }
        }
    }

    private func record(_ session: Int, _ message: LinkMessage) {
        let due = lock.withLock { () -> [CheckedContinuation<Void, Never>] in
            sentMessages.append((session, message))
            let due = waiters.filter { $0.0 <= sentMessages.count }.map(\.1)
            waiters.removeAll { $0.0 <= sentMessages.count }
            return due
        }
        for waiter in due { waiter.resume() }
    }

    func capture() -> MirrorStore.BoundSender {
        let admitted = lock.withLock { session }
        return { [self] message, permit in
            let hold = lock.withLock { () -> HandoffHold? in
                defer { self.hold = nil }
                return self.hold
            }
            await hold?.hold()
            let handedOff = permit.handoff {
                guard lock.withLock({ session }) == admitted else { return false }
                record(admitted, message)
                return true
            }
            guard handedOff else { throw LinkSendError.notConnected }
        }
    }
}

@MainActor
private final class PhoneKeys: SetupPhoneKeys {
    var values: [String: SetupValue] = ["DisplayGridColor": .text("#FFFFFF28")]
    private let subject = PassthroughSubject<Void, Never>()
    func value(forPhoneKey key: String) -> SetupValue? { values[key] }
    func set(_ value: SetupValue, forPhoneKey key: String) -> Bool {
        guard values[key] != nil else { return false }
        values[key] = value
        subject.send()
        return true
    }
    var changes: AnyPublisher<Void, Never> { subject.eraseToAnyPublisher() }
}

@MainActor
@Suite struct SetupControlDispatcherTests {
    @Test("Setup uses the settled connection-drop words and ordinary controls retain the disconnected words")
    func settledConnectionOutcomeWords() {
        #expect(SetupControlDispatcher.linkLostReason == "The connection to the Core dropped before it confirmed this change.")
        #expect(SetupControlDispatcher.linkLostReason == PropertyWriteOutcome.linkLost.reason)
        #expect(PropertyWriteOutcome.notSent.reason == "This app is not connected to the Core, so nothing was changed.")
    }

    /// A synthetic General page with one control per owner and kind.
    static func general(label: String = "Echo") -> String {
        func control(_ id: String, _ kind: String, _ binding: String, _ extras: String = "") -> String {
            #"{"id":"general.main.\#(id)","label":"\#(id)","tooltip":"","kind":"\#(kind)","binding":\#(binding),"applies":"live"\#(extras)}"#
        }
        let controls = [
            control("toggle", "toggle", #"{"setting":"SliceSampleOn"}"#, #","valueEncoding":{"true":"True","false":"False"}"#),
            control("count", "integer", #"{"setting":"SliceSampleCount"}"#, #","min":1,"max":9,"step":1"#),
            control("mode", "choice", #"{"setting":"SliceSampleMode"}"#, #","choices":["One","Two","Three"]"#),
            control("slope", "decimal", #"{"property":{"object":"slice:active","name":"agcSlope"}}"#,
                    #","min":0,"max":10,"step":0.5"#),
            control("drive", "integer", #"{"property":{"object":"transmit","name":"power"}}"#,
                    #","min":0,"max":100,"step":1,"gate":{"offAir":true}"#),
            control("reading", "readout", #"{"property":{"object":"transmit","name":"power"}}"#),
            control("emulate", "toggle", #"{"command":{"verb":"setOptions","valueProperty":{"object":"tci","name":"emulate"},"arguments":{"emulate":{"$controlValue":true},"other":{"$property":{"object":"tci","name":"other"}},"source":"phone"}}}"#),
            control("preset", "button", #"{"command":{"verb":"loadPreset","arguments":{"name":"defaults"}}}"#),
            control("grid", "colour", #"{"phone":"DisplayGridColor"}"#),
            control("edge", "colour", #"{"phone":"DisplayBandEdgeColor"}"#),
            control("future", "toggle", #"{"property":{"object":"tci","name":"emulate"}}"#,
                    #","gate":{"capability":"futureVersion","min":3}"#),
            control("keyed", "toggle", #"{"property":{"object":"tci","name":"emulate"}}"#, #","gate":{"transmit":true}"#),
            control("name", "text", #"{"setting":"SliceSampleName"}"#),
        ]
        return #"{"version":3,"category":{"id":"general","title":"General","where":"mixed"},"pages":[{"id":"general.main","title":"\#(label)","where":"mixed","sections":[{"title":"All","controls":["#
            + controls.joined(separator: ",") + "]}]}]}"
    }

    @MainActor final class Rig {
        fileprivate let route = TestRoute()
        let store: MirrorStore
        let settings: SettingsProxyClient
        let commands: CommandClient
        let feed: SetupDescriptionFeed
        fileprivate let phone = PhoneKeys()
        let selection = CurrentValueSubject<Int?, Never>(0)
        let dispatcher: SetupControlDispatcher

        init(clock: any LinkClock = SystemLinkClock()) {
            let route = route
            store = MirrorStore(send: { _ in }, clock: clock)
            settings = SettingsProxyClient(send: { _ in }, captureSender: { route.capture() }, clock: clock)
            commands = CommandClient(send: { _ in }, captureSender: { route.capture() })
            feed = SetupDescriptionFeed(store: store)
            let selection = selection
            dispatcher = SetupControlDispatcher(store: store, feed: feed, settings: settings, commands: commands,
                                                captureSender: { route.capture() },
                                                selectedSlice: { selection.value },
                                                selectionChanges: selection.eraseToAnyPublisher(), phone: phone)
        }

        func event(_ event: StationSession.Event) async {
            store.handle(event)
            settings.handle(event)
            await commands.handle(event)
        }

        /// A session `session` with the description, the settings and the
        /// transmit state given.
        func connect(session: Int, general: String = SetupControlDispatcherTests.general(), keyed: Bool = false,
                     txPermitted: Bool = false, propertyResults: Bool = false) async {
            route.use(session)
            await event(.stateChanged(.receivingSnapshot))
            let messages: [LinkMessage] = [
                FixtureReplay.stationHello(minor: 11), FixtureReplay.accepted,
                .capabilities(.init(properties: [
                    .init(name: "setupDescriptionVersion", value: .i64(10)),
                    .init(name: "txPermitted", value: .bool(txPermitted)),
                    .init(name: "txRefusalReason", value: .utf8(txPermitted ? "" : "Another device has transmit.")),
                ] + (propertyResults ? [.init(name: "propertyResultVersion", value: .i64(1))] : []))),
                .schema(.init(className: "SetupDescription", fields: [
                    .init(ordinal: 0, name: "revision", kind: .i64), .init(ordinal: 1, name: "general", kind: .utf8),
                ])),
                .objectCreate(.init(key: "setup", className: "SetupDescription", properties: [
                    .init(ordinal: 0, name: "revision", value: .i64(1)),
                    .init(ordinal: 1, name: "general", value: .utf8(general)),
                ])),
                .objectCreate(.init(key: "slice:0", className: "SliceModel", properties: [
                    .init(name: "sliceIndex", value: .i64(0)), .init(name: "agcSlope", value: .f64(2.5)),
                ])),
                .objectCreate(.init(key: "slice:1", className: "SliceModel", properties: [
                    .init(name: "sliceIndex", value: .i64(1)), .init(name: "agcSlope", value: .f64(4)),
                ])),
                .objectCreate(.init(key: "transmit", className: "TransmitModel", properties: [
                    .init(name: "power", value: .i64(50)),
                ])),
                .objectCreate(.init(key: "tci", className: "TciFacade", properties: [
                    .init(name: "emulate", value: .bool(false)), .init(name: "other", value: .bool(true)),
                ])),
                .objectCreate(.init(key: "txState", className: "TransmitState", properties: [
                    .init(name: "keyed", value: .bool(keyed)), .init(name: "tuning", value: .bool(false)),
                    .init(name: "twoTone", value: .bool(false)), .init(name: "txEnding", value: .bool(false)),
                ])),
                .settingsSnapshot(.init(properties: [
                    .init(name: "SliceSampleOn", value: .utf8("false")), .init(name: "SliceSampleCount", value: .utf8("3")),
                    .init(name: "SliceSampleMode", value: .utf8("1")), .init(name: "SliceSampleName", value: .utf8("shack")),
                ])),
                .snapshotComplete,
            ]
            for message in messages { await event(.message(message)) }
            await event(.stateChanged(.ready))
            for _ in 0..<5 { await Task.yield() }
        }

        func control(_ id: String) -> SetupDescription.Control {
            guard let description = feed.description(for: "general") else {
                preconditionFailure("no current General description")
            }
            return description.pages[0].sections[0].controls.first { $0.id == "general.main.\(id)" }!
        }

        func publish(_ general: String, revision: Int64) async {
            store.apply(.delta(.init(key: "setup", properties: [
                .init(name: "general", value: .utf8(general)), .init(name: "revision", value: .i64(revision)),
            ])))
            for _ in 0..<5 { await Task.yield() }
        }
    }

    // MARK: Each owner

    @Test func eachOwnerReceivesItsWrite() async throws {
        let rig = Rig()
        await rig.connect(session: 1)
        let dispatcher = rig.dispatcher

        // A setting, as its exact encoded string.
        let toggle = rig.control("toggle")
        #expect(dispatcher.state(of: toggle, in: "general") == .init(value: .bool(false), editable: true, reason: nil))
        let writing = Task { await dispatcher.edit(toggle, in: "general", to: .bool(true)) }
        #expect(await rig.route.waitForCount(1, unless: writing))
        guard case .settingsWrite(let write) = rig.route.sent[0].message else {
            Issue.record("no settings write"); return
        }
        #expect(write.key == "SliceSampleOn" && write.properties.first?.value == .utf8("True"))
        rig.settings.apply(.settingsValue(.init(key: write.key, origin: write.origin, properties: write.properties)))
        #expect(await writing.value == .applied)

        // A choice among named choices writes its ordinal.
        let choosing = Task { await dispatcher.edit(rig.control("mode"), in: "general", to: .integer(2)) }
        #expect(await rig.route.waitForCount(2, unless: choosing))
        guard case .settingsWrite(let choice) = rig.route.sent[1].message else { Issue.record("no choice"); return }
        #expect(choice.properties.first?.value == .utf8("2"))
        rig.settings.apply(.settingsValue(.init(key: choice.key, origin: choice.origin, properties: choice.properties)))
        #expect(await choosing.value == .applied)

        // A property of the selected slice goes to that slice.
        let slope = rig.control("slope")
        #expect(dispatcher.state(of: slope, in: "general").value == .decimal(2.5))
        let sloping = Task { await dispatcher.edit(slope, in: "general", to: .decimal(3.5)) }
        #expect(await rig.route.waitForCount(3, unless: sloping))
        guard case .propertyWrite(let property) = rig.route.sent[2].message else { Issue.record("no write"); return }
        #expect(property.key == "slice:0" && property.properties.first?.value == .f64(3.5))
        rig.store.apply(.delta(.init(key: "slice:0", properties: [.init(name: "agcSlope", value: .f64(3.5))])))
        #expect(await sloping.value == .applied)

        // A verb: the edited value and the others read now, from this session.
        let emulate = rig.control("emulate")
        let invoking = Task { await dispatcher.edit(emulate, in: "general", to: .bool(true)) }
        #expect(await rig.route.waitForCount(4, unless: invoking))
        guard case .commandInvoke(let invoke) = rig.route.sent[3].message else { Issue.record("no verb"); return }
        #expect(invoke.verb == "setOptions")
        #expect(invoke.args == [.init(name: "emulate", value: .bool(true)), .init(name: "other", value: .bool(true)),
                                .init(name: "source", value: .utf8("phone"))])
        await rig.commands.receive(.commandResult(.init(verb: invoke.verb, id: invoke.id, accepted: true, reason: "",
                                                        affected: [])))
        #expect(await invoking.value == .applied)

        // A button sends its literal arguments; a refusal carries the Core's words.
        let pressing = Task { await dispatcher.edit(rig.control("preset"), in: "general", to: nil) }
        #expect(await rig.route.waitForCount(5, unless: pressing))
        guard case .commandInvoke(let preset) = rig.route.sent[4].message else { Issue.record("no button"); return }
        #expect(preset.args == [.init(name: "name", value: .utf8("defaults"))])
        await rig.commands.receive(.commandResult(.init(verb: preset.verb, id: preset.id, accepted: false,
                                                        reason: "The radio is busy.", affected: [])))
        #expect(await pressing.value == .refused("The radio is busy."))

        // A phone key stays on this phone; one the phone lacks stays visible and disabled.
        #expect(await dispatcher.edit(rig.control("grid"), in: "general", to: .text("#102030FF")) == .applied)
        #expect(rig.phone.values["DisplayGridColor"] == .text("#102030FF"))
        let edge = dispatcher.state(of: rig.control("edge"), in: "general")
        #expect(!edge.editable && edge.reason == SetupControlDispatcher.notOnThisPhoneReason)
        #expect(rig.route.sent.count == 5)
        #expect(rig.route.sent.allSatisfy { $0.session == 1 })
    }

    /// A change the Core holds for a question (the link document, section
    /// 7.3; the several-devices design, section 7.3) is answered "Waiting
    /// for you to confirm.", by phase or by those words; the question
    /// follows. It is not a refusal, so no row shows it as one.
    @Test func aChangeTheCoreHoldsForAQuestionIsNotARefusal() async throws {
        let rig = Rig()
        await rig.connect(session: 1, propertyResults: true)
        let dispatcher = rig.dispatcher
        let waiting = SeveralDevices.waitingReason

        // A setting: settings.reject with the Core's value.
        let writing = Task { await dispatcher.edit(rig.control("toggle"), in: "general", to: .bool(true)) }
        #expect(await rig.route.waitForCount(1, unless: writing))
        guard case .settingsWrite(let write) = rig.route.sent[0].message else { Issue.record("no write"); return }
        rig.settings.apply(.settingsReject(.init(key: write.key, properties: [.init(name: write.key, value: .utf8("False"))],
                                                 reason: waiting)))
        let setting = await writing.value
        #expect(setting != .refused(waiting))
        #expect(setting == .awaitingConfirmation)

        // A property: property.result with the Core's value.
        let sloping = Task { await dispatcher.edit(rig.control("slope"), in: "general", to: .decimal(3.5)) }
        #expect(await rig.route.waitForCount(2, unless: sloping))
        guard case .propertyWrite(let property) = rig.route.sent[1].message else { Issue.record("no property"); return }
        let writeId = try #require(property.writeId)
        rig.store.apply(.propertyResult(.init(key: property.key, writeId: writeId, results: [
            .init(property: "agcSlope", accepted: false, reason: waiting,
                  value: .init(name: "agcSlope", value: .f64(2.5))),
        ])))
        let held = await sloping.value
        #expect(held != .refused(waiting))
        #expect(held == .awaitingConfirmation)

        // A verb answered by phase, with no words.
        let invoking = Task { await dispatcher.edit(rig.control("emulate"), in: "general", to: .bool(true)) }
        #expect(await rig.route.waitForCount(3, unless: invoking))
        guard case .commandInvoke(let invoke) = rig.route.sent[2].message else { Issue.record("no verb"); return }
        await rig.commands.receive(.commandResult(.init(verb: invoke.verb, id: invoke.id, accepted: false, reason: "",
                                                        affected: [], values: [.init(name: "phase",
                                                                                     value: .utf8("needsConfirmation"))])))
        let phased = await invoking.value
        #expect(phased != .refused(SetupControlDispatcher.refusedReason))
        #expect(phased == .awaitingConfirmation)

        // A button answered by the words alone.
        let pressing = Task { await dispatcher.edit(rig.control("preset"), in: "general", to: nil) }
        #expect(await rig.route.waitForCount(4, unless: pressing))
        guard case .commandInvoke(let preset) = rig.route.sent[3].message else { Issue.record("no button"); return }
        await rig.commands.receive(.commandResult(.init(verb: preset.verb, id: preset.id, accepted: false,
                                                        reason: waiting, affected: [])))
        let worded = await pressing.value
        #expect(worded != .refused(waiting))
        #expect(worded == .awaitingConfirmation)
        #expect(rig.route.sent.count == 4)
    }

    @Test func readoutsGatesAndRangesNeverSend() async {
        let rig = Rig()
        await rig.connect(session: 1)
        let dispatcher = rig.dispatcher
        // A readout of a writable property is still read-only.
        let reading = dispatcher.state(of: rig.control("reading"), in: "general")
        #expect(reading == .init(value: .integer(50), editable: false, reason: nil))
        #expect(await dispatcher.edit(rig.control("reading"), in: "general", to: .integer(10))
                == .notSent(SetupControlDispatcher.readingReason))
        // A capability the Core lacks, and transmit held elsewhere, with the Core's words.
        #expect(dispatcher.state(of: rig.control("future"), in: "general").reason
                == SetupControlDispatcher.needsNewerCoreReason)
        #expect(dispatcher.state(of: rig.control("keyed"), in: "general").reason == "Another device has transmit.")
        // Outside the described range.
        #expect(await dispatcher.edit(rig.control("count"), in: "general", to: .integer(12))
                == .notSent(SetupControlDispatcher.outOfRangeReason))
        #expect(rig.route.sent.isEmpty)
    }

    // MARK: Gates

    @Test func offAirControlsLockWhileKeyedAndUngatedOnesStayLive() async {
        let rig = Rig()
        await rig.connect(session: 1, keyed: true)
        let dispatcher = rig.dispatcher
        let drive = dispatcher.state(of: rig.control("drive"), in: "general")
        #expect(!drive.editable && drive.reason == SetupControlDispatcher.onAirReason && drive.value == .integer(50))
        #expect(dispatcher.state(of: rig.control("slope"), in: "general").editable)
        #expect(dispatcher.state(of: rig.control("toggle"), in: "general").editable)

        // Off the air, it unlocks; an edit admitted then is retired when the radio keys.
        rig.store.apply(.delta(.init(key: "txState", properties: [.init(name: "keyed", value: .bool(false))])))
        #expect(dispatcher.state(of: rig.control("drive"), in: "general").editable)
        guard case .success(let admission) = dispatcher.admit(rig.control("drive"), in: "general") else {
            Issue.record("off the air, drive was not admitted"); return
        }
        rig.store.apply(.delta(.init(key: "txState", properties: [.init(name: "tuning", value: .bool(true))])))
        #expect(admission.isRevoked)
        #expect(await dispatcher.perform(admission, value: .integer(60))
                == .notSent(SetupControlDispatcher.changedFirstReason))
        // Missing transmit state disables it too.
        rig.store.apply(.objectDestroy(.init(key: "txState", className: "TransmitState")))
        #expect(dispatcher.state(of: rig.control("drive"), in: "general").reason
                == SetupControlDispatcher.transmitStateUnknownReason)
        #expect(rig.route.sent.isEmpty)
    }

    // MARK: Staleness

    @Test func descriptionChangeBetweenGestureAndSendSendsNothing() async {
        let rig = Rig()
        await rig.connect(session: 1)
        guard case .success(let admission) = rig.dispatcher.admit(rig.control("toggle"), in: "general") else {
            Issue.record("not admitted"); return
        }
        rig.store.apply(.delta(.init(key: "setup", properties: [
            .init(name: "general", value: .utf8(SetupControlDispatcherTests.general(label: "Renamed"))),
            .init(name: "revision", value: .i64(2)),
        ])))
        #expect(admission.isRevoked, "revoked synchronously, before the feed's queued refresh")
        #expect(await rig.dispatcher.perform(admission, value: .bool(true))
                == .notSent(SetupControlDispatcher.changedFirstReason))

        // Held inside the sender when the description changes: the handoff refuses.
        await rig.publish(SetupControlDispatcherTests.general(), revision: 3)
        let hold = HandoffHold()
        rig.route.holdNext(hold)
        let dispatcher = rig.dispatcher
        let control = rig.control("count")
        let writing = Task { await dispatcher.edit(control, in: "general", to: .integer(4)) }
        await hold.waitUntilEntered()
        await rig.publish(SetupControlDispatcherTests.general(label: "Again"), revision: 4)
        await hold.release()
        #expect(await writing.value == .notSent(SetupControlDispatcher.changedFirstReason))
        #expect(rig.route.sent.isEmpty)
        #expect(rig.settings.value("SliceSampleCount") == "3", "the optimistic value went back")
    }

    @Test func replacedSessionReceivesNothingFromAnOldGesture() async {
        let rig = Rig()
        await rig.connect(session: 1)
        let hold = HandoffHold()
        rig.route.holdNext(hold)
        let dispatcher = rig.dispatcher
        let control = rig.control("slope")
        let writing = Task { await dispatcher.edit(control, in: "general", to: .decimal(6)) }
        await hold.waitUntilEntered()
        // The link drops and a new session signs in with the same values.
        await rig.event(.stateChanged(.waitingToRetry(seconds: 1)))
        await rig.connect(session: 2)
        await hold.release()
        let outcome = await writing.value
        #expect(outcome != .applied)
        #expect(rig.route.sent.isEmpty, "nothing reached the new session")

        // An admission from before the replacement cannot be used after it.
        guard case .success(let admission) = rig.dispatcher.admit(rig.control("toggle"), in: "general") else {
            Issue.record("not admitted"); return
        }
        await rig.event(.stateChanged(.waitingToRetry(seconds: 1)))
        await rig.connect(session: 3)
        #expect(admission.isRevoked)
        #expect(await rig.dispatcher.perform(admission, value: .bool(true))
                == .notSent(SetupControlDispatcher.changedFirstReason))
        #expect(rig.route.sent.isEmpty)
    }

    @Test func staleSnapshotAndSelectionChangeRevokeBeforeSending() async {
        let rig = Rig()
        await rig.connect(session: 1)
        let slopeControl = rig.control("slope")
        guard case .success(let slope) = rig.dispatcher.admit(slopeControl, in: "general") else {
            Issue.record("not admitted"); return
        }
        rig.selection.send(1)
        #expect(slope.isRevoked, "a new selection cancels the edit, never retargets it")
        #expect(await rig.dispatcher.perform(slope, value: .decimal(1)) == .notSent(SetupControlDispatcher.changedFirstReason))
        #expect(rig.dispatcher.state(of: rig.control("slope"), in: "general").value == .decimal(4))

        guard case .success(let toggle) = rig.dispatcher.admit(rig.control("toggle"), in: "general") else {
            Issue.record("not admitted"); return
        }
        rig.store.handle(.stateChanged(.waitingToRetry(seconds: 1)))
        #expect(toggle.isRevoked)
        #expect(await rig.dispatcher.perform(toggle, value: .bool(true)) == .notSent(SetupControlDispatcher.changedFirstReason))
        #expect(rig.route.sent.isEmpty)

        // No selected slice: its controls wait for one.
        rig.selection.send(nil)
        #expect(rig.dispatcher.state(of: slopeControl, in: "general").editable == false)
    }

    @Test func cachedValuesShowDisabledWithAReasonWhenNotCurrent() async {
        let rig = Rig()
        await rig.connect(session: 1)
        let toggle = rig.control("toggle")
        let reading = rig.control("reading")
        let grid = rig.control("grid")
        await rig.event(.stateChanged(.waitingToRetry(seconds: 1)))
        let state = rig.dispatcher.state(of: toggle, in: "general")
        #expect(state == .init(value: .bool(false), editable: false, reason: SetupControlDispatcher.notConnectedReason))
        // A phone key shows its kept value, disabled until the Core's page is current.
        #expect(rig.dispatcher.state(of: grid, in: "general")
                == .init(value: .text("#FFFFFF28"), editable: false, reason: SetupControlDispatcher.notConnectedReason))
        // A readout never shows a value from a session that is not current.
        #expect(rig.dispatcher.state(of: reading, in: "general").value == nil)
        #expect(await rig.dispatcher.edit(toggle, in: "general", to: .bool(true))
                == .notSent(SetupControlDispatcher.notConnectedReason))
        #expect(rig.route.sent.isEmpty)
    }
    @Test("Setup's admitted property and settings operations deliver their late refusal to their owner", arguments: ["slope", "count"])
    func lateOwnedRefusal(id: String) async throws {
        let clock = ManualLinkClock()
        let rig = Rig(clock: clock)
        await rig.connect(session: 1, propertyResults: true)
        let control = rig.control(id)
        var late: [SetupEditOutcome] = []
        let task = Task { await rig.dispatcher.edit(control, in: "general", to: id == "slope" ? .decimal(5) : .integer(5),
                                                   onLateOutcome: { late.append($0) }) }
        #expect(await rig.route.waitForCount(1, unless: task))
        await clock.advance(by: 5_000)
        #expect(await task.value == .notSent(PropertyWriteOutcome.notConfirmed.reason))
        let reason = "This Setup value is unavailable."
        if id == "slope" {
            guard case .propertyWrite(let write) = try #require(rig.route.sent.last?.message) else {
                Issue.record("missing property write"); return
            }
            rig.store.apply(.propertyResult(.init(key: write.key, writeId: try #require(write.writeId), results: [
                .init(property: "agcSlope", accepted: false, reason: reason, value: nil),
            ])))
        } else {
            rig.settings.apply(.settingsReject(.init(key: "SliceSampleCount", properties: [], reason: reason)))
        }
        #expect(late == [.refused(reason)])
    }

}
