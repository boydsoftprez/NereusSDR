// NereusSDR for iOS: the fake Core's AM Mod Monitor: its two record streams, txModMonitor.reset and the shared feedback receiver
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink

/// The AM Mod Monitor as the Core serves it (link document sections 6.3,
/// 7.7, 8.1 and 9.1), with ``Additions/modMonitor``: `txModMonitorVersion`
/// 1, the `txAmModulation` and `txAmModulationFeedback` streams (one
/// record, `id` "0", sent only to a device that subscribes),
/// `txModMonitor.reset {source}` and the station setting `ModMon/FbStream`.
extension FakeStation {
    public static let modMonitorTxStream = "txAmModulation"
    public static let modMonitorFeedbackStream = "txAmModulationFeedback"
    public static let modMonitorResetVerb = "txModMonitor.reset"
    public static let modMonitorFeedbackKey = "ModMon/FbStream"
    public static let modMonitorUnreadableReason = "The Core could not read this request."

    /// The monitor's streams and what the app asked of them, behind its own lock.
    final class ModMonitorState: @unchecked Sendable {
        private let lock = NSLock()
        private var records: [String: [String: LinkJSON]] = [:]
        private var watched: Set<String> = []
        private var generation: Int64 = 1

        func read<Result>(_ body: (inout [String: [String: LinkJSON]], inout Set<String>, inout Int64) -> Result)
            -> Result {
            lock.withLock { body(&records, &watched, &generation) }
        }
    }

    /// The streams the app watches now.
    public var modMonitorWatched: Set<String> {
        modMonitorState.read { _, watched, _ in watched }
    }

    /// The Core reads its analyzer: `fields` becomes the record on `stream`,
    /// sent at once to an app that watches it.
    public func deliverModMonitor(_ fields: [String: LinkJSON], stream: String = FakeStation.modMonitorTxStream) async {
        let batch = modMonitorState.read { records, watched, generation -> LinkMessage? in
            records[stream] = fields
            guard watched.contains(stream) else {
                return nil
            }
            return .recordBatch(LinkMessage.RecordBatch(stream: stream, generation: generation, reset: false,
                                                        upserts: [.init(id: "0", fields: fields)], removes: []))
        }
        if let batch {
            await deliver(batch)
        }
    }

    /// The key ends (or the mode leaves AM, SAM and DSB): the Core removes the record.
    public func endModMonitor(stream: String = FakeStation.modMonitorTxStream) async {
        let batch = modMonitorState.read { records, watched, generation -> LinkMessage? in
            records[stream] = nil
            guard watched.contains(stream) else {
                return nil
            }
            return .recordBatch(LinkMessage.RecordBatch(stream: stream, generation: generation, reset: false,
                                                        upserts: [], removes: ["0"]))
        }
        if let batch {
            await deliver(batch)
        }
    }

    /// The fake's answer to a monitor verb or stream; nil leaves it to the rest of the fake.
    func modMonitorReplies(_ invoke: LinkMessage.CommandInvoke) -> [LinkMessage]? {
        func text(_ name: String) -> String? {
            if case .utf8(let value)? = invoke.args.first(where: { $0.name == name })?.value {
                return value
            }
            return nil
        }
        let streams = [Self.modMonitorTxStream, Self.modMonitorFeedbackStream]
        let ownStream = ["records.subscribe", "records.unsubscribe"].contains(invoke.verb)
            && streams.contains(text("stream") ?? "")
        guard additions.contains(.modMonitor), ownStream || invoke.verb == Self.modMonitorResetVerb else {
            return nil
        }
        func result(_ accepted: Bool, _ reason: String = "") -> LinkMessage {
            .commandResult(LinkMessage.CommandResult(verb: invoke.verb, id: invoke.id, accepted: accepted,
                                                     reason: reason, affected: [], values: nil))
        }
        if let reason = takeRefusal(invoke.verb) {
            return [result(false, reason)]
        }
        return modMonitorState.read { records, watched, generation -> [LinkMessage] in
            switch invoke.verb {
            case "records.subscribe":
                let stream = text("stream") ?? ""
                watched.insert(stream)
                let upserts = records[stream].map { [LinkMessage.RecordBatch.Record(id: "0", fields: $0)] } ?? []
                return [result(true), .recordBatch(LinkMessage.RecordBatch(
                    stream: stream, generation: generation, reset: true, upserts: upserts, removes: []))]
            case "records.unsubscribe":
                watched.remove(text("stream") ?? "")
                return [result(true)]
            default:
                guard invoke.args.count == 1, invoke.args[0].name == "source",
                      case .i64(let source) = invoke.args[0].value, source == 0 || source == 1 else {
                    return [result(false, Self.modMonitorUnreadableReason)]
                }
                return [result(true)]
            }
        }
    }

    /// The fake's answer to a write of the shared feedback receiver: its echo.
    func modMonitorSettingReplies(_ write: LinkMessage.SettingsWrite) -> [LinkMessage]? {
        guard additions.contains(.modMonitor), write.key == Self.modMonitorFeedbackKey else {
            return nil
        }
        if let reason = takeRefusal(write.key) {
            return [.settingsReject(LinkMessage.SettingsReject(key: write.key, properties: [], reason: reason))]
        }
        var value = ""
        if case .utf8(let text)? = write.properties.first?.value {
            value = text
        }
        return [Self.settingEcho(write.key, value, origin: write.origin)]
    }
}

extension FakeStation.Additions {
    /// The Core's AM Mod Monitor: `txModMonitorVersion` 1 with its record streams.
    public static let modMonitor = FakeStation.Additions(rawValue: 1 << 27)
}
