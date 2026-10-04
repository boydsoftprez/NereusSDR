// NereusSDR for iOS: the fake Core's Diversity pattern, logging category labels and radio model choices
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkSessionTestSupport
import LinkTestSupport
import NereusLink

/// The fake Core's side of the phone wire batch (link document sections
/// 6.1, 6.3, 7.1 and 7.7, Core checkpoint bf46f86b2). With
/// ``Additions/diversityPattern`` it advertises `diversityPatternVersion` 1
/// and every slice's snapshot carries `diversityPattern`, the pattern the
/// suite's diversity-pattern session sends (``deliverDiversityPattern(_:key:)``
/// sends another); with ``Additions/logCategoryList`` it advertises
/// `logCategoryListVersion` 1 and the `radio` snapshot carries
/// `logCategoryList`, the list the suite's log-category-list session sends;
/// with ``Additions/radioModels`` it advertises `radioModelsVersion` (1
/// beside ``Additions/stationRadios``, else 0), each `stationRadios` record
/// carries `modelLabel` and `models`, and `station.setRadioModel` takes a
/// model the radio lists and refuses another with the Core's words.
extension FakeStation {
    public static let modelMismatchReason = "That model does not match this radio."

    static func wireBatchCapabilityVersions(_ additions: Additions) -> [(String, Int)] {
        var versions: [(String, Int)] = []
        if additions.contains(.diversityPattern) {
            versions.append(("diversityPatternVersion", 1))
        }
        if additions.contains(.logCategoryList) {
            versions.append(("logCategoryListVersion", 1))
        }
        if additions.contains(.radioModels) {
            versions.append(("radioModelsVersion", additions.contains(.stationRadios) ? 1 : 0))
        }
        return versions
    }

    /// The first pattern the suite's diversity-pattern session sends for
    /// slice 0 (in its snapshot), as the Core's compact JSON.
    public static func suiteDiversityPattern() throws -> String {
        try suiteValue(fixture: "session-diversity-pattern", property: "diversityPattern")
    }

    /// The logging categories the suite's log-category-list session sends, as the Core's compact JSON.
    public static func suiteLogCategoryList() throws -> String {
        try suiteValue(fixture: "session-log-category-list", property: "logCategoryList")
    }

    /// Sends a new pattern on `key`, as the Core does when a rounded sample changes.
    public func deliverDiversityPattern(_ json: String, key: String = "slice:0") async throws {
        let ordinal = try Self.featureField(className: "SliceModel", name: "diversityPattern").ordinal
        await deliver(.delta(LinkMessage.Delta(key: key, properties: [
            .init(ordinal: ordinal, name: "diversityPattern", value: .utf8(json)),
        ])))
    }

    /// A feature-gated property the snapshot carries, with its value.
    struct SnapshotAddition {
        let className: String
        let name: String
        let value: String
    }

    /// The feature-gated properties the additions put in the snapshot.
    static func wireBatchSnapshotAdditions(_ additions: Additions) throws -> [SnapshotAddition] {
        var added: [SnapshotAddition] = []
        if additions.contains(.diversityPattern) {
            added.append(SnapshotAddition(className: "SliceModel", name: "diversityPattern",
                                          value: try suiteDiversityPattern()))
        }
        if additions.contains(.logCategoryList) {
            added.append(SnapshotAddition(className: "RadioModel", name: "logCategoryList",
                                          value: try suiteLogCategoryList()))
        }
        return added
    }

    /// A snapshot's schema or object message with the `added` properties of
    /// its class, each at the end, as the Core declares them last.
    static func withWireBatchProperties(_ object: [String: Any], added: [SnapshotAddition]) throws -> [String: Any] {
        guard !added.isEmpty, let type = object["type"] as? String, let className = object["class"] as? String else {
            return object
        }
        var object = object
        for entry in added where entry.className == className {
            let field = try featureField(className: className, name: entry.name)
            if type == "schema", var fields = object["fields"] as? [[String: Any]],
               !fields.contains(where: { $0["name"] as? String == entry.name }) {
                fields.append(["kind": field.kind.rawValue, "name": entry.name, "ordinal": Int(field.ordinal)])
                object["fields"] = fields
            } else if type == "object.create", var properties = object["properties"] as? [[String: Any]],
                      !properties.contains(where: { $0["name"] as? String == entry.name }) {
                properties.append(["kind": field.kind.rawValue, "name": entry.name, "ordinal": Int(field.ordinal),
                                   "value": entry.value])
                object["properties"] = properties
            }
        }
        return object
    }

