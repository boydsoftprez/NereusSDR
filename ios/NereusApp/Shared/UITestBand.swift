// NereusSDR for iOS: a debug build's stand-in for the Core's band and one slice, for the UI tests of touches on the band
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// Modification history (NereusSDR):
// 2026-10-24: Roundtable plan Task 17, a Roundtable room on slice A and the
// hold PTT's record for the UI tests. J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// 2026-10-25: Roundtable plan Task 18, the record of every ask to key, for
// the swipes' never-key tests. J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// 2026-10-25: Roundtable fix wave: the hold record names a quick tap's
// unkey. J.J. Boyd (KG4VCF), AI-assisted via Claude Code.

#if DEBUG
import Combine
import Foundation
import SwiftUI
import NereusLink
import NereusMedia
import NereusMirror

/// With ``argument`` on a debug build's command line (beside
/// ``ConnectionFlow/showBandArgument``), the band shows one slice, A on
/// 7.2364 MHz in a 48 kHz view, with the Core's catalogue, so the UI tests
/// can open a flag's panels and drag on the band and inside them with no
/// Core. Slice A carries an AF gain, so its Audio panel's slider moves
/// under a finger as it does with a Core. The catalogue is read at run time from the file the UI test names
/// in ``catalogueEnvironment`` (the link's conformance suite), never
/// bundled (D4). Nothing on screen reaches it, and a release build has none
/// of it.
enum UITestBand {
    static let argument = "-NereusFlagsOnBand"
    /// Beside ``argument``: a connected rotor on the Core, offered on Tools.
    static let rotorArgument = "-NereusRotorFixture"
    /// Beside ``argument``: a Core that controls rotors with none set up.
    static let noRotorArgument = "-NereusNoRotorFixture"
    /// Beside ``argument``: two spots on the band, one the Core can place (330 degrees) and one it cannot.
    static let spotsArgument = "-NereusSpotFixture"
    /// Beside ``argument``: a Core running Roundtable, live, with slice A
    /// in a room of four, a talker on the air and a message naming you.
    /// PTT takes presses with no Core, and each press and release the hold
    /// PTT makes is written in ``UITestHoldLog``.
    static let roundtableArgument = "-NereusRoundtableFixture"
    /// With ``roundtableArgument``: the station has not answered Roundtable's first question.
    static let roundtableFirstRunArgument = "-NereusRoundtableFirstRun"
    static let roundtableRoomId = "lsb-7236400"
    static let catalogueEnvironment = "NEREUS_UITEST_CATALOGUE"
    static let centreHz = 7_244_500.0
    static let spanHz = 48_000.0
    static let sliceHz = 7_236_400.0

