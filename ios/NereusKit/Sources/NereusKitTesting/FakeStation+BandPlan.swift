// NereusSDR for iOS: the fake Core's band plan setting: a write of BandPlanName kept and echoed, or refused on request
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink

/// The fake Core's `BandPlanName` (D79): a `settings.write` of it is kept
/// and echoed with the writer's origin, as the Core echoes a setting (link
/// section 8.1), or refused with the reason queued by
/// ``refuseNext(_:reason:)`` under ``bandPlanKey``, carrying the value the
/// fake kept, as the Core's refusal carries its own. When the catalogue the
/// fake last sent marks its plans `active` (a Core that follows its band
/// plan), a taken write also moves the mark with a catalogue delta, and a
/// plan it does not have is refused, as that Core does. Other settings are
/// left for the test, as before.
extension FakeStation {
    /// The Core's setting that names its band plan.
    public static let bandPlanKey = "BandPlanName"
    /// The Core's refusal of a plan it does not have.
    public static let unknownBandPlanReason = "This Core does not have that band plan."

    /// The value the fake keeps, and the catalogue it last sent, behind its own lock.
    final class BandPlanSetting: @unchecked Sendable {
        private let lock = NSLock()
        private var value: [LinkMessage.PropertyEntry] = []
        private var catalogue: (json: String, revision: Int64)?

        var entries: [LinkMessage.PropertyEntry] {
            get { lock.withLock { value } }
            set { lock.withLock { value = newValue } }
        }

        /// The catalogue `json` and `revision` the fake last sent.
        var lastCatalogue: (json: String, revision: Int64)? {
            get { lock.withLock { catalogue } }
            set { lock.withLock { catalogue = newValue } }
        }
    }

    /// Keeps the catalogue a message the fake sends carries, so a plan
    /// change can move its `active` mark as the Core does.
    func noteCatalogue(_ message: LinkMessage) {
        let properties: [LinkMessage.PropertyEntry]
        switch message {
        case .objectCreate(let create) where create.key == "catalog":
            properties = create.properties
        case .delta(let delta) where delta.key == "catalog":
            properties = delta.properties
        default:
            return
        }
        guard case .utf8(let json)? = properties.first(where: { $0.name == "json" })?.value else {
            return
        }
        var revision = bandPlanSetting.lastCatalogue?.revision ?? 0
        if case .i64(let sent)? = properties.first(where: { $0.name == "revision" })?.value {
            revision = sent
        }
        bandPlanSetting.lastCatalogue = (json, revision)
    }

    /// For a catalogue that marks its plans `active` (a Core that follows
    /// its band plan, link 7.4): the plans' names, and the catalogue with the
    /// mark moved to `name` (the default plan's when nil).
    private func catalogue(marking name: String?) -> (names: [String], json: String, revision: Int64)? {
        guard let last = bandPlanSetting.lastCatalogue,
              var object = (try? JSONSerialization.jsonObject(with: Data(last.json.utf8))) as? [String: Any],
              let plans = object["bandPlans"] as? [[String: Any]], plans.contains(where: { $0["active"] != nil })
        else {
            return nil
        }
        let names = plans.compactMap { $0["name"] as? String }
        object["bandPlans"] = plans.map { plan -> [String: Any] in
            var plan = plan
            plan["active"] = name.map { plan["name"] as? String == $0 } ?? (plan["default"] as? Bool == true)
            return plan
        }
        guard let data = try? JSONSerialization.data(withJSONObject: object, options: [.sortedKeys]) else {
            return nil
        }
        return (names, String(decoding: data, as: UTF8.self), last.revision + 1)
    }

    /// The fake's answer to a `settings.write` the app sent.
    func settingsReplies(_ write: LinkMessage.SettingsWrite) -> [LinkMessage] {
        guard write.key == Self.bandPlanKey else {
            return []
        }
        if let reason = takeRefusal(Self.bandPlanKey) {
            return [.settingsReject(LinkMessage.SettingsReject(key: write.key, properties: bandPlanSetting.entries,
                                                               reason: reason))]
        }
        var name: String?
        if case .utf8(let text)? = write.properties.first?.value {
            name = text
        }
        let marked = catalogue(marking: name)
        if let marked, let name, !marked.names.contains(name) {
            // A Core that follows its band plan refuses one it does not have.
            return [.settingsReject(LinkMessage.SettingsReject(key: write.key, properties: bandPlanSetting.entries,
                                                               reason: Self.unknownBandPlanReason))]
        }
        bandPlanSetting.entries = write.properties
        var replies: [LinkMessage] = [.settingsValue(LinkMessage.SettingsValue(key: write.key, origin: write.origin,
                                                                               properties: write.properties))]
        if let marked {
            // Its catalogue follows: the mark moves, one revision on.
            let delta = LinkMessage.delta(LinkMessage.Delta(key: "catalog", properties: [
                .init(ordinal: 0, name: "json", value: .utf8(marked.json)),
                .init(ordinal: 1, name: "revision", value: .i64(marked.revision)),
            ]))
            noteCatalogue(delta)
            replies.append(delta)
        }
        return replies
    }
}
