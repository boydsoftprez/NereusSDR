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
        ])))
        if let path = environment[catalogueEnvironment], let json = catalogueJSON(path: path) {
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
        mirror.apply(.snapshotComplete)
        mirror.handle(.stateChanged(.ready))
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
