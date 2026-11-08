// NereusSDR for iOS: the fake Core's SSB Mod Monitor: its two record streams and txSsbModMonitor.reset
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink

/// The SSB Mod Monitor as the Core serves it (link document sections 6.3,
/// 7.7 and 9.1), with ``Additions/ssbModMonitor``: `txSsbModMonitorVersion`
/// 1, the `txSsbModulation` and `txSsbModulationFeedback` streams (one
/// record, `id` "0", sent only to a device that subscribes) and
/// `txSsbModMonitor.reset {source}`. Without it the fake plays an older Core.
extension FakeStation {
    public static let ssbModMonitorTxStream = "txSsbModulation"
    public static let ssbModMonitorFeedbackStream = "txSsbModulationFeedback"
    public static let ssbModMonitorResetVerb = "txSsbModMonitor.reset"

    /// The streams the app watches now.
    public var ssbModMonitorWatched: Set<String> {
        ssbModMonitorState.read { _, watched, _ in watched }
    }

    /// The Core reads its analyzer: `fields` becomes the record on `stream`,
    /// sent at once to an app that watches it.
    public func deliverSsbModMonitor(_ fields: [String: LinkJSON],
                                     stream: String = FakeStation.ssbModMonitorTxStream) async {
        let batch = ssbModMonitorState.read { records, watched, generation -> LinkMessage? in
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

    /// The key ends (or the mode leaves USB and LSB): the Core removes the record.
    public func endSsbModMonitor(stream: String = FakeStation.ssbModMonitorTxStream) async {
        let batch = ssbModMonitorState.read { records, watched, generation -> LinkMessage? in
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

    /// The fake's answer to the monitor's verb or streams; nil leaves it to the rest of the fake.
    func ssbModMonitorReplies(_ invoke: LinkMessage.CommandInvoke) -> [LinkMessage]? {
        func text(_ name: String) -> String? {
            if case .utf8(let value)? = invoke.args.first(where: { $0.name == name })?.value {
                return value
            }
            return nil
        }
        let streams = [Self.ssbModMonitorTxStream, Self.ssbModMonitorFeedbackStream]
        let ownStream = ["records.subscribe", "records.unsubscribe"].contains(invoke.verb)
            && streams.contains(text("stream") ?? "")
        guard additions.contains(.ssbModMonitor), ownStream || invoke.verb == Self.ssbModMonitorResetVerb else {
            return nil
        }
        func result(_ accepted: Bool, _ reason: String = "") -> LinkMessage {
            .commandResult(LinkMessage.CommandResult(verb: invoke.verb, id: invoke.id, accepted: accepted,
                                                     reason: reason, affected: [], values: nil))
        }
        if let reason = takeRefusal(invoke.verb) {
            return [result(false, reason)]
        }
        return ssbModMonitorState.read { records, watched, generation -> [LinkMessage] in
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
}

extension FakeStation.Additions {
    /// The Core's SSB Mod Monitor: `txSsbModMonitorVersion` 1 with its record streams.
    public static let ssbModMonitor = FakeStation.Additions(rawValue: 1 << 9)
}