    /// Hands the main screen's models the band, the slice and the catalogue
    /// when the launch arguments carry ``argument``.
    @MainActor
    static func show(_ app: AppModel, arguments: [String], environment: [String: String]) {
        guard arguments.contains(argument) else {
            return
        }
        let mirror = app.mirror
        let rotor = arguments.contains(rotorArgument)
        let noRotor = arguments.contains(noRotorArgument)
        let roundtable = arguments.contains(roundtableArgument)
        app.useBandFixturePropertySender { [weak mirror] write in
            guard write.key == "slice:0", write.properties.count == 1,
                  let entry = write.properties.first, entry.name == "afGain" else {
                throw LinkSendError.notConnected
            }
            await MainActor.run {
                guard let mirror else { return }
                if let id = write.writeId {
                    mirror.apply(.propertyResult(.init(key: write.key, writeId: id, results: [
                        .init(property: entry.name, accepted: true, reason: "", value: entry),
                    ])))
                }
                mirror.apply(.delta(.init(key: write.key, properties: [entry])))
            }
        }
        mirror.handle(.stateChanged(.receivingSnapshot))
        mirror.apply(.authResult(.init(accepted: true, reason: "", retryable: false)))
        mirror.apply(.hello(LinkMessage.Hello(major: 1, minor: 11, settingsSchema: 0, peer: "nereusd")))
        mirror.apply(.capabilities(LinkMessage.Capabilities(properties: [
            .init(ordinal: 0, name: BandSlicesModel.remoteTxCapability, value: .i64(1)),
            .init(ordinal: 1, name: "propertyResultVersion", value: .i64(1)),
            .init(ordinal: 2, name: "stationCatalogVersion", value: .i64(1)),
        ] + (rotor || noRotor ? [.init(ordinal: 3, name: RotorModel.capability, value: .i64(1))] : [])
          + (roundtable ? [.init(ordinal: 4, name: RoundtableModel.capabilityName, value: .i64(1))] : []))))
        if let path = environment[catalogueEnvironment], var json = catalogueJSON(path: path) {
            if rotor {
                // The rotor set up, so the Core offers it.
                json = json.replacingOccurrences(of: #""id":"rotor","label":"Rotor","offered":false"#,
                                                 with: #""id":"rotor","label":"Rotor","offered":true"#)
            }
            mirror.apply(.objectCreate(LinkMessage.ObjectCreate(key: CatalogFeed.objectKey, className: "StationCatalog",
                                                               properties: [
                .init(ordinal: 0, name: "json", value: .utf8(json)),
                .init(ordinal: 1, name: "revision", value: .i64(1)),
            ])))
        }
        mirror.apply(.objectCreate(LinkMessage.ObjectCreate(key: "slice:0", className: "SliceModel", properties: [
            .init(ordinal: 1, name: "frequency", value: .f64(sliceHz)),
            .init(ordinal: 2, name: "dspMode", value: .enumeration(0)),
            .init(ordinal: 3, name: "filterLow", value: .i64(-3000)),
            .init(ordinal: 4, name: "filterHigh", value: .i64(-100)),
            .init(ordinal: 6, name: "stepHz", value: .i64(100)),
            .init(ordinal: 7, name: "afGain", value: .i64(37)),
            .init(ordinal: 9, name: "rxAntenna", value: .utf8("ANT1")),
            .init(ordinal: 10, name: "txAntenna", value: .utf8("ANT1")),
            .init(ordinal: 11, name: "active", value: .bool(true)),
            .init(ordinal: 12, name: "txSlice", value: .bool(true)),
            .init(ordinal: 13, name: "sliceIndex", value: .i64(0)),
            .init(ordinal: 15, name: "signalStrengthDbm", value: .f64(-72)),
            .init(ordinal: 28, name: "sampleRateHz", value: .i64(192_000)),
            .init(ordinal: 35, name: "locked", value: .bool(false)),
        ])))
        if rotor {
            mirror.apply(.objectCreate(LinkMessage.ObjectCreate(key: RotorModel.objectKey, className: "RotorModel",
                                                               properties: rotorProperties)))
        } else if noRotor {
            mirror.apply(.objectCreate(LinkMessage.ObjectCreate(key: RotorModel.objectKey, className: "RotorModel",
                                                               properties: [
                .init(ordinal: 0, name: "connectionPhase", value: .enumeration(0)),
                .init(ordinal: 1, name: "driver", value: .enumeration(0)),
            ])))
        }
        if roundtable {
            let now = Int64(Date().timeIntervalSince1970 * 1000)
            mirror.apply(.objectCreate(LinkMessage.ObjectCreate(key: RoundtableModel.objectKey,
                                                               className: "RoundtableState", properties: [
                .init(ordinal: 0, name: "linkState", value: .enumeration(2)),
                .init(ordinal: 1, name: "linkSince", value: .i64(now - 600_000)),
                .init(ordinal: 2, name: "firstRunAnswered", value: .bool(!arguments.contains(roundtableFirstRunArgument))),
                .init(ordinal: 3, name: "hidden", value: .bool(false)),
                .init(ordinal: 4, name: "mentionCounts", value: .utf8(#"{"\#(roundtableRoomId)":1}"#)),
                .init(ordinal: 5, name: "sliceRooms", value: .utf8(#"{"0":"\#(roundtableRoomId)"}"#)),
            ])))
        }
        mirror.apply(.snapshotComplete)
        mirror.handle(.stateChanged(.ready))
        if roundtable {
            holdLog = UITestHoldLog()
            let keys = UITestKeyLog()
            keyLog = keys
            app.main.transmit.keyObserver = { [weak keys] ask in keys?.record(ask) }
            let now = Int64(Date().timeIntervalSince1970 * 1000)
            // The model wants the band list and slice A's room once it reads the object.
            _ = app.roundtable
            app.records.apply(LinkMessage.RecordBatch(stream: RoundtableModel.roomsStream, generation: 1, reset: true,
                                                      upserts: [roundtableSummary(now: now)], removes: []))
            app.records.apply(LinkMessage.RecordBatch(stream: RoundtableModel.roomStreamName(roundtableRoomId),
                                                      generation: 1, reset: true,
                                                      upserts: roundtableRecords(now: now), removes: []))
        }
        if arguments.contains(spotsArgument) {
            app.records.apply(LinkMessage.RecordBatch(stream: RecordStreamClient.spotsStream, generation: 1, reset: true,
                                                      upserts: spotRecords, removes: []))
        }
        let band = app.main.band
        band.endpointId = 1
        let payload: [String: LinkJSON] = [
            "op": .string("context"), "connectionId": .string("uitest"), "endpointId": .number(1),
            "revision": .number(1), "contextGeneration": .number(1), "sourceStream": .number(0),
            "sourceCentreHz": .number(centreHz), "sampleRateHz": .number(192_000), "centreHz": .number(centreHz),
            "spanHz": .number(spanHz), "wideCentreHz": .number(0), "wideSpanHz": .number(0),
            "traceSamples": .number(1_206), "waterfallSamples": .number(1_206), "wideSamples": .number(0),
            "minDbm": .number(-160), "maxDbm": .number(0), "fps": .number(30), "framesPerLine": .number(1),
        ]
        if let context = MediaControlDecoder.context(payload, wideband: false, grant: false) {
            band.receive(.context(context))
        }
    }

    /// A Yaesu-style rotor on an Easy Rotor Control at the Core, connected,
    /// pointing at 47 degrees, with three presets.
    private static let rotorProperties: [LinkMessage.PropertyEntry] = {
        let values: [(String, LinkMessage.PropertyValue)] = [
            ("connectionPhase", .enumeration(6)), ("connectionError", .utf8("")), ("label", .utf8("Easy Rotor Control on COM4")),
            ("driver", .enumeration(2)), ("serialPort", .utf8("COM4")), ("baud", .i64(9600)), ("host", .utf8("")),
            ("port", .i64(4533)), ("serialPorts", .utf8("COM3\nCOM4")), ("hamlibModel", .i64(0)),
            ("rotctldAvailable", .bool(false)), ("axes", .enumeration(0)), ("rangeDeg", .i64(450)),
            ("endStop", .enumeration(2)), ("offsetDeg", .f64(0)), ("spanPositionDeg", .f64(227)),
            ("travelDeg", .f64(0)), ("routeKnown", .bool(true)), ("positionFresh", .bool(true)),
            ("azimuthDeg", .f64(47)), ("elevationDeg", .f64(-1)), ("targetAzimuthDeg", .f64(-1)),
            ("targetElevationDeg", .f64(-1)), ("motion", .enumeration(0)),
            ("presets", .utf8("EU\t45\nJA\t330\nVK\t250")), ("fault", .utf8("")),
        ]
        return values.enumerated().map { index, entry in
            LinkMessage.PropertyEntry(ordinal: UInt16(index), name: entry.0, value: entry.1)
        }
    }()

    /// Two cluster spots in the band's view: JA1ABC, which the Core places at
    /// 330 degrees, and W1AW with no bearing (-1).
    private static let spotRecords: [LinkMessage.RecordBatch.Record] = [
        ("1", 7_250_000.0, "JA1ABC", 330.0),
        ("2", 7_260_000.0, "W1AW", -1.0),
    ].map { id, hz, call, bearing in
        LinkMessage.RecordBatch.Record(id: id, fields: [
            "timeUtc": .string("2026-10-07T14:02:00Z"), "frequencyHz": .number(hz), "call": .string(call),
            "mode": .string("CW"), "source": .string("Cluster"), "spotter": .string("K1TTT"),
            "comment": .string("599"), "band": .number(3), "dxccColour": .string(""), "dxccPriority": .number(0),
            "bearingDeg": .number(bearing),
        ])
    }

    /// The hold PTT's record while ``roundtableArgument`` is on, else nil.
    @MainActor static var holdLog: UITestHoldLog?
    /// With ``roundtableArgument``: every ask to key, for the swipes' never-key tests.
    @MainActor static var keyLog: UITestKeyLog?

    /// Slice A's room on the band list: four stations, K4ABC talking.
    private static func roundtableSummary(now: Int64) -> LinkMessage.RecordBatch.Record {
        LinkMessage.RecordBatch.Record(id: roundtableRoomId, fields: [
            "record": .string("room"), "id": .string(roundtableRoomId), "band": .string("40m"),
            "family": .string("lsb"), "hz": .number(7_236_400), "state": .string("talking"),
            "talkers": .array([.string("K4ABC")]), "members": .number(4), "messages": .number(2),
            "last": .number(Double(now)), "folded": .bool(false), "start": .null, "heard": .number(3),
            "readings": .array([]), "lastMessage": .null, "ended": .null,
        ])
    }

    /// Slice A's room: K4ABC on the air, three readings of it, you, an
    /// earlier turn, and two messages, the newer naming you.
    private static func roundtableRecords(now: Int64) -> [LinkMessage.RecordBatch.Record] {
        func member(_ ref: String, _ callsign: String, km: Double?, offset: Double, seq: Int64,
                    you: Bool = false) -> LinkMessage.RecordBatch.Record {
            .init(id: "member:\(ref)", fields: [
                "record": .string("member"), "ref": .string(ref), "callsign": .string(callsign), "nth": .number(1),
                "grid": .string("EM73"), "km": km.map(LinkJSON.number) ?? .null, "offsetHz": .number(offset),
                "joining": .bool(false), "away": .bool(false), "keyed": .bool(false),
                "software": .string("NereusSDR"), "softwareVersion": .string("26.10.3"), "you": .bool(you),
                "seq": .number(Double(seq)),
            ])
        }
        func reading(_ ref: String, _ dbm: Double, ageMs: Int64) -> LinkMessage.RecordBatch.Record {
            .init(id: "reading:\(ref)", fields: [
                "record": .string("reading"), "ref": .string(ref), "dbm": .number(dbm),
                "t": .number(Double(now - ageMs)), "present": .bool(true), "turn": .number(12),
            ])
        }
        func message(_ id: Int64, _ callsign: String, _ text: String, ageMs: Int64) -> LinkMessage.RecordBatch.Record {
            .init(id: "message:\(id)", fields: [
                "record": .string("message"), "id": .number(Double(id)), "ref": .null, "callsign": .string(callsign),
                "nth": .number(1), "t": .number(Double(now - ageMs)), "state": .string("shown"),
                "text": .string(text), "dbm": .null,
            ])
        }
        return [
            member("k/1", "K4ABC", km: nil, offset: 0, seq: 0),
            member("n/2", "N4XYZ", km: 215, offset: 0, seq: 1),
            reading("n/2", -43, ageMs: 3_000),
            member("q/3", "K4QRS", km: 593, offset: -20, seq: 2),
            reading("q/3", -55, ageMs: 1_000),
            member("y/4", "KG4VCF", km: 340, offset: 0, seq: 3, you: true),
            reading("y/4", -61, ageMs: 1_000),
            .init(id: "turn:12", fields: [
                "record": .string("turn"), "id": .number(12), "kind": .string("talking"),
                "talkers": .array([.object(["ref": .string("k/1"), "callsign": .string("K4ABC"), "nth": .number(1)])]),
                "start": .number(Double(now - 78_000)), "end": .null, "ending": .bool(false), "heard": .number(3),
                "mine": .null,
            ]),
            .init(id: "turn:11", fields: [
                "record": .string("turn"), "id": .number(11), "kind": .string("talking"), "talkers": .array([]),
                "start": .number(Double(now - 200_000)), "end": .number(Double(now - 140_000)),
                "ending": .bool(false), "heard": .number(2), "mine": .null,
            ]),
            message(21, "N4XYZ", "good copy all", ageMs: 90_000),
            message(22, "K4QRS", "KG4VCF you are S9 here", ageMs: 30_000),
        ]
    }

    /// The catalogue's JSON from a conformance-suite session file: the
    /// `json` property of its `catalog` message.
    private static func catalogueJSON(path: String) -> String? {
        guard let data = FileManager.default.contents(atPath: path),
              let object = try? JSONSerialization.jsonObject(with: data) as? [String: Any] else {
            return nil
        }
        for step in object["steps"] as? [[String: Any]] ?? [] {
            guard let message = step["message"] as? [String: Any], message["key"] as? String == "catalog",
                  let properties = message["properties"] as? [[String: Any]],
                  let json = properties.first(where: { $0["name"] as? String == "json" })?["value"] as? String else {
                continue
            }
            return json
        }
        return nil
    }
}

/// The hold PTT's presses and releases, for the UI tests to read: shown as
/// the label of a tiny element, `pttHoldLog`, at the band's corner.
@MainActor
final class UITestHoldLog: ObservableObject {
    @Published private(set) var actions: [String] = []

    func record(_ action: PttHold.Action) {
        switch action {
        case .press:
            actions.append("press")
        case .release:
            actions.append("release")
        case .unkey:
            actions.append("unkey")
        }
    }

    /// The tiny element that carries the record.
    struct Label: View {
        @ObservedObject var log: UITestHoldLog

        var body: some View {
            Text(log.actions.isEmpty ? "none" : log.actions.joined(separator: ","))
                .font(.system(size: 1))
                .foregroundStyle(Color.clear)
                .frame(width: 1, height: 1)
                .accessibilityIdentifier("pttHoldLog")
                .allowsHitTesting(false)
        }
    }
}
/// Every ask to key (PTT, MOX, the hold PTT, TUNE), for the UI tests to
/// read: the label of a tiny element, `pttKeyLog`, at the band's corner.
@MainActor
final class UITestKeyLog: ObservableObject {
    @Published private(set) var asks: [String] = []

    func record(_ ask: String) {
        asks.append(ask)
    }

    struct Label: View {
        @ObservedObject var log: UITestKeyLog

        var body: some View {
            Text(log.asks.isEmpty ? "none" : log.asks.joined(separator: ","))
                .font(.system(size: 1))
                .foregroundStyle(Color.clear)
                .frame(width: 1, height: 1)
                .accessibilityIdentifier("pttKeyLog")
                .allowsHitTesting(false)
        }
    }
}
#endif
