// NereusSDR for iOS: taking control of a slice: who controls it, the Core's words for it, Take control and Take back
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkTestSupport
import NereusLink
import Testing
@testable import NereusMirror

/// R-IOS-42 (D109, D110, D115): `access:<id>` read as the slice access
/// contract note and `surface.json` give it; the Core's owner line and
/// holder words; `slice.takeControl` sent with exactly the values the
/// phone saw and every answer read as the Core sends it; and the
/// `controlTaken` notice's Take back at `sliceAccessVersion` 1 and 2, with
/// `takeBack` true and false, a refusal that may be tried again, a final
/// one, and a repeat.
@MainActor
@Suite struct SliceAccessTests {
    private let sent = SentMessages()

    // MARK: The gate and the access object

    private func store(version: Int64?, minor: UInt16 = 11) -> MirrorStore {
        let store = MirrorStore(send: sent.sender)
        store.apply(FixtureReplay.stationHello(minor: minor))
        var caps: [String: LinkMessage.PropertyValue] = [SeveralDevices.capability: .i64(1)]
        if let version {
            caps[SliceAccess.capability] = .i64(version)
        }
        store.apply(FixtureReplay.capabilities(caps))
        return store
    }

    private static func access(_ sliceId: Int64 = 1, incarnation: Int64 = 3, controller: String,
                               revision: Int64 = 12, listeners: [String] = []) -> LinkMessage {
        let list = LinkJSON.array(listeners.map(LinkJSON.string)).compactText
        return .objectCreate(LinkMessage.ObjectCreate(key: "access:\(sliceId)", className: SliceAccess.accessClass,
                                                      properties: [
            .init(ordinal: 0, name: "sliceId", value: .i64(sliceId)),
            .init(ordinal: 1, name: "incarnation", value: .i64(incarnation)),
            .init(ordinal: 2, name: "controllerDeviceId", value: .utf8(controller)),
            .init(ordinal: 3, name: "controlRevision", value: .i64(revision)),
            .init(ordinal: 4, name: "listenerDeviceIds", value: .utf8(list)),
            .init(ordinal: 5, name: "activeRxDeviceIds", value: .utf8("[]")),
            .init(ordinal: 6, name: "txSelected", value: .bool(false)),
            .init(ordinal: 7, name: "onAir", value: .bool(false)),
        ]))
    }

    @Test func theGateNeedsMinorElevenAndTheCapability() {
        #expect(!SliceAccess.available(in: store(version: nil)))
        #expect(SliceAccess.version(in: store(version: 1)) == 1)
        #expect(SliceAccess.version(in: store(version: 2)) == 2)
        #expect(SliceAccess.version(in: store(version: 3)) == 3)
        #expect(SliceAccess.version(in: store(version: 2, minor: 10)) == 0)
        let older = store(version: 2, minor: 10)
        older.apply(Self.access(controller: "mac"))
        #expect(SliceAccess.states(in: older).isEmpty)
    }