    /// The fake's answer to `station.setRadioModel`, with ``Additions/stationRadios``.
    func radioModelReplies(_ invoke: LinkMessage.CommandInvoke) -> [LinkMessage]? {
        guard additions.contains(.stationRadios), invoke.verb == "station.setRadioModel" else {
            return nil
        }
        func result(_ accepted: Bool, _ reason: String = "") -> LinkMessage {
            .commandResult(LinkMessage.CommandResult(verb: invoke.verb, id: invoke.id, accepted: accepted,
                                                     reason: reason, affected: [], values: nil))
        }
        if let reason = takeRefusal(invoke.verb) {
            return [result(false, reason)]
        }
        guard invoke.args.count == 2, case .utf8(let mac)? = invoke.args.first(where: { $0.name == "mac" })?.value,
              case .i64(let model)? = invoke.args.first(where: { $0.name == "model" })?.value else {
            return [result(false, Self.unreadableRequestReason)]
        }
        let modelsSent = additions.contains(.radioModels)
        return stationRadiosState.read { scene, generation -> [LinkMessage] in
            guard let index = scene.radios.firstIndex(where: { $0.mac == mac }) else {
                return [result(false, Self.cannotSeeRadioReason)]
            }
            guard let choice = scene.radios[index].models.first(where: { $0.model == model }) else {
                return [result(false, Self.modelMismatchReason)]
            }
            scene.radios[index].model = choice.model
            scene.radios[index].modelLabel = choice.label
            return [result(true), .recordBatch(LinkMessage.RecordBatch(
                stream: Self.stationRadiosStream, generation: generation, reset: false,
                upserts: [Self.stationRadioRecord(scene.radios[index], models: modelsSent)], removes: []))]
        }
    }

    /// A property the suite's surface declares for `className`.
    private static func featureField(className: String, name: String) throws -> LinkMessage.SchemaField {
        guard let field = try schema(ofClass: className).fields.first(where: { $0.name == name }) else {
            throw LinkFixtureLoader.Malformed(description: "surface.json: \(className) has no \(name)")
        }
        return field
    }

    /// The first utf8 value of `property` the Core sends in `fixture`, in a delta or a property result.
    private static func suiteValue(fixture fixtureId: String, property: String) throws -> String {
        guard let fixture = try SessionFixtures.all().first(where: { $0.id == fixtureId }) else {
            throw LinkFixtureLoader.Malformed(description: "the suite has no session fixture \(fixtureId)")
        }
        for step in fixture.steps where step["from"] as? String == "station" {
            guard let message = step["message"] as? [String: Any] else {
                continue
            }
            let entries = (message["properties"] as? [[String: Any]] ?? [])
                + (message["results"] as? [[String: Any]] ?? []).compactMap { $0["value"] as? [String: Any] }
            if let value = entries.first(where: { $0["name"] as? String == property })?["value"] as? String,
               !value.isEmpty, !value.hasPrefix("$") {
                return value
            }
        }
        throw LinkFixtureLoader.Malformed(description: "\(fixtureId) sends no \(property)")
    }
}

extension FakeStation.Additions {
    /// `diversityPatternVersion` 1 and each slice's `diversityPattern`. Not in ``all``.
    public static let diversityPattern = FakeStation.Additions(rawValue: 1 << 50)
    /// `logCategoryListVersion` 1 and `radio`'s `logCategoryList`. Not in ``all``.
    public static let logCategoryList = FakeStation.Additions(rawValue: 1 << 51)
    /// `radioModelsVersion` and each `stationRadios` record's `modelLabel`
    /// and `models`. Not in ``all``.
    public static let radioModels = FakeStation.Additions(rawValue: 1 << 52)
}
