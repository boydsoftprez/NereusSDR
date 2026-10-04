// NereusSDR for iOS: Setup description 17's Alex-1 low-pass rows through the Setup dispatcher
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import LinkSessionTestSupport
import NereusLink
import Testing
@testable import NereusMirror

/// R-IOS-18: the Alex-1 Filters page's low-pass rows as the Core's
/// resource describes them. One edge is written as the Core names it, the
/// Core's neighbour moves arrive as its own settings, its refusal is shown
/// in its words, the filter in use is marked from `alexLpfBits`, and every
/// row that cannot change now says why.
@MainActor
@Suite struct SetupAlexLowPassTests {
    static let mac = "AA:BB:CC:DD:EE:01"

    /// The Core's Alex LPF Bands section, read from the checkout, in a
    /// version 17 Hardware description. `bypassClosed` marks the bypass as
    /// the Core does on the radios that do not have it.
    static func hardware(bypassClosed: Bool = false) throws -> String {
        let url = URL(fileURLWithPath: #filePath).deletingLastPathComponent()
            .appendingPathComponent("../../../../resources/setup/hardware.json").standardizedFileURL
        let root = try #require(JSONSerialization.jsonObject(with: Data(contentsOf: url)) as? [String: Any])
        let pages = try #require(root["pages"] as? [[String: Any]])
        let page = try #require(pages.first { $0["id"] as? String == "hardware.alex1Filters" })
        let sections = try #require(page["sections"] as? [[String: Any]])
        var section = try #require(sections.first { $0["title"] as? String == "Alex LPF Bands" })
        var controls = try #require(section["controls"] as? [[String: Any]])
        if bypassClosed, let index = controls.firstIndex(where: { $0["id"] as? String == bypassId }) {
            controls[index]["availability"] = ["enabled": false, "reason": bypassMissingReason]
        }
        section["controls"] = controls
        let described: [String: Any] = [
            "version": 17, "category": ["id": "hardware", "title": "Hardware", "where": "mixed"],
            "pages": [["id": "hardware.alex1Filters", "title": "Alex-1 Filters", "where": "station",
                       "sections": [section]]],
        ]
        return String(decoding: try JSONSerialization.data(withJSONObject: described), as: UTF8.self)
    }

    static let bypassId = "hardware.alex1Filters.lpfBypass"
    static let bypassMissingReason = "This radio does not have the 6m low-pass bypass on receive."
    static func edge(_ band: String, _ side: String) -> String { "hardware.alex1Filters.lpf.\(band).\(side)" }
    static func key(_ band: String, _ side: String) -> String { "hardware/\(mac)/alex/lpf/\(band)/\(side)" }

    @MainActor final class Rig {
        let route = TestRoute()
        let store: MirrorStore
        let settings: SettingsProxyClient
        let commands: CommandClient
        let feed: SetupDescriptionFeed
        let dispatcher: SetupControlDispatcher

        init() {
            let route = route
            store = MirrorStore(send: { _ in })
            settings = SettingsProxyClient(send: { _ in }, captureSender: { route.capture() })
            commands = CommandClient(send: { _ in }, captureSender: { route.capture() })
            feed = SetupDescriptionFeed(store: store)
            dispatcher = SetupControlDispatcher(store: store, feed: feed, settings: settings, commands: commands,
                                                captureSender: { route.capture() }, selectedSlice: { nil },
                                                selectionChanges: Just(nil).eraseToAnyPublisher(), phone: nil)
        }

        func event(_ event: StationSession.Event) async {
            store.handle(event)
            settings.handle(event)
            await commands.handle(event)
        }

        /// Connects to a Core at `radioHardware`, transmit allowed or not,
        /// with `bits` as `radio`'s alexLpfBits (nil: not sent).
        func connect(radioHardware: Int64 = 10, transmit: Bool = true, refusal: String? = nil,
                     bits: Int64? = 0x02, bypassClosed: Bool = false,
                     settingsValues: [String: String] = [:]) async throws {
            route.use(1)
            await event(.stateChanged(.receivingSnapshot))
            var capabilities: [LinkMessage.PropertyEntry] = [
                .init(name: "setupDescriptionVersion", value: .i64(17)),
                .init(name: "radioHardwareVersion", value: .i64(radioHardware)),
                .init(name: "radioConnected", value: .bool(true)),
                .init(name: "macAddress", value: .utf8(SetupAlexLowPassTests.mac)),
                .init(name: "txPermitted", value: .bool(transmit)),
            ]
            if let refusal {
                capabilities.append(.init(name: "txRefusalReason", value: .utf8(refusal)))
            }
            var radio: [LinkMessage.PropertyEntry] = [.init(name: "txInhibitReason", value: .utf8(""))]
            if let bits {
                radio.append(.init(name: "alexLpfBits", value: .i64(bits)))
            }
            let hardware = try SetupAlexLowPassTests.hardware(bypassClosed: bypassClosed)
            let messages: [LinkMessage] = [
                FixtureReplay.stationHello(minor: 11), FixtureReplay.accepted,
                .capabilities(.init(properties: capabilities)),
                .schema(.init(className: "SetupDescription", fields: [
                    .init(ordinal: 0, name: "revision", kind: .i64), .init(ordinal: 1, name: "hardware", kind: .utf8),
                ])),
                .objectCreate(.init(key: "setup", className: "SetupDescription", properties: [
                    .init(ordinal: 0, name: "revision", value: .i64(1)),
                    .init(ordinal: 1, name: "hardware", value: .utf8(hardware)),
                ])),
                .objectCreate(.init(key: "radio", className: "RadioModel", properties: radio)),
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
    }

    static let bands = ["160m", "80m", "40m", "20m", "15m", "10m", "6m"]

    @Test func theSevenRowsAndTheBypassAreDrawnAsTheCoreDescribesThem() async throws {
        let rig = Rig()
        try await rig.connect()
        #expect(rig.feed.description(for: "hardware")?.version == 17)
        let section = try #require(rig.feed.description(for: "hardware")?.pages.first?.sections.first)
        #expect(section.controls.map(\.id) == Self.bands.flatMap { [Self.edge($0, "start"), Self.edge($0, "end")] }
            + [Self.bypassId])
        // Each edge's range is its own, as the Core's description gives it.
        let ranges: [String: (Double, Double)] = [
            Self.edge("160m", "start"): (0, 1.999999), Self.edge("160m", "end"): (1.5, 2.5),
            Self.edge("15m", "end"): (23.000001, 25), Self.edge("6m", "end"): (50.000001, 61.44),
        ]
        for control in section.controls where control.id != Self.bypassId {
            #expect(control.metadataIssue == nil, "\(control.id)")
            #expect(control.kind == .decimal && control.unit == "MHz" && control.decimals == 6, "\(control.id)")
            #expect(control.range?.step == 0.001, "\(control.id)")
            if let (low, high) = ranges[control.id] {
                #expect(control.range?.minimum == low && control.range?.maximum == high, "\(control.id)")
            }
            #expect(try rig.state(control.id).editable, "\(control.id)")
        }
        #expect(try rig.state(Self.edge("40m", "start")).value == .decimal(5.000001))
        #expect(try rig.state(Self.edge("40m", "end")).value == .decimal(8))
        let bypass = try rig.control(Self.bypassId)
        #expect(bypass.kind == .toggle && bypass.label == "6m/ByPass on RX" && bypass.metadataIssue == nil)
        #expect(try rig.state(Self.bypassId) == .init(value: .bool(false), editable: true, reason: nil))
    }

    @Test func oneEdgeIsWrittenAsTheCoreNamesItAndItsNeighbourArrivesFromTheCore() async throws {
        let rig = Rig()
        try await rig.connect()
        let start = try rig.control(Self.edge("80m", "start"))
        let writing = Task { await rig.dispatcher.edit(start, in: "hardware", to: .decimal(1.8)) }
        #expect(await rig.route.waitForCount(1, unless: writing))
        guard case .settingsWrite(let write) = rig.route.sent[0].message else { Issue.record("no write"); return }
        #expect(write.key == Self.key("80m", "start"))
        #expect(write.properties.first?.value == .utf8("1.8"))
        rig.settings.apply(.settingsValue(.init(key: write.key, origin: write.origin, properties: write.properties)))
        #expect(await writing.value == .applied)
        // The phone moved nothing itself: the Core's move of the 160m End
        // arrives as a setting of its own, and is shown as it came.
        #expect(rig.route.sent.count == 1)
        #expect(try rig.state(Self.edge("160m", "end")).value == .decimal(2.5))
        rig.settings.apply(.settingsValue(.init(key: Self.key("160m", "end"), origin: "station",
                                                properties: [.init(name: "value", value: .utf8("1.799999"))])))
        #expect(try rig.state(Self.edge("160m", "end")).value == .decimal(1.799999))
        #expect(try rig.state(Self.edge("80m", "start")).value == .decimal(1.8))
    }

    @Test func theCoresRefusalIsShownAsSentAndAnEdgeOutsideItsRangeIsNotSent() async throws {
        let rig = Rig()
        try await rig.connect()
        let end = try rig.control(Self.edge("160m", "end"))
        #expect(await rig.dispatcher.edit(end, in: "hardware", to: .decimal(2.6))
                == .notSent(SetupControlDispatcher.outOfRangeReason))
        #expect(rig.route.sent.isEmpty)
        let refusal = "Choose the 160m low-pass end from 1.5 to 2.5 MHz."
        let writing = Task { await rig.dispatcher.edit(end, in: "hardware", to: .decimal(2.4)) }
        #expect(await rig.route.waitForCount(1, unless: writing))
        guard case .settingsWrite(let write) = rig.route.sent[0].message else { Issue.record("no write"); return }
        #expect(write.key == Self.key("160m", "end"))
        rig.settings.apply(.settingsReject(.init(key: write.key, properties: [], reason: refusal)))
        #expect(await writing.value == .refused(refusal))
    }

    @Test func theFilterInUseIsMarkedFromTheCoresBitsAndNothingIsMarkedWithoutThem() async throws {
        let rig = Rig()
        try await rig.connect(bits: 0x02)
        func lamps() throws -> [SetupControlDispatcher.LowPassLamp?] {
            try Self.bands.map { rig.dispatcher.lowPassLamp(for: try rig.control(Self.edge($0, "start"))) }
        }
        // 0x02 is the 60/40m filter.
        #expect(try lamps() == [.notInUse, .notInUse, .inUse, .notInUse, .notInUse, .notInUse, .notInUse])
        #expect(rig.dispatcher.lowPassLamp(for: try rig.control(Self.edge("40m", "end"))) == .inUse)
        #expect(rig.dispatcher.lowPassLamp(for: try rig.control(Self.bypassId)) == nil)
        // Each bit names its own row.
        let expected: [(Int64, String)] = [(0x01, "20m"), (0x04, "80m"), (0x08, "160m"), (0x10, "6m"),
                                           (0x20, "10m"), (0x40, "15m")]
        for (bit, band) in expected {
            rig.store.apply(.delta(.init(key: "radio", properties: [.init(name: "alexLpfBits", value: .i64(bit))])))
            let marked = try Self.bands.filter { rig.dispatcher.lowPassLamp(for: try rig.control(Self.edge($0, "end"))) == .inUse }
            #expect(marked == [band], "\(bit)")
        }
        // -1 before the Core's first choice: no mark at all.
        rig.store.apply(.delta(.init(key: "radio", properties: [.init(name: "alexLpfBits", value: .i64(-1))])))
        #expect(try lamps().allSatisfy { $0 == nil })
        // A Core that sends no bits marks nothing either.
        let older = Rig()
        try await older.connect(bits: nil)
        #expect(try Self.bands.allSatisfy { older.dispatcher.lowPassLamp(for: try older.control(Self.edge($0, "start"))) == nil })
    }

    @Test func rowsThatCannotChangeNowAreGreyedWithTheirReason() async throws {
        // A Core below radioHardwareVersion 10: every row and the bypass, in plain words.
        let older = Rig()
        try await older.connect(radioHardware: 9)
        for id in [Self.edge("160m", "start"), Self.edge("6m", "end"), Self.bypassId] {
            #expect(try older.state(id).reason == SetupControlDispatcher.lowPassNeedsNewerCoreReason, "\(id)")
            #expect(await older.dispatcher.edit(try older.control(id), in: "hardware", to: .bool(true))
                    == .notSent(SetupControlDispatcher.lowPassNeedsNewerCoreReason), "\(id)")
        }
        #expect(older.route.sent.isEmpty)
        // A receive-only session: the rows carry the Core's reason; the bypass stays live.
        let listening = Rig()
        let reason = "This Core is set to receive only."
        try await listening.connect(transmit: false, refusal: reason)
        #expect(try listening.state(Self.edge("20m", "start")).reason == reason)
        #expect(try listening.state(Self.edge("20m", "start")).editable == false)
        #expect(try listening.state(Self.bypassId).editable)
        // The radios without the bypass: greyed with the Core's words.
        let closed = Rig()
        try await closed.connect(bypassClosed: true)
        #expect(try closed.state(Self.bypassId).reason == Self.bypassMissingReason)
        #expect(try closed.state(Self.edge("160m", "start")).editable)
    }
}