    @Test func anAccessObjectReadsAsTheCoreSendsIt() throws {
        let store = store(version: 2)
        store.apply(Self.access(controller: "mac", listeners: ["mac", "phone"]))
        let state = try #require(SliceAccess.states(in: store)[1])
        #expect(state == SliceAccess.State(sliceId: 1, incarnation: 3, controllerDeviceId: "mac", controlRevision: 12,
                                           listenerDeviceIds: ["mac", "phone"]))
        #expect(!state.nobodyControls)
    }

    @Test func theAccessClassMatchesTheSurface() throws {
        let surface = try LinkFixtureLoader.jsonObject(at: LinkFixtureLoader.root.appendingPathComponent("surface.json"))
        let classes = try #require(surface["mirrorClasses"] as? [String: [String: Any]])
        let properties = try #require(classes[SliceAccess.accessClass]?["properties"] as? [[String: Any]])
        #expect(properties.compactMap { $0["name"] as? String } == [
            "sliceId", "incarnation", "controllerDeviceId", "controlRevision", "listenerDeviceIds",
            "activeRxDeviceIds", "txSelected", "onAir",
        ])
    }

    // MARK: The Core's words

    private static func device(_ id: String, name: String, kind: String, shortName: String = "",
                               hostsCore: Bool = false) -> SeveralDevices.ConnectedDevice {
        SeveralDevices.ConnectedDevice(deviceId: id, name: name, shortName: shortName, kind: kind, paired: true,
                                       hostsCore: hostsCore, revocable: true, state: .listening, holdsTransmit: false,
                                       lastActivitySeconds: 0, connectedForSeconds: 0, awayForSeconds: 0,
                                       transmittingForSeconds: 0, listeningOn: [], transmittingOn: nil)
    }

    @Test func theHolderIsWordedAsTheCoreWordsIt() {
        let devices = [Self.device("mac", name: "Shack desktop", kind: "computer"),
                       Self.device("pad", name: "", kind: "tablet"), Self.device("phone2", name: "", kind: "phone"),
                       Self.device("odd", name: "", kind: "watch")]
        #expect(SliceAccess.holderWords("mac", devices: devices) == "Shack desktop")
        #expect(SliceAccess.holderWords("", devices: devices) == "the Core")
        #expect(SliceAccess.holderWords("station", devices: devices) == "the Core")
        #expect(SliceAccess.holderWords("pad", devices: devices) == "a tablet")
        #expect(SliceAccess.holderWords("phone2", devices: devices) == "a phone")
        #expect(SliceAccess.holderWords("odd", devices: devices) == "a computer")
        #expect(SliceAccess.holderWords("gone", devices: devices) == "another device")

        let listening = SliceAccess.State(sliceId: 1, incarnation: 3, controllerDeviceId: "mac", controlRevision: 12)
        #expect(SliceAccess.ownerLine(listening, devices: devices)
                == "Slice B is controlled by Shack desktop. Take control to change it.")
        let nobody = SliceAccess.State(sliceId: 2, incarnation: 4, controllerDeviceId: "", controlRevision: 5)
        #expect(SliceAccess.ownerLine(nobody, devices: devices) == "Nobody controls slice C. Take control to change it.")
        let core = SliceAccess.State(sliceId: 0, incarnation: 1, controllerDeviceId: "station", controlRevision: 1)
        #expect(SliceAccess.ownerLine(core, devices: devices)
                == "Slice A is controlled by the Core. Take control to change it.")
    }

    /// The desktop hosting the Core, as `connectedDevices` sends it: its
    /// id is base64url of `station`, never `station` itself.
    private static let hostingId = "c3RhdGlvbg"

    @Test func theCoresOwnPositionIsNamedByTheDesktopHostingIt() {
        let hosted = [Self.device("mac", name: "Laptop", kind: "computer"),
                      Self.device(Self.hostingId, name: "Shack desktop", kind: "computer", shortName: "Shack",
                                  hostsCore: true)]
        #expect(SliceAccess.hostingDesktop(hosted)?.deviceId == Self.hostingId)
        #expect(SliceAccess.holderWords("station", devices: hosted) == "Shack desktop")
        #expect(SliceAccess.deviceWords("station", devices: hosted, short: true) == "Shack")
        let core = SliceAccess.State(sliceId: 0, incarnation: 1, controllerDeviceId: "station", controlRevision: 1)
        #expect(SliceAccess.ownerLine(core, devices: hosted)
                == "Slice A is controlled by Shack desktop. Take control to change it.")
        // The entry is never matched by its id.
        #expect(SliceAccess.holderWords(Self.hostingId, devices: hosted) == "Shack desktop")
        #expect(SliceAccess.holderWords("station", devices: [Self.device("station", name: "Imposter", kind: "computer")])
                == "the Core")
    }

    @Test func aHostingDesktopWithOnlyAShortNameIsNamedByIt() {
        let hosted = [Self.device(Self.hostingId, name: "", kind: "computer", shortName: "Shack", hostsCore: true)]
        #expect(SliceAccess.deviceWords("station", devices: hosted, short: true) == "Shack")
        #expect(SliceAccess.holderWords("station", devices: hosted) == "Shack")
        let unnamed = [Self.device(Self.hostingId, name: "", kind: "computer", hostsCore: true)]
        #expect(SliceAccess.holderWords("station", devices: unnamed) == "the Core")
        // Where short names are used, any device's short name comes first.
        let phone = [Self.device("pad", name: "Kitchen iPad", kind: "tablet", shortName: "iPad")]
        #expect(SliceAccess.deviceWords("pad", devices: phone, short: true) == "iPad")
        #expect(SliceAccess.deviceWords("pad", devices: phone) == "Kitchen iPad")
    }

    @Test func aHeadlessCoreIsTheCore() {
        let headless = [Self.device("mac", name: "Laptop", kind: "computer")]
        #expect(SliceAccess.hostingDesktop(headless) == nil)
        #expect(SliceAccess.holderWords("station", devices: headless) == "the Core")
        #expect(SliceAccess.deviceWords("station", devices: headless, short: true) == "the Core")
        #expect(SliceAccess.holderWords("station", devices: []) == "the Core")
    }

    @Test func aListenerListNamesTheCoresPositionByItsDesktop() {
        let state = SliceAccess.State(sliceId: 0, incarnation: 1, controllerDeviceId: "phone", controlRevision: 2,
                                      listenerDeviceIds: ["phone", "station", "mac", "gone"])
        let hosted = [Self.device("mac", name: "Laptop", kind: "computer", shortName: "Lap"),
                      Self.device(Self.hostingId, name: "Shack desktop", kind: "computer", shortName: "Shack",
                                  hostsCore: true)]
        #expect(SliceAccess.listenerWords(state, devices: hosted) == ["Shack desktop", "Laptop", "another device"])
        #expect(SliceAccess.listenerWords(state, devices: hosted, short: true) == ["Shack", "Lap", "another device"])
        #expect(SliceAccess.listenerWords(state, devices: hosted, excluding: "mac") == ["Shack desktop", "another device"])
        let headless = [Self.device("mac", name: "Laptop", kind: "computer")]
        #expect(SliceAccess.listenerWords(state, devices: headless) == ["the Core", "Laptop", "another device"])
    }

    // MARK: Take control offered

    private static func state(controller: String, onAir: Bool = false) -> SliceAccess.State {
        SliceAccess.State(sliceId: 0, incarnation: 1, controllerDeviceId: controller, controlRevision: 4,
                          listenerDeviceIds: [controller, "phone"], onAir: onAir)
    }

    @Test func atThreeEverySliceCanBeTakenButOneOnTheAir() {
        let headless = [Self.device("mac", name: "Laptop", kind: "computer")]
        let hosted = headless + [Self.device(Self.hostingId, name: "Shack desktop", kind: "computer", hostsCore: true)]
        for devices in [headless, hosted] {
            #expect(SliceAccess.takeRefusal(Self.state(controller: "station"), version: 3, devices: devices) == nil)
            #expect(SliceAccess.takeRefusal(Self.state(controller: "mac"), version: 3, devices: devices) == nil)
            #expect(SliceAccess.takeRefusal(Self.state(controller: ""), version: 3, devices: devices) == nil)
            #expect(SliceAccess.takeRefusal(Self.state(controller: "station", onAir: true), version: 3, devices: devices)
                    == "Slice A is transmitting. Take control once it stops.")
            #expect(SliceAccess.takeRefusal(Self.state(controller: "mac", onAir: true), version: 4, devices: devices)
                    == "Slice A is transmitting. Take control once it stops.")
        }
    }

    @Test(arguments: [Int64(1), 2])
    func belowThreeTheCoresOwnSliceOnAHeadlessCoreIsGreyed(_ version: Int64) {
        let headless = [Self.device("mac", name: "Laptop", kind: "computer")]
        let hosted = headless + [Self.device(Self.hostingId, name: "Shack desktop", kind: "computer", hostsCore: true)]
        #expect(SliceAccess.takeRefusal(Self.state(controller: "station"), version: version, devices: headless)
                == "Slice A is run by the Core itself, so control of it cannot pass to this device.")
        #expect(SliceAccess.takeRefusal(Self.state(controller: "station"), version: version, devices: []) != nil)
        // A hosting desktop's slice passes as any device's, as before.
        #expect(SliceAccess.takeRefusal(Self.state(controller: "station"), version: version, devices: hosted) == nil)
        // Every other take is the Core's to answer, as before.
        #expect(SliceAccess.takeRefusal(Self.state(controller: "mac"), version: version, devices: headless) == nil)
    }

    /// JJ, 2026-09-30: Take control is greyed ahead of time while the slice
    /// is on the air at every version the Core sends `onAir`, as the
    /// desktop greys it, with the Core's words.
    @Test(arguments: [Int64(1), 2, 3, 4])
    func onTheAirIsGreyedAtEveryVersion(_ version: Int64) {
        let headless = [Self.device("mac", name: "Laptop", kind: "computer")]
        let hosted = headless + [Self.device(Self.hostingId, name: "Shack desktop", kind: "computer", hostsCore: true)]
        for devices in [headless, hosted] {
            for controller in ["mac", "station", ""] {
                #expect(SliceAccess.takeRefusal(Self.state(controller: controller, onAir: true), version: version,
                                                devices: devices)
                        == "Slice A is transmitting. Take control once it stops.")
            }
        }
    }

    // MARK: The listening flag's owner row

    @Test func theOwnerRowNamesTheHolderAsTheCoreDoes() {
        let hosted = [Self.device("mac", name: "MacBook Pro", kind: "computer"),
                      Self.device(Self.hostingId, name: "Shack desktop", kind: "computer", hostsCore: true),
                      Self.device("pad", name: "", kind: "tablet"),
                      Self.device("low", name: "iPhone de JJ", kind: "phone")]
        func words(_ controller: String, _ devices: [SeveralDevices.ConnectedDevice] = hosted) -> String {
            SliceAccess.ownerWords(Self.state(controller: controller), devices: devices)
        }
        #expect(words("station") == "Shack desktop controls A")
        #expect(words("station", []) == "The Core controls A")
        #expect(words("") == "The Core controls A")
        #expect(words("mac") == "MacBook Pro controls A")
        #expect(words("pad") == "A tablet controls A")
        #expect(words("gone") == "Another device controls A")
        // A name as sent keeps its own letters.
        #expect(words("low") == "iPhone de JJ controls A")
    }

    // MARK: Take control

    private static func answer(_ verb: String, id: UInt32, accepted: Bool, reason: String = "",
                               values: [LinkMessage.PropertyEntry]? = nil) -> LinkMessage {
        .commandResult(LinkMessage.CommandResult(verb: verb, id: id, accepted: accepted, reason: reason,
                                                 affected: accepted ? ["slice:1", "access:1"] : [], values: values))
    }

    private func ready(_ commands: CommandClient) async {
        await commands.handle(.stateChanged(.receivingSnapshot))
        await commands.handle(.stateChanged(.ready))
    }

    @Test func takeControlSendsTheValuesThePhoneSawAndReadsTheNewRevision() async throws {
        let commands = CommandClient(send: sent.sender)
        await ready(commands)
        let state = SliceAccess.State(sliceId: 1, incarnation: 3, controllerDeviceId: "mac", controlRevision: 12)
        let task = Task { await SliceAccess.takeControl(state, commands: commands) }
        #expect(await sent.settle(untilCount: 1))
        #expect(sent.messages.last == .commandInvoke(LinkMessage.CommandInvoke(verb: "slice.takeControl", id: 1, args: [
            LinkMessage.PropertyEntry(name: "sliceId", value: .i64(1)),
            LinkMessage.PropertyEntry(name: "incarnation", value: .i64(3)),
            LinkMessage.PropertyEntry(name: "controlRevision", value: .i64(12)),
        ])))
        await commands.receive(Self.answer("slice.takeControl", id: 1, accepted: true,
                                           values: [.init(name: "controlRevision", value: .i64(13))]))
        #expect(await task.value == .accepted(controlRevision: 13))
    }

    @Test func listenStopReleaseAndLevelSendTheValuesThePhoneSaw() async throws {
        let commands = CommandClient(send: sent.sender)
        await ready(commands)
        let state = SliceAccess.State(sliceId: 0, incarnation: 5, controllerDeviceId: "station", controlRevision: 9)
        let slice = [LinkMessage.PropertyEntry(name: "sliceId", value: .i64(0)),
                     LinkMessage.PropertyEntry(name: "incarnation", value: .i64(5))]

        let listen = Task { await SliceAccess.listen(state, commands: commands) }
        #expect(await sent.settle(untilCount: 1))
        #expect(sent.messages.last == .commandInvoke(LinkMessage.CommandInvoke(verb: "slice.listen", id: 1, args: slice)))
        await commands.receive(Self.answer("slice.listen", id: 1, accepted: true,
                                           values: [.init(name: "controlRevision", value: .i64(9))]))
        #expect(await listen.value == .accepted(controlRevision: 9))

        let level = Task { await SliceAccess.setListenLevel(state, level: 1.4, muted: true, commands: commands) }
        #expect(await sent.settle(untilCount: 2))
        #expect(sent.messages.last == .commandInvoke(LinkMessage.CommandInvoke(verb: "slice.setListenLevel", id: 2,
                                                                                args: slice + [
            LinkMessage.PropertyEntry(name: "level", value: .f64(1)),
            LinkMessage.PropertyEntry(name: "muted", value: .bool(true)),
        ])))
        let notListening = "You are not listening to slice A. Listen in first."
        await commands.receive(Self.answer("slice.setListenLevel", id: 2, accepted: false, reason: notListening))
        #expect(await level.value == .refused(notListening))

        let stop = Task { await SliceAccess.stopListening(state, commands: commands) }
        #expect(await sent.settle(untilCount: 3))
        #expect(sent.messages.last == .commandInvoke(LinkMessage.CommandInvoke(verb: "slice.stopListening", id: 3,
                                                                                args: slice)))
        await commands.receive(Self.answer("slice.stopListening", id: 3, accepted: true))
        #expect(await stop.value == .accepted(controlRevision: nil))

        let release = Task { await SliceAccess.release(state, commands: commands) }
        #expect(await sent.settle(untilCount: 4))
        #expect(sent.messages.last == .commandInvoke(LinkMessage.CommandInvoke(verb: "slice.release", id: 4, args: slice + [
            LinkMessage.PropertyEntry(name: "controlRevision", value: .i64(9)),
        ])))
        await commands.receive(Self.answer("slice.release", id: 4, accepted: true))
        #expect(await release.value == .accepted(controlRevision: nil))
    }

    @Test func theVerbsMatchTheSurface() throws {
        let surface = try LinkFixtureLoader.jsonObject(at: LinkFixtureLoader.root.appendingPathComponent("surface.json"))
        let commands = try #require(surface["commands"] as? [[String: Any]])
        func arguments(_ verb: String) -> [String]? {
            (commands.first { $0["verb"] as? String == verb }?["arguments"] as? [[String: Any]])?
                .compactMap { $0["name"] as? String }
        }
        #expect(arguments(SliceAccess.listenVerb) == ["sliceId", "incarnation"])
        #expect(arguments(SliceAccess.stopListeningVerb) == ["sliceId", "incarnation"])
        #expect(arguments(SliceAccess.releaseVerb) == ["sliceId", "incarnation", "controlRevision"])
        #expect(arguments(SliceAccess.setListenLevelVerb) == ["sliceId", "incarnation", "level", "muted"])
    }

    /// Every refusal the Core words for a take, shown exactly as sent. At
    /// trunk b26112687 the Core no longer sends "needs an update" or "is
    /// away" for a take (the contract note, "Refusal words"): a take from
    /// under an older app or an away device goes through.
    nonisolated static let takeRefusals = [
        "That slice has closed. Choose it again from the list.",
        "Someone else changed who controls slice B. Look again and try once more.",
        "Slice B is transmitting. Take control once it stops.",
        "Slice A is run by the Core itself, so control of it cannot pass to this device.",
        "The Core has no radio ready.",
        "This Core cannot share slices between devices.",
        "Update this app to listen to and take slices on this Core.",
        "The Core could not read this request.",
    ]

    @Test(arguments: takeRefusals)
    func aRefusedTakeCarriesTheCoresWordsAsSent(_ words: String) async throws {
        let commands = CommandClient(send: sent.sender)
        await ready(commands)
        let state = SliceAccess.State(sliceId: 1, incarnation: 3, controllerDeviceId: "mac", controlRevision: 12)
        let task = Task { await SliceAccess.takeControl(state, commands: commands) }
        #expect(await sent.settle(untilCount: 1))
        await commands.receive(Self.answer("slice.takeControl", id: 1, accepted: false, reason: words))
        #expect(await task.value == .refused(words))
    }

    /// The Core's own trace: its accepted take and its unreadable one
    /// (tests/data/link/v1/sessions/slice-access.json), read as the phone reads them.
    @Test func theCoresTraceReadsAsAnAcceptedAndARefusedTake() throws {
        let fixture = try LinkFixtureLoader.jsonObject(
            at: LinkFixtureLoader.root.appendingPathComponent("sessions/slice-access.json"))
        let steps = try #require(fixture["steps"] as? [[String: Any]])
        let results = steps.compactMap { $0["message"] as? [String: Any] }.filter {
            $0["type"] as? String == "command.result" && $0["verb"] as? String == "slice.takeControl"
        }
        #expect(results.count == 2)
        for result in results {
            let accepted = try #require(result["accepted"] as? Bool)
            let reason = try #require(result["reason"] as? String)
            var values: [String: MirrorValue] = [:]
            for value in result["values"] as? [[String: Any]] ?? [] where value["name"] as? String == "controlRevision" {
                #expect(value["kind"] as? String == "i64")
                // The trace holds the revision as a placeholder; any whole number stands for it.
                values["controlRevision"] = .int(12)
            }
            let outcome = SliceAccess.outcome(CommandResult(accepted: accepted, reason: reason, affectedKeys: [],
                                                            values: values, phase: nil))
            #expect(outcome == (accepted ? .accepted(controlRevision: 12)
                                         : .refused("The Core could not read this request.")))
        }
    }

    // MARK: Take back on controlTaken

    private static func controlTaken(id: Int64 = 41, takeBack: Bool, v2: Bool = true) -> LinkMessage {
        var slice: [String: LinkJSON] = ["sliceId": .number(1), "letter": .string("B"), "frequencyHz": .number(7_249_000),
                                         "mode": .number(0), "band": .number(5)]
        if v2 {
            slice["incarnation"] = .number(3)
            slice["controlRevision"] = .number(12)
        }
        return .notice(LinkMessage.Notice(id: id, kind: "controlTaken",
                                          reason: "Shack phone took control of slice B. You are still listening.",
                                          secondsAgo: 0, takeBack: takeBack, byDeviceId: "shack-phone",
                                          byName: "Shack phone", byShortName: "Shack phone", byKind: "phone",
                                          bySource: "device", slices: [.object(slice)]))
    }

    private func devices(_ store: MirrorStore, _ commands: CommandClient) async -> SeveralDevicesClient {
        let client = SeveralDevicesClient(store: store, settings: SettingsProxyClient(send: sent.sender),
                                          commands: commands)
        await ready(commands)
        client.handle(.stateChanged(.receivingSnapshot))
        client.handle(.stateChanged(.ready))
        return client
    }

    @Test func aControlTakenNoticeReadsItsSliceEntry() throws {
        guard case .notice(let wire) = Self.controlTaken(takeBack: true) else {
            return
        }
        let notice = SeveralDevices.Notice(wire)
        #expect(notice.kind == .controlTaken)
        #expect(notice.reason == "Shack phone took control of slice B. You are still listening.")
        #expect(notice.slices.first?.incarnation == 3)
        #expect(notice.slices.first?.controlRevision == 12)
        #expect(notice.by?.name == "Shack phone")
        guard case .notice(let older) = Self.controlTaken(takeBack: false, v2: false) else {
            return
        }
        #expect(SeveralDevices.Notice(older).slices.first?.incarnation == nil)
        #expect(SeveralDevices.Notice(older).slices.first?.controlRevision == nil)
    }

    @Test func takeBackIsGreyedWithTheCoresReasonWhereItCannotWork() async {
        let v1 = await devices(store(version: 1), CommandClient(send: sent.sender))
        v1.receive(Self.controlTaken(takeBack: false, v2: false))
        let told = v1.notices[0]
        #expect(v1.takeBackUnavailableReason(told)
                == "This Core cannot give control back from here. Updating the Core may help.")
        await v1.takeBack(41)
        #expect(sent.count == 0)

        let v2 = await devices(store(version: 2), CommandClient(send: sent.sender))
        v2.receive(Self.controlTaken(takeBack: false))
        #expect(v2.takeBackUnavailableReason(v2.notices[0]) == "That can no longer be taken back.")
        v2.receive(Self.controlTaken(id: 42, takeBack: true))
        #expect(v2.takeBackUnavailableReason(v2.notices[1]) == nil)
    }

    @Test func anAcceptedTakeBackTakesTheCardDownAndMarksTheSlice() async throws {
        let store = store(version: 2)
        store.apply(Self.access(controller: "shack-phone"))
        let commands = CommandClient(send: sent.sender)
        let devices = await devices(store, commands)
        var tookBack: [(Int, Int64?)] = []
        devices.tookControlBack = { tookBack.append(($0, $1)) }
        devices.receive(Self.controlTaken(takeBack: true))
        let task = Task { await devices.takeBack(41) }
        #expect(await sent.settle(untilCount: 1))
        #expect(sent.messages.last == .commandInvoke(LinkMessage.CommandInvoke(verb: "notice.takeBack", id: 1, args: [
            LinkMessage.PropertyEntry(name: "id", value: .i64(41)),
        ])))
        #expect(devices.notices.first?.takingBack == true)
        await commands.receive(Self.answer("notice.takeBack", id: 1, accepted: true,
                                           values: [.init(name: "controlRevision", value: .i64(13))]))
        await task.value
        #expect(devices.notices.isEmpty)
        #expect(devices.endedTakeBack == nil)
        #expect(tookBack.map(\.0) == [1] && tookBack.map(\.1) == [13])
    }

    @Test func aTakeBackRefusedWhileTheSliceTransmitsKeepsTheCard() async throws {
        let store = store(version: 2)
        store.apply(Self.access(controller: "shack-phone"))
        let commands = CommandClient(send: sent.sender)
        let devices = await devices(store, commands)
        devices.receive(Self.controlTaken(takeBack: true))
        let words = "Slice B is transmitting. Take control once it stops."
        let task = Task { await devices.takeBack(41) }
        #expect(await sent.settle(untilCount: 1))
        await commands.receive(Self.answer("notice.takeBack", id: 1, accepted: false, reason: words))
        await task.value
        #expect(devices.notices.first?.takeBackRefusal == words)
        #expect(devices.notices.first?.takingBack == false)
        #expect(devices.endedTakeBack == nil)
        // It may be tried again: the same id goes again.
        let again = Task { await devices.takeBack(41) }
        #expect(await sent.settle(untilCount: 2))
        await commands.receive(Self.answer("notice.takeBack", id: 2, accepted: true))
        await again.value
        #expect(devices.notices.isEmpty)
    }

    @Test(arguments: [
        ("Someone else changed who controls slice B. Look again and try once more.", true),
        ("That slice has closed. Choose it again from the list.", false),
        ("That can no longer be taken back.", true),
    ])
    func aTakeBackRefusedForGoodTakesTheCardDownAndShowsTheWords(_ words: String, sliceLives: Bool) async throws {
        let store = store(version: 2)
        if sliceLives {
            // Control moved on since the notice: the revision is past the entry's.
            store.apply(Self.access(controller: "garden-pad", revision: 14))
        }
        let commands = CommandClient(send: sent.sender)
        let devices = await devices(store, commands)
        devices.receive(Self.controlTaken(takeBack: true))
        let task = Task { await devices.takeBack(41) }
        #expect(await sent.settle(untilCount: 1))
        await commands.receive(Self.answer("notice.takeBack", id: 1, accepted: false, reason: words))
        await task.value
        #expect(devices.notices.isEmpty)
        #expect(devices.endedTakeBack?.text == words)
        devices.dismissEndedTakeBack()
        #expect(devices.endedTakeBack == nil)
    }

    @Test func aTakeBackTheCoreNeverAnsweredKeepsTheCardWithThePhonesWords() async throws {
        let store = store(version: 2)
        store.apply(Self.access(controller: "shack-phone"))
        let commands = CommandClient(send: sent.sender)
        let devices = await devices(store, commands)
        devices.receive(Self.controlTaken(takeBack: true))
        let task = Task { await devices.takeBack(41) }
        #expect(await sent.settle(untilCount: 1))
        await commands.handle(.stateChanged(.waitingToRetry(seconds: 1)))
        await task.value
        #expect(devices.notices.first?.takeBackRefusal == SeveralDevicesClient.noAnswerText)
    }
}
