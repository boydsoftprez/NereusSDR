// NereusSDR for iOS: the fake Core's VAX channels: the vax object, its writes, and the vaxLevels record stream
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink

/// The VAX channels of the fake Core's computer (link document sections
/// 6.3, 7.1, 7.3 and 7.7). With ``Additions/vax`` it plays a Core the
/// desktop hosts, at `vaxVersion` 1: ``deliverVax(_:)`` sends the `vax`
/// object (class `StationVax`); a `property.write` on `vax` is answered as
/// the Core answers it (a level outside 0 to 1 refused, `txGain` refused
/// unless the device may transmit, an outbound property refused), and an
/// accepted one is sent back in a delta; `records.subscribe` for
/// `vaxLevels` answers with a reset carrying the newest meters, and
/// ``deliverVaxLevels(_:)`` sends new meters to a device that follows
/// them. With ``Additions/vaxUnhosted`` it plays a headless Core,
/// `vaxVersion` 0, with no object. A refusal queued with
/// ``refuseNext(_:reason:)`` under the key `vax` answers the next write.
extension FakeStation {
    /// The fake Core's VAX channels.
    public struct SceneVax: Sendable, Equatable {
        public var slices: [String]
        public var rxGains: [Double]
        public var muted: [Bool]
        public var devices: [String]
        public var txSlice: String
        public var txGain: Double

        public init(slices: [String], rxGains: [Double], muted: [Bool], devices: [String], txSlice: String,
                    txGain: Double) {
            self.slices = slices
            self.rxGains = rxGains
            self.muted = muted
            self.devices = devices
            self.txSlice = txSlice
            self.txGain = txGain
        }

        /// Slices A and B on channel 1, channel 2 muted, as a desktop's VAX applet might show them.
        public static let board = SceneVax(
            slices: ["AB", "", "C", ""], rxGains: [0.8, 0.5, 1, 1], muted: [false, true, false, false],
            devices: ["NereusSDR VAX 1", "NereusSDR VAX 2", "NereusSDR VAX 3", "NereusSDR VAX 4"],
            txSlice: "A", txGain: 0.6)
    }

    /// The meters a device gets on subscribing, until a test sends others.
    public static let vaxBoardLevels: [String: LinkJSON] = [
        "ch1Level": .number(0.42), "ch2Level": .number(0), "ch3Level": .number(0.17), "ch4Level": .number(0),
        "txLevel": .number(0.05), "atMs": .number(1_790_000_000_500),
    ]

    public static let vaxClass = "StationVax"
    public static let vaxKey = "vax"
    public static let vaxLevelsStream = "vaxLevels"
    public static let vaxLevelReason = "A VAX level goes from 0 to 1."
    public static let vaxOutboundReason = "The Core sets this itself; it cannot be changed from here."
    public static let vaxReceiveOnlyReason = "This Core is set to receive only."

    final class VaxState: @unchecked Sendable {
        private let lock = NSLock()
        private var scene = SceneVax.board
        private var levels: [String: LinkJSON] = FakeStation.vaxBoardLevels
        private var watched = false

        func read<Result>(_ body: (inout SceneVax, inout [String: LinkJSON], inout Bool) -> Result) -> Result {
            lock.withLock { body(&scene, &levels, &watched) }
        }
    }

    static func vaxCapabilityVersions(_ additions: Additions) -> [(String, Int)] {
        if additions.contains(.vax) {
            return [("vaxVersion", 1)]
        }
        return additions.contains(.vaxUnhosted) ? [("vaxVersion", 0)] : []
    }

    /// The fake Core's channels as it holds them now.
    public var vaxScene: SceneVax {
        vaxState.read { scene, _, _ in scene }
    }

    /// Whether a device follows the meters now.
    public var vaxLevelsWatched: Bool {
        vaxState.read { _, _, watched in watched }
    }

    /// Sets the channels and sends them as a Core at `vaxVersion` 1 would:
    /// the class's schema and the `vax` object.
    public func deliverVax(_ scene: SceneVax = .board) async throws {
        vaxState.read { current, _, _ in current = scene }
        await deliver(.schema(try Self.schema(ofClass: Self.vaxClass)))
        await deliver(.objectCreate(try Self.objectCreate(key: Self.vaxKey, className: Self.vaxClass,
                                                          values: Self.vaxValues(scene))))
    }

    /// The Core reads its meters: `fields` becomes the record, sent at once
    /// to a device that follows it.
    public func deliverVaxLevels(_ fields: [String: LinkJSON]) async {
        let batch = vaxState.read { _, levels, watched -> LinkMessage? in
            levels = fields
            guard watched else {
                return nil
            }
            return .recordBatch(LinkMessage.RecordBatch(stream: Self.vaxLevelsStream, generation: 1, reset: false,
                                                        upserts: [.init(id: "0", fields: fields)], removes: []))
        }
        if let batch {
            await deliver(batch)
        }
    }

