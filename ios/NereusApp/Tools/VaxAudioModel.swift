// NereusSDR for iOS: the VAX channels of the Core's computer: their slices, levels, mutes and meters, read and written
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusMirror
import os

/// The VAX Audio page's model (spec section 5.2 item 4, R-IOS-18): the VAX
/// channels of the computer the Core runs on, as its desktop VAX applet
/// shows them, from the Core's `vax` object (link document sections 6.3,
/// 7.1, 7.3 and 7.7, `vaxVersion` 1): each channel's slices, receive level,
/// mute and device, the transmit slice and the level of VAX used as the
/// microphone, and the meters on the `vaxLevels` record stream, asked for
/// only while the page is open. Receive levels and mutes change at once, on
/// or off the air; the microphone level changes only while this phone may
/// transmit. The Core decides every change and the page shows its words
/// for one it refuses.
@MainActor
final class VaxAudioModel: ObservableObject {
    // MARK: Words

    static let notConnectedReason = SpotsModel.notConnectedReason
    static let olderCoreReason = "This Core does not send its VAX channels. Updating the Core may help."
    static let noVaxReason = StationToolList.noVaxReason
    static let noLevelsReason = "This Core does not send the VAX levels. Updating the Core may help."
    static let noTransmitReason = "This Core does not let this phone transmit."
    static let note = "VAX carries the Core's audio to digital-mode apps on the Core's computer."

    // MARK: State

    /// The Core's channels, or nil when it sends none.
    @Published private(set) var vax: StationVax?
    /// The newest meters, or nil when none are here.
    @Published private(set) var levels: StationVax.Levels?
    /// Why the channels cannot be used now; nil when they can.
    @Published private(set) var reason: String?
    /// Why the meters are not shown; nil when they are.
    @Published private(set) var levelsReason: String?
    /// Why the microphone level cannot change now; nil when it can.
    @Published private(set) var txReason: String?
    /// The Core's words for the last change it refused.
    @Published private(set) var note: String?

    private static let logger = Logger(subsystem: "NereusSDR", category: "tools.vax")

    private let mirror: MirrorStore
    private let records: RecordStreamClient?
    private var watch: ToolMirrorWatch?
    private var open = false
    /// This model asked for the meters, so it stops asking when the page closes.
    private var wanting = false
    /// Each write shows at the touch and stays until the Core answers
    /// (`StationClient.cpp:1040-1068`), one at a time per property, the
    /// newest value next (``PropertyWriteQueue``).
    private lazy var writes = PropertyWriteQueue(store: mirror) { [weak self] property, outcome in
        self?.noteOutcome(outcome, property)
    }

    init(mirror: MirrorStore, records: RecordStreamClient?) {
        self.mirror = mirror
        self.records = records
        let watch = ToolMirrorWatch(mirror: mirror) { [weak self] in self?.objectWillChange.send(); self?.refresh() }
        self.watch = watch
        if let records {
            watch.watch(records.$streams)
            watch.watch(records.$available)
            watch.watch(records.$refusals)
        }
        refresh()
    }

    /// The page opened or closed: the meters are asked for only while it shows.
    func setOpen(_ shown: Bool) {
        open = shown
        refresh()
    }

    /// The Core's `vaxVersion`, or nil when it sends none (an older Core, or before the agreed minor).
    private var version: Int64? {
        guard (mirror.agreedMinor ?? 0) >= StationVax.minor, mirror.capabilities[StationVax.capabilityName] != nil else {
            return nil
        }
        return mirror.capabilityVersion(StationVax.capabilityName)
    }

    func isUnconfirmed(_ property: String) -> Bool { mirror.isUnconfirmed(StationVax.objectKey, property: property) }

    func refresh() {
        let object = watch?.object(StationVax.objectKey)
        let connected = mirror.isSnapshotComplete && !mirror.isStale
        func set<Value: Equatable>(_ path: ReferenceWritableKeyPath<VaxAudioModel, Value>, _ value: Value) {
            if self[keyPath: path] != value {
                self[keyPath: path] = value
            }
        }
        let version = version
        let channelsWhy: String? = !connected ? Self.notConnectedReason
            : version == nil ? Self.olderCoreReason
            : version == 0 ? Self.noVaxReason
            : object == nil ? Self.olderCoreReason : nil
        set(\.reason, channelsWhy)
        set(\.vax, channelsWhy == nil ? object.map { StationVax(values: $0.values) } : nil)
        let sendsLevels = channelsWhy == nil && records?.available == true
        set(\.levelsReason, channelsWhy ?? (sendsLevels ? records?.refusals[StationVax.levelsStream]
                                                         : Self.noLevelsReason))
        if sendsLevels && open {
            wanting = true
            records?.want(StationVax.levelsStream, backlog: StationVax.levelsCapacity, by: self)
        } else if wanting {
            wanting = false
            records?.unwant(StationVax.levelsStream, by: self)
        }
        let record = sendsLevels ? records?.records(StationVax.levelsStream).last : nil
        set(\.levels, record.map(StationVax.Levels.init(record:)))
        let permitted = mirror.capabilities["txPermitted"] == .bool(true)
        var refusal = ""
        if case .text(let text)? = mirror.capabilities["txRefusalReason"] {
            refusal = text
        }
        set(\.txReason, channelsWhy ?? (permitted ? nil : (refusal.isEmpty ? Self.noTransmitReason : refusal)))
    }

    // MARK: Shown

    /// The slices feeding a channel or transmitting, in a few words.
    static func slicesText(_ letters: String) -> String {
        switch letters.count {
        case 0:
            return "None"
        case 1:
            return "Slice \(letters)"
        default:
            return "Slices " + letters.map(String.init).joined(separator: "+")
        }
    }

    /// A level from 0 to 1 as a percentage.
    static func percent(_ level: Double) -> String {
        "\(Int((min(max(level, 0), 1) * 100).rounded()))%"
    }

    // MARK: Changing

    func setRxGain(_ channel: Int, _ level: Double) {
        write(StationVax.rxGainProperty(channel), .double(Self.hundredths(level)), allowed: reason == nil)
    }

    func setMuted(_ channel: Int, _ on: Bool) {
        write(StationVax.mutedProperty(channel), .bool(on), allowed: reason == nil)
    }

    func setTxGain(_ level: Double) {
        write(StationVax.txGainProperty, .double(Self.hundredths(level)), allowed: txReason == nil)
    }

    /// A level held to 0 to 1, to the slider's hundredth.
    static func hundredths(_ level: Double) -> Double {
        (min(max(level, StationVax.gainRange.lowerBound), StationVax.gainRange.upperBound) * 100).rounded() / 100
    }

    /// Writes to one property go one at a time, each after the Core has
    /// answered the one before; a value that arrives meanwhile replaces any
    /// other still waiting, and every value shows at the touch.
    private func write(_ property: String, _ value: MirrorValue, allowed: Bool) {
        guard allowed else {
            return
        }
        writes.write(StationVax.objectKey, property, value)
    }

    private func noteOutcome(_ outcome: PropertyWriteOutcome, _ property: String) {
        if outcome.accepted {
            if note != nil {
                note = nil
            }
        } else if outcome.answeredByCore, !outcome.reason.isEmpty {
            note = outcome.reason
        } else if !outcome.answeredByCore {
            // Not sent, not answered in time, or cut off by a dropped link.
            Self.logger.info("A \(property, privacy: .public) change: \(outcome.reason, privacy: .private)")
            note = outcome.reason
        }
    }
}
