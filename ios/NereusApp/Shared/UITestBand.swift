// NereusSDR for iOS: a debug build's stand-in for the Core's band and one slice, for the UI tests of touches on the band
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

#if DEBUG
import Foundation
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
    /// Beside ``argument`` and ``displayEnvironment``: the band menu's
    /// Core. Slice A is on 40 m, the Core takes notch requests (version 1)
    /// and the link is up for settings and commands: each settings write
    /// comes back as the Core's value, and each `notch.add` is refused with
    /// its own arguments in the reason, so the UI test reads what was sent.
    static let bandMenuArgument = "-NereusBandMenuFixture"
    static let catalogueEnvironment = "NEREUS_UITEST_CATALOGUE"
    /// Beside ``argument``: the Core describes Setup > Display at version
    /// 29, read from the file this names (the repository's
    /// `resources/setup/display.json`), so the Display pill and Setup's eight
    /// Display pages show as they do with a Core.
    static let displayEnvironment = "NEREUS_UITEST_DISPLAY"
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
        let bandMenu = arguments.contains(bandMenuArgument)
        let display = environment[displayEnvironment].flatMap { path in
            FileManager.default.contents(atPath: path).flatMap { String(data: $0, encoding: .utf8) }
        }
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
          + (display == nil ? [] : displayCapabilities)
          + (bandMenu ? [.init(ordinal: 8, name: PanSheetModel.notchControlCapability, value: .i64(1))] : []))))
        if let display {
            mirror.apply(.schema(.init(className: "SetupDescription", fields: [
                .init(ordinal: 0, name: "revision", kind: .i64), .init(ordinal: 1, name: "display", kind: .utf8),
            ])))
            mirror.apply(.objectCreate(.init(key: "setup", className: "SetupDescription", properties: [
                .init(ordinal: 0, name: "revision", value: .i64(1)), .init(ordinal: 1, name: "display", value: .utf8(display)),
            ])))
        }
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
        ] + (bandMenu ? [.init(ordinal: 30, name: "band", value: .i64(3))] : []))))
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
        mirror.apply(.snapshotComplete)
        mirror.handle(.stateChanged(.ready))
        if bandMenu {
            linkForBandMenu(app)
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

    /// The settings proxy and the command client up as on a live link, and
    /// the Core's answers to what they send (``bandMenuArgument``).
    @MainActor
    private static func linkForBandMenu(_ app: AppModel) {
        let settings = app.settings
        let commands = app.commands
        let notchVerb = PanSheetModel.addNotchAtVerb
        app.useBandFixtureCommandSender { [weak settings] message in
            switch message {
            case .settingsWrite(let write):
                // The Core keeps it and says so, after the write is sent.
                Task { @MainActor in
                    settings?.handle(.message(.settingsValue(.init(key: write.key, origin: write.origin,
                                                                   properties: write.properties))))
                }
            case .commandInvoke(let invoke) where invoke.verb == notchVerb:
                let reason = "UI fixture: " + notchText(invoke.args)
                Task {
                    await commands.handle(.message(.commandResult(.init(verb: invoke.verb, id: invoke.id,
                                                                        accepted: false, reason: reason,
                                                                        affected: []))))
                }
            default:
                throw LinkSendError.notConnected
            }
        }
        settings.handle(.stateChanged(.ready))
        settings.handle(.message(.authResult(.init(accepted: true, reason: "", retryable: false))))
        settings.handle(.message(.settingsSnapshot(.init(properties: []))))
        Task { await commands.handle(.stateChanged(.ready)) }
    }

    /// A `notch.add`'s arguments in words, for its refusal.
    private static func notchText(_ args: [LinkMessage.PropertyEntry]) -> String {
        func number(_ name: String) -> Double? {
            switch args.first(where: { $0.name == name })?.value {
            case .f64(let value)?: return value
            case .i64(let value)?: return Double(value)
            default: return nil
            }
        }
        let centre = number("centreHz").map { String(format: "%.6f MHz", $0 / 1.0e6) } ?? "no centre"
        let width = number("widthHz").map { "\(Int($0)) Hz wide" } ?? "no width"
        let slice = number("sliceId").map { "slice \(Int($0))" } ?? "no slice"
        return "notch.add at \(centre), \(width), \(slice)"
    }

    /// What a Core that describes Display at version 29 offers, as the
    /// Display pill's tests deliver it.
    private static let displayCapabilities: [LinkMessage.PropertyEntry] = [
        .init(ordinal: 4, name: "setupDescriptionVersion", value: .i64(29)),
        .init(ordinal: 5, name: "remoteMediaVersion", value: .i64(1)),
        .init(ordinal: 6, name: "remoteWidebandDisplayVersion", value: .i64(1)),
        .init(ordinal: 7, name: "displayExtrasVersion", value: .i64(4)),
    ]

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
#endif