    /// The fake's answer to `records.subscribe` or `records.unsubscribe` for the meters.
    func vaxReplies(_ invoke: LinkMessage.CommandInvoke) -> [LinkMessage]? {
        guard additions.contains(.vax), ["records.subscribe", "records.unsubscribe"].contains(invoke.verb),
              case .utf8(Self.vaxLevelsStream)? = invoke.args.first(where: { $0.name == "stream" })?.value else {
            return nil
        }
        func result(_ accepted: Bool, _ reason: String = "") -> LinkMessage {
            .commandResult(LinkMessage.CommandResult(verb: invoke.verb, id: invoke.id, accepted: accepted,
                                                     reason: reason, affected: [], values: nil))
        }
        if let reason = takeRefusal(invoke.verb) {
            return [result(false, reason)]
        }
        return vaxState.read { _, levels, watched -> [LinkMessage] in
            if invoke.verb == "records.unsubscribe" {
                watched = false
                return [result(true)]
            }
            watched = true
            return [result(true), .recordBatch(LinkMessage.RecordBatch(
                stream: Self.vaxLevelsStream, generation: 1, reset: true,
                upserts: [.init(id: "0", fields: levels)], removes: []))]
        }
    }

    /// The fake's answer to a `property.write` on `vax`: a result per
    /// property, then a delta of the accepted ones.
    func vaxWriteReplies(_ write: LinkMessage.PropertyWrite) -> [LinkMessage]? {
        guard additions.contains(.vax), write.key == Self.vaxKey else {
            return nil
        }
        let queued = takeRefusal(Self.vaxKey)
        let fields = (try? Self.schema(ofClass: Self.vaxClass).fields) ?? []
        let mayTransmit = additions.contains(.remoteTx)
        return vaxState.read { scene, _, _ -> [LinkMessage] in
            var results: [LinkMessage.PropertyResult.Result] = []
            var accepted: [LinkMessage.PropertyEntry] = []
            for entry in write.properties {
                let current = Self.vaxValues(scene)[entry.name]
                let ordinal = fields.first { $0.name == entry.name }?.ordinal ?? entry.ordinal
                let kept = current.map { LinkMessage.PropertyEntry(ordinal: ordinal, name: entry.name, value: $0) }
                func refuse(_ reason: String) {
                    results.append(.init(property: entry.name, accepted: false, reason: reason, value: kept))
                }
                if let queued {
                    refuse(queued)
                    continue
                }
                let channel = (1...4).first { entry.name == "ch\($0)RxGain" || entry.name == "ch\($0)Muted" }
                if entry.name.hasSuffix("Muted"), let channel {
                    guard case .bool(let on) = entry.value else {
                        refuse("The Core could not read this request.")
                        continue
                    }
                    scene.muted[channel - 1] = on
                } else if entry.name.hasSuffix("RxGain") || entry.name == "txGain" {
                    guard case .f64(let level) = entry.value, level.isFinite, (0...1).contains(level) else {
                        refuse(Self.vaxLevelReason)
                        continue
                    }
                    if entry.name == "txGain" {
                        guard mayTransmit else {
                            refuse(Self.vaxReceiveOnlyReason)
                            continue
                        }
                        scene.txGain = level
                    } else if let channel {
                        scene.rxGains[channel - 1] = level
                    }
                } else {
                    refuse(Self.vaxOutboundReason)
                    continue
                }
                let next = LinkMessage.PropertyEntry(ordinal: ordinal, name: entry.name, value: entry.value)
                results.append(.init(property: entry.name, accepted: true, reason: "", value: next))
                accepted.append(next)
            }
            var replies: [LinkMessage] = []
            if let writeId = write.writeId, writeId != 0 {
                replies.append(.propertyResult(LinkMessage.PropertyResult(key: Self.vaxKey, writeId: writeId,
                                                                          results: results)))
            }
            if !accepted.isEmpty {
                replies.append(.delta(LinkMessage.Delta(key: Self.vaxKey, properties: accepted)))
            }
            return replies
        }
    }

    private static func vaxValues(_ scene: SceneVax) -> [String: LinkMessage.PropertyValue] {
        var values: [String: LinkMessage.PropertyValue] = ["txSlice": .utf8(scene.txSlice), "txGain": .f64(scene.txGain)]
        for index in 0..<4 {
            values["ch\(index + 1)Slices"] = .utf8(scene.slices[index])
            values["ch\(index + 1)RxGain"] = .f64(scene.rxGains[index])
            values["ch\(index + 1)Muted"] = .bool(scene.muted[index])
            values["ch\(index + 1)Device"] = .utf8(scene.devices[index])
        }
        return values
    }
}

extension FakeStation.Additions {
    /// A Core the desktop hosts, whose computer publishes VAX devices: `vaxVersion` 1 with the `vax` object and
    /// the `vaxLevels` stream (on the record streams, at least version 1).
    public static let vax = FakeStation.Additions(rawValue: 1 << 40)
    /// A headless Core with no VAX devices: `vaxVersion` 0 and no object.
    public static let vaxUnhosted = FakeStation.Additions(rawValue: 1 << 41)
}
