// NereusSDR for iOS: Setup rows from description versions 13 to 16 through the Setup dispatcher
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import LinkSessionTestSupport
import NereusLink
import Testing
@testable import NereusMirror

/// R-IOS-18: a version 16 Hardware page's formats, offsets, named choices, held
/// rows, gates and the radio's own settings, and the rows that stay greyed
/// with a plain reason.
/// The phone's own kept unit for the PA temperature, as the app keeps it.
@MainActor
final class TemperatureUnitKeys: SetupPhoneKeys {
    var unit: String? = "C"
    private let subject = PassthroughSubject<Void, Never>()
    func value(forPhoneKey key: String) -> SetupValue? {
        key == "PaTempUnit" ? unit.map { .text($0) } : nil
    }
    func set(_ value: SetupValue, forPhoneKey key: String) -> Bool {
        guard key == "PaTempUnit", unit != nil, let text = value.text, text == "C" || text == "F" else { return false }
        unit = text
        subject.send()
        return true
    }
    var changes: AnyPublisher<Void, Never> { subject.eraseToAnyPublisher() }
    func canPerform(_ action: String) -> Bool { false }
    func perform(_ action: String) -> Bool { false }
}

@MainActor
@Suite struct SetupControlDispatcherModernTests {
    static func row(_ body: String) -> String {
        let applies = body.contains(#""applies":"#) ? "" : #""applies":"live","#
        return #"{"tooltip":"t","# + applies + #""requiresDescriptionVersion":16,"# + body + "}"
    }

    static let rows: [String] = [
        row(#""id":"g.flag","label":"Flag","kind":"readout","format":"enabledOff","binding":{"property":{"object":"radio","name":"flag"}}"#),
        row(#""id":"g.limit","label":"Limit","kind":"readout","format":"nnrLimit","binding":{"property":{"object":"radio","name":"limit"}}"#),
        row(#""id":"g.odd","label":"Odd","kind":"readout","format":"somethingNew","binding":{"property":{"object":"radio","name":"limit"}}"#),
        row(#""id":"g.info","label":"Board","kind":"readout","value":"Sample","binding":{"radioInfo":"board"}"#),
        row(#""id":"g.offset","label":"Offset","kind":"integer","min":10,"max":40,"step":1,"valueOffset":10,"binding":{"property":{"object":"radio","name":"shift"}}"#),
        row(#""id":"g.named","label":"Named","kind":"choice","choicesFrom":{"jsonNames":{"object":"radio","name":"names"}},"binding":{"property":{"object":"radio","name":"chosen"}}"#),
        row(#""id":"g.model","label":"Model","kind":"choice","options":[{"label":"Standard","value":"std.onnx"}],"choicesFrom":{"dspAssets":"nr3"},"binding":{"property":{"object":"radio","name":"model"}}"#),
        row(#""id":"g.held","label":"Name","kind":"text","applies":"staged","maxLength":4,"binding":{"property":{"object":"radio","name":"label"}}"#),
        row(#""id":"g.send","label":"Send","kind":"button","binding":{"command":{"verb":"sendName","arguments":{"name":{"$control":"g.held"}}}}"#),
        row(#""id":"g.mic","label":"Mic","kind":"toggle","gate":{"micLine":true},"binding":{"property":{"object":"radio","name":"flag"}}"#),
        row(#""id":"g.drive","label":"Drive","kind":"integer","rangeFrom":{"catalogueTransmit":"drive"},"binding":{"property":{"object":"radio","name":"shift"}}"#),
        row(#""id":"g.point","label":"10 W","kind":"decimal","min":0,"max":100,"step":0.1,"decimals":1,"default":10.0,"boardClass":1,"binding":{"radioSetting":"paCalibration/point1"}"#),
        row(#""id":"g.notch","label":"Visual Notch","kind":"toggle","binding":{"phone":"NotchVisualEnabled"}"#),
        row(#""id":"g.presets","label":"Presets","kind":"table","binding":{"filterPresets":{}}"#),
        row(#""id":"g.profile","label":"Profile","kind":"choice","options":[{"label":"A","value":0}],"binding":{"paProfile":{}}"#),
        row(#""id":"g.save","label":"Save","kind":"button","binding":{"command":{"verb":"saveAs","arguments":{"name":{"$prompt":true}}}}"#),
        row(#""id":"g.temp","label":"PA","kind":"readout","decimals":1,"temperatureUnit":"PaTempUnit","binding":{"property":{"object":"radio","name":"limit"}}"#),
        row(#""id":"g.paTemp","label":"PA Temp","kind":"readout","format":"paTemperature","binding":{"property":{"object":"radio","name":"limit"}}"#),
        row(#""id":"g.delete","label":"Delete","kind":"button","confirm":"Delete profile \"%1\"?","binding":{"command":{"verb":"deleteProfile","arguments":{"name":{"$property":{"object":"radio","name":"chosen"}}}}}"#),
        row(#""id":"g.unnamed","label":"Forget","kind":"button","confirm":"Forget \"%1\"?","binding":{"command":{"verb":"forget","arguments":{"name":{"$property":{"object":"radio","name":"absent"}}}}}"#),
        row(#""id":"g.switch","label":"Switch","kind":"choice","options":[{"label":"A","value":0}],"unsavedChanges":"activeProfile","binding":{"property":{"object":"radio","name":"shift"}}"#),
    ]

    static var hardware: String {
        #"{"version":16,"category":{"id":"hardware","title":"Hardware","where":"mixed"},"pages":[{"id":"g","title":"All","where":"mixed","sections":[{"title":"All","controls":["#
            + rows.joined(separator: ",") + "]}]}]}"
    }

    /// A second category, so a change to one can be told from the other.
    static func audio(label: String = "Level") -> String {
        // Audio is at version 15 when the Core describes up to 16.
        #"{"version":15,"category":{"id":"audio","title":"Audio","where":"mixed"},"pages":[{"id":"a","title":"All","where":"mixed","sections":[{"title":"All","controls":[{"id":"a.level","label":"\#(label)","tooltip":"t","kind":"readout","applies":"live","requiresDescriptionVersion":15,"binding":{"property":{"object":"radio","name":"limit"}}}]}]}]}"#
    }

    @MainActor final class Rig {
        let route = TestRoute()
        let selection = CurrentValueSubject<Int?, Never>(nil)
        let store: MirrorStore
        let settings: SettingsProxyClient
        let commands: CommandClient
        let feed: SetupDescriptionFeed
        let dispatcher: SetupControlDispatcher

        init(phone: (any SetupPhoneKeys)? = nil) {
            let route = route
            store = MirrorStore(send: { _ in })
            settings = SettingsProxyClient(send: { _ in }, captureSender: { route.capture() })
            commands = CommandClient(send: { _ in }, captureSender: { route.capture() })
            feed = SetupDescriptionFeed(store: store)
            let selection = selection
            dispatcher = SetupControlDispatcher(store: store, feed: feed, settings: settings, commands: commands,
                                                captureSender: { route.capture() }, selectedSlice: { selection.value },
                                                selectionChanges: selection.eraseToAnyPublisher(), phone: phone)
        }

        func event(_ event: StationSession.Event) async {
            store.handle(event)
            settings.handle(event)
            await commands.handle(event)
        }

        func connect(settingsValues: [String: String] = ["NotchVisualEnabled": "False"]) async {
            route.use(1)
            await event(.stateChanged(.receivingSnapshot))
            let messages: [LinkMessage] = [
                FixtureReplay.stationHello(minor: 11), FixtureReplay.accepted,
                .capabilities(.init(properties: [
                    .init(name: "setupDescriptionVersion", value: .i64(16)),
                    .init(name: "radioConnected", value: .bool(true)),
                    .init(name: "macAddress", value: .utf8("AA:BB:CC:DD:EE:01")),
                ])),
                .schema(.init(className: "SetupDescription", fields: [
                    .init(ordinal: 0, name: "revision", kind: .i64), .init(ordinal: 1, name: "hardware", kind: .utf8),
                    .init(ordinal: 2, name: "audio", kind: .utf8),
                ])),
                .objectCreate(.init(key: "setup", className: "SetupDescription", properties: [
                    .init(ordinal: 0, name: "revision", value: .i64(1)),
                    .init(ordinal: 1, name: "hardware", value: .utf8(SetupControlDispatcherModernTests.hardware)),
                    .init(ordinal: 2, name: "audio", value: .utf8(SetupControlDispatcherModernTests.audio())),
                ])),
                .objectCreate(.init(key: "radio", className: "RadioModel", properties: [
                    .init(name: "flag", value: .bool(true)), .init(name: "limit", value: .i64(0)),
                    .init(name: "shift", value: .i64(5)), .init(name: "names", value: .utf8(#"["Alpha","Beta"]"#)),
                    .init(name: "chosen", value: .utf8("Beta")), .init(name: "model", value: .utf8("gone.onnx")),
                    .init(name: "label", value: .utf8("ab")),
                ])),
                .objectCreate(.init(key: "txState", className: "TransmitState", properties: [
                    .init(name: "keyed", value: .bool(false)), .init(name: "tuning", value: .bool(false)),
                    .init(name: "twoTone", value: .bool(false)), .init(name: "txEnding", value: .bool(false)),
                ])),
                .settingsSnapshot(.init(properties: settingsValues.sorted { $0.key < $1.key }
                    .map { .init(name: $0.key, value: .utf8($0.value)) })),
                .snapshotComplete,
            ]
            for message in messages { await event(.message(message)) }
            await event(.stateChanged(.ready))
            for _ in 0..<5 { await Task.yield() }
        }

        func control(_ id: String) throws -> SetupDescription.Control {
            let description = try #require(feed.description(for: "hardware"))
            return try #require(description.pages.flatMap(\.sections).flatMap(\.controls).first { $0.id == id })
        }

        func state(_ id: String) throws -> SetupControlState {
            dispatcher.state(of: try control(id), in: "hardware")
        }

        /// Publishes `category` as `json` and waits for the feed to read it.
        func publish(_ category: String, _ json: String, revision: Int64) async {
            let before = feed.generation
            store.apply(.delta(.init(key: "setup", properties: [
                .init(name: category, value: .utf8(json)), .init(name: "revision", value: .i64(revision)),
            ])))
            for await generation in feed.$generation.values where generation != before {
                break
            }
        }
    }

    @Test func readoutsUseTheDesktopsWordsAndUnknownFormatsShowThePlainValue() async throws {
        let rig = Rig()
        await rig.connect()
        #expect(rig.feed.description(for: "hardware")?.version == 16)
        #expect(try rig.state("g.flag").value == .text("enabled"))
        #expect(try rig.state("g.limit").value == .text(SetupControlDispatcher.noLimitText))
        #expect(try rig.state("g.odd").value == .integer(0))
        #expect(try rig.state("g.info").value == .text("Sample"))
        #expect(try rig.state("g.flag").editable == false)
    }

    @Test func aShownOffsetIsAddedToTheReadingAndTakenOffTheWrite() async throws {
        let rig = Rig()
        await rig.connect()
        #expect(try rig.state("g.offset").value == .integer(15))
        let writing = Task { await rig.dispatcher.edit(try! rig.control("g.offset"), in: "hardware", to: .integer(20)) }
        #expect(await rig.route.waitForCount(1, unless: writing))
        guard case .propertyWrite(let write) = rig.route.sent[0].message else { Issue.record("no write"); return }
        #expect(write.key == "radio" && write.properties.first?.value == .i64(10))
        rig.store.apply(.delta(.init(key: "radio", properties: [.init(name: "shift", value: .i64(10))])))
        #expect(await writing.value == .applied)
    }

    @Test func namedChoicesComeFromTheCoresListAndAMissingModelIsShownByName() async throws {
        let rig = Rig()
        await rig.connect()
        let named = rig.dispatcher.resolved(try rig.control("g.named"))
        #expect(named.options?.map(\.label) == ["Alpha", "Beta"])
        #expect(try rig.state("g.named").value == .integer(1))
        let choosing = Task { await rig.dispatcher.edit(try! rig.control("g.named"), in: "hardware", to: .integer(0)) }
        #expect(await rig.route.waitForCount(1, unless: choosing))
        guard case .propertyWrite(let write) = rig.route.sent[0].message else { Issue.record("no write"); return }
        #expect(write.properties.first?.value == .utf8("Alpha"))
        rig.store.apply(.delta(.init(key: "radio", properties: [.init(name: "chosen", value: .utf8("Alpha"))])))
        #expect(await choosing.value == .applied)

        let model = rig.dispatcher.resolved(try rig.control("g.model"))
        #expect(model.options?.map(\.label) == ["Standard", SetupControlDispatcher.missingModelText])
        #expect(try rig.state("g.model").value == .integer(1))
    }

    /// The NR3 model row lists, after its own options, the valid NR3 models
    /// from this session's dspAssets.list (setup description, `choicesFrom`
    /// `{"dspAssets":"nr3"}`), asked once, labelled as the desktop's NR3
    /// picker labels them; the chosen model is sent as its id.
    @Test func theNr3ModelRowListsTheCoresModelsFromOneListPerSession() async throws {
        let rig = Rig()
        await rig.connect()
        let model = try rig.control("g.model")
        #expect(rig.dispatcher.resolved(model).options?.map(\.label) == ["Standard", SetupControlDispatcher.missingModelText])
        let asking = try #require(rig.dispatcher.nr3List.task)
        await rig.route.waitForCount(1)
        guard case .commandInvoke(let list) = rig.route.sent[0].message else { Issue.record("no list"); return }
        #expect(list.verb == "dspAssets.list" && list.args.isEmpty)
        // Asked once: drawing the row again asks nothing more.
        _ = rig.dispatcher.resolved(model)
        #expect(rig.route.sent.count == 1)

        let long = "sha256:0123456789abcdef0123456789abcdef"
        let assets = """
            [{"id":"wide.rnn","kind":2,"label":"Wide","valid":true},
             {"id":"\(long)","kind":2,"label":"","valid":true},
             {"id":"nnr.bin","kind":0,"label":"Other kind","valid":true},
             {"id":"bad.rnn","kind":2,"label":"Broken","valid":false},
             {"id":"gone.onnx","kind":2,"label":"Gone","valid":true}]
            """
        await rig.commands.receive(.commandResult(.init(verb: list.verb, id: list.id, accepted: true, reason: "",
                                                        affected: [], values: [.init(name: "assets",
                                                                                     value: .utf8(assets))])))
        await asking.value
        let listed = rig.dispatcher.resolved(model)
        #expect(listed.options?.map(\.label) == ["Standard", "Wide", "sha256:0123456789abcdef\u{2026}", "Gone"])
        #expect(try rig.state("g.model").value == .integer(3))
        #expect(rig.route.sent.count == 1)

        let choosing = Task { await rig.dispatcher.edit(model, in: "hardware", to: .integer(2)) }
        #expect(await rig.route.waitForCount(2, unless: choosing))
        guard case .propertyWrite(let write) = rig.route.sent[1].message else { Issue.record("no write"); return }
        #expect(write.properties.first?.value == .utf8(long))
        rig.store.apply(.delta(.init(key: "radio", properties: [.init(name: "model", value: .utf8(long))])))
        #expect(await choosing.value == .applied)
    }

    @Test func aHeldRowWaitsForItsButtonAndTooLongTextIsRefused() async throws {
        let rig = Rig()
        await rig.connect()
        let held = try rig.control("g.held")
        #expect(await rig.dispatcher.edit(held, in: "hardware", to: .text("abcdef"))
                == .notSent(SetupControlDispatcher.tooLongReason))
        #expect(await rig.dispatcher.edit(held, in: "hardware", to: .text("abcd")) == .applied)
        #expect(rig.route.sent.isEmpty)
        #expect(try rig.state("g.held").value == .text("abcd"))
        let pressing = Task { await rig.dispatcher.edit(try! rig.control("g.send"), in: "hardware", to: nil) }
        #expect(await rig.route.waitForCount(1, unless: pressing))
        guard case .commandInvoke(let invoke) = rig.route.sent[0].message else { Issue.record("no verb"); return }
        #expect(invoke.verb == "sendName" && invoke.args == [.init(name: "name", value: .utf8("abcd"))])
        await rig.commands.receive(.commandResult(.init(verb: invoke.verb, id: invoke.id, accepted: true, reason: "",
                                                        affected: [])))
        #expect(await pressing.value == .applied)
    }

    /// A held value belongs to its category's description: a change to
    /// another category, or another slice chosen for a row that is not a
    /// slice's, keeps it; a change to its own category drops it.
    @Test func aHeldValueStaysThroughAnotherCategorysChangeAndGoesWithItsOwn() async throws {
        let rig = Rig()
        await rig.connect()
        #expect(await rig.dispatcher.edit(try rig.control("g.held"), in: "hardware", to: .text("abcd")) == .applied)

        #expect(rig.feed.description(for: "audio")?.pages[0].sections[0].controls[0].label == "Level")
        await rig.publish("audio", Self.audio(label: "Output level"), revision: 2)
        #expect(rig.feed.description(for: "audio")?.pages[0].sections[0].controls[0].label == "Output level")
        #expect(try rig.state("g.held").value == .text("abcd"))

        rig.selection.send(1)
        #expect(try rig.state("g.held").value == .text("abcd"))

        let changed = Self.hardware.replacingOccurrences(of: #""label":"Flag""#, with: #""label":"Flags""#)
        await rig.publish("hardware", changed, revision: 3)
        #expect(try rig.control("g.flag").label == "Flags")
        #expect(try rig.state("g.held").value == .text("ab"))
        #expect(rig.route.sent.isEmpty)
    }

    /// The PA temperature shows in the viewer's own unit (setup
    /// description: "the renderer shows it in the viewer's own PA
    /// temperature unit ... default `C`"). `PaTempUnit` is the desktop
    /// operator's own setting, never the phone's, so a value of it in the
    /// Core's settings does not turn the phone's reading to Fahrenheit.
    @Test func thePaTemperatureIsInTheViewersOwnUnitNotTheCoresSetting() async throws {
        let rig = Rig()
        await rig.connect(settingsValues: ["NotchVisualEnabled": "False", "PaTempUnit": "F"])
        #expect(rig.settings.value("PaTempUnit") == "F")
        #expect(try rig.state("g.temp").value == .text("0.0 \u{00B0}C"))
        #expect(try rig.state("g.paTemp").value == .text("0.0 \u{00B0}C"))
    }

    /// The PA temperature's unit is chosen on the phone, beside the
    /// reading (the desktop flips it by a click on its readout, which the
    /// phone shows as a visible choice instead): Celsius until chosen,
    /// kept by the phone, and the reading follows it.
    @Test func thePaTemperatureUnitIsChosenOnThePhoneAndTheReadingFollowsIt() async throws {
        let keys = TemperatureUnitKeys()
        let rig = Rig(phone: keys)
        await rig.connect(settingsValues: ["NotchVisualEnabled": "False", "PaTempUnit": "F"])
        let temp = try rig.control("g.temp")
        #expect(rig.dispatcher.temperatureUnitKey(of: temp) == "PaTempUnit")
        #expect(rig.dispatcher.temperatureUnitKey(of: try rig.control("g.paTemp")) == "PaTempUnit")
        #expect(rig.dispatcher.temperatureUnitKey(of: try rig.control("g.flag")) == nil)
        #expect(rig.dispatcher.temperatureInFahrenheit(temp) == false)
        #expect(try rig.state("g.temp").value == .text("0.0 \u{00B0}C"))

        #expect(rig.dispatcher.setTemperatureInFahrenheit(true, for: temp))
        #expect(keys.unit == "F")
        #expect(rig.dispatcher.temperatureInFahrenheit(temp) == true)
        #expect(try rig.state("g.temp").value == .text("32.0 \u{00B0}F"))
        #expect(try rig.state("g.paTemp").value == .text("32.0 \u{00B0}F"))
        #expect(rig.route.sent.isEmpty)

        // A phone that keeps no unit: no choice to make, and Celsius.
        keys.unit = nil
        #expect(rig.dispatcher.temperatureInFahrenheit(temp) == nil)
        #expect(!rig.dispatcher.setTemperatureInFahrenheit(true, for: temp))
        #expect(try rig.state("g.temp").value == .text("0.0 \u{00B0}C"))
    }

    @Test func theMicrophoneGateAndCatalogueLimitsFollowThePhone() async throws {
        let rig = Rig()
        await rig.connect()
        #expect(try rig.state("g.mic").reason == SetupControlDispatcher.micLineReason)
        rig.dispatcher.microphoneLineOpen = { true }
        #expect(try rig.state("g.mic").editable)

        #expect(try rig.state("g.drive").reason == SetupControlDispatcher.catalogueWaitingReason)
        rig.dispatcher.catalogueTransmitRange = { $0 == "drive" ? .init(minimum: 0, maximum: 100, step: 1) : nil }
        #expect(try rig.state("g.drive").editable)
        #expect(rig.dispatcher.resolved(try rig.control("g.drive")).range?.maximum == 100)
    }

    @Test func aCalibrationPointNamesItsModelFirstAndAnotherModelsPointIsGreyed() async throws {
        let rig = Rig()
        await rig.connect()
        let point = try rig.control("g.point")
        #expect(try rig.state("g.point") == .init(value: .decimal(10), editable: true, reason: nil))
        let writing = Task { await rig.dispatcher.edit(point, in: "hardware", to: .decimal(12.5)) }
        #expect(await rig.route.waitForCount(1, unless: writing))
        guard case .settingsWrite(let first) = rig.route.sent[0].message else { Issue.record("no class"); return }
        #expect(first.key == "hardware/AA:BB:CC:DD:EE:01/paCalibration/boardClass")
        #expect(first.properties.first?.value == .utf8("1"))
        rig.settings.apply(.settingsValue(.init(key: first.key, origin: first.origin, properties: first.properties)))
        #expect(await rig.route.waitForCount(2, unless: writing))
        guard case .settingsWrite(let second) = rig.route.sent[1].message else { Issue.record("no point"); return }
        #expect(second.key == "hardware/AA:BB:CC:DD:EE:01/paCalibration/point1")
        rig.settings.apply(.settingsValue(.init(key: second.key, origin: second.origin, properties: second.properties)))
        #expect(await writing.value == .applied)

        let other = Rig()
        await other.connect(settingsValues: ["hardware/AA:BB:CC:DD:EE:01/paCalibration/boardClass": "2"])
        #expect(try other.state("g.point").reason == SetupControlDispatcher.otherModelReason)
    }

    /// A button's question names what it is about (setup description,
    /// `confirm` with `%1`: the value the command's first text argument
    /// resolves to), read when it asks. Yes sends only while that is still
    /// what the question named; a question with nothing to name is not asked.
    @Test func aQuestionNamesItsProfileWhenItAsksAndYesSendsOnlyThatProfile() async throws {
        let rig = Rig()
        await rig.connect()
        let delete = try rig.control("g.delete")
        let asked = try #require(rig.dispatcher.question(for: delete, in: "hardware", value: nil)).get()
        #expect(asked.text == #"Delete profile "Beta"?"#)

        // The profile changes before Yes: nothing is sent.
        rig.store.apply(.delta(.init(key: "radio", properties: [.init(name: "chosen", value: .utf8("Alpha"))])))
        #expect(await rig.dispatcher.edit(delete, in: "hardware", to: nil, asked: asked)
                == .notSent(SetupControlDispatcher.changedFirstReason))
        #expect(rig.route.sent.isEmpty)

        // Asked again, Yes sends the profile the question named.
        let again = try #require(rig.dispatcher.question(for: delete, in: "hardware", value: nil)).get()
        #expect(again.text == #"Delete profile "Alpha"?"#)
        let pressing = Task { await rig.dispatcher.edit(delete, in: "hardware", to: nil, asked: again) }
        #expect(await rig.route.waitForCount(1, unless: pressing))
        guard case .commandInvoke(let invoke) = rig.route.sent[0].message else { Issue.record("no verb"); return }
        #expect(invoke.verb == "deleteProfile" && invoke.args == [.init(name: "name", value: .utf8("Alpha"))])
        await rig.commands.receive(.commandResult(.init(verb: invoke.verb, id: invoke.id, accepted: true, reason: "",
                                                        affected: [])))
        #expect(await pressing.value == .applied)

        // Nothing to name: no question with a bare %1 in it.
        let unnamed = rig.dispatcher.question(for: try rig.control("g.unnamed"), in: "hardware", value: nil)
        #expect(unnamed == .failure(SetupRefusal(reason: SetupControlDispatcher.valueMissingReason)))
        // A row without a question asks nothing.
        #expect(rig.dispatcher.question(for: try rig.control("g.send"), in: "hardware", value: nil) == nil)
        #expect(rig.route.sent.count == 1)
    }

    @Test func aCalibrationPointTheCoreHoldsForAQuestionIsNotRefusedAndSendsNoMore() async throws {
        // The model's class is held for a question: the point waits with it.
        let rig = Rig()
        await rig.connect()
        let writing = Task { await rig.dispatcher.edit(try! rig.control("g.point"), in: "hardware", to: .decimal(12.5)) }
        #expect(await rig.route.waitForCount(1, unless: writing))
        guard case .settingsWrite(let first) = rig.route.sent[0].message else { Issue.record("no class"); return }
        rig.settings.apply(.settingsReject(.init(key: first.key, properties: [], reason: SeveralDevices.waitingReason)))
        let outcome = await writing.value
        #expect(outcome != .refused(SeveralDevices.waitingReason))
        #expect(outcome == .awaitingConfirmation)
        #expect(rig.route.sent.count == 1)
    }

    @Test func visualNotchIsTheCoresStationSetting() async throws {
        let rig = Rig()
        await rig.connect()
        let notch = try rig.control("g.notch")
        #expect(notch.binding == .setting("NotchVisualEnabled"))
        #expect(try rig.state("g.notch") == .init(value: .bool(false), editable: true, reason: nil))
    }

    @Test func rowsThePhoneCannotRunStayGreyedWithAPlainReason() async throws {
        let rig = Rig()
        await rig.connect()
        let expected: [(String, String)] = [
            ("g.presets", SetupDescription.filterPresetsReason),
            ("g.save", SetupDescription.promptReason),
            ("g.switch", SetupDescription.unsavedChangesReason),
        ]
        for (id, reason) in expected {
            let control = try rig.control(id)
            #expect(control.metadataIssue == nil, "\(id)")
            #expect(rig.dispatcher.state(of: control, in: "hardware").reason == reason, "\(id)")
            #expect(await rig.dispatcher.edit(control, in: "hardware", to: .integer(0)) == .notSent(reason), "\(id)")
        }
        let malformedPa = try rig.control("g.profile")
        #expect(malformedPa.metadataIssue == "Invalid PA profile metadata.")
        #expect(rig.dispatcher.state(of: malformedPa, in: "hardware").reason == SetupControlDispatcher.unreadableReason)
        #expect(rig.route.sent.isEmpty)
    }
}
