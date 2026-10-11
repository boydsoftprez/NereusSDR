// NereusSDR for iOS: the speaker at the Core, its level and mute as the Core keeps them for every window and phone
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusMirror
import os

/// The Sound panel's Core speaker section (R-AUD-29, D25, D26): the level
/// and mute of the speaker or sound card plugged into the Core's own
/// computer, which the Core keeps for every desktop window and phone
/// (`radio`'s `coreSpeakerVolume` and `coreSpeakerMuted`, with
/// `coreSpeakerState` beside them, sent to a peer that declares
/// `coreSpeaker` 1 with `coreSpeakerVersion` 1). A change here is written
/// to the Core, and the Core's value moves the slider back here, whoever
/// changed it. Choosing the Core's card stays on the desktop.
///
/// While the Core plays on no card (none plugged in, or a desktop box
/// waiting for a pick) the section is left out, and it comes back by itself
/// when the Core plays on one (D26). Otherwise it is greyed with its
/// reason, never hidden: while the Core is unreachable, and with a Core
/// that cannot set its speaker.
@MainActor
final class CoreSpeakerModel: ObservableObject {
    // MARK: The Core's names

    static let objectKey = "radio"
    static let capabilityName = "coreSpeakerVersion"
    static let volumeProperty = "coreSpeakerVolume"
    static let mutedProperty = "coreSpeakerMuted"
    static let stateProperty = "coreSpeakerState"
    /// The level's range, as the Core keeps it.
    static let volumeRange: ClosedRange<Double> = 0...100

    /// The Core's `coreSpeakerState`, as its JSON text names it:
    /// `{"chosen":"...","desktop":false,"playing":"...","state":"playing"}`.
    struct State: Decodable, Equatable {
        enum Kind: String, Decodable {
            case playing
            case notConnected
            case inUse
            case noCard
            case waitingForPick
        }

        /// What the Core does now.
        let state: Kind
        /// The card the Core plays on now; empty while it is silent.
        let playing: String
        /// The chosen card's name; empty for the Core's default.
        let chosen: String
        /// The Core's computer starts into a desktop.
        let desktop: Bool

        /// The four keys the state has, no more and no fewer.
        static let keys: Set<String> = ["state", "playing", "chosen", "desktop"]

        /// The state in `text`, or nil when it is not one. A key missing or
        /// extra makes it not one, as the desktop reads it (the link spec's
        /// Core speaker notes).
        static func decode(_ text: String) -> State? {
            guard let data = text.data(using: .utf8),
                  let object = (try? JSONSerialization.jsonObject(with: data)) as? [String: Any],
                  Set(object.keys) == keys else {
                return nil
            }
            return try? JSONDecoder().decode(State.self, from: data)
        }
    }

    // MARK: Words (the desktop card's, R-AUD-27 and R-AUD-11)

    static let unreachableReason = "Connect to the Core to change these."
    static let olderCoreReason = "This Core can't set its speaker from here. Update the Core."

    /// The amber note while the chosen card is missing and the Core plays
    /// on its default meanwhile.
    static func missingNote(chosen: String, playing: String, inUse: Bool) -> String {
        let why = inUse ? "is in use by another program" : "is not connected"
        return "\(chosen) \(why) at the Core. Playing on the Core's default, \(playing), until it comes back."
    }

    // MARK: State

    /// The section is in the panel: false only while the Core plays on no card.
    @Published private(set) var isShown = true
    /// The Core's level, 0 to 100, or nil when it sends none.
    @Published private(set) var volume: Int64?
    /// The Core's mute.
    @Published private(set) var muted = false
    /// Why the section is greyed; nil while it is live.
    @Published private(set) var reason: String?
    /// The amber note while the chosen card is missing at the Core.
    @Published private(set) var missingNote: String?
    /// The Core's words for a change it refused.
    @Published private(set) var refusal: String?

    var isEnabled: Bool { reason == nil }

    private static let logger = Logger(subsystem: "NereusSDR", category: "audio.coreSpeaker")

    private let mirror: MirrorStore
    private var watch: ToolMirrorWatch?
    private var lastRefusal: String?
    /// Each write shows at the touch and stays until the Core answers, one
    /// at a time per property, the newest value next (``PropertyWriteQueue``).
    private lazy var writes = PropertyWriteQueue(store: mirror) { [weak self] property, outcome in
        self?.noteOutcome(outcome, property)
    }

    init(mirror: MirrorStore) {
        self.mirror = mirror
        watch = ToolMirrorWatch(mirror: mirror) { [weak self] in self?.refresh() }
        refresh()
    }

    func refresh() {
        let object = watch?.object(Self.objectKey)
        let connected = mirror.isSnapshotComplete && !mirror.isStale
        var state: State?
        if case .text(let text)? = object?[Self.stateProperty] {
            state = State.decode(text)
        }
        let why: String?
        let shown: Bool
        if !connected {
            why = Self.unreachableReason
            shown = true
        } else if mirror.capabilityVersion(Self.capabilityName) < 1 {
            why = Self.olderCoreReason
            shown = true
        } else {
            why = nil
            // D26 and design choice 8: shown only while the Core plays on a card.
            if let state, !state.playing.isEmpty, state.state != .noCard, state.state != .waitingForPick {
                shown = true
            } else {
                shown = false
            }
        }
        let live = why == nil && shown
        if !live {
            lastRefusal = nil
        }
        var level: Int64?
        if live, case .int(let value)? = object?[Self.volumeProperty] {
            level = value
        }
        var mute = false
        if live, case .bool(let value)? = object?[Self.mutedProperty] {
            mute = value
        }
        var amber: String?
        if live, let state, state.state == .notConnected || state.state == .inUse, !state.chosen.isEmpty {
            amber = Self.missingNote(chosen: state.chosen, playing: state.playing, inUse: state.state == .inUse)
        }
        let shownRefusal = live ? lastRefusal : nil
        if isShown != shown { isShown = shown }
        if reason != why { reason = why }
        if volume != level { volume = level }
        if muted != mute { muted = mute }
        if missingNote != amber { missingNote = amber }
        if refusal != shownRefusal { refusal = shownRefusal }
    }

    // MARK: Changes

    /// Sets the level, held to 0 to 100 in whole steps.
    func setVolume(_ level: Double) {
        guard isEnabled, isShown else {
            return
        }
        let clamped = min(max(level, Self.volumeRange.lowerBound), Self.volumeRange.upperBound)
        let value = Int64(clamped.rounded())
        guard value != volume else {
            return
        }
        writes.write(Self.objectKey, Self.volumeProperty, .int(value))
        refresh()
    }

    func setMuted(_ on: Bool) {
        guard isEnabled, isShown, on != muted else {
            return
        }
        writes.write(Self.objectKey, Self.mutedProperty, .bool(on))
        refresh()
    }

    private func noteOutcome(_ outcome: PropertyWriteOutcome, _ property: String) {
        if outcome.accepted {
            lastRefusal = nil
        } else if outcome.answeredByCore, !outcome.reason.isEmpty {
            lastRefusal = outcome.reason
        } else if !outcome.answeredByCore {
            // Not sent, not answered in time, or cut off by a dropped link.
            Self.logger.info("A \(property, privacy: .public) change: \(outcome.reason, privacy: .private)")
            lastRefusal = outcome.reason.isEmpty ? nil : outcome.reason
        }
        refresh()
    }
}
