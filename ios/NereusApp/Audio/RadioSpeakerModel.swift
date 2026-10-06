// NereusSDR for iOS: the speaker at the radio, its level and mute as the Core keeps them for every window and phone
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusMirror
import os

/// The Sound panel's Radio speaker section (R-SPK-20, D12): the level and
/// mute of the speaker at the radio, which the Core owns and every desktop
/// window and phone shares (`radio`'s `radioSpeakerVolume` and
/// `radioSpeakerMuted`, sent to a peer that declares `radioSpeaker` 1 with
/// `radioSpeakerVersion` 1). A change here is written to the Core, and the
/// Core's value moves the slider back here, whoever changed it.
///
/// It is greyed with a note, never hidden, while no radio is connected or
/// the Core cannot set it (R-SPK-06). On a Hermes Lite 2 it stays live and
/// the note says what the headphone output needs (R-SPK-07). The amplifier
/// choice is in Setup > Audio > Outputs, not here.
@MainActor
final class RadioSpeakerModel: ObservableObject {
    // MARK: The Core's names

    static let objectKey = "radio"
    static let capabilityName = "radioSpeakerVersion"
    static let volumeProperty = "radioSpeakerVolume"
    static let mutedProperty = "radioSpeakerMuted"
    static let availabilityProperty = "radioSpeakerAvailability"
    /// The level's range, as the Core keeps it.
    static let volumeRange: ClosedRange<Double> = 0...100

    /// The Core's `radioSpeakerAvailability`.
    enum Availability: Int64 {
        case noRadio = 0
        case available = 1
        case needsAddOn = 2
    }

    // MARK: Words (the desktop's, R-SPK-06 and R-SPK-07)

    static let noRadioReason = "No radio connected"
    static let olderCoreReason = "This Core can't set the radio speaker. Update the Core."
    /// RadioModel::radioSpeakerAddOnNote.
    static let addOnNote = "Needs the Hermes Lite 2 audio add-on board for its headphone output."

    // MARK: State

    /// The Core's level, 0 to 100, or nil when it sends none.
    @Published private(set) var volume: Int64?
    /// The Core's mute.
    @Published private(set) var muted = false
    /// Why the section is greyed; nil while it is live.
    @Published private(set) var reason: String?
    /// The note under a live section: the add-on board's, or the Core's
    /// words for a change it refused.
    @Published private(set) var note: String?

    var isEnabled: Bool { reason == nil }

    private static let logger = Logger(subsystem: "NereusSDR", category: "audio.radioSpeaker")

    private let mirror: MirrorStore
    private var watch: ToolMirrorWatch?
    private var refusal: String?
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
        var availability: Availability?
        if case .int(let raw)? = object?[Self.availabilityProperty] {
            availability = Availability(rawValue: raw)
        }
        let why: String?
        if !connected {
            why = Self.noRadioReason
        } else if mirror.capabilityVersion(Self.capabilityName) < 1 {
            why = Self.olderCoreReason
        } else if object == nil || availability == nil || availability == .noRadio {
            why = Self.noRadioReason
        } else {
            why = nil
        }
        if why != nil {
            refusal = nil
        }
        var level: Int64?
        if why == nil, case .int(let value)? = object?[Self.volumeProperty] {
            level = value
        }
        var mute = false
        if why == nil, case .bool(let value)? = object?[Self.mutedProperty] {
            mute = value
        }
        let shownNote = why == nil ? (refusal ?? (availability == .needsAddOn ? Self.addOnNote : nil)) : nil
        if reason != why { reason = why }
        if volume != level { volume = level }
        if muted != mute { muted = mute }
        if note != shownNote { note = shownNote }
    }

    // MARK: Changes

    /// Sets the level, held to 0 to 100 in whole steps.
    func setVolume(_ level: Double) {
        guard isEnabled else {
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
        guard isEnabled, on != muted else {
            return
        }
        writes.write(Self.objectKey, Self.mutedProperty, .bool(on))
        refresh()
    }

    private func noteOutcome(_ outcome: PropertyWriteOutcome, _ property: String) {
        if outcome.accepted {
            refusal = nil
        } else if outcome.answeredByCore, !outcome.reason.isEmpty {
            refusal = outcome.reason
        } else if !outcome.answeredByCore {
            // Not sent, not answered in time, or cut off by a dropped link.
            Self.logger.info("A \(property, privacy: .public) change: \(outcome.reason, privacy: .private)")
            refusal = outcome.reason.isEmpty ? nil : outcome.reason
        }
        refresh()
    }
}
